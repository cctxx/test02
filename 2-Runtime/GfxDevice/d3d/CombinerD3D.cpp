#include "UnityPrefix.h"
#include "CombinerD3D.h"
#include "External/shaderlab/Library/texenv.h"
#include "External/shaderlab/Library/pass.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "D3D9Context.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "External/DirectX/builds/dx9include/d3dx9.h"


// --------------------------------------------------------------------------
//  Combiners to fixed function texture stages

// NOTE: not all GL combiner modes are representable in TSS:
//	* per-stage constants
//	* DOUBLE/QUAD on arbitrary operations
//	* a*b+-c, a*b-c
// So what we do is: if hardware supports ps_1_1, we generate pixel shaders on the fly, see below.

static D3DTEXTUREOP kCombinerFuncTable[3][8] = {
	{ D3DTOP_SELECTARG1,   D3DTOP_MODULATE,     D3DTOP_ADD,   D3DTOP_ADDSIGNED, D3DTOP_SUBTRACT,    D3DTOP_LERP, D3DTOP_DOTPRODUCT3, D3DTOP_DOTPRODUCT3 },
	{        D3DTOP_ADD, D3DTOP_MODULATE2X, D3DTOP_DISABLE, D3DTOP_ADDSIGNED2X,  D3DTOP_DISABLE, D3DTOP_DISABLE,     D3DTOP_DISABLE,     D3DTOP_DISABLE },
	{    D3DTOP_DISABLE, D3DTOP_MODULATE4X, D3DTOP_DISABLE,     D3DTOP_DISABLE,  D3DTOP_DISABLE, D3DTOP_DISABLE,     D3DTOP_DISABLE,     D3DTOP_DISABLE },
};
static DWORD kCombinerFuncCapsTable[3][8] = {
	{ D3DTEXOPCAPS_SELECTARG1,   D3DTEXOPCAPS_MODULATE, D3DTEXOPCAPS_ADD,   D3DTEXOPCAPS_ADDSIGNED, D3DTEXOPCAPS_SUBTRACT, D3DTEXOPCAPS_LERP, D3DTEXOPCAPS_DOTPRODUCT3, D3DTEXOPCAPS_DOTPRODUCT3 },
	{        D3DTEXOPCAPS_ADD, D3DTEXOPCAPS_MODULATE2X,                0, D3DTEXOPCAPS_ADDSIGNED2X, 0, 0, 0, 0 },
	{                       0, D3DTEXOPCAPS_MODULATE4X,                0,                        0, 0, 0, 0, 0 },
};

void InitializeCombinerCapsD3D9()
{
	DWORD texOpCaps = gGraphicsCaps.d3d.d3dcaps.TextureOpCaps;
	for( int r = 0; r < 3; ++r ) {
		for( int c = 0; c < 8; ++c ) {
			if( kCombinerFuncCapsTable[r][c] ) {
				if( !(texOpCaps & kCombinerFuncCapsTable[r][c]) )
					kCombinerFuncTable[r][c] = D3DTOP_DISABLE;
			}
		}
	}
}

static int kCombinerSourceTable[4] = {
	D3DTA_CURRENT, D3DTA_TEXTURE, D3DTA_TFACTOR, D3DTA_DIFFUSE // TODO: TFACTOR is global, not per-stage!
};
static const int kCombinerOperandModTableRGB[4] = {
	0, D3DTA_ALPHAREPLICATE, D3DTA_COMPLEMENT, D3DTA_ALPHAREPLICATE | D3DTA_COMPLEMENT
};
static const int kCombinerOperandModTableAlpha[4] = {
	0, 0, D3DTA_COMPLEMENT, D3DTA_COMPLEMENT
};



