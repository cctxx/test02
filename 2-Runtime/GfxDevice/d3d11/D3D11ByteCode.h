#pragma once

#include "Runtime/Utilities/dynamic_array.h"

struct _D3D11_SIGNATURE_PARAMETER_DESC;
typedef struct _D3D11_SIGNATURE_PARAMETER_DESC D3D11_SIGNATURE_PARAMETER_DESC;



#define MAKE_FOURCC(a, b, c, d) ((UInt32)(UInt8)(a) | ((UInt32)(UInt8)(b) << 8) | ((UInt32)(UInt8)(c) << 16) | ((UInt32)(UInt8)(d) << 24 ))
#define kFOURCC_DXBC MAKE_FOURCC('D', 'X', 'B', 'C')

// Reflection or debug information? stripped shaders don't have this chunk
#define kFOURCC_RDEF MAKE_FOURCC('R', 'D', 'E', 'F')
// Shader input signature
#define kFOURCC_ISGN MAKE_FOURCC('I', 'S', 'G', 'N')
// Shader output signature
#define kFOURCC_OSGN MAKE_FOURCC('O', 'S', 'G', 'N')
// Shader code
#define kFOURCC_SHDR MAKE_FOURCC('S', 'H', 'D', 'R')
#define kFOURCC_SHEX MAKE_FOURCC('S', 'H', 'E', 'X')
// Statistics? stripped shaders don't have this chunk
#define kFOURCC_STAT MAKE_FOURCC('S', 'T', 'A', 'T')
// Patch information?
#define kFOURCC_PCSG MAKE_FOURCC('P', 'C', 'S', 'G')
//FL 9.x SM 2.0 shader magic signature
#define kFOURCC_SM20 MAKE_FOURCC('A', 'o', 'n', '9')


struct DXBCChunkHeader
{
	unsigned fourcc;
	unsigned size;
};


struct DXBCContainer
{
	const void* data;
	dynamic_array<DXBCChunkHeader*> chunks;
};

struct DXBCSignatureElement
{
	UInt32 name_offset;
	UInt32 semantic_index;
	UInt32 system_value_type;
	UInt32 component_type;
	UInt32 register_num;
	UInt8 mask;
	UInt8 read_write_mask;
	UInt8 stream;
	UInt8 unused;
};

struct DXBCChunkSig : public DXBCChunkHeader
{
	UInt32 count;
	UInt32 unk8; // always has 8?
	DXBCSignatureElement elements[1];
};

struct DXBCChunkCode : public DXBCChunkHeader
{
	UInt32 version;
	UInt32 length; // length in dword tokens
};

struct DXBCChunkSM20 : public DXBCChunkHeader
{
	UInt32 length; // length in bytes
};


struct SM4TokInstruction
{
	// not an union directly because unions can't be inherited from
	union
	{
		UInt32 dword;
		// length and extended are always present, but they are only here to reduce duplication
		struct
		{
			unsigned opcode : 11;
			unsigned _11_23 : 13;
			unsigned length : 7;
			unsigned extended : 1;
		};
		struct
		{
			unsigned opcode : 11;
			unsigned resinfo_return_type : 2;
			unsigned sat : 1;
			unsigned _14_17 : 4;
			unsigned test_nz : 1; // bit 18
			unsigned precise_mask : 4;
			unsigned _23 : 1;
			unsigned length : 7;
			unsigned extended : 1;
		} insn;
		struct
		{
			unsigned opcode : 11;
			unsigned threads_in_group : 1;
			unsigned shared_memory : 1;
			unsigned uav_group : 1;
			unsigned uav_global : 1;
			unsigned _15_17 : 3;
		} sync;
		struct
		{
			unsigned opcode : 11;
			unsigned allow_refactoring : 1;
			unsigned fp64 : 1;
			unsigned early_depth_stencil : 1;
			unsigned enable_raw_and_structured_in_non_cs : 1;
		} dcl_global_flags;
		struct
		{
			unsigned opcode : 11;
			unsigned target : 5;
			unsigned nr_samples : 7;
		} dcl_resource;
		struct
		{
			unsigned opcode : 11;
			unsigned shadow : 1;
			unsigned mono : 1;
		} dcl_sampler;
		struct
		{
			unsigned opcode : 11;
			unsigned interpolation : 5;
		} dcl_input_ps;
		struct
		{
			unsigned opcode : 11;
			unsigned dynamic : 1;
		} dcl_constant_buffer;
		struct
		{
			unsigned opcode : 11;
			unsigned primitive : 6;
		} dcl_gs_input_primitive;
		struct
		{
			unsigned opcode : 11;
			unsigned primitive_topology : 7;
		} dcl_gs_output_primitive_topology;
		struct
		{
			unsigned opcode : 11;
			unsigned control_points : 6;
		} dcl_input_control_point_count;
		struct
		{
			unsigned opcode : 11;
			unsigned control_points : 6;
		} dcl_output_control_point_count;
		struct
		{
			unsigned opcode : 11;
			unsigned domain : 3; /* D3D_TESSELLATOR_DOMAIN */
		} dcl_tess_domain;
		struct
		{
			unsigned opcode : 11;
			unsigned partitioning : 3; /* D3D_TESSELLATOR_PARTITIONING */
		} dcl_tess_partitioning;
		struct
		{
			unsigned opcode : 11;
			unsigned primitive : 3; /* D3D_TESSELLATOR_OUTPUT_PRIMITIVE */
		} dcl_tess_output_primitive;
	};
};

