#include "UnityPrefix.h"
#include "D3D11ByteCode.h"
#include "D3D11Includes.h"
#include "Runtime/Utilities/BitUtility.h"
#include "External/DirectX/builds/dx9include/d3d9.h"

// Some things in this file are based on Mesa3d DX11 state tracker:

/**************************************************************************
 *
 * Copyright 2010 Luca Barbieri
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/




enum SM4SystemValue
{
	kSM4SV_UNDEFINED,
	kSM4SV_POSITION,
	kSM4SV_CLIP_DISTANCE,
	kSM4SV_CULL_DISTANCE,
	kSM4SV_RENDER_TARGET_ARRAY_INDEX,
	kSM4SV_VIEWPORT_ARRAY_INDEX,
	kSM4SV_VERTEX_ID,
	kSM4SV_PRIMITIVE_ID,
	kSM4SV_INSTANCE_ID,
	kSM4SV_IS_FRONT_FACE,
	kSM4SV_SAMPLE_INDEX,
	kSM4SV_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR,
	kSM4SV_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR,
	kSM4SV_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR,
	kSM4SV_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR,
	kSM4SV_FINAL_QUAD_U_INSIDE_TESSFACTOR,
	kSM4SV_FINAL_QUAD_V_INSIDE_TESSFACTOR,
	kSM4SV_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR,
	kSM4SV_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR,
	kSM4SV_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR,
	kSM4SV_FINAL_TRI_INSIDE_TESSFACTOR,
	kSM4SV_FINAL_LINE_DETAIL_TESSFACTOR,
	kSM4SV_FINAL_LINE_DENSITY_TESSFACTOR,
	kSM4SV_COUNT
};

enum SM4InstrExtype
{
	SM4_TOKEN_INSTRUCTION_EXTENDED_TYPE_EMPTY,
	SM4_TOKEN_INSTRUCTION_EXTENDED_TYPE_SAMPLE_CONTROLS,
	SM4_TOKEN_INSTRUCTION_EXTENDED_TYPE_RESOURCE_DIM,
	SM4_TOKEN_INSTRUCTION_EXTENDED_TYPE_RESOURCE_RETURN_TYPE,
	SM4_TOKEN_INSTRUCTION_EXTENDED_TYPE_COUNT
};

enum SM4OperExtype
{
	SM4_TOKEN_OPERAND_EXTENDED_TYPE_EMPTY,
	SM4_TOKEN_OPERAND_EXTENDED_TYPE_MODIFIER,
	SM4_TOKEN_OPERAND_EXTENDED_TYPE_COUNT
};

enum SM4ReturnType
{
	kSM4RetType_UNORM = 1,
	kSM4RetType_SNORM = 2,
	kSM4RetType_SINT = 3,
	kSM4RetType_UINT = 4,
	kSM4RetType_FLOAT = 5,
	kSM4RetType_MIXED = 6,
};


const char* kSM4OpcodeNames[kSM4Op_COUNT] = {
	"add",
	"and",
	"break",
	"breakc",
	"call",
	"callc",
	"case",
	"continue",
	"continuec",
	"cut",
	"default",
	"deriv_rtx",
	"deriv_rty",
	"discard",
	"div",
	"dp2",
	"dp3",
	"dp4",
	"else",
	"emit",
	"emitthencut",
	"endif",
	"endloop",
	"endswitch",
	"eq",
	"exp",
	"frc",
	"ftoi",
	"ftou",
	"ge",
	"iadd",
	"if",
	"ieq",
	"ige",
	"ilt",
	"imad",
	"imax",
	"imin",
	"imul",
	"ine",
	"ineg",
	"ishl",
	"ishr",
	"itof",
	"label",
	"ld",
	"ldms",
	"log",
	"loop",
	"lt",
	"mad",
	"min",
	"max",
	"customdata",
	"mov",
	"movc",
	"mul",
	"ne",
	"nop",
	"not",
	"or",
	"resinfo",
	"ret",
	"retc",
	"round_ne",
	"round_ni",
	"round_pi",
	"round_z",
	"rsq",
	"sample",
	"sample_c",
	"sample_c_lz",
	"sample_l",
	"sample_d",
	"sample_b",
	"sqrt",
	"switch",
	"sincos",
	"udiv",
	"ult",
	"uge",
	"umul",
	"umad",
	"umax",
	"umin",
	"ushr",
	"utof",
	"xor",
	"dcl_resource",
	"dcl_constant_buffer",
	"dcl_sampler",
	"dcl_index_range",
	"dcl_gs_output_primitive_topology",
	"dcl_gs_input_primitive",
	"dcl_max_output_vertex_count",
	"dcl_input",
	"dcl_input_sgv",
	"dcl_input_siv",
	"dcl_input_ps",
	"dcl_input_ps_sgv",
	"dcl_input_ps_siv",
	"dcl_output",
	"dcl_output_sgv",
	"dcl_output_siv",
	"dcl_temps",
	"dcl_indexable_temp",
	"dcl_global_flags",
	"d3d10_count",
	"lod",
	"gather4",
	"sample_pos",
	"sample_info",
	"d3d10_1_count",
	"hs_decls",
	"hs_control_point_phase",
	"hs_fork_phase",
	"hs_join_phase",
	"emit_stream",
	"cut_stream",
	"emitthencut_stream",
	"interface_call",
	"bufinfo",
	"deriv_rtx_coarse",
	"deriv_rtx_fine",
	"deriv_rty_coarse",
	"deriv_rty_fine",
	"gather4_c",
	"gather4_po",
	"gather4_po_c",
	"rcp",
	"f32tof16",
	"f16tof32",
	"uaddc",
	"usubb",
	"countbits",
	"firstbit_hi",
	"firstbit_lo",
	"firstbit_shi",
	"ubfe",
	"ibfe",
	"bfi",
	"bfrev",
	"swapc",
	"dcl_stream",
	"dcl_function_body",
	"dcl_function_table",
	"dcl_interface",
	"dcl_input_control_point_count",
	"dcl_output_control_point_count",
	"dcl_tess_domain",
	"dcl_tess_partitioning",
	"dcl_tess_output_primitive",
	"dcl_hs_max_tessfactor",
	"dcl_hs_fork_phase_instance_count",
	"dcl_hs_join_phase_instance_count",
	"dcl_thread_group",
	"dcl_unordered_access_view_typed",
	"dcl_unordered_access_view_raw",
	"dcl_unordered_access_view_structured",
	"dcl_thread_group_shared_memory_raw",
	"dcl_thread_group_shared_memory_structured",
	"dcl_resource_raw",
	"dcl_resource_structured",
	"ld_uav_typed",
	"store_uav_typed",
	"ld_raw",
	"store_raw",
	"ld_structured",
	"store_structured",
	"atomic_and",
	"atomic_or",
	"atomic_xor",
	"atomic_cmp_store",
	"atomic_iadd",
	"atomic_imax",
	"atomic_imin",
	"atomic_umax",
	"atomic_umin",
	"imm_atomic_alloc",
	"imm_atomic_consume",
	"imm_atomic_iadd",
	"imm_atomic_and",
	"imm_atomic_or",
	"imm_atomic_xor",
	"imm_atomic_exch",
	"imm_atomic_cmp_exch",
	"imm_atomic_imax",
	"imm_atomic_imin",
	"imm_atomic_umax",
	"imm_atomic_umin",
	"sync",
	"dadd",
	"dmax",
	"dmin",
	"dmul",
	"deq",
	"dge",
	"dlt",
	"dne",
	"dmov",
	"dmovc",
	"dtof",
	"ftod",
	"eval_snapped",
	"eval_sample_index",
	"eval_centroid",
	"dcl_gs_instance_count",
};

struct SM4TokVersion
{
	unsigned minor : 4;
	unsigned major : 4;
	unsigned format : 8;
	unsigned type : 16;
};



struct SM4TokResourceReturnType
{
	unsigned x : 4;
	unsigned y : 4;
	unsigned z : 4;
	unsigned w : 4;
};


#define SM4_OPERAND_SEL_MASK(sel) ((sel) & 0xf)
#define SM4_OPERAND_SEL_SWZ(sel, i) (((sel) >> ((i) * 2)) & 3)
#define SM4_OPERAND_SEL_SCALAR(sel) ((sel) & 3)

struct SM4TokOperandEx
{
	union {
		UInt32 dword;
		struct {
			unsigned type : 6;
			unsigned neg : 1;
			unsigned abs : 1;
		};
	};
};

struct SM4TokResourceRetType
{
	union {
		UInt32 dword;
		struct {
			unsigned x : 4;
			unsigned y : 4;
			unsigned z : 4;
			unsigned w : 4;
			unsigned reserved : 16;
		};
	};
};


union SM4Any
{
	float f32;
	SInt64 i64;
	SInt32 i32;
};

struct SM4Op;
struct SM4Instr;
struct SM4Decl;
struct SM4Program;


struct SM4Op
{
	UInt8 mode;
	UInt8 comps;
	UInt8 mask;
	UInt8 num_indices;
	UInt8 swizzle[4];
	SM4RegFile file;
	SM4Any imm_values[4];
	bool neg;
	bool abs;
	struct
	{
		SInt64 disp;
		std::auto_ptr<SM4Op> reg;
	} indices[3];

	bool is_index_simple(unsigned i) const
	{
		 return !indices[i].reg.get() && indices[i].disp >= 0 && (SInt64)(SInt32)indices[i].disp == indices[i].disp;
	}

	bool has_simple_index() const
	{
		return num_indices == 1 && is_index_simple(0);
	}

	SM4Op()
	{
		memset(this, 0, sizeof(*this));
	}

private:
	SM4Op(const SM4Op& op)
	{}
};

/* for sample_d */
#define SM4_MAX_OPS 6

