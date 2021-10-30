#include "UnityPrefix.h"
#if GFX_SUPPORTS_OPENGLES30
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
#include "Runtime/Graphics/Image.h"
#include "Runtime/Threads/AtomicOps.h"
#include "Runtime/GfxDevice/TransformState.h"
#include "IncludesGLES30.h"
#include "AssertGLES30.h"
#include "ConstantBuffersGLES30.h"
#include "ContextGLES30.h"
#include "VBOGLES30.h"
#include "TexturesGLES30.h"
#include "CombinerGLES30.h"
#include "GpuProgramsGLES30.h"
#include "FixedFunctionStateGLES30.h"
#include "ShaderGeneratorGLES30.h"
#include "RenderTextureGLES30.h"
#include "DebugGLES30.h"
#include "TimerQueryGLES30.h"
#include "TextureIdMapGLES30.h"
#include "UtilsGLES30.h"
#include "Runtime/GfxDevice/opengles30/TransformFeedbackSkinnedMesh.h"
#include "DataBuffersGLES30.h"
#include "Runtime/GfxDevice/opengles/ExtensionsGLES.h"

#if UNITY_IPHONE
	#include "PlatformDependent/iPhonePlayer/iPhoneSettings.h"
#elif UNITY_ANDROID
	#include "PlatformDependent/AndroidPlayer/ContextGLES.h"
	#include "PlatformDependent/AndroidPlayer/EntryPoint.h"
	#include "PlatformDependent/AndroidPlayer/AndroidSystemInfo.h"
#endif

#include "Runtime/GfxDevice/GLDataBufferCommon.h"

#define UNITY_GLES3_ENTRYPOINTS_FROM_GETPROCADDR 0

#if UNITY_ANDROID
#undef UNITY_GLES3_ENTRYPOINTS_FROM_GETPROCADDR
#define UNITY_GLES3_ENTRYPOINTS_FROM_GETPROCADDR 1
#endif

#if UNITY_GLES3_ENTRYPOINTS_FROM_GETPROCADDR

#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x