static bool CombinerToTextureStage( UInt32 comb, D3DTEXTUREOP& outOp, int outArgs[3], bool alpha )
{
	int s0 = (comb >> combiner::kSrcZeroShift) & 0xFF;

	int cf = COMBINER_GET_FUNC(comb);
	int s1 = (comb) & 0xFF;
	int scale = HighestBit( (comb >> combiner::kScaleShift) );
	AssertIf( scale < 0 || scale > 2 );

	const int* kCombinerOperandModTable = alpha ? kCombinerOperandModTableAlpha : kCombinerOperandModTableRGB;
	int source0 = kCombinerSourceTable[s0 & combiner::kSourceMask] | kCombinerOperandModTable[s0 >> combiner::kOperandShift];
	int source1 = kCombinerSourceTable[s1 & combiner::kSourceMask] | kCombinerOperandModTable[s1 >> combiner::kOperandShift];
	if( !(cf & combiner::kBlendFuncMask) )
	{
		outOp = kCombinerFuncTable[scale][cf];
		if( outOp == D3DTOP_DISABLE )
			return false;
		// we emulate "source double" with "source + source"
		if( cf == 0 )
			source1 = source0;
		outArgs[0] = source0;
		outArgs[1] = source1;
		outArgs[2] = D3DTA_CURRENT;
	}
	else
	{
		int blendF = COMBINER_GET_BLEND_FUNC_INDEX(cf);
		int src2 = cf & combiner::kSourceMask;
		int oper2 = ((cf & combiner::kOperandTwo) >> combiner::kOperandShift) | 1;
		int source2 = kCombinerSourceTable[src2] | kCombinerOperandModTable[oper2];

		DWORD texOpCaps = gGraphicsCaps.d3d.d3dcaps.TextureOpCaps;

		switch( blendF )
		{
		case 0:
			// src0 lerp(src2 alpha) src1
			if( (scale != 0) || !(texOpCaps & D3DTEXOPCAPS_LERP) )
				return false;
			outOp = D3DTOP_LERP;
			outArgs[0] = source0;
			outArgs[1] = source1;
			outArgs[2] = source2;
			break;
		case 1:
			// src0 * src2 alpha + src1
			if( texOpCaps & D3DTEXOPCAPS_MULTIPLYADD ) {
				if( scale != 0 )
					return false;
				outOp = D3DTOP_MULTIPLYADD;
				outArgs[0] = source0;
				outArgs[1] = source2;
				outArgs[2] = source1;
			} else {
				// TODO
				return false;
			}
			break;
		case 2:
			// src0 * src2 alpha +- src1
			// not supported!
			return false;
		case 3:
			// src0 * src2 alpha - src1
			// not supported!
			return false;
		default:
			AssertString( "Unknown combiner blend function" );
			return false;
		}
		AssertIf( outOp <= D3DTOP_DISABLE || outOp > D3DTOP_LERP );
	}

	return true;
}

static bool CombinerToTextureStage( const ShaderLab::TextureBinding& te, D3DTextureStage& stage, bool& outTFactorUsed )
{
	int combColor = te.m_CombColor;
	if( !CombinerToTextureStage( combColor, stage.colorOp, stage.colorArgs, false ) )
		return false;

	// For DOT3 operation, we have to force using no function on alpha
	// However, on some old cards this has no effect; they always replicate DOT3 to all channels
	// (e.g. GeForce 2). Oh well.
	int combColorFunc = COMBINER_GET_FUNC(combColor);
	int combAlpha = te.m_CombAlpha;
	if( combColorFunc == 6 ) { // DOT3
		combAlpha &= ~(0xFF << combiner::kFuncShift);
	}
	if( !CombinerToTextureStage( combAlpha, stage.alphaOp, stage.alphaArgs, true ) )
		return false;

	if( te.IsTexColorUsed() )
	{
		outTFactorUsed = true;
	}

	return true;
}


// --------------------------------------------------------------------------
//  Combiners to pixel shader 1.1


// Supports up to 4 texture stages.
// Each stage outputs into r0 register.
// Per-stage constants are stored in corresponding constant registers [c0..c3].
// "Color" command (TFACTOR equivalent) should store in c4.
// r1 is used in some cases to load & process some constants (e.g. where we'd want to do a c4_bias, we first load it into r1 and then do r1_bias)


// Cache for generated pixel shaders.
struct CombinersCacheEntry
{
	UInt32 combColor[kMaxD3DTextureStagesForPS];
	UInt32 combAlpha[kMaxD3DTextureStagesForPS];
	IDirect3DPixelShader9*	pixelShader;
	bool	specular;