struct SM4Instr : public SM4TokInstruction
{
	SInt8 sample_offset[3];
	UInt8 resource_target;
	UInt8 resource_return_type[4];

	unsigned num;
	unsigned num_ops;
	std::auto_ptr<SM4Op> ops[SM4_MAX_OPS];

	SM4Instr()
	{
		memset(this, 0, sizeof(*this));
	}

private:
	SM4Instr(const SM4Instr& op)
	{}
};

struct SM4Decl : public SM4TokInstruction
{
	std::auto_ptr<SM4Op> op;
	union
	{
		unsigned num;
		float f32;
		SM4SystemValue sv;
		struct
		{
			unsigned id;
			unsigned expected_function_table_length;
			unsigned table_length;
			unsigned array_length;
		} intf;
		unsigned thread_group_size[3];
		SM4TokResourceReturnType rrt;
		struct
		{
			unsigned num;
			unsigned comps;
		} indexable_temp;
		struct
		{
			unsigned stride;
			unsigned count;
		} structured;
	};

	void* data;

	SM4Decl()
	{
		memset(this, 0, sizeof(*this));
	}

	~SM4Decl()
	{
		free(data);
	}

private:
	SM4Decl(const SM4Decl& op)
	{}
};

struct _D3D11_SIGNATURE_PARAMETER_DESC;

struct SM4Program
{
	SM4TokVersion version;
	dynamic_array<SM4Decl*> dcls;
	dynamic_array<SM4Instr*> insns;

	SM4Program()
	{
		memset(&version, 0, sizeof(version));
	}

	~SM4Program()
	{
		for(dynamic_array<SM4Decl*>::iterator i = dcls.begin(), e = dcls.end(); i != e; ++i)
			delete *i;
		for(dynamic_array<SM4Instr*>::iterator i = insns.begin(), e = insns.end(); i != e; ++i)
			delete *i;
	}

private:
	SM4Program(const SM4Decl& op)
	{}
};

SM4Program* sm4_parse(const void* tokens, int size);




struct DXBCHeader
{
	UInt32 fourcc;
	UInt32 hash[4];
	UInt32 one;
	UInt32 total_size;
	UInt32 chunk_count;
};


static inline DXBCChunkHeader* dxbc_find_shader_bytecode(const void* data, int size)
{
	DXBCChunkHeader* chunk;
	chunk = dxbc_find_chunk(data, size, kFOURCC_SHDR);
	if(!chunk)
		chunk = dxbc_find_chunk(data, size, kFOURCC_SHEX);
	return chunk;
}

#define DXBC_FIND_INPUT_SIGNATURE    0
#define DXBC_FIND_OUTPUT_SIGNATURE   1
#define DXBC_FIND_PATCH_SIGNATURE    2

static inline DXBCChunkSig* dxbc_find_signature(const void* data, int size, unsigned kind)
{
	unsigned fourcc;
	switch(kind) {
	case DXBC_FIND_INPUT_SIGNATURE:  fourcc = kFOURCC_ISGN; break;
	case DXBC_FIND_OUTPUT_SIGNATURE: fourcc = kFOURCC_OSGN; break;
	case DXBC_FIND_PATCH_SIGNATURE:  fourcc = kFOURCC_PCSG; break;
	default:
		return NULL;
	}
	return (DXBCChunkSig*)dxbc_find_chunk(data, size, fourcc);
}

std::pair<void*, size_t> dxbc_assemble(struct DXBCChunkHeader** chunks, unsigned num_chunks);


DXBCContainer* dxbc_parse(const void* data, int size)
{
	std::auto_ptr<DXBCContainer> container(new DXBCContainer());
	container->data = data;
	DXBCHeader* header = (DXBCHeader*)data;
	UInt32* chunk_offsets = (UInt32*)(header + 1);
	if(header->fourcc != kFOURCC_DXBC)
		return 0;
	unsigned num_chunks = header->chunk_count;
	for(unsigned i = 0; i < num_chunks; ++i)
	{
		unsigned offset = chunk_offsets[i];
		DXBCChunkHeader* chunk = (DXBCChunkHeader*)((char*)data + offset);
		unsigned fourcc = chunk->fourcc;
		container->chunks.push_back(chunk);
	}
	return container.release();
}

DXBCChunkHeader* dxbc_find_chunk(const void* data, int size, unsigned fourcc)
{
	DXBCHeader* header = (DXBCHeader*)data;
	UInt32* chunk_offsets = (UInt32*)(header + 1);
	if(header->fourcc != kFOURCC_DXBC)
		return 0;
	unsigned num_chunks = header->chunk_count;
	for(unsigned i = 0; i < num_chunks; ++i)
	{
		unsigned offset = chunk_offsets[i];
		DXBCChunkHeader* chunk = (DXBCChunkHeader*)((char*)data + offset);
		if(chunk->fourcc == fourcc)
			return chunk;
	}
	return 0;
}

static void print_binary_chunk (const DXBCChunkHeader& chk, int perLine = 16)
{
	const char* kHex = "0123456789abcdef";
	const UInt8* ptr = ((const UInt8*)&chk) + sizeof(DXBCChunkHeader);
	std::string res;
	for (unsigned i = 0; i < chk.size; ++i)
	{
		if (i != 0 && i%perLine == 0)
			res += '\n';
		if ((i & 3) == 0)
			res += ' ';
		UInt8 b = ptr[i];
		res += kHex[b>>4];
		res += kHex[b&0xF];
	}
	printf_console ("%s\n", res.c_str());
}

static void print_sm4_program (const SM4Program& prog)
{
}

void dxbc_print(const DXBCContainer* dxbc)
{
	printf_console ("DXBC dump:\n");
	if (!dxbc)
	{
		printf_console ("null\n");
		return;
	}

	printf_console ("chunk count: %d\n", (int)dxbc->chunks.size());
	for (size_t i = 0; i < dxbc->chunks.size(); ++i)
	{
		const DXBCChunkHeader& chk = *dxbc->chunks[i];
		printf_console ("chunk #%i: %c%c%c%c size %u\n", (int)i, chk.fourcc&0xFF, (chk.fourcc>>8)&0xFF, (chk.fourcc>>16)&0xFF, (chk.fourcc>>24)&0xFF, chk.size);
		if (chk.fourcc == kFOURCC_ISGN || chk.fourcc == kFOURCC_OSGN)
		{
			print_binary_chunk (chk);
			const DXBCChunkSig* sig = (const DXBCChunkSig*)&chk;
			D3D11_SIGNATURE_PARAMETER_DESC* params;
			int count = dxbc_parse_signature (sig, &params);
			for (int j = 0; j < count; ++j)
			{
				const D3D11_SIGNATURE_PARAMETER_DESC& p = params[j];
				printf_console ("  #%i: %s/%u reg %u sv %u type %u mask %x rwmask %x stream %u\n",
					j, p.SemanticName, p.SemanticIndex, p.Register, p.SystemValueType, p.ComponentType, p.Mask, p.ReadWriteMask, p.Stream);
			}
			free (params);
		}
		else if (chk.fourcc == kFOURCC_SHDR || chk.fourcc == kFOURCC_SHEX)
		{
			printf_console ("shader code:\n");
			print_binary_chunk (chk);
			SM4Program* prog = sm4_parse((&chk)+1, chk.size);
			if (prog)
			{
				print_sm4_program(*prog);
				delete prog;
			}
		}
	}
}


int dxbc_parse_signature(const DXBCChunkSig* sig, D3D11_SIGNATURE_PARAMETER_DESC** params)
{
	unsigned count = sig->count;
	*params = (D3D11_SIGNATURE_PARAMETER_DESC*)malloc(sizeof(D3D11_SIGNATURE_PARAMETER_DESC) * count);

	for (unsigned i = 0; i < count; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC& param = (*params)[i];
		param.SemanticName = (char*)&sig->count + sig->elements[i].name_offset;
		param.SemanticIndex = sig->elements[i].semantic_index;
		param.SystemValueType = (D3D_NAME)sig->elements[i].system_value_type;
		param.ComponentType = (D3D_REGISTER_COMPONENT_TYPE)sig->elements[i].component_type;
		param.Register = sig->elements[i].register_num;
		param.Mask = sig->elements[i].mask;
		param.ReadWriteMask = sig->elements[i].read_write_mask;
		param.Stream = sig->elements[i].stream;
	}
	return count;
}