#define DO_GLFUNC(retval, name, ...) typedef void (GL_APIENTRYP PFN##name) (__VA_ARGS__); \
	retval GL_APIENTRY name (__VA_ARGS__) \
{ \
	static PFN##name addr = NULL; \
	if(!addr) \
	{ \
	/*DBG_LOG_GLES30("Retrieving GLES3 proc address " STRINGIFY(name) );*/ \
	addr = (PFN##name) GetGLExtProcAddress(STRINGIFY(name)); \
		if(!addr) \
		{ \
		Assert("Could not find GLES 3.0 entry point" STRINGIFY(name)); \
			return (retval)0; \
		} \
		DBG_LOG_GLES30("Success\n"); \
	} \
	__builtin_return(__builtin_apply((void (*)(...))addr, __builtin_apply_args(), 100)); \
	/*DBG_LOG_GLES30("Called " STRINGIFY(name) " successfully\n");*/ \
}

DO_GLFUNC(void,			glBindBufferBase,					GLenum target, GLuint index, GLuint buffer)
DO_GLFUNC(void,			glBindBufferRange,					GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr range)
DO_GLFUNC(void,			glBlitFramebuffer,					GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
DO_GLFUNC(void,			glDeleteTransformFeedbacks,			GLsizei n, const GLuint* ids)
DO_GLFUNC(void,			glDrawBuffers,						GLsizei n, const GLenum* bufs)
DO_GLFUNC(void, glGenQueries, GLsizei n, GLuint* ids)
DO_GLFUNC(void,			glGetActiveUniformBlockiv,			GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params)
DO_GLFUNC(void,			glGetActiveUniformBlockName,		GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName)
DO_GLFUNC(void,			glGetActiveUniformsiv,				GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params)
DO_GLFUNC(void,			glGetProgramBinary,					GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, GLvoid* binary)
DO_GLFUNC(void, glGetQueryObjectuiv, GLuint id, GLenum pname, GLuint* params)
DO_GLFUNC(GLuint,		glGetUniformBlockIndex,				GLuint program, const GLchar *uniformBlockName)
DO_GLFUNC(void,			glInvalidateFramebuffer,			GLenum target, GLsizei numAttachments, const GLenum* attachments)
DO_GLFUNC(void *, glMapBufferRange, GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
DO_GLFUNC(void,			glProgramBinary,					GLuint program, GLenum binaryFormat, const GLvoid* binary, GLsizei length)
DO_GLFUNC(void,			glReadBuffer,						GLenum buf)
DO_GLFUNC(void,			glRenderbufferStorageMultisample,	GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
DO_GLFUNC(void,			glUniformBlockBinding,				GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
DO_GLFUNC(GLboolean, glUnmapBuffer, GLenum target)
DO_GLFUNC(void, glGenTransformFeedbacks, GLsizei n, GLuint *ids)
DO_GLFUNC(void, glVertexAttribIPointer, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
DO_GLFUNC(void, glTransformFeedbackVaryings, GLuint program, GLsizei count, const char* const* varyings, GLenum bufferMode)
DO_GLFUNC(void, glBeginTransformFeedback, GLenum primitivemode)
DO_GLFUNC(void, glEndTransformFeedback)
DO_GLFUNC(void, glFlushMappedBufferRange, GLenum target, GLintptr offset, GLsizeiptr length)
DO_GLFUNC(void,			glGenVertexArrays,					GLsizei n, GLuint* arrays)
DO_GLFUNC(void,			glDeleteVertexArrays,				GLsizei n, const GLuint* arrays)
DO_GLFUNC(void,			glBindVertexArray,					GLuint array)

#undef DO_GLFUNC
#undef STRINGIFY
#undef STRINGIFY2

#endif
static void ClearFixedFunctionPrograms();

// \todo [2013-04-16 pyry] UtilsGLES30 or similar for these?

static int queryInt (UInt32 pname)
{
	int value = 0;
	GLES_CHK(glGetIntegerv(pname, &value));
	return value;
}

// let's play safe here:
//   ios/glesemu works just fine
//   and shadows demands more careful eps choice - do it later
#define WORKAROUND_POLYGON_OFFSET UNITY_ANDROID

// forward declarations

namespace ShaderLab {
	TexEnv* GetTexEnvForBinding( const TextureBinding& binding, const PropertySheet* props ); // pass.cpp
}

// local forward declarations
struct DeviceStateGLES30;
static void ApplyBackfaceMode( const DeviceStateGLES30& state );

static FramebufferObjectManagerGLES30& GetFBOManager (DeviceStateGLES30& deviceState);

// NOTE: GLES3.0 supports only 4 lights for now
enum { kMaxSupportedVertexLightsByGLES30 = 4 };

// Constant tables
static const unsigned int kBlendModeES2[] = {
	GL_ZERO, GL_ONE, GL_DST_COLOR, GL_SRC_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR,
	GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE, GL_ONE_MINUS_SRC_ALPHA,
};

static const unsigned int kBlendFuncES2[] = {
	GL_FUNC_ADD, GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX,
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

static const GLenum kGLES30TextureDimensionTable[kTexDimCount] = {0, -1/*GL_TEXTURE_1D*/, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP, 0};

static const GLint kMinFilterES2[kTexFilterCount] = { GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR };

// --------------------------------------------------------------------------

struct DeviceDepthStateGLES30 : public DeviceDepthState
{
	UInt32      depthFunc;
};

struct DeviceStencilStateGLES30 : public DeviceStencilState
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
typedef std::map< GfxDepthState, DeviceDepthStateGLES30,  memcmp_less<GfxDepthState> > CachedDepthStates;
typedef std::map< GfxStencilState, DeviceStencilStateGLES30,  memcmp_less<GfxStencilState> > CachedStencilStates;
typedef std::map< GfxRasterState, DeviceRasterState,  memcmp_less<GfxRasterState> > CachedRasterStates;

// --------------------------------------------------------------------------
struct TextureUnitStateGLES3
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

void TextureUnitStateGLES3::Invalidate()
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
struct ImmediateVertexGLES30 {
	Vector3f    vertex;
	Vector3f    normal;
	UInt32      color;
	Vector3f    texCoords[8];
};

struct ImmediateModeGLES30 {
	std::vector<ImmediateVertexGLES30>  m_Vertices;
	ImmediateVertexGLES30               m_Current;
	GfxPrimitiveType                    m_Mode;

	DataBufferGLES30*                   m_IndexBufferQuads;

	ImmediateModeGLES30();
	~ImmediateModeGLES30();
	void Invalidate();
};

// --------------------------------------------------------------------------
struct DeviceStateGLES30
{
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

	TextureUnitStateGLES3
					textures[kMaxSupportedTextureUnitsGLES];
	int             textureCount;
	int             activeTextureUnit;

	// pure optimization: texGen is very special case and is used sparingly
	UInt32          positionTexGen;
	UInt32          normalTexGen;

	int             vertexLightCount;
	LightType       vertexLightTypes[kMaxSupportedVertexLights];

	TransformState  transformState;

	ConstantBuffersGLES30
					m_CBs;

	DynamicVBO*     m_DynamicVBO;
	bool            vboContainsColor;

	int             viewport[4];
	int             scissorRect[4];


	GpuProgram*					activeProgram;
	const GpuProgramParameters*	activeProgramParams;
	dynamic_array<UInt8>		activeProgramParamsBuffer;
	UInt32						activeProgramID;


	CachedBlendStates   m_CachedBlendStates;
	CachedDepthStates   m_CachedDepthStates;
	CachedStencilStates m_CachedStencilStates;
	CachedRasterStates  m_CachedRasterStates;

	const DeviceBlendState*         m_CurrBlendState;
	const DeviceDepthStateGLES30*   m_CurrDepthState;
	const DeviceStencilStateGLES30* m_CurrStencilState;
	int                             m_StencilRef;
	const DeviceRasterState*        m_CurrRasterState;

	MaterialPropertyBlock       m_MaterialProperties;
	ImmediateModeGLES30 m_Imm;

	// Framebuffer objects.
	FramebufferObjectManagerGLES30*		m_fboManager;
	FramebufferObjectGLES30*			m_activeFbo;	//!< Currently bound FBO.
	FramebufferObjectGLES30*			m_defaultFbo;	//!< Default render target FBO or null if using default framebuffer (0).

public:
	DeviceStateGLES30();
	void Invalidate();
	void ComputeFixedFunctionState(FixedFunctionStateGLES30& state, const GfxFogParams& fog) const;

	inline void ApplyTexGen( UInt32 unit );
	inline void DropTexGen( UInt32 unit );
};

DeviceStateGLES30::DeviceStateGLES30()
:   m_DynamicVBO(0)
,	m_fboManager(0)
,	m_activeFbo	(0)
,	m_defaultFbo(0)
{
	m_TextureIDGenerator = 0;
}

void DeviceStateGLES30::Invalidate()
{
	DBG_LOG_GLES30("Invalidate");
	int i;

	depthFunc = -1; //unknown
	depthWrite = -1;

	blending = -1; // unknown
	srcBlend = destBlend = -1; // won't match any GL mode
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

	m_activeFbo = 0;

	InvalidateVertexInputCacheGLES30();

	GLESAssert();
}

void DeviceStateGLES30::ComputeFixedFunctionState(FixedFunctionStateGLES30& state, const GfxFogParams& fog) const
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
		Assert(textures[i].texDim != kTexDim3D); // OpenGLES3.0 does NOT supports 3D textures
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

	if(gGraphicsCaps.gles30.hasAlphaTestQCOM)
	{
		// we dont want to generate special shader if we have alpha-test done gl style
		state.alphaTest = kFuncDisabled;
	}
	else
	{
	state.alphaTest = alphaTest;
	switch (alphaTest)
	{
		case kFuncNever:
			/* \todo Disable drawing. */
			break;
		case kFuncUnknown:
		case kFuncDisabled:
		case kFuncAlways:
			state.alphaTest = kFuncDisabled;
		default:
			break;
	}
	}
}

inline void DeviceStateGLES30::ApplyTexGen( UInt32 unit )
{
	const TextureUnitStateGLES3& state = textures[unit];

	positionTexGen = state.posForTexGen ? positionTexGen |  (1<<unit)
										: positionTexGen & ~(1<<unit);

	normalTexGen   = state.nrmForTexGen ? normalTexGen |  (1<<unit)
										: normalTexGen & ~(1<<unit);
}

inline void  DeviceStateGLES30::DropTexGen( UInt32 unit )
{
	positionTexGen &= ~(1<<unit);
	normalTexGen   &= ~(1<<unit);
}


#include "GfxDeviceGLES30.h"
#include "Runtime/GfxDevice/GLESCommon.h"

void GfxDeviceGLES30_MarkWorldViewProjDirty()
{
	GFX_GL_IMPL& device = static_cast<GFX_GL_IMPL&>(GetRealGfxDevice());
	GetGLES30DeviceState(device).transformState.dirtyFlags |= TransformState::kWorldViewProjDirty;
}

void GfxDeviceGLES30_DisableDepthTest()
{
	GFX_GL_IMPL& device = static_cast<GFX_GL_IMPL&>(GetRealGfxDevice());
	GetGLES30DeviceState(device).depthFunc = GL_NEVER;
	GLES_CHK(glDisable(GL_DEPTH_TEST));
}

void GraphicsCaps::InitGLES30()
{
	// \todo [2013-04-16 pyry] Requires some serious cleanaup:
	// - This functions shouldn't be member of GraphicsCaps (why it is?)
	// - query extensions once, split to set and check from there
	// - nicer wrapper for int caps queries

	GLES_InitCommonCaps(this);
	gGles3ExtFunc.InitExtFunc();

	shaderCaps = kShaderLevel3;

	maxLights       = kMaxSupportedVertexLightsByGLES30; // vertex light count
	hasAnisoFilter  = QueryExtension("GL_EXT_texture_filter_anisotropic");      // has anisotropic filtering?
	if (hasAnisoFilter)
		GLES_CHK(glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *)&maxAnisoLevel));
	else
		maxAnisoLevel = 1;

	maxTexImageUnits        = 8;
	GLES_CHK(glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexImageUnits));
	maxTexImageUnits        = std::max<int>(std::min<int>( maxTexImageUnits, kMaxSupportedTextureUnitsGLES ), 1);

	maxTexUnits             = maxTexImageUnits;
	maxTexCoords            = maxTexImageUnits;
	maxMRTs					= std::min<int>(FramebufferAttachmentsGLES30::kMaxColorAttachments, queryInt(GL_MAX_COLOR_ATTACHMENTS));

	GLES_CHK(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize));
	GLES_CHK(glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeMapSize));
	GLES_CHK(glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderTextureSize));

	hasMipLevelBias         = true;
	hasMipMaxLevel          = true;

	hasMultiSampleAutoResolve   = false; // ES3 requires explicit glBlitFramebuffer() to do resolve. This affects how RenderTexture behaves.
	hasMultiSample              = true;

	hasBlendSquare          = true;
	hasSeparateAlphaBlend   = true;
	hasBlendSub             = true;
	hasBlendMinMax          = true;

	hasS3TCCompression      = false;

	hasAutoMipMapGeneration = false; // \todo [2013-04-16 pyry] glGenMipmap() does exist

	has3DTexture            = false; // \todo [2013-04-16 pyry] Expose 3D textures

	npot					= kNPOTFull;
	npotRT					= npot;

	hasRenderToTexture      = true;
	hasShadowCollectorPass  = false;

	hasHighPrecisionTextureCombiners = false; // \todo [2013-04-16 pyry] Yep. We can use highp (fp32) in FS. Should be enough.

	hasRenderToCubemap      = true;

	// \note supportsTextureFormat[N < kTexFormatPCCount] = true

	supportsTextureFormat[kTexFormatBGRA32]         = QueryExtension("GL_APPLE_texture_format_BGRA8888");
	supportsTextureFormat[kTexFormatDXT1]           = QueryExtension("GL_EXT_texture_compression_s3tc") || QueryExtension("GL_EXT_texture_compression_dxt1");
	supportsTextureFormat[kTexFormatDXT3]           = QueryExtension("GL_EXT_texture_compression_s3tc") || QueryExtension("GL_CHROMIUM_texture_compression_dxt3");
	supportsTextureFormat[kTexFormatDXT5]           = QueryExtension("GL_EXT_texture_compression_s3tc") || QueryExtension("GL_CHROMIUM_texture_compression_dxt5");
	supportsTextureFormat[kTexFormatPVRTC_RGB2]     = QueryExtension("GL_IMG_texture_compression_pvrtc");
	supportsTextureFormat[kTexFormatPVRTC_RGBA2]    = QueryExtension("GL_IMG_texture_compression_pvrtc");
	supportsTextureFormat[kTexFormatPVRTC_RGB4]     = QueryExtension("GL_IMG_texture_compression_pvrtc");
	supportsTextureFormat[kTexFormatPVRTC_RGBA4]    = QueryExtension("GL_IMG_texture_compression_pvrtc");
	supportsTextureFormat[kTexFormatATC_RGB4]       = QueryExtension("GL_AMD_compressed_ATC_texture") || QueryExtension("GL_ATI_texture_compression_atitc");
	supportsTextureFormat[kTexFormatATC_RGBA8]      = QueryExtension("GL_AMD_compressed_ATC_texture") || QueryExtension("GL_ATI_texture_compression_atitc");

	supportsTextureFormat[kTexFormatETC_RGB4]       = true; // \note Mapped to ETC2.
	supportsTextureFormat[kTexFormatEAC_R]			= true;
	supportsTextureFormat[kTexFormatEAC_R_SIGNED]	= true;
	supportsTextureFormat[kTexFormatEAC_RG]			= true;
	supportsTextureFormat[kTexFormatEAC_RG_SIGNED]	= true;
	supportsTextureFormat[kTexFormatETC2_RGB]		= true;
	supportsTextureFormat[kTexFormatETC2_RGBA1]		= true;
	supportsTextureFormat[kTexFormatETC2_RGBA8]		= true;

	const bool supportsASTC = QueryExtension("GL_KHR_texture_compression_astc_ldr");
	for(int loop = kTexFormatASTC_RGB_4x4; loop <= kTexFormatASTC_RGBA_12x12; loop++)
		supportsTextureFormat[loop] = supportsASTC;

	// \todo [2013-04-16 pyry] Support means nothing until we can expose these to shaders as well.
	//						   It requires changes to shader translator.
	supportsRenderTextureFormat[kRTFormatARGB32]		= true;
	supportsRenderTextureFormat[kRTFormatDepth]			= true;
	supportsRenderTextureFormat[kRTFormatShadowMap]		= true;
	supportsRenderTextureFormat[kRTFormatRGB565]		= true;
	supportsRenderTextureFormat[kRTFormatARGB4444]		= true;
	supportsRenderTextureFormat[kRTFormatARGB1555]		= true;
	supportsRenderTextureFormat[kRTFormatDefault]		= true;
	supportsRenderTextureFormat[kRTFormatA2R10G10B10]	= true;
	supportsRenderTextureFormat[kRTFormatARGB64]		= false;
	supportsRenderTextureFormat[kRTFormatR8]			= true;
	supportsRenderTextureFormat[kRTFormatARGBInt]		= true;
	supportsRenderTextureFormat[kRTFormatRGInt]			= true;
	supportsRenderTextureFormat[kRTFormatRInt]			= true;

	if (QueryExtension("GL_EXT_color_buffer_float"))
	{
		// Support for fp render targets was stripped from spec as last minute decision. Sigh..
		supportsRenderTextureFormat[kRTFormatARGBHalf]		= true;
		supportsRenderTextureFormat[kRTFormatRGHalf]		= true;
		supportsRenderTextureFormat[kRTFormatRHalf]			= true;

		supportsRenderTextureFormat[kRTFormatARGBFloat]		= true;
		supportsRenderTextureFormat[kRTFormatRGFloat]		= true;
		supportsRenderTextureFormat[kRTFormatRFloat]		= true;

		supportsRenderTextureFormat[kRTFormatDefaultHDR]	= true; // ES3 backend uses R11F_G11F_B10F as default HDR format
	}

	const bool isPvrGpu		= (rendererString.find("PowerVR") != string::npos);
	const bool isMaliGpu	= (rendererString.find("Mali") != string::npos);
	const bool isAdrenoGpu	= (rendererString.find("Adreno") != string::npos);
	const bool isTegraGpu	= (rendererString.find("Tegra") != string::npos);

	hasNativeDepthTexture		= true;
	hasNativeShadowMap			= true;

	hasRenderTargetStencil		= true;
	hasTwoSidedStencil			= true;
	hasStencilInDepthTexture	= true;
	hasStencil					= true;

	has16BitFloatVertex			= true;
	needsToSwizzleVertexColors  = false;

	hasSRGBReadWrite			= false; // \todo [2013-06-05 pyry] Doesn't function properly

	disableSoftShadows			= false;
	buggyCameraRenderToCubemap	= false;

	hasShadowCollectorPass		= false;

	// Timer queries
	gGraphicsCaps.hasTimerQuery	= QueryExtension("GL_NV_timer_query");

	// GLES3-specific variables.
	gles30.maxAttributes		= std::min<int>(kGLES3MaxVertexAttribs, queryInt(GL_MAX_VERTEX_ATTRIBS));
	gles30.maxVaryings			= queryInt(GL_MAX_VARYING_VECTORS);
	gles30.maxSamples			= queryInt(GL_MAX_SAMPLES);

	hasTiledGPU			= isPvrGpu || isAdrenoGpu || isMaliGpu;

	const bool isAdreno330 = (rendererString.find("Adreno (TM) 330") != string::npos); // MSM8974 dev device, with rather unstable drivers.
	const bool isMali628 = (rendererString.find("Mali-T628") != string::npos); // ARM TG4 dev device, mapbuffer broken.
	const bool isGLESEmu =  (rendererString.find("Mali OpenGL ES Emulator") != string::npos);  // ARM OpenGL ES 3.0 emulator, glGetProgramBinary broken

	gles30.useMapBuffer			= true;
	if(isAdrenoGpu || !isAdreno330)
		gles30.useMapBuffer		= false; // Buffer mapping is dead-slow on Adreno, but using BufferData() is broken on Adreno 330

	if(isMali628)
		gles30.useMapBuffer		= false; // Mapbuffer broken on TG4

	gles30.useProgramBinary		= true; // Set true first, check actual support in Init call below.
	gles30.useProgramBinary		=  GlslGpuProgramGLES30::InitBinaryShadersSupport();


	gles30.useTFSkinning		= true;  // TF skinning used to be broken on adreno, worked around now.


	// \todo [2013-04-17 pyry] Extension queries.

	// \todo [2013-04-16 pyry] Figure out which gles20 flags make sense for ES3 as well.

	// \todo [2013-04-16 pyry] Why init timer queries here?
#if ENABLE_PROFILER
	g_TimerQueriesGLES30.Init();
#endif
}

GfxDevice* CreateGLES30GfxDevice()
{
#if UNITY_WIN || UNITY_LINUX || UNITY_BB10 || UNITY_ANDROID
	InitializeGLES30();
#endif

	gGraphicsCaps.InitGLES30();

#if UNITY_EDITOR
	return NULL;
#else
	return UNITY_NEW_AS_ROOT(GFX_GL_IMPL(), kMemGfxDevice, "GLES30GfxDevice","");
#endif
}

GFX_GL_IMPL::GFX_GL_IMPL()
{
	printf_console("Creating OpenGL ES 3.0 graphics device\n");
	#if !GFX_DEVICE_VIRTUAL
	impl = new GfxDeviceImpl();
	#endif

	OnCreate();
	InvalidateState();
	m_Renderer = kGfxRendererOpenGLES30;
	m_IsThreadable = true;

	m_UsesOpenGLTextureCoords = true;
	m_UsesHalfTexelOffset = false;

	STATE.m_CurrBlendState = NULL;
	STATE.m_CurrDepthState = NULL;
	STATE.m_CurrStencilState = NULL;
	STATE.m_CurrRasterState = NULL;

	extern RenderSurfaceBase* CreateBackBufferColorSurfaceGLES3();
	SetBackBufferColorSurface(CreateBackBufferColorSurfaceGLES3());

	extern RenderSurfaceBase* CreateBackBufferDepthSurfaceGLES3();
	SetBackBufferDepthSurface(CreateBackBufferDepthSurfaceGLES3());
}