	bool Equals( int count, const ShaderLab::TextureBinding* texEnvs, bool specular ) const
	{
		AssertIf( count > kMaxD3DTextureStagesForPS );
		if( specular != this->specular )
			return false;
		for( int i = 0; i < count; ++i )
		{
			if( texEnvs[i].m_CombColor != combColor[i] )
				return false;
			if( texEnvs[i].m_CombAlpha != combAlpha[i] )
				return false;
		}
		return true;
	}
};
// Not a map on purpose - comparison is cheap and we want to store everything in a single block.
static std::vector<CombinersCacheEntry> s_CombinersCache[kMaxD3DTextureStagesForPS][2]; // [2] = lighting off, lighting on

void TextureCombinersD3D::CleanupCombinerCache()
{
	for( int i = 0; i < kMaxD3DTextureStagesForPS; ++i )
	{
		for( int j = 0; j < 2; ++j )
		{
			std::vector<CombinersCacheEntry>& cache = s_CombinersCache[i][j];
			for( int k = 0; k < cache.size(); ++k )
			{
				IDirect3DPixelShader9* ps = cache[k].pixelShader;
				if( ps ) {
					ULONG refCount = ps->Release();
					AssertIf( refCount != 0 );
				}
			}
			cache.clear();
			cache.swap(std::vector<CombinersCacheEntry>());
		}
	}
}

static const char* kPSDestRegRGBA[3] = { " r0",     "_x2 r0",     "_x4 r0" };
static const char* kPSDestRegRGB [3] = { " r0.rgb", "_x2 r0.rgb", "_x4 r0.rgb" };
static const char* kPSDestRegA   [3] = { " r0.a",   "_x2 r0.a",   "_x4 r0.a" };

static const char* kPSOperandPrefixTable[4] = {
	"", "", "1-", "1-",
};
static const char* kPSOperandSuffixTableRGB[4] = {
	"", ".a", "", ".a",
};

static std::string CombinerSrcPS( combiner::Source source, int stage, bool lighting )
{
	switch( source )
	{
	case combiner::kSrcPrevious:
		if( stage == 0 )
			return lighting ? "v0" : "c4";
		else
			return "r0";
	case combiner::kSrcTexture:
		return 't' + IntToString(stage);
	case combiner::kSrcConstant:
		return 'c' + IntToString(stage);
	case combiner::kSrcPrimaryColor:
		return lighting ? "v0" : "c4";
	default:
		AssertString( "Unknown source" );
		return "";
	}
}

static void FixupForConstantModifiers( std::string& source, std::string& outFixup, UInt32 operand, bool alpha, bool dot3 )
{
	if( source.size() >= 4 && source[0]=='1' && source[1]=='-' && source[2] == 'c' )
	{
		std::string sub = source.substr( 3, source.size()-3 );
		outFixup += std::string( alpha ? "+mov_sat r1.a, c" : "mov_sat r1.rgb, c" ) + sub + '\n';
		source = "1-r1";
		if( !alpha )
			source += kPSOperandSuffixTableRGB[operand];
	}
	if( dot3 && source.size() >= 2 && source[0]=='c' )
	{
		std::string sub = source.substr( 1, source.size()-1 );
		outFixup += std::string( alpha ? "+mov_sat r1.a, c" : "mov_sat r1.rgb, c" ) + sub + '\n';
		source = "r1";
		if( !alpha )
			source += kPSOperandSuffixTableRGB[operand];
	}
}

