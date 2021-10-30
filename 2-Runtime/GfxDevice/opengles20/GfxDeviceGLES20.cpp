#include "UnityPrefix.h"
#if GFX_SUPPORTS_OPENGLES20
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "External/shaderlab/Library/texenv.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Quaternion.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/program.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Threads/AtomicOps.h"
#include "Runtime/GfxDevice/TransformState.h"
#include "Runtime/GfxDevice/GpuProgramParamsApply.h"
#include "IncludesGLES20.h"
#include "AssertGLES20.h"
#include "ContextGLES20.h"
#include "VBOGLES20.h"
#include "TexturesGLES20.h"
#include "CombinerGLES20.h"
#include "GpuProgramsGLES20.h"
#include "FixedFunctionStateGLES20.h"
#include "ShaderGeneratorGLES20.h"
#include "RenderTextureGLES20.h"
#include "DebugGLES20.h"
#include "TimerQueryGLES20.h"
#include "TextureIdMapGLES20.h"
#include "UnityGLES20Ext.h"

#if UNITY_IPHONE
	#include "PlatformDependent/iPhonePlayer/iPhoneSettings.h"
#elif UNITY_ANDROID
	#include "PlatformDependent/AndroidPlayer/ContextGLES.h"
	#include "PlatformDependent/AndroidPlayer/EntryPoint.h"
	#include "PlatformDependent/AndroidPlayer/AndroidSystemInfo.h"
#endif

#include "Runtime/GfxDevice/GLDataBufferCommon.h"

#if NV_STATE_FILTERING

void filteredInitGLES20();
void filteredBindBufferGLES20(GLenum target, GLuint buffer, bool isImmediate);
void filteredVertexAttribPointerGLES20(GLuint  index,  GLint  size,  GLenum  type,  GLboolean  normalized,  GLsizei  stride,  const GLvoid *  pointer);
void filteredDeleteBuffersGLES20(GLsizei n, const GLuint *buffers);

#ifdef glBindBuffer
#undef glBindBuffer
#endif
#define glBindBuffer(a, b) filteredBindBufferGLES20(a, b, true)
#ifndef glDeleteBuffers
#define glDeleteBuffers filteredDeleteBuffersGLES20
#endif
#ifndef glVertexAttribPointer
#define glVertexAttribPointer filteredVertexAttribPointerGLES20
#endif

#endif

// let's play safe here:
//   ios/glesemu works just fine
//   and shadows demands more careful eps choice - do it later
#define WORKAROUND_POLYGON_OFFSET UNITY_ANDROID

// forward declarations

bool IsActiveRenderTargetWithColorGLES2(); // RenderTextureGL.cpp
namespace ShaderLab {
	TexEnv* GetTexEnvForBinding( const TextureBinding& binding, const PropertySheet* props ); // pass.cpp
}

// local forward declarations
struct DeviceStateGLES20;
static void ApplyBackfaceMode( const DeviceStateGLES20& state );
static GLuint GetSharedFBO (DeviceStateGLES20& state);

extern GLint gDefaultFBO;

// NOTE: GLES2.0 supports only 4 lights for now
enum { kMaxSupportedVertexLightsByGLES20 = 4 };

// Constant tables
static const unsigned int kBlendModeES2[] = {
	GL_ZERO, GL_ONE, GL_DST_COLOR, GL_SRC_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR,
	GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE, GL_ONE_MINUS_SRC_ALPHA,
};

static const unsigned int kBlendFuncES2[] = {
	GL_FUNC_ADD, GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT, GL_MIN_EXT, GL_MAX_EXT,
};

static const unsigned int kCmpFuncES2[] = {
	GL_NEVER, GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS
};

static const unsigned int kStencilOpES2[] = {
	GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR,
	GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP
};

static const GLenum kWrapModeES2[kTexWrapCount] = {
	GL_REPEAT,
	GL_CLAMP_TO_EDGE,
};

static const GLint kMinFilterES2[kTexFilterCount] = { GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR };

// --------------------------------------------------------------------------

struct DeviceDepthStateGLES20 : public DeviceDepthState
{
	UInt32      depthFunc;
};

struct DeviceStencilStateGLES20 : public DeviceStencilState
{
	GLenum  stencilFuncFront;
	GLenum  stencilFailOpFront;
	GLenum  depthFailOpFront;
	GLenum  depthPassOpFront;
	GLenum  stencilFuncBack;
	GLenum  stencilFailOpBack;
	GLenum  depthFailOpBack;
	GLenum  depthPassOpBack;
};


typedef std::map< GfxBlendState, DeviceBlendState,  memcmp_less<GfxBlendState> > CachedBlendStates;
typedef std::map< GfxDepthState, DeviceDepthStateGLES20,  memcmp_less<GfxDepthState> > CachedDepthStates;
typedef std::map< GfxStencilState, DeviceStencilStateGLES20,  memcmp_less<GfxStencilState> > CachedStencilStates;
typedef std::map< GfxRasterState, DeviceRasterState,  memcmp_less<GfxRasterState> > CachedRasterStates;

// --------------------------------------------------------------------------
struct TextureUnitStateGLES2
{
	GLuint              texID;
	TextureDimension    texDim;
	unsigned int        combColor, combAlpha;
	Vector4f            color;
	TexGenMode          texGen;
	Matrix4x4f          textureMatrix;
	float               bias;

	// right let waste space here instead of device-level int
	bool                identityMatrix;
	bool                isProjected;
	bool                posForTexGen;
	bool                nrmForTexGen;

	void    Invalidate();

	static bool PositionRequiredForTexGen(TexGenMode mode)
	{
		return (mode == kTexGenObject || mode == kTexGenEyeLinear);
	}

	static bool NormalRequiredForTexGen(TexGenMode mode)
	{
		return (mode == kTexGenSphereMap || mode == kTexGenCubeReflect || mode == kTexGenCubeNormal);
	}

	void        SetTexGen( TexGenMode mode )
	{
		posForTexGen = PositionRequiredForTexGen(mode);
		nrmForTexGen = NormalRequiredForTexGen(mode);

		texGen = mode;
	}
};

void TextureUnitStateGLES2::Invalidate()
{
	texID = -1;
	texDim = kTexDimNone;
	combColor = combAlpha = 0xFFFFFFFF;
	color.Set( -1, -1, -1, -1 );
	texGen = kTexGenUnknown;
	posForTexGen = 0;
	nrmForTexGen = 0;
	textureMatrix.SetIdentity();
	identityMatrix = true;
	isProjected = false;
	bias = 1.0e6f;
}


// --------------------------------------------------------------------------
// TODO: optimize this. Right now we just send off whole 8 float3 UVs with each
// immediate mode vertex. We could at least detect the number of them used from
// ImmediateTexCoord calls.
struct ImmediateVertexGLES20 {
	Vector3f    vertex;
	Vector3f    normal;
	UInt32      color;
	Vector3f    texCoords[8];
};

struct ImmediateModeGLES20 {
	std::vector<ImmediateVertexGLES20>  m_Vertices;
	ImmediateVertexGLES20               m_Current;
	GfxPrimitiveType                    m_Mode;
	UInt16*                             m_QuadsIB;

	int                                 m_IndexBufferQuadsID;

	ImmediateModeGLES20();
	~ImmediateModeGLES20();
	void Invalidate();
};

// --------------------------------------------------------------------------
struct DeviceStateGLES20
{
	GLuint          m_SharedFBO;
	GLuint          m_HelperFBO;
	int             m_TextureIDGenerator;

	int             depthFunc;
	int             depthWrite; // 0/1 or -1

	int             blending;
	int             srcBlend, destBlend, srcBlendAlpha, destBlendAlpha; // Blend modes
	int             blendOp, blendOpAlpha;
	CompareFunction alphaTest;
	float           alphaValue;

	CullMode        culling;
	bool            appBackfaceMode, userBackfaceMode;
	NormalizationMode
					normalization;
	int             scissor;

	bool            lighting;
	bool            separateSpecular;
	SimpleVec4      matDiffuse, matAmbient, matSpecular, matEmissive;
	SimpleVec4      ambient;
	float           matShininess;
	ColorMaterialMode
					colorMaterial;

	float           offsetFactor, offsetUnits;

	int             colorWriteMask; // ColorWriteMask combinations
	SimpleVec4      color;

	TextureUnitStateGLES2
					textures[kMaxSupportedTextureUnitsGLES];
	int             textureCount;
	int             activeTextureUnit;

	// pure optimization: texGen is very special case and is used sparingly
	UInt32          positionTexGen;
	UInt32          normalTexGen;

	int             vertexLightCount;
	LightType       vertexLightTypes[kMaxSupportedVertexLights];

    TransformState  transformState;

	DynamicVBO*     m_DynamicVBO;
	bool            vboContainsColor;

	int             viewport[4];
	int             scissorRect[4];

	// should be set before BeforeDrawCall call
	GfxPrimitiveType	drawCallTopology;


    GpuProgram*                     activeProgram;
    const GpuProgramParameters*     activeProgramParams;
    dynamic_array<UInt8>            activeProgramParamsBuffer;
    UInt32                          activeProgramID;


	CachedBlendStates   m_CachedBlendStates;
	CachedDepthStates   m_CachedDepthStates;
	CachedStencilStates m_CachedStencilStates;
	CachedRasterStates  m_CachedRasterStates;

	const DeviceBlendState*         m_CurrBlendState;
	const DeviceDepthStateGLES20*   m_CurrDepthState;
	const DeviceStencilStateGLES20* m_CurrStencilState;
	int                             m_StencilRef;
	const DeviceRasterState*        m_CurrRasterState;

	ImmediateModeGLES20 m_Imm;

public:
	DeviceStateGLES20();
	void Invalidate();
	void ComputeFixedFunctionState(FixedFunctionStateGLES20& state, const GfxFogParams& fog) const;

	inline void ApplyTexGen( UInt32 unit );
	inline void DropTexGen( UInt32 unit );
};

DeviceStateGLES20::DeviceStateGLES20()
:   m_DynamicVBO(0)
{
	m_TextureIDGenerator = 0;
}

void DeviceStateGLES20::Invalidate()
{
	DBG_LOG_GLES20("Invalidate");
	int i;

	depthFunc = -1; //unknown
	depthWrite = -1;

	blending = -1; // unknown
	srcBlend = destBlend = srcBlendAlpha = destBlendAlpha = -1; // won't match any GL mode
	blendOp = blendOpAlpha = -1;
	alphaTest = kFuncUnknown;
	alphaValue = -1.0f;

	culling = kCullUnknown;
	normalization = kNormalizationUnknown;
	scissor = -1;

	lighting = false;
	separateSpecular = false;

	matDiffuse.set( -1, -1, -1, -1 );
	matAmbient.set( -1, -1, -1, -1 );
	matSpecular.set( -1, -1, -1, -1 );
	matEmissive.set( -1, -1, -1, -1 );
	ambient.set( -1, -1, -1, -1 );
	matShininess = -1.0f;
	colorMaterial = kColorMatUnknown;

	offsetFactor = offsetUnits = -1000.0f;

	colorWriteMask = -1; // TBD ?
	m_StencilRef = -1;

	color.set( -1, -1, -1, -1 );

	activeTextureUnit = -1;
	for( i = 0; i < kMaxSupportedTextureUnitsGLES; ++i )
		textures[i].Invalidate();
	textureCount = 0;

	positionTexGen  = 0;
	normalTexGen    = 0;

	vertexLightCount = 0;
	for ( i = 0; i < kMaxSupportedVertexLights; ++i)
		vertexLightTypes[i] = kLightDirectional;

	// make sure backface mode is in sync
	appBackfaceMode = false;
	userBackfaceMode = false;
	ApplyBackfaceMode( *this );

	vboContainsColor = true;

	viewport[0] = 0;
	viewport[1] = 0;
	viewport[2] = 0;
	viewport[3] = 0;

	scissorRect[0] = 0;
	scissorRect[1] = 0;
	scissorRect[2] = 0;
	scissorRect[3] = 0;

	activeProgram = 0;
	activeProgramParams = 0;
	activeProgramParamsBuffer.resize_uninitialized(0);
	activeProgramID = -1;

	m_Imm.Invalidate();

	InvalidateVertexInputCacheGLES20();

	GLESAssert();
}

void DeviceStateGLES20::ComputeFixedFunctionState(FixedFunctionStateGLES20& state, const GfxFogParams& fog) const
{
	if (lighting)
	{
		int  numLights = vertexLightCount;
		bool onlyDir   = true;
		for(int i = 0 ; i < numLights ; ++i)
		{
			onlyDir = onlyDir && (vertexLightTypes[i] == kLightDirectional);
			state.SetLightType(i,vertexLightTypes[i]);
		}

		state.lightingEnabled = true;
		state.lightCount = numLights;
		state.onlyDirectionalLights = onlyDir;
		state.specularEnabled = separateSpecular;

		switch (colorMaterial)
		{
			case kColorMatDisabled:
				break;

			case kColorMatAmbientAndDiffuse:
				state.useVertexColorAsAmbientAndDiffuse = true;
				break;

			case kColorMatEmission:
				state.useVertexColorAsEmission = true;
				break;

			default:
				ErrorString("Unsupported color material mode");
				break;
		}
	}
	else
	{
		state.lightingEnabled = false;
		state.lightCount = 0;
	}

	state.useUniformInsteadOfVertexColor = !vboContainsColor;
	state.texUnitCount = textureCount;

	for (int i = 0; i < textureCount; i++)
	{
		Assert(textures[i].texDim != kTexDimUnknown);
		Assert(textures[i].texDim != kTexDimNone);
		Assert(textures[i].texDim != kTexDimAny);
		Assert(textures[i].texDim != kTexDim3D); // OpenGLES2.0 does NOT supports 3D textures
		state.texUnitCube[i] = (textures[i].texDim == kTexDimCUBE);
		state.texUnitColorCombiner[i] = textures[i].combColor,
		state.texUnitAlphaCombiner[i] = textures[i].combAlpha;
		state.texUnitGen[i] = textures[i].texGen;

		bool needMatrix = !textures[i].identityMatrix || textures[i].texGen > kTexGenDisabled;
		state.SetTexUnitMatrixParam(i, needMatrix, textures[i].isProjected);
	}

	state.fogMode = fog.mode;
	switch (fog.mode)
	{
		case kFogUnknown:
		case kFogDisabled:
			state.fogMode = kFogDisabled;
		default:
			break;
	}

	if(gGraphicsCaps.gles20.hasAlphaTestQCOM)
	{
		// we dont want to generate special shader if we have alpha-test done gl style
		state.alphaTest = kFuncDisabled;
	}
	else
	{
		state.alphaTest = alphaTest;
		switch (alphaTest)
		{
			case kFuncNever:    /* \todo Disable drawing. */
			case kFuncUnknown:
			case kFuncDisabled:
			case kFuncAlways:
				state.alphaTest = kFuncDisabled;
			default:
				break;
		}
	}

	state.setupPointSize = drawCallTopology == kPrimitivePoints;
}