GFX_GL_IMPL::~GFX_GL_IMPL()
{
	TransformFeedbackSkinningInfo::CleanupTransformFeedbackShaders();
	ClearFixedFunctionPrograms();
	STATE.m_CBs.Clear();
	delete STATE.m_DynamicVBO;
	delete STATE.m_fboManager;

	#if !GFX_DEVICE_VIRTUAL
	delete impl;
	#endif
#if UNITY_WIN || UNITY_ANDROID
	ShutdownGLES30();
#endif
}

static void ActivateTextureUnitGLES3 (DeviceStateGLES30& state, int unit)
{
	if (state.activeTextureUnit == unit)
		return;
	GLES_CHK(glActiveTexture(GL_TEXTURE0 + unit));
	state.activeTextureUnit = unit;
}

void GFX_GL_IMPL::InvalidateState()
{
	DBG_LOG_GLES30("InvalidateState");
	m_FogParams.Invalidate();
	STATE.transformState.Invalidate(m_BuiltinParamValues);
	STATE.Invalidate();

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
	std::pair<CachedDepthStates::iterator, bool> result = STATE.m_CachedDepthStates.insert(std::make_pair(state, DeviceDepthStateGLES30()));
	if (!result.second)
		return &result.first->second;

	DeviceDepthStateGLES30& glstate = result.first->second;
	memcpy(&glstate.sourceState, &state, sizeof(glstate.sourceState));
	glstate.depthFunc = kCmpFuncES2[state.depthFunc];
	return &result.first->second;
}

DeviceStencilState* GFX_GL_IMPL::CreateStencilState(const GfxStencilState& state)
{
	std::pair<CachedStencilStates::iterator, bool> result = STATE.m_CachedStencilStates.insert(std::make_pair(state, DeviceStencilStateGLES30()));
	if (!result.second)
		return &result.first->second;

	DeviceStencilStateGLES30& st = result.first->second;
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
	if (STATE.m_activeFbo && STATE.m_activeFbo->GetNumColorAttachments() == 0)
		mask = 0;

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
			GLES_CHK(glBlendEquationSeparate(glfunc, glfunca));
			STATE.blendOp = glfunc;
			STATE.blendOpAlpha = glfunca;
		}
		if( STATE.blending != 1 )
		{
			GLES_CHK(glEnable( GL_BLEND ));
			STATE.blending = 1;
		}
	}
	// fragment shader is expected to implement per fragment culling
	CompareFunction alphaTest = devstate->sourceState.alphaTest;

	// \todo [2013-04-16 pyry] Alpha testing should be moved to shaders

	if (gGraphicsCaps.gles30.hasAlphaTestQCOM && (alphaTest != STATE.alphaTest || alphaRef != STATE.alphaValue))
	{
		if (alphaTest != kFuncDisabled)
		{
			GLES_CHK(gGles3ExtFunc.glAlphaFuncQCOM(kCmpFuncES2[alphaTest], alphaRef));
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
	DeviceDepthStateGLES30* devstate = (DeviceDepthStateGLES30*)state;
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
	const DeviceStencilStateGLES30* st = static_cast<const DeviceStencilStateGLES30*>(state);
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
	// \todo [2013-04-16 pyry] Implement sRGB support:
	// - In ES3 sRGB bit is tied to format and there is no way to create views with different format
	// - This is used rather liberally from Camera
	// -> Use sRGB FBO for emulation, and defer necessary blits until it is known whether they are needed
	Assert("Not implemented");
}

bool GFX_GL_IMPL::GetSRGBWrite ()
{
	return false;
}

void GFX_GL_IMPL::Clear (UInt32 clearFlags, const float color[4], float depth, int stencil)
{
	DBG_LOG_GLES30("Clear(%d, (%.2f, %.2f, %.2f, %.2f), %.2f, %d", clearFlags, color[0], color[1], color[2], color[3], depth, stencil);

	// \todo [2013-04-16 pyry] Integer render targets require use of glClearBuffer()
	// \todo [2013-04-29 pyry] Here was a call that restored FBO binding to default one. Why?

	if (STATE.m_activeFbo && STATE.m_activeFbo->GetNumColorAttachments() == 0)
		clearFlags &= ~kGfxClearColor;

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
		GLES_CHK(glClearColor( color[0], color[1], color[2], color[3] ));
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

static void ApplyBackfaceMode( const DeviceStateGLES30& state )
{
	DBG_LOG_GLES30("ApplyBackfaceMode");
	if (state.appBackfaceMode != state.userBackfaceMode)
		GLES_CHK(glFrontFace( GL_CCW ));
	else
		GLES_CHK(glFrontFace( GL_CW ));
}

void GFX_GL_IMPL::SetUserBackfaceMode( bool enable )
{
	DBG_LOG_GLES30("SetUserBackfaceMode(%s)", GetBoolString(enable));
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
	DBG_LOG_GLES30("SetProjectionMatrix(...)");

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
	DBG_LOG_GLES30("SetNormalizationBackface(%d  %s)", mode, backface?"back":"front");
	STATE.normalization = mode;
	if( STATE.appBackfaceMode != backface )
	{
		STATE.appBackfaceMode = backface;
		ApplyBackfaceMode( STATE );
	}
}

void GFX_GL_IMPL::SetFFLighting( bool on, bool separateSpecular, ColorMaterialMode colorMaterial )
{
	DBG_LOG_GLES30("SetFFLighting(%s, %s, %d)", on?"True":"False", separateSpecular?"True":"False", colorMaterial);
	STATE.lighting = on;
	STATE.separateSpecular = separateSpecular;
	STATE.colorMaterial = colorMaterial;
}

void GFX_GL_IMPL::SetMaterial( const float ambient[4], const float diffuse[4], const float specular[4], const float emissive[4], const float shininess )
{
	DBG_LOG_GLES30("SetMaterial()");
	STATE.matAmbient.set (ambient[0], ambient[1], ambient[2], 1.0F);
	STATE.matDiffuse.set (diffuse);
	STATE.matSpecular.set (specular[0], specular[1], specular[2], 1.0F);
	STATE.matEmissive.set (emissive[0], emissive[1], emissive[2], 1.0F);
	float glshine = std::max<float>(std::min<float>(shininess,1.0f), 0.0f) * 128.0f;
	STATE.matShininess = glshine;
}

void GFX_GL_IMPL::SetColor( const float color[4] )
{
	DBG_LOG_GLES30("SetColor()");
	STATE.color.set( color );
	// Emulate OpenGL's behaviour
	ImmediateColor( color[0], color[1], color[2], color[3] );
}

void GFX_GL_IMPL::SetViewport( int x, int y, int width, int height )
{
	DBG_LOG_GLES30("SetViewport(%d, %d, %d, %d)", x, y, width, height);
	STATE.viewport[0] = x;
	STATE.viewport[1] = y;
	STATE.viewport[2] = width;
	STATE.viewport[3] = height;
	GLES_CHK(glViewport( x, y, width, height ));
}

void GFX_GL_IMPL::GetViewport( int* port ) const
{
	DBG_LOG_GLES30("GetViewport()");
	port[0] = STATE.viewport[0];
	port[1] = STATE.viewport[1];
	port[2] = STATE.viewport[2];
	port[3] = STATE.viewport[3];
}

void GFX_GL_IMPL::SetScissorRect( int x, int y, int width, int height )
{
	DBG_LOG_GLES30("SetScissorRect(%d, %d, %d, %d)", x, y, width, height);

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

void GFX_GL_IMPL::DisableScissor()
{
	DBG_LOG_GLES30("DisableScissor()");
	if (STATE.scissor != 0)
	{
		GLES_CHK(glDisable( GL_SCISSOR_TEST ));
		STATE.scissor = 0;
	}
}

bool GFX_GL_IMPL::IsScissorEnabled() const
{
	DBG_LOG_GLES30("IsScissorEnabled():returns %s", STATE.scissor == 1?"True":"False");
	return STATE.scissor == 1;
}

void GFX_GL_IMPL::GetScissorRect( int scissor[4] ) const
{
	DBG_LOG_GLES30("GetScissorRect()");
	scissor[0] = STATE.scissorRect[0];
	scissor[1] = STATE.scissorRect[1];
	scissor[2] = STATE.scissorRect[2];
	scissor[3] = STATE.scissorRect[3];
}
bool	GFX_GL_IMPL::IsCombineModeSupported( unsigned int combiner )
{
	return true;
}

TextureCombinersHandle GFX_GL_IMPL::CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular )
{
	DBG_LOG_GLES30("CreateTextureCombiners()");
	TextureCombinersGLES3* implGLES = TextureCombinersGLES3::Create (count, texEnvs);
	return TextureCombinersHandle (implGLES);
}

void GFX_GL_IMPL::DeleteTextureCombiners( TextureCombinersHandle& textureCombiners )
{
	DBG_LOG_GLES30("DeleteTextureCombiners()");
	TextureCombinersGLES3* implGLES = OBJECT_FROM_HANDLE(textureCombiners, TextureCombinersGLES3);
	delete implGLES;
	textureCombiners.Reset();
}

void GFX_GL_IMPL::SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props )
{
	DBG_LOG_GLES30("SetTextureCombiners()");
	TextureCombinersGLES3* implGLES = OBJECT_FROM_HANDLE(textureCombiners,TextureCombinersGLES3);
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


void	GFX_GL_IMPL::SetTextureCombinersThreadable( TextureCombinersHandle textureCombiners, const TexEnvData* texEnvData, const Vector4f* texColors )
{
	DBG_LOG_GLES30("SetTextureCombiners()");
	TextureCombinersGLES3* implGLES = OBJECT_FROM_HANDLE(textureCombiners,TextureCombinersGLES3);
	Assert (implGLES);

	const int count = std::min(gGraphicsCaps.maxTexUnits, implGLES->count);
	STATE.textureCount = count;
	for (int i = 0; i < count; ++i)
	{
		const ShaderLab::TextureBinding& binding = implGLES->texEnvs[i];

		// set the texture
		ApplyTexEnvData (i, i, texEnvData[i]);

		// setup texture unit state
		TextureUnitStateGLES3& texUnitState = STATE.textures[i];
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
	ActivateTextureUnitGLES3 (STATE, 0);

}

void GFX_GL_IMPL::SetTexture (ShaderType shaderType, int unit, int samplerUnit, TextureID texture, TextureDimension dim, float bias)
{
	DBG_LOG_GLES30("SetTexture(%d %d)", unit, texture.m_ID);
	DebugAssertIf( unit < 0 || unit >= gGraphicsCaps.maxTexUnits );

	GLenum	texType		= kGLES30TextureDimensionTable[dim];
	GLuint	targetTex	= (GLuint)TextureIdMap::QueryNativeTexture(texture);

	if (texType == (GLenum)-1)
	{
		Assert("Not supported");
		return;
	}

	// \todo [2013-04-16 pyry] Shouldn't we clear state still?
	if (targetTex == 0)
		return;

	TextureUnitStateGLES3& currTex = STATE.textures[unit];
	if (STATE.textureCount > unit && targetTex == currTex.texID)
		return; // Already bound.

	ActivateTextureUnitGLES3 (STATE, unit);

	GLES_CHK(glBindTexture(texType, targetTex));

	if (STATE.textureCount <= unit)
		STATE.textureCount = unit+1;
	currTex.texID = targetTex;
	currTex.texDim = dim;
	if (currTex.texGen == kTexGenUnknown)
		currTex.texGen = kTexGenDisabled;

	STATE.ApplyTexGen(unit);

	GLESAssert();

	// \todo [2013-04-16 pyry] Lod bias is given from shader.
	if (bias != std::numeric_limits<float>::infinity())
		currTex.bias = bias;
}

void GFX_GL_IMPL::SetTextureTransform( int unit, TextureDimension dim, TexGenMode texGen, bool identity, const float matrix[16])
{
	DBG_LOG_GLES30("SetTextureTransform()");
	DebugAssertIf( unit < 0 || unit >= gGraphicsCaps.maxTexUnits );
	TextureUnitStateGLES3& unitState = STATE.textures[unit];

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

void GFX_GL_IMPL::SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace )
{
	DBG_LOG_GLES30("SetTextureParams()");

	TextureIdMapGLES30_QueryOrCreate(texture);

	GLuint target = kGLES30TextureDimensionTable[texDim];
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

void GFX_GL_IMPL::SetMaterialProperties( const MaterialPropertyBlock& block )
{
	STATE.m_MaterialProperties = block;
}

void    GFX_GL_IMPL::SetShadersThreadable (GpuProgram* programs[kShaderTypeCount], const GpuProgramParameters* params[kShaderTypeCount], UInt8 const * const paramsBuffer[kShaderTypeCount])
{
	GpuProgram* vertexProgram = programs[kShaderVertex];
	GpuProgram* fragmentProgram = programs[kShaderFragment];

	DBG_LOG_GLES30("SetShaders(%s, %s)",
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
	GlslGpuProgramGLES30& programGLES = static_cast<GlslGpuProgramGLES30&>(program->GetGpuProgram());
	programGLES.GetGLProgram(fogMode, program->GetParams(fogMode), program->GetChannels());
}

bool GFX_GL_IMPL::IsShaderActive( ShaderType type ) const
{
	//DBG_LOG_GLES30("IsShaderActive(%s): returns %s", GetShaderTypeString(type), STATE.shaderEnabledImpl[type] != kShaderImplUndefined?"True":"False");
	//return STATE.shaderEnabledImpl[type] != kShaderImplUndefined;
	DBG_LOG_GLES30("IsShaderActive(%s):", GetShaderTypeString(type));
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

void GFX_GL_IMPL::SetConstantBufferInfo( int id, int size )
{
	STATE.m_CBs.SetCBInfo(id, size);
}

void GFX_GL_IMPL::DisableLights( int startLight )
{
	DBG_LOG_GLES30("DisableLights(%d)", startLight);
	startLight = std::min (startLight, gGraphicsCaps.maxLights);
	STATE.vertexLightCount = startLight;
	for (int i = startLight; i < kMaxSupportedVertexLightsByGLES30; ++i)
	{
		m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Position + i), Vector4f(0.0f, 0.0f, 1.0f, 0.0f));
		m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Diffuse + i), Vector4f(0.0f, 0.0f, 0.0f, 0.0f));
	}
}

void GFX_GL_IMPL::SetLight( int light, const GfxVertexLight& data)
{
	DBG_LOG_GLES30("SetLight(%d), [{%f, %f, %f, %f}, {%f, %f, %f, %f}, {%f, %f, %f, %f}, %f, %f, %f, %d]",
			 light,
			 data.position[0],  data.position[1],   data.position[2],   data.position[3],
			 data.spotDirection[0], data.spotDirection[1],  data.spotDirection[2],  data.spotDirection[3],
			 data.color[0], data.color[1],  data.color[2],  data.color[3],
			 data.range, data.quadAtten, data.spotAngle, data.type);
	Assert( light >= 0 && light < kMaxSupportedVertexLights );

	if (light >= kMaxSupportedVertexLightsByGLES30)
		return;

	STATE.vertexLightTypes[light] = data.type;

	// Transform lighting into view space
	const Matrix4x4f& viewMat = m_BuiltinParamValues.GetMatrixParam(kShaderMatView);

	// spot direction
	Vector4f& spotDirection = m_BuiltinParamValues.GetWritableVectorParam(BuiltinShaderVectorParam(kShaderVecLight0SpotDirection + light));
	if( data.spotAngle > 0.0f )
	{
		Vector3f d = viewMat.MultiplyVector3( (const Vector3f&)data.spotDirection );
		spotDirection.Set(-d.x, -d.y, -d.z, 0.0f);
	}
	else
	{
		spotDirection.Set(0.0f, 0.0f, 1.0f, 0.0f);
	}

	Vector4f& position = m_BuiltinParamValues.GetWritableVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Position + light));
	if (data.type == kLightDirectional)
	{
		Vector3f p = viewMat.MultiplyVector3( (const Vector3f&)data.position );
		position.Set(-p.x, -p.y, -p.z, 0.0f);
	}
	else
	{
		Vector3f p = viewMat.MultiplyPoint3( (const Vector3f&)data.position );
		position.Set(p.x, p.y, p.z, 1.0f);
	}

	m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Diffuse + light), data.color);
	if (data.spotAngle > 0.0f)
	{
		// spot attenuation formula taken from D3D9 fixed-function emulation
		// see: VertexPipeD3D9.cpp
		const float cosTheta = cosf(Deg2Rad(data.spotAngle)*0.25f);
		const float cosPhi = cosf(Deg2Rad(data.spotAngle)*0.5f);
		const float cosDiff = cosTheta - cosPhi;
		m_BuiltinParamValues.GetWritableVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Atten + light)).Set(cosPhi, (cosDiff != 0.0f) ? 1.0f / cosDiff : 1.0f, data.quadAtten, data.range*data.range);
	}
	else
	{
		// non-spot light
		m_BuiltinParamValues.GetWritableVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Atten + light)).Set(-1.0f, 1.0f, data.quadAtten, data.range*data.range);
	}
}