// sm4_parse.cpp

struct SM4Parser
{
	const unsigned* tokens;
	const unsigned* tokens_end;
	SM4Program& program;

	SM4Parser(SM4Program& program, const void* p_tokens, unsigned size)
	: program(program)
	{
		tokens = (const unsigned*)p_tokens;
		tokens_end = (const unsigned*)((const char*)p_tokens + size);
	}

	UInt32 read32()
	{
		Assert(tokens < tokens_end);
		return *tokens++;
	}

	template<typename T>
	void read_token(T* tok)
	{
		*(unsigned*)tok = read32();
	}

	UInt64 read64()
	{
		unsigned a = read32();
		unsigned b = read32();
		return (UInt64)a | ((UInt64)b << 32);
	}

	void skip(unsigned toskip)
	{
		tokens += toskip;
	}

	bool read_op(SM4Op* pop)
	{
		SM4Op& op = *pop;
		SM4TokOperand optok;
		read_token(&optok);
		if (optok.file >= kSM4File_COUNT)
		{
			AssertString ("DXBC: unknown register type");
			return false;
		}
		op.swizzle[0] = 0;
		op.swizzle[1] = 1;
		op.swizzle[2] = 2;
		op.swizzle[3] = 3;
		op.mask = 0xf;
		switch(optok.comps_enum)
		{
		case kSM4OperComp0:
			op.comps = 0;
			break;
		case kSM4OperComp1:
			op.comps = 1;
			op.swizzle[1] = op.swizzle[2] = op.swizzle[3] = 0;
			break;
		case kSM4OperComp4:
			op.comps = 4;
			op.mode = optok.mode;
			switch(optok.mode)
			{
			case SM4_OPERAND_MODE_MASK:
				op.mask = SM4_OPERAND_SEL_MASK(optok.sel);
				break;
			case SM4_OPERAND_MODE_SWIZZLE:
				op.swizzle[0] = SM4_OPERAND_SEL_SWZ(optok.sel, 0);
				op.swizzle[1] = SM4_OPERAND_SEL_SWZ(optok.sel, 1);
				op.swizzle[2] = SM4_OPERAND_SEL_SWZ(optok.sel, 2);
				op.swizzle[3] = SM4_OPERAND_SEL_SWZ(optok.sel, 3);
				break;
			case SM4_OPERAND_MODE_SCALAR:
				op.swizzle[0] = op.swizzle[1] = op.swizzle[2] = op.swizzle[3] = SM4_OPERAND_SEL_SCALAR(optok.sel);
				break;
			}
			break;
		case kSM4OperCompN:
			AssertString("Unhandled operand component type");
			return false;
			break;
		}
		op.file = (SM4RegFile)optok.file;
		op.num_indices = optok.num_indices;

		if(optok.extended)
		{
			SM4TokOperandEx optokext;
			read_token(&optokext);
			if(optokext.type == 0)
			{}
			else if(optokext.type == 1)
			{
				op.neg = optokext.neg;
				op.abs= optokext.abs;
			}
			else
			{
				AssertString("Unhandled extended operand token type");
				return false;
			}
		}

		for(unsigned i = 0; i < op.num_indices; ++i)
		{
			unsigned repr;
			if(i == 0)
				repr = optok.index0_repr;
			else if(i == 1)
				repr = optok.index1_repr;
			else if(i == 2)
				repr = optok.index2_repr;
			else
			{
				AssertString("Unhandled operand index representation");
				return false;
			}
			op.indices[i].disp = 0;
			// TODO: is disp supposed to be signed here??
			switch(repr)
			{
			case SM4_OPERAND_INDEX_REPR_IMM32:
				op.indices[i].disp = (SInt32)read32();
				break;
			case SM4_OPERAND_INDEX_REPR_IMM64:
				op.indices[i].disp = read64();
				break;
			case SM4_OPERAND_INDEX_REPR_REG:
relative:
				op.indices[i].reg.reset(new SM4Op());
				if (!read_op(&*op.indices[i].reg))
					return false;
				break;
			case SM4_OPERAND_INDEX_REPR_REG_IMM32:
				op.indices[i].disp = (SInt32)read32();
				goto relative;
			case SM4_OPERAND_INDEX_REPR_REG_IMM64:
				op.indices[i].disp = read64();
				goto relative;
			}
		}

		if(op.file == kSM4File_IMMEDIATE32)
		{
			for(unsigned i = 0; i < op.comps; ++i)
				op.imm_values[i].i32 = read32();
		}
		else if(op.file == kSM4File_IMMEDIATE64)
		{
			for(unsigned i = 0; i < op.comps; ++i)
				op.imm_values[i].i64 = read64();
		}
		return true;
	}

