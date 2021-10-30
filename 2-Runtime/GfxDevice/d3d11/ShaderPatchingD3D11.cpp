#include "UnityPrefix.h"
#include "ShaderPatchingD3D11.h"
#include "D3D11ByteCode.h"
#include "External/DirectX/builds/dx9include/d3d9.h"


#define DEBUG_D3D11_SHADER_PATCHING 0


// Vertex & pixel shaders might have different set of interpolated registers, so hard to adaptively
// pick register that would always fit. So instead always put fog into this register; and in case of
// shaders already using this one, we'll just fail and there will be no fog.
// SM4 has 16 output registers, let's use second-to-last.
const int kFogInterpRegister = 14;


// -------------------------------------------------------------------

#if DEBUG_D3D11_SHADER_PATCHING
struct D3D10BlobHack
{
	virtual HRESULT WINAPI QueryInterface(const int& iid, void** ppv) = 0;
	virtual ULONG WINAPI AddRef() = 0;
	virtual ULONG WINAPI Release() = 0;
	virtual void* WINAPI GetBufferPointer() = 0;
	virtual SIZE_T WINAPI GetBufferSize() = 0;
};

typedef HRESULT (WINAPI *D3DDisassembleFunc)(const void* pSrcData, SIZE_T SrcDataSize, UINT Flags, const char* szComments, D3D10BlobHack** ppDisassembly);

static void DisassembleShader(const char* name, const void* data, size_t size)
{
	static D3DDisassembleFunc func = NULL;
	if (!func)
	{
		HMODULE dll;
#if !UNITY_METRO
		dll = LoadLibraryA("d3dcompiler_43.dll");
#else
		dll = LoadPackagedLibrary(L"d3dcompiler_46.dll", 0); // _46.dll is ARM
#endif
		if (!dll)
		{
			printf_console("--- Failed to load d3dcompiler dll - error code: %d", GetLastError());
			return;
		}
		func = (D3DDisassembleFunc)GetProcAddress(dll,"D3DDisassemble");
		if (!func)
			return;
	}
	D3D10BlobHack* blob = NULL;
	HRESULT hr = func(data, size, 0, NULL, &blob);
	if (FAILED(hr))
	{
		printf_console("Failed to disasm shader!\n");
	}
	if (blob)
	{
		std::string str((const char*)blob->GetBufferPointer(), blob->GetBufferSize());
		printf_console("==== %s:\n%s\n", name, str.c_str());
		blob->Release();
	}
}
#endif // #if DEBUG_D3D11_SHADER_PATCHING



// -------------------------------------------------------------------



static DXBCChunkSig* AddFogToSignatureChunk (const DXBCChunkSig* in)
{
	// check for max number of registers used
	UInt32 maxRegNum = 0;
	for (int i = 0; i < in->count; ++i)
		maxRegNum = std::max(maxRegNum, in->elements[i].register_num);
	if (maxRegNum >= kFogInterpRegister)
		return NULL;

	//@TODO: check for FOG already being used?
	const char* kFogName = "FOG";

	// size = header + existing size + one more DXBCSignatureElement + name
	unsigned size = sizeof(DXBCChunkHeader) + in->size + sizeof(DXBCSignatureElement);
	const unsigned nameOffset = size;
	size += strlen(kFogName)+1;
	size = (size + 3) & ~3; // align to next dword

	UInt8* buf = (UInt8*)malloc(size);
	DXBCChunkSig* chunk = (DXBCChunkSig*)buf;
	memset (chunk, 0xAB, size);
	chunk->fourcc = in->fourcc;
	chunk->size = size - 8;
	chunk->count = in->count + 1;
	chunk->unk8 = in->unk8;
	// copy existing signature elements
	memcpy (chunk->elements, in->elements, sizeof(chunk->elements[0]) * in->count);
	// move name offsets
	for (int i = 0; i < in->count; ++i)
		chunk->elements[i].name_offset += sizeof(DXBCSignatureElement);

	// add our signature element
	DXBCSignatureElement& el = chunk->elements[in->count];
	el.name_offset = nameOffset - 8;
	el.semantic_index = 0;
	el.system_value_type = 0;
	el.component_type = 3; // float
	el.register_num = kFogInterpRegister;
	el.mask = 1;
	el.read_write_mask = (in->fourcc == kFOURCC_ISGN) ? 1 : (~1 & 0xF);
	el.stream = 0;
	el.unused = 0;

	// copy old names
	const unsigned oldNamesOffset = sizeof(DXBCChunkHeader) + 8 + sizeof(DXBCSignatureElement) * in->count;
	const unsigned newNamesOffset = oldNamesOffset + sizeof(DXBCSignatureElement);
	memcpy (buf + newNamesOffset, ((const char*)in) + oldNamesOffset, nameOffset - newNamesOffset);

	// add our name
	memcpy (buf + nameOffset, kFogName, strlen(kFogName)+1);

	return chunk;
}


