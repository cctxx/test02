// Written by Aras Pranckevicius
// I would place this code in the public domain but...
//
// D3D enumerations, structures and interfaces are
// pretty much copied from MSDN. Not sure if those can fall
// under "public domain".

#pragma once

#include "External/Wodka/wodka_WinHelper.h"


struct D3D11_SHADER_DESC;
struct D3D11_SHADER_INPUT_BIND_DESC;
struct D3D11_SIGNATURE_PARAMETER_DESC;

enum D3D10_PRIMITIVE
{
	D3D10_PRIMITIVE_UNDEFINED = 0,
	D3D10_PRIMITIVE_POINT = 1,
	D3D10_PRIMITIVE_LINE = 2,
	D3D10_PRIMITIVE_TRIANGLE = 3,
	D3D10_PRIMITIVE_LINE_ADJ = 6,
	D3D10_PRIMITIVE_TRIANGLE_ADJ = 7
};
enum D3D_FEATURE_LEVEL
{	D3D_FEATURE_LEVEL_9_1  = 0x9100,
	D3D_FEATURE_LEVEL_9_2  = 0x9200,
	D3D_FEATURE_LEVEL_9_3  = 0x9300,
	D3D_FEATURE_LEVEL_10_0 = 0xa000,
	D3D_FEATURE_LEVEL_10_1 = 0xa100,
	D3D_FEATURE_LEVEL_11_0 = 0xb000,
};
enum D3D10_PRIMITIVE_TOPOLOGY
{
	D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED = 0,
	D3D10_PRIMITIVE_TOPOLOGY_POINTLIST = 1,
	D3D10_PRIMITIVE_TOPOLOGY_LINELIST = 2,
	D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP = 3,
	D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST = 4,
	D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP = 5,
	D3D10_PRIMITIVE_TOPOLOGY_LINELIST_ADJ = 10,
	D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ = 11,
	D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ = 12,
	D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ = 13,
};
enum D3D10_NAME
{
    D3D10_NAME_UNDEFINED = 0,
    D3D10_NAME_POSITION = 1,
    D3D10_NAME_CLIP_DISTANCE = 2,
    D3D10_NAME_CULL_DISTANCE = 3,
    D3D10_NAME_RENDER_TARGET_ARRAY_INDEX = 4,
    D3D10_NAME_VIEWPORT_ARRAY_INDEX = 5,
    D3D10_NAME_VERTEX_ID = 6,
    D3D10_NAME_PRIMITIVE_ID = 7,
    D3D10_NAME_INSTANCE_ID = 8,
    D3D10_NAME_IS_FRONT_FACE = 9,
    D3D10_NAME_SAMPLE_INDEX = 10,
    D3D11_NAME_FINAL_QUAD_EDGE_TESSFACTOR = 11,
    D3D11_NAME_FINAL_QUAD_INSIDE_TESSFACTOR = 12,
    D3D11_NAME_FINAL_TRI_EDGE_TESSFACTOR = 13,
    D3D11_NAME_FINAL_TRI_INSIDE_TESSFACTOR = 14,
    D3D11_NAME_FINAL_LINE_DETAIL_TESSFACTOR = 15,
    D3D11_NAME_FINAL_LINE_DENSITY_TESSFACTOR = 16,
    D3D10_NAME_TARGET = 64,
    D3D10_NAME_DEPTH = 65,
    D3D10_NAME_COVERAGE = 66,
    D3D11_NAME_DEPTH_GREATER_EQUAL = 67,
    D3D11_NAME_DEPTH_LESS_EQUAL = 68,
};
enum D3D10_REGISTER_COMPONENT_TYPE
{
	D3D10_REGISTER_COMPONENT_UNKNOWN = 0,
	D3D10_REGISTER_COMPONENT_UINT32 = 1,
	D3D10_REGISTER_COMPONENT_SINT32 = 2,
	D3D10_REGISTER_COMPONENT_FLOAT32 = 3
};
enum D3D10_SHADER_INPUT_TYPE
{
	D3D10_SIT_CBUFFER,
	D3D10_SIT_TBUFFER,
	D3D10_SIT_TEXTURE,
	D3D10_SIT_SAMPLER,
	D3D11_SIT_UAV_RWTYPED,
	D3D11_SIT_STRUCTURED,
	D3D11_SIT_UAV_RWSTRUCTURED,
	D3D11_SIT_BYTEADDRESS,
	D3D11_SIT_UAV_RWBYTEADDRESS,
	D3D11_SIT_UAV_APPEND_STRUCTURED,
	D3D11_SIT_UAV_CONSUME_STRUCTURED,
	D3D11_SIT_UAV_RWSTRUCTURED_WITH_COUNTER,
};
enum D3D10_SRV_DIMENSION
{
	D3D10_SRV_DIMENSION_UNKNOWN = 0,
	D3D10_SRV_DIMENSION_BUFFER = 1,
	D3D10_SRV_DIMENSION_TEXTURE1D = 2,
	D3D10_SRV_DIMENSION_TEXTURE1DARRAY = 3,
	D3D10_SRV_DIMENSION_TEXTURE2D = 4,
	D3D10_SRV_DIMENSION_TEXTURE2DARRAY = 5,
	D3D10_SRV_DIMENSION_TEXTURE2DMS = 6,
	D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY = 7,
	D3D10_SRV_DIMENSION_TEXTURE3D = 8,
	D3D10_SRV_DIMENSION_TEXTURECUBE = 9,
};