	bool parse()
	{
		read_token(&program.version);

		unsigned lentok = read32();
		tokens_end = tokens - 2 + lentok;

		while(tokens != tokens_end)
		{
			SM4TokInstruction insntok;
			read_token(&insntok);
			const unsigned* insn_end = tokens - 1 + insntok.length;
			SM4Opcode opcode = (SM4Opcode)insntok.opcode;
			if (opcode >= kSM4Op_COUNT)
			{
				AssertString ("Unknown DXBC opcode");
				return false;
			}

			if(opcode == kSM4Op_CUSTOMDATA)
			{
				// immediate constant buffer data
				unsigned customlen = read32() - 2;

				SM4Decl& dcl = *new SM4Decl;
				program.dcls.push_back(&dcl);

				dcl.opcode = kSM4Op_CUSTOMDATA;
				dcl.num = customlen;
				dcl.data = malloc(customlen * sizeof(tokens[0]));

				memcpy(dcl.data, &tokens[0], customlen * sizeof(tokens[0]));

				skip(customlen);
				continue;
			}

			if(opcode == kSM4Op_HS_FORK_PHASE || opcode == kSM4Op_HS_JOIN_PHASE)
			{
				// need to interleave these with the declarations or we cannot
				// assign fork/join phase instance counts to phases
				SM4Decl& dcl = *new SM4Decl;
				program.dcls.push_back(&dcl);
				dcl.opcode = opcode;
			}

			if((opcode >= kSM4Op_DCL_RESOURCE && opcode <= kSM4Op_DCL_GLOBAL_FLAGS)
				|| (opcode >= kSM4Op_DCL_STREAM && opcode <= kSM4Op_DCL_RESOURCE_STRUCTURED))
			{
				SM4Decl& dcl = *new SM4Decl;
				program.dcls.push_back(&dcl);
				(SM4TokInstruction&)dcl = insntok;

				SM4TokInstructionEx exttok;
				memcpy(&exttok, &insntok, sizeof(exttok));
				while(exttok.extended)
				{
					read_token(&exttok);
				}

#define READ_OP_ANY dcl.op.reset(new SM4Op()); if (!read_op(&*dcl.op)) return false;
#define READ_OP(FILE) READ_OP_ANY
				//Assert(dcl.op->file == kSM4File_##FILE);

				switch(opcode)
				{
				case kSM4Op_DCL_GLOBAL_FLAGS:
					break;
				case kSM4Op_DCL_RESOURCE:
					READ_OP(RESOURCE);
					read_token(&dcl.rrt);
					break;
				case kSM4Op_DCL_SAMPLER:
					READ_OP(SAMPLER);
					break;
				case kSM4Op_DCL_INPUT:
				case kSM4Op_DCL_INPUT_PS:
					READ_OP(INPUT);
					break;
				case kSM4Op_DCL_INPUT_SIV:
				case kSM4Op_DCL_INPUT_SGV:
				case kSM4Op_DCL_INPUT_PS_SIV:
				case kSM4Op_DCL_INPUT_PS_SGV:
					READ_OP(INPUT);
					dcl.sv = (SM4SystemValue)(UInt16)read32();
					break;
				case kSM4Op_DCL_OUTPUT:
					READ_OP(OUTPUT);
					break;
				case kSM4Op_DCL_OUTPUT_SIV:
				case kSM4Op_DCL_OUTPUT_SGV:
					READ_OP(OUTPUT);
					dcl.sv = (SM4SystemValue)(UInt16)read32();
					break;
				case kSM4Op_DCL_INDEX_RANGE:
					READ_OP_ANY;
					Assert(dcl.op->file == kSM4File_INPUT || dcl.op->file == kSM4File_OUTPUT);
					dcl.num = read32();
					break;
				case kSM4Op_DCL_TEMPS:
					dcl.num = read32();
					break;
				case kSM4Op_DCL_INDEXABLE_TEMP:
					READ_OP(INDEXABLE_TEMP);
					dcl.indexable_temp.num = read32();
					dcl.indexable_temp.comps = read32();
					break;
				case kSM4Op_DCL_CONSTANT_BUFFER:
					READ_OP(CONSTANT_BUFFER);
					break;
				case kSM4Op_DCL_GS_INPUT_PRIMITIVE:
				case kSM4Op_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY:
					break;
				case kSM4Op_DCL_MAX_OUTPUT_VERTEX_COUNT:
					dcl.num = read32();
					break;
				case kSM4Op_DCL_GS_INSTANCE_COUNT:
					dcl.num = read32();
					break;
				case kSM4Op_DCL_INPUT_CONTROL_POINT_COUNT:
				case kSM4Op_DCL_OUTPUT_CONTROL_POINT_COUNT:
				case kSM4Op_DCL_TESS_DOMAIN:
				case kSM4Op_DCL_TESS_PARTITIONING:
				case kSM4Op_DCL_TESS_OUTPUT_PRIMITIVE:
					break;
				case kSM4Op_DCL_HS_MAX_TESSFACTOR:
					dcl.f32 = read32();
					break;
				case kSM4Op_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
					dcl.num = read32();
					break;
				case kSM4Op_DCL_FUNCTION_BODY:
					dcl.num = read32();
					break;
				case kSM4Op_DCL_FUNCTION_TABLE:
					dcl.num = read32();
					dcl.data = malloc(dcl.num * sizeof(UInt32));
					for(unsigned i = 0; i < dcl.num; ++i)
						((UInt32*)dcl.data)[i] = read32();
					break;
				case kSM4Op_DCL_INTERFACE:
					dcl.intf.id = read32();
					dcl.intf.expected_function_table_length = read32();
					{
						UInt32 v = read32();
						dcl.intf.table_length = v & 0xffff;
						dcl.intf.array_length = v >> 16;
					}
					dcl.data = malloc(dcl.intf.table_length * sizeof(UInt32));
					for(unsigned i = 0; i < dcl.intf.table_length; ++i)
						((UInt32*)dcl.data)[i] = read32();
					break;
				case kSM4Op_DCL_THREAD_GROUP:
					dcl.thread_group_size[0] = read32();
					dcl.thread_group_size[1] = read32();
					dcl.thread_group_size[2] = read32();
					break;
				case kSM4Op_DCL_UNORDERED_ACCESS_VIEW_TYPED:
					READ_OP(UNORDERED_ACCESS_VIEW);
					read_token(&dcl.rrt);
					break;
				case kSM4Op_DCL_UNORDERED_ACCESS_VIEW_RAW:
					READ_OP(UNORDERED_ACCESS_VIEW);
					break;
				case kSM4Op_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:
					READ_OP(UNORDERED_ACCESS_VIEW);
					dcl.structured.stride = read32();
					break;
				case kSM4Op_DCL_THREAD_GROUP_SHARED_MEMORY_RAW:
					READ_OP(THREAD_GROUP_SHARED_MEMORY);
					dcl.num = read32();
					break;
				case kSM4Op_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED:
					READ_OP(THREAD_GROUP_SHARED_MEMORY);
					dcl.structured.stride = read32();
					dcl.structured.count = read32();
					break;
				case kSM4Op_DCL_RESOURCE_RAW:
					READ_OP(RESOURCE);
					break;
				case kSM4Op_DCL_RESOURCE_STRUCTURED:
					READ_OP(RESOURCE);
					dcl.structured.stride = read32();
					break;
				case kSM4Op_DCL_STREAM:
					// TODO: dcl_stream is undocumented: what is it?
					AssertString("DXBC: Unhandled dcl_stream since it's undocumented");
					return false;
					break;
				default:
					AssertString("DXBC: Unhandled declaration type");
					return false;
				}

				if (tokens != insn_end)
				{
					AssertString("DXBC: token size mismatch");
					return false;
				}
			}
			else
			{
				SM4Instr& insn = *new SM4Instr;
				program.insns.push_back(&insn);
				(SM4TokInstruction&)insn = insntok;

				SM4TokInstructionEx exttok;
				memcpy(&exttok, &insntok, sizeof(exttok));
				while(exttok.extended)
				{
					read_token(&exttok);
					if(exttok.type == SM4_TOKEN_INSTRUCTION_EXTENDED_TYPE_SAMPLE_CONTROLS)
					{
						insn.sample_offset[0] = exttok.sample_controls.offset_u;
						insn.sample_offset[1] = exttok.sample_controls.offset_v;
						insn.sample_offset[2] = exttok.sample_controls.offset_w;
					}
					else if(exttok.type == SM4_TOKEN_INSTRUCTION_EXTENDED_TYPE_RESOURCE_DIM)
						insn.resource_target = exttok.resource_target.target;
					else if(exttok.type == SM4_TOKEN_INSTRUCTION_EXTENDED_TYPE_RESOURCE_RETURN_TYPE)
					{
						insn.resource_return_type[0] = exttok.resource_return_type.x;
						insn.resource_return_type[1] = exttok.resource_return_type.y;
						insn.resource_return_type[2] = exttok.resource_return_type.z;
						insn.resource_return_type[3] = exttok.resource_return_type.w;
					}
				}

				switch(opcode)
				{
				case kSM4Op_INTERFACE_CALL:
					insn.num = read32();
					break;
				default:
					break;
				}

				unsigned op_num = 0;
				while(tokens != insn_end)
				{
					if (tokens >= insn_end)
					{
						AssertString ("DXBC: token size mismatch");
						return false;
					}
					if (op_num >= SM4_MAX_OPS)
					{
						AssertString ("DXBC: too many operands");
						return false;
					}
					insn.ops[op_num].reset(new SM4Op);
					if (!read_op(&*insn.ops[op_num]))
						return false;
					++op_num;
				}
				insn.num_ops = op_num;
			}
		}
		return true;
	}
};

SM4Program* sm4_parse(const void* tokens, int size)
{
	SM4Program* program = new SM4Program;
	SM4Parser parser(*program, tokens, size);
	if(parser.parse())
		return program;
	delete program;
	return 0;
}

static void dxbc_create_internal(struct DXBCChunkHeader** chunks, unsigned num_chunks, void* buffer, unsigned total_size)
{
	DXBCHeader* header = (DXBCHeader*)buffer;

	header->fourcc = kFOURCC_DXBC;
	memset(header->hash, 0, sizeof(header->hash));

	header->one = 1;
	header->total_size = total_size;
	header->chunk_count = num_chunks;

	UInt32* chunk_offsets = (UInt32*)(header + 1);
	UInt32 off = sizeof(struct DXBCHeader) + num_chunks * sizeof(UInt32);
	for(unsigned i = 0; i < num_chunks; ++i)
	{
		chunk_offsets[i] = off;
		unsigned chunk_full_size = sizeof(DXBCChunkHeader) + chunks[i]->size;
		memcpy((char*)header + off, chunks[i], chunk_full_size);
		off += chunk_full_size;
	}

	void D3DHash (const unsigned char* data, unsigned size, unsigned char res[16]);
	D3DHash ((const UInt8*)&header->one, total_size-20, (UInt8*)header->hash);
}


static std::pair<void*, size_t> dxbc_create(struct DXBCChunkHeader** chunks, unsigned num_chunks)
{
	size_t data_size = 0;
	for(unsigned i = 0; i < num_chunks; ++i)
		data_size += sizeof(UInt32) + sizeof(DXBCChunkHeader) + chunks[i]->size;
	const size_t total_size = sizeof(DXBCHeader) + data_size;

	void* buffer = malloc(total_size);
	if (!buffer)
		return std::make_pair((void*)0, 0);

	dxbc_create_internal (chunks, num_chunks, buffer, total_size);

	return std::make_pair(buffer, total_size);
}


void dxbc_create(struct DXBCChunkHeader** chunks, unsigned num_chunks, dynamic_array<UInt8>& out)
{
	size_t data_size = 0;
	for(unsigned i = 0; i < num_chunks; ++i)
		data_size += sizeof(UInt32) + sizeof(DXBCChunkHeader) + chunks[i]->size;
	const size_t total_size = sizeof(DXBCHeader) + data_size;

	out.resize_uninitialized (total_size);

	dxbc_create_internal (chunks, num_chunks, out.data(), total_size);
}




// -------------------------------------------------------------------

enum DXBCBuilderChunks {
	kBuilderChunkInput = 0,
	kBuilderChunkOutput,
	kBuilderChunkCode,
	kBuilderChunkSM20,
	kBuilderChunkCount
};
struct SigElement {
	const char* name;
	int index;
	int reg;
	int mask;
};

struct DXBCCodeBuilder
{
	DXBCCodeBuilder(dynamic_array<UInt32>& destArray)
		: insns(destArray)
		, curInsnIndex(-1)
		, tempCount(0)
		, opcode(kSM4Op_MOV)
		, opcode2(kSM2Op_MOV)
		, saturate(0)
	{ }
	dynamic_array<UInt32>& insns; // reference to actual output array!
	int		curInsnIndex;
	int		tempCount;

	SM4Opcode	opcode;
	SM2Opcode	opcode2;

	int saturate;
	char dstRegType;
	int  dstRegIndex;
	unsigned dstRegMask;
	unsigned dstRegComps;
	int tmpSatRegIndex;
};