void GFX_GL_IMPL::SetAmbient( const float ambient[4] )
{
	DBG_LOG_GLES30("SetAmbient()");
	STATE.ambient.set (ambient[0], ambient[1], ambient[2], ambient[3]);
	m_BuiltinParamValues.SetVectorParam(kShaderVecLightModelAmbient, Vector4f(ambient));
}

void GFX_GL_IMPL::EnableFog (const GfxFogParams& fog)
{
	DBG_LOG_GLES30("EnableFog()");
	DebugAssertIf( fog.mode <= kFogDisabled );
	m_FogParams = fog;
}

void GFX_GL_IMPL::DisableFog()
{
	DBG_LOG_GLES30("DisableFog()");

	if( m_FogParams.mode != kFogDisabled )
	{
		m_FogParams.mode = kFogDisabled;
		m_FogParams.density = 0.0f;
	}
}

VBO* GFX_GL_IMPL::CreateVBO()
{
	VBO* vbo = new GLES3VBO();
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
		STATE.m_DynamicVBO = new DynamicGLES3VBO();
	}
	return *STATE.m_DynamicVBO;
}

// ---------- render textures

static FramebufferObjectManagerGLES30& GetFBOManager (DeviceStateGLES30& deviceState)
{
	if (!deviceState.m_fboManager)
		deviceState.m_fboManager = new FramebufferObjectManagerGLES30();
	return *deviceState.m_fboManager;
}

static RenderSurfaceGLES30* CreateRenderTexture (TextureDimension dim, TextureID texID, UInt32 internalFormat, int width, int height, int depth)
{
	if (dim == kTexDim2D)
	{
		Assert(depth == 1);
		return new RenderTexture2DGLES30(texID, internalFormat, width, height);
	}
	else if (dim == kTexDimCUBE)
	{
		Assert(width == height && depth == 1);
		return new RenderTextureCubeGLES30(texID, internalFormat, width, height);
	}
	else
	{
		Assert(!"Not supported");
		return 0;
	}
}

static RenderSurfaceGLES30* CreateShadowMapRenderTexture (TextureDimension dim, TextureID texID, UInt32 internalFormat, int width, int height, int depth)
{
	// \note Assumes that constructor binds texID!
	if (dim == kTexDim2D)
	{
		Assert(depth == 1);
		RenderTexture2DGLES30* tex = new RenderTexture2DGLES30(texID, internalFormat, width, height);
		GLES_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE));
		GLES_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL));
		return tex;
	}
	else if (dim == kTexDimCUBE)
	{
		Assert(width == height && depth == 1);
		RenderTextureCubeGLES30* tex = new RenderTextureCubeGLES30(texID, internalFormat, width, height);
		GLES_CHK(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE));
		GLES_CHK(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL));
		return tex;
	}
	else
	{
		Assert(!"Not supported");
		return 0;
	}
}

static RenderSurfaceGLES30* CreateRenderBuffer (TextureDimension dim, UInt32 internalFormat, int width, int height, int depth, int numSamples)
{
	if (dim == kTexDim2D)
	{
		Assert(depth == 1);
		return new RenderBufferGLES30(internalFormat, width, height, numSamples);
	}
	else if (dim == kTexDimCUBE)
	{
		Assert(width == height && depth == 1);
		return new RenderBufferCubeGLES30(internalFormat, width, height, numSamples);
	}
	else
	{
		Assert(!"Not supported");
		return 0;
	}
}

RenderSurfaceHandle GFX_GL_IMPL::CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags)
{
	DBG_LOG_GLES30("CreateRenderColorSurface(id = %d, %dx%dx%d, samples = %d, dim = %d, fmt = %d, flags = 0x%04x)", textureID.m_ID, width, height, depth, samples, dim, format, createFlags);

	// \note Sample count 0 and 1 both map to non-multisampled textures.

	if (createFlags & kSurfaceCreateNeverUsed)
	{
		// Use dummy surface that is not backed by any real GL object.
		return RenderSurfaceHandle(new DummyRenderSurfaceGLES30(width, height));
	}
	else
	{
		const bool		isSRGB			= (createFlags & kSurfaceCreateSRGB) != 0;
		const UInt32	internalFormat	= isSRGB ? GL_SRGB8_ALPHA8 : GetColorFormatGLES30(format);
		const bool		useRBO			= textureID.m_ID == 0 || samples > 1;

		Assert(internalFormat != GL_NONE);
		Assert((createFlags & kSurfaceCreateShadowmap) == 0);

		if (useRBO)
		{
			// \todo [2013-06-04 pyry] There is no global graphics caps for max samples.
			const int numSamplesToUse = std::min(gGraphicsCaps.gles30.maxSamples, std::max(samples,1));
			return RenderSurfaceHandle(CreateRenderBuffer(dim, internalFormat, width, height, depth, numSamplesToUse));
		}
		else
			return RenderSurfaceHandle(CreateRenderTexture(dim, textureID, internalFormat, width, height, depth));
	}

	return RenderSurfaceHandle(0);
}

RenderSurfaceHandle GFX_GL_IMPL::CreateRenderDepthSurface (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags)
{
	DBG_LOG_GLES30("CreateRenderDepthSurface(id = %d, %dx%d, samples = %d, dim = %d, fmt = %d, createFlags = 0x%04x)", textureID.m_ID, width, height, samples, dim, depthFormat, createFlags);

	// \note Sample count 0 and 1 both map to non-multisampled textures.

	if (depthFormat == kDepthFormatNone || (createFlags & kSurfaceCreateNeverUsed) != 0)
	{
		// Umm... Assuming that we don't want depth buffer at all, but still this is called?
		return RenderSurfaceHandle(new DummyRenderSurfaceGLES30(width, height));
	}
	else
	{
		const bool		shadowMap		= (createFlags & kSurfaceCreateShadowmap) != 0;
		const bool		useStencil		= !shadowMap;
		const UInt32	internalFormat	= useStencil ? GetDepthStencilFormatGLES30(depthFormat) : GetDepthOnlyFormatGLES30(depthFormat);
		const bool		useRBO			= textureID.m_ID == 0 || samples > 1;

		Assert(internalFormat != GL_NONE);
		Assert(!shadowMap || !useRBO);

		if (useRBO)
		{
			// \todo [2013-06-04 pyry] There is no global graphics caps for max samples.
			const int numSamplesToUse = std::min(gGraphicsCaps.gles30.maxSamples, std::max(samples,1));
			return RenderSurfaceHandle(CreateRenderBuffer(dim, internalFormat, width, height, 1, numSamplesToUse));
		}
		else if (shadowMap)
			return RenderSurfaceHandle(CreateShadowMapRenderTexture(dim, textureID, internalFormat, width, height, 1));
		else
			return RenderSurfaceHandle(CreateRenderTexture(dim, textureID, internalFormat, width, height, 1));
	}

	return RenderSurfaceHandle(0);
}