union SM4TokInstructionEx
{
	UInt32 dword;
	struct
	{
		unsigned type : 6;
		unsigned _6_30 : 25;
		unsigned extended :1;
	};
	struct
	{
		unsigned type : 6;
		unsigned _6_8 : 3;
		int offset_u : 4;
		int offset_v : 4;
		int offset_w : 4;
	} sample_controls;
	struct
	{
		unsigned type : 6;
		unsigned target : 5;
	} resource_target;
	struct
	{
		unsigned type : 6;
		unsigned x : 4;
		unsigned y : 4;
		unsigned z : 4;
		unsigned w : 4;
	} resource_return_type;
};

struct SM4TokOperand
{
	union {
		UInt32 dword;
		struct {
			unsigned comps_enum : 2; /* sm4_operands_comps */
			unsigned mode : 2; /* SM4OperMode */
			unsigned sel : 8;
			unsigned file : 8; /* SM4RegFile */
			unsigned num_indices : 2;
			unsigned index0_repr : 3; /* SM4OperIndexRepr */
			unsigned index1_repr : 3; /* SM4OperIndexRepr */
			unsigned index2_repr : 3; /* SM4OperIndexRepr */
			unsigned extended : 1;
		};
	};
};

struct SM2TokInstruction
{
	union
	{
		UInt32 dword;
		struct
		{
			unsigned opcode     : 16;
			unsigned specific   : 8;
			unsigned length     : 4;
			unsigned predicated : 1;
			unsigned _res0      : 1;
			unsigned _res1      : 1;
			unsigned _res2      : 1;
		};
		struct
		{
			unsigned minor      : 8;
			unsigned major      : 8;
			unsigned _magic     : 16;
		} version;
	};
};

struct SM2TokDst
{
	union {
		UInt32 dword;
		struct {
			unsigned reg_num    : 11;
			unsigned reg_type34 : 2;
			unsigned _res0      : 1;
			unsigned _res1      : 2;
			unsigned write_mask : 4;
			unsigned res_mod    : 4;
			unsigned shift_scl  : 4;
			unsigned reg_type02 : 3;
			unsigned _one       : 1;
		};
	};
};

void dxbc_create(struct DXBCChunkHeader** chunks, unsigned num_chunks, dynamic_array<UInt8>& out);


DXBCContainer* dxbc_parse(const void* data, int size);
DXBCChunkHeader* dxbc_find_chunk(const void* data, int size, unsigned fourcc);
void dxbc_print(const DXBCContainer* dxbc);
int dxbc_parse_signature(const DXBCChunkSig* sig, D3D11_SIGNATURE_PARAMETER_DESC** params);