inline void DeviceStateGLES20::ApplyTexGen( UInt32 unit )
{
	const TextureUnitStateGLES2& state = textures[unit];

	positionTexGen = state.posForTexGen ? positionTexGen |  (1<<unit)
										: positionTexGen & ~(1<<unit);

	normalTexGen   = state.nrmForTexGen ? normalTexGen |  (1<<unit)
										: normalTexGen & ~(1<<unit);
}

inline void  DeviceStateGLES20::DropTexGen( UInt32 unit )
{
	positionTexGen &= ~(1<<unit);
	normalTexGen   &= ~(1<<unit);
}


#include "GfxDeviceGLES20.h"
#include "Runtime/GfxDevice/GLESCommon.h"

void GfxDeviceGLES20_MarkWorldViewProjDirty()
{
	GFX_GL_IMPL& device = static_cast<GFX_GL_IMPL&>(GetRealGfxDevice());
	GetGLES20DeviceState(device).transformState.dirtyFlags |= TransformState::kWorldViewProjDirty;
}

void GfxDeviceGLES20_DisableDepthTest()
{
	GFX_GL_IMPL& device = static_cast<GFX_GL_IMPL&>(GetRealGfxDevice());
	GetGLES20DeviceState(device).depthFunc = GL_NEVER;
	GLES_CHK(glDisable(GL_DEPTH_TEST));
}

void GfxDeviceGLES20_SetDrawCallTopology(GfxPrimitiveType topology)
{
	GFX_GL_IMPL& device = static_cast<GFX_GL_IMPL&>(GetRealGfxDevice());
	GetGLES20DeviceState(device).drawCallTopology = topology;
}


#define GL_RT_COMMON_GLES2 1
#include "Runtime/GfxDevice/GLRTCommon.h"
#undef GL_RT_COMMON_GLES2

void GraphicsCaps::InitGLES20()
{
    GLES_InitCommonCaps(this);
    gGlesExtFunc.InitExtFunc();

	shaderCaps = kShaderLevel3;

	maxLights       = kMaxSupportedVertexLightsByGLES20; // vertex light count
	hasAnisoFilter  = QueryExtension("GL_EXT_texture_filter_anisotropic");      // has anisotropic filtering?
	if (hasAnisoFilter)
	{
	#if UNITY_PEPPER
		// Google's implementation incorrectly reports a maxAnisoLevel of 1.
		// Until they fix this, just set it here.
		maxAnisoLevel = 16;
	#else
		GLES_CHK(glGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *)&maxAnisoLevel ));
	#endif
	}
	else
		maxAnisoLevel = 1;


	maxTexImageUnits        = 8;
	GLES_CHK(glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexImageUnits));
	maxTexImageUnits        = std::max<int>(std::min<int>( maxTexImageUnits, kMaxSupportedTextureUnitsGLES ), 1);

	maxTexUnits             = maxTexImageUnits;
	maxTexCoords            = maxTexImageUnits;

	GLES_CHK(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize));
	GLES_CHK(glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeMapSize));
	GLES_CHK(glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderTextureSize));

	hasMipLevelBias         = QueryExtension("GL_EXT_texture_lod_bias");    // can apply bias for mips?
	hasMipMaxLevel          = QueryExtension("GL_APPLE_texture_max_level");

    gles20.hasAppleMSAA     = QueryExtension("GL_APPLE_framebuffer_multisample");
    gles20.hasImgMSAA       = QueryExtension("GL_IMG_multisampled_render_to_texture") && gGlesExtFunc.glRenderbufferStorageMultisampleIMG && gGlesExtFunc.glFramebufferTexture2DMultisampleIMG;

    hasMultiSampleAutoResolve   = gles20.hasImgMSAA;
    hasMultiSample              = gles20.hasAppleMSAA || gles20.hasImgMSAA;


	hasBlendSquare          = true;
	hasSeparateAlphaBlend   = true;
	hasBlendSub             = true;
	hasBlendMinMax          = QueryExtension("GL_EXT_blend_minmax");

	hasS3TCCompression      = false;

	hasAutoMipMapGeneration = true;

	has3DTexture            = false;

	npot = kNPOTRestricted;
	if( QueryExtension("GL_OES_texture_npot") || QueryExtension("GL_ARB_texture_non_power_of_two") )
		npot = kNPOTFull;

	npotRT = npot;

	hasRenderToTexture      = true; // We have render-to-texture functionality.
	hasShadowCollectorPass  = false;

	hasHighPrecisionTextureCombiners = false;

	hasRenderToCubemap      = true;

	supportsTextureFormat[kTexFormatBGRA32]         = QueryExtension("GL_APPLE_texture_format_BGRA8888") || QueryExtension("GL_EXT_texture_format_BGRA8888");
	supportsTextureFormat[kTexFormatAlphaLum16]     = false;
	supportsTextureFormat[kTexFormatARGB32]         = false; // OpenGL ES has no support for INT_8_8_8_8 format, so don't bother with Unity reversed formats at all
	supportsTextureFormat[kTexFormatBGR24]          = false; // OpenGL ES has no support for INT_8_8_8_8 format, so don't bother with Unity reversed formats at all
	supportsTextureFormat[kTexFormatDXT1]           = QueryExtension("GL_EXT_texture_compression_s3tc") || QueryExtension("GL_EXT_texture_compression_dxt1");
	supportsTextureFormat[kTexFormatDXT3]           = QueryExtension("GL_EXT_texture_compression_s3tc") || QueryExtension("GL_CHROMIUM_texture_compression_dxt3");
	supportsTextureFormat[kTexFormatDXT5]           = QueryExtension("GL_EXT_texture_compression_s3tc") || QueryExtension("GL_CHROMIUM_texture_compression_dxt5");
	supportsTextureFormat[kTexFormatPVRTC_RGB2]     = QueryExtension("GL_IMG_texture_compression_pvrtc");
	supportsTextureFormat[kTexFormatPVRTC_RGBA2]    = QueryExtension("GL_IMG_texture_compression_pvrtc");
	supportsTextureFormat[kTexFormatPVRTC_RGB4]     = QueryExtension("GL_IMG_texture_compression_pvrtc");
	supportsTextureFormat[kTexFormatPVRTC_RGBA4]    = QueryExtension("GL_IMG_texture_compression_pvrtc");
	supportsTextureFormat[kTexFormatETC_RGB4]       = QueryExtension("GL_OES_compressed_ETC1_RGB8_texture");
	supportsTextureFormat[kTexFormatATC_RGB4]       = QueryExtension("GL_AMD_compressed_ATC_texture") || QueryExtension("GL_ATI_texture_compression_atitc");
	supportsTextureFormat[kTexFormatATC_RGBA8]      = QueryExtension("GL_AMD_compressed_ATC_texture") || QueryExtension("GL_ATI_texture_compression_atitc");

    supportsRenderTextureFormat[kRTFormatARGB32]    = true;

    {
        const bool supportsHalfRB = QueryExtension("GL_EXT_color_buffer_half_float");
        // if we dont have OES_texture_half_float we cant have half texture (only RB)
        const bool supportsHalfRT = supportsHalfRB && QueryExtension("GL_OES_texture_half_float");
	const bool supportsRG = QueryExtension("GL_EXT_texture_rg");

        supportsRenderTextureFormat[kRTFormatARGBHalf]  = supportsHalfRT;
	supportsRenderTextureFormat[kRTFormatR8]        = supportsRG;
        supportsRenderTextureFormat[kRTFormatRHalf]     = supportsHalfRT && supportsRG;
        supportsRenderTextureFormat[kRTFormatRGHalf]    = supportsHalfRT && supportsRG;
    }

	{
		FBColorFormatCheckerGLES2 checker;

		supportsRenderTextureFormat[kRTFormatRGB565]	= checker.CheckFormatSupported(GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
		supportsRenderTextureFormat[kRTFormatARGB4444]	= checker.CheckFormatSupported(GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4);
		supportsRenderTextureFormat[kRTFormatARGB1555]	= checker.CheckFormatSupported(GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1);

        // for float formats there is no ext to check rif they are renderable, so do manually
        if(QueryExtension("OES_texture_float"))
		{
            supportsRenderTextureFormat[kRTFormatARGBHalf]  = checker.CheckFormatSupported(GL_RGBA, GL_RGBA, GL_FLOAT);
            if(QueryExtension("GL_EXT_texture_rg"))
			{
                // TODO: move all this unity gles ext header
                supportsRenderTextureFormat[kRTFormatRFloat]    = checker.CheckFormatSupported(0x1903, 0x1903, GL_FLOAT);
                supportsRenderTextureFormat[kRTFormatRGFloat]   = checker.CheckFormatSupported(0x8227, 0x8227, GL_FLOAT);
            }
			}
		}

    const bool isPvrGpu    = (rendererString.find("PowerVR") != string::npos);
    const bool isMaliGpu   = (rendererString.find("Mali") != string::npos);
    const bool isAdrenoGpu = (rendererString.find("Adreno") != string::npos);
    const bool isTegraGpu  = (rendererString.find("Tegra") != string::npos);

	// Only support stencil when we have packed depth stencil, for simplicity
	hasStencil = hasTwoSidedStencil = hasRenderTargetStencil = QueryExtension("GL_OES_packed_depth_stencil");

#if UNITY_BB10
    if(isAdrenoGpu)
		hasRenderTargetStencil = false;
#endif

	// Adreno 2xx seems to not like a texture attached to color & depth & stencil at once;
	// Adreno 3xx seems to be fine. Most 3xx devices have GL_OES_depth_texture_cube_map extension
	// present, and 2xx do not. So detect based on that.
	const bool isAdreno2xx = isAdrenoGpu && !QueryExtension("GL_OES_depth_texture_cube_map");
	if (isAdreno2xx)
		hasRenderTargetStencil = false;

	hasNativeDepthTexture  = QueryExtension("GL_OES_depth_texture") || QueryExtension ("GL_GOOGLE_depth_texture");
#if UNITY_ANDROID
    if(android::systeminfo::ApiLevel() < android::apiHoneycomb || (android::systeminfo::ApiLevel() < android::apiIceCreamSandwich && isPvrGpu))
		hasNativeDepthTexture = false;
#endif

	hasStencilInDepthTexture = hasRenderTargetStencil && hasNativeDepthTexture;
	gles20.has24DepthForFBO = QueryExtension("GL_OES_depth24");

	hasNativeShadowMap = QueryExtension("GL_EXT_shadow_samplers");

	supportsRenderTextureFormat[kRTFormatA2R10G10B10] = false;
	supportsRenderTextureFormat[kRTFormatARGB64] = false;
	supportsRenderTextureFormat[kRTFormatDepth] = hasNativeDepthTexture;
	supportsRenderTextureFormat[kRTFormatShadowMap] = hasNativeShadowMap;


	has16BitFloatVertex = QueryExtension("GL_OES_vertex_half_float");
	needsToSwizzleVertexColors  = false;

	// ---- driver bug/workaround flags

#if UNITY_LINUX
    if(isTegraGpu)
	{
		supportsRenderTextureFormat[kRTFormatDepth] = hasNativeDepthTexture = true;
		hasRenderTargetStencil = true;
		hasStencilInDepthTexture = true;
		hasTwoSidedStencil = true;
	}
#endif

	hasSRGBReadWrite        = QueryExtension("GL_EXT_sRGB");
	// TODO: we should check srgb+compressed on gpu-case basis, but for now it is tegra only anyway ;-)
	hasSRGBReadWrite        = hasSRGBReadWrite && QueryExtension("GL_NV_sRGB_formats");

	disableSoftShadows      = true;

	hasShadowCollectorPass  = false;

	// ---- gles20 specifics

    gles20.hasGLSL          = true;
    gles20.maxAttributes    = 16;
	gles20.hasNLZ           = QueryExtension("GL_NV_depth_nonlinear");
    gles20.hasAlphaTestQCOM = QueryExtension("GL_QCOM_alpha_test") && gGlesExtFunc.glAlphaFuncQCOM;

    gles20.hasMapbuffer     = QueryExtension("GL_OES_mapbuffer") && gGlesExtFunc.glMapBufferOES && gGlesExtFunc.glUnmapBufferOES;
    gles20.hasMapbufferRange= QueryExtension("GL_EXT_map_buffer_range") && gGlesExtFunc.glMapBufferRangeEXT && gGlesExtFunc.glUnmapBufferOES;

#if UNITY_PEPPER
	// it was cut out in ifdef before, so lets play safe and assume we dont have map
	gles20.hasMapbuffer = gles20.hasMapbufferRange = false;
#endif

    gles20.hasBinaryShaders = (UNITY_ANDROID || UNITY_BLACKBERRY || UNITY_TIZEN) && QueryExtension("GL_OES_get_program_binary") && GlslGpuProgramGLES20::InitBinaryShadersSupport();

    gles20.hasNVMRT         = QueryExtension("GL_NV_draw_buffers") && QueryExtension("GL_NV_fbo_color_attachments") && gGlesExtFunc.glDrawBuffersNV;
	if(gles20.hasNVMRT)
	{
		GLES_CHK(glGetIntegerv(GL_MAX_DRAW_BUFFERS_NV, &maxMRTs));
		maxMRTs = clamp<int> (maxMRTs, 1, kMaxSupportedRenderTargets);
	}

#if UNITY_IPHONE
	if( iphone::GetDeviceGeneration() == iphone::kiPhoneGenerationiPad2Gen && iphone::isIOSVersionOrNewerButBefore("4.3", "5.0") )
		gles20.buggyColorMaskBlendMSAA = true;
#endif

#if UNITY_ANDROID || UNITY_BB10
	// Adreno (Qualcomm chipsets) have a driver bug (Android2.2)
	// which fails to patch vfetch instructions under certain circumstances
    gles20.buggyVFetchPatching = isAdrenoGpu;
#endif

	// Mali-400 MP? They promised to fix drivers (they know about the issue)
    gles20.buggyDisableVAttrKeepsActive = isMaliGpu;

#if UNITY_ANDROID
	// samsung galaxy on mali + ics update ruined shader compiler
	// most osam part: on some shaders tex sample in vprog result in crash close to kernel (no callstack in logcat)
    gles20.buggyVprogTextures = isMaliGpu && (android::systeminfo::ApiLevel() >= android::apiIceCreamSandwich);
#endif

	// on android pvr if we enable dynamic geom drawing from memory sometimes we hit crash in the driver
	// it seems that some internal caching goes crazy - solved by actually drawing watermark
	// This is also needed on BB10 devices (both pvr and adreno) to fix video display issues
    gles20.needToRenderWatermarkDueToDrawFromMemoryBuggy = (UNITY_ANDROID && isPvrGpu) || UNITY_BB10;

	// PowerVR GPUs have slow discard
    gles20.slowAlphaTest = isPvrGpu;

	// on adreno devices drivers don't like if+discard combo
	// so set slowAlphaTest here too, to remove trivial clip
    gles20.slowAlphaTest |= isAdrenoGpu;

	// orphaning path is terribly slower universally
	//   apart from some corner cases on ios where it is not faster anyway
	gles20.hasVBOOrphaning = false;

    gles20.hasDebugMarkers       = QueryExtension("GL_EXT_debug_marker") && gGlesExtFunc.glPushGroupMarkerEXT && gGlesExtFunc.glPopGroupMarkerEXT;
    gles20.hasDiscardFramebuffer = QueryExtension("GL_EXT_discard_framebuffer") && gGlesExtFunc.glDiscardFramebufferEXT;
	gles20.hasHalfLinearFilter   = QueryExtension("GL_OES_texture_half_float_linear");


    gles20.slowDynamicVBO           = isPvrGpu || isAdrenoGpu || isMaliGpu;
    gles20.forceStaticBatchFromMem  = isMaliGpu;
#if UNITY_IPHONE
	gles20.forceStaticBatchFromMem  = true;
#endif
	gles20.needFlushAfterTextureUpload = isAdrenoGpu;

	gles20.needFlushAfterTextureUpload = isAdrenoGpu;

    hasTiledGPU              = isPvrGpu || isAdrenoGpu || isMaliGpu;

    // on Tegra (for now all drivers) there is a bug that sometimes results in samplers not being reported
    // the only known workaround is to force highp default precision
    gles20.forceHighpFSPrec = isTegraGpu;

    // on Adreno (Nexus4 at least) there is a bug that sometimes results in program link crash
    // the only known workaround is to force highp default precision
    gles20.forceHighpFSPrec = isAdrenoGpu;


    GLES_CHK(glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &gles20.maxAttributes));
    GLES_CHK(glGetIntegerv(GL_MAX_VARYING_VECTORS, &gles20.maxVaryings));

    if(gles20.hasAppleMSAA)
        GLES_CHK(glGetIntegerv(GL_MAX_SAMPLES_APPLE, &gles20.maxSamples));
    if(gles20.hasImgMSAA)
        GLES_CHK(glGetIntegerv(GL_MAX_SAMPLES_IMG, &gles20.maxSamples));

