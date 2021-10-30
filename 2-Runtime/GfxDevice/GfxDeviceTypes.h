#pragma once

#include "Configuration/UnityConfigure.h"
#include "GfxDeviceConfigure.h"
#include "Runtime/Math/Matrix4x4.h"

// Never change the enum values!
// They are used in low level native plugin interface.
enum GfxDeviceRenderer
{
	kGfxRendererOpenGL = 0,
	kGfxRendererD3D9 = 1,
	kGfxRendererD3D11 = 2,
	kGfxRendererGCM = 3,
	kGfxRendererNull = 4,
	kGfxRendererHollywood = 5,
	kGfxRendererXenon = 6,
	//kGfxRendererOpenGLES = 7, // removed
	kGfxRendererOpenGLES20Mobile = 8,
	kGfxRendererMolehill = 9,
	kGfxRendererOpenGLES20Desktop = 10,
	kGfxRendererOpenGLES30 = 11,
	kGfxRendererCount = 12
};

enum GfxThreadingMode
{
	kGfxThreadingModeDirect,
	kGfxThreadingModeThreaded,
	kGfxThreadingModeNonThreaded,
	kGfxThreadingModeAutoDetect
};

#if UNITY_WIN
typedef HWND NativeWindow;
#else
typedef unsigned long NativeWindow;
#endif

enum
{
	#if GFX_OPENGLESxx_ONLY || GFX_SUPPORTS_MOLEHILL
	kMaxSupportedTextureUnits = 8,
	#else
	kMaxSupportedTextureUnits = 16,
	#endif
	kMaxSupportedTextureUnitsGLES = 8,

	kMaxSupportedVertexLights = 8,
	kMaxSupportedTextureCoords = 8,

	kMaxSupportedRenderTargets = 4,
	kMaxSupportedConstantBuffers = 16,
	kMaxSupportedComputeResources = 16,
};


enum TextureDimension
{
	kTexDimUnknown = -1, // unknown
	kTexDimNone = 0, // no texture
	kTexDimDeprecated1D, // not used anymore, value there for backwards compatibility in serialization
	kTexDim2D,
	kTexDim3D,
	kTexDimCUBE,
	kTexDimAny,

	kTexDimCount, // keep this last!
	kTexDimForce32Bit = 0x7fffffff
};


// this is kept as UInt32 because it's serialized in some places; to ensure that it's 32
// bits everywhere.


typedef UInt32 TextureFormat;
enum
{
	kTexFormatAlpha8 = 1,
	kTexFormatARGB4444 = 2,
	kTexFormatRGB24 = 3,
	kTexFormatRGBA32 = 4,
	kTexFormatARGB32 = 5,
	kTexFormatARGBFloat = 6, // only for internal use at runtime
	kTexFormatRGB565 = 7,
	kTexFormatBGR24 = 8,
	// This one is for internal use; storage is 16 bits/pixel; samples
	// as Alpha (OpenGL) or RGB (D3D9). Can be reduced to 8 bit alpha/luminance on lower hardware.
	// Why it's not Luminance on GL: for some reason alpha seems to be faster.
	kTexFormatAlphaLum16 = 9,
	kTexFormatDXT1 = 10,
	kTexFormatDXT3 = 11,
	kTexFormatDXT5 = 12,
	kTexFormatRGBA4444 = 13,

	kTexFormatPCCount = 14,

	kTexReserved1 = 14, // Use reservedX when adding a new 'PC' texture format
	kTexReserved2 = 15,
	kTexReserved3 = 16,
	kTexReserved4 = 17,
	kTexReserved5 = 18,
	kTexReserved6 = 19,
	// [20..27] used to be Wii-specific formats before Unity 4.0
	kTexReserved11 = 28,
	kTexReserved12 = 29,

	// iPhone
	kTexFormatPVRTC_RGB2 = 30,
	kTexFormatPVRTC_RGBA2 = 31,

	kTexFormatPVRTC_RGB4 = 32,
	kTexFormatPVRTC_RGBA4 = 33,

	kTexFormatETC_RGB4 = 34,