static bool IsDclOpcode (SM4Opcode op)
{
	return
		op >= kSM4Op_DCL_RESOURCE && op <= kSM4Op_DCL_GLOBAL_FLAGS ||
		op >= kSM4Op_DCL_STREAM && op <= kSM4Op_DCL_RESOURCE_STRUCTURED ||
		op == kSM4Op_DCL_GS_INSTANCE_COUNT;
}


static int FindOutputRegister(const dynamic_array<UInt32>& tokens, bool vertex)
{
	SM4TokInstruction instr;
	instr.dword = 0;
	for (size_t idx = 0; idx < tokens.size(); idx += instr.length)
	{
		instr.dword = tokens[idx];
		if (vertex && instr.opcode == kSM4Op_DCL_OUTPUT_SIV && instr.length == 4)
		{
			UInt32 siv = tokens[idx+3];
			if (siv == 1) // D3D10_NAME_POSITION
				return tokens[idx+2];
		}
		if (!vertex && instr.opcode == kSM4Op_DCL_OUTPUT && instr.length == 3)
		{
			//@TODO: what happens when shader writes to both color & depth, in various orderings?
			return tokens[idx+2];
		}
	}
	return -1;
}


static size_t FindAfterLastDclPos(const dynamic_array<UInt32>& tokens)
{
	SM4TokInstruction instr;
	instr.dword = 0;
	size_t afterLastDclPos = 0;
	for (size_t idx = 0; idx < tokens.size(); idx += instr.length)
	{
		instr.dword = tokens[idx];
		if (IsDclOpcode((SM4Opcode)instr.opcode))
			afterLastDclPos = idx + instr.length;
		else
			break;
	}
	return afterLastDclPos;
}


static int AddTempRegisters(dynamic_array<UInt32>& tokens, int tempsToAdd)
{
	size_t afterLastDclPos = FindAfterLastDclPos(tokens);

	SM4TokInstruction instr;
	instr.dword = 0;
	for (size_t idx = 0; idx < afterLastDclPos; idx += instr.length)
	{
		instr.dword = tokens[idx];
		if (instr.opcode == kSM4Op_DCL_TEMPS && instr.length == 2)
		{
			int tempsCount = tokens[idx+1];
			tokens[idx+1] += tempsToAdd;
			return tempsCount;
		}
	}

	// no temps used, insert dcl_temps after all other declaration statements
	SM4TokInstruction tok;
	tok.dword = 0;
	tok.opcode = kSM4Op_DCL_TEMPS;
	tok.length = 2;
	tokens.insert(tokens.begin()+(afterLastDclPos++), tok.dword);
	tokens.insert(tokens.begin()+(afterLastDclPos++), tempsToAdd);

	return 0;
}


static void AddFogRegister(dynamic_array<UInt32>& tokens, bool output)
{
	size_t afterLastDclPos = FindAfterLastDclPos(tokens);

	// add dcl statement for fog register after all other declarations
	SM4TokInstruction dcl;
	dcl.dword = 0;
	dcl.opcode = output ? kSM4Op_DCL_OUTPUT : kSM4Op_DCL_INPUT_PS;
	if (dcl.opcode == kSM4Op_DCL_INPUT_PS)
		dcl.dcl_input_ps.interpolation = kSM4Interp_LINEAR;
	dcl.length = 3;
	SM4TokOperand op;
	op.dword = 0;
	op.comps_enum = kSM4OperComp4;
	op.mode = SM4_OPERAND_MODE_MASK;
	op.sel = 1;
	op.file = output ? kSM4File_OUTPUT : kSM4File_INPUT;
	op.num_indices = 1;
	op.index0_repr = SM4_OPERAND_INDEX_REPR_IMM32;

	tokens.insert(tokens.begin()+(afterLastDclPos++), dcl.dword);
	tokens.insert(tokens.begin()+(afterLastDclPos++), op.dword);
	tokens.insert(tokens.begin()+(afterLastDclPos++), kFogInterpRegister);
}