#if ENABLE_PROFILER
	g_TimerQueriesGLES.Init();
#endif
#if NV_STATE_FILTERING
	filteredInitGLES20();
#endif
}

GfxDevice* CreateGLES20GfxDevice()
{
#if UNITY_WIN || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN || UNITY_ANDROID
	InitializeGLES20();
#endif
	gGraphicsCaps.InitGLES20();
#if UNITY_EDITOR
	return NULL;
#else
	return UNITY_NEW_AS_ROOT(GFX_GL_IMPL(), kMemGfxDevice, "GLES20GfxDevice","");
#endif
}

GFX_GL_IMPL::GFX_GL_IMPL()
{
	;;printf_console("Creating OpenGLES2.0 graphics device\n");
	#if !GFX_DEVICE_VIRTUAL
	impl = new GfxDeviceImpl();
	#endif

	// ??? ReJ: (Q for Tomas) WHY InvalidateState was commented out here? InvalidateState is necessary to enable correct back-face culling
	OnCreate();
	InvalidateState();
#if UNITY_PEPPER || UNITY_WEBGL
	m_Renderer = kGfxRendererOpenGLES20Desktop;
#else
	m_Renderer = kGfxRendererOpenGLES20Mobile;
#endif
	m_IsThreadable = true;

	m_UsesOpenGLTextureCoords = true;
	m_UsesHalfTexelOffset = false;

	STATE.m_CurrBlendState = NULL;
	STATE.m_CurrDepthState = NULL;
	STATE.m_CurrStencilState = NULL;
	STATE.m_CurrRasterState = NULL;
	STATE.m_SharedFBO = STATE.m_HelperFBO = 0;

	InitBackBufferGLES2(&m_BackBufferColor.object, &m_BackBufferDepth.object);
}

GFX_GL_IMPL::~GFX_GL_IMPL()
{
	delete STATE.m_DynamicVBO;

	#if !GFX_DEVICE_VIRTUAL
	delete impl;
	#endif
#if UNITY_WIN || UNITY_ANDROID
	ShutdownGLES20();
#endif
}


static void ActivateTextureUnitGLES2 (DeviceStateGLES20& state, int unit)
{
	if (state.activeTextureUnit == unit)
		return;
	GLES_CHK(glActiveTexture(GL_TEXTURE0 + unit));
	state.activeTextureUnit = unit;
}


void GFX_GL_IMPL::InvalidateState()
{
	DBG_LOG_GLES20("InvalidateState");
	m_FogParams.Invalidate();
	STATE.transformState.Invalidate(m_BuiltinParamValues);
	STATE.Invalidate();

#if NV_STATE_FILTERING
	StateFiltering_InvalidateVBOCacheGLES20();
#endif

	m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatEmission, Vector4f(1,1,1,1));
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatAmbient, Vector4f(1,1,1,1));
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatDiffuse, Vector4f(1,1,1,1));
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatSpecular, Vector4f(1,1,1,1));
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatShininess, Vector4f(1,1,1,1));

	m_BuiltinParamValues.SetVectorParam(kShaderVecFFFogColor, Vector4f(0,0,0,0));
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFFogParams, Vector4f(0,0,0,0));

	m_BuiltinParamValues.SetVectorParam(kShaderVecFFAlphaTestRef, Vector4f(0,0,0,0));

	m_BuiltinParamValues.SetVectorParam(kShaderVecFFColor, Vector4f(1,1,1,1));
	for (int i = 0; i < kMaxSupportedTextureUnitsGLES; i++)
		m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecFFTextureEnvColor0+i), Vector4f(0,0,0,0));
}



DeviceBlendState* GFX_GL_IMPL::CreateBlendState(const GfxBlendState& state)
{
	std::pair<CachedBlendStates::iterator, bool> result = STATE.m_CachedBlendStates.insert(std::make_pair(state, DeviceBlendState()));
	if (!result.second)
		return &result.first->second;

	DeviceBlendState& glstate = result.first->second;
	memcpy(&glstate.sourceState, &state, sizeof(glstate.sourceState));
	DebugAssertIf(kFuncUnknown==state.alphaTest);

	return &result.first->second;
}


DeviceDepthState* GFX_GL_IMPL::CreateDepthState(const GfxDepthState& state)
{
	std::pair<CachedDepthStates::iterator, bool> result = STATE.m_CachedDepthStates.insert(std::make_pair(state, DeviceDepthStateGLES20()));
	if (!result.second)
		return &result.first->second;

	DeviceDepthStateGLES20& glstate = result.first->second;
	memcpy(&glstate.sourceState, &state, sizeof(glstate.sourceState));
	glstate.depthFunc = kCmpFuncES2[state.depthFunc];
	return &result.first->second;
}

DeviceStencilState* GFX_GL_IMPL::CreateStencilState(const GfxStencilState& state)
{
	std::pair<CachedStencilStates::iterator, bool> result = STATE.m_CachedStencilStates.insert(std::make_pair(state, DeviceStencilStateGLES20()));
	if (!result.second)
		return &result.first->second;

	DeviceStencilStateGLES20& st = result.first->second;
	memcpy (&st.sourceState, &state, sizeof(st.sourceState));
	st.stencilFuncFront = kCmpFuncES2[state.stencilFuncFront];
	st.stencilFailOpFront = kStencilOpES2[state.stencilFailOpFront];
	st.depthFailOpFront = kStencilOpES2[state.stencilZFailOpFront];
	st.depthPassOpFront = kStencilOpES2[state.stencilPassOpFront];
	st.stencilFuncBack = kCmpFuncES2[state.stencilFuncBack];
	st.stencilFailOpBack = kStencilOpES2[state.stencilFailOpBack];
	st.depthFailOpBack = kStencilOpES2[state.stencilZFailOpBack];
	st.depthPassOpBack = kStencilOpES2[state.stencilPassOpBack];
	return &result.first->second;
}

DeviceRasterState* GFX_GL_IMPL::CreateRasterState(const GfxRasterState& state)
{
	std::pair<CachedRasterStates::iterator, bool> result = STATE.m_CachedRasterStates.insert(std::make_pair(state, DeviceRasterState()));
	if (!result.second)
		return &result.first->second;

	DeviceRasterState& glstate = result.first->second;
	memcpy(&glstate.sourceState, &state, sizeof(glstate.sourceState));

	return &result.first->second;
}

void GFX_GL_IMPL::SetBlendState(const DeviceBlendState* state, float alphaRef)
{
	DeviceBlendState* devstate = (DeviceBlendState*)state;

	if (STATE.m_CurrBlendState == devstate && alphaRef == STATE.alphaValue)
		return;

	STATE.m_CurrBlendState = devstate;
	if (!STATE.m_CurrBlendState)
		return;

	const GfxBlendState& desc = state->sourceState;
	const GLenum glsrc = kBlendModeES2[desc.srcBlend];
	const GLenum gldst = kBlendModeES2[desc.dstBlend];
	const GLenum glsrca = kBlendModeES2[desc.srcBlendAlpha];
	const GLenum gldsta = kBlendModeES2[desc.dstBlendAlpha];
	const GLenum glfunc = kBlendFuncES2[desc.blendOp];
	const GLenum glfunca = kBlendFuncES2[desc.blendOpAlpha];
	const bool blendDisabled = (glsrc == GL_ONE && gldst == GL_ZERO && glsrca == GL_ONE && gldsta == GL_ZERO);

	int mask = devstate->sourceState.renderTargetWriteMask;

	if (!IsActiveRenderTargetWithColorGLES2())
		mask = 0;

#if UNITY_IPHONE
	if( (mask && mask != 15) && gGraphicsCaps.gles20.buggyColorMaskBlendMSAA && !blendDisabled && IsActiveMSAARenderTargetGLES2() )
		mask = 15;
#endif

	if( mask != STATE.colorWriteMask )
	{
		GLES_CHK(glColorMask( (mask & kColorWriteR) != 0, (mask & kColorWriteG) != 0, (mask & kColorWriteB) != 0, (mask & kColorWriteA) != 0 ));
		STATE.colorWriteMask = mask;
	}

	if( blendDisabled )
	{
		if( STATE.blending != 0 )
		{
			GLES_CHK(glDisable (GL_BLEND));
			STATE.blending = 0;
		}
	}
	else
	{
		if( glsrc != STATE.srcBlend || gldst != STATE.destBlend || glsrca != STATE.srcBlendAlpha || gldsta != STATE.destBlendAlpha )
		{
			GLES_CHK(glBlendFuncSeparate(glsrc, gldst, glsrca, gldsta));
			STATE.srcBlend = glsrc;
			STATE.destBlend = gldst;
			STATE.srcBlendAlpha = glsrca;
			STATE.destBlendAlpha = gldsta;
		}
		if (glfunc != STATE.blendOp || glfunca != STATE.blendOpAlpha)
		{
			bool supports = true;
			if( (glfunc == GL_MIN_EXT || glfunc == GL_MAX_EXT) && !gGraphicsCaps.hasBlendMinMax )
				supports = false;
			if( (glfunca == GL_MIN_EXT || glfunca == GL_MAX_EXT) && !gGraphicsCaps.hasBlendMinMax )
				supports = false;

			if(supports)
			{
				GLES_CHK(glBlendEquationSeparate(glfunc, glfunca));
				STATE.blendOp = glfunc;
				STATE.blendOpAlpha = glfunca;
			}
		}
		if( STATE.blending != 1 )
		{
			GLES_CHK(glEnable( GL_BLEND ));
			STATE.blending = 1;
		}
	}
	// fragment shader is expected to implement per fragment culling
	CompareFunction alphaTest = devstate->sourceState.alphaTest;

	if (gGraphicsCaps.gles20.slowAlphaTest)
	{
		// Alpha testing is slow on some GPUs
		// skip trivial cases such as (alpha > 0)
		if (alphaTest == kFuncGreater && alphaRef <= 0.01)
			alphaTest = kFuncDisabled;
	}

	if( gGraphicsCaps.gles20.hasAlphaTestQCOM && (alphaTest != STATE.alphaTest || alphaRef != STATE.alphaValue) )
	{
		if( alphaTest != kFuncDisabled )
		{
			GLES_CHK(gGlesExtFunc.glAlphaFuncQCOM(kCmpFuncES2[alphaTest], alphaRef));
			GLES_CHK(glEnable(GL_ALPHA_TEST_QCOM));
		}
		else
		{
			GLES_CHK(glDisable(GL_ALPHA_TEST_QCOM));
		}
	}

	STATE.alphaTest = alphaTest;
	STATE.alphaValue = alphaRef;
}