	kTexFormatATC_RGB4 = 35,
	kTexFormatATC_RGBA8 = 36,

	// Pixels returned by iPhone camera
	kTexFormatBGRA32 = 37,

	kTexFormatFlashATF_RGB_DXT1 = 38,
	kTexFormatFlashATF_RGBA_JPG = 39,
	kTexFormatFlashATF_RGB_JPG = 40,

	// EAC and ETC2 compressed formats, mandated by OpenGL ES 3.0
	kTexFormatEAC_R = 41,
	kTexFormatEAC_R_SIGNED = 42,
	kTexFormatEAC_RG = 43,
	kTexFormatEAC_RG_SIGNED = 44,
	kTexFormatETC2_RGB = 45,
	kTexFormatETC2_RGBA1 = 46,
	kTexFormatETC2_RGBA8 = 47,

	// ASTC. The RGB and RGBA formats are internally identical, we just need to carry the has-alpha information somehow
	kTexFormatASTC_RGB_4x4 = 48,
	kTexFormatASTC_RGB_5x5 = 49,
	kTexFormatASTC_RGB_6x6 = 50,
	kTexFormatASTC_RGB_8x8 = 51,
	kTexFormatASTC_RGB_10x10 = 52,
	kTexFormatASTC_RGB_12x12 = 53,

	kTexFormatASTC_RGBA_4x4 = 54,
	kTexFormatASTC_RGBA_5x5 = 55,
	kTexFormatASTC_RGBA_6x6 = 56,
	kTexFormatASTC_RGBA_8x8 = 57,
	kTexFormatASTC_RGBA_10x10 = 58,
	kTexFormatASTC_RGBA_12x12 = 59,

	kTexFormatTotalCount	= 60 // keep this last!
};


enum TextureUsageMode 
{
	kTexUsageNone = 0,
	kTexUsageLightmapDoubleLDR,
	kTexUsageLightmapRGBM,
	kTexUsageNormalmapDXT5nm,
	kTexUsageNormalmapPlain,
};


enum TextureColorSpace 
{
	kTexColorSpaceLinear = 0,
	kTexColorSpaceSRGB,
	kTexColorSpaceSRGBXenon
};


enum TextureFilterMode
{
	kTexFilterNearest = 0,
	kTexFilterBilinear,
	kTexFilterTrilinear,
	kTexFilterCount // keep this last!
};


enum TextureWrapMode
{
	kTexWrapRepeat,
	kTexWrapClamp,
	kTexWrapCount // keep this last!
};


#if UNITY_EDITOR
// enum values to match 0..100 scale used on some platforms
enum TextureCompressionQuality
{
	kTexCompressionFast		= 0,
	kTexCompressionNormal	= 50,
	kTexCompressionBest		= 100
};
#endif


enum RenderTextureFormat
{
	kRTFormatARGB32 = 0,	// ARGB, 8 bit/channel
	kRTFormatDepth,			// whatever is for "depth texture": Depth16 on GL, R32F on D3D9, ...
	kRTFormatARGBHalf,		// ARGB, 16 bit floating point/channel
	kRTFormatShadowMap,		// whatever is "native" (with built-in comparisons) shadow map format
	kRTFormatRGB565,
	kRTFormatARGB4444,
	kRTFormatARGB1555,
	kRTFormatDefault,
	kRTFormatA2R10G10B10,
	kRTFormatDefaultHDR,
	kRTFormatARGB64,
	kRTFormatARGBFloat,
	kRTFormatRGFloat,
	kRTFormatRGHalf,
	kRTFormatRFloat,
	kRTFormatRHalf,
	kRTFormatR8,
	kRTFormatARGBInt,
	kRTFormatRGInt,
	kRTFormatRInt,
	kRTFormatBGRA32,
	kRTFormatCount			// keep this last!
};


enum RenderTextureReadWrite
{
	kRTReadWriteDefault = 0,		// The 'correct' state for the given position in the render pipeline
	kRTReadWriteLinear,			// No sRGB read / write
	kRTReadWriteSRGB,			// sRGB read / write
	kRTSRGBCount			// keep this last!
};