enum SM2Opcode
{
	kSM2Op_NOP          = 0,
	kSM2Op_MOV          ,
	kSM2Op_ADD          ,
	kSM2Op_SUB          ,
	kSM2Op_MAD          ,
	kSM2Op_MUL          ,
	kSM2Op_RCP          ,
	kSM2Op_RSQ          ,
	kSM2Op_DP3          ,
	kSM2Op_DP4          ,
	kSM2Op_MIN          ,
	kSM2Op_MAX          ,
	kSM2Op_SLT          ,
	kSM2Op_SGE          ,
	kSM2Op_EXP          ,
	kSM2Op_LOG          ,
	kSM2Op_LIT          ,
	kSM2Op_DST          ,
	kSM2Op_LRP          ,
	kSM2Op_FRC          ,
	kSM2Op_M4x4         ,
	kSM2Op_M4x3         ,
	kSM2Op_M3x4         ,
	kSM2Op_M3x3         ,
	kSM2Op_M3x2         ,
	kSM2Op_CALL         ,
	kSM2Op_CALLNZ       ,
	kSM2Op_LOOP         ,
	kSM2Op_RET          ,
	kSM2Op_ENDLOOP      ,
	kSM2Op_LABEL        ,
	kSM2Op_DCL          ,
	kSM2Op_POW          ,
	kSM2Op_CRS          ,
	kSM2Op_SGN          ,
	kSM2Op_ABS          ,
	kSM2Op_NRM          ,
	kSM2Op_SINCOS       ,
	kSM2Op_REP          ,
	kSM2Op_ENDREP       ,
	kSM2Op_IF           ,
	kSM2Op_IFC          ,
	kSM2Op_ELSE         ,
	kSM2Op_ENDIF        ,
	kSM2Op_BREAK        ,
	kSM2Op_BREAKC       ,
	kSM2Op_MOVA         ,
	kSM2Op_DEFB         ,
	kSM2Op_DEFI         ,

	kSM2Op_TEXCOORD     = 64,
	kSM2Op_TEXKILL      ,
	kSM2Op_TEX          ,
	kSM2Op_TEXBEM       ,
	kSM2Op_TEXBEML      ,
	kSM2Op_TEXREG2AR    ,
	kSM2Op_TEXREG2GB    ,
	kSM2Op_TEXM3x2PAD   ,
	kSM2Op_TEXM3x2TEX   ,
	kSM2Op_TEXM3x3PAD   ,
	kSM2Op_TEXM3x3TEX   ,
	kSM2Op_RESERVED0    ,
	kSM2Op_TEXM3x3SPEC  ,
	kSM2Op_TEXM3x3VSPEC ,
	kSM2Op_EXPP         ,
	kSM2Op_LOGP         ,
	kSM2Op_CND          ,
	kSM2Op_DEF          ,
	kSM2Op_TEXREG2RGB   ,
	kSM2Op_TEXDP3TEX    ,
	kSM2Op_TEXM3x2DEPTH ,
	kSM2Op_TEXDP3       ,
	kSM2Op_TEXM3x3      ,
	kSM2Op_TEXDEPTH     ,
	kSM2Op_CMP          ,
	kSM2Op_BEM          ,
	kSM2Op_DP2ADD       ,
	kSM2Op_DSX          ,
	kSM2Op_DSY          ,
	kSM2Op_TEXLDD       ,
	kSM2Op_SETP         ,
	kSM2Op_TEXLDL       ,
	kSM2Op_BREAKP       ,

	kSM2Op_PHASE        = 0xFFFD,
	kSM2Op_COMMENT      = 0xFFFE,
	kSM2Op_END          = 0xFFFF,
};

enum SM4ShaderType
{
	kSM4Shader_Pixel,
	kSM4Shader_Vertex,
};