static bool AddConstantBufferRegister(dynamic_array<UInt32>& tokens)
{
	size_t afterLastDclPos = FindAfterLastDclPos(tokens);

	// are we already using the needed CB bind point?
	SM4TokInstruction instr;
	instr.dword = 0;
	for (size_t idx = 0; idx < afterLastDclPos; idx += instr.length)
	{
		instr.dword = tokens[idx];
		if (instr.opcode == kSM4Op_DCL_CONSTANT_BUFFER && instr.length == 4)
			if (tokens[idx+2] == k11FogConstantBufferBind)
				return false;
	}

	// add dcl statement for constant buffer after all other declarations
	SM4TokInstruction dcl;
	dcl.dword = 0;
	dcl.opcode = kSM4Op_DCL_CONSTANT_BUFFER;
	dcl.length = 4;
	SM4TokOperand op;
	op.dword = 0;
	op.comps_enum = kSM4OperComp4;
	op.file = kSM4File_CONSTANT_BUFFER;
	op.num_indices = 2;

	tokens.insert(tokens.begin()+(afterLastDclPos++), dcl.dword);
	tokens.insert(tokens.begin()+(afterLastDclPos++), op.dword);
	tokens.insert(tokens.begin()+(afterLastDclPos++), k11FogConstantBufferBind);
	tokens.insert(tokens.begin()+(afterLastDclPos++), k11FogSize);

	return true;
}



static bool SupportedIndexRepr(SM4OperIndexRepr repr)
{
	return repr == SM4_OPERAND_INDEX_REPR_IMM32 || repr == SM4_OPERAND_INDEX_REPR_REG_IMM32;
}

static void RemapRegisterToTemp(dynamic_array<UInt32>& tokens, SM4RegFile type, int num, int tempNum)
{
	SM4TokInstruction instr;
	instr.dword = 0;
	for (size_t idx = 0; idx < tokens.size(); idx += instr.length)
	{
		instr.dword = tokens[idx];
		if (IsDclOpcode((SM4Opcode)instr.opcode) || instr.opcode == kSM4Op_CUSTOMDATA)
			continue;

		size_t idxEnd = idx + instr.length;

		// skip over extended tokens
		size_t jj = idx;
		SM4TokInstructionEx exttok;
		exttok.dword = instr.dword;
		while (exttok.extended)
			exttok.dword = tokens[++jj];
		Assert(jj < idxEnd);

		// go over operands
		while (jj+1 < idxEnd)
		{
			SM4TokOperand optok;
			optok.dword = tokens[++jj];
			size_t opIdx = jj;
			if (optok.extended)
				++jj;

			// only remap simple register references: needed file, one simple index
			if (optok.file == type && optok.num_indices == 1)
			{
				if (optok.index0_repr == SM4_OPERAND_INDEX_REPR_IMM32 || optok.index0_repr == SM4_OPERAND_INDEX_REPR_REG_IMM32)
				{
					if (tokens[jj+1] == num)
					{
						optok.file = kSM4File_TEMP;
						tokens[opIdx] = optok.dword;
						tokens[jj+1] = tempNum;
					}
				}
			}

			// skip over data for this operand
			if (optok.num_indices >= 1) {
				if (!SupportedIndexRepr((SM4OperIndexRepr)optok.index0_repr))
					break;
				++jj;
			}
			if (optok.num_indices >= 2) {
				if (!SupportedIndexRepr((SM4OperIndexRepr)optok.index1_repr))
					break;
				++jj;
			}
			if (optok.num_indices >= 3) {
				if (!SupportedIndexRepr((SM4OperIndexRepr)optok.index2_repr))
					break;
				++jj;
			}
			if (optok.file == kSM4File_IMMEDIATE32)
				++jj;
			if (optok.file == kSM4File_IMMEDIATE64)
				jj += 2;
		}
	}
}