enum D3D11_PRIMITIVE_TOPOLOGY
{
	D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED = 0,
	D3D11_PRIMITIVE_TOPOLOGY_POINTLIST = 1,
	D3D11_PRIMITIVE_TOPOLOGY_LINELIST = 2,
	D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP = 3,
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST = 4,
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP = 5,
	D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ = 10,
	D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ = 11,
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ = 12,
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ = 13,
	D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST = 33,
	D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST = 34,
	D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST = 35,
	D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST = 36,
	D3D11_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST = 37,
	D3D11_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST = 38,
	D3D11_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST = 39,
	D3D11_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST = 40,
	D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST = 41,
	D3D11_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST = 42,
	D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST = 43,
	D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST = 44,
	D3D11_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST = 45,
	D3D11_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST = 46,
	D3D11_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST = 47,
	D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST = 48,
	D3D11_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST = 49,
	D3D11_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST = 50,
	D3D11_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST = 51,
	D3D11_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST = 52,
	D3D11_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST = 53,
	D3D11_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST = 54,
	D3D11_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST = 55,
	D3D11_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST = 56,
	D3D11_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST = 57,
	D3D11_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST = 58,
	D3D11_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST = 59,
	D3D11_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST = 60,
	D3D11_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST = 61,
	D3D11_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST = 62,
	D3D11_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST = 63,
	D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST = 64,
};