enum SM4Opcode
{
	kSM4Op_ADD,
	kSM4Op_AND,
	kSM4Op_BREAK,
	kSM4Op_BREAKC,
	kSM4Op_CALL,
	kSM4Op_CALLC,
	kSM4Op_CASE,
	kSM4Op_CONTINUE,
	kSM4Op_CONTINUEC,
	kSM4Op_CUT,
	kSM4Op_DEFAULT,
	kSM4Op_DERIV_RTX,
	kSM4Op_DERIV_RTY,
	kSM4Op_DISCARD,
	kSM4Op_DIV,
	kSM4Op_DP2,
	kSM4Op_DP3,
	kSM4Op_DP4,
	kSM4Op_ELSE,
	kSM4Op_EMIT,
	kSM4Op_EMITTHENCUT,
	kSM4Op_ENDIF,
	kSM4Op_ENDLOOP,
	kSM4Op_ENDSWITCH,
	kSM4Op_EQ,
	kSM4Op_EXP,
	kSM4Op_FRC,
	kSM4Op_FTOI,
	kSM4Op_FTOU,
	kSM4Op_GE,
	kSM4Op_IADD,
	kSM4Op_IF,
	kSM4Op_IEQ,
	kSM4Op_IGE,
	kSM4Op_ILT,
	kSM4Op_IMAD,
	kSM4Op_IMAX,
	kSM4Op_IMIN,
	kSM4Op_IMUL,
	kSM4Op_INE,
	kSM4Op_INEG,
	kSM4Op_ISHL,
	kSM4Op_ISHR,
	kSM4Op_ITOF,
	kSM4Op_LABEL,
	kSM4Op_LD,
	kSM4Op_LD_MS,
	kSM4Op_LOG,
	kSM4Op_LOOP,
	kSM4Op_LT,
	kSM4Op_MAD,
	kSM4Op_MIN,
	kSM4Op_MAX,
	kSM4Op_CUSTOMDATA,
	kSM4Op_MOV,
	kSM4Op_MOVC,
	kSM4Op_MUL,
	kSM4Op_NE,
	kSM4Op_NOP,
	kSM4Op_NOT,
	kSM4Op_OR,
	kSM4Op_RESINFO,
	kSM4Op_RET,
	kSM4Op_RETC,
	kSM4Op_ROUND_NE,
	kSM4Op_ROUND_NI,
	kSM4Op_ROUND_PI,
	kSM4Op_ROUND_Z,
	kSM4Op_RSQ,
	kSM4Op_SAMPLE,
	kSM4Op_SAMPLE_C,
	kSM4Op_SAMPLE_C_LZ,
	kSM4Op_SAMPLE_L,
	kSM4Op_SAMPLE_D,
	kSM4Op_SAMPLE_B,
	kSM4Op_SQRT,
	kSM4Op_SWITCH,
	kSM4Op_SINCOS,
	kSM4Op_UDIV,
	kSM4Op_ULT,
	kSM4Op_UGE,
	kSM4Op_UMUL,
	kSM4Op_UMAD,
	kSM4Op_UMAX,
	kSM4Op_UMIN,
	kSM4Op_USHR,
	kSM4Op_UTOF,
	kSM4Op_XOR,
	kSM4Op_DCL_RESOURCE,
	kSM4Op_DCL_CONSTANT_BUFFER,
	kSM4Op_DCL_SAMPLER,
	kSM4Op_DCL_INDEX_RANGE,
	kSM4Op_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY,
	kSM4Op_DCL_GS_INPUT_PRIMITIVE,
	kSM4Op_DCL_MAX_OUTPUT_VERTEX_COUNT,
	kSM4Op_DCL_INPUT,
	kSM4Op_DCL_INPUT_SGV,
	kSM4Op_DCL_INPUT_SIV,
	kSM4Op_DCL_INPUT_PS,
	kSM4Op_DCL_INPUT_PS_SGV,
	kSM4Op_DCL_INPUT_PS_SIV,
	kSM4Op_DCL_OUTPUT,
	kSM4Op_DCL_OUTPUT_SGV,
	kSM4Op_DCL_OUTPUT_SIV,
	kSM4Op_DCL_TEMPS,
	kSM4Op_DCL_INDEXABLE_TEMP,
	kSM4Op_DCL_GLOBAL_FLAGS,
	kSM4Op_D3D10_COUNT,
	kSM4Op_LOD,
	kSM4Op_GATHER4,
	kSM4Op_SAMPLE_POS,
	kSM4Op_SAMPLE_INFO,
	kSM4Op_D3D10_1_COUNT,
	kSM4Op_HS_DECLS,
	kSM4Op_HS_CONTROL_POINT_PHASE,
	kSM4Op_HS_FORK_PHASE,
	kSM4Op_HS_JOIN_PHASE,
	kSM4Op_EMIT_STREAM,
	kSM4Op_CUT_STREAM,
	kSM4Op_EMITTHENCUT_STREAM,
	kSM4Op_INTERFACE_CALL,
	kSM4Op_BUFINFO,
	kSM4Op_DERIV_RTX_COARSE,
	kSM4Op_DERIV_RTX_FINE,
	kSM4Op_DERIV_RTY_COARSE,
	kSM4Op_DERIV_RTY_FINE,
	kSM4Op_GATHER4_C,
	kSM4Op_GATHER4_PO,
	kSM4Op_GATHER4_PO_C,
	kSM4Op_RCP,
	kSM4Op_F32TOF16,
	kSM4Op_F16TOF32,
	kSM4Op_UADDC,
	kSM4Op_USUBB,
	kSM4Op_COUNTBITS,
	kSM4Op_FIRSTBIT_HI,
	kSM4Op_FIRSTBIT_LO,
	kSM4Op_FIRSTBIT_SHI,
	kSM4Op_UBFE,
	kSM4Op_IBFE,
	kSM4Op_BFI,
	kSM4Op_BFREV,
	kSM4Op_SWAPC,
	kSM4Op_DCL_STREAM,
	kSM4Op_DCL_FUNCTION_BODY,
	kSM4Op_DCL_FUNCTION_TABLE,
	kSM4Op_DCL_INTERFACE,
	kSM4Op_DCL_INPUT_CONTROL_POINT_COUNT,
	kSM4Op_DCL_OUTPUT_CONTROL_POINT_COUNT,
	kSM4Op_DCL_TESS_DOMAIN,
	kSM4Op_DCL_TESS_PARTITIONING,
	kSM4Op_DCL_TESS_OUTPUT_PRIMITIVE,
	kSM4Op_DCL_HS_MAX_TESSFACTOR,
	kSM4Op_DCL_HS_FORK_PHASE_INSTANCE_COUNT,
	kSM4Op_DCL_HS_JOIN_PHASE_INSTANCE_COUNT,
	kSM4Op_DCL_THREAD_GROUP,
	kSM4Op_DCL_UNORDERED_ACCESS_VIEW_TYPED,
	kSM4Op_DCL_UNORDERED_ACCESS_VIEW_RAW,
	kSM4Op_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED,
	kSM4Op_DCL_THREAD_GROUP_SHARED_MEMORY_RAW,
	kSM4Op_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED,
	kSM4Op_DCL_RESOURCE_RAW,
	kSM4Op_DCL_RESOURCE_STRUCTURED,
	kSM4Op_LD_UAV_TYPED,
	kSM4Op_STORE_UAV_TYPED,
	kSM4Op_LD_RAW,
	kSM4Op_STORE_RAW,
	kSM4Op_LD_STRUCTURED,
	kSM4Op_STORE_STRUCTURED,
	kSM4Op_ATOMIC_AND,
	kSM4Op_ATOMIC_OR,
	kSM4Op_ATOMIC_XOR,
	kSM4Op_ATOMIC_CMP_STORE,
	kSM4Op_ATOMIC_IADD,
	kSM4Op_ATOMIC_IMAX,
	kSM4Op_ATOMIC_IMIN,
	kSM4Op_ATOMIC_UMAX,
	kSM4Op_ATOMIC_UMIN,
	kSM4Op_IMM_ATOMIC_ALLOC,
	kSM4Op_IMM_ATOMIC_CONSUME,
	kSM4Op_IMM_ATOMIC_IADD,
	kSM4Op_IMM_ATOMIC_AND,
	kSM4Op_IMM_ATOMIC_OR,
	kSM4Op_IMM_ATOMIC_XOR,
	kSM4Op_IMM_ATOMIC_EXCH,
	kSM4Op_IMM_ATOMIC_CMP_EXCH,
	kSM4Op_IMM_ATOMIC_IMAX,
	kSM4Op_IMM_ATOMIC_IMIN,
	kSM4Op_IMM_ATOMIC_UMAX,
	kSM4Op_IMM_ATOMIC_UMIN,
	kSM4Op_SYNC,
	kSM4Op_DADD,
	kSM4Op_DMAX,
	kSM4Op_DMIN,
	kSM4Op_DMUL,
	kSM4Op_DEQ,
	kSM4Op_DGE,
	kSM4Op_DLT,
	kSM4Op_DNE,
	kSM4Op_DMOV,
	kSM4Op_DMOVC,
	kSM4Op_DTOF,
	kSM4Op_FTOD,
	kSM4Op_EVAL_SNAPPED,
	kSM4Op_EVAL_SAMPLE_INDEX,
	kSM4Op_EVAL_CENTROID,
	kSM4Op_DCL_GS_INSTANCE_COUNT,
	kSM4Op_COUNT
};