static void RemoveRetFromEnd (dynamic_array<UInt32>& tokens)
{
	if (tokens.empty())
		return;

	SM4TokInstruction instr;
	instr.dword = tokens.back();
	if (instr.opcode == kSM4Op_RET && instr.length == 1)
		tokens.pop_back();
}


static DXBCChunkCode* AddFogToVertexCodeChunk (const DXBCChunkCode* in)
{
	const UInt32* inTokens = (const UInt32*)&in->version;

	dynamic_array<UInt32> newTokens;
	newTokens.insert(newTokens.end(), inTokens + 2, inTokens + in->length); // don't add version & length words

	int posRegister = FindOutputRegister (newTokens, true);
	if (posRegister < 0)
	{
		#if DEBUG_D3D11_SHADER_PATCHING
		printf_console("Fog patch failed: output register not found\n");
		#endif
		return NULL;
	}
	AddFogRegister (newTokens, true);
	int tmpRegister = AddTempRegisters (newTokens, 1);
	RemapRegisterToTemp (newTokens, kSM4File_OUTPUT, posRegister, tmpRegister);
	RemoveRetFromEnd (newTokens);

	DXBCCodeBuilder* codeBuilder = dxb_create_code(newTokens);
	DXBCBuilderStream bld(codeBuilder);
	bld.op(kSM4Op_MOV).reg('o',posRegister,0xF).swz('r',tmpRegister,kSM4SwzNone); // pos.xyzw = tmp.xyzw
	bld.op(kSM4Op_MOV).reg('o',kFogInterpRegister,0x1).swz('r',tmpRegister,kSM4SwzRepZ); // fog.x = tmp.z
	bld.op(kSM4Op_RET);
	dxb_destroy_code(codeBuilder);

	const unsigned chunkSize = sizeof(DXBCChunkCode) + newTokens.size()*sizeof(newTokens[0]);
	UInt8* buf = (UInt8*)malloc(chunkSize);
	DXBCChunkCode* chunk = (DXBCChunkCode*)buf;
	chunk->fourcc = in->fourcc;
	chunk->size = chunkSize - sizeof(DXBCChunkHeader);
	chunk->version = inTokens[0];
	chunk->length = newTokens.size() + 2;
	memcpy (buf + sizeof(DXBCChunkCode), newTokens.data(), newTokens.size()*sizeof(newTokens[0]));

	return chunk;
}