void GFX_GL_IMPL::SetRasterState(const DeviceRasterState* state)
{
	DeviceRasterState* devstate = (DeviceRasterState*)state;
	if(!devstate)
	{
		STATE.m_CurrRasterState = NULL;
		return;
	}

	STATE.m_CurrRasterState = devstate;

	CullMode cull = devstate->sourceState.cullMode;
	if( cull != STATE.culling )
	{
		switch (cull) {
		case kCullOff:
			GLES_CHK(glDisable (GL_CULL_FACE));
			break;
		case kCullFront:
			GLES_CHK(glCullFace (GL_FRONT));
			GLES_CHK(glEnable (GL_CULL_FACE));
			break;
		case kCullBack:
			GLES_CHK(glCullFace (GL_BACK));
			GLES_CHK(glEnable (GL_CULL_FACE));
			break;
		default:
			break;
		}
		STATE.culling = cull;
	}

    float zFactor = devstate->sourceState.slopeScaledDepthBias;
    float zUnits  = devstate->sourceState.depthBias;
    if( zFactor != STATE.offsetFactor || zUnits != STATE.offsetUnits )
    {

#if WORKAROUND_POLYGON_OFFSET
        // on some androids polygon offset work the other way (positive push to viewer)
        // so we tweak projection matrix to do offset
        // also use available depth precision better (on Adreno for example fix bugs with z-fighting)
        // Game Programming Gems Vol1
        // Eric Lengyel's "Tweaking a Vertex's Projected Depth Value"
        // we use simplified formula: just smallest possible eps directly (multiplied by zUnits)
        // we calculate eps for 16bit depth (good enough for larger depth)
        // in projection matrix [2,2] = (f+n)/(n-f)
        // so eps would be BitsMult * -1/proj[2,2]

        static const float _BitsMult = -1.0f / (float)0xFFFF; // FFFF = 2^16-1, minus sign incorporated here

        float* matrixElem = &m_BuiltinParamValues.GetWritableMatrixParam(kShaderMatProj).Get(2,2);

        const float eps = _BitsMult / *matrixElem;
        *matrixElem *= (1.0f + zUnits*eps);
#else
        GLES_CHK(glPolygonOffset( zFactor, zUnits ));
        if( zFactor || zUnits )
            GLES_CHK(glEnable (GL_POLYGON_OFFSET_FILL));
        else
            GLES_CHK(glDisable (GL_POLYGON_OFFSET_FILL));
#endif
		STATE.offsetFactor = zFactor;
		STATE.offsetUnits = zUnits;
	}
}


void GFX_GL_IMPL::SetDepthState(const DeviceDepthState* state)
{
	DeviceDepthStateGLES20* devstate = (DeviceDepthStateGLES20*)state;
	if (STATE.m_CurrDepthState == devstate)
		return;

	STATE.m_CurrDepthState = devstate;

	if (!STATE.m_CurrDepthState)
		return;

	const int depthFunc = devstate->depthFunc;
	if( depthFunc != STATE.depthFunc )
	{
		if( depthFunc != GL_NEVER ) {
			GLES_CHK(glDepthFunc (depthFunc));
			GLES_CHK(glEnable (GL_DEPTH_TEST));
		} else {
			GLES_CHK(glDisable (GL_DEPTH_TEST));
		}

		STATE.depthFunc = depthFunc;
	}

	const int writeMode = devstate->sourceState.depthWrite ? GL_TRUE : GL_FALSE;
	if( writeMode != STATE.depthWrite )
	{
		GLES_CHK(glDepthMask (writeMode));
		STATE.depthWrite = writeMode;
	}
}

void GFX_GL_IMPL::SetStencilState (const DeviceStencilState* state, int stencilRef)
{
	if (STATE.m_CurrStencilState == state && STATE.m_StencilRef == stencilRef)
		return;
	const DeviceStencilStateGLES20* st = static_cast<const DeviceStencilStateGLES20*>(state);
	STATE.m_CurrStencilState = st;
	if (!st)
		return;

	if (st->sourceState.stencilEnable)
		GLES_CHK(glEnable (GL_STENCIL_TEST));
	else
		GLES_CHK(glDisable (GL_STENCIL_TEST));
	if (gGraphicsCaps.hasTwoSidedStencil)
	{
		GLES_CHK(glStencilFuncSeparate (GL_FRONT, st->stencilFuncFront, stencilRef, st->sourceState.readMask));
		GLES_CHK(glStencilOpSeparate (GL_FRONT, st->stencilFailOpFront, st->depthFailOpFront, st->depthPassOpFront));
		GLES_CHK(glStencilFuncSeparate (GL_BACK, st->stencilFuncBack, stencilRef, st->sourceState.readMask));
		GLES_CHK(glStencilOpSeparate (GL_BACK, st->stencilFailOpBack, st->depthFailOpBack, st->depthPassOpBack));
	}
	else
	{
		GLES_CHK(glStencilFunc (st->stencilFuncFront, stencilRef, st->sourceState.readMask));
		GLES_CHK(glStencilOp (st->stencilFailOpFront, st->depthFailOpFront, st->depthPassOpFront));
	}
	GLES_CHK(glStencilMask (st->sourceState.writeMask));

	STATE.m_StencilRef = stencilRef;
}

void GFX_GL_IMPL::SetSRGBWrite (bool enable)
{
}

bool GFX_GL_IMPL::GetSRGBWrite ()
{
	return false;
}

void GFX_GL_IMPL::Clear (UInt32 clearFlags, const float color[4], float depth, int stencil)
{
	DBG_LOG_GLES20("Clear(%d, (%.2f, %.2f, %.2f, %.2f), %.2f, %d", clearFlags, color[0], color[1], color[2], color[3], depth, stencil);

	EnsureDefaultFBInitedGLES2();

	if (!IsActiveRenderTargetWithColorGLES2())
		clearFlags &= ~kGfxClearColor;

	float clearColorAlpha = color[3];

	// In OpenGL, clears are affected by color write mask and depth writing parameters.
	// So make sure to set them!
	GLbitfield flags = 0;
	if (clearFlags & kGfxClearColor)
	{
		if (STATE.colorWriteMask != 15)
		{
			GLES_CHK(glColorMask( true, true, true, true ));
			STATE.colorWriteMask = 15;
			STATE.m_CurrBlendState = NULL;
		}
		flags |= GL_COLOR_BUFFER_BIT;
		GLES_CHK(glClearColor( color[0], color[1], color[2], clearColorAlpha ));
	}
	if (clearFlags & kGfxClearDepth)
	{
		GLES_CHK(glDepthMask (GL_TRUE));
		STATE.depthWrite = GL_TRUE;
		STATE.m_CurrDepthState = NULL;
		flags |= GL_DEPTH_BUFFER_BIT;
		GLES_CHK(glClearDepthf( depth ));
	}
	if (clearFlags & kGfxClearStencil)
	{
		//@TODO: need to set stencil writes on?
		flags |= GL_STENCIL_BUFFER_BIT;
		GLES_CHK(glClearStencil (stencil));
	}
	GLES_CHK(glClear(flags));
}

static void ApplyBackfaceMode( const DeviceStateGLES20& state )
{
	DBG_LOG_GLES20("ApplyBackfaceMode");
	if (state.appBackfaceMode != state.userBackfaceMode)
		GLES_CHK(glFrontFace( GL_CCW ));
	else
		GLES_CHK(glFrontFace( GL_CW ));
}


void GFX_GL_IMPL::SetUserBackfaceMode( bool enable )
{
	DBG_LOG_GLES20("SetUserBackfaceMode(%s)", GetBoolString(enable));
	if( STATE.userBackfaceMode == enable )
		return;

	STATE.userBackfaceMode = enable;
	ApplyBackfaceMode( STATE );
}

void GFX_GL_IMPL::SetInvertProjectionMatrix( bool enable )
{
	Assert (!enable); // projection should never be flipped upside down on OpenGL
}

bool GFX_GL_IMPL::GetInvertProjectionMatrix() const
{
	return false;
}

void GFX_GL_IMPL::SetProjectionMatrix (const Matrix4x4f& matrix)
{
	DBG_LOG_GLES20("SetProjectionMatrix(...)");

	CopyMatrix(matrix.GetPtr(), STATE.transformState.projectionMatrixOriginal.GetPtr());
	CopyMatrix (matrix.GetPtr(), m_BuiltinParamValues.GetWritableMatrixParam(kShaderMatProj).GetPtr());
	STATE.transformState.dirtyFlags |= TransformState::kProjDirty;
}


void GFX_GL_IMPL::SetWorldMatrix( const float matrix[16] )
{
	CopyMatrix( matrix, STATE.transformState.worldMatrix.GetPtr() );
	STATE.transformState.dirtyFlags |= TransformState::kWorldDirty;
}

void GFX_GL_IMPL::SetViewMatrix( const float matrix[16] )
{
	STATE.transformState.SetViewMatrix (matrix, m_BuiltinParamValues);
}


void GFX_GL_IMPL::GetMatrix( float outMatrix[16] ) const
{
	STATE.transformState.UpdateWorldViewMatrix (m_BuiltinParamValues);
	CopyMatrix (STATE.transformState.worldViewMatrix.GetPtr(), outMatrix);
}

const float* GFX_GL_IMPL::GetWorldMatrix() const
{
	return STATE.transformState.worldMatrix.GetPtr();
}

const float* GFX_GL_IMPL::GetViewMatrix() const
{
	return m_BuiltinParamValues.GetMatrixParam(kShaderMatView).GetPtr();
}

const float* GFX_GL_IMPL::GetProjectionMatrix() const
{
	return STATE.transformState.projectionMatrixOriginal.GetPtr();
}

const float* GFX_GL_IMPL::GetDeviceProjectionMatrix() const
{
	return GetProjectionMatrix();
}

void GFX_GL_IMPL::SetNormalizationBackface( NormalizationMode mode, bool backface )
{
	DBG_LOG_GLES20("SetNormalizationBackface(%d  %s)", mode, backface?"back":"front");
	STATE.normalization = mode;
	if( STATE.appBackfaceMode != backface )
	{
		STATE.appBackfaceMode = backface;
		ApplyBackfaceMode( STATE );
	}
}

void GFX_GL_IMPL::SetFFLighting( bool on, bool separateSpecular, ColorMaterialMode colorMaterial )
{
	DBG_LOG_GLES20("SetFFLighting(%s, %s, %d)", on?"True":"False", separateSpecular?"True":"False", colorMaterial);
	STATE.lighting = on;
	STATE.separateSpecular = separateSpecular;
	STATE.colorMaterial = colorMaterial;
}

void GFX_GL_IMPL::SetMaterial( const float ambient[4], const float diffuse[4], const float specular[4], const float emissive[4], const float shininess )
{
	DBG_LOG_GLES20("SetMaterial()");
	STATE.matAmbient.set (ambient[0], ambient[1], ambient[2], 1.0F);
	STATE.matDiffuse.set (diffuse);
	STATE.matSpecular.set (specular[0], specular[1], specular[2], 1.0F);
	STATE.matEmissive.set (emissive[0], emissive[1], emissive[2], 1.0F);
	float glshine = std::max<float>(std::min<float>(shininess,1.0f), 0.0f) * 128.0f;
	STATE.matShininess = glshine;
}


void GFX_GL_IMPL::SetColor( const float color[4] )
{
	DBG_LOG_GLES20("SetColor()");
	STATE.color.set( color );
	// Emulate OpenGL's behaviour
	ImmediateColor( color[0], color[1], color[2], color[3] );
}

void GFX_GL_IMPL::SetViewport( int x, int y, int width, int height )
{
	DBG_LOG_GLES20("SetViewport(%d, %d, %d, %d)", x, y, width, height);
	STATE.viewport[0] = x;
	STATE.viewport[1] = y;
	STATE.viewport[2] = width;
	STATE.viewport[3] = height;
	GLES_CHK(glViewport( x, y, width, height ));
}

void GFX_GL_IMPL::GetViewport( int* port ) const
{
	DBG_LOG_GLES20("GetViewport()");
	port[0] = STATE.viewport[0];
	port[1] = STATE.viewport[1];
	port[2] = STATE.viewport[2];
	port[3] = STATE.viewport[3];
}



void GFX_GL_IMPL::SetScissorRect( int x, int y, int width, int height )
{
	DBG_LOG_GLES20("SetScissorRect(%d, %d, %d, %d)", x, y, width, height);
	if (STATE.scissor != 1)
	{
		GLES_CHK(glEnable( GL_SCISSOR_TEST ));
		STATE.scissor = 1;
	}


	STATE.scissorRect[0] = x;
	STATE.scissorRect[1] = y;
	STATE.scissorRect[2] = width;
	STATE.scissorRect[3] = height;
	GLES_CHK(glScissor( x, y, width, height ));

}

bool GFX_GL_IMPL::IsCombineModeSupported( unsigned int combiner )
{
	return true;
}

void GFX_GL_IMPL::DisableScissor()
{
	DBG_LOG_GLES20("DisableScissor()");
	if (STATE.scissor != 0)
	{
		GLES_CHK(glDisable( GL_SCISSOR_TEST ));
		STATE.scissor = 0;
	}
}

bool GFX_GL_IMPL::IsScissorEnabled() const
{
	DBG_LOG_GLES20("IsScissorEnabled():returns %s", STATE.scissor == 1?"True":"False");
	return STATE.scissor == 1;
}

void GFX_GL_IMPL::GetScissorRect( int scissor[4] ) const
{
	DBG_LOG_GLES20("GetScissorRect()");
	scissor[0] = STATE.scissorRect[0];
	scissor[1] = STATE.scissorRect[1];
	scissor[2] = STATE.scissorRect[2];
	scissor[3] = STATE.scissorRect[3];
}

TextureCombinersHandle GFX_GL_IMPL::CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular )
{
	DBG_LOG_GLES20("CreateTextureCombiners()");
	TextureCombinersGLES2* implGLES = TextureCombinersGLES2::Create (count, texEnvs);
	return TextureCombinersHandle (implGLES);
}