enum SM4Interpolation
{
	kSM4Interp_UNDEFINED,
	kSM4Interp_CONSTANT,
	kSM4Interp_LINEAR,
	kSM4Interp_LINEAR_CENTROID,
	kSM4Interp_LINEAR_NOPERSPECTIVE,
	kSM4Interp_LINEAR_NOPERSPECTIVE_CENTROID,
	kSM4Interp_LINEAR_SAMPLE,
	kSM4Interp_LINEAR_NOPERSPECTIVE_SAMPLE,
	kSM4Interp_COUNT
};


enum SM4OperCompnum
{
	kSM4OperComp0,
	kSM4OperComp1,
	kSM4OperComp4,
	kSM4OperCompN,
	kSM4OperComp,
	kSM4OperCompCOUNT
};

enum SM4OperMode
{
	SM4_OPERAND_MODE_MASK,
	SM4_OPERAND_MODE_SWIZZLE,
	SM4_OPERAND_MODE_SCALAR,
	SM4_OPERAND_MODE_,
	SM4_OPERAND_MODE_COUNT
};

enum SM4OperIndexRepr
{
	SM4_OPERAND_INDEX_REPR_IMM32,
	SM4_OPERAND_INDEX_REPR_IMM64,
	SM4_OPERAND_INDEX_REPR_REG,
	SM4_OPERAND_INDEX_REPR_REG_IMM32,
	SM4_OPERAND_INDEX_REPR_REG_IMM64,
	SM4_OPERAND_INDEX_REPR_COUNT
};