enum D3D11_PRIMITIVE
{
	D3D11_PRIMITIVE_UNDEFINED = 0,
	D3D11_PRIMITIVE_POINT = 1,
	D3D11_PRIMITIVE_LINE = 2,
	D3D11_PRIMITIVE_TRIANGLE = 3,
	D3D11_PRIMITIVE_LINE_ADJ = 6,
	D3D11_PRIMITIVE_TRIANGLE_ADJ = 7,
	D3D11_PRIMITIVE_1_CONTROL_POINT_PATCH = 8,
	D3D11_PRIMITIVE_2_CONTROL_POINT_PATCH = 9,
	D3D11_PRIMITIVE_3_CONTROL_POINT_PATCH = 10,
	D3D11_PRIMITIVE_4_CONTROL_POINT_PATCH = 11,
	D3D11_PRIMITIVE_5_CONTROL_POINT_PATCH = 12,
	D3D11_PRIMITIVE_6_CONTROL_POINT_PATCH = 13,
	D3D11_PRIMITIVE_7_CONTROL_POINT_PATCH = 14,
	D3D11_PRIMITIVE_8_CONTROL_POINT_PATCH = 15,
	D3D11_PRIMITIVE_9_CONTROL_POINT_PATCH = 16,
	D3D11_PRIMITIVE_10_CONTROL_POINT_PATCH = 17,
	D3D11_PRIMITIVE_11_CONTROL_POINT_PATCH = 18,
	D3D11_PRIMITIVE_12_CONTROL_POINT_PATCH = 19,
	D3D11_PRIMITIVE_13_CONTROL_POINT_PATCH = 20,
	D3D11_PRIMITIVE_14_CONTROL_POINT_PATCH = 21,
	D3D11_PRIMITIVE_15_CONTROL_POINT_PATCH = 22,
	D3D11_PRIMITIVE_16_CONTROL_POINT_PATCH = 23,
	D3D11_PRIMITIVE_17_CONTROL_POINT_PATCH = 24,
	D3D11_PRIMITIVE_18_CONTROL_POINT_PATCH = 25,
	D3D11_PRIMITIVE_19_CONTROL_POINT_PATCH = 26,
	D3D11_PRIMITIVE_20_CONTROL_POINT_PATCH = 27,
	D3D11_PRIMITIVE_21_CONTROL_POINT_PATCH = 28,
	D3D11_PRIMITIVE_22_CONTROL_POINT_PATCH = 29,
	D3D11_PRIMITIVE_23_CONTROL_POINT_PATCH = 30,
	D3D11_PRIMITIVE_24_CONTROL_POINT_PATCH = 31,
	D3D11_PRIMITIVE_25_CONTROL_POINT_PATCH = 32,
	D3D11_PRIMITIVE_26_CONTROL_POINT_PATCH = 33,
	D3D11_PRIMITIVE_27_CONTROL_POINT_PATCH = 34,
	D3D11_PRIMITIVE_28_CONTROL_POINT_PATCH = 35,
	D3D11_PRIMITIVE_29_CONTROL_POINT_PATCH = 36,
	D3D11_PRIMITIVE_30_CONTROL_POINT_PATCH = 37,
	D3D11_PRIMITIVE_31_CONTROL_POINT_PATCH = 38,
	D3D11_PRIMITIVE_32_CONTROL_POINT_PATCH = 39,
};

enum D3D11_TESSELLATOR_DOMAIN
{
    D3D11_TESSELLATOR_DOMAIN_UNDEFINED = 0,
    D3D11_TESSELLATOR_DOMAIN_ISOLINE = 1,
    D3D11_TESSELLATOR_DOMAIN_TRI = 2,
    D3D11_TESSELLATOR_DOMAIN_QUAD = 3,
};

enum D3D11_TESSELLATOR_PARTITIONING
{
    D3D11_TESSELLATOR_PARTITIONING_UNDEFINED = 0,
    D3D11_TESSELLATOR_PARTITIONING_INTEGER = 1,
    D3D11_TESSELLATOR_PARTITIONING_POW2 = 2,
    D3D11_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD = 3,
    D3D11_TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN = 4,
};

enum D3D11_TESSELLATOR_OUTPUT_PRIMITIVE
{
    D3D11_TESSELLATOR_OUTPUT_UNDEFINED = 0,
    D3D11_TESSELLATOR_OUTPUT_POINT = 1,
    D3D11_TESSELLATOR_OUTPUT_LINE = 2,
    D3D11_TESSELLATOR_OUTPUT_TRIANGLE_CW = 3,
    D3D11_TESSELLATOR_OUTPUT_TRIANGLE_CCW = 4,
};

enum D3D11_RESOURCE_RETURN_TYPE
{
	D3D11_RETURN_TYPE_UNORM = 1,
	D3D11_RETURN_TYPE_SNORM = 2,
	D3D11_RETURN_TYPE_SINT = 3,
	D3D11_RETURN_TYPE_UINT = 4,
	D3D11_RETURN_TYPE_FLOAT = 5,
	D3D11_RETURN_TYPE_MIXED = 6,
	D3D11_RETURN_TYPE_DOUBLE = 7,
	D3D11_RETURN_TYPE_CONTINUED = 8,
};

enum D3D_MIN_PRECISION
{
	D3D_MIN_PRECISION_DEFAULT = 0,
	D3D_MIN_PRECISION_FLOAT_16 = 1,
	D3D_MIN_PRECISION_FLOAT_2_8	= 2,
	D3D_MIN_PRECISION_RESERVED = 3,
	D3D_MIN_PRECISION_SINT_16 = 4,
	D3D_MIN_PRECISION_UINT_16 = 5,
	D3D_MIN_PRECISION_ANY_16 = 0xf0,
	D3D_MIN_PRECISION_ANY_10 = 0xf1
};