void GFX_GL_IMPL::DestroyRenderSurface (RenderSurfaceHandle& rs)
{
	DBG_LOG_GLES30("DestroyRenderSurface(%p)", rs.object);

	if (!rs.IsValid())
		return;

	RenderSurfaceGLES30* renderSurface = static_cast<RenderSurfaceGLES30*>(rs.object);

	// Default FBO should not be managed from outside.
	Assert(!STATE.m_defaultFbo || !IsInFramebufferAttachmentsGLES30(*STATE.m_defaultFbo->GetAttachments(), renderSurface));

	// If rs was attached to current FBO, unbind it.
	if (STATE.m_activeFbo && IsInFramebufferAttachmentsGLES30(*STATE.m_activeFbo->GetAttachments(), renderSurface))
	{
		if (STATE.m_defaultFbo)
			BindFramebufferObjectGLES30(STATE.m_defaultFbo);
		else
			BindDefaultFramebufferGLES30();

		STATE.m_activeFbo = STATE.m_defaultFbo;
	}

	// Delete FBOs where renderSurface was attached.
	STATE.m_fboManager->InvalidateSurface(renderSurface);

	delete renderSurface;
	rs.object = 0;
}

static bool IsAnyRSHandleValid (const RenderSurfaceHandle* handles, int numHandles)
{
	for (int ndx = 0; ndx < numHandles; ndx++)
	{
		if (handles[ndx].IsValid())
			return true;
	}
	return false;
}

void GFX_GL_IMPL::SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face)
{
	bool hasColor	= !colorHandles[0].object->backBuffer;
	bool hasDepth	= !depthHandle.object->backBuffer;

	DBG_LOG_GLES30("SetRenderTargets(count = %d, color =  %s, depth = %s, mip = %d, face = %d)", count, (hasColor ? "yes" : "no"), (hasDepth ? "yes" : "no"), mipLevel, face);

	// \todo [2013-05-06 pyry] Enable SetRenderTargets() variant with flags and check if we should discard buffers.
	// \todo [2013-05-02 pyry] It probably makes sense to do RT change deferred.

	Assert(count <= FramebufferAttachmentsGLES30::kMaxColorAttachments);

	Assert(colorHandles[0].IsValid() && depthHandle.IsValid());
	Assert(colorHandles[0].object->backBuffer == depthHandle.object->backBuffer);

	if(!hasColor && !hasDepth)
	{
		// Assuming default FB - right?
		if (STATE.m_activeFbo != STATE.m_defaultFbo)
		{
			if (STATE.m_defaultFbo)
				BindFramebufferObjectGLES30(STATE.m_defaultFbo);
			else
				BindDefaultFramebufferGLES30();

			STATE.m_activeFbo = STATE.m_defaultFbo;
			GetRealGfxDevice().GetFrameStats().AddRenderTextureChange();
		}
	}
	else
	{
		// Translate to FramebufferAttachments
		FramebufferAttachmentsGLES30 attachments;

		attachments.numColorAttachments		= count;
		attachments.depthStencil			= static_cast<RenderSurfaceGLES30*>(depthHandle.object);
		attachments.cubemapFace				= face;

		for (int ndx = 0; ndx < count; ndx++)
			attachments.color[ndx] = static_cast<RenderSurfaceGLES30*>(colorHandles[ndx].object);

		// Create (or fetch from cache) FBO
		FramebufferObjectGLES30* fbo = GetFBOManager(STATE).GetFramebufferObject(attachments);

		if (STATE.m_activeFbo != fbo)
		{
			BindFramebufferObjectGLES30(fbo);
			STATE.m_activeFbo = fbo;
			GetRealGfxDevice().GetFrameStats().AddRenderTextureChange();
		}
	}
}

void GFX_GL_IMPL::ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle)
{
	DBG_LOG_GLES30("ResolveColorSurface(src = %p, dst = %p)", srcHandle.object, dstHandle.object);

	// Fetch temporary FBOs for resolve - use single color attachment.
	FramebufferObjectGLES30*	srcFbo	= 0;
	FramebufferObjectGLES30*	dstFbo	= 0;

	Assert(srcHandle.object && dstHandle.object);

	for (int ndx = 0; ndx < 2; ndx++)
	{
		FramebufferAttachmentsGLES30 attachments;
		attachments.numColorAttachments	= 1;
		attachments.color[0]			= static_cast<RenderSurfaceGLES30*>(ndx ? srcHandle.object : dstHandle.object);

		(ndx ? srcFbo : dstFbo) = GetFBOManager(STATE).GetFramebufferObject(attachments);
	}

	Assert(srcFbo && dstFbo);

	int	width	= static_cast<RenderSurfaceGLES30*>(dstHandle.object)->GetWidth();
	int	height	= static_cast<RenderSurfaceGLES30*>(dstHandle.object)->GetHeight();

	glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo->GetFboID());
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo->GetFboID());
	GLES_CHK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

	// Restore old FBO binding
	GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, STATE.m_activeFbo ? STATE.m_activeFbo->GetFboID() : 0));
}

RenderSurfaceHandle GFX_GL_IMPL::GetActiveRenderColorSurface(int index)
{
	Assert(0 <= index && index <= FramebufferAttachmentsGLES30::kMaxColorAttachments);

	if (STATE.m_activeFbo)
		return RenderSurfaceHandle(STATE.m_activeFbo->GetColorAttachment(index));
	else
		return RenderSurfaceHandle(0);
}

RenderSurfaceHandle GFX_GL_IMPL::GetActiveRenderDepthSurface()
{
	if (STATE.m_activeFbo)
		return RenderSurfaceHandle(STATE.m_activeFbo->GetDepthStencilAttachment());
	else
		return RenderSurfaceHandle(0);
}

void GFX_GL_IMPL::SetSurfaceFlags (RenderSurfaceHandle surf, UInt32 flags, UInt32 keepFlags)
{
	// \todo [2013-04-29 pyry] Implement handling for flags!
}

void GFX_GL_IMPL::DiscardContents (RenderSurfaceHandle& rs)
{
	DBG_LOG_GLES30("DiscardContents(%p)", rs.object);

	if (!rs.IsValid())
		return; // \todo [2013-06-05 pyry] Bug in threaded device code causes DiscardContents() calls to invalid handles.

	FramebufferObjectGLES30*	curFbo			= STATE.m_activeFbo;
	GLenum						discardAttachments[FramebufferAttachmentsGLES30::kMaxColorAttachments+1];
	int							attachNdx		= 0;

	if (curFbo)
	{
		// Check if rs is attached to current fbo.
		for (int colorNdx = 0; colorNdx < curFbo->GetNumColorAttachments(); colorNdx++)
		{
			if (rs.object == curFbo->GetColorAttachment(colorNdx))
				discardAttachments[attachNdx++] = GL_COLOR_ATTACHMENT0+colorNdx;
		}

		if (rs.object == curFbo->GetDepthStencilAttachment())
		{
			// \todo [2013-05-02 pyry] Should we check if FBO actually has stencil attachment enabled?
			discardAttachments[attachNdx++] = GL_DEPTH_STENCIL_ATTACHMENT;
		}
	}

	Assert(attachNdx <= (int)(sizeof(discardAttachments)/sizeof(discardAttachments[0])));
	if (attachNdx > 0)
		GLES_CHK(glInvalidateFramebuffer(GL_FRAMEBUFFER, attachNdx, &discardAttachments[attachNdx]));

	DBG_LOG_GLES30(" .. discarded in current FBO = %s", (attachNdx > 0) ? "true" : "false");

	// If attachment was not discarded yet, do it later when it is bound.
	if (attachNdx == 0)
	{
		RenderSurfaceGLES30* renderSurface = static_cast<RenderSurfaceGLES30*>(rs.object);
		renderSurface->SetFlags(renderSurface->GetFlags() | RenderSurfaceGLES30::kDiscardOnBind);
	}
}

// ---------- uploading textures

void GFX_GL_IMPL::UploadTexture2D( TextureID texture, TextureDimension dimension, UInt8* srcData, int srcSize, int width, int height, TextureFormat format, int mipCount, UInt32 uploadFlags, int skipMipLevels, TextureUsageMode usageMode, TextureColorSpace colorSpace )
{
	::UploadTexture2DGLES3( texture, dimension, srcData, width, height, format, mipCount, uploadFlags, skipMipLevels, colorSpace );
}

void GFX_GL_IMPL::UploadTextureSubData2D( TextureID texture, UInt8* srcData, int srcSize, int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace )
{
	::UploadTextureSubData2DGLES3( texture, srcData, mipLevel, x, y, width, height, format, colorSpace );
}

void GFX_GL_IMPL::UploadTextureCube( TextureID texture, UInt8* srcData, int srcSize, int faceDataSize, int size, TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace )
{
	::UploadTextureCubeGLES3( texture, srcData, faceDataSize, size, format, mipCount, uploadFlags, colorSpace );
}