enum SM4RegFile
{
	kSM4File_TEMP,
	kSM4File_INPUT,
	kSM4File_OUTPUT,
	kSM4File_INDEXABLE_TEMP,
	kSM4File_IMMEDIATE32,
	kSM4File_IMMEDIATE64,
	kSM4File_SAMPLER,
	kSM4File_RESOURCE,
	kSM4File_CONSTANT_BUFFER,
	kSM4File_IMMEDIATE_CONSTANT_BUFFER,
	kSM4File_LABEL,
	kSM4File_INPUT_PRIMITIVEID,
	kSM4File_OUTPUT_DEPTH,
	kSM4File_NULL,
	kSM4File_RASTERIZER,
	kSM4File_OUTPUT_COVERAGE_MASK,
	kSM4File_STREAM,
	kSM4File_FUNCTION_BODY,
	kSM4File_FUNCTION_TABLE,
	kSM4File_INTERFACE,
	kSM4File_FUNCTION_INPUT,
	kSM4File_FUNCTION_OUTPUT,
	kSM4File_OUTPUT_CONTROL_POINT_ID,
	kSM4File_INPUT_FORK_INSTANCE_ID,
	kSM4File_INPUT_JOIN_INSTANCE_ID,
	kSM4File_INPUT_CONTROL_POINT,
	kSM4File_OUTPUT_CONTROL_POINT,
	kSM4File_INPUT_PATCH_CONSTANT,
	kSM4File_INPUT_DOMAIN_POINT,
	kSM4File_THIS_POINTER,
	kSM4File_UNORDERED_ACCESS_VIEW,
	kSM4File_THREAD_GROUP_SHARED_MEMORY,
	kSM4File_INPUT_THREAD_ID,
	kSM4File_INPUT_THREAD_GROUP_ID,
	kSM4File_INPUT_THREAD_ID_IN_GROUP,
	kSM4File_INPUT_COVERAGE_MASK,
	kSM4File_INPUT_THREAD_ID_IN_GROUP_FLATTENED,
	kSM4File_INPUT_GS_INSTANCE_ID,
	kSM4File_OUTPUT_DEPTH_GREATER_EQUAL,
	kSM4File_OUTPUT_DEPTH_LESS_EQUAL,
	kSM4File_CYCLE_COUNTER,
	kSM4File_COUNT
};