struct DXBCBuilder
{
	DXBCBuilder() : codeBuilder(insns) { }
	DXBCCodeBuilder codeBuilder;
	dynamic_array<SigElement> inputs;
	dynamic_array<SigElement> outputs;
	dynamic_array<UInt32> insns;
	dynamic_array<UInt32> dcls;
	//sm20 stuff
	dynamic_array<UInt32>   inputs2;
	dynamic_array<UInt32>   defs2;
	dynamic_array<UInt32>   insns2;
	std::map<UInt32,UInt32> outputMap;
	std::map<UInt32,UInt32> inputMap;

	int                     numTextures2;
	int                     curInsnIndex2;

	SM4TokVersion version;
	DXBCChunkHeader* chunks[kBuilderChunkCount];
};

DXBCCodeBuilder* dxb_create_code(dynamic_array<UInt32>& destArray)
{
	DXBCCodeBuilder* b = new DXBCCodeBuilder(destArray);
	return b;
}

void dxb_destroy_code(DXBCCodeBuilder* b)
{
	delete b;
}

const UInt32* dxb_get_code(DXBCCodeBuilder* b, size_t* outSize)
{
	*outSize = b->insns.size();
	return b->insns.data();
}


DXBCBuilder* dxb_create(int major, int minor, SM4ShaderType type)
{
	DXBCBuilder* b = new DXBCBuilder();
	memset (b->chunks, 0, sizeof(b->chunks));
	b->version.major = major;
	b->version.minor = minor;
	b->version.type = type;
	b->version.format = 0; //@TODO?
  b->numTextures2 = 0;

	return b;
}
void dxb_destroy(DXBCBuilder* b)
{
	if (b)
	{
		for (int i = 0; i < kBuilderChunkCount; ++i)
			if (b->chunks[i])
				free(b->chunks[i]);
	}
	delete b;
}

DXBCCodeBuilder* dxb_get_code_builder(DXBCBuilder* b)
{
	return &b->codeBuilder;
}


void dxb_dcl_input (DXBCBuilder* b, const char* name, int index, int reg, int mask)
{
	SigElement el;
	el.name = name;
	el.index = index;
	el.reg = reg;
	el.mask = mask;
	b->inputs.push_back (el);

	dxb_dcl_input2(b,name,index,reg,mask);
}

void dxb_dcl_output (DXBCBuilder* b, const char* name, int index, int reg, int mask)
{
	SigElement el;
	el.name = name;
	el.index = index;
	el.reg = reg;
	el.mask = mask;
	b->outputs.push_back (el);

	dxb_dcl_output2(b,name,index,reg,mask);
}


void dxb_dcl_tex (DXBCBuilder* b, int index, SM4Target dim)
{
	SM4TokInstruction dcl;
	dcl.dword = 0;
	dcl.opcode = kSM4Op_DCL_SAMPLER;
	dcl.length = 3;
	b->dcls.push_back (dcl.dword);
	SM4TokOperand op;
	op.dword = 0;
	op.file = kSM4File_SAMPLER;
	op.num_indices = 1;
	b->dcls.push_back (op.dword);
	b->dcls.push_back (index);

	dcl.dword = 0;
	dcl.opcode = kSM4Op_DCL_RESOURCE;
	dcl.length = 4;
	dcl.dcl_resource.target = dim;
	b->dcls.push_back (dcl.dword);
	op.dword = 0;
	op.file = kSM4File_RESOURCE;
	op.num_indices = 1;
	SM4TokResourceRetType ret;
	ret.dword = 0;
	ret.x = ret.y = ret.z = ret.w = kSM4RetType_FLOAT;
	b->dcls.push_back (op.dword);
	b->dcls.push_back (index);
	b->dcls.push_back (ret.dword);

	dxb_dcl_tex2 (b, index, dim);
}


void dxb_dcl_cb (DXBCBuilder* b, int index, int size)
{
	SM4TokInstruction dcl;
	dcl.dword = 0;
	dcl.opcode = kSM4Op_DCL_CONSTANT_BUFFER;
	dcl.length = 4;
	dcl.dcl_constant_buffer.dynamic = 0;
	b->dcls.push_back (dcl.dword);
	SM4TokOperand op;
	op.dword = 0;
	op.comps_enum = kSM4OperComp4;
	op.file = kSM4File_CONSTANT_BUFFER;
	op.num_indices = 2;
	b->dcls.push_back (op.dword);
	b->dcls.push_back (index);
	b->dcls.push_back (size);
}


static void dxbc_update_insn_length (DXBCCodeBuilder* b, int len)
{
	DebugAssert (b->curInsnIndex >= 0 && b->curInsnIndex < b->insns.size());
	SM4TokInstruction* tok = (SM4TokInstruction*)&b->insns[b->curInsnIndex];
	tok->length += len;
}

void dxb_op (DXBCCodeBuilder* b, SM4Opcode op, bool sat)
{
	b->opcode = op;
	b->curInsnIndex = b->insns.size();
	SM4TokInstruction tok;
	tok.dword = 0;
	tok.opcode = op;
	tok.insn.sat = sat ? 1 : 0;
	tok.length = 1;
	if (op == kSM4Op_DISCARD) // discard instructions emitted by HLSL have nz flag set
		tok.insn.test_nz = 1;
	b->insns.push_back (tok.dword);
}

static inline SM4RegFile dxb_reg_type(char c)
{
	switch (c)
	{
	case 'r': return kSM4File_TEMP;
	case 'v': return kSM4File_INPUT;
	case 'o': return kSM4File_OUTPUT;
	case 'c': return kSM4File_CONSTANT_BUFFER;
	case 's': return kSM4File_SAMPLER;
	case 't': return kSM4File_RESOURCE;
	default: AssertString("unknown register type"); return kSM4File_TEMP;
	}
}


void dxb_reg (DXBCCodeBuilder* b, char rchar, int reg, unsigned mask)
{
	b->dstRegMask = mask;
	b->dstRegComps = BitsInMask(mask);

	SM4RegFile rtype = dxb_reg_type(rchar);
	dxbc_update_insn_length (b, 2);
	SM4TokOperand op;
	op.dword = 0;
	op.comps_enum = rtype==kSM4File_SAMPLER ? kSM4OperComp0 : kSM4OperComp4;

	// discard only has 1 parameter which is similar to a source register argument:
	// if it's one channel only, then emit it as a "scalar channel index"
	op.mode = (b->dstRegComps==1 && b->opcode==kSM4Op_DISCARD) ? SM4_OPERAND_MODE_SCALAR : SM4_OPERAND_MODE_MASK;
	op.sel = (b->dstRegComps==1 && b->opcode==kSM4Op_DISCARD) ? HighestBit(mask) : mask;
	if (rtype==kSM4File_SAMPLER)
		op.sel = 0;
	op.file = rtype;
	op.num_indices = 1;
	b->insns.push_back (op.dword);
	b->insns.push_back (reg);
	if (rtype == kSM4File_TEMP)
		b->tempCount = std::max (b->tempCount, reg+1);
}


static unsigned adjust_swizzle (DXBCCodeBuilder* b, unsigned swizzle, bool sm2)
{
	// For DP3 with "no swizzle", SM2 expects to not have the swizzle, while SM4 expects xyzx
	if (b->opcode == kSM4Op_DP3 && swizzle == kSM4SwzNone && !sm2)
		swizzle = kSM4SwzXYZX;

	// arguments for single-channel destinations in SM4 should be emitted as "1 channel index"
	// instead of "swizzle that replicates all channels"
	if (!sm2 && b->dstRegComps==1)
	{
		if (swizzle == kSM4SwzRepX)
			swizzle = 0;
		else if (swizzle == kSM4SwzRepY)
			swizzle = 1;
		else if (swizzle == kSM4SwzRepZ)
			swizzle = 2;
		else if (swizzle == kSM4SwzRepW)
			swizzle = 3;
	}
	return swizzle;
}


void dxb_swz (DXBCCodeBuilder* b, char rchar, int reg, unsigned swizzle, bool neg)
{
	swizzle = adjust_swizzle (b, swizzle, false);

	SM4RegFile rtype = dxb_reg_type(rchar);
	int len = 2;
	if (neg) ++len;
	if (rtype==kSM4File_CONSTANT_BUFFER) ++len;
	dxbc_update_insn_length (b, len);
	SM4TokOperand op;
	op.dword = 0;
	op.comps_enum = rtype==kSM4File_SAMPLER ? kSM4OperComp0 : kSM4OperComp4;
	op.mode = (b->dstRegComps==1 && swizzle<4) ? SM4_OPERAND_MODE_SCALAR : SM4_OPERAND_MODE_SWIZZLE;
	op.sel = swizzle;
	op.file = rtype;
	op.num_indices = rtype==kSM4File_CONSTANT_BUFFER ? 2 : 1;
	op.extended = neg ? 1 : 0;
	b->insns.push_back (op.dword);
	if (neg)
	{
		SM4TokOperandEx opex;
		opex.dword = 0;
		opex.type = 1;
		opex.neg = 1;
		b->insns.push_back (opex.dword);
	}
	if (rtype==kSM4File_CONSTANT_BUFFER)
	{
		b->insns.push_back (reg >> 16);
		b->insns.push_back (reg & 0xFFFF);
	}
	else
	{
		b->insns.push_back (reg);
	}
	if (rtype == kSM4File_TEMP)
		b->tempCount = std::max (b->tempCount, reg+1);
}