void GFX_GL_IMPL::UploadTexture3D( TextureID texture, UInt8* srcData, int srcSize, int width, int height, int depth, TextureFormat format, int mipCount, UInt32 uploadFlags )
{
	// \todo [2013-04-16 pyry] Add support.
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
		TextureUnitStateGLES3& currTex = STATE.textures[i];
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

static void ResetFboBindingToDefault (DeviceStateGLES30& state)
{
	if (state.m_activeFbo != state.m_defaultFbo)
	{
		if (state.m_defaultFbo)
			BindFramebufferObjectGLES30(state.m_defaultFbo);
		else
			BindDefaultFramebufferGLES30();
		state.m_activeFbo = state.m_defaultFbo;
	}
}

void GFX_GL_IMPL::BeginFrame()
{
	DBG_LOG_GLES30("BeginFrame()");
	m_InsideFrame = true;

	if (gGraphicsCaps.hasTiledGPU)
	{
		// \todo [2013-05-02 pyry] Should we reset FBO binding here?
		ResetFboBindingToDefault(STATE);
		if(STATE.scissor)
			GLES_CHK(glDisable(GL_SCISSOR_TEST));
		GLES_CHK(glViewport(0, 0, 65536, 65536));
		GLES_CHK(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
		GLES_CHK(glClearStencil(0));
		GLES_CHK(glClearDepthf(1.0f));
		GLES_CHK(glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT));
		GLES_CHK(glViewport(STATE.viewport[0], STATE.viewport[1], STATE.viewport[2], STATE.viewport[3]));
		if(STATE.scissor)
			GLES_CHK(glEnable(GL_SCISSOR_TEST));

	}
}
void GFX_GL_IMPL::EndFrame()
{
	// \todo [2013-05-02 pyry] Do we really want to reset FBO binding here?
	ResetFboBindingToDefault(STATE);
	if (STATE.m_activeFbo != 0)
	{
		// If rendering to FBO, discard contents.
		static const GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_STENCIL_ATTACHMENT };
		GLES_CHK(glInvalidateFramebuffer(GL_FRAMEBUFFER, sizeof(attachments)/sizeof(attachments[0]), &attachments[0]));
	}
	else
	{
		// System "FBO", discard only depthstencil
		static const GLenum attachments[] = { GL_DEPTH, GL_STENCIL };
		GLES_CHK(glInvalidateFramebuffer(GL_FRAMEBUFFER, sizeof(attachments)/sizeof(attachments[0]), &attachments[0]));
	}

	GetBufferManagerGLES30()->AdvanceFrame();

	DBG_LOG_GLES30("EndFrame()");
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
	DBG_LOG_GLES30("====================================");
	DBG_LOG_GLES30("====================================");
	DBG_LOG_GLES30("PresentFrame");
	DBG_LOG_GLES30("====================================");
	DBG_LOG_GLES30("====================================");

#if UNITY_WIN
	PresentContextGLES30();
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


ImmediateModeGLES30::ImmediateModeGLES30()
	: m_Mode			(kPrimitiveTriangles)
	, m_IndexBufferQuads(0)
{
#if 0
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
#endif
}

ImmediateModeGLES30::~ImmediateModeGLES30()
{
	Invalidate();
}

void ImmediateModeGLES30::Invalidate()
{
	if (m_IndexBufferQuads)
	{
		m_IndexBufferQuads->Release();
		m_IndexBufferQuads = 0;
	}

	m_Vertices.clear();
	memset(&m_Current, 0, sizeof(m_Current));
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
	if (STATE.m_Imm.m_Vertices.empty())
		return;

	const UInt32			minBufferThreshold	= 2048;

	const int				numVertices			= STATE.m_Imm.m_Vertices.size();
	const UInt32			vertexBufStride		= (UInt32)sizeof(ImmediateVertexGLES30);
	const UInt32			vertexBufUsage		= GL_STREAM_DRAW;
	const UInt32			vertexBufSize		= vertexBufStride*numVertices;
	const bool				useBuffer			= vertexBufSize >= minBufferThreshold;
	DataBufferGLES30*		vertexBuffer		= useBuffer ? GetBufferManagerGLES30()->AcquireBuffer(numVertices*vertexBufStride, vertexBufUsage) : 0;
	VertexArrayInfoGLES30	vertexSetup;

	// \todo [2013-05-31 pyry] Recreate or update?
	if (vertexBuffer)
		vertexBuffer->RecreateWithData(vertexBufSize, vertexBufUsage, &STATE.m_Imm.m_Vertices[0]);

	if (STATE.m_Imm.m_Mode == kPrimitiveQuads && !STATE.m_Imm.m_IndexBufferQuads)
	{
		// \todo [2013-05-31 pyry] Move somewhere else.
		const int			quadCount			= kMaxImmediateVerticesPerDraw/4;
		const int			quadIndexCount		= quadCount * 6;
		const int			indexBufferSize		= quadIndexCount * sizeof(UInt16);
		const UInt32		indexBufferUsage	= GL_STATIC_DRAW;
		std::vector<UInt16>	quadIndices			(quadIndexCount);

		for (int quadNdx = 0; quadNdx < quadCount; ++quadNdx)
		{
			const UInt16	srcBaseNdx	= quadNdx*4;
			const int		dstBaseNdx	= quadNdx*6;
			quadIndices[dstBaseNdx + 0] = srcBaseNdx + 1;
			quadIndices[dstBaseNdx + 1] = srcBaseNdx + 2;
			quadIndices[dstBaseNdx + 2] = srcBaseNdx;
			quadIndices[dstBaseNdx + 3] = srcBaseNdx + 2;
			quadIndices[dstBaseNdx + 4] = srcBaseNdx + 3;
			quadIndices[dstBaseNdx + 5] = srcBaseNdx;
		}

		STATE.m_Imm.m_IndexBufferQuads = GetBufferManagerGLES30()->AcquireBuffer(indexBufferSize, indexBufferUsage);
		STATE.m_Imm.m_IndexBufferQuads->RecreateWithData(indexBufferSize, indexBufferUsage, &quadIndices[0]);
	}

	// Fill in vertex setup info.
	{
		const UInt8*	basePtr		= vertexBuffer ? 0 : (const UInt8*)&STATE.m_Imm.m_Vertices[0];
		const UInt32	buffer		= vertexBuffer ? vertexBuffer->GetBuffer() : 0;

		// \todo [2013-05-31 pyry] Do not send unused attributes!
		vertexSetup.enabledArrays = (1<<kGLES3AttribLocationPosition)
								  | (1<<kGLES3AttribLocationColor)
								  | (1<<kGLES3AttribLocationNormal);

		// All are sourcing from same buffer
		for (int i = 0; i < kGLES3MaxVertexAttribs; i++)
			vertexSetup.buffers[i] = buffer;

		vertexSetup.arrays[kGLES3AttribLocationPosition]	= VertexInputInfoGLES30(basePtr + 0,				kChannelFormatFloat, 3, vertexBufStride);
		vertexSetup.arrays[kGLES3AttribLocationNormal]		= VertexInputInfoGLES30(basePtr + 3*sizeof(float),	kChannelFormatFloat, 3, vertexBufStride);
		vertexSetup.arrays[kGLES3AttribLocationColor]		= VertexInputInfoGLES30(basePtr + 6*sizeof(float),	kChannelFormatColor, 4, vertexBufStride);
		UInt32		curOffset	= 6*sizeof(float) + sizeof(UInt32);

		for (int texCoordNdx = 0; texCoordNdx < (kGLES3MaxVertexAttribs-kGLES3AttribLocationTexCoord0); texCoordNdx++)
		{
			const int	attribLoc	= kGLES3AttribLocationTexCoord0+texCoordNdx;

			if (attribLoc < gGraphicsCaps.gles30.maxAttributes)
			{
				vertexSetup.enabledArrays |= (1<<attribLoc);
				vertexSetup.arrays[kGLES3AttribLocationTexCoord0+texCoordNdx] =
					VertexInputInfoGLES30(basePtr + curOffset, kChannelFormatFloat, 3, vertexBufStride);
				curOffset += 3*sizeof(float);
			}
		}
	}

	// Setup state
	BeforeDrawCall(true /* immediate */);
	SetupDefaultVertexArrayStateGLES30(vertexSetup);

	switch (STATE.m_Imm.m_Mode)
	{
		case kPrimitiveTriangles:
			GLES_CHK(glDrawArrays(GL_TRIANGLES, 0, numVertices));
			m_Stats.AddDrawCall(numVertices / 3, numVertices);
			break;

		case kPrimitiveTriangleStripDeprecated:
			GLES_CHK(glDrawArrays(GL_TRIANGLE_STRIP, 0, numVertices));
			m_Stats.AddDrawCall(numVertices - 2, numVertices);
			break;

		case kPrimitiveLines:
			GLES_CHK(glDrawArrays(GL_LINES, 0, numVertices));
			m_Stats.AddDrawCall(numVertices / 2, numVertices);
			break;

		case kPrimitiveQuads:
			Assert(STATE.m_Imm.m_IndexBufferQuads);
			GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, STATE.m_Imm.m_IndexBufferQuads->GetBuffer()));
			GLES_CHK(glDrawElements(GL_TRIANGLES, (numVertices/4)*6, GL_UNSIGNED_SHORT, 0));
			GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
			m_Stats.AddDrawCall(numVertices / 2, numVertices);
			break;

		default:
			AssertString("ImmediateEnd: unknown draw mode");
	}

	if (vertexBuffer)
		vertexBuffer->Release();
}

// ---------- readback path

static int GetFirstValidColorAttachmentNdx (const FramebufferObjectGLES30* fbo)
{
	for (int colorNdx = 0; colorNdx < fbo->GetNumColorAttachments(); colorNdx++)
	{
		if (fbo->GetColorAttachment(colorNdx))
			return colorNdx;
	}

	return -1;
}

static bool IsRenderSurfaceFormatCompatibleWithRGBA8Read (UInt32 format)
{
	switch (format)
	{
		case GL_RGBA8:
		case GL_RGB8:
		case GL_RG8:
		case GL_R8:
		case GL_RGB565:
		case GL_RGB10_A2:
		case GL_RGBA4:
		case GL_RGB5_A1:
		case GL_SRGB8_ALPHA8:
		case GL_SRGB8:
			return true;

		default:
			return false;
	}
}

static bool CanReadRGBA8FromFirstColorAttachment (const FramebufferObjectGLES30* fbo)
{
	int ndx = GetFirstValidColorAttachmentNdx(fbo);
	if (ndx >= 0)
		return IsRenderSurfaceFormatCompatibleWithRGBA8Read(fbo->GetColorAttachment(ndx)->GetFormat());
	else
		return false;
}

static void ReadPixelsFromDefaultFramebufferRGBA8 (int x, int y, int width, int height, UInt8* dstPtr)
{
	GLES_CHK(glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dstPtr));
}

static void ReadPixelsFromActiveFramebufferObjectRGBA8 (DeviceStateGLES30& state, int x, int y, int width, int height, UInt8* dstPtr)
{
	Assert(state.m_activeFbo != 0);
	Assert(CanReadRGBA8FromFirstColorAttachment(state.m_activeFbo));

	FramebufferObjectGLES30*		activeFbo		= state.m_activeFbo;
	const int						colorNdx		= GetFirstValidColorAttachmentNdx(state.m_activeFbo);
	const RenderSurfaceGLES30*		colorSurface	= activeFbo->GetColorAttachment(colorNdx);
	const bool						requiresResolve	= colorSurface->GetNumSamples() > 1;

	if (requiresResolve)
	{
		FramebufferObjectManagerGLES30&	fboManager		= GetFBOManager(state);
		FramebufferObjectGLES30*		resolveBuffer	= GetResolveFramebufferObjectGLES30(&fboManager, colorSurface->GetFormat(), 0,
																							colorSurface->GetWidth(), colorSurface->GetHeight());

		GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
		GLES_CHK(glDrawBuffers(1, &drawBuffer));
		GLES_CHK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveBuffer->GetFboID()));

		// \note active FBO is already GL_READ_FRAMEBUFFER
		GLES_CHK(glReadBuffer(GL_COLOR_ATTACHMENT0+colorNdx));

		// Resolve blit.
		GLES_CHK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

		// Read from resolve buffer.
		GLES_CHK(glReadBuffer(GL_COLOR_ATTACHMENT0));
		GLES_CHK(glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dstPtr));

		// Restore binding.
		BindFramebufferObjectGLES30(activeFbo);
	}
	else
	{
		GLES_CHK(glReadBuffer(GL_COLOR_ATTACHMENT0+colorNdx));
		GLES_CHK(glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dstPtr));
	}
}

void GFX_GL_IMPL::ResolveDepthIntoTexture (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle)
{
	FramebufferAttachmentsGLES30 att;
	att.numColorAttachments = 1;
	att.color[0] = static_cast<RenderSurfaceGLES30 *>(colorHandle.object);
	att.depthStencil = static_cast<RenderSurfaceGLES30 *>(depthHandle.object);
	att.cubemapFace = kCubeFaceUnknown;

	FramebufferObjectGLES30 *helperFBO = GetFBOManager(STATE).GetFramebufferObject(att);
	FramebufferObjectGLES30 *oldFBO = STATE.m_activeFbo;


	// use the full size of the depth buffer, sub-rects are not needed and might not work on some hardware
	GLint x = 0;
	GLint y = 0;
	GLint width = att.depthStencil->GetWidth();
	GLint height = att.depthStencil->GetHeight();

	// bind helper FBO
	GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, helperFBO->GetFboID()));

	GLES_CHK(glReadBuffer (GL_NONE));

	// blit
	GLES_CHK(glBindFramebuffer (GL_READ_FRAMEBUFFER, oldFBO->GetFboID()));
	GLES_CHK(glBindFramebuffer (GL_DRAW_FRAMEBUFFER, helperFBO->GetFboID()));
	GLES_CHK(glBlitFramebuffer (x, y, x + width, y + height, x, y, x + width, y + height, GL_DEPTH_BUFFER_BIT, GL_NEAREST));

	// restore the previously bound FBO
	GLES_CHK(glBindFramebuffer (GL_FRAMEBUFFER, oldFBO->GetFboID()));


}


bool GFX_GL_IMPL::CaptureScreenshot (int left, int bottom, int width, int height, UInt8* rgba32)
{
	if (STATE.m_activeFbo)
	{
		if (CanReadRGBA8FromFirstColorAttachment(STATE.m_activeFbo))
		{
			ReadPixelsFromActiveFramebufferObjectRGBA8(STATE, left, bottom, width, height, rgba32);
			return true;
		}
		else
		{
			ErrorString("Active FBO is not compatible with screenshots");
			return false;
		}
	}
	else
	{
		ReadPixelsFromDefaultFramebufferRGBA8(left, bottom, width, height, rgba32);
		return true;
	}
}