static DXBCChunkCode* AddFogToPixelCodeChunk (const DXBCChunkCode* in, FogMode fogMode)
{
	const UInt32* inTokens = (const UInt32*)&in->version;

	dynamic_array<UInt32> newTokens;
	newTokens.insert(newTokens.end(), inTokens + 2, inTokens + in->length); // don't add version & length words

	int colRegister = FindOutputRegister (newTokens, false);
	if (colRegister < 0)
	{
		#if DEBUG_D3D11_SHADER_PATCHING
		printf_console("Fog patch failed: output register not found\n");
		#endif
		return NULL;
	}
	AddFogRegister (newTokens, false);
	if (!AddConstantBufferRegister (newTokens))
	{
		#if DEBUG_D3D11_SHADER_PATCHING
		printf_console("Fog patch failed: can't add constant buffer\n");
		#endif
		return NULL;
	}

	int tmpReg = AddTempRegisters (newTokens, 2);
	const int tmp2 = tmpReg+1;
	const UInt32 fogParamsReg = (k11FogConstantBufferBind << 16) | k11FogParams;
	const UInt32 fogColorReg = (k11FogConstantBufferBind << 16) | k11FogColor;

	RemapRegisterToTemp (newTokens, kSM4File_OUTPUT, colRegister, tmpReg);
	RemoveRetFromEnd (newTokens);

	// add fog handling code
	DXBCCodeBuilder* codeBuilder = dxb_create_code(newTokens);
	DXBCBuilderStream bld(codeBuilder);

	if (fogMode == kFogExp2)
	{
		// fog = exp(-(density*z)^2)
		bld.op(kSM4Op_MUL).reg('r',tmp2,1).swz('c',fogParamsReg,kSM4SwzRepX).swz('v',kFogInterpRegister,kSM4SwzRepX); // tmp = (density/sqrt(ln(2))) * fog
		bld.op(kSM4Op_MUL).reg('r',tmp2,1).swz('r',tmp2,kSM4SwzRepX).swz('r',tmp2,kSM4SwzRepX); // tmp = tmp * tmp
		bld.op_sat(kSM4Op_EXP,tmp2).reg('r',tmp2,1).swz('r',tmp2,kSM4SwzRepX,true); // tmp = saturate (exp2 (-tmp))
	}
	else if (fogMode == kFogExp)
	{
		// fog = exp(-density*z)
		bld.op(kSM4Op_MUL).reg('r',tmp2,1).swz('c',fogParamsReg,kSM4SwzRepX).swz('v',kFogInterpRegister,kSM4SwzRepX); // tmp = (density/sqrt(ln(2))) * fog
		bld.op_sat(kSM4Op_EXP,tmp2).reg('r',tmp2,1).swz('r',tmp2,kSM4SwzRepX,true); // tmp = saturate (exp2 (-tmp))
	}
	else if (fogMode == kFogLinear)
	{
		// fog = (end-z)/(end-start)
		// ->  tmp = (-1/(end-start)) * fog + (end/(end-start))
		bld.op_sat(kSM4Op_MAD,tmp2).reg('r',tmp2,1).swz('v',kFogInterpRegister,kSM4SwzRepX).swz('c',fogParamsReg,kSM4SwzRepZ).swz('c',fogParamsReg,kSM4SwzRepW);
	}
	else
	{
		AssertString("unknown fog mode");
	}

	// color.rgb = lerp (fogColor.rgb, color.rgb, fogVar) =
	//		(color.rgb-fogColor.rgb) * fogVar + fogColor.rgb
	bld.op(kSM4Op_ADD).reg('r',tmpReg,7).swz('r',tmpReg,kSM4SwzXYZX).swz('c',fogColorReg,kSM4SwzXYZX, true);
	bld.op(kSM4Op_MAD).reg('r',tmpReg,7).swz('r',tmpReg,kSM4SwzXYZX).swz('r',tmp2,kSM4SwzRepX).swz('c',fogColorReg,kSM4SwzXYZX);

	// move into final output
	bld.op(kSM4Op_MOV).reg('o',colRegister,0xF).swz('r',tmpReg,kSM4SwzNone); // col.xyzw = tmp.xyzw
	bld.op(kSM4Op_RET);
	dxb_destroy_code(codeBuilder);

	const unsigned chunkSize = sizeof(DXBCChunkCode) + newTokens.size()*sizeof(newTokens[0]);
	UInt8* buf = (UInt8*)malloc(chunkSize);
	DXBCChunkCode* chunk = (DXBCChunkCode*)buf;
	chunk->fourcc = in->fourcc;
	chunk->size = chunkSize - sizeof(DXBCChunkHeader);
	chunk->version = inTokens[0];
	chunk->length = newTokens.size() + 2;
	memcpy (buf + sizeof(DXBCChunkCode), newTokens.data(), newTokens.size()*sizeof(newTokens[0]));

	return chunk;
}


// -------------------------------------------------------------------


static bool FindShaderChunks (const DXBCContainer* dxbc, int* inputIdx, int* outputIdx, int* codeIdx)
{
	*inputIdx = -1;
	*outputIdx = -1;
	*codeIdx = -1;
	for (int i = 0; i < dxbc->chunks.size(); ++i)
	{
		DXBCChunkHeader* c = dxbc->chunks[i];
		if (c->fourcc == kFOURCC_ISGN)
			*inputIdx = i;
		else if (c->fourcc == kFOURCC_OSGN)
			*outputIdx = i;
		else if (c->fourcc == kFOURCC_SHDR || c->fourcc == kFOURCC_SHEX)
			*codeIdx = i;
	}
	if (*inputIdx < 0 || *outputIdx < 0 || *codeIdx < 0)
		return false;
	return true;
}