enum DepthBufferFormat
{
	kDepthFormatNone = 0,	// no depth buffer
	kDepthFormat16,			// 16 bit depth buffer
	kDepthFormat24,			// 24 bit depth buffer
	kDepthFormatCount		// keep this last!
};


enum SurfaceCreateFlags
{
	// unused (1<<0),
	kSurfaceCreateMipmap = (1<<1),
	kSurfaceCreateSRGB = (1<<2),
	kSurfaceCreateShadowmap = (1<<3),
	kSurfaceCreateRandomWrite = (1<<4),
	kSurfaceCreateSampleOnly = (1<<5),
	kSurfaceCreateNeverUsed = (1<<6),
	kSurfaceCreateAutoGenMips = (1<<7),
};


enum StencilOp
{
	kStencilOpKeep = 0,
	kStencilOpZero,
	kStencilOpReplace,
	kStencilOpIncrSat,
	kStencilOpDecrSat,
	kStencilOpInvert,
	kStencilOpIncrWrap,
	kStencilOpDecrWrap,
	kStencilOpCount
};


enum BlendOp
{
	kBlendOpAdd = 0,
	kBlendOpSub,
	kBlendOpRevSub,
	kBlendOpMin,
	kBlendOpMax,
	kBlendOpLogicalClear,
	kBlendOpLogicalSet,
	kBlendOpLogicalCopy,
	kBlendOpLogicalCopyInverted,
	kBlendOpLogicalNoop,
	kBlendOpLogicalInvert,
	kBlendOpLogicalAnd,
	kBlendOpLogicalNand,
	kBlendOpLogicalOr,
	kBlendOpLogicalNor,
	kBlendOpLogicalXor,
	kBlendOpLogicalEquiv,
	kBlendOpLogicalAndReverse,
	kBlendOpLogicalAndInverted,
	kBlendOpLogicalOrReverse,
	kBlendOpLogicalOrInverted,
	kBlendOpCount,
};


enum BlendMode
{
	kBlendZero = 0,
	kBlendOne,
	kBlendDstColor,
	kBlendSrcColor,
	kBlendOneMinusDstColor,
	kBlendSrcAlpha,
	kBlendOneMinusSrcColor,
	kBlendDstAlpha,
	kBlendOneMinusDstAlpha,
	kBlendSrcAlphaSaturate,
	kBlendOneMinusSrcAlpha,
	kBlendCount
};


enum CompareFunction
{
	kFuncUnknown = -1,
	kFuncDisabled = 0,
	kFuncNever,
	kFuncLess,
	kFuncEqual,
	kFuncLEqual,
	kFuncGreater,
	kFuncNotEqual,
	kFuncGEqual,
	kFuncAlways,
	kFuncCount
};


enum CullMode
{
	kCullUnknown = -1,
	kCullOff = 0,
	kCullFront,
	kCullBack,
	kCullCount
};


enum ColorWriteMask
{
	kColorWriteA = 1,
	kColorWriteB = 2,
	kColorWriteG = 4,
	kColorWriteR = 8,
	KColorWriteAll = (kColorWriteR|kColorWriteG|kColorWriteB|kColorWriteA)
};


enum ColorMaterialMode
{
	kColorMatUnknown = -1,
	kColorMatDisabled = 0,
	kColorMatEmission,
	kColorMatAmbientAndDiffuse,
	kColorMatTypeCount
};


enum TexGenMode
{
	kTexGenUnknown = -1,
	kTexGenDisabled = 0,
	kTexGenSphereMap,		// Spherical reflection map
	kTexGenObject,			// Object space
	kTexGenEyeLinear,		// Projected Eye space
	kTexGenCubeReflect,		// Cubemap reflection calculation
	kTexGenCubeNormal,		// Cubemap normal calculation
	kTexGenCount
};


enum ShaderType  // bit masks!
{
	kShaderNone = 0,
	kShaderVertex = 1,
	kShaderFragment = 2,
	kShaderGeometry = 3,
	kShaderHull = 4,
	kShaderDomain = 5,
	kShaderTypeCount // keep this last!
};