extern const char* kSM4OpcodeNames[kSM4Op_COUNT];


enum SM4Swizzle {
	kSM4SwzNone = 0xE4, // 11.10.01.00
	kSM4SwzRepX = 0x00, // 00.00.00.00
	kSM4SwzRepY = 0x55, // 01.01.01.01
	kSM4SwzRepZ = 0xAA, // 10.10.10.10
	kSM4SwzRepW = 0xFF, // 11.11.11.11
	kSM4SwzXYZZ = 0xA4, // 10.10.01.00
	kSM4SwzXYYY = 0x54, // 01.01.01.00
	kSM4SwzXYXX = 0x04, // 00.00.01.00
	kSM4SwzXYXY = 0x44, // 01.00.01.00
	kSM4SwzZWZW = 0xEE, // 11.10.11.10
	kSM4SwzZWWW = 0xFE, // 11.11.11.10
	kSM4SwzYYYZ = 0x95, // 10.01.01.01
	kSM4SwzXYZX = 0x24, // 00.10.01.00
	kSM4SwzXYZY = 0x64, // 01.10.01.00
	kSM4SwzYZWY = 0x79, // 01.11.10.01
};

enum SM4Target
{
	kSM4Target_UNKNOWN,
	kSM4Target_BUFFER,
	kSM4Target_TEXTURE1D,
	kSM4Target_TEXTURE2D,
	kSM4Target_TEXTURE2DMS,
	kSM4Target_TEXTURE3D,
	kSM4Target_TEXTURECUBE,
	kSM4Target_TEXTURE1DARRAY,
	kSM4Target_TEXTURE2DARRAY,
	kSM4Target_TEXTURE2DMSARRAY,
	kSM4Target_TEXTURECUBEARRAY,
	kSM4Target_RAW_BUFFER,
	kSM4Target_STRUCTURED_BUFFER,
	kSM4Target_COUNT
};


struct DXBCBuilder;
DXBCBuilder* dxb_create(int major, int minor, SM4ShaderType type);
void dxb_destroy(DXBCBuilder* b);


struct DXBCCodeBuilder;
DXBCCodeBuilder* dxb_get_code_builder(DXBCBuilder* b);
DXBCCodeBuilder* dxb_create_code(dynamic_array<UInt32>& destArray);
void dxb_destroy_code(DXBCCodeBuilder* b);

// result must be free()'d
void* dxb_build (DXBCBuilder* b, size_t& outSize);

void dxb_dcl_input (DXBCBuilder* b, const char* name, int index, int reg, int mask = 0xF);
void dxb_dcl_output (DXBCBuilder* b, const char* name, int index, int reg, int mask = 0xF);
void dxb_dcl_tex (DXBCBuilder* b, int index, SM4Target dim);
void dxb_dcl_cb (DXBCBuilder* b, int index, int size);

void dxb_op (DXBCCodeBuilder* b, SM4Opcode op, bool sat);
void dxb_reg (DXBCCodeBuilder* b, char rchar, int reg, unsigned mask = 0xF);
void dxb_swz (DXBCCodeBuilder* b, char rchar, int reg, unsigned swiz = kSM4SwzNone, bool neg=false);
void dxb_float1 (DXBCCodeBuilder* b, float v);
void dxb_int1 (DXBCCodeBuilder* b, int i);
void dxb_float4 (DXBCCodeBuilder* b, float v0, float v1, float v2, float v3);