static void CombinerToPixelShaderText( int stage, bool lighting, UInt32 comb, bool alpha, std::string& outInstruction, std::string& outPrevFixup, bool& outPrevSat, bool& outSkipAlpha )
{
	outSkipAlpha = false;

	int s0 = (comb >> combiner::kSrcZeroShift) & 0xFF;

	int cf = COMBINER_GET_FUNC(comb);
	int s1 = (comb) & 0xFF;
	int scale = HighestBit( (comb >> combiner::kScaleShift) );
	AssertIf( scale < 0 || scale > 2 );

	combiner::Source src0 = static_cast<combiner::Source>(s0 & combiner::kSourceMask);
	combiner::Source src1 = static_cast<combiner::Source>(s1 & combiner::kSourceMask);
	UInt32 oper0 = s0 >> combiner::kOperandShift;
	UInt32 oper1 = s1 >> combiner::kOperandShift;
	std::string source0 = kPSOperandPrefixTable[oper0] + CombinerSrcPS( src0, stage, lighting );
	if( !alpha )
		source0 += kPSOperandSuffixTableRGB[oper0];
	std::string source1 = kPSOperandPrefixTable[oper1] + CombinerSrcPS( src1, stage, lighting );
	std::string suffix1 = alpha ? "" : kPSOperandSuffixTableRGB[oper1];
	const char** kPSDestReg = (cf == 7) ? kPSDestRegRGBA : (alpha ? kPSDestRegA : kPSDestRegRGB);
	std::string destReg = kPSDestReg[scale];

	std::string text;

	// Some special rules:
	// * For bias modifier or a lerp, a previous instruction should saturate the result
	// * For negate modifier, a previous instruction can't saturate the result (hence can't just saturate everything)
	// * 1-x or x_bias not allowed on constants. So if we detect such case, we try to load it into r1
	//	 with additional instruction.
	// * Emulating DOT3 requires _bx2 modifier, which is not allowed on constants. So we also detect
	//   that and load it into r1 with additional instruction.

	bool dot3 = (cf == 6 || cf == 7);
	FixupForConstantModifiers( source0, outPrevFixup, oper0, alpha, dot3 );
	FixupForConstantModifiers( source1, outPrevFixup, oper1, alpha, dot3 );

	bool addSatOnPrevious = false; // should we add "saturate" on previous instruction?
	if( !(cf & combiner::kBlendFuncMask) )
	{
		switch( cf )
		{
		case 0: text = "mov" + destReg + ", " + source0; break;
		case 1: text = "mul" + destReg + ", " + source0 + ", " + source1 + suffix1; break;
		case 2: text = "add" + destReg + ", " + source0 + ", " + source1 + suffix1; break;
		case 3:
			if( source1[0] == 'c' )
			{
				std::string sub = source1.substr( 1, source1.size()-1 );
				outPrevFixup += std::string( alpha ? "+mov_sat r1.a, c" : "mov_sat r1.rgb, c" ) + sub + '\n';
				source1 = "r1";
			}
			text = "add" + destReg + ", " + source0 + ", " + source1 + "_bias" + suffix1;
			if( (s1 & combiner::kSourceMask) == 0 ) // if source1 is "previous", need to saturate previous instruction
				addSatOnPrevious = true;
			break;
		case 4: text = "sub" + destReg + ", " + source0 + ", " + source1 + suffix1; break;
		case 5: AssertIf(false); break;
		case 6:
			// DOT3: for rgb do the dot, for alpha just use source0
			if( alpha )
				text += "mov" + destReg + ", " + source0;
			else
				text = "dp3" + destReg + ", " + source0 + "_bx2, " + source1 + "_bx2" + suffix1; break;
		case 7:
			// DOT3RGBA: do the dot into all four channels, and do not issue co-issued alpha
			// instructions for this stage.
			text = "dp3" + destReg + ", " + source0 + "_bx2, " + source1 + "_bx2" + suffix1;
			outSkipAlpha = true;
			break;
		}
	}
	else
	{
		int blendF = COMBINER_GET_BLEND_FUNC_INDEX(cf);
		combiner::Source src2 = static_cast<combiner::Source>(cf & combiner::kSourceMask);
		int oper2 = ((cf & combiner::kOperandTwo) >> combiner::kOperandShift) | 1;
		std::string source2 = kPSOperandPrefixTable[oper2] + CombinerSrcPS(src2, stage, lighting);
		if( !alpha )
			source2 += kPSOperandSuffixTableRGB[oper2];

		FixupForConstantModifiers( source2, outPrevFixup, oper2, alpha, false );

		switch( blendF )
		{
		case 0:
			// src0 lerp(src2 alpha) src1
			text = "lrp" + destReg + ", " + source2 + ", " + source0 + ", " + source1 + suffix1;
			if( src2 == combiner::kSrcPrevious ) // if src2 is "previous", need to saturate previous instruction
				addSatOnPrevious = true;
			break;
		case 1:
			// src0 * src2 alpha + src1
			text = "mad" + destReg + ", " + source0 + ", " + source2 + ", " + source1 + suffix1;
			break;
		case 2:
			// src0 * src2 alpha +- src1
			if( source1[0] == 'c' )
			{
				std::string sub = source1.substr( 1, source1.size()-1 );
				outPrevFixup += std::string( alpha ? "+mov_sat r1.a, c" : "mov_sat r1.rgb, c" ) + sub + '\n';
				source1 = "r1";
			}
			text = "mad" + destReg + ", " + source0 + ", " + source2 + ", " + source1 + "_bias" + suffix1;
			if( (s1 & combiner::kSourceMask) == 0 ) // if source1 is "previous", need to saturate previous instruction
				addSatOnPrevious = true;
			break;
		case 3:
			// src0 * src2 alpha - src1
			text = "mad" + destReg + ", " + source0 + ", " + source2 + ", -" + source1 + suffix1;
			break;
		default:
			AssertString( "Unknown combiner blend function" );
			break;
		}
	}

	// if we're not the first instruction and we need to modify previous one - do it.
	int typeIndex = alpha ? 1 : 0;
	outPrevSat = false;
	if( stage != 0 && addSatOnPrevious )
		outPrevSat = true;

	if( alpha )
		outInstruction += '+';
	outInstruction += text;
	outInstruction += '\n';
}