enum ShaderImplType
{
	kShaderImplUndefined = -1,

	// Must match ShaderType!
	kShaderImplVertex = 1,		// Vertex shader
	kShaderImplFragment = 2,	// Fragment shader
	kShaderImplGeometry = 3,
	kShaderImplHull = 4,
	kShaderImplDomain = 5,


	kShaderImplBoth = 6,		// Vertex+fragment (e.g. GLSL)
};


enum ShaderParamType
{
	kShaderParamFloat = 0,
	kShaderParamInt,
	kShaderParamBool,
	kShaderParamTypeCount
};


// Ordering is like this so it matches valid D3D9 FVF layouts
// Range must fit into an SInt8
enum ShaderChannel
{
	kShaderChannelNone = -1,
	kShaderChannelVertex = 0,	// Vertex (vector3)
	kShaderChannelNormal,		// Normal (vector3)
	kShaderChannelColor,		// Vertex color
	kShaderChannelTexCoord0,	// UV set 0 (vector2)
	kShaderChannelTexCoord1,	// UV set 1 (vector2)
	kShaderChannelTangent,		// Tangent (vector4)
	kShaderChannelCount,			// Keep this last!
};


enum ShaderChannelMask
{ 
	kShaderChannelsAll = ( (1<<kShaderChannelVertex) | (1<<kShaderChannelNormal) | (1<<kShaderChannelColor) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelTexCoord1) | (1<<kShaderChannelTangent) ), 
	kShaderChannelsHot = ( (1<<kShaderChannelVertex) | (1<<kShaderChannelNormal) | (1<<kShaderChannelTangent) ), 
	kShaderChannelsCold= ( (1<<kShaderChannelColor) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelTexCoord1) ), 
};


enum VertexComponent
{
	kVertexCompNone = 0,
	kVertexCompVertex,
	kVertexCompColor,
	kVertexCompNormal,
	kVertexCompTexCoord,
	kVertexCompTexCoord0, kVertexCompTexCoord1, kVertexCompTexCoord2, kVertexCompTexCoord3,
	kVertexCompTexCoord4, kVertexCompTexCoord5, kVertexCompTexCoord6, kVertexCompTexCoord7,
	kVertexCompAttrib0, kVertexCompAttrib1, kVertexCompAttrib2, kVertexCompAttrib3,
	kVertexCompAttrib4, kVertexCompAttrib5, kVertexCompAttrib6, kVertexCompAttrib7,
	kVertexCompAttrib8, kVertexCompAttrib9, kVertexCompAttrib10, kVertexCompAttrib11,
	kVertexCompAttrib12, kVertexCompAttrib13, kVertexCompAttrib14, kVertexCompAttrib15,
	kVertexCompCount // keep this last!
};


enum VertexChannelFormat
{
    kChannelFormatFloat = 0,
    kChannelFormatFloat16,
    kChannelFormatColor,
    kChannelFormatByte,
    kChannelFormatCount
};


enum FogMode
{
	kFogUnknown = -1,
	kFogDisabled = 0,
	kFogLinear,
	kFogExp,
	kFogExp2,
	kFogModeCount // keep this last!
};


enum NormalizationMode
{
	kNormalizationUnknown = -1,
	kNormalizationDisabled = 0,
	kNormalizationScale = 1,	// = kUniformScaleTransform,
	kNormalizationFull = 2		// = kNonUniformScaleTransform
};


enum CubemapFace
{
	kCubeFaceUnknown = -1,
	kCubeFacePX = 0,
	kCubeFaceNX,
	kCubeFacePY,
	kCubeFaceNY,
	kCubeFacePZ,
	kCubeFaceNZ,
};


enum GfxPrimitiveType
{
	kPrimitiveTriangles = 0,
	kPrimitiveTriangleStripDeprecated,
	kPrimitiveQuads,
	kPrimitiveLines,
	kPrimitiveLineStrip,
	kPrimitivePoints,

	kPrimitiveTypeCount, // keep this last!
	kPrimitiveForce32BitInt = 0x7fffffff // force 32 bit enum size
};