void dxb_float1 (DXBCCodeBuilder* b, float v)
{
	dxbc_update_insn_length (b, 2);
	SM4TokOperand op;
	op.dword = 0;
	op.comps_enum = kSM4OperComp1;
	op.file = kSM4File_IMMEDIATE32;
	b->insns.push_back (op.dword);
	union { float f; UInt32 i; } f2i;
	f2i.f = v;
	b->insns.push_back (f2i.i);
}

void dxb_int1 (DXBCCodeBuilder* b, int i)
{
	dxbc_update_insn_length (b, 2);
	SM4TokOperand op;
	op.dword = 0;
	op.comps_enum = kSM4OperComp1;
	op.file = kSM4File_IMMEDIATE32;
	b->insns.push_back (op.dword);
	b->insns.push_back (i);
}


void dxb_float4 (DXBCCodeBuilder* b, float v0, float v1, float v2, float v3)
{
	dxbc_update_insn_length (b, 5);
	SM4TokOperand op;
	op.dword = 0;
	op.comps_enum = kSM4OperComp4;
	op.file = kSM4File_IMMEDIATE32;
	b->insns.push_back (op.dword);
	union { float f; UInt32 i; } f2i;
	f2i.f = v0; b->insns.push_back (f2i.i);
	f2i.f = v1; b->insns.push_back (f2i.i);
	f2i.f = v2; b->insns.push_back (f2i.i);
	f2i.f = v3; b->insns.push_back (f2i.i);
}



static void dxbc_builder_build_sig (DXBCBuilder* b, bool input)
{
	int chunkIdx = input ? kBuilderChunkInput : kBuilderChunkOutput;
	const dynamic_array<SigElement>& sigs = input ? b->inputs : b->outputs;
	const int nsigs = sigs.size();

	// size = 8 (header) + 8 (sig chunk header) + count * DXBCSignatureElement + names
	unsigned size = 16 + nsigs * sizeof(DXBCSignatureElement);
	unsigned namesOffset = size;
	for (int i = 0; i < nsigs; ++i)
		size += strlen(sigs[i].name) + 1;
	size = (size + 3) & ~3; // align to next dword

	UInt8* buf = (UInt8*)malloc(size);
	DXBCChunkSig* chunk = (DXBCChunkSig*)buf;
	memset (chunk, 0xAB, size);

	chunk->fourcc = input ? kFOURCC_ISGN : kFOURCC_OSGN;
	chunk->size = size-8;
	chunk->count = nsigs;
	chunk->unk8 = 8;
	for (int i = 0; i < nsigs; ++i)
	{
		DXBCSignatureElement* s = &chunk->elements[i];
		s->name_offset = namesOffset-8;
		int len = strlen(sigs[i].name) + 1;
		memcpy (buf + namesOffset, sigs[i].name, len);
		namesOffset += len;
		const bool outputPos = !strcmp(sigs[i].name,"SV_POSITION");
		unsigned mask = sigs[i].mask;
		s->semantic_index = sigs[i].index;
		s->system_value_type = outputPos ? 1 : 0;
		s->component_type = 3; // float
		s->register_num = sigs[i].reg;
		s->mask = mask; // mask of channels used
		if (input)
			s->read_write_mask = mask;
		else
			s->read_write_mask = ~mask & 0xF;
		s->stream = 0;
		s->unused = 0;


		SM4TokInstruction dcl;
		dcl.dword = 0;
		dcl.opcode = input ?
			(b->version.type == kSM4Shader_Pixel ? kSM4Op_DCL_INPUT_PS : kSM4Op_DCL_INPUT) :
			(outputPos ? kSM4Op_DCL_OUTPUT_SIV : kSM4Op_DCL_OUTPUT);
		dcl.length = outputPos ? 4 : 3;
		if (dcl.opcode == kSM4Op_DCL_INPUT_PS)
			dcl.dcl_input_ps.interpolation = kSM4Interp_LINEAR;
		b->dcls.push_back (dcl.dword);
		SM4TokOperand op;
		op.dword = 0;
		op.comps_enum = kSM4OperComp4;
		op.mode = SM4_OPERAND_MODE_MASK;
		op.sel = mask;
		op.file = input ? kSM4File_INPUT : kSM4File_OUTPUT;
		op.num_indices = 1;
		op.index0_repr = SM4_OPERAND_INDEX_REPR_IMM32;
		b->dcls.push_back (op.dword);
		b->dcls.push_back (sigs[i].reg);
		if (outputPos)
			b->dcls.push_back (s->system_value_type);
	}

	b->chunks[chunkIdx] = chunk;
}


static void dxbc_builder_build_code (DXBCBuilder* b)
{
	const unsigned n = b->dcls.size() + b->insns.size();

	// size = 8 (header) + 8 (code chunk header) + tokens
	unsigned size = 16 + n*4;

	UInt8* buf = (UInt8*)malloc(size);
	DXBCChunkCode* chunk = (DXBCChunkCode*)buf;

	chunk->fourcc = kFOURCC_SHDR;
	chunk->size = size-8;
	chunk->version = *(UInt32*)&b->version;
	chunk->length = n+2;

	UInt8* codePtr = (UInt8*)(chunk+1);
	if (!b->dcls.empty())
	{
		size_t size = b->dcls.size()*sizeof(b->dcls[0]);
		memcpy (codePtr, &b->dcls[0], size);
		codePtr += size;
	}
	if (!b->insns.empty())
	{
		size_t size = b->insns.size()*sizeof(b->insns[0]);
		memcpy (codePtr, &b->insns[0], size);
		codePtr += size;
	}
	DebugAssert (buf + size == codePtr);

	b->chunks[kBuilderChunkCode] = chunk;
}

void dxbc_builder_build_code2 (DXBCBuilder* b);

void* dxb_build (DXBCBuilder* b, size_t& outSize)
{
	dxbc_builder_build_sig (b, true);
	dxbc_builder_build_sig (b, false);
	if (b->codeBuilder.tempCount > 0)
	{
		SM4TokInstruction tok;
		tok.dword = 0;
		tok.opcode = kSM4Op_DCL_TEMPS;
		tok.length = 2;
		b->dcls.push_back (tok.dword);
		b->dcls.push_back (b->codeBuilder.tempCount);
	}
	dxbc_builder_build_code (b);
	dxbc_builder_build_code2(b);

	std::pair<void*,size_t> dxbc = dxbc_create (b->chunks, kBuilderChunkCount);
	outSize = dxbc.second;

	return dxbc.first;
}



//------------------------------------------------------------------------------------
//SM 2.0 stuff


//table format was reversed and is still not 100% known
static dynamic_array<UInt32> dxbc_builder_build_mapping_table2 (DXBCBuilder* b)
{
	const unsigned shader_size = b->inputs2.size() + b->insns2.size() + b->defs2.size() + 1;
	dynamic_array<UInt32> table;

	const unsigned rmap_offset = 0x24;
	const unsigned cmap_offset = rmap_offset + b->numTextures2*4;
	const unsigned imap_offset = cmap_offset + 3*4;
	const unsigned shdr_offset = imap_offset + (b->version.type == kSM4Shader_Vertex ? 4 : 0);

	unsigned t = b->version.type == kSM4Shader_Pixel ? 4 : 0;
	table.push_back(b->version.type == kSM4Shader_Pixel ? 0xffff0200 : 0xfffe0200);
	table.push_back(shader_size*4);
	table.push_back(shdr_offset);
	table.push_back(1|(cmap_offset<<16));
	table.push_back(shdr_offset<<16);
	table.push_back(shdr_offset<<16);
	table.push_back(b->numTextures2|(rmap_offset<<16));
	table.push_back((b->version.type == kSM4Shader_Pixel ? 0 : 1)|(imap_offset<<16));

	// Sampler mapping: 16-23 bits target sampler, 8-15 bits source sampler, 0-7 bits source resource
	for (int n = 0;n < b->numTextures2;n++)
		table.push_back(n|(n<<8)|(n<<16));

	//constant to CB mapping
	//CBn:CBoff; always zero
	table.push_back(0);
	//REGnum:REGn
	table.push_back(b->version.type == kSM4Shader_Pixel ? 10 : 63); //k11VertexSize or k11PixelSize
	//convert mode 8:8:8:8; always zero
	table.push_back(0);

	//input mapping; always one constant for vertex shader with pixel offset: oPos.xy += offset.xy * oPos.w;
	if (b->version.type == kSM4Shader_Vertex)
		table.push_back(64<<16);  //k11VertexPosOffset9x

	return table;
}