struct D3D11_SHADER_DESC
{
	UINT Version;
	const char* Creator;
	UINT Flags;
	UINT ConstantBuffers;
	UINT BoundResources;
	UINT InputParameters;
	UINT OutputParameters;
	UINT InstructionCount;
	UINT TempRegisterCount;
	UINT TempArrayCount;
	UINT DefCount;
	UINT DclCount;
	UINT TextureNormalInstructions;
	UINT TextureLoadInstructions;
	UINT TextureCompInstructions;
	UINT TextureBiasInstructions;
	UINT TextureGradientInstructions;
	UINT FloatInstructionCount;
	UINT IntInstructionCount;
	UINT UintInstructionCount;
	UINT StaticFlowControlCount;
	UINT DynamicFlowControlCount;
	UINT MacroInstructionCount;
	UINT ArrayInstructionCount;
	UINT CutInstructionCount;
	UINT EmitInstructionCount;
	D3D10_PRIMITIVE_TOPOLOGY GSOutputTopology;
	UINT GSMaxOutputVertexCount;
	D3D11_PRIMITIVE InputPrimitive;
	UINT PatchConstantParameters;
	UINT cGSInstanceCount;
	UINT cControlPoints;
	D3D11_TESSELLATOR_OUTPUT_PRIMITIVE HSOutputPrimitive;
	D3D11_TESSELLATOR_PARTITIONING HSPartitioning;
	D3D11_TESSELLATOR_DOMAIN TessellatorDomain;
	UINT cBarrierInstructions;
	UINT cInterlockedInstructions;
	UINT cTextureStoreInstructions;
};

struct D3D11_SIGNATURE_PARAMETER_DESC
{
    const char* SemanticName;
    UINT SemanticIndex;
    UINT Register;
    D3D10_NAME SystemValueType;
    D3D10_REGISTER_COMPONENT_TYPE ComponentType;
    BYTE Mask;
    BYTE ReadWriteMask;
    UINT Stream;
	#if WODKA_USE_D3DCOMPILER_46 || WODKA_USE_D3DCOMPILER_47
	D3D_MIN_PRECISION           MinPrecision;   // Minimum desired interpolation precision
	#endif
};

struct D3D11_SHADER_INPUT_BIND_DESC
{
    const char* Name;
    D3D10_SHADER_INPUT_TYPE Type;
    UINT BindPoint;
    UINT BindCount;
    UINT uFlags;
    D3D11_RESOURCE_RETURN_TYPE ReturnType;
    D3D10_SRV_DIMENSION Dimension;
    UINT NumSamples;
};


enum D3D11_CBUFFER_TYPE
{
	D3D11_CT_CBUFFER,
	D3D11_CT_TBUFFER,
	D3D11_CT_INTERFACE_POINTERS,
	D3D11_CT_RESOURCE_BIND_INFO,
};

enum D3D10_SHADER_VARIABLE_CLASS
{
	D3D10_SVC_SCALAR,
	D3D10_SVC_VECTOR,
	D3D10_SVC_MATRIX_ROWS,
	D3D10_SVC_MATRIX_COLUMNS,
	D3D10_SVC_OBJECT,
	D3D10_SVC_STRUCT,

	D3D11_SVC_INTERFACE_CLASS,
	D3D11_SVC_INTERFACE_POINTER,

	// force 32-bit size enum
	D3D10_SVC_FORCE_DWORD = 0x7fffffff
};