// -------------------------------------------------------------------



bool PatchVertexOrDomainShaderFogD3D11 (dynamic_array<UInt8>& byteCode)
{
	std::auto_ptr<DXBCContainer> dxbc (dxbc_parse (byteCode.data(), byteCode.size()));
	if (dxbc.get() == NULL)
		return false;

	#if DEBUG_D3D11_SHADER_PATCHING
	DisassembleShader ("VS before patching", byteCode.data(), byteCode.size());
	#endif

	int inputIdx, outputIdx, codeIdx;
	if (!FindShaderChunks (dxbc.get(), &inputIdx, &outputIdx, &codeIdx))
	{
		#if DEBUG_D3D11_SHADER_PATCHING
		printf_console("Fog patch failed: shader chunks not found?\n");
		#endif
		return false;
	}
	const DXBCChunkSig* inputChunk = (DXBCChunkSig*)dxbc->chunks[inputIdx];
	const DXBCChunkSig* outputChunk = (DXBCChunkSig*)dxbc->chunks[outputIdx];
	const DXBCChunkCode* codeChunk = (DXBCChunkCode*)dxbc->chunks[codeIdx];

	DXBCChunkSig* newOutputChunk = AddFogToSignatureChunk (outputChunk);
	if (!newOutputChunk)
	{
		#if DEBUG_D3D11_SHADER_PATCHING
		printf_console("Fog patch failed: can't add fog output signature\n");
		#endif
		return false;
	}

	DXBCChunkCode* newCodeChunk = AddFogToVertexCodeChunk (codeChunk);
	if (!newCodeChunk)
	{
		free (newOutputChunk);
		return false;
	}

	dxbc->chunks[outputIdx] = newOutputChunk;
	dxbc->chunks[codeIdx] = newCodeChunk;
	dynamic_array<UInt8> newByteCode = byteCode;
	dxbc_create(dxbc->chunks.data(), dxbc->chunks.size(), newByteCode);
	byteCode = newByteCode;

	#if DEBUG_D3D11_SHADER_PATCHING
	DisassembleShader ("VS after patching", byteCode.data(), byteCode.size());
	#endif

	free (newOutputChunk);
	free (newCodeChunk);

	return true;
}


// -------------------------------------------------------------------


bool PatchPixelShaderFogD3D11 (dynamic_array<UInt8>& byteCode, FogMode fog)
{
	std::auto_ptr<DXBCContainer> dxbc (dxbc_parse (byteCode.data(), byteCode.size()));
	if (dxbc.get() == NULL)
		return false;

	#if DEBUG_D3D11_SHADER_PATCHING
	printf_console("Patching fog mode %d\n", fog);
	DisassembleShader ("PS before patching", byteCode.data(), byteCode.size());
	#endif

	int inputIdx, outputIdx, codeIdx;
	if (!FindShaderChunks (dxbc.get(), &inputIdx, &outputIdx, &codeIdx))
	{
		#if DEBUG_D3D11_SHADER_PATCHING
		printf_console("Fog patch failed: shader chunks not found?\n");
		#endif
		return false;
	}
	const DXBCChunkSig* inputChunk = (DXBCChunkSig*)dxbc->chunks[inputIdx];
	const DXBCChunkSig* outputChunk = (DXBCChunkSig*)dxbc->chunks[outputIdx];
	const DXBCChunkCode* codeChunk = (DXBCChunkCode*)dxbc->chunks[codeIdx];

	DXBCChunkSig* newInputChunk = AddFogToSignatureChunk (inputChunk);
	if (!newInputChunk)
	{
		#if DEBUG_D3D11_SHADER_PATCHING
		printf_console("Fog patch failed: can't add fog input signature\n");
		#endif
		return false;
	}

	DXBCChunkCode* newCodeChunk = AddFogToPixelCodeChunk (codeChunk, fog);
	if (!newCodeChunk)
	{
		free (newInputChunk);
		return false;
	}

	dxbc->chunks[inputIdx] = newInputChunk;
	dxbc->chunks[codeIdx] = newCodeChunk;

	dynamic_array<UInt8> newByteCode = byteCode;
	dxbc_create(dxbc->chunks.data(), dxbc->chunks.size(), newByteCode);
	byteCode = newByteCode;

	#if DEBUG_D3D11_SHADER_PATCHING
	DisassembleShader ("PS after patching", byteCode.data(), byteCode.size());
	#endif

	free (newInputChunk);
	free (newCodeChunk);

	return true;
}