bool GFX_GL_IMPL::ReadbackImage (ImageReference& image, int left, int bottom, int width, int height, int destX, int destY)
{
	const bool		coordsOk		= destX == 0;
	const bool		formatOk		= image.GetFormat() == kTexFormatRGBA32;
	const bool		strideOk		= image.GetRowBytes() == 4*width;
	const bool		directRead		= coordsOk && formatOk && strideOk;

	if (directRead)
		return CaptureScreenshot(left, bottom, width, height, image.GetRowPtr(destY));
	else
	{
		std::vector<UInt8> tmpBuf(width*height*4);
		if (!CaptureScreenshot(left, bottom, width, height, &tmpBuf[0]))
			return false;

		ImageReference blitSrc(width, height, 4*width, kTexFormatRGBA32, &tmpBuf[0]);
		image.BlitImage(destX, destY, blitSrc);

		return true;
	}
}

void GFX_GL_IMPL::GrabIntoRenderTexture( RenderSurfaceHandle rs, RenderSurfaceHandle rd, int x, int y, int width, int height )
{
	if (!rs.IsValid())
		return;

	RenderSurfaceGLES30*	dstColorSurface		= static_cast<RenderSurfaceGLES30*>(rs.object);
	RenderSurfaceGLES30*	dstDepthSurface		= static_cast<RenderSurfaceGLES30*>(rd.object);

	// Grabbing to MSAA targets is not supported.
	if (dstColorSurface->GetNumSamples() > 1 || (dstDepthSurface && dstDepthSurface->GetNumSamples() > 1))
	{
		ErrorString("GrabIntoRenderTexture(): Grabbing to MSAA RenderSurfaces is not supported");
		return;
	}

	if (dstColorSurface->GetType() == RenderSurfaceGLES30::kTypeTexture2D && !dstDepthSurface && !STATE.m_activeFbo)
	{
		// Simple path: use glCopyTexSubImage()
		RenderTexture2DGLES30* dstColorTex = static_cast<RenderTexture2DGLES30*>(dstColorSurface);

		GetRealGfxDevice().SetTexture(kShaderFragment, 0, 0, dstColorTex->GetTextureID(), kTexDim2D, std::numeric_limits<float>::infinity());
		GLES_CHK(glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height));
	}
	else
	{
		// Temporary dst fbo.
		FramebufferObjectManagerGLES30&	fboManager	= GetFBOManager(STATE);
		FramebufferObjectGLES30*		dstFbo		= 0;
		{
			FramebufferAttachmentsGLES30 attachments;
			attachments.numColorAttachments = 1;
			attachments.color[0]			= dstColorSurface;
			attachments.depthStencil		= dstDepthSurface;

			dstFbo = fboManager.GetFramebufferObject(attachments);
		}

		bool		copyColor			= false;
		bool		copyDepth			= false;
		bool		colorNeedsResolve	= false;
		bool		depthNeedsResolve	= false;
		UInt32		srcColorFormat		= 0;
		UInt32		srcDepthFormat		= 0;
		GLenum		srcColorAttachment	= 0;

		if (STATE.m_activeFbo)
		{
			const int					srcColorNdx			= GetFirstValidColorAttachmentNdx(STATE.m_activeFbo);
			const RenderSurfaceGLES30*	srcColorSurface		= srcColorNdx >= 0 ? STATE.m_activeFbo->GetColorAttachment(srcColorNdx) : 0;
			const RenderSurfaceGLES30*	srcDepthSurface		= STATE.m_activeFbo->GetDepthStencilAttachment() ? STATE.m_activeFbo->GetDepthStencilAttachment() : 0;

			copyColor			= srcColorSurface && dstColorSurface;
			copyDepth			= dstColorSurface && dstDepthSurface;
			colorNeedsResolve	= copyColor && srcColorSurface->GetNumSamples() > 1;
			depthNeedsResolve	= copyDepth && srcDepthSurface->GetNumSamples() > 1;
			srcColorFormat		= srcColorSurface ? srcColorSurface->GetFormat() : 0;
			srcDepthFormat		= srcDepthSurface ? srcDepthSurface->GetFormat() : 0;
			srcColorAttachment	= GL_COLOR_ATTACHMENT0 + srcColorNdx;
		}
		else
		{
			const bool isMSAA = queryInt(GL_SAMPLE_BUFFERS) > 0;

			srcColorFormat		= GetDefaultFramebufferColorFormatGLES30();
			srcDepthFormat		= GetDefaultFramebufferDepthFormatGLES30();
			copyColor			= srcColorFormat != 0 && dstColorSurface;
			copyDepth			= srcDepthFormat != 0 && dstDepthSurface;
			colorNeedsResolve	= isMSAA;
			depthNeedsResolve	= isMSAA;
			srcColorAttachment	= GL_BACK;
		}

		const bool			colorFormatMatch	= !copyColor || srcColorFormat == dstColorSurface->GetFormat();
		const bool			depthFormatMatch	= !copyDepth || srcDepthFormat == dstDepthSurface->GetFormat();
		const bool			copyBoundsOk		= x == 0 && y == 0;
		const bool			copyDirectly		= (!colorNeedsResolve || (colorFormatMatch && copyBoundsOk)) && (!depthNeedsResolve || (depthFormatMatch && copyBoundsOk));
		const UInt32		blitBuffers			= (copyColor?GL_COLOR_BUFFER_BIT:0)|(copyDepth?GL_DEPTH_BUFFER_BIT:0);

		// \note There are blits that are not supported altogether. For example blitting
		//		 from unorm to integer format. Supporting them would require emulating the
		//		 blit and even in such case the semantics are rather vague. So we just rely
		//		 on GL giving an error if user attempts to do something strange.

		if (copyDirectly)
		{
			GLenum drawBuffer = GL_COLOR_ATTACHMENT0;

			GLES_CHK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo->GetFboID()));
			GLES_CHK(glDrawBuffers(1, &drawBuffer));

			// \note active FBO is already GL_READ_FRAMEBUFFER
			GLES_CHK(glReadBuffer(srcColorAttachment));

			GLES_CHK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, blitBuffers, GL_NEAREST));
		}
		else
		{
			Assert(colorNeedsResolve || depthNeedsResolve);

			GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
			GLES_CHK(glDrawBuffers(1, &drawBuffer));

			FramebufferObjectGLES30* resolveBuffer = GetResolveFramebufferObjectGLES30(&fboManager,
																					   copyColor ? srcColorFormat : 0,
																					   copyDepth ? srcDepthFormat : 0,
																					   width, height);

			GLES_CHK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveBuffer->GetFboID()));

			// \note active FBO is already GL_READ_FRAMEBUFFER
			GLES_CHK(glReadBuffer(srcColorAttachment));

			// Resolve blit.
			GLES_CHK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, blitBuffers, GL_NEAREST));

			// Blit from resolve buffer to destination.
			GLES_CHK(glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveBuffer->GetFboID()));
			GLES_CHK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo->GetFboID()));
			GLES_CHK(glReadBuffer(GL_COLOR_ATTACHMENT0));
			GLES_CHK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, blitBuffers, GL_NEAREST));
		}

		// Restore readbuffer state.
		GLES_CHK(glReadBuffer(GL_BACK));

		// Restore binding.
		if (!STATE.m_activeFbo)
			BindDefaultFramebufferGLES30();
		else
			BindFramebufferObjectGLES30(STATE.m_activeFbo);
	}
}

#if ENABLE_PROFILER

void GFX_GL_IMPL::BeginProfileEvent(const char* name)
{
	if(gGraphicsCaps.gles30.hasDebugMarkers)
		gGles3ExtFunc.glPushGroupMarkerEXT(0, name);
}

void GFX_GL_IMPL::EndProfileEvent()
{
	if(gGraphicsCaps.gles30.hasDebugMarkers)
		gGles3ExtFunc.glPopGroupMarkerEXT();
}

GfxTimerQuery* GFX_GL_IMPL::CreateTimerQuery()
{
	if( gGraphicsCaps.hasTimerQuery )
		return new TimerQueryGLES30;
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

	g_TimerQueriesGLES30.BeginTimerQueries();
}

void GFX_GL_IMPL::EndTimerQueries()
{
	if( !gGraphicsCaps.hasTimerQuery )
		return;

	g_TimerQueriesGLES30.EndTimerQueries();
}

#endif // ENABLE_PROFILER

typedef std::map<FixedFunctionStateGLES30, FixedFunctionProgramGLES30*, FullStateCompareGLES30> FFProgramCacheT;
typedef std::map<FixedFunctionStateGLES30, GLShaderID, VertexStateCompareGLES30> FFVertexProgramCacheT;
typedef std::map<FixedFunctionStateGLES30, GLShaderID, FragmentStateCompareGLES30> FFFragmentProgramCacheT;

static FFProgramCacheT g_FixedFunctionProgramCache;
static FFVertexProgramCacheT g_FFVertexProgramCache;
static FFFragmentProgramCacheT g_FFFragmentProgramCache;

static void ClearFixedFunctionPrograms()
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

static const FixedFunctionProgramGLES30* GetFixedFunctionProgram(const FixedFunctionStateGLES30& state)
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
		std::string src     = BuildVertexShaderSourceGLES30(state);
		const char* cStr    = src.c_str();

		DBG_SHADER_VERBOSE_GLES30("Compiling generated vertex shader");
		GlslGpuProgramGLES30::CompileGlslShader(vertexShader, cStr);
		GLESAssert();

		g_FFVertexProgramCache[state] = vertexShader;
	}

	if (fragmentShader == 0)
	{
		fragmentShader      = glCreateShader(GL_FRAGMENT_SHADER);
		std::string src     = BuildFragmentShaderSourceGLES30(state);
		const char* cStr    = src.c_str();

		DBG_SHADER_VERBOSE_GLES30("Compiling generated fragment shader");
		GlslGpuProgramGLES30::CompileGlslShader(fragmentShader, cStr);
		GLESAssert();

		g_FFFragmentProgramCache[state] = fragmentShader;
	}

	DBG_SHADER_VERBOSE_GLES30("Creating and linking GLES program");
	FixedFunctionProgramGLES30* ffProg = new FixedFunctionProgramGLES30(vertexShader, fragmentShader);
	g_FixedFunctionProgramCache[state] = ffProg;

	return ffProg;
}