enum D3D10_SHADER_VARIABLE_TYPE
{
	D3D10_SVT_VOID = 0,
	D3D10_SVT_BOOL = 1,
	D3D10_SVT_INT = 2,
	D3D10_SVT_FLOAT = 3,
	D3D10_SVT_STRING = 4,
	D3D10_SVT_TEXTURE = 5,
	D3D10_SVT_TEXTURE1D = 6,
	D3D10_SVT_TEXTURE2D = 7,
	D3D10_SVT_TEXTURE3D = 8,
	D3D10_SVT_TEXTURECUBE = 9,
	D3D10_SVT_SAMPLER = 10,
	D3D10_SVT_PIXELSHADER = 15,
	D3D10_SVT_VERTEXSHADER = 16,
	D3D10_SVT_UINT = 19,
	D3D10_SVT_UINT8 = 20,
	D3D10_SVT_GEOMETRYSHADER = 21,
	D3D10_SVT_RASTERIZER = 22,
	D3D10_SVT_DEPTHSTENCIL = 23,
	D3D10_SVT_BLEND = 24,
	D3D10_SVT_BUFFER = 25,
	D3D10_SVT_CBUFFER = 26,
	D3D10_SVT_TBUFFER = 27,
	D3D10_SVT_TEXTURE1DARRAY = 28,
	D3D10_SVT_TEXTURE2DARRAY = 29,
	D3D10_SVT_RENDERTARGETVIEW = 30,
	D3D10_SVT_DEPTHSTENCILVIEW = 31,

	D3D10_SVT_TEXTURE2DMS = 32,
	D3D10_SVT_TEXTURE2DMSARRAY = 33,

	// force 32-bit size enum
	D3D10_SVT_FORCE_DWORD = 0x7fffffff
};

enum D3D10_SHADER_VARIABLE_FLAGS
{
	D3D10_SVF_USERPACKED = 1,
	D3D10_SVF_USED       = 2,

	// force 32-bit size enum
	D3D10_SVF_FORCE_DWORD = 0x7fffffff
};




struct D3D11_SHADER_BUFFER_DESC
{
	const char*                  Name;           // Name of the constant buffer
	D3D11_CBUFFER_TYPE      Type;           // Indicates type of buffer content
	UINT                    Variables;      // Number of member variables
	UINT                    Size;           // Size of CB (in bytes)
	UINT                    uFlags;         // Buffer description flags
};

struct D3D11_SHADER_VARIABLE_DESC
{
	const char*                  Name;           // Name of the variable
	UINT                    StartOffset;    // Offset in constant buffer's backing store
	UINT                    Size;           // Size of variable (in bytes)
	UINT                    uFlags;         // Variable flags
	void*                   DefaultValue;   // Raw pointer to default value
	UINT                    StartTexture;   // First texture index (or -1 if no textures used)
	UINT                    TextureSize;    // Number of texture slots possibly used.
	UINT                    StartSampler;   // First sampler index (or -1 if no textures used)
	UINT                    SamplerSize;    // Number of sampler slots possibly used.
};

struct D3D11_SHADER_TYPE_DESC
{
	D3D10_SHADER_VARIABLE_CLASS Class;          // Variable class (e.g. object, matrix, etc.)
	D3D10_SHADER_VARIABLE_TYPE  Type;           // Variable type (e.g. float, sampler, etc.)
	UINT                        Rows;           // Number of rows (for matrices, 1 for other numeric, 0 if not applicable)
	UINT                        Columns;        // Number of columns (for vectors & matrices, 1 for other numeric, 0 if not applicable)
	UINT                        Elements;       // Number of elements (0 if not an array)
	UINT                        Members;        // Number of members (0 if not a structure)
	UINT                        Offset;         // Offset from the start of structure (0 if not a structure member)
	const char*                      Name;           // Name of type, can be NULL
};



struct D3D11ShaderReflectionConstantBuffer;

struct D3D11ShaderReflectionType
{
	virtual HRESULT WINAPI_IMPL GetDesc(D3D11_SHADER_TYPE_DESC *pDesc) = 0;

	virtual D3D11ShaderReflectionType* WINAPI_IMPL GetMemberTypeByIndex(UINT Index) = 0;
	virtual D3D11ShaderReflectionType* WINAPI_IMPL GetMemberTypeByName(const char* Name) = 0;
	virtual const char* WINAPI_IMPL GetMemberTypeName(UINT Index) = 0;

	virtual HRESULT WINAPI_IMPL IsEqual(D3D11ShaderReflectionType* pType) = 0;
	virtual D3D11ShaderReflectionType* WINAPI_IMPL GetSubType() = 0;
	virtual D3D11ShaderReflectionType* WINAPI_IMPL GetBaseClass() = 0;
	virtual UINT WINAPI_IMPL GetNumInterfaces() = 0;
	virtual D3D11ShaderReflectionType* WINAPI_IMPL GetInterfaceByIndex(UINT uIndex) = 0;
	virtual HRESULT WINAPI_IMPL IsOfType(D3D11ShaderReflectionType* pType) = 0;
	virtual HRESULT WINAPI_IMPL ImplementsInterface(D3D11ShaderReflectionType* pBase) = 0;
};