static unsigned swizzle_from_mask (unsigned mask)
{
	if (mask == 1)
		return kSM4SwzRepX;
	if (mask == 2)
		return kSM4SwzRepY;
	if (mask == 4)
		return kSM4SwzRepZ;
	if (mask == 8)
		return kSM4SwzRepW;
	//@TODO: more?
	return kSM4SwzNone;
}

static void dxb_handle_saturate2 (DXBCBuilder* b)
{
	if (!b->codeBuilder.saturate || b->version.type == kSM4Shader_Pixel)
		return;

	// previous instruction had saturate on destination, so insert manual min/max instructions
	const int tmpReg = b->codeBuilder.tmpSatRegIndex;
	DebugAssert(tmpReg >= 0);
	b->codeBuilder.saturate = 0;

	const int constReg = dxb_imm_f4 (b, 0, 1, 0, 0);
	const unsigned mask = b->codeBuilder.dstRegMask;
	const unsigned swz = swizzle_from_mask(mask);

	dxb_op2(b, kSM2Op_MAX, false);
	dxb_reg2(b,'r',tmpReg,mask);
	dxb_swz2(b,'r',tmpReg,swz);
	dxb_swz2(b,'c',constReg,kSM4SwzRepX);
	dxb_op2(b, kSM2Op_MIN, false);
	dxb_reg2(b,b->codeBuilder.dstRegType,b->codeBuilder.dstRegIndex,mask);
	dxb_swz2(b,'r',tmpReg,swz);
	dxb_swz2(b,'c',constReg,kSM4SwzRepY);
}

static void dxbc_builder_build_code2 (DXBCBuilder* b)
{
	dxb_handle_saturate2(b); // handle possible saturate on the last instruction

	dynamic_array<UInt32> table = dxbc_builder_build_mapping_table2(b);

	const unsigned shader_size = b->inputs2.size() + b->insns2.size() + b->defs2.size() + 1;

	// size = 8 (header) + 8 (code chunk header) + tokens
	unsigned size = 12 + 4*table.size() + shader_size*4;

	UInt8* buf = (UInt8*)malloc(size);
	DXBCChunkSM20* chunk = (DXBCChunkSM20*)buf;

	chunk->fourcc = kFOURCC_SM20;
	chunk->size = size - 8;
	chunk->length = chunk->size;

	UInt8* codePtr = (UInt8*)(chunk+1);
	if (!table.empty())
	{
		size_t size = table.size()*sizeof(table[0]);
		memcpy (codePtr, &table[0], size);
		codePtr += size;
	}

	*(UInt32*)codePtr = b->version.type == kSM4Shader_Pixel ? 0xffff0201 : 0xfffe0201;
	codePtr += 4;

	if (!b->defs2.empty())
	{
		size_t size = b->defs2.size()*sizeof(b->defs2[0]);
		memcpy (codePtr, &b->defs2[0], size);
		codePtr += size;
	}
	if (!b->inputs2.empty())
	{
		size_t size = b->inputs2.size()*sizeof(b->inputs2[0]);
		memcpy (codePtr, &b->inputs2[0], size);
		codePtr += size;
	}
	if (!b->insns2.empty())
	{
		size_t size = b->insns2.size()*sizeof(b->insns2[0]);
		memcpy (codePtr, &b->insns2[0], size);
		codePtr += size;
	}

	b->chunks[kBuilderChunkSM20] = chunk;
}


struct SM2TokSrc
{
	union {
		UInt32 dword;
		struct {
			unsigned reg_num    : 11;
			unsigned reg_type34 : 2;
			unsigned _res0      : 1;
			unsigned _res1      : 2;
			unsigned swizzle    : 8;
			unsigned src_mod    : 4;
			unsigned reg_type02 : 3;
			unsigned _one       : 1;
		};
	};
};

static inline UInt32 dxb_reg_split2(UInt32 n)
{
	return ((n<<D3DSP_REGTYPE_SHIFT)&D3DSP_REGTYPE_MASK)|((n<<(D3DSP_REGTYPE_SHIFT2))&D3DSP_REGTYPE_MASK2);
}

static inline UInt32 dxb_reg_type2(char c)
{
	switch (c)
	{
	case 'r': return D3DSPR_TEMP;
	case 'v': return D3DSPR_INPUT;
	case 'c': return D3DSPR_CONST;
		//should be D3DSPR_TEXTURE, however, I'm replacing
		//t swizzle with sampler source and removing s source reg 
		//to convert sm40 sample to sm20 texld on-fly
	case 't': return D3DSPR_SAMPLER;
		//	case 's': return D3DSPR_SAMPLER;

	case 'x': return D3DSPR_RASTOUT;
	case 'y': return D3DSPR_TEXCRDOUT;

	case 'o': return D3DSPR_RASTOUT;

	default: AssertString("unknown register type"); return D3DSPR_TEMP;
	}
}

static void dxbc_update_insn_length2 (DXBCBuilder* b, int len)
{
	DebugAssert (b->curInsnIndex2 >= 0 && b->curInsnIndex2 < b->insns2.size());
	SM2TokInstruction* tok = (SM2TokInstruction*)&b->insns2[b->curInsnIndex2];
	tok->length += len;
}

struct SNameToID
{
	const char* name;
	UInt32 id;
};


//SM4.0 semantic to SM2.0 VS usage mapping
static const SNameToID s_VSDeclNames[] = 
{
	{"POSITION",    D3DDECLUSAGE_POSITION},
	{"NORMAL",      D3DDECLUSAGE_NORMAL},
	{"TEXCOORD",    D3DDECLUSAGE_TEXCOORD},
	{"COLOR",       D3DDECLUSAGE_COLOR},
	{"FOG",         D3DDECLUSAGE_FOG},

	{"SV_POSITION", 0},

	{NULL,      0},
};


//SM4.0 semantic to SM2.0 PS register file mapping
static const SNameToID s_PSDeclNames[] = 
{
	{"TEXCOORD",    D3DSPR_TEXTURE},
	{"COLOR",       D3DSPR_INPUT},

	{"SV_Target",   D3DSPR_COLOROUT},

	{NULL,      0},
};


//SM4.0 semantic to SM2.0 register file mapping
static const SNameToID s_OutNames[] = 
{
	{"POSITION",    D3DSPR_RASTOUT},
	{"SV_POSITION", D3DSPR_RASTOUT},
	{"FOG",         D3DSPR_RASTOUT},
	{"TEXCOORD",    D3DSPR_TEXCRDOUT},
	{"COLOR",       D3DSPR_ATTROUT},

	{"SV_Target",   D3DSPR_COLOROUT},

	{NULL,      0},
};


static UInt32 find_dcl_by_name(const char* name,const SNameToID* p=s_VSDeclNames)
{
	while (p->name)
	{
		if (0 == strcmp(p->name,name))
			return p->id;
		p++;
	}

	Assert(0 || "DCL name not found!");

	return -1;
}

void dxb_dcl_tex2 (DXBCBuilder* b, int index, SM4Target dim)
{
	UInt32 tok2 = 0x80000000;

	switch (dim)
	{
	case kSM4Target_TEXTURE2D:
		tok2 |= D3DSTT_2D;
		break;
	case kSM4Target_TEXTURE3D:
		tok2 |= D3DSTT_VOLUME;
		break;
	case kSM4Target_TEXTURECUBE:
		tok2 |= D3DSTT_CUBE;
		break;
	default:
		Assert(0 || "Wrong texture type!");
	};

	SM2TokInstruction tok;
	tok.dword  = 0;
	tok.opcode = kSM2Op_DCL;
	tok.length = 2;

	b->inputs2.push_back (tok.dword);
	b->inputs2.push_back (tok2);

	SM2TokDst op;
	op.dword      = dxb_reg_split2(D3DSPR_SAMPLER);
	op.reg_num    = index;
	op.write_mask = 0xf;
	op._one       = 1;
	b->inputs2.push_back (op.dword);

	b->numTextures2++;
}

//CAVEAT: DX11 feature level 9.x vertex shaders use texcoord semantics for _all_ attributes
int dxb_find_input_by_name2(DXBCBuilder* b,const char* name, int index)
{
	int index2 = 0;
	while (0 != strcmp(b->inputs[index2].name,name) || b->inputs[index2].index != index)
		index2++;

	return index2;
}

int dxb_find_output_by_name2(DXBCBuilder* b,const char* name, int index)
{
	int index2 = 0;
	while (0 != strcmp(b->outputs[index2].name,name) || b->outputs[index2].index != index)
		index2++;

	return index2;
}