// GpuProgramsD3D.cpp
ID3DXBuffer* AssembleD3DShader( const std::string& source );


static IDirect3DPixelShader9* CombinersToPixelShader( int count, const ShaderLab::TextureBinding* texEnvs, bool lighting, bool addSpecular )
{
	AssertIf( count < 1 );

	// ps_1_1 supports only 4 textures
	if (count > kMaxD3DTextureStagesForPS)
		return NULL;

	// look for such combiner setup in cache
	int lightingIdx = lighting ? 1 : 0;
	int cacheCount = s_CombinersCache[count-1][lightingIdx].size();
	for( int i = 0; i < cacheCount; ++i )
	{
		CombinersCacheEntry& ce = s_CombinersCache[count-1][lightingIdx][i];
		if( ce.Equals( count, texEnvs, addSpecular ) )
		{
			AssertIf( !ce.pixelShader );
			return ce.pixelShader;
		}
	}

	std::string text = "ps_1_1\n";
	// sample textures
	for( int i = 0; i < count; ++i )
	{
		text += "tex t" + IntToString(i) + '\n';
	}

	// do combiner operations
	CombinersCacheEntry cacheEntry;
	int previousInstructions[2] = { 0, 0 };
	for( int i = 0; i < count; ++i )
	{
		UInt32 combColor = texEnvs[i].m_CombColor;
		UInt32 combAlpha = texEnvs[i].m_CombAlpha;
		cacheEntry.combColor[i] = combColor;
		cacheEntry.combAlpha[i] = combAlpha;
		cacheEntry.specular = addSpecular;
		std::string instruction, fixup;
		bool satPrevious,  skipAlpha;
		// color
		CombinerToPixelShaderText( i, lighting, combColor, false, instruction, fixup, satPrevious, skipAlpha );
		if( satPrevious ) {
			while( text[previousInstructions[0]] != ' ' )
				++previousInstructions[0];
			text.insert( previousInstructions[0], "_sat" );
			previousInstructions[1] += 4; // move the other pointer forward by _sat length as well
		}
		int colorInstructionLength = instruction.size();
		// alpha
		if( !skipAlpha )
		{
			CombinerToPixelShaderText( i, lighting, combAlpha, true, instruction, fixup, satPrevious, skipAlpha );
			if( satPrevious ) {
				while( text[previousInstructions[1]] != ' ' )
					++previousInstructions[1];
				text.insert( previousInstructions[1], "_sat" );
				previousInstructions[0] += 4; // move the other pointer forward by _sat length as well
			}
		}

		text += fixup;
		previousInstructions[0] = text.size();
		previousInstructions[1] = text.size() + (skipAlpha ? 0 : colorInstructionLength);
		text += instruction;
	}

	// add specular at the end if needed
	if( addSpecular )
	{
		text += "add r0.rgb, r0, v1";
	}

	// compile pixel shader
	HRESULT hr;
	IDirect3DDevice9* dev = GetD3DDevice();

	// assemble shader
	ID3DXBuffer *compiledShader = AssembleD3DShader( text );
	IDirect3DPixelShader9* ps = NULL;
	if( compiledShader )
	{
		// create shader
		hr = dev->CreatePixelShader( (const DWORD*)compiledShader->GetBufferPointer(), &ps );
		compiledShader->Release();
		if( FAILED(hr) )
		{
			ErrorStringMsg ("D3D9 Combiners: failed to create pixel shader representation: %s", text.c_str());
		}
	}
	AssertIf( !ps );

	// insert into cache
	cacheEntry.pixelShader = ps;
	s_CombinersCache[count-1][lightingIdx].push_back( cacheEntry );

	return ps;
}