struct D3D11ShaderReflectionVariable
{
	virtual HRESULT WINAPI_IMPL GetDesc(D3D11_SHADER_VARIABLE_DESC *pDesc) = 0;
	virtual D3D11ShaderReflectionType* WINAPI_IMPL GetType() = 0;
	virtual D3D11ShaderReflectionConstantBuffer* WINAPI_IMPL GetBuffer() = 0;
	virtual UINT WINAPI_IMPL GetInterfaceSlot(UINT uArrayIndex) = 0;
};


struct D3D11ShaderReflectionConstantBuffer
{
	virtual HRESULT WINAPI_IMPL GetDesc(D3D11_SHADER_BUFFER_DESC *pDesc) = 0;
	virtual D3D11ShaderReflectionVariable* WINAPI_IMPL GetVariableByIndex(UINT Index) = 0;
	virtual D3D11ShaderReflectionVariable* WINAPI_IMPL GetVariableByName(const char* Name) = 0;
};


struct D3D11ShaderReflection
{
	virtual HRESULT WINAPI_IMPL QueryInterface(const GUIDImpl& iid, void** ppv) = 0;
	virtual ULONG WINAPI_IMPL AddRef() = 0;
	virtual ULONG WINAPI_IMPL Release() = 0;
	virtual HRESULT WINAPI_IMPL GetDesc(D3D11_SHADER_DESC* desc) = 0;
	virtual D3D11ShaderReflectionConstantBuffer* WINAPI_IMPL GetConstantBufferByIndex(UINT idx) = 0;
	virtual D3D11ShaderReflectionConstantBuffer* WINAPI_IMPL GetConstantBufferByName(const char* name) = 0;
	virtual HRESULT WINAPI_IMPL GetResourceBindingDesc(UINT idx, D3D11_SHADER_INPUT_BIND_DESC* desc) = 0;
	virtual HRESULT WINAPI_IMPL GetInputParameterDesc(UINT idx, D3D11_SIGNATURE_PARAMETER_DESC* desc) = 0;
	virtual HRESULT WINAPI_IMPL GetOutputParameterDesc(UINT idx, D3D11_SIGNATURE_PARAMETER_DESC* desc) = 0;
	virtual HRESULT WINAPI_IMPL GetPatchConstantParameterDesc(UINT idx, D3D11_SIGNATURE_PARAMETER_DESC* desc) = 0;
	virtual D3D11ShaderReflectionVariable* WINAPI_IMPL GetVariableByName(const char* name) = 0;
	virtual HRESULT WINAPI_IMPL GetResourceBindingDescByName(const char* name, D3D11_SHADER_INPUT_BIND_DESC* desc) = 0;
	virtual UINT WINAPI_IMPL GetMovInstructionCount() = 0;
	virtual UINT WINAPI_IMPL GetMovcInstructionCount() = 0;
	virtual UINT WINAPI_IMPL GetConversionInstructionCount() = 0;
	virtual UINT WINAPI_IMPL GetBitwiseInstructionCount() = 0;
	virtual D3D10_PRIMITIVE WINAPI_IMPL GetGSInputPrimitive() = 0;
	virtual BOOL WINAPI_IMPL IsSampleFrequencyShader() = 0;
	virtual UINT WINAPI_IMPL GetNumInterfaceSlots() = 0;
	virtual HRESULT WINAPI_IMPL GetMinFeatureLevel(enum D3D_FEATURE_LEVEL* level) = 0;

	#if WODKA_USE_D3DCOMPILER_46 || WODKA_USE_D3DCOMPILER_47
	virtual UINT WINAPI_IMPL GetThreadGroupSize(UINT* pSizeX, UINT* pSizeY, UINT* pSizeZ) = 0;
	virtual UINT64 WINAPI_IMPL GetRequiresFlags() = 0;
	#endif
};