//CAVEAT1: DX11 feature level 9.x vertex shaders use texcoord semantics for _all_ attributes
//CAVEAT2: [Qualcomm] DX11 feature level 9.x uses texcoord interpolators for _all_ varyings
void dxb_dcl_input2 (DXBCBuilder* b, const char* name, int index, int reg, int mask)
{
	//TODO: this is not needed anymore. 
	UInt32 decl = find_dcl_by_name(name,b->version.type == kSM4Shader_Vertex ? s_VSDeclNames : s_PSDeclNames);
	UInt32 reg_type = b->version.type == kSM4Shader_Vertex ? dxb_reg_type2('v') : (decl);

	SM2TokInstruction tok;
	tok.dword  = 0;
	tok.opcode = kSM2Op_DCL;
	tok.length = 2;

	b->inputs2.push_back (tok.dword);

	UInt32 tok2 = 0x80000000;
	//CAVEAT1
	if (b->version.type == kSM4Shader_Vertex)
		tok2 |= D3DDECLUSAGE_TEXCOORD|(dxb_find_input_by_name2(b,name,index)<<16);
	b->inputs2.push_back (tok2);

#if 1
	//CAVEAT2
	if (b->version.type == kSM4Shader_Pixel)
	{
		index = dxb_find_input_by_name2(b,name,index);
		reg_type = D3DSPR_TEXTURE;
	}
#endif

	SM2TokDst op;
	op.dword      = dxb_reg_split2(reg_type);
	op.reg_num    = b->version.type == kSM4Shader_Vertex ? reg : index;
	op.write_mask = mask;
	op._one       = 1;
	op._res0      = 0;
	op._res1      = 0;
	b->inputs2.push_back (op.dword);

	if (b->version.type == kSM4Shader_Pixel)
	{
		op.write_mask    = 0;
		b->inputMap[reg] = op.dword;
	}
}

void dxb_dcl_output2 (DXBCBuilder* b, const char* name, int index, int reg, int mask)
{
	UInt32 reg_type = find_dcl_by_name(name,s_OutNames);
#if 1
	//CAVEAT2. Excluding POSITION
	if (b->version.type == kSM4Shader_Vertex && NULL == strstr(name,"POSITION"))
	{
		index = dxb_find_output_by_name2(b,name,index);
		reg_type = D3DSPR_TEXCRDOUT;
	}
#else
	//special case in raster reg. file
	if (0 == strcmp(name,"FOG"))
		index = 2;
#endif

	SM2TokDst op;
	op.dword   = dxb_reg_split2(reg_type);
	op.reg_num = index;

	b->outputMap[reg] = op.dword;
}

static UInt32 dxb_find_output2 (DXBCBuilder* b, int reg)
{
	Assert (b->outputMap.find(reg) != b->outputMap.end());
	return b->outputMap[reg];
}

static UInt32 dxb_find_input2 (DXBCBuilder* b, int reg)
{
	Assert (b->inputMap.find(reg) != b->inputMap.end());
	return b->inputMap[reg];
}


void dxb_op2 (DXBCBuilder* b, SM2Opcode op, bool sat, int scratchTmpRegForSat)
{
	dxb_handle_saturate2 (b);

	b->codeBuilder.opcode2 = op;
	b->curInsnIndex2 = b->insns2.size();

	SM2TokInstruction tok;
	tok.dword    = 0;
	tok.opcode   = op;
	tok.specific = op>>16;
	tok.length   = 0;

	b->insns2.push_back (tok.dword);
	b->codeBuilder.saturate = sat;
	b->codeBuilder.tmpSatRegIndex = scratchTmpRegForSat;
}

void dxb_reg2 (DXBCBuilder* b, char rchar, int reg,unsigned mask)
{
	b->codeBuilder.dstRegMask = mask;
	b->codeBuilder.dstRegComps = BitsInMask(mask);
	if (b->codeBuilder.saturate)
	{
		b->codeBuilder.dstRegType = rchar;
		b->codeBuilder.dstRegIndex = reg;
		if (b->version.type != kSM4Shader_Pixel)
		{
			rchar = 'r';
			reg = b->codeBuilder.tmpSatRegIndex;
		}
	}

	//for sm20 sampler will be emitted by texture coords swizzle source op
	if ('s' == rchar && b->version.type == kSM4Shader_Pixel)
		return;

	dxbc_update_insn_length2 (b, 1);
	SM2TokDst op;

	if ('o' == rchar)
		op.dword = dxb_find_output2(b,reg);
	else if ('v' == rchar && b->version.type == kSM4Shader_Pixel)
		op.dword = dxb_find_input2(b,reg);
	else
	{
		op.dword = dxb_reg_split2(dxb_reg_type2(rchar));
		op.reg_num = reg;
	}

	op.res_mod = 0;
	if (b->codeBuilder.saturate && b->version.type == kSM4Shader_Pixel)
	{
		op.res_mod = D3DSPDM_SATURATE>>D3DSP_DSTMOD_SHIFT;
		b->codeBuilder.saturate = 0;
	}

	op.write_mask = mask;
	op._one       = 1;
	op._res0      = 0;
	op._res1      = 0;

	b->insns2.push_back (op.dword);
}

void dxb_swz2 (DXBCBuilder* b, char rchar, int reg, unsigned swizzle, bool neg)
{
	swizzle = adjust_swizzle (&b->codeBuilder, swizzle, true);

	UInt32 rtype = dxb_reg_type2(rchar);
	dxbc_update_insn_length2 (b, 1);

	SM2TokSrc op;
	if ('o' == rchar)
		op.dword = dxb_find_output2(b,reg);
	else if ('v' == rchar && b->version.type == kSM4Shader_Pixel)
		op.dword = dxb_find_input2(b,reg);
	else
	{
		op.dword = dxb_reg_split2(dxb_reg_type2(rchar));
		op.reg_num = reg;
	}

	//texld must not swizzle
	if ('s' != rchar && 't' != rchar)// && op.reg_type02 != D3DSPR_TEXTURE)
	{
		op.swizzle = swizzle;
		op.src_mod = neg ? 1 : 0;
	}
	else
		op.swizzle = kSM4SwzNone;

	op._one  = 1;
	op._res0 = 0;
	op._res1 = 0;

	b->insns2.push_back (op.dword);
}

//find a constant, define a constant if not found
int dxb_imm_f4 (DXBCBuilder* b, float v0, float v1, float v2, float v3)
{
	union { float f; UInt32 i; } f2i;
	//TODO: do not hardcode!
	int reg = b->version.type == kSM4Shader_Pixel ? 10 : 63 + 2;
	dynamic_array<UInt32>::const_iterator it = b->defs2.begin();
	for (;it != b->defs2.end();it += 6, reg++)
	{
		f2i.f = v0;
		if (f2i.i != *(it + 2))
			continue;
		f2i.f = v1;
		if (f2i.i != *(it + 3))
			continue;
		f2i.f = v2;
		if (f2i.i != *(it + 4))
			continue;
		f2i.f = v3;
		if (f2i.i != *(it + 5))
			continue;

		break;
	}

	if (it == b->defs2.end())
	{
		SM2TokInstruction tok;
		tok.dword = 0;
		tok.opcode = kSM2Op_DEF;
		tok.specific = 0;
		tok.length = 5;
		b->defs2.push_back (tok.dword);

		SM2TokDst op;
		op.dword      = dxb_reg_split2(dxb_reg_type2('c'));
		op.reg_num    = reg;
		op.write_mask = 0xF;
		op._one       = 1;
		b->defs2.push_back (op.dword);

		f2i.f = v0; b->defs2.push_back (f2i.i);
		f2i.f = v1; b->defs2.push_back (f2i.i);
		f2i.f = v2; b->defs2.push_back (f2i.i);
		f2i.f = v3; b->defs2.push_back (f2i.i);
	}

	return reg;
}

SM2Opcode dxb_to_sm2 (SM4Opcode op)
{
	switch (op)
	{
	case kSM4Op_SQRT:
		break;
	case kSM4Op_DIV:
		break;
	case kSM4Op_RSQ:
		return kSM2Op_RSQ;
	case kSM4Op_LOG:
		return kSM2Op_LOG;
	case kSM4Op_EXP:
		return kSM2Op_EXP;
	case kSM4Op_RCP:
		return kSM2Op_RCP;

	case kSM4Op_MOV:
		return kSM2Op_MOV;
	case kSM4Op_MOVC: 
		break;

	case kSM4Op_MAD:
		return kSM2Op_MAD;
	case kSM4Op_DP2:
		break;
	case kSM4Op_DP3:
		return kSM2Op_DP3;
	case kSM4Op_DP4:
		return kSM2Op_DP4;
	case kSM4Op_MUL:
		return kSM2Op_MUL;
	case kSM4Op_ADD:
		return kSM2Op_ADD;
	case kSM4Op_AND:
		break;
	case kSM4Op_MIN:
		return kSM2Op_MIN;
	case kSM4Op_MAX:
		return kSM2Op_MAX;

	case kSM4Op_SAMPLE:
		return kSM2Op_TEX;
	case kSM4Op_DISCARD:
		return kSM2Op_TEXKILL;

	case kSM4Op_LT:
		return kSM2Op_SLT;
	case kSM4Op_GE:
		return kSM2Op_SGE;
	case kSM4Op_NE:
		break;
	case kSM4Op_EQ:
		break;

	case kSM4Op_RET:
		return kSM2Op_END;
	}

	AssertString("unknown SM4 opcode");
	__debugbreak();

	return kSM2Op_NOP;
}