void GFX_GL_IMPL::SetTextureCombinersThreadable( TextureCombinersHandle textureCombiners, const TexEnvData* texEnvData, const Vector4f* texColors )
{
	DBG_LOG_GLES20("SetTextureCombiners()");
	TextureCombinersGLES2* implGLES = OBJECT_FROM_HANDLE(textureCombiners,TextureCombinersGLES2);
	Assert (implGLES);

	const int count = std::min(gGraphicsCaps.maxTexUnits, implGLES->count);
	STATE.textureCount = count;
	for (int i = 0; i < count; ++i)
	{
		const ShaderLab::TextureBinding& binding = implGLES->texEnvs[i];

		// set the texture
		ApplyTexEnvData (i, i, texEnvData[i]);

		// setup texture unit state
		TextureUnitStateGLES2& texUnitState = STATE.textures[i];
		texUnitState.color = texColors[i];
		texUnitState.combColor = binding.m_CombColor;
		texUnitState.combAlpha = binding.m_CombAlpha;
	}
	// we can just create mask and "and" with it
	// but consider me lazy
	for( int i = count ; i < gGraphicsCaps.maxTexUnits ; ++i)
		STATE.DropTexGen(i);

	STATE.activeProgram = 0;

	// Get us back to TU 0, so we know where we stand
	ActivateTextureUnitGLES2 (STATE, 0);
}

void GFX_GL_IMPL::DeleteTextureCombiners( TextureCombinersHandle& textureCombiners )
{
	DBG_LOG_GLES20("DeleteTextureCombiners()");
	TextureCombinersGLES2* implGLES = OBJECT_FROM_HANDLE(textureCombiners, TextureCombinersGLES2);
	delete implGLES;
	textureCombiners.Reset();
}

void GFX_GL_IMPL::SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props )
{
	DBG_LOG_GLES20("SetTextureCombiners()");
	TextureCombinersGLES2* implGLES = OBJECT_FROM_HANDLE(textureCombiners,TextureCombinersGLES2);
	Assert (implGLES);

	const int count = std::min(gGraphicsCaps.maxTexUnits, implGLES->count);
	// Fill in arrays
	TexEnvData* texEnvData;
	ALLOC_TEMP (texEnvData, TexEnvData, count);
	for( int i = 0; i < count; ++i )
	{
		ShaderLab::TexEnv *te = ShaderLab::GetTexEnvForBinding( implGLES->texEnvs[i], props );
		Assert( te != NULL );
		te->PrepareData (implGLES->texEnvs[i].m_TextureName.index, implGLES->texEnvs[i].m_MatrixName, props, &texEnvData[i]);
	}

	Vector4f* texColors;
	ALLOC_TEMP (texColors, Vector4f, implGLES->count);
	for( int i = 0; i < implGLES->count; ++i )
	{
		const ShaderLab::TextureBinding& binding = implGLES->texEnvs[i];
		texColors[i] = binding.GetTexColor().Get (props);
	}
	GFX_GL_IMPL::SetTextureCombinersThreadable(textureCombiners, texEnvData, texColors);
}

void GFX_GL_IMPL::SetTexture (ShaderType shaderType, int unit, int samplerUnit, TextureID texture, TextureDimension dim, float bias)
{
	DBG_LOG_GLES20("SetTexture(%d %d)", unit, texture.m_ID);
	DebugAssertIf( unit < 0 || unit >= gGraphicsCaps.maxTexUnits );

	GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(texture);
	if(targetTex == 0)
		return;

	TextureUnitStateGLES2& currTex = STATE.textures[unit];
	if (STATE.textureCount > unit && targetTex == currTex.texID)
	{
		return;
	}
	ActivateTextureUnitGLES2 (STATE, unit);

	switch (dim)
	{
	case kTexDim2D: GLES_CHK(glBindTexture(GL_TEXTURE_2D, targetTex)); break;
	case kTexDimCUBE: GLES_CHK(glBindTexture(GL_TEXTURE_CUBE_MAP, targetTex)); break;
	default: break;
	}

	if (STATE.textureCount <= unit)
		STATE.textureCount = unit+1;
	currTex.texID = targetTex;
	currTex.texDim = dim;
	if (currTex.texGen == kTexGenUnknown)
		currTex.texGen = kTexGenDisabled;

	STATE.ApplyTexGen(unit);

	GLESAssert();

	#if defined(GL_EXT_texture_lod_bias) && !UNITY_LINUX
	if (gGraphicsCaps.hasMipLevelBias && unitState.bias != bias && bias != std::numeric_limits<float>::infinity())
	{
		GL_CHK(glTexEnvf( GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, bias ));
		unitState.bias = bias;
	}
	#endif
}

void GFX_GL_IMPL::SetTextureTransform( int unit, TextureDimension dim, TexGenMode texGen, bool identity, const float matrix[16])
{
	DBG_LOG_GLES20("SetTextureTransform()");
	DebugAssertIf( unit < 0 || unit >= gGraphicsCaps.maxTexUnits );
	TextureUnitStateGLES2& unitState = STATE.textures[unit];

	unitState.SetTexGen(texGen);
	unitState.textureMatrix = *(Matrix4x4f const*)matrix;

	// Since we will set texture matrix even if TexGen is disabled (since matrix can contain scale/offset information)
	// we set textureMatrix to identity to be on a safe side
	if (identity)
		unitState.textureMatrix.SetIdentity();
	unitState.identityMatrix = identity;

	unitState.isProjected = false;
	if (!identity && dim==kTexDim2D)
		unitState.isProjected = (matrix[3] != 0.0f || matrix[7] != 0.0f || matrix[11] != 0.0f || matrix[15] != 1.0f);

	STATE.ApplyTexGen(unit);
}

static const unsigned long kGLES20TextureDimensionTable[kTexDimCount] = {0, -1/*GL_TEXTURE_1D*/, GL_TEXTURE_2D, -1/*GL_TEXTURE_3D*/, GL_TEXTURE_CUBE_MAP, 0};

void GFX_GL_IMPL::SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace )
{
	DBG_LOG_GLES20("SetTextureParams()");

	TextureIdMapGLES20_QueryOrCreate(texture);

	GLuint target = kGLES20TextureDimensionTable[texDim];
	SetTexture (kShaderFragment, 0, 0, texture, texDim, std::numeric_limits<float>::infinity());

	// Anisotropic texturing...
	if( gGraphicsCaps.hasAnisoFilter )
	{
		anisoLevel = std::min( anisoLevel, gGraphicsCaps.maxAnisoLevel );
		GLES_CHK(glTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisoLevel ));
	}

	GLenum wrapMode = kWrapModeES2[wrap];
	GLES_CHK(glTexParameteri( target, GL_TEXTURE_WRAP_S, wrapMode ));
	GLES_CHK(glTexParameteri( target, GL_TEXTURE_WRAP_T, wrapMode ));

	if( !hasMipMap && filter == kTexFilterTrilinear )
		filter = kTexFilterBilinear;

	GLES_CHK(glTexParameteri( target, GL_TEXTURE_MAG_FILTER, filter != kTexFilterNearest ? GL_LINEAR : GL_NEAREST ));
	if( hasMipMap )
		GLES_CHK(glTexParameteri( target, GL_TEXTURE_MIN_FILTER, kMinFilterES2[filter] ));
	else
		GLES_CHK(glTexParameteri (target, GL_TEXTURE_MIN_FILTER, filter != kTexFilterNearest ? GL_LINEAR : GL_NEAREST));

	GLESAssert();
}

void GFX_GL_IMPL::SetShadersThreadable (GpuProgram* programs[kShaderTypeCount], const GpuProgramParameters* params[kShaderTypeCount], UInt8 const * const paramsBuffer[kShaderTypeCount])
{
	DBG_LOG_GLES20("SetShadersThreadable()");
	GpuProgram* vertexProgram = programs[kShaderVertex];
	GpuProgram* fragmentProgram = programs[kShaderFragment];

	DBG_LOG_GLES20("SetShaders(%s, %s)",
		GetShaderImplTypeString(vertexProgram? vertexProgram->GetImplType():kShaderImplUndefined),
		GetShaderImplTypeString(fragmentProgram? fragmentProgram->GetImplType():kShaderImplUndefined));

	// GLSL is only supported like this:
	// vertex shader actually is both vertex & fragment linked program
	// fragment shader is unused
	if (vertexProgram && vertexProgram->GetImplType() == kShaderImplBoth)
	{
		Assert(fragmentProgram == 0 || fragmentProgram->GetImplType() == kShaderImplBoth);
		STATE.activeProgram = vertexProgram;
		STATE.activeProgramParams = params[kShaderVertex];
		DebugAssert(STATE.activeProgramParams->IsReady());
		int bufferSize = STATE.activeProgramParams->GetValuesSize();
		STATE.activeProgramParamsBuffer.resize_uninitialized(bufferSize);
		memcpy(STATE.activeProgramParamsBuffer.data(), paramsBuffer[kShaderVertex], bufferSize);
	}
	else
	{
		Assert(vertexProgram == 0);
		STATE.activeProgram = 0;
		STATE.activeProgramParams = 0;
		STATE.activeProgramParamsBuffer.resize_uninitialized(0);
	}
}

void GFX_GL_IMPL::CreateShaderParameters( ShaderLab::SubProgram* program, FogMode fogMode )
{
       GlslGpuProgramGLES20& programGLES = static_cast<GlslGpuProgramGLES20&>(program->GetGpuProgram());
	   programGLES.GetGLProgram(fogMode, program->GetParams(fogMode), program->GetChannels());
}

bool GFX_GL_IMPL::IsShaderActive( ShaderType type ) const
{
	//DBG_LOG_GLES20("IsShaderActive(%s): returns %s", GetShaderTypeString(type), STATE.shaderEnabledImpl[type] != kShaderImplUndefined?"True":"False");
	//return STATE.shaderEnabledImpl[type] != kShaderImplUndefined;
	DBG_LOG_GLES20("IsShaderActive(%s):", GetShaderTypeString(type));
	return (STATE.activeProgram != 0);
}

void GFX_GL_IMPL::DestroySubProgram( ShaderLab::SubProgram* subprogram )
{
	//@TODO
	//if (STATE.activeProgram == program)
	//{
	//  STATE.activeProgram = NULL;
	//  STATE.activeProgramProps = NULL;
	//}
	delete subprogram;
}



void GFX_GL_IMPL::DisableLights( int startLight )
{
	DBG_LOG_GLES20("DisableLights(%d)", startLight);
	startLight = std::min (startLight, gGraphicsCaps.maxLights);
	STATE.vertexLightCount = startLight;
	for (int i = startLight; i < kMaxSupportedVertexLightsByGLES20; ++i)
	{
		m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Position + i), Vector4f(0.0f, 0.0f, 1.0f, 0.0f));
		m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Diffuse + i), Vector4f(0.0f, 0.0f, 0.0f, 0.0f));
	}
}


void GFX_GL_IMPL::SetLight( int light, const GfxVertexLight& data)
{
	DBG_LOG_GLES20("SetLight(%d), [{%f, %f, %f, %f}, {%f, %f, %f, %f}, {%f, %f, %f, %f}, %f, %f, %f, %d]",
			 light,
			 data.position[0],  data.position[1],   data.position[2],   data.position[3],
			 data.spotDirection[0], data.spotDirection[1],  data.spotDirection[2],  data.spotDirection[3],
			 data.color[0], data.color[1],  data.color[2],  data.color[3],
			 data.range, data.quadAtten, data.spotAngle, data.type);
	Assert( light >= 0 && light < kMaxSupportedVertexLights );

	if (light >= kMaxSupportedVertexLightsByGLES20)
		return;

	STATE.vertexLightTypes[light] = data.type;

	SetupVertexLightParams (light, data);
}

void GFX_GL_IMPL::SetAmbient( const float ambient[4] )
{
	DBG_LOG_GLES20("SetAmbient()");
	STATE.ambient.set (ambient[0], ambient[1], ambient[2], ambient[3]);
	m_BuiltinParamValues.SetVectorParam(kShaderVecLightModelAmbient, Vector4f(ambient));
}

void GFX_GL_IMPL::EnableFog (const GfxFogParams& fog)
{
	DBG_LOG_GLES20("EnableFog()");
	DebugAssertIf( fog.mode <= kFogDisabled );
	m_FogParams = fog;
}

void GFX_GL_IMPL::DisableFog()
{
	DBG_LOG_GLES20("DisableFog()");

	if( m_FogParams.mode != kFogDisabled )
	{
		m_FogParams.mode = kFogDisabled;
		m_FogParams.density = 0.0f;
	}
}

VBO* GFX_GL_IMPL::CreateVBO()
{
	VBO* vbo = new GLES2VBO();
	OnCreateVBO(vbo);
	return vbo;
}

void GFX_GL_IMPL::DeleteVBO( VBO* vbo )
{
	OnDeleteVBO(vbo);
	delete vbo;
}

DynamicVBO& GFX_GL_IMPL::GetDynamicVBO()
{
	if( !STATE.m_DynamicVBO ) {
		STATE.m_DynamicVBO = new DynamicGLES2VBO();
	}
	return *STATE.m_DynamicVBO;
}

// ---------- render textures

static GLuint GetFBO(GLuint* fbo)
{
	if (!(*fbo) && gGraphicsCaps.hasRenderToTexture)
		glGenFramebuffers(1, fbo);

	return *fbo;
}
static GLuint GetSharedFBO (DeviceStateGLES20& state)   { return GetFBO (&state.m_SharedFBO); }
static GLuint GetHelperFBO (DeviceStateGLES20& state)   { return GetFBO (&state.m_HelperFBO); }