enum GfxImmediateType
{
	kImmediateVertex,
	kImmediateNormal,
	kImmediateColor,
	kImmediateTexCoordAll,
	kImmediateTexCoord,
};


enum GfxClearFlags
{
	kGfxClearColor = (1<<0),
	kGfxClearDepth = (1<<1),
	kGfxClearStencil = (1<<2),
	kGfxClearDepthStencil = kGfxClearDepth | kGfxClearStencil,
	kGfxClearAll = kGfxClearColor | kGfxClearDepth | kGfxClearStencil,
};


struct TextureID 
{
	explicit TextureID() : m_ID(0) { }
	explicit TextureID(unsigned int i) : m_ID(i) { }
	bool operator==(const TextureID& o) const { return m_ID==o.m_ID; }
	bool operator!=(const TextureID& o) const { return m_ID!=o.m_ID; }
	bool operator < (const TextureID& o) const { return m_ID<o.m_ID; }
	unsigned int m_ID;
};


struct ComputeBufferID {
	explicit ComputeBufferID() : m_ID(0) { }
	explicit ComputeBufferID(unsigned int i) : m_ID(i) { }
	bool IsValid() const { return m_ID != 0; }
	bool operator==(const ComputeBufferID& o) const { return m_ID==o.m_ID; }
	bool operator!=(const ComputeBufferID& o) const { return m_ID!=o.m_ID; }
	bool operator < (const ComputeBufferID& o) const { return m_ID<o.m_ID; }
	unsigned int m_ID;
};


#define VERTEX_FORMAT1(a) (1 << kShaderChannel##a)
#define VERTEX_FORMAT2(a,b) ((1 << kShaderChannel##a) | (1 << kShaderChannel##b))
#define VERTEX_FORMAT3(a,b,c) ((1 << kShaderChannel##a) | (1 << kShaderChannel##b) | (1 << kShaderChannel##c))
#define VERTEX_FORMAT4(a,b,c,d) ((1 << kShaderChannel##a) | (1 << kShaderChannel##b) | (1 << kShaderChannel##c) | (1 << kShaderChannel##d))
#define VERTEX_FORMAT5(a,b,c,d,e) ((1 << kShaderChannel##a) | (1 << kShaderChannel##b) | (1 << kShaderChannel##c) | (1 << kShaderChannel##d) | (1 << kShaderChannel##e))
#define VERTEX_FORMAT6(a,b,c,d,e,f) ((1 << kShaderChannel##a) | (1 << kShaderChannel##b) | (1 << kShaderChannel##c) | (1 << kShaderChannel##d) | (1 << kShaderChannel##e) | (1 << kShaderChannel##f))

#define kMaxVertexStreams 4


enum ComputeBufferFlags
{
	kCBFlagNone = 0,
	kCBFlagRaw = (1<<0),
	kCBFlagAppend = (1<<1),
	kCBFlagCounter = (1<<2),
	kCBFlagTypeMask = kCBFlagRaw | kCBFlagAppend | kCBFlagCounter,

	kCBFlagDrawIndirect = (1<<8),
};

enum BuiltinSamplerState
{
	kSamplerPointClamp = 0,
	kSamplerLinearClamp,
	kSamplerPointRepeat,
	kSamplerLinearRepeat,

	kBuiltinSamplerStateCount, // keep this last!
	kBuiltinSamplerStateForce32Bit = 0x7fffffff
};

struct TexEnvProperties
{
	TextureID			textureID;			// 4
	int					texturePropertyID;	// 4
	float				mipBias;			// 4
	UInt32				texDim : 4;			// 4
	UInt32				texGen : 4;
	UInt32				identityMatrix : 1;
	// ---- 16 bytes
};

struct TexEnvData : public TexEnvProperties // 16
{
	Matrix4x4f			matrix;				// 64
	// ---- 80 bytes
};

#if !UNITY_SPU
#include "GfxDeviceResources.h"
#endif

#if UNITY_XENON
enum HiZstate
{
	kHiZDisable = 0,
	kHiZEnable,
	kHiZAuto
};
enum HiSflush
{
	kHiSflush_async = 0,
	kHiSflush_sync
};
#endif