//------------------------------------------------------------------------------------
//SM 2.0 stuff
void dxb_dcl_input2 (DXBCBuilder* b, const char* name, int index, int reg, int mask);
void dxb_dcl_output2 (DXBCBuilder* b, const char* name, int index, int reg, int mask);
void dxb_dcl_tex2 (DXBCBuilder* b, int index, SM4Target dim);
void dxb_op2 (DXBCBuilder* b, SM2Opcode op, bool sat, int scratchTmpRegForSat = -1);
void dxb_reg2 (DXBCBuilder* b, char rchar, int reg, unsigned mask = 0xF);
void dxb_swz2 (DXBCBuilder* b, char rchar, int reg, unsigned swiz = kSM4SwzNone, bool neg=false);
int dxb_imm_f4 (DXBCBuilder* b, float v0, float v1, float v2, float v3);
SM2Opcode dxb_to_sm2 (SM4Opcode op);


struct DXBCBuilderStream
{
	//no shader builder and no auto SM20 code for raw code works
	DXBCBuilderStream (DXBCCodeBuilder* b) : builder(NULL), bldcode(b), noSM2(true) { }
	DXBCBuilderStream (DXBCBuilder* b) : builder(b), bldcode(dxb_get_code_builder(b)), noSM2(false) { }

	DXBCBuilderStream& op(SM4Opcode op)
	{
		dxb_op(bldcode, op, false);
		if (!noSM2 && builder)
			dxb_op2(builder, dxb_to_sm2(op), false);
		return *this;
	}
	DXBCBuilderStream& op_sat(SM4Opcode op, int scratchTmpRegForSM2)
	{
		dxb_op(bldcode, op, true);
		if (!noSM2 && builder)
			dxb_op2(builder, dxb_to_sm2(op), true, scratchTmpRegForSM2);
		return *this;
	}
	DXBCBuilderStream& reg (char rchar, int reg, unsigned mask = 0xF)
	{
		dxb_reg(bldcode, rchar, reg, mask);
		if (!noSM2 && builder)
			dxb_reg2(builder, rchar, reg, mask);
		return *this;
	}
	DXBCBuilderStream& swz (char rchar, int reg, unsigned swiz = kSM4SwzNone, bool neg=false)
	{
		dxb_swz(bldcode, rchar, reg, swiz, neg);
		if (!noSM2 && builder)
			dxb_swz2(builder, rchar, reg, swiz, neg);
		return *this;
	}
	DXBCBuilderStream& float1 (float v)
	{
		dxb_float1(bldcode, v);

		if (!noSM2 && builder)
		{
			const int reg = dxb_imm_f4 (builder, v, v, v, v);
			dxb_swz2(builder, 'c', reg, kSM4SwzRepX);
		}
		return *this;
	}
	// float1 constant for SM2 only
	DXBCBuilderStream& float1_2 (float v)
	{
		if (builder)
		{
			const int reg = dxb_imm_f4 (builder, v, v, v, v);
			dxb_swz2(builder, 'c', reg, kSM4SwzRepX);
		}
		return *this;
	}
	DXBCBuilderStream& float4 (float v0, float v1, float v2, float v3)
	{
		dxb_float4(bldcode, v0, v1, v2, v3);

		if (!noSM2 && builder)
		{
			const int reg = dxb_imm_f4 (builder, v0, v1, v2, v3);
			dxb_swz2(builder, 'c', reg);
		}
		return *this;
	}


	//------------------------------------------------------------------------------------
	//SM 2.0 stuff

	DXBCBuilderStream& op2(SM2Opcode op)
	{
		dxb_op2(builder, op, false);
		return *this;
	}
	DXBCBuilderStream& reg2 (char rchar, int reg, unsigned mask = 0xF)
	{
		dxb_reg2(builder, rchar, reg, mask);
		return *this;
	}
	DXBCBuilderStream& swz2 (char rchar, int reg, unsigned swiz = kSM4SwzNone, bool neg=false)
	{
		dxb_swz2(builder, rchar, reg, swiz, neg);
		return *this;
	}
	void noAutoSM2(bool _noSM2=true)
	{
		noSM2 = _noSM2;
	}
	void autoSM2()
	{
		noSM2 = false;
	}

	DXBCBuilder*     builder;
	DXBCCodeBuilder* bldcode;
	bool             noSM2;
};