RenderSurfaceHandle GFX_GL_IMPL::CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags)
{
	RenderSurfaceHandle rs = CreateRenderColorSurfaceGLES2 (textureID, 0, width, height, dim, createFlags, format, samples);
#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
#endif
	return rs;
}
RenderSurfaceHandle GFX_GL_IMPL::CreateRenderDepthSurface (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags)
{
	RenderSurfaceHandle rs = CreateRenderDepthSurfaceGLES2 (textureID, 0, width, height, createFlags, depthFormat, samples);
#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
#endif
	return rs;
}
void GFX_GL_IMPL::DestroyRenderSurface (RenderSurfaceHandle& rs)
{
	// Early out if render surface is not created (don't do invalidate/flush in that case)
	if( !rs.IsValid() )
		return;

	DestroyRenderSurfaceGLES2 (rs);

	InvalidateState();
#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
#endif
}
void GFX_GL_IMPL::SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face)
{
#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
#endif
	if (SetRenderTargetGLES2 (count, colorHandles, depthHandle, mipLevel, face, GetSharedFBO(STATE)))
	{
		// changing render target might mean different color clear flags; so reset current state
		STATE.m_CurrBlendState = NULL;
	}
#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
#endif
}

void GFX_GL_IMPL::ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle)
{
	ResolveColorSurfaceGLES2(srcHandle, dstHandle, GetSharedFBO(STATE), GetHelperFBO(STATE));
}

RenderSurfaceHandle GFX_GL_IMPL::GetActiveRenderColorSurface(int index)
{
	Assert (index == 0);
	return GetActiveRenderColorSurfaceGLES2();
}
RenderSurfaceHandle GFX_GL_IMPL::GetActiveRenderDepthSurface()
{
	return GetActiveRenderDepthSurfaceGLES2();
}
void GFX_GL_IMPL::SetSurfaceFlags (RenderSurfaceHandle surf, UInt32 flags, UInt32 keepFlags)
{
}
void GFX_GL_IMPL::DiscardContents (RenderSurfaceHandle& rs)
{
	DiscardContentsGLES2(rs);
}


// ---------- uploading textures

void GFX_GL_IMPL::UploadTexture2D( TextureID texture, TextureDimension dimension, UInt8* srcData, int srcSize, int width, int height, TextureFormat format, int mipCount, UInt32 uploadFlags, int skipMipLevels, TextureUsageMode usageMode, TextureColorSpace colorSpace )
{
	::UploadTexture2DGLES2( texture, dimension, srcData, width, height, format, mipCount, uploadFlags, skipMipLevels, colorSpace );
}

void GFX_GL_IMPL::UploadTextureSubData2D( TextureID texture, UInt8* srcData, int srcSize, int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace )
{
	::UploadTextureSubData2DGLES2( texture, srcData, mipLevel, x, y, width, height, format, colorSpace );
}

void GFX_GL_IMPL::UploadTextureCube( TextureID texture, UInt8* srcData, int srcSize, int faceDataSize, int size, TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace )
{
	::UploadTextureCubeGLES2( texture, srcData, faceDataSize, size, format, mipCount, uploadFlags, colorSpace );
}

void GFX_GL_IMPL::UploadTexture3D( TextureID texture, UInt8* srcData, int srcSize, int width, int height, int depth, TextureFormat format, int mipCount, UInt32 uploadFlags )
{
	ErrorString("3D textures are not supported by OpenGLES!");
}

void GFX_GL_IMPL::DeleteTexture(TextureID texture)
{
	GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(texture);
	if(targetTex == 0)
		return;

	REGISTER_EXTERNAL_GFX_DEALLOCATION(texture.m_ID);
	GLES_CHK(glDeleteTextures(1, &targetTex));

	// invalidate texture unit states that used this texture
	for( int i = 0; i < gGraphicsCaps.maxTexUnits; ++i )
	{
		TextureUnitStateGLES2& currTex = STATE.textures[i];
		if( currTex.texID == targetTex )
			currTex.Invalidate();
	}

	TextureIdMap::RemoveTexture(texture);
}

// ---------- context

GfxDevice::PresentMode GFX_GL_IMPL::GetPresentMode()
{
	return UNITY_IPHONE ? kPresentAfterDraw : kPresentBeforeUpdate;
}
void GFX_GL_IMPL::BeginFrame()
{
	DBG_LOG_GLES20("BeginFrame()");
	m_InsideFrame = true;

	if(gGraphicsCaps.hasTiledGPU)
	{
		SetBackBufferGLES2();

		extern void ClearCurrentFBImpl(bool clearColor, bool clearDepth);
		ClearCurrentFBImpl(true,true);
	}
}
void GFX_GL_IMPL::EndFrame()
{
	// on ios we do it in trampoline
	// also we handle BackBuffer MSAA and Render Resolution ourselves, so discard is a bit more complicated
#if !UNITY_IPHONE
	if(gGraphicsCaps.gles20.hasDiscardFramebuffer)
	{
		SetBackBufferGLES2();

		extern void DiscardCurrentFBImpl(bool discardColor, bool discardDepth, GLenum target);
		DiscardCurrentFBImpl(false, true, GL_FRAMEBUFFER);
	}
#endif


	DBG_LOG_GLES20("EndFrame()");
	m_InsideFrame = false;
}

bool GFX_GL_IMPL::IsValidState()
{
#if UNITY_ANDROID
	return ContextGLES::IsValid();
#else
	return true;
#endif
}

bool GFX_GL_IMPL::HandleInvalidState()
{
#if UNITY_ANDROID
	bool needReload;
	if (!ContextGLES::HandleInvalidState(&needReload))
		return false;
	if (needReload)
		ReloadResources();
	return true;
#else
	return true;
#endif
}

void GFX_GL_IMPL::PresentFrame()
{
	DBG_LOG_GLES20("====================================");
	DBG_LOG_GLES20("====================================");
	DBG_LOG_GLES20("PresentFrame");
	DBG_LOG_GLES20("====================================");
	DBG_LOG_GLES20("====================================");

#if UNITY_WIN
	PresentContextGLES20();
#else
	PresentContextGLES();
#endif
}

void GFX_GL_IMPL::FinishRendering()
{
	GLES_CHK(glFinish());
}


// ---------- immediate mode rendering

// we break very large immediate mode submissions into multiple batches internally
const int kMaxImmediateVerticesPerDraw = 2048;


ImmediateModeGLES20::ImmediateModeGLES20()
:   m_Mode(kPrimitiveTriangles)
,   m_QuadsIB(0)
,   m_IndexBufferQuadsID(0)
{
	m_QuadsIB = (UInt16*)UNITY_MALLOC(kMemGeometry, kMaxImmediateVerticesPerDraw*6*sizeof(UInt16));
	UInt32 baseIndex = 0;
	UInt16* ibPtr = m_QuadsIB;
	for( int i = 0; i < kMaxImmediateVerticesPerDraw; ++i )
	{
		ibPtr[0] = baseIndex + 1;
		ibPtr[1] = baseIndex + 2;
		ibPtr[2] = baseIndex;
		ibPtr[3] = baseIndex + 2;
		ibPtr[4] = baseIndex + 3;
		ibPtr[5] = baseIndex;
		baseIndex += 4;
		ibPtr += 6;
	}
}

ImmediateModeGLES20::~ImmediateModeGLES20()
{
	Invalidate();
	UNITY_FREE(kMemGeometry,m_QuadsIB);
}

void ImmediateModeGLES20::Invalidate()
{
	if( m_IndexBufferQuadsID )
	{
		glDeregisterBufferData(1, (GLuint*)&m_IndexBufferQuadsID);
		GLES_CHK(glDeleteBuffers(1, (GLuint*)&m_IndexBufferQuadsID));
		m_IndexBufferQuadsID = 0;
	}
	m_Vertices.clear();
	memset( &m_Current, 0, sizeof(m_Current) );
}

void GFX_GL_IMPL::ImmediateVertex( float x, float y, float z )
{
	// If the current batch is becoming too large, internally end it and begin it again.
	size_t currentSize = STATE.m_Imm.m_Vertices.size();
	if( currentSize >= kMaxImmediateVerticesPerDraw - 4 )
	{
		GfxPrimitiveType mode = STATE.m_Imm.m_Mode;
		// For triangles, break batch when multiple of 3's is reached.
		if( mode == kPrimitiveTriangles && currentSize % 3 == 0 )
		{
			ImmediateEnd();
			ImmediateBegin( mode );
		}
		// For other primitives, break on multiple of 4's.
		// NOTE: This won't quite work for triangle strips, but we'll just pretend
		// that will never happen.
		else if( mode != kPrimitiveTriangles && currentSize % 4 == 0 )
		{
			ImmediateEnd();
			ImmediateBegin( mode );
		}
	}
	Vector3f& vert = STATE.m_Imm.m_Current.vertex;
	vert.x = x;
	vert.y = y;
	vert.z = z;
	STATE.m_Imm.m_Vertices.push_back( STATE.m_Imm.m_Current );
}

void GFX_GL_IMPL::ImmediateNormal( float x, float y, float z )
{
	STATE.m_Imm.m_Current.normal.x = x;
	STATE.m_Imm.m_Current.normal.y = y;
	STATE.m_Imm.m_Current.normal.z = z;
}

void GFX_GL_IMPL::ImmediateColor( float r, float g, float b, float a )
{
	r = clamp01(r);
	g = clamp01(g);
	b = clamp01(b);
	a = clamp01(a);

	STATE.m_Imm.m_Current.color =
		((UInt32)(a * 255.0f) << 24) |
		((UInt32)(b * 255.0f) << 16) |
		((UInt32)(g * 255.0f) << 8) |
		((UInt32)(r * 255.0f));
}

void GFX_GL_IMPL::ImmediateTexCoordAll( float x, float y, float z )
{
	for( int i = 0; i < gGraphicsCaps.maxTexCoords; ++i )
	{
		Vector3f& uv = STATE.m_Imm.m_Current.texCoords[i];
		uv.x = x;
		uv.y = y;
		uv.z = z;
	}
}

void GFX_GL_IMPL::ImmediateTexCoord( int unit, float x, float y, float z )
{
	if( unit < 0 || unit >= 8 )
	{
		ErrorString( "Invalid unit for texcoord" );
		return;
	}
	Vector3f& uv = STATE.m_Imm.m_Current.texCoords[unit];
	uv.x = x;
	uv.y = y;
	uv.z = z;
}

void GFX_GL_IMPL::ImmediateBegin( GfxPrimitiveType type )
{
	STATE.m_Imm.m_Mode = type;
	STATE.m_Imm.m_Vertices.clear();
}

void GFX_GL_IMPL::ImmediateEnd()
{
	int vertexCount = STATE.m_Imm.m_Vertices.size();
	if( vertexCount == 0 )
		return;

	InvalidateVertexInputCacheGLES20();

	const size_t stride = sizeof(ImmediateVertexGLES20);
	const ImmediateVertexGLES20* vertices = &STATE.m_Imm.m_Vertices[0];

	void* dstVertices = LockSharedBufferGLES20 (GL_ARRAY_BUFFER, stride * vertexCount);
	DebugAssert (dstVertices);
	memcpy (dstVertices, vertices, stride * vertexCount);

	if(STATE.m_Imm.m_Mode == kPrimitiveQuads && !STATE.m_Imm.m_IndexBufferQuadsID)
	{
		GLES_CHK(glGenBuffers( 1, (GLuint*)&STATE.m_Imm.m_IndexBufferQuadsID));
		GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, STATE.m_Imm.m_IndexBufferQuadsID));
		GLES_CHK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, kMaxImmediateVerticesPerDraw * 6, STATE.m_Imm.m_QuadsIB, GL_STATIC_DRAW));
		GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
	}

	const GLuint vbo = UnlockSharedBufferGLES20 ();
	const GLuint ibo = (STATE.m_Imm.m_Mode == kPrimitiveQuads)? STATE.m_Imm.m_IndexBufferQuadsID: 0;

	//;;printf_console("ImmediateEnd> vbo: %d ibo: %d\n", vbo, ibo);
	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, vbo));
	GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo));

	size_t offset = 0;
	if(gGraphicsCaps.gles20.slowDynamicVBO)
		offset = (size_t)static_cast<DynamicGLES2VBO&> (GetRealGfxDevice ().GetDynamicVBO ()).GetVertexMemory(stride * vertexCount);

	GLES_CHK(glEnableVertexAttribArray(GL_VERTEX_ARRAY));
	//;;printf_console("ImmediateEnd> vertex %d\n", offset);
	GLES_CHK(glVertexAttribPointer(GL_VERTEX_ARRAY, 3, GL_FLOAT, false, stride, (void*)offset)); offset += sizeof(Vector3f);
	GLES_CHK(glEnableVertexAttribArray(GL_NORMAL_ARRAY));
	//;;printf_console("ImmediateEnd> normal %d\n", offset);
	GLES_CHK(glVertexAttribPointer(GL_NORMAL_ARRAY, 3, GL_FLOAT, false, stride, (void*)offset)); offset += sizeof(Vector3f);
	GLES_CHK(glEnableVertexAttribArray(GL_COLOR_ARRAY));
	//;;printf_console("ImmediateEnd> color %d\n", offset);
	GLES_CHK(glVertexAttribPointer(GL_COLOR_ARRAY, 4, GL_UNSIGNED_BYTE, true, stride, (void*)offset)); offset += sizeof(UInt32);
	for (size_t q = 0; q < gGraphicsCaps.maxTexUnits; ++q)
	{
        if (GL_TEXTURE_ARRAY0 + q < gGraphicsCaps.gles20.maxAttributes)
		{
			GLES_CHK(glEnableVertexAttribArray(GL_TEXTURE_ARRAY0 + q));
			//;;printf_console("ImmediateEnd> tex%d %d\n", q, offset);
			GLES_CHK(glVertexAttribPointer(GL_TEXTURE_ARRAY0 + q, 3, GL_FLOAT, false, stride, (void*)offset)); offset += sizeof(Vector3f);
		}
		else
		{
			#pragma message ("ToDo")
		}
	}

	if(!gGraphicsCaps.gles20.slowDynamicVBO)
	{
		DebugAssert (offset <= stride);
	}

	BeforeDrawCall (true);

	//;;printf_console("ImmediateEnd> dip %d %d\n", STATE.m_Imm.m_Mode, vertexCount);
		switch (STATE.m_Imm.m_Mode)
		{
			case kPrimitiveTriangles:
				GLES_CHK(glDrawArrays(GL_TRIANGLES, 0, vertexCount));
				m_Stats.AddDrawCall( vertexCount / 3, vertexCount );
				break;
			case kPrimitiveTriangleStripDeprecated:
				GLES_CHK(glDrawArrays(GL_TRIANGLE_STRIP, 0, vertexCount));
				m_Stats.AddDrawCall( vertexCount - 2, vertexCount );
				break;
			case kPrimitiveQuads:
			GLES_CHK(glDrawElements(GL_TRIANGLES, (vertexCount/2)*3, GL_UNSIGNED_SHORT, 0));
				m_Stats.AddDrawCall( vertexCount / 2, vertexCount );
				break;
			case kPrimitiveLines:
				GLES_CHK(glDrawArrays(GL_LINES, 0, vertexCount));
				m_Stats.AddDrawCall( vertexCount / 2, vertexCount );
				break;
			default:
				AssertString("ImmediateEnd: unknown draw mode");
		}

	// reset VBO bindings
	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
	GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
	InvalidateVertexInputCacheGLES20();

	// clear vertices
	STATE.m_Imm.m_Vertices.clear();
}