static bool ComputeTextureTransformMatrix(TextureUnitStateGLES3 const& tex, Matrix4x4f const& worldViewMatrix, Matrix4x4f const& worldMatrix,
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

void VBOContainsColorGLES30(bool flag)
{
	GFX_GL_IMPL& device = static_cast<GFX_GL_IMPL&>(GetRealGfxDevice());
	GetGLES30DeviceState(device).vboContainsColor = flag;
}

void GLSLUseProgramGLES30(UInt32 programID)
{
	GFX_GL_IMPL& device = static_cast<GFX_GL_IMPL&>(GetRealGfxDevice());
	if (GetGLES30DeviceState(device).activeProgramID == programID)
		return;

	GLES_CHK(glUseProgram (programID));
	GetGLES30DeviceState(device).activeProgramID = programID;
}

static void UploadUniformMatrix4(BuiltinShaderParamIndices::MatrixParamData& matParam, const GpuProgramParameters::ConstantBufferList* constantBuffers, const float* dataPtr, ConstantBuffersGLES30& cbs)
{
	Assert(matParam.cols == 4 && matParam.rows == 4);
	if (matParam.cbID == -1)
	{
		GLES_CHK(glUniformMatrix4fv (matParam.gpuIndex, 1, GL_FALSE, dataPtr));
	}
	else if (constantBuffers != NULL)
	{
		for (int i = 0; i < constantBuffers->size(); ++i)
		{
			if ((*constantBuffers)[i].m_Name.index == matParam.cbID)
			{
				const GpuProgramParameters::ConstantBuffer& cb = (*constantBuffers)[i];
				int idx = cbs.FindAndBindCB(cb.m_Name.index, cb.m_BindIndex, cb.m_Size);
				cbs.SetCBConstant(idx, matParam.gpuIndex, dataPtr, sizeof(Matrix4x4f));
				break;
			}
		}
	}
}

static void UploadUniformMatrix3(BuiltinShaderParamIndices::MatrixParamData& matParam, const GpuProgramParameters::ConstantBufferList* constantBuffers, const float* dataPtr, ConstantBuffersGLES30& cbs)
{
	Assert(matParam.cols == 3 && matParam.rows == 3);
	if (matParam.cbID == -1)
	{
		GLES_CHK(glUniformMatrix3fv (matParam.gpuIndex, 1, GL_FALSE, dataPtr));
	}
	else if (constantBuffers != NULL)
	{
		for (int i = 0; i < constantBuffers->size(); ++i)
		{
			if ((*constantBuffers)[i].m_Name.index == matParam.cbID)
			{
				const GpuProgramParameters::ConstantBuffer& cb = (*constantBuffers)[i];
				int idx = cbs.FindAndBindCB(cb.m_Name.index, cb.m_BindIndex, cb.m_Size);
				cbs.SetCBConstant(idx, matParam.gpuIndex, dataPtr, sizeof(Matrix3x3f));
				break;
			}
		}
	}
}

void GFX_GL_IMPL::BeforeDrawCall(bool immediateMode)
{
	DBG_LOG_GLES30("BeforeDrawCall(%s)", GetBoolString(immediateMode));

	ShaderLab::PropertySheet *props = ShaderLab::g_GlobalProperties;
	Assert(props);

	// WorldView Matrix
	STATE.transformState.UpdateWorldViewMatrix (m_BuiltinParamValues);

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

	UniformCacheGLES30* targetCache = 0;
	const GpuProgramParameters::ConstantBufferList* constantBuffers = NULL;
	if (STATE.activeProgram)
	{
		// Apply GPU program
		GlslGpuProgramGLES30& prog = static_cast<GlslGpuProgramGLES30&>(*STATE.activeProgram);
		int fogIndex = prog.ApplyGpuProgramES30 (*STATE.activeProgramParams, STATE.activeProgramParamsBuffer.data());
		constantBuffers = &STATE.activeProgramParams->GetConstantBuffers();
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
		DBG_LOG_GLES30("  using fixed-function");
		FixedFunctionStateGLES30 ffstate;
		STATE.ComputeFixedFunctionState(ffstate, m_FogParams);
		const FixedFunctionProgramGLES30* program = GetFixedFunctionProgram(ffstate);
		program->ApplyFFGpuProgram(m_BuiltinParamValues, STATE.m_CBs);
		m_BuiltinParamIndices[kShaderVertex] = &program->GetBuiltinParams();
		constantBuffers = &program->GetConstantBuffers();

		targetCache = &program->m_UniformCache;
	}

	// Set Unity built-in parameters
	{
		Assert(m_BuiltinParamIndices[kShaderVertex]);
		const BuiltinShaderParamIndices& params = *m_BuiltinParamIndices[kShaderVertex];

		// MVP matrix
		if (params.mat[kShaderInstanceMatMVP].gpuIndex >= 0)
		{
			Matrix4x4f wvp;
			MultiplyMatrices4x4(&m_BuiltinParamValues.GetMatrixParam(kShaderMatProj), &STATE.transformState.worldViewMatrix, &wvp);

			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatMVP];
			Assert(matParam.rows == 4 && matParam.cols == 4);
			UploadUniformMatrix4(matParam, constantBuffers, wvp.GetPtr(), STATE.m_CBs);
		}
		// MV matrix
		if (params.mat[kShaderInstanceMatMV].gpuIndex >= 0)
		{
			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatMV];
			Assert(matParam.rows == 4 && matParam.cols == 4);
			UploadUniformMatrix4(matParam, constantBuffers, STATE.transformState.worldViewMatrix.GetPtr(), STATE.m_CBs);
		}
		// Transpose of MV matrix
		if (params.mat[kShaderInstanceMatTransMV].gpuIndex >= 0)
		{
			Matrix4x4f tWV;
			TransposeMatrix4x4(&STATE.transformState.worldViewMatrix, &tWV);

			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatTransMV];
			Assert(matParam.rows == 4 && matParam.cols == 4);
			UploadUniformMatrix4(matParam, constantBuffers, tWV.GetPtr(), STATE.m_CBs);
		}
		// Inverse transpose of MV matrix
		if (params.mat[kShaderInstanceMatInvTransMV].gpuIndex >= 0)
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
			UploadUniformMatrix4(matParam, constantBuffers, tInvWV.GetPtr(), STATE.m_CBs);
		}
		// M matrix
		if (params.mat[kShaderInstanceMatM].gpuIndex >= 0)
		{
			BuiltinShaderParamIndices::MatrixParamData matParam = params.mat[kShaderInstanceMatM];
			const Matrix4x4f& mat = STATE.transformState.worldMatrix;
			Assert(matParam.rows == 4 && matParam.cols == 4);
			UploadUniformMatrix4(matParam, constantBuffers, mat.GetPtr(), STATE.m_CBs);
		}
		// Inverse M matrix
		if (params.mat[kShaderInstanceMatInvM].gpuIndex >= 0)
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
			UploadUniformMatrix4(matParam, constantBuffers, inverseMat.GetPtr(), STATE.m_CBs);
		}

		// Normal matrix
		if (params.mat[kShaderInstanceMatNormalMatrix].gpuIndex >= 0)
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
			UploadUniformMatrix3(matParam, constantBuffers, rotWV33.GetPtr(), STATE.m_CBs);
		}

		// Set instance vector parameters
		for (int i = 0; i < kShaderInstanceVecCount; ++i)
		{
			int gpuIndexVS = params.vec[i].gpuIndex;
			if (gpuIndexVS >= 0)
			{
				const float* val = m_BuiltinParamValues.GetInstanceVectorParam((ShaderBuiltinInstanceVectorParam)i).GetPtr();
				switch (params.vec[i].dim) {
					case 1: CachedUniform1(targetCache, gpuIndexVS, val); break;
					case 2: CachedUniform2(targetCache, gpuIndexVS, val); break;
					case 3: CachedUniform3(targetCache, gpuIndexVS, val); break;
					case 4: CachedUniform4(targetCache, gpuIndexVS, val); break;
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
				UploadUniformMatrix4(matParam, constantBuffers, texM.GetPtr(), STATE.m_CBs);
			}
		}
	}

	// Set per-drawcall properties
	GpuProgram* subprogram = STATE.activeProgram;
	if (subprogram)
	{
		const MaterialPropertyBlock::Property* curProp = STATE.m_MaterialProperties.GetPropertiesBegin();
		const MaterialPropertyBlock::Property* propEnd = STATE.m_MaterialProperties.GetPropertiesEnd();
		const float* propBuffer = STATE.m_MaterialProperties.GetBufferBegin();
		while (curProp != propEnd)
		{
			FastPropertyName name;
			name.index = curProp->nameIndex;
			const GpuProgramParameters::ValueParameter* param = STATE.activeProgramParams->FindParam(name);
			if (param && curProp->rows == param->m_RowCount)
			{
				if (curProp->rows == 1)
				{
					const float* src = &propBuffer[curProp->offset];
					switch (param->m_ColCount) {
						case 1: CachedUniform1(targetCache, param->m_Index, src); break;
						case 2: CachedUniform2(targetCache, param->m_Index, src); break;
						case 3: CachedUniform3(targetCache, param->m_Index, src); break;
						case 4: CachedUniform4(targetCache, param->m_Index, src); break;
					}
					GLESAssert();
				}
				else if (curProp->rows == 4)
				{
					DebugAssert(curProp->cols == 4);
					const Matrix4x4f* mat = (const Matrix4x4f*)&propBuffer[curProp->offset];
					GLES_CHK(glUniformMatrix4fv (param->m_Index, 1, GL_FALSE, mat->GetPtr()));
				}
				else
				{
					AssertString("Unknown property dimensions");
				}
			}
			++curProp;
		}
	}
	STATE.m_MaterialProperties.Clear();

	STATE.m_CBs.UpdateBuffers ();
}

bool GFX_GL_IMPL::IsPositionRequiredForTexGen(int unit) const
{
	if (unit >= STATE.textureCount)
		return false;
	if (STATE.activeProgram)
		return false;

	//DebugAssertIf( unit < 0 || unit >= gGraphicsCaps.maxTexUnits);
	const TextureUnitStateGLES3& unitState = STATE.textures[unit];
	return TextureUnitStateGLES3::PositionRequiredForTexGen(unitState.texGen);
}

bool GFX_GL_IMPL::IsNormalRequiredForTexGen(int unit) const
{
	if (unit >= STATE.textureCount)
		return false;
	if (STATE.activeProgram)
		return false;

	//DebugAssertIf( unit < 0 || unit >= gGraphicsCaps.maxTexUnits );
	const TextureUnitStateGLES3& unitState = STATE.textures[unit];
	return TextureUnitStateGLES3::NormalRequiredForTexGen(unitState.texGen);
}

bool GFX_GL_IMPL::IsPositionRequiredForTexGen() const
{
	return ( STATE.positionTexGen != 0 && !STATE.activeProgram );
}

bool GFX_GL_IMPL::IsNormalRequiredForTexGen() const
{
	return ( STATE.normalTexGen != 0 && !STATE.activeProgram );
}

void* GFX_GL_IMPL::GetNativeTexturePointer(TextureID id)
{
	return (void*)TextureIdMap::QueryNativeTexture(id);
}

void GFX_GL_IMPL::ReloadResources()
{
	// Buffers in BufferManager must be cleared before recreating VBOs.
	GetBufferManagerGLES30()->InvalidateAll();

	RecreateAllVBOs();
	GfxDevice::CommonReloadResources(kReleaseRenderTextures | kReloadShaders | kReloadTextures);
	ClearFixedFunctionPrograms();

	if (STATE.m_fboManager)
		STATE.m_fboManager->InvalidateObjects();

	InvalidateState();
}

// GPU skinning functionality
GPUSkinningInfo * GFX_GL_IMPL::CreateGPUSkinningInfo()
{
	if (gGraphicsCaps.gles30.useTFSkinning)
		return new TransformFeedbackSkinningInfo();
	else
		return 0;
}

void	GFX_GL_IMPL::DeleteGPUSkinningInfo(GPUSkinningInfo *info)
{
	delete reinterpret_cast<TransformFeedbackSkinningInfo *>(info);
}

// All actual functionality is performed in TransformFeedbackSkinningInfo, just forward the calls
void GFX_GL_IMPL::SkinOnGPU( GPUSkinningInfo * info, bool lastThisFrame )
{
	reinterpret_cast<TransformFeedbackSkinningInfo *>(info)->SkinMesh(lastThisFrame);
}

void GFX_GL_IMPL::UpdateSkinSourceData(GPUSkinningInfo *info, const void *vertData, const BoneInfluence *skinData, bool dirty)
{
	reinterpret_cast<TransformFeedbackSkinningInfo *>(info)->UpdateSourceData(vertData, skinData, dirty);
}

void GFX_GL_IMPL::UpdateSkinBonePoses(GPUSkinningInfo *info, const int boneCount, const Matrix4x4f* poses)
{
	reinterpret_cast<TransformFeedbackSkinningInfo *>(info)->UpdateSourceBones(boneCount, poses);
}

// Acquire thread ownership on the calling thread. Worker releases ownership.
void GFX_GL_IMPL::AcquireThreadOwnership()
{
	AcquireGLES30Context();
}

// Release thread ownership on the calling thread. Worker acquires ownership.
void GFX_GL_IMPL::ReleaseThreadOwnership()
{
	ReleaseGLES30Context();
}



// ---------- verify state
#if GFX_DEVICE_VERIFY_ENABLE
void GFX_GL_IMPL::VerifyState()
{
}

#endif // GFX_DEVICE_VERIFY_ENABLE

#endif // GFX_SUPPORTS_OPENGLES30