// --------------------------------------------------------------------------



TextureCombinersD3D* TextureCombinersD3D::Create( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular )
{
	static int uniqueIDCounter = 1;

	static bool combinerCapsInitialized = false;
	if( !combinerCapsInitialized )
	{
		InitializeCombinerCapsD3D9();
		combinerCapsInitialized = true;
	}

	// For threaded rendering this check is done on the client side (and we get NULL here)
	if (props)
	{
		// check texgen modes & texture dimension are supported
		for( int i = 0; i < count; ++i ) {
			TextureDimension texDim;
			TexGenMode texGen;
			GetTexEnvInfoFromName( texEnvs[i].m_TextureName, texDim, texGen, props );
			if( !ShaderLab::IsTexEnvSupported( texEnvs[i].m_TextureName, texDim, texGen ) )
				return NULL;
		}
	}

	bool canConvertToStages = true;

	// "primary" in the combiner might refer to diffuse or texture factor, depending on
	// whether lighting is on or vertex colors are bound
	kCombinerSourceTable[3] = hasVertexColorOrLighting ? D3DTA_DIFFUSE : D3DTA_TFACTOR;
	
	TextureCombinersD3D* d3dte = new TextureCombinersD3D();
	d3dte->uniqueID = ++uniqueIDCounter;
	d3dte->envCount = count;
	d3dte->texEnvs = texEnvs;
	d3dte->pixelShader = NULL;
	d3dte->textureFactorIndex = -1;

	// special case: when no SetTextures are present, setup to do { combine primary } equivalent
	if( count == 0 )
	{
		d3dte->stages[0].colorOp = D3DTOP_SELECTARG1;
		d3dte->stages[0].colorArgs[0] = d3dte->stages[0].colorArgs[1] = d3dte->stages[0].colorArgs[2] = kCombinerSourceTable[3];
		d3dte->stages[0].alphaOp = D3DTOP_SELECTARG1;
		d3dte->stages[0].alphaArgs[0] = d3dte->stages[0].alphaArgs[1] = d3dte->stages[0].alphaArgs[2] = kCombinerSourceTable[3];
		d3dte->stages[1].colorOp = D3DTOP_DISABLE;
		d3dte->stageCount = 1;
		return d3dte;
	}

	// try to convert to pixel shader and use that if everything is ok
	d3dte->pixelShader = CombinersToPixelShader( count, texEnvs, hasVertexColorOrLighting, usesAddSpecular );

	// if don't have pixel shader, convert to TSS setup
	if( !d3dte->pixelShader )
	{
		if( count > gGraphicsCaps.d3d.d3dcaps.MaxSimultaneousTextures )
		{
			// In theory we could convert more; if most of combiner stages do not actually
			// use the texture. In practice we just cap it at MaxSimultaneousTextures;
			// it will match GL behaviour as well.
			canConvertToStages = false;
		}
		else
		{
			for( int i = 0; i < count; ++i )
			{
				const ShaderLab::TextureBinding& te = texEnvs[i];
				D3DTextureStage& stage = d3dte->stages[i];
				bool textureFactorUsed = false;
				if( !CombinerToTextureStage(te, stage, textureFactorUsed) )
				{
					canConvertToStages = false;
					break;
				}
				if (textureFactorUsed)
				{
					d3dte->textureFactorIndex = i;
				}
			}
			d3dte->stages[count].colorOp = D3DTOP_DISABLE;
		}

		// no can't do
		if( !canConvertToStages )
		{
			delete d3dte;
			return NULL;
		}
	}

	d3dte->stageCount = count;
	return d3dte;
}