// ---------- readback path

bool GFX_GL_IMPL::CaptureScreenshot( int left, int bottom, int width, int height, UInt8* rgba32 )
{
	GLES_CHK(glReadPixels( left, bottom, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba32 ));
	return true;
}

bool GFX_GL_IMPL::ReadbackImage( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY )
{
	return ReadbackTextureGLES2(image, left, bottom, width, height, destX, destY, GetSharedFBO(STATE), GetHelperFBO(STATE));
}
void GFX_GL_IMPL::GrabIntoRenderTexture( RenderSurfaceHandle rs, RenderSurfaceHandle rd, int x, int y, int width, int height )
{
	::GrabIntoRenderTextureGLES2(rs, rd, x, y, width, height, GetSharedFBO(STATE), GetHelperFBO(STATE));
}


#if ENABLE_PROFILER


void GFX_GL_IMPL::BeginProfileEvent(const char* name)
{
    if(gGraphicsCaps.gles20.hasDebugMarkers)
        gGlesExtFunc.glPushGroupMarkerEXT(0, name);
}

void GFX_GL_IMPL::EndProfileEvent()
{
    if(gGraphicsCaps.gles20.hasDebugMarkers)
        gGlesExtFunc.glPopGroupMarkerEXT();
}

GfxTimerQuery* GFX_GL_IMPL::CreateTimerQuery()
{
	if( gGraphicsCaps.hasTimerQuery )
		return new TimerQueryGLES;
	return NULL;
}

void GFX_GL_IMPL::DeleteTimerQuery(GfxTimerQuery* query)
{
	delete query;
}

void GFX_GL_IMPL::BeginTimerQueries()
{
	if( !gGraphicsCaps.hasTimerQuery )
		return;

	g_TimerQueriesGLES.BeginTimerQueries();
}

void GFX_GL_IMPL::EndTimerQueries()
{
	if( !gGraphicsCaps.hasTimerQuery )
		return;

	g_TimerQueriesGLES.EndTimerQueries();
}
#endif // ENABLE_PROFILER

typedef std::map<FixedFunctionStateGLES20, FixedFunctionProgramGLES20*, FullStateCompareGLES20> FFProgramCacheT;
typedef std::map<FixedFunctionStateGLES20, GLShaderID, VertexStateCompareGLES20> FFVertexProgramCacheT;
typedef std::map<FixedFunctionStateGLES20, GLShaderID, FragmentStateCompareGLES20> FFFragmentProgramCacheT;

static FFProgramCacheT g_FixedFunctionProgramCache;
static FFVertexProgramCacheT g_FFVertexProgramCache;
static FFFragmentProgramCacheT g_FFFragmentProgramCache;

void ClearFixedFunctionPrograms()
{
	for (FFVertexProgramCacheT::iterator it = g_FFVertexProgramCache.begin(); it != g_FFVertexProgramCache.end(); ++it)
	{
		GLES_CHK(glDeleteShader(it->second));
	}
	g_FFVertexProgramCache.clear();
	for (FFFragmentProgramCacheT::iterator it = g_FFFragmentProgramCache.begin(); it != g_FFFragmentProgramCache.end(); ++it)
	{
		GLES_CHK(glDeleteShader(it->second));
	}
	g_FFFragmentProgramCache.clear();

	for(FFProgramCacheT::iterator i = g_FixedFunctionProgramCache.begin(); i != g_FixedFunctionProgramCache.end(); ++i)
	{
		delete i->second;
	}
	g_FixedFunctionProgramCache.clear();

}

static const FixedFunctionProgramGLES20* GetFixedFunctionProgram(const FixedFunctionStateGLES20& state)
{
	FFProgramCacheT::const_iterator cachedProgIt = g_FixedFunctionProgramCache.find(state);
	if (cachedProgIt != g_FixedFunctionProgramCache.end())
		return cachedProgIt->second;

	// Cache miss, create fixed function program
	// NOTE: don't worry too much about performance of vertex/fragment maps
	// shader building/compilation is crazy expensive anyway
	FFVertexProgramCacheT::const_iterator vertexProgIt = g_FFVertexProgramCache.find(state);
	FFFragmentProgramCacheT::const_iterator fragmentProgIt = g_FFFragmentProgramCache.find(state);

	GLShaderID vertexShader = (vertexProgIt != g_FFVertexProgramCache.end())? vertexProgIt->second: 0;
	GLShaderID fragmentShader   = (fragmentProgIt != g_FFFragmentProgramCache.end())? fragmentProgIt->second: 0;

	if (vertexShader == 0)
	{
		vertexShader        = glCreateShader(GL_VERTEX_SHADER);
		std::string src     = BuildVertexShaderSourceGLES20(state);
		const char* cStr    = src.c_str();

		DBG_SHADER_VERBOSE_GLES20("Compiling generated vertex shader");
        CompileGlslShader(vertexShader, kErrorCompileVertexShader, cStr);
		GLESAssert();

		g_FFVertexProgramCache[state] = vertexShader;
	}

	if (fragmentShader == 0)
	{
		fragmentShader      = glCreateShader(GL_FRAGMENT_SHADER);
		std::string src     = BuildFragmentShaderSourceGLES20(state);
		const char* cStr    = src.c_str();

		DBG_SHADER_VERBOSE_GLES20("Compiling generated fragment shader");
        CompileGlslShader(fragmentShader, kErrorCompileFragShader, cStr);
		GLESAssert();

		g_FFFragmentProgramCache[state] = fragmentShader;
	}

	DBG_SHADER_VERBOSE_GLES20("Creating and linking GLES program");
    FixedFunctionProgramGLES20* ffProg = new FixedFunctionProgramGLES20(vertexShader, fragmentShader);
	g_FixedFunctionProgramCache[state] = ffProg;

	return ffProg;
}

static bool ComputeTextureTransformMatrix(TextureUnitStateGLES2 const& tex, Matrix4x4f const& worldViewMatrix, Matrix4x4f const& worldMatrix,
										  Matrix4x4f& outMatrix)
{
	switch (tex.texGen)
	{
		case kTexGenDisabled:
			// NOTE: although tex-gen can be disabled
			// textureMatrix can contain UV scale/offset
			// so we will set it
		case kTexGenObject:
			if (tex.identityMatrix)
			{
				outMatrix.SetIdentity();
				return false;
			}
			CopyMatrix(tex.textureMatrix.GetPtr(), outMatrix.GetPtr());
			break;
		case kTexGenSphereMap:
		{
			float invScale = 1.0f / Magnitude (worldViewMatrix.GetAxisX());

			Matrix4x4f scaleOffsetMatrix;
			scaleOffsetMatrix.SetScale(Vector3f(0.5*invScale, 0.5*invScale, 0.0));
			scaleOffsetMatrix.SetPosition(Vector3f(0.5, 0.5, 0.0));

			Matrix4x4f worldViewMatrixRotation = worldViewMatrix;
			worldViewMatrixRotation.SetPosition(Vector3f::zero);
			Matrix4x4f combo;
			MultiplyMatrices4x4(&scaleOffsetMatrix, &worldViewMatrixRotation, &combo);
			MultiplyMatrices4x4(&tex.textureMatrix, &combo, &outMatrix);
			break;
		}
		case kTexGenEyeLinear:
			MultiplyMatrices4x4(&tex.textureMatrix, &worldViewMatrix, &outMatrix);
			break;
		case kTexGenCubeNormal:
		case kTexGenCubeReflect:
		{
			float invScale = 1.0f / Magnitude (worldMatrix.GetAxisX());
			CopyMatrix(worldViewMatrix.GetPtr(), outMatrix.GetPtr());
			outMatrix.Scale(Vector3f(invScale, invScale, invScale));
			outMatrix.SetPosition(Vector3f::zero);
			break;
		}
		default:
			ErrorString( Format("Unknown TexGen mode %d", tex.texGen) );
	}
	return true;
}

void VBOContainsColorGLES20(bool flag)
{
	GFX_GL_IMPL& device = static_cast<GFX_GL_IMPL&>(GetRealGfxDevice());
	GetGLES20DeviceState(device).vboContainsColor = flag;
}

void GLSLUseProgramGLES20(UInt32 programID)
{
	GFX_GL_IMPL& device = static_cast<GFX_GL_IMPL&>(GetRealGfxDevice());
	if (GetGLES20DeviceState(device).activeProgramID == programID)
	{
#if UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
        if (gGraphicsCaps.gles20.buggyVFetchPatching)
		{
			// NOTE: Qualcomm driver (Android2.2) fails to re-patch vfetch instructions
			// if shader stays the same, but vertex layout has been changed
			// as a temporary workaround just reset the shader
			GLES_CHK(glUseProgram (0)); // driver caches shaderid, so set to 0 first
			GLES_CHK(glUseProgram (programID));
			return;
		}
#endif
		return;
	}

#if NV_STATE_FILTERING
	GetGLES20DeviceState(device).transformState.dirtyFlags |= TransformState::kWorldViewProjDirty;
#endif

	GLES_CHK(glUseProgram (programID));
	GetGLES20DeviceState(device).activeProgramID = programID;
}


struct SetValuesFunctorES2
{
	SetValuesFunctorES2(GfxDevice& device, UniformCacheGLES20* targetCache) : m_Device(device), m_TargetCache(targetCache) { }
	GfxDevice& m_Device;
	UniformCacheGLES20* m_TargetCache;
	void SetVectorVal (ShaderParamType type, int index, const float* ptr, int cols)
	{
		switch(cols) {
		case 1: CachedUniform1(m_TargetCache, type, index, ptr); break;
		case 2: CachedUniform2(m_TargetCache, type, index, ptr); break;
		case 3: CachedUniform3(m_TargetCache, type, index, ptr); break;
		case 4: CachedUniform4(m_TargetCache, type, index, ptr); break;
		}
	}
	void SetMatrixVal (int index, const Matrix4x4f* ptr, int rows)
	{
		DebugAssert(rows == 4);
		GLES_CHK(glUniformMatrix4fv (index, 1, GL_FALSE, ptr->GetPtr()));
	}
	void SetTextureVal (ShaderType shaderType, int index, int samplerIndex, TextureDimension dim, TextureID texID)
	{
		m_Device.SetTexture (shaderType, index, samplerIndex, texID, dim, std::numeric_limits<float>::infinity());
	}
};