bool PatchRemovePartialPrecisionD3D11 (dynamic_array<UInt8>& byteCode)
{
	DXBCChunkSM20* codeChunk = static_cast<DXBCChunkSM20*>(dxbc_find_chunk(byteCode.data(), byteCode.size(), kFOURCC_SM20));
	if (!codeChunk)
		return false;

	// Reference for the bytecode format: "Direct3D Shader Codes" on MSDN,
	// http://msdn.microsoft.com/en-us/library/windows/hardware/ff552891(v=vs.85).aspx

	#if DEBUG_D3D11_SHADER_PATCHING
	dynamic_array<UInt8> origBC = byteCode;
	#endif

	const UInt32* codeEnd = (const UInt32*)(((const UInt8*)codeChunk) + sizeof(DXBCChunkHeader) + codeChunk->size);

	// Go past chunk header into chunk content
	UInt32* codePtr = (UInt32*)(codeChunk+1);
	// Table, ints: version, shader size, code offset
	if (codePtr+2 >= codeEnd)
		return false;
	const UInt32 codeOffset = codePtr[2];

	// Get to actual byte code start.
	// Code offset in the table is from end of DXBCChunkHeader
	codePtr = (UInt32*)(((UInt8*)codeChunk) + sizeof(DXBCChunkHeader) + codeOffset);
	codePtr++; // skip version token

	// Go over all instructions
	bool didChanges = false;
	while (codePtr < codeEnd)
	{
		SM2TokInstruction* insn = (SM2TokInstruction*)codePtr;
		const int length = insn->length;
		const SM2Opcode op = (SM2Opcode)insn->opcode;
		if (op == kSM2Op_DCL)
		{
			// DCL instructions have special format: destination token is the 2nd token
			if (length == 2)
			{
				SM2TokDst* dst = (SM2TokDst*)(codePtr+2);
				if (dst->res_mod & (D3DSPDM_PARTIALPRECISION>>D3DSP_DSTMOD_SHIFT))
				{
					dst->res_mod &= ~(D3DSPDM_PARTIALPRECISION>>D3DSP_DSTMOD_SHIFT);
					didChanges = true;
				}
			}
		}
		else if (op == kSM2Op_DEF || op == kSM2Op_DEFI || op == kSM2Op_DEFB)
		{
			// DEF* instructions have special format, don't have to do anything
		}
		else if (op == kSM2Op_CALLNZ || op == D3DSIO_LOOP || op == kSM2Op_IFC || op == kSM2Op_BREAKP || op == kSM2Op_BREAKC)
		{
			// these instructions don't have destination token
		}
		else if (length > 1) // must be a more than 2 argument instruction to have a destination token
		{
			SM2TokDst* dst = (SM2TokDst*)(codePtr+1);
			if (dst->res_mod & (D3DSPDM_PARTIALPRECISION>>D3DSP_DSTMOD_SHIFT))
			{
				dst->res_mod &= ~(D3DSPDM_PARTIALPRECISION>>D3DSP_DSTMOD_SHIFT);
				didChanges = true;
			}
		}
		codePtr += length + 1;
	}

	// Update shader checksum if we did changes
	if (didChanges && byteCode.size() > 20)
	{
		void D3DHash (const unsigned char* data, unsigned size, unsigned char res[16]);
		UInt8 hsh[16];
		D3DHash (&byteCode[20], byteCode.size()-20, &byteCode[4]);

		#if DEBUG_D3D11_SHADER_PATCHING
		DisassembleShader ("Shader before partial precision remove", origBC.data(), origBC.size());
		DisassembleShader ("Shader after partial precision remove", byteCode.data(), byteCode.size());
		#endif
	}


	return true;
}