void GFX_GL_IMPL::BeforeDrawCall(bool immediateMode)
{
	DBG_LOG_GLES20("BeforeDrawCall(%s)", GetBoolString(immediateMode));

	ShaderLab::PropertySheet *props = ShaderLab::g_GlobalProperties;
	Assert(props);

	// WorldView Matrix
#if NV_STATE_FILTERING
	// UpdateWorldViewMatrix will reset the dirty flags, however we want to forward any modified matrices
	// to the shader, so we "reset" the dirty state after UpdateWorldViewMatrix has been called
	UInt32 savedDirty = STATE.transformState.dirtyFlags;
	STATE.transformState.UpdateWorldViewMatrix (m_BuiltinParamValues);
	STATE.transformState.dirtyFlags = savedDirty;
#else
	STATE.transformState.UpdateWorldViewMatrix (m_BuiltinParamValues);
#endif

	// Materials
	if (STATE.lighting)
	{
		m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatEmission, Vector4f(STATE.matEmissive.GetPtr()));
		m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatAmbient, Vector4f(STATE.matAmbient.GetPtr()));
		m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatDiffuse, Vector4f(STATE.matDiffuse.GetPtr()));
		m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatSpecular, Vector4f(STATE.matSpecular.GetPtr()));
		m_BuiltinParamValues.SetVectorParam(kShaderVecFFMatShininess, Vector4f(STATE.matShininess, STATE.matShininess, STATE.matShininess, STATE.matShininess));
	}

	// Fog
	if (m_FogParams.mode > kFogDisabled)
	{
		float diff    = m_FogParams.mode == kFogLinear ? m_FogParams.end - m_FogParams.start : 0.0f;
		float invDiff = Abs(diff) > 0.0001f ? 1.0f/diff : 0.0f;

		m_BuiltinParamValues.SetVectorParam(kShaderVecFFFogColor, m_FogParams.color);
		m_BuiltinParamValues.SetVectorParam(kShaderVecFFFogParams, Vector4f(m_FogParams.density * 1.2011224087f,
																		   m_FogParams.density * 1.4426950408f,
																		   m_FogParams.mode == kFogLinear ? -invDiff : 0.0f,
																		   m_FogParams.mode == kFogLinear ? m_FogParams.end * invDiff : 0.0f
																		 ));
	}
	// Alpha-test
	m_BuiltinParamValues.SetVectorParam(kShaderVecFFAlphaTestRef, Vector4f(STATE.alphaValue, STATE.alphaValue, STATE.alphaValue, STATE.alphaValue));

    UniformCacheGLES20* targetCache = 0;
	if (STATE.activeProgram)
	{
		// Apply GPU program
        GlslGpuProgramGLES20& prog = static_cast<GlslGpuProgramGLES20&>(*STATE.activeProgram);
        int fogIndex = prog.ApplyGpuProgramES20 (*STATE.activeProgramParams, STATE.activeProgramParamsBuffer.data());
		m_BuiltinParamIndices[kShaderVertex] = &STATE.activeProgramParams->GetBuiltinParams();

		targetCache = &prog.m_UniformCache[fogIndex];
	}
	else
	{
		// Emulate Fixed Function pipe
		m_BuiltinParamValues.SetVectorParam(kShaderVecFFColor, Vector4f(STATE.color.GetPtr()));
		for (int i = 0; i < STATE.textureCount; ++i)
		{
			m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecFFTextureEnvColor0+i), STATE.textures[i].color);
		}

		// generate program from fixed function state
		DBG_LOG_GLES20("  using fixed-function");
		FixedFunctionStateGLES20 ffstate;
		STATE.ComputeFixedFunctionState(ffstate, m_FogParams);
        const FixedFunctionProgramGLES20* program = GetFixedFunctionProgram(ffstate);
        program->ApplyFFGpuProgram(m_BuiltinParamValues);
		m_BuiltinParamIndices[kShaderVertex] = &program->GetBuiltinParams();

		targetCache = &program->m_UniformCache;
	}

	// Set Unity built-in parameters
	{
		Assert(m_BuiltinParamIndices[kShaderVertex]);
		const BuiltinShaderParamIndices& params = *m_BuiltinParamIndices[kShaderVertex];

		// MVP matrix
		if (params.mat[kShaderInstanceMatMVP].gpuIndex >= 0
#if NV_STATE_FILTERING
				&& (STATE.transformState.dirtyFlags & TransformState::kWorldViewProjDirty)
#endif
			)
		{
			Matrix4x4f wvp;
			MultiplyMatrices4x4(&m_BuiltinParamValues.GetMatrixParam(kShaderMatProj), &STATE.transformState.worldViewMatrix, &wvp);

			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatMVP];
			Assert(matParam.rows == 4 && matParam.cols == 4);
			GLES_CHK(glUniformMatrix4fv (matParam.gpuIndex, 1, GL_FALSE, wvp.GetPtr()));
		}
		// MV matrix
		if (params.mat[kShaderInstanceMatMV].gpuIndex >= 0
#if NV_STATE_FILTERING
				&& (STATE.transformState.dirtyFlags & TransformState::kWorldViewDirty)
#endif
			)
		{
			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatMV];
			Assert(matParam.rows == 4 && matParam.cols == 4);
			GLES_CHK(glUniformMatrix4fv (matParam.gpuIndex, 1, GL_FALSE, STATE.transformState.worldViewMatrix.GetPtr()));
		}
		// Transpose of MV matrix
		if (params.mat[kShaderInstanceMatTransMV].gpuIndex >= 0
#if NV_STATE_FILTERING
				&& (STATE.transformState.dirtyFlags & TransformState::kWorldViewDirty)
#endif
			)
		{
			Matrix4x4f tWV;
			TransposeMatrix4x4(&STATE.transformState.worldViewMatrix, &tWV);

			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatTransMV];
			Assert(matParam.rows == 4 && matParam.cols == 4);
			GLES_CHK(glUniformMatrix4fv (matParam.gpuIndex, 1, GL_FALSE, tWV.GetPtr()));
		}
		// Inverse transpose of MV matrix
		if (params.mat[kShaderInstanceMatInvTransMV].gpuIndex >= 0
#if NV_STATE_FILTERING
				&& (STATE.transformState.dirtyFlags & TransformState::kWorldViewDirty)
#endif
			)
		{
			// Inverse transpose of modelview should be scaled by uniform
			// normal scale (this will match state.matrix.invtrans.modelview
			// and gl_NormalMatrix in OpenGL)
			Matrix4x4f mat = STATE.transformState.worldViewMatrix;
			if (STATE.normalization == kNormalizationScale)
			{
				float invScale = m_BuiltinParamValues.GetInstanceVectorParam(kShaderInstanceVecScale).w;
				mat.Get (0, 0) *= invScale;
				mat.Get (1, 0) *= invScale;
				mat.Get (2, 0) *= invScale;
				mat.Get (0, 1) *= invScale;
				mat.Get (1, 1) *= invScale;
				mat.Get (2, 1) *= invScale;
				mat.Get (0, 2) *= invScale;
				mat.Get (1, 2) *= invScale;
				mat.Get (2, 2) *= invScale;
			}
			Matrix4x4f invWV, tInvWV;
			Matrix4x4f::Invert_General3D (mat, invWV);
			TransposeMatrix4x4(&invWV, &tInvWV);

			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatInvTransMV];
			Assert(matParam.rows == 4 && matParam.cols == 4);
			GLES_CHK(glUniformMatrix4fv (matParam.gpuIndex, 1, GL_FALSE, tInvWV.GetPtr()));
		}
		// M matrix
		if (params.mat[kShaderInstanceMatM].gpuIndex >= 0
#if NV_STATE_FILTERING
				&& (STATE.transformState.dirtyFlags & TransformState::kWorldDirty)
#endif
			)
		{
			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatM];
			const Matrix4x4f& mat = STATE.transformState.worldMatrix;
			Assert(matParam.rows == 4 && matParam.cols == 4);
			GLES_CHK(glUniformMatrix4fv (matParam.gpuIndex, 1, GL_FALSE, mat.GetPtr()));
		}
		// Inverse M matrix
		if (params.mat[kShaderInstanceMatInvM].gpuIndex >= 0
#if NV_STATE_FILTERING
				&& (STATE.transformState.dirtyFlags & TransformState::kWorldDirty)
#endif
			)
		{
			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatInvM];
			Matrix4x4f mat = STATE.transformState.worldMatrix;
			if (STATE.normalization == kNormalizationScale)
			{
				// Kill scale in the world matrix before inverse
				float invScale = m_BuiltinParamValues.GetInstanceVectorParam(kShaderInstanceVecScale).w;
				mat.Get (0, 0) *= invScale;
				mat.Get (1, 0) *= invScale;
				mat.Get (2, 0) *= invScale;
				mat.Get (0, 1) *= invScale;
				mat.Get (1, 1) *= invScale;
				mat.Get (2, 1) *= invScale;
				mat.Get (0, 2) *= invScale;
				mat.Get (1, 2) *= invScale;
				mat.Get (2, 2) *= invScale;
			}
			Matrix4x4f inverseMat;
			Matrix4x4f::Invert_General3D (mat, inverseMat);
			Assert(matParam.rows == 4 && matParam.cols == 4);
			GLES_CHK(glUniformMatrix4fv (matParam.gpuIndex, 1, GL_FALSE, inverseMat.GetPtr()));
		}

		// Normal matrix
		if (params.mat[kShaderInstanceMatNormalMatrix].gpuIndex >= 0
#if NV_STATE_FILTERING
				&& (STATE.transformState.dirtyFlags & TransformState::kWorldViewDirty)
#endif
			)
		{
			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatNormalMatrix];

			// @TBD: remove normalization in fixed function emulation after Normal matrix multiply.
			Matrix4x4f rotWV;
			rotWV = STATE.transformState.worldViewMatrix;
			rotWV.SetPosition(Vector3f::zero); // reset translation

			if (STATE.normalization == kNormalizationScale) // reset scale
			{
				float invScale = m_BuiltinParamValues.GetInstanceVectorParam(kShaderInstanceVecScale).w;
				rotWV.Get (0, 0) *= invScale;
				rotWV.Get (1, 0) *= invScale;
				rotWV.Get (2, 0) *= invScale;
				rotWV.Get (0, 1) *= invScale;
				rotWV.Get (1, 1) *= invScale;
				rotWV.Get (2, 1) *= invScale;
				rotWV.Get (0, 2) *= invScale;
				rotWV.Get (1, 2) *= invScale;
				rotWV.Get (2, 2) *= invScale;
			}
			Matrix3x3f rotWV33 = Matrix3x3f(rotWV);

			GLES_CHK(glUniformMatrix3fv (matParam.gpuIndex, 1, GL_FALSE, rotWV33.GetPtr()));
		}

		// Set instance vector parameters
		for (int i = 0; i < kShaderInstanceVecCount; ++i)
		{
			int gpuIndexVS = params.vec[i].gpuIndex;
			if (gpuIndexVS >= 0)
			{
				const float* val = m_BuiltinParamValues.GetInstanceVectorParam((ShaderBuiltinInstanceVectorParam)i).GetPtr();
				switch (params.vec[i].dim) {
					case 1: CachedUniform1(targetCache, kShaderParamFloat, gpuIndexVS, val); break;
					case 2: CachedUniform2(targetCache, kShaderParamFloat, gpuIndexVS, val); break;
					case 3: CachedUniform3(targetCache, kShaderParamFloat, gpuIndexVS, val); break;
					case 4: CachedUniform4(targetCache, kShaderParamFloat, gpuIndexVS, val); break;
				}
				GLESAssert();
			}
		}

		// Texture Matrices
		Matrix4x4f texM;
		for (int i = 0; i < kMaxSupportedTextureUnitsGLES; ++i)
		{
			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatTexture0 + i];
			if (matParam.gpuIndex >= 0)
			{
				if (i < STATE.textureCount)
					ComputeTextureTransformMatrix(STATE.textures[i], STATE.transformState.worldViewMatrix, STATE.transformState.worldMatrix, texM);
				else
					texM.SetIdentity();

				Assert(matParam.rows == 4 && matParam.cols == 4);
				GLES_CHK(glUniformMatrix4fv (matParam.gpuIndex, 1, GL_FALSE, texM.GetPtr()));
			}
		}
#if NV_STATE_FILTERING
		STATE.transformState.dirtyFlags = 0;
#endif
	}

	// Set per-drawcall properties
	SetValuesFunctorES2 setValuesFunc(*this, targetCache);
	ApplyMaterialPropertyBlockValuesES(m_MaterialProperties, STATE.activeProgram, STATE.activeProgramParams, setValuesFunc);
}

bool GFX_GL_IMPL::IsPositionRequiredForTexGen(int unit) const
{
	if (unit >= STATE.textureCount)
		return false;
	if (STATE.activeProgram)
		return false;

	//DebugAssertIf( unit < 0 || unit >= gGraphicsCaps.maxTexUnits);
	const TextureUnitStateGLES2& unitState = STATE.textures[unit];
	return TextureUnitStateGLES2::PositionRequiredForTexGen(unitState.texGen);
}

bool GFX_GL_IMPL::IsNormalRequiredForTexGen(int unit) const
{
	if (unit >= STATE.textureCount)
		return false;
	if (STATE.activeProgram)
		return false;

	//DebugAssertIf( unit < 0 || unit >= gGraphicsCaps.maxTexUnits );
	const TextureUnitStateGLES2& unitState = STATE.textures[unit];
	return TextureUnitStateGLES2::NormalRequiredForTexGen(unitState.texGen);
}


bool GFX_GL_IMPL::IsPositionRequiredForTexGen() const
{
	return ( STATE.positionTexGen != 0 && !STATE.activeProgram );
}

bool GFX_GL_IMPL::IsNormalRequiredForTexGen() const
{
	return ( STATE.normalTexGen != 0 && !STATE.activeProgram );
}

RenderTextureFormat GFX_GL_IMPL::GetDefaultRTFormat() const
{
#if UNITY_PEPPER
	// desktop platfrom = noone cares
	return kRTFormatARGB32;
#else

	RenderTextureFormat defaultFormat = GetCurrentFBColorFormatGLES20();
	return gGraphicsCaps.supportsRenderTextureFormat[defaultFormat] ? defaultFormat : kRTFormatARGB32;

#endif
}

RenderTextureFormat GFX_GL_IMPL::GetDefaultHDRRTFormat() const
{
#if UNITY_PEPPER
	// desktop platfrom = noone cares
	return kRTFormatARGB32;
#else

	if(gGraphicsCaps.supportsRenderTextureFormat[kRTFormatARGBHalf])
		return kRTFormatARGBHalf;

	RenderTextureFormat defaultFormat = GetCurrentFBColorFormatGLES20();
	return gGraphicsCaps.supportsRenderTextureFormat[defaultFormat] ? defaultFormat : kRTFormatARGB32;

#endif
}


void* GFX_GL_IMPL::GetNativeTexturePointer(TextureID id)
{
	return (void*)TextureIdMap::QueryNativeTexture(id);
}



void GFX_GL_IMPL::ReloadResources()
{
	MarkAllVBOsLost();
	GetDynamicVBO().Recreate();

	GfxDevice::CommonReloadResources(kReleaseRenderTextures | kReloadShaders | kReloadTextures);
	ClearFixedFunctionPrograms();

    extern void ClearFBMapping();
    ClearFBMapping();
	STATE.m_SharedFBO = STATE.m_HelperFBO = 0;

	InvalidateState();
}

void GFX_GL_IMPL::InitFramebufferDepthFormat()
{
	m_FramebufferDepthFormat = QueryFBDepthFormatGLES2();
}

// Acquire thread ownership on the calling thread. Worker releases ownership.
void GFX_GL_IMPL::AcquireThreadOwnership()
{
#if UNITY_ANDROID
	AcquireGLES20Context();
#endif
}

// Release thread ownership on the calling thread. Worker acquires ownership.
void GFX_GL_IMPL::ReleaseThreadOwnership()
{
#if UNITY_ANDROID
	ReleaseGLES20Context();
#endif
}

// and now the real magic
void GfxDeviceGLES20_InitFramebufferDepthFormat()
{
	((GFX_GL_IMPL&)GetRealGfxDevice()).InitFramebufferDepthFormat();
}

// ---------- verify state
#if GFX_DEVICE_VERIFY_ENABLE
void GFX_GL_IMPL::VerifyState()
{
}

#endif // GFX_DEVICE_VERIFY_ENABLE

#if NV_STATE_FILTERING
	#ifdef glDeleteBuffers
		#undef glDeleteBuffers
	#endif
	#ifdef glBindBuffer
		#undef glBindBuffer
	#endif
	#ifdef glVertexAttribPointer
		#undef glVertexAttribPointer
	#endif
#endif

#endif // GFX_SUPPORTS_OPENGLES20
