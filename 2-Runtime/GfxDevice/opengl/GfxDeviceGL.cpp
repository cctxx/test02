#include "UnityPrefix.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "UnityGL.h"
#include "GLContext.h"
#include "TexturesGL.h"
#include "TimerQueryGL.h"
#include "ARBVBO.h"
#include "NullVBO.h"
#include "CombinerGL.h"
#include "External/shaderlab/Library/program.h"
#include "External/shaderlab/Library/texenv.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "ChannelsGL.h"
#include "Runtime/Threads/AtomicOps.h"
#include "Runtime/Allocator/LinearAllocator.h"
#include "GpuProgramsGL.h"
#include "ArbGpuProgamGL.h"
#include "TextureIdMapGL.h"
#include "Runtime/Graphics/GraphicsHelper.h"
#include "Runtime/GfxDevice/GpuProgramParamsApply.h"
#if UNITY_EDITOR && UNITY_WIN
#include "GLWindow.h"
#endif
#include "Runtime/Misc/Plugins.h"


void InvalidateChannelStateGL(); // ChannelsGL.cpp
void InvalidateFPParamCacheGL(); // GpuProgramsGL.cpp
bool IsActiveRenderTargetWithColorGL(); // RenderTextureGL.cpp

namespace ShaderLab {
	TexEnv* GetTexEnvForBinding( const TextureBinding& binding, const PropertySheet* props ); // pass.cpp
}

static const unsigned int kBlendModeGL[] = {
	GL_ZERO, GL_ONE, GL_DST_COLOR, GL_SRC_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR,
	GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE, GL_ONE_MINUS_SRC_ALPHA,
};

static const unsigned int kBlendFuncGL[] = {
	GL_FUNC_ADD, GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX,
};

static const unsigned int kCmpFuncGL[] = {
	GL_NEVER, GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS
};

static const unsigned int kStencilOpGL[] = {
	GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR,
	GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP
};

static const GLenum kColorMatModeGL[kColorMatTypeCount] = { 0, GL_EMISSION, GL_AMBIENT_AND_DIFFUSE };

static const unsigned long kTexDimTableGL[kTexDimCount] = {0, GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP_ARB, 0};

static GLenum kWrapModeGL[kTexWrapCount] = { GL_REPEAT, GL_CLAMP_TO_EDGE };

static const GLint kMinFilterGL[kTexFilterCount] = { GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR };

static const unsigned int kFogModeGL[kFogModeCount] = { GL_LINEAR, GL_LINEAR, GL_EXP, GL_EXP2 };

GLenum kTopologyGL[kPrimitiveTypeCount] = { GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_QUADS, GL_LINES, GL_LINE_STRIP, GL_POINTS };


GfxThreadableDevice* CreateGLGfxDevice();
const unsigned long* GetGLTextureDimensionTable();
unsigned int GetGLShaderImplTarget( ShaderImplType implType );
void InvalidateActiveShaderStateGL( ShaderType type );

#if SUPPORT_REPRODUCE_LOG_GFX_TRACE
#include "Runtime/Misc/ReproductionLog.h"
#define GFX_LOG(pattern) LogToScreenshotLog(__FUNCTION__+(" "+Format pattern))
#else
#define GFX_LOG(pattern)
#endif


static inline void ActivateTextureUnitGL (int unit)
{
	// When using the fixed-function texture, the correct assert is:
	//Assert(unit >= 0 && unit < gGraphicsCaps.maxTexUnits);
	
	Assert(unit >= 0 && unit < gGraphicsCaps.maxTexImageUnits);
	
	GFX_LOG(("%d", unit));
	if (gGraphicsCaps.maxTexImageUnits > 1)
		OGL_CALL(glActiveTextureARB(GL_TEXTURE0_ARB + unit));
}


// --------------------------------------------------------------------------

struct DeviceBlendStateGL : public DeviceBlendState
{
	UInt32	alphaFunc;
};


struct DeviceDepthStateGL : public DeviceDepthState
{
	UInt32		depthFunc;
};

struct DeviceStencilStateGL : public DeviceStencilState
{
	GLenum	stencilFuncFront;
	GLenum	stencilFailOpFront;
	GLenum	depthFailOpFront;
	GLenum	depthPassOpFront;
	GLenum	stencilFuncBack;
	GLenum	stencilFailOpBack;
	GLenum	depthFailOpBack;
	GLenum	depthPassOpBack;
};



typedef std::map< GfxBlendState, DeviceBlendStateGL,  memcmp_less<GfxBlendState> > CachedBlendStates;
typedef std::map< GfxDepthState, DeviceDepthStateGL,  memcmp_less<GfxDepthState> > CachedDepthStates;
typedef std::map< GfxStencilState, DeviceStencilStateGL,  memcmp_less<GfxStencilState> > CachedStencilStates;
typedef std::map< GfxRasterState, DeviceRasterState,  memcmp_less<GfxRasterState> > CachedRasterStates;


// --------------------------------------------------------------------------


struct TextureUnitStateGL
{
	GLuint				texID;
	TextureDimension	texDim;
	unsigned int		combColor, combAlpha;
	Vector4f			color;
	TexGenMode			texGen;
	float				bias;

	void	Invalidate();
	void	Verify( int unit );
};

void TextureUnitStateGL::Invalidate()
{
	texID = -1;
	texDim = kTexDimUnknown;
	combColor = combAlpha = 0xFFFFFFFF;
	color.Set( -1, -1, -1, -1 );
	texGen = kTexGenUnknown;
	bias = 1.0e6f;
}

// --------------------------------------------------------------------------


struct VertexLightStateGL
{
	int		enabled; // 0/1 or -1 if unknown
	float	attenQuad;
	float	spotAngle;

	void	Invalidate();
	void	Verify( int unit );
};

void VertexLightStateGL::Invalidate()
{
	enabled = -1;
	attenQuad = -1.0f;
	spotAngle = -1000.0f; // -1.0f is used to flag non-spot lights, so it would be a valid value
}

// --------------------------------------------------------------------------


struct DeviceStateGL
{
	GLuint			m_SharedFBO;
	GLuint			m_HelperFBO;
	int				m_TextureIDGenerator;

	int				depthFunc;
	int				depthWrite; // 0/1 or -1

	int				blending;
	int				srcBlend, destBlend, srcBlendAlpha, destBlendAlpha; // GL modes
	int				blendOp, blendOpAlpha; // GL modes
	CompareFunction alphaFunc;
	float			alphaValue;
	int				alphaToMask;

	CullMode		culling;
	bool			appBackfaceMode, userBackfaceMode;
	bool			wireframe;
	NormalizationMode	normalization;
	int				scissor;


	int				lighting;
	Vector4f		matDiffuse, matAmbient, matSpecular, matEmissive;
	Vector4f		ambient;
	float			matShininess;
	ColorMaterialMode	colorMaterial;

	float offsetFactor, offsetUnits;

	GpuProgram* activeGpuProgram[kShaderTypeCount];
	const GpuProgramParameters* activeGpuProgramParams[kShaderTypeCount];
	ShaderImplType shaderEnabledImpl[kShaderTypeCount];
	int	shaderEnabledID[kShaderTypeCount];

	int separateSpecular;

	int				colorWriteMask; // ColorWriteMask combinations

	TextureUnitStateGL	textures[kMaxSupportedTextureUnits];
	VertexLightStateGL	vertexLights[kMaxSupportedVertexLights];

	DynamicVBO*	m_DynamicVBO;

	int			viewport[4];
	int			scissorRect[4];

	GfxPrimitiveType	m_ImmediateMode;
	dynamic_array<UInt8>	m_ImmediateVertices;

	#if GFX_USES_VIEWPORT_OFFSET
	float	viewportOffsetX, viewportOffsetY;
	#endif

	CachedBlendStates			m_CachedBlendStates;
	CachedDepthStates			m_CachedDepthStates;
	CachedStencilStates			m_CachedStencilStates;
	CachedRasterStates			m_CachedRasterStates;

	DeviceBlendStateGL*			m_CurrBlendState;
	DeviceDepthStateGL*			m_CurrDepthState;
	const DeviceStencilStateGL*	m_CurrStencilState;
	int							m_StencilRef;
	DeviceRasterState*			m_CurrRasterState;
	Matrix4x4f					m_WorldMatrix;

	void	Invalidate(BuiltinShaderParamValues& builtins);
	void	Verify();
};

static void ApplyBackfaceMode( const DeviceStateGL& state );


void DeviceStateGL::Invalidate (BuiltinShaderParamValues& builtins)
{
	int i;

	depthFunc = kFuncUnknown;
	depthWrite = -1;

	blending = -1; // unknown
	srcBlend = destBlend = srcBlendAlpha = destBlendAlpha = -1; // won't match any GL mode
	blendOp = blendOpAlpha = -1; // won't match any GL mode
	alphaFunc = kFuncUnknown;
	alphaValue = -1.0f;
	alphaToMask = -1; // unknown

	culling = kCullUnknown;
	normalization = kNormalizationUnknown;
	scissor = -1;

	lighting = -1;
	matDiffuse.Set( -1, -1, -1, -1 );
	matAmbient.Set( -1, -1, -1, -1 );
	matSpecular.Set( -1, -1, -1, -1 );
	matEmissive.Set( -1, -1, -1, -1 );
	ambient.Set( -1, -1, -1, -1 );
	matShininess = -1.0f;
	colorMaterial = kColorMatUnknown;

	offsetFactor = offsetUnits = -1000.0f;
	for( i = 0; i < kShaderTypeCount; ++i )
	{
		activeGpuProgram[i] = NULL;
		activeGpuProgramParams[i] = NULL;
		shaderEnabledImpl[i] = kShaderImplUndefined;
		shaderEnabledID[i] = -1;
	}

	separateSpecular = -1;

	colorWriteMask = -1; // TBD ?

	m_StencilRef = -1;

	for( i = 0; i < kMaxSupportedTextureUnits; ++i )
		textures[i].Invalidate();
	for( i = 0; i < kMaxSupportedVertexLights; ++i )
		vertexLights[i].Invalidate();
	InvalidateChannelStateGL();
	InvalidateFPParamCacheGL();

	// make sure we aren't using any programs
	if( gGraphicsCaps.gl.hasGLSL )
		glUseProgramObjectARB( 0 );
	glDisable( GL_VERTEX_PROGRAM_ARB );
	glDisable( GL_FRAGMENT_PROGRAM_ARB );

	// misc. state
	glHint (GL_FOG_HINT, GL_NICEST);
	glLightModelf (GL_LIGHT_MODEL_LOCAL_VIEWER, 1.0F);

	// Setup GL_EYE_PLANE for R coordinate to reasonable value. This has to be done initially,
	// and NOT when some arbitrary matrix is already set up. So it's important
	// to load identity matrix here and only then setup GL_EYE_PLANE value.
	glLoadIdentity();
	m_WorldMatrix.SetIdentity();
	builtins.GetWritableMatrixParam(kShaderMatView).SetIdentity();
	builtins.GetWritableMatrixParam(kShaderMatProj).SetIdentity();
	builtins.GetWritableMatrixParam(kShaderMatViewProj).SetIdentity();
	const float zplane[4] = {0.0f,0.0f,1.0f,0.0f};
	for( i = 0; i < gGraphicsCaps.maxTexUnits; ++i )
	{
		ActivateTextureUnitGL (i);
		glTexGenfv (GL_R, GL_EYE_PLANE, zplane);
	}

	// make sure backface mode is in synch
	ApplyBackfaceMode( *this );

	// make sure no vertex buffers are bound
	UnbindVertexBuffersGL();

	m_ImmediateMode = kPrimitiveTriangles;
	m_ImmediateVertices.clear();

	#if GFX_DEVICE_VERIFY_ENABLE
	Verify();
	#endif
}

#include "GfxDeviceGL.h"

// GLContext.cpp
#if UNITY_WIN
bool CreateMasterGraphicsContext();
#endif
void CleanupMasterContext();


GfxThreadableDevice* CreateGLGfxDevice()
{
	#if UNITY_WIN
	SetMasterContextClassName(L"WindowGLClassName");
	if( !CreateMasterGraphicsContext() )
		return NULL;
	#endif
	GraphicsContextHandle context = GetMasterGraphicsContext();
	if( !context.IsValid() )
		return NULL;

	SetMainGraphicsContext( context );
	ActivateGraphicsContext (context, true);

	InitGLExtensions();
	gGraphicsCaps.InitGL();
	GLAssert();

	return UNITY_NEW_AS_ROOT(GFX_GL_IMPL(), kMemGfxDevice, "GLGfxDevice", "");
}


GFX_GL_IMPL::GFX_GL_IMPL()
{
	#if !GFX_DEVICE_VIRTUAL
	impl = new GfxDeviceImpl();
	#endif
	STATE.m_SharedFBO = 0;
	STATE.m_HelperFBO = 0;
	STATE.m_TextureIDGenerator = 0;
	STATE.appBackfaceMode = false;
	STATE.userBackfaceMode = false;
	STATE.m_DynamicVBO = NULL;
	STATE.wireframe = false;
	#if GFX_USES_VIEWPORT_OFFSET
	STATE.viewportOffsetX = 0;
	STATE.viewportOffsetY = 0;
	#endif

	OnCreate();
	InvalidateState();

	m_Renderer = kGfxRendererOpenGL;
	m_UsesOpenGLTextureCoords = true;
	m_UsesHalfTexelOffset = false;
	// Should be safe to assume we can get 24 bits for the framebuffer on desktop.
	m_FramebufferDepthFormat = kDepthFormat24;
	m_IsThreadable = true;

	STATE.m_CurrBlendState = NULL;
	STATE.m_CurrDepthState = NULL;
	STATE.m_CurrStencilState = NULL;
	STATE.m_CurrRasterState = NULL;

	STATE.m_WorldMatrix.SetIdentity();

	m_MaxBufferedFrames = -1; // no limiting

	STATE.viewport[0] = STATE.viewport[1] = STATE.viewport[2] = STATE.viewport[3] = 0;
	STATE.scissorRect[0] = STATE.scissorRect[1] = STATE.scissorRect[2] = STATE.scissorRect[3] = 0;

	extern RenderSurfaceBase* CreateBackBufferColorSurfaceGL();
	SetBackBufferColorSurface(CreateBackBufferColorSurfaceGL());

	extern RenderSurfaceBase* CreateBackBufferDepthSurfaceGL();
	SetBackBufferDepthSurface(CreateBackBufferDepthSurfaceGL());
}

GFX_GL_IMPL::~GFX_GL_IMPL()
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_WORKER
	PluginsSetGraphicsDevice (NULL, kGfxRendererOpenGL, kGfxDeviceEventShutdown);
#endif
	if (STATE.m_SharedFBO)
		glDeleteFramebuffersEXT (1, &STATE.m_SharedFBO);
	if (STATE.m_HelperFBO)
		glDeleteFramebuffersEXT (1, &STATE.m_HelperFBO);
	delete STATE.m_DynamicVBO;

	#if !GFX_DEVICE_VIRTUAL
	delete impl;
	#endif

	#if UNITY_WIN
	CleanupMasterContext();
	#endif
	CleanupGLExtensions();
}

void GFX_GL_IMPL::OnDeviceCreated (bool callingFromRenderThread)
{
	// needs to activate graphics context on both main & render threads
	ActivateGraphicsContext (GetMainGraphicsContext(), true);
}

void GFX_GL_IMPL::InvalidateState()
{
	m_FogParams.Invalidate();
	STATE.Invalidate(m_BuiltinParamValues);

	// Disable fog initially. At least on Mac/Intel, initial state seems to be off
	// for fixed function (correct), but on for fragment/vertex programs (incorrect).
	glFogf( GL_FOG_DENSITY, 0.0f );
	glDisable( GL_FOG );
	m_FogParams.mode = kFogDisabled;
	m_FogParams.density = 0.0f;
}


const int kFPParamCacheSize = 32; // don't make this larger than 32 (won't fit into s_FPParamCacheValid mask)
extern UInt32 s_FPParamCacheValid;
extern Vector4f s_FPParamCache[kFPParamCacheSize];


#define SET_LOCAL_MATRIX_PARAM( type, index, ptr )												\
	OGL_CALL(glProgramLocalParameter4fARB( type, index+0, ptr[0], ptr[4], ptr[8], ptr[12] ));	\
	OGL_CALL(glProgramLocalParameter4fARB( type, index+1, ptr[1], ptr[5], ptr[9], ptr[13] ));	\
	OGL_CALL(glProgramLocalParameter4fARB( type, index+2, ptr[2], ptr[6], ptr[10], ptr[14] ));	\
	OGL_CALL(glProgramLocalParameter4fARB( type, index+3, ptr[3], ptr[7], ptr[11], ptr[15] ));	\


#define SET_ENV_MATRIX_PARAM( type, index, ptr )												\
	OGL_CALL(glProgramEnvParameter4fARB( type, index+0, ptr[0], ptr[4], ptr[8], ptr[12] ));		\
	OGL_CALL(glProgramEnvParameter4fARB( type, index+1, ptr[1], ptr[5], ptr[9], ptr[13] ));		\
	OGL_CALL(glProgramEnvParameter4fARB( type, index+2, ptr[2], ptr[6], ptr[10], ptr[14] ));	\
	OGL_CALL(glProgramEnvParameter4fARB( type, index+3, ptr[3], ptr[7], ptr[11], ptr[15] ));	\


static void GLSetShaderMatrixConstant (ShaderImplType type, int index, int rows, int cols, const float* ptr)
{
	if (type == kShaderImplVertex)
	{
		SET_LOCAL_MATRIX_PARAM(GL_VERTEX_PROGRAM_ARB, index, ptr);
	}
	else if (type == kShaderImplFragment)
	{
		if (gGraphicsCaps.gl.cacheFPParamsWithEnvs)
		{
			// don't care here about cache
			UInt32 mask0 = index+0 < kFPParamCacheSize ? 1 << (index+0) : 0;
			UInt32 mask1 = index+1 < kFPParamCacheSize ? 1 << (index+1) : 0;
			UInt32 mask2 = index+2 < kFPParamCacheSize ? 1 << (index+2) : 0;
			UInt32 mask3 = index+3 < kFPParamCacheSize ? 1 << (index+3) : 0;

			s_FPParamCacheValid &= ~mask0;
			s_FPParamCacheValid &= ~mask1;
			s_FPParamCacheValid &= ~mask2;
			s_FPParamCacheValid &= ~mask3;

			SET_ENV_MATRIX_PARAM(GL_FRAGMENT_PROGRAM_ARB, index, ptr);
		}
		else
		{
			SET_LOCAL_MATRIX_PARAM(GL_FRAGMENT_PROGRAM_ARB, index, ptr);
		}
	}
	else if (type == kShaderImplBoth)
	{
		Assert(rows == 4 && cols == 4);
		OGL_CALL(glUniformMatrix4fvARB(index, 1, false, ptr));
	}
	else
	{
		AssertString("unknown shader type");
	}
}


#undef SET_ENV_MATRIX_PARAM
#undef SET_LOCAL_MATRIX_PARAM

static void GLSetShaderVectorConstant (ShaderImplType shaderType, ShaderParamType type, int index, int dim, const Vector4f& val)
{
	if (shaderType == kShaderImplVertex)
	{
		OGL_CALL(glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, index, val.GetPtr()));
	}
	else if (shaderType == kShaderImplFragment)
	{
		if (gGraphicsCaps.gl.cacheFPParamsWithEnvs)
		{
			UInt32 mask = 1 << index;
			if (index >= kFPParamCacheSize)
			{
				OGL_CALL(glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, index, val.GetPtr()));
			}
			else
			{
				if( !(s_FPParamCacheValid & mask) || s_FPParamCache[index] != val )
				{
					OGL_CALL(glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, index, val.GetPtr()));
					s_FPParamCache[index] = val;
				}
				s_FPParamCacheValid |= mask;
			}
		}
		else
			OGL_CALL(glProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, index, val.GetPtr()));
	}
	else if (shaderType == kShaderImplBoth)
	{
		if (type == kShaderParamFloat)
		{
			switch (dim) {
			case 1: OGL_CALL(glUniform1fvARB(index, 1, val.GetPtr())); break;
			case 2: OGL_CALL(glUniform2fvARB(index, 1, val.GetPtr())); break;
			case 3: OGL_CALL(glUniform3fvARB(index, 1, val.GetPtr())); break;
			case 4: OGL_CALL(glUniform4fvARB(index, 1, val.GetPtr())); break;
			default: AssertString ("unknown uniform dimension"); break;
			}
		}
		else
		{
			// In theory Uniform*f can also be used to load bool uniforms, in practice
			// some drivers don't like that. So load both integers and bools via *i functions.
			int ival[4] = {val.x, val.y, val.z, val.w};
			switch (dim) {
			case 1: OGL_CALL(glUniform1ivARB(index, 1, ival)); break;
			case 2: OGL_CALL(glUniform2ivARB(index, 1, ival)); break;
			case 3: OGL_CALL(glUniform3ivARB(index, 1, ival)); break;
			case 4: OGL_CALL(glUniform4ivARB(index, 1, ival)); break;
			default: AssertString ("unknown uniform dimension"); break;
			}
		}
	}
	else
	{
		AssertString("unknown shader type");
	}
}


struct SetValuesFunctorGL
{
	SetValuesFunctorGL(GfxDevice& device, const ShaderImplType* shaderEnabledImpl) : m_Device(device), m_ShaderEnabledImpl(shaderEnabledImpl) { }
	GfxDevice& m_Device;
	const ShaderImplType* m_ShaderEnabledImpl;
	void SetVectorVal (ShaderType shaderType, ShaderParamType type, int index, const float* ptr, int cols, const GpuProgramParameters& params, int cbIndex)
	{
		GLSetShaderVectorConstant (m_ShaderEnabledImpl[shaderType], type, index, cols, *(const Vector4f*)ptr);
	}
	void SetMatrixVal (ShaderType shaderType, int index, const Matrix4x4f* ptr, int rows, const GpuProgramParameters& params, int cbIndex)
	{
		GLSetShaderMatrixConstant (m_ShaderEnabledImpl[shaderType], index, rows, 4, (const float*)ptr);
	}
	void SetTextureVal (ShaderType shaderType, int index, int samplerIndex, TextureDimension dim, TextureID texID)
	{
		UInt32 texIndex = UInt32(index) >> 24;
		m_Device.SetTexture (shaderType, texIndex, samplerIndex, texID, dim, std::numeric_limits<float>::infinity());
	}
};


void GFX_GL_IMPL::BeforeDrawCall( bool immediateMode )
{
	GFX_LOG(("%d",immediateMode));

	// Special Matrix parameters (defined by Unity as built-ins
	// but not present in GL default state)
	const BuiltinShaderParamIndices& paramsVS = *m_BuiltinParamIndices[kShaderVertex];
	const BuiltinShaderParamIndices& paramsFS = *m_BuiltinParamIndices[kShaderFragment];

	// M matrix
	if (paramsVS.mat[kShaderInstanceMatM].gpuIndex >= 0)
	{
		BuiltinShaderParamIndices::MatrixParamData matParam = paramsVS.mat[kShaderInstanceMatM];
		const Matrix4x4f& mat = STATE.m_WorldMatrix;
		Assert(matParam.rows == 4 && matParam.cols == 4);
		GLSetShaderMatrixConstant (STATE.shaderEnabledImpl[kShaderVertex], matParam.gpuIndex, matParam.rows, matParam.cols, mat.GetPtr());
	}
	if (paramsFS.mat[kShaderInstanceMatM].gpuIndex >= 0)
	{
		BuiltinShaderParamIndices::MatrixParamData matParam = paramsFS.mat[kShaderInstanceMatM];
		const Matrix4x4f& mat = STATE.m_WorldMatrix;
		Assert(matParam.rows == 4 && matParam.cols == 4);
		GLSetShaderMatrixConstant (STATE.shaderEnabledImpl[kShaderFragment], matParam.gpuIndex, matParam.rows, matParam.cols, mat.GetPtr());
	}
	// Inverse M matrix
	if (paramsVS.mat[kShaderInstanceMatInvM].gpuIndex >= 0 || paramsFS.mat[kShaderInstanceMatInvM].gpuIndex >= 0)
	{
		Matrix4x4f mat = STATE.m_WorldMatrix;
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
		if (paramsVS.mat[kShaderInstanceMatInvM].gpuIndex >= 0)
		{
			BuiltinShaderParamIndices::MatrixParamData matParam = paramsVS.mat[kShaderInstanceMatInvM];
			Assert(matParam.rows == 4 && matParam.cols == 4);
			GLSetShaderMatrixConstant (STATE.shaderEnabledImpl[kShaderVertex], matParam.gpuIndex, matParam.rows, matParam.cols, inverseMat.GetPtr());
		}
		if (paramsFS.mat[kShaderInstanceMatInvM].gpuIndex >= 0)
		{
			BuiltinShaderParamIndices::MatrixParamData matParam = paramsFS.mat[kShaderInstanceMatInvM];
			Assert(matParam.rows == 4 && matParam.cols == 4);
			GLSetShaderMatrixConstant (STATE.shaderEnabledImpl[kShaderFragment], matParam.gpuIndex, matParam.rows, matParam.cols, inverseMat.GetPtr());
		}
	}

	// Set instance vector parameters
	for (int i = 0; i < kShaderInstanceVecCount; ++i)
	{
		const BuiltinShaderParamIndices::VectorParamData& parVS = paramsVS.vec[i];
		if (parVS.gpuIndex >= 0)
			GLSetShaderVectorConstant (STATE.shaderEnabledImpl[kShaderVertex], kShaderParamFloat, parVS.gpuIndex, parVS.dim, m_BuiltinParamValues.GetInstanceVectorParam((ShaderBuiltinInstanceVectorParam)i));
		const BuiltinShaderParamIndices::VectorParamData& parFS = paramsFS.vec[i];
		if (parFS.gpuIndex >= 0)
			GLSetShaderVectorConstant (STATE.shaderEnabledImpl[kShaderFragment], kShaderParamFloat, parFS.gpuIndex, parFS.dim, m_BuiltinParamValues.GetInstanceVectorParam((ShaderBuiltinInstanceVectorParam)i));
	}

	SetValuesFunctorGL setValuesFunc(*this, STATE.shaderEnabledImpl);
	ApplyMaterialPropertyBlockValues(m_MaterialProperties, STATE.activeGpuProgram, STATE.activeGpuProgramParams, setValuesFunc);
}


DeviceBlendState* GFX_GL_IMPL::CreateBlendState(const GfxBlendState& state)
{

	std::pair<CachedBlendStates::iterator, bool> result = STATE.m_CachedBlendStates.insert(std::make_pair(state, DeviceBlendStateGL()));
	if (!result.second)
		return &result.first->second;

	DeviceBlendStateGL& glstate = result.first->second;
	memcpy(&glstate.sourceState, &state, sizeof(GfxBlendState));
	DebugAssertIf(kFuncUnknown==state.alphaTest);
	glstate.alphaFunc = kCmpFuncGL[state.alphaTest];

	return &result.first->second;
}


DeviceDepthState* GFX_GL_IMPL::CreateDepthState(const GfxDepthState& state)
{
	std::pair<CachedDepthStates::iterator, bool> result = STATE.m_CachedDepthStates.insert(std::make_pair(state, DeviceDepthStateGL()));
	if (!result.second)
		return &result.first->second;

	DeviceDepthStateGL& glstate = result.first->second;
	memcpy(&glstate.sourceState, &state, sizeof(GfxDepthState));
	glstate.depthFunc = kCmpFuncGL[state.depthFunc];
	return &result.first->second;
}

DeviceStencilState* GFX_GL_IMPL::CreateStencilState(const GfxStencilState& state)
{
	std::pair<CachedStencilStates::iterator, bool> result = STATE.m_CachedStencilStates.insert(std::make_pair(state, DeviceStencilStateGL()));
	if (!result.second)
		return &result.first->second;

	DeviceStencilStateGL& st = result.first->second;
	memcpy (&st.sourceState, &state, sizeof(state));
	st.stencilFuncFront = kCmpFuncGL[state.stencilFuncFront];
	st.stencilFailOpFront = kStencilOpGL[state.stencilFailOpFront];
	st.depthFailOpFront = kStencilOpGL[state.stencilZFailOpFront];
	st.depthPassOpFront = kStencilOpGL[state.stencilPassOpFront];
	st.stencilFuncBack = kCmpFuncGL[state.stencilFuncBack];
	st.stencilFailOpBack = kStencilOpGL[state.stencilFailOpBack];
	st.depthFailOpBack = kStencilOpGL[state.stencilZFailOpBack];
	st.depthPassOpBack = kStencilOpGL[state.stencilPassOpBack];
	return &result.first->second;
}


DeviceRasterState* GFX_GL_IMPL::CreateRasterState(const GfxRasterState& state)
{
	std::pair<CachedRasterStates::iterator, bool> result = STATE.m_CachedRasterStates.insert(std::make_pair(state, DeviceRasterState()));
	if (!result.second)
		return &result.first->second;

	DeviceRasterState& glstate = result.first->second;
	memcpy(&glstate.sourceState, &state, sizeof(DeviceRasterState));

	return &result.first->second;
}

void GFX_GL_IMPL::SetBlendState(const DeviceBlendState* state, float alphaRef)
{
	DeviceBlendStateGL* devstate = (DeviceBlendStateGL*)state;

	if (STATE.m_CurrBlendState == devstate && alphaRef == STATE.alphaValue)
		return;

	STATE.m_CurrBlendState = devstate;
	if (!STATE.m_CurrBlendState)
		return;

	int mask = devstate->sourceState.renderTargetWriteMask;
	if (!IsActiveRenderTargetWithColorGL())
		mask = 0;
	if( mask != STATE.colorWriteMask ) {
		OGL_CALL(glColorMask( (mask & kColorWriteR) != 0, (mask & kColorWriteG) != 0, (mask & kColorWriteB) != 0, (mask & kColorWriteA) != 0 ));
		STATE.colorWriteMask = mask;
	}

	const GfxBlendState& desc = state->sourceState;
	const CompareFunction mode = state->sourceState.alphaTest;
	const GLenum glsrc = kBlendModeGL[desc.srcBlend];
	const GLenum gldst = kBlendModeGL[desc.dstBlend];
	const GLenum glsrca = kBlendModeGL[desc.srcBlendAlpha];
	const GLenum gldsta = kBlendModeGL[desc.dstBlendAlpha];
	const GLenum glfunc = kBlendFuncGL[desc.blendOp];
	const GLenum glfunca = kBlendFuncGL[desc.blendOpAlpha];
	const bool blendDisabled = (glsrc == GL_ONE && gldst == GL_ZERO && glsrca == GL_ONE && gldsta == GL_ZERO);

	GFX_LOG(("%d, %d, %d, %f", desc.srcBlend, desc.dstBlend, mode, alphaRef));

	// alpha blending states

	if( blendDisabled )
	{
		if( STATE.blending != 0 )
		{
			OGL_CALL(glDisable (GL_BLEND));
			STATE.blending = 0;
		}
	}
	else
	{
		if( glsrc != STATE.srcBlend || gldst != STATE.destBlend || glsrca != STATE.srcBlendAlpha || gldsta != STATE.destBlendAlpha )
		{
			if (gGraphicsCaps.hasSeparateAlphaBlend)
				OGL_CALL(glBlendFuncSeparateEXT (glsrc, gldst, glsrca, gldsta));
			else
				OGL_CALL(glBlendFunc( glsrc, gldst ));
			STATE.srcBlend = glsrc;
			STATE.destBlend = gldst;
			STATE.srcBlendAlpha = glsrca;
			STATE.destBlendAlpha = gldsta;
		}
		if (glfunc != STATE.blendOp || glfunca != STATE.blendOpAlpha)
		{
			bool supports = true;
			if( (glfunc == GL_FUNC_SUBTRACT || glfunc == GL_FUNC_REVERSE_SUBTRACT) && !gGraphicsCaps.hasBlendSub )
				supports = false;
			if( (glfunca == GL_FUNC_SUBTRACT || glfunca == GL_FUNC_REVERSE_SUBTRACT) && !gGraphicsCaps.hasBlendSub )
				supports = false;
			if( (glfunc == GL_MIN || glfunc == GL_MAX) && !gGraphicsCaps.hasBlendMinMax )
				supports = false;
			if( (glfunca == GL_MIN || glfunca == GL_MAX) && !gGraphicsCaps.hasBlendMinMax )
				supports = false;

			if(supports)
			{
			if (gGraphicsCaps.hasSeparateAlphaBlend)
				OGL_CALL(glBlendEquationSeparateEXT (glfunc, glfunca));
			else
				OGL_CALL(glBlendEquation (glfunc));
			STATE.blendOp = glfunc;
			STATE.blendOpAlpha = glfunca;
		}
		}
		if( STATE.blending != 1 )
		{
			OGL_CALL(glEnable( GL_BLEND ));
			STATE.blending = 1;
		}
	}

	// alpha testing states
#if UNITY_EDITOR // gles2.0 doesn't have FF alpha testing(only discard/clip on shader side), so disable on editor while emulating
	bool skipAlphaTestFF = (gGraphicsCaps.IsEmulatingGLES20() && IsShaderActive(kShaderFragment));
	// possible that vertex shader will be used with FF "frag shader" (like Transparent/vertexlit.shader),
	// which will change alphatesting. So later on when real frag shaders come, we need to force disable alpha
	// testing or enjoy nasty artefacts (like active alpha testing messing up the whole scene).
	if ( skipAlphaTestFF && STATE.alphaFunc!=kFuncDisabled )
	{
		OGL_CALL(glDisable (GL_ALPHA_TEST));
		STATE.alphaFunc = kFuncDisabled;
	}

	if ( !skipAlphaTestFF )
	{
#endif
		if( mode != STATE.alphaFunc || alphaRef != STATE.alphaValue )
		{
			if( mode != kFuncDisabled )
			{
				OGL_CALL(glAlphaFunc( kCmpFuncGL[mode], alphaRef ));
				OGL_CALL(glEnable( GL_ALPHA_TEST ));
			}
			else
			{
				OGL_CALL(glDisable (GL_ALPHA_TEST));
			}

			STATE.alphaFunc = mode;
			STATE.alphaValue = alphaRef;
		}
#if UNITY_EDITOR
	}
#endif

#if 0
	int atomask = alphaToMask ? 1 : 0;
	if( atomask != STATE.alphaToMask && gGraphicsCaps.hasMultiSample )
	{
		if( atomask )
			OGL_CALL(glEnable( GL_SAMPLE_ALPHA_TO_COVERAGE ));
		else
			OGL_CALL(glDisable( GL_SAMPLE_ALPHA_TO_COVERAGE ));
		STATE.alphaToMask = atomask;
	}
#endif
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
			OGL_CALL(glDisable (GL_CULL_FACE));
			break;
		case kCullFront:
			OGL_CALL(glCullFace (GL_FRONT));
			OGL_CALL(glEnable (GL_CULL_FACE));
			break;
		case kCullBack:
			OGL_CALL(glCullFace (GL_BACK));
			OGL_CALL(glEnable (GL_CULL_FACE));
			break;
		}
		STATE.culling = cull;
	}

	float zFactor = devstate->sourceState.slopeScaledDepthBias;
	float zUnits  = devstate->sourceState.depthBias;
	if( zFactor != STATE.offsetFactor || zUnits != STATE.offsetUnits )
	{
		OGL_CALL(glPolygonOffset( zFactor, zUnits ));
		if( zFactor || zUnits )
			OGL_CALL(glEnable (GL_POLYGON_OFFSET_FILL));
		else
			OGL_CALL(glDisable (GL_POLYGON_OFFSET_FILL));

		STATE.offsetFactor = zFactor;
		STATE.offsetUnits = zUnits;
	}
}


void GFX_GL_IMPL::SetDepthState(const DeviceDepthState* state)
{
	DeviceDepthStateGL* devstate = (DeviceDepthStateGL*)state;
	if (STATE.m_CurrDepthState == devstate)
		return;

	STATE.m_CurrDepthState = devstate;

	if (!STATE.m_CurrDepthState)
		return;

	const CompareFunction testFunc = devstate->sourceState.depthFunc;
	if( testFunc != STATE.depthFunc )
	{
		if( testFunc != kFuncDisabled ) {
			OGL_CALL(glDepthFunc (kCmpFuncGL[testFunc]));
			OGL_CALL(glEnable (GL_DEPTH_TEST));
		} else {
			OGL_CALL(glDisable (GL_DEPTH_TEST));
		}

		STATE.depthFunc = testFunc;
	}

	const int writeMode = devstate->sourceState.depthWrite ? GL_TRUE : GL_FALSE;
	if( writeMode != STATE.depthWrite )
	{
		OGL_CALL(glDepthMask (writeMode));
		STATE.depthWrite = writeMode;
	}
}

void GFX_GL_IMPL::SetStencilState (const DeviceStencilState* state, int stencilRef)
{
	if (STATE.m_CurrStencilState == state && STATE.m_StencilRef == stencilRef)
		return;
	const DeviceStencilStateGL* st = static_cast<const DeviceStencilStateGL*>(state);
	STATE.m_CurrStencilState = st;
	if (!st)
		return;

	if (st->sourceState.stencilEnable)
		glEnable (GL_STENCIL_TEST);
	else
		glDisable (GL_STENCIL_TEST);
	if (gGraphicsCaps.hasTwoSidedStencil)
	{
		glStencilFuncSeparate (GL_FRONT, st->stencilFuncFront, stencilRef, st->sourceState.readMask);
		glStencilOpSeparate (GL_FRONT, st->stencilFailOpFront, st->depthFailOpFront, st->depthPassOpFront);
		glStencilFuncSeparate (GL_BACK, st->stencilFuncBack, stencilRef, st->sourceState.readMask);
		glStencilOpSeparate (GL_BACK, st->stencilFailOpBack, st->depthFailOpBack, st->depthPassOpBack);
	}
	else
	{
		glStencilFunc (st->stencilFuncFront, stencilRef, st->sourceState.readMask);
		glStencilOp (st->stencilFailOpFront, st->depthFailOpFront, st->depthPassOpFront);
	}
	glStencilMask (st->sourceState.writeMask);

	STATE.m_StencilRef = stencilRef;
}


void GFX_GL_IMPL::SetSRGBWrite (bool enable)
{
	if (!gGraphicsCaps.hasSRGBReadWrite)
		return;

	if (enable)
		OGL_CALL(glEnable(GL_FRAMEBUFFER_SRGB_EXT));
	else
		OGL_CALL(glDisable(GL_FRAMEBUFFER_SRGB_EXT));
}

bool GFX_GL_IMPL::GetSRGBWrite ()
{
	return gGraphicsCaps.hasSRGBReadWrite ?
		glIsEnabled(GL_FRAMEBUFFER_SRGB_EXT):
		false;
}

void GFX_GL_IMPL::Clear (UInt32 clearFlags, const float color[4], float depth, int stencil)
{
	GFX_LOG(("%d, {%f, %f, %f, %f}, %f, %d", clearFlags, color[0], color[1], color[2], color[3], depth, stencil));

	if (!IsActiveRenderTargetWithColorGL())
		clearFlags &= ~kGfxClearColor;

	// In OpenGL, clears are affected by color write mask and depth writing parameters.
	// So make sure to set them!
	GLbitfield flags = 0;
	if (clearFlags & kGfxClearColor)
	{
		if (STATE.colorWriteMask != 15)
		{
			OGL_CALL(glColorMask( true, true, true, true ));
			STATE.colorWriteMask = 15;
			STATE.m_CurrBlendState = NULL;
		}
		flags |= GL_COLOR_BUFFER_BIT;
		glClearColor (color[0], color[1], color[2], color[3]);
	}
	if (clearFlags & kGfxClearDepth)
	{
		OGL_CALL(glDepthMask (GL_TRUE));
		STATE.depthWrite = GL_TRUE;
		STATE.m_CurrDepthState = NULL;
		flags |= GL_DEPTH_BUFFER_BIT;
		glClearDepth (depth);
	}
	if (clearFlags & kGfxClearStencil)
	{
		//@TODO: need to set stencil writes on?
		flags |= GL_STENCIL_BUFFER_BIT;
		glClearStencil (stencil);
	}
	glClear (flags);
}


static void ApplyBackfaceMode( const DeviceStateGL& state )
{
	const bool bFlip = (state.appBackfaceMode != state.userBackfaceMode);

	if( bFlip )
		OGL_CALL(glFrontFace( GL_CCW ));
	else
		OGL_CALL(glFrontFace( GL_CW ));

	if(state.culling == kCullUnknown || state.culling == kCullOff)
		return;

	DeviceRasterState* devstate = state.m_CurrRasterState;
	if(!devstate)
		return;

	GfxDevice &device = GetRealGfxDevice();
	device.SetRasterState(0);
	device.SetRasterState(devstate);
}


void GFX_GL_IMPL::SetUserBackfaceMode( bool enable )
{
	GFX_LOG(("%d", enable));
	if( STATE.userBackfaceMode == enable )
		return;
	STATE.userBackfaceMode = enable;
	ApplyBackfaceMode( STATE );
}


void GFX_GL_IMPL::SetWireframe( bool wire )
{
	if( wire )
	{
		OGL_CALL(glEnable (GL_POLYGON_OFFSET_LINE));
		OGL_CALL(glPolygonMode (GL_FRONT_AND_BACK, GL_LINE));
		STATE.wireframe = true;
	}
	else
	{
		OGL_CALL(glDisable (GL_POLYGON_OFFSET_LINE));
		OGL_CALL(glPolygonMode (GL_FRONT_AND_BACK, GL_FILL));
		STATE.wireframe = false;
	}
}

bool GFX_GL_IMPL::GetWireframe() const
{
	return STATE.wireframe;
}



void GFX_GL_IMPL::SetInvertProjectionMatrix (bool enable)
{
	Assert (!enable); // projection should never be flipped upside down on OpenGL
}

bool GFX_GL_IMPL::GetInvertProjectionMatrix() const
{
	return false;
}

#if GFX_USES_VIEWPORT_OFFSET
void GFX_GL_IMPL::SetViewportOffset( float x, float y )
{
	GFX_LOG(("%f, %f", x, y));
	STATE.viewportOffsetX = x;
	STATE.viewportOffsetY = y;
}

void GFX_GL_IMPL::GetViewportOffset( float &x, float &y ) const
{
	x = STATE.viewportOffsetX;
	y = STATE.viewportOffsetY;
}
#endif

void GFX_GL_IMPL::SetProjectionMatrix (const Matrix4x4f& matrix)
{
	GFX_LOG(("{%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f}",
		matrix[0],	matrix[1],	matrix[2],	matrix[3],
		matrix[4],	matrix[5],	matrix[6],	matrix[7],
		matrix[8],	matrix[9],	matrix[10],	matrix[11],
		matrix[12],	matrix[13],	matrix[14],	matrix[15]));

	Matrix4x4f& projMat = m_BuiltinParamValues.GetWritableMatrixParam(kShaderMatProj);
	CopyMatrix (matrix.GetPtr(), projMat.GetPtr());
	OGL_CALL(glMatrixMode( GL_PROJECTION ));
	OGL_CALL(glLoadMatrixf (matrix.GetPtr()));
	OGL_CALL(glMatrixMode( GL_MODELVIEW ));
}

void GFX_GL_IMPL::SetWorldMatrix( const float matrix[16] )
{
	GFX_LOG(("{%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f}",
		matrix[0],	matrix[1],	matrix[2],	matrix[3],
		matrix[4],	matrix[5],	matrix[6],	matrix[7],
		matrix[8],	matrix[9],	matrix[10],	matrix[11],
		matrix[12],	matrix[13],	matrix[14],	matrix[15]));

	CopyMatrix(matrix, STATE.m_WorldMatrix.GetPtr());

	OGL_CALL(glLoadMatrixf(m_BuiltinParamValues.GetMatrixParam(kShaderMatView).GetPtr()));
	OGL_CALL(glMultMatrixf(matrix));
}

void GFX_GL_IMPL::SetViewMatrix( const float matrix[16] )
{
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(matrix);

	const Matrix4x4f& projMat = m_BuiltinParamValues.GetMatrixParam(kShaderMatProj);
	Matrix4x4f& viewMat = m_BuiltinParamValues.GetWritableMatrixParam(kShaderMatView);
	Matrix4x4f& viewProjMat = m_BuiltinParamValues.GetWritableMatrixParam(kShaderMatViewProj);
	CopyMatrix (matrix, viewMat.GetPtr());
	MultiplyMatrices4x4 (&projMat, &viewMat, &viewProjMat);
	STATE.m_WorldMatrix.SetIdentity();
}


void GFX_GL_IMPL::GetMatrix( float outMatrix[16] ) const
{
	OGL_CALL(glGetFloatv( GL_MODELVIEW_MATRIX, outMatrix ));
}


const float* GFX_GL_IMPL::GetWorldMatrix() const
{
	return STATE.m_WorldMatrix.GetPtr();
}

const float* GFX_GL_IMPL::GetViewMatrix() const
{
	return m_BuiltinParamValues.GetMatrixParam(kShaderMatView).GetPtr();
}

const float* GFX_GL_IMPL::GetProjectionMatrix() const
{
	return m_BuiltinParamValues.GetMatrixParam(kShaderMatProj).GetPtr();
}

const float* GFX_GL_IMPL::GetDeviceProjectionMatrix() const
{
	return GetProjectionMatrix();
}



void GFX_GL_IMPL::SetNormalizationBackface( NormalizationMode mode, bool backface )
{
	GFX_LOG(("%d, %d", mode, backface));

	if( mode != STATE.normalization )
	{
		if( mode == kNormalizationDisabled )
		{
			OGL_CALL(glDisable (GL_NORMALIZE));
			OGL_CALL(glDisable (GL_RESCALE_NORMAL));
		}
		else if( mode == kNormalizationScale )
		{
			OGL_CALL(glDisable (GL_NORMALIZE));
			OGL_CALL(glEnable (GL_RESCALE_NORMAL));
		}
		else
		{
			OGL_CALL(glEnable (GL_NORMALIZE));
			OGL_CALL(glDisable (GL_RESCALE_NORMAL));
		}

		STATE.normalization = mode;
	}
	if( STATE.appBackfaceMode != backface )
	{
		STATE.appBackfaceMode = backface;
		ApplyBackfaceMode( STATE );
	}
}

void GFX_GL_IMPL::SetFFLighting( bool on, bool separateSpecular, ColorMaterialMode colorMaterial )
{
	GFX_LOG(("%d, %d, %d", on, separateSpecular, colorMaterial));

	int lighting = on ? 1 : 0;
	if( lighting != STATE.lighting )
	{
		if( lighting )
			OGL_CALL(glEnable( GL_LIGHTING ));
		else
			OGL_CALL(glDisable( GL_LIGHTING ));
		STATE.lighting = lighting;
	}

	int sepSpec = separateSpecular ? 1 : 0;

	if (STATE.separateSpecular != sepSpec)
	{
		// Never set separate specular and color sum to different values.
		// Otherwise SIS 76x on OpenGL will hang up the machine.
		// And we probably never need them to be different anyway :)
		if( separateSpecular )
		{
			OGL_CALL(glLightModeli( GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR ));
			OGL_CALL(glEnable( GL_COLOR_SUM_EXT ));
		}
		else
		{
			OGL_CALL(glLightModeli( GL_LIGHT_MODEL_COLOR_CONTROL, GL_SINGLE_COLOR ));
			OGL_CALL(glDisable( GL_COLOR_SUM_EXT ));
		}
		STATE.separateSpecular = sepSpec;
	}

	if( colorMaterial != STATE.colorMaterial )
	{
		if( colorMaterial != kColorMatDisabled )
		{
			OGL_CALL(glColorMaterial( GL_FRONT_AND_BACK, kColorMatModeGL[colorMaterial] ));
			OGL_CALL(glEnable( GL_COLOR_MATERIAL ));
		}
		else
		{
			OGL_CALL(glDisable( GL_COLOR_MATERIAL ));

			// looks like by disabling ColorMaterial OpenGL driver will reset material props
			// therefore we invalidate materials cache
			STATE.matAmbient.Set(-1, -1, -1, -1);
			STATE.matDiffuse.Set(-1, -1, -1, -1);
			STATE.matSpecular.Set(-1, -1, -1, -1);
			STATE.matEmissive.Set(-1, -1, -1, -1);
		}

		STATE.colorMaterial = colorMaterial;
	}
}

void GFX_GL_IMPL::SetMaterial( const float ambient[4], const float diffuse[4], const float specular[4], const float emissive[4], const float shininess )
{
	GFX_LOG(("{%f, %f, %f, %f}, {%f, %f, %f, %f}, {%f, %f, %f, %f}, {%f, %f, %f, %f}, %f",
		ambient[0],	ambient[1],	ambient[2],	ambient[3],
		diffuse[0],	diffuse[1],	diffuse[2],	diffuse[3],
		specular[0],	specular[1],	specular[2],	specular[3],
		emissive[0],	emissive[1],	emissive[2],	emissive[3],
		shininess));
	if (STATE.matAmbient != ambient) {
		OGL_CALL(glMaterialfv (GL_FRONT, GL_AMBIENT, ambient));
		STATE.matAmbient.Set (ambient);
	}

	if (STATE.matDiffuse != diffuse) {
		OGL_CALL(glMaterialfv (GL_FRONT, GL_DIFFUSE, diffuse));
		STATE.matDiffuse.Set (diffuse);
	}

	if (STATE.matSpecular != specular) {
		OGL_CALL(glMaterialfv (GL_FRONT, GL_SPECULAR, specular));
		STATE.matSpecular.Set (specular);
	}

	if (STATE.matEmissive != emissive) {
		OGL_CALL(glMaterialfv (GL_FRONT, GL_EMISSION, emissive));
		STATE.matEmissive.Set (emissive);
	}

	if( STATE.matShininess != shininess ) {
		float glshine = std::max<float>( std::min<float>(shininess,1.0f), 0.0f) * 128.0f;
		OGL_CALL(glMaterialf (GL_FRONT, GL_SHININESS, glshine));
		STATE.matShininess = shininess;
	}

	// From shaderstate, the material is set after the ColorMaterial.
	// So here if color material is used, setup invalid values to material
	// cache; otherwise they would get out of sync.

	switch( STATE.colorMaterial ) {
	case kColorMatEmission: STATE.matEmissive.Set(-1, -1, -1, -1); break;
	case kColorMatAmbientAndDiffuse:
		STATE.matAmbient.Set(-1, -1, -1, -1);
		STATE.matDiffuse.Set(-1, -1, -1, -1);
		break;
	}
}


void GFX_GL_IMPL::SetColor( const float color[4] )
{
	GFX_LOG(("{%f, %f, %f, %f}", color[0], color[1], color[2], color[3]));
	OGL_CALL(glColor4fv( color ));
}


void GFX_GL_IMPL::SetViewport( int x, int y, int width, int height )
{
	GFX_LOG(("{%d, %d, %d, %d}", x, y, width, height));
	STATE.viewport[0] = x;
	STATE.viewport[1] = y;
	STATE.viewport[2] = width;
	STATE.viewport[3] = height;
	OGL_CALL(glViewport( x, y, width, height ));
}

void GFX_GL_IMPL::GetViewport( int* port ) const
{
	port[0] = STATE.viewport[0];
	port[1] = STATE.viewport[1];
	port[2] = STATE.viewport[2];
	port[3] = STATE.viewport[3];
}

void GFX_GL_IMPL::SetScissorRect( int x, int y, int width, int height )
{
	GFX_LOG(("{%d, %d, %d, %d}", x, y, width, height));
	if (STATE.scissor != 1)
	{
		OGL_CALL(glEnable( GL_SCISSOR_TEST ));
		STATE.scissor = 1;
	}

	STATE.scissorRect[0] = x;
	STATE.scissorRect[1] = y;
	STATE.scissorRect[2] = width;
	STATE.scissorRect[3] = height;
	OGL_CALL(glScissor( x, y, width, height ));

}

void GFX_GL_IMPL::DisableScissor()
{
	GFX_LOG((""));
	if (STATE.scissor != 0)
	{
		OGL_CALL(glDisable( GL_SCISSOR_TEST ));
		STATE.scissor = 0;
	}
}

bool GFX_GL_IMPL::IsScissorEnabled() const
{
	return STATE.scissor == 1;
}

void GFX_GL_IMPL::GetScissorRect( int scissor[4] ) const
{
	scissor[0] = STATE.scissorRect[0];
	scissor[1] = STATE.scissorRect[1];
	scissor[2] = STATE.scissorRect[2];
	scissor[3] = STATE.scissorRect[3];
}

bool GFX_GL_IMPL::IsCombineModeSupported( unsigned int combiner )
{
	return TextureCombinersGL::IsCombineModeSupported( combiner );
}

TextureCombinersHandle GFX_GL_IMPL::CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular )
{
	GFX_LOG(("%d, %d, %d", count, hasVertexColorOrLighting, usesAddSpecular));
	TextureCombinersGL* implGL = TextureCombinersGL::Create( count, texEnvs, props );
	return TextureCombinersHandle( implGL );
}

void GFX_GL_IMPL::DeleteTextureCombiners( TextureCombinersHandle& textureCombiners )
{
	GFX_LOG((""));
	TextureCombinersGL* implGL = OBJECT_FROM_HANDLE(textureCombiners,TextureCombinersGL);
	delete implGL;
	textureCombiners.Reset();
}

static void ClearTextureUnitGL (DeviceStateGL& state, int unit)
{
	GFX_LOG(("%d", unit));
	DebugAssert (unit >= 0 && unit < gGraphicsCaps.maxTexUnits);
	
	TextureUnitStateGL& currTex = state.textures[unit];
	if (currTex.texDim == kTexDimNone)
		return;

	if (state.shaderEnabledImpl[kShaderFragment] == kShaderImplUndefined)
	{
		ActivateTextureUnitGL (unit);

		if( currTex.texDim != kTexDimUnknown )
		{
			OGL_CALL(glDisable( kTexDimTableGL[currTex.texDim] ));
		}
		else
		{
			OGL_CALL(glDisable( GL_TEXTURE_2D ));
			OGL_CALL(glDisable( GL_TEXTURE_1D ));
			OGL_CALL(glDisable( GL_TEXTURE_CUBE_MAP_ARB ));
			if( gGraphicsCaps.has3DTexture )
				OGL_CALL(glDisable( GL_TEXTURE_3D ));
		}
		currTex.texDim = kTexDimNone;
	}
	else
	{
		currTex.texDim = kTexDimUnknown;
	}
}


void GFX_GL_IMPL::SetTextureCombinersThreadable( TextureCombinersHandle textureCombiners, const TexEnvData* texEnvData, const Vector4f* texColors )
{
	TextureCombinersGL* implGL = OBJECT_FROM_HANDLE(textureCombiners,TextureCombinersGL);
	AssertIf( !implGL );
	AssertIf (IsShaderActive( kShaderFragment ));

	int i = 0;
	for( ; i < gGraphicsCaps.maxTexUnits && i < implGL->count; ++i )
	{
		const ShaderLab::TextureBinding& binding = implGL->texEnvs[i];

		// set the texture
		ApplyTexEnvData (i, i, texEnvData[i]);
		// apply the combiner
		Vector4f texcolorVal = texColors[i];
		TextureUnitStateGL& unitState = STATE.textures[i];

		// TODO: on Joe's machine this is sometimes failing (FPS tutorial, rotate scene view).
		// So we just don't check, which disables the cache.
		//if (current.texColor != texcolorVal) {
		unitState.color = texcolorVal;
		OGL_CALL(glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, texcolorVal.GetPtr() ));
		//}

		// setup combine modes
		ApplyCombinerGL( unitState.combColor, unitState.combAlpha, binding.m_CombColor, binding.m_CombAlpha );
	}

	// clear unused textures
	for (; i < gGraphicsCaps.maxTexUnits; ++i)
		ClearTextureUnitGL (STATE, i);

	// Get us back to TU 0, so we know where we stand
	ActivateTextureUnitGL (0);
}

void GFX_GL_IMPL::SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props )
{
	TextureCombinersGL* implGL = OBJECT_FROM_HANDLE(textureCombiners,TextureCombinersGL);
	AssertIf( !implGL );

	int count = std::min(implGL->count, gGraphicsCaps.maxTexUnits);

	// Fill in arrays
	TexEnvData* texEnvData;
	ALLOC_TEMP (texEnvData, TexEnvData, count);
	for( int i = 0; i < count; ++i )
	{
		ShaderLab::TexEnv *te = ShaderLab::GetTexEnvForBinding( implGL->texEnvs[i], props );
		Assert( te != NULL );
		te->PrepareData (implGL->texEnvs[i].m_TextureName.index, implGL->texEnvs[i].m_MatrixName, props, &texEnvData[i]);
	}

	Vector4f* texColors;
	ALLOC_TEMP (texColors, Vector4f, count);
	for( int i = 0; i < count; ++i )
	{
		const ShaderLab::TextureBinding& binding = implGL->texEnvs[i];
		texColors[i] = binding.GetTexColor().Get (props);
	}
	GFX_GL_IMPL::SetTextureCombinersThreadable(textureCombiners, texEnvData, texColors);
}

const unsigned long* GetGLTextureDimensionTable() { return kTexDimTableGL; }

void GFX_GL_IMPL::SetTexture (ShaderType shaderType, int unit, int samplerUnit, TextureID texture, TextureDimension dim, float bias)
{
	GFX_LOG(("%d, %d, %d", unit, texture.m_ID, dim));
	DebugAssertIf( dim < kTexDim2D || dim > kTexDimCUBE );

	GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(texture);
	if(targetTex == 0)
		return;

	TextureUnitStateGL& currTex = STATE.textures[unit];

	DebugAssertIf( unit < 0 || unit >= gGraphicsCaps.maxTexImageUnits );
	ActivateTextureUnitGL (unit);

	int dimGLTarget = kTexDimTableGL[dim];


	if (!IsShaderActive(kShaderFragment))
	{
		switch( currTex.texDim ) {
		case kTexDimUnknown:
			// We don't know which target is enabled, so disable all but the one we
			// want to set, and enable only that one
			if( dim != kTexDimDeprecated1D ) OGL_CALL(glDisable( GL_TEXTURE_1D ));
			if( dim != kTexDim2D ) OGL_CALL(glDisable( GL_TEXTURE_2D ));
			if( dim != kTexDimCUBE ) OGL_CALL(glDisable( GL_TEXTURE_CUBE_MAP_ARB ));
			if( dim != kTexDim3D && gGraphicsCaps.has3DTexture ) OGL_CALL(glDisable( GL_TEXTURE_3D ));
			OGL_CALL(glEnable( dimGLTarget ));
			break;
		case kTexDimNone:
			// Texture unit was disabled. So simply enable our target.
			OGL_CALL(glEnable( dimGLTarget ));
			break;
		default:
			// Disable current and enable ours if they're different.
			if( currTex.texDim != dim ) {
				OGL_CALL(glDisable( kTexDimTableGL[currTex.texDim] ));
				OGL_CALL(glEnable( dimGLTarget ));
			}
			break;
		}
		currTex.texDim = dim;
	}
	else
	{
		currTex.texDim = kTexDimUnknown;
	}

	if( targetTex != currTex.texID )
	{
		OGL_CALL(glBindTexture(dimGLTarget, targetTex));
		currTex.texID = targetTex;
	}
	m_Stats.AddUsedTexture(texture);
	if (gGraphicsCaps.hasMipLevelBias && bias != currTex.bias && bias != std::numeric_limits<float>::infinity())
	{
		OGL_CALL(glTexEnvf( GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, bias ));
		currTex.bias = bias;
	}
}


void GFX_GL_IMPL::SetTextureTransform( int unit, TextureDimension dim, TexGenMode texGen, bool identity, const float matrix[16])
{
	GFX_LOG(("%d, %d, %d, %d, {%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f}",
		unit, dim, texGen, identity,
		matrix[0],	matrix[1],	matrix[2],	matrix[3],
		matrix[4],	matrix[5],	matrix[6],	matrix[7],
		matrix[8],	matrix[9],	matrix[10],	matrix[11],
		matrix[12],	matrix[13],	matrix[14],	matrix[15]
		));

	Assert (unit >= 0 && unit < kMaxSupportedTextureCoords);

	// This assumes the current texture unit is already set!!!
	TextureUnitStateGL& unitState = STATE.textures[unit];

	// -------- texture matrix

	if (unit < gGraphicsCaps.maxTexUnits)
	{
		OGL_CALL(glMatrixMode( GL_TEXTURE ));
		OGL_CALL(glLoadMatrixf( matrix ));
		OGL_CALL(glMatrixMode( GL_MODELVIEW ));
	}

	// -------- texture coordinate generation

	if( IsShaderActive(kShaderVertex) )
	{
		// Ok, in theory we could just return here, as texgen is completely ignored
		// with vertex shaders. However, on Mac OS X 10.4.10, Radeon X1600 and GeForce 8600M,
		// there seems to be a bug. Repro case: shader that uses a single cubemap texture and
		// CubeReflect texgen, and use glow on the camera = glow is wrong.
		//
		// On Windows / GeForce 7600 on OpenGL (93.71) the behaviour is correct. But I feel safer
		// just using the workaround everywhere...
		//
		// So: with vertex shaders, force texgen to be disabled. The redundant change check below
		// will catch most of the cases cheaply anyway.
		texGen = kTexGenDisabled;
	}

	if( texGen == unitState.texGen )
		return;

	switch( texGen )
	{
	case kTexGenDisabled:
		OGL_CALL(glDisable (GL_TEXTURE_GEN_S));
		OGL_CALL(glDisable (GL_TEXTURE_GEN_T));
		OGL_CALL(glDisable (GL_TEXTURE_GEN_R));
		break;
	case kTexGenSphereMap:
		OGL_CALL(glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP));
		OGL_CALL(glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_S));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_T));
		OGL_CALL(glDisable(GL_TEXTURE_GEN_R));
		break;
	case kTexGenObject: {
		float zplane[4] = {0,0,1,0};
		OGL_CALL(glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR));
		OGL_CALL(glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR));
		OGL_CALL(glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR));
		OGL_CALL(glTexGenfv (GL_R, GL_OBJECT_PLANE, zplane));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_S));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_T));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_R));
		break;
		}
	case kTexGenEyeLinear: {
		OGL_CALL(glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR));
		OGL_CALL(glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR));
		OGL_CALL(glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_S));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_T));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_R));
		break;
		}
	case kTexGenCubeReflect:
		OGL_CALL(glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB));
		OGL_CALL(glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB));
		OGL_CALL(glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_S));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_T));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_R));
		break;
	case kTexGenCubeNormal:
		OGL_CALL(glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP_ARB));
		OGL_CALL(glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP_ARB));
		OGL_CALL(glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP_ARB));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_S));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_T));
		OGL_CALL(glEnable(GL_TEXTURE_GEN_R));
		break;
	}

	unitState.texGen = texGen;
}

void GFX_GL_IMPL::SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace )
{
	GFX_LOG(("%d, %d, %d, %d, %d, %d, %d", texture.m_ID, texDim, filter, wrap, anisoLevel, hasMipMap, colorSpace));

	TextureIdMapGL_QueryOrCreate(texture);

	GLuint target = GetGLTextureDimensionTable()[texDim];
	SetTexture (kShaderFragment, 0, 0, texture, texDim, std::numeric_limits<float>::infinity());

	// Anisotropic texturing...
	if( gGraphicsCaps.hasAnisoFilter && target != GL_TEXTURE_3D )
	{
		anisoLevel = std::min( anisoLevel, gGraphicsCaps.maxAnisoLevel );
		OGL_CALL(glTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisoLevel ));
	}

	OGL_CALL(glTexParameteri( target, GL_TEXTURE_WRAP_S, kWrapModeGL[wrap] ));
	OGL_CALL(glTexParameteri( target, GL_TEXTURE_WRAP_T, kWrapModeGL[wrap] ));
	if( target == GL_TEXTURE_3D || target == GL_TEXTURE_CUBE_MAP_ARB )
		OGL_CALL(glTexParameteri( target, GL_TEXTURE_WRAP_R, kWrapModeGL[wrap] ));

	if( !hasMipMap && filter == kTexFilterTrilinear )
		filter = kTexFilterBilinear;

	OGL_CALL(glTexParameteri( target, GL_TEXTURE_MAG_FILTER, filter != kTexFilterNearest ? GL_LINEAR : GL_NEAREST ));
	if( hasMipMap )
		OGL_CALL(glTexParameteri( target, GL_TEXTURE_MIN_FILTER, kMinFilterGL[filter] ));
	else
		OGL_CALL(glTexParameteri (target, GL_TEXTURE_MIN_FILTER, filter != kTexFilterNearest ? GL_LINEAR : GL_NEAREST));
}




// Dark corner of OpenGL:
//  (this is at least on OS X 10.4.9, MacBook Pro)
// Before switching to new context, unbind any textures/shaders/whatever in the previous one!
//
// Otherwise is some _rare_ cases the textures will leak VRAM. Cases like this:
// 1) create a depth render texture (this creates actual GL texture; as copy is required)
// 2) create another render texture, render into it using the depth texture
// 3) release both depth RT and this other RT
// 4) the memory used by GL texture from step 1 is leaked!
//
// It seems like OS X is refcounting resources between contexts, yet when
// a context is destroyed the refcounts are not released. Or something like that.
//
// The texture leak also happens if shader programs are not unbound (hard to correlate why, but...)!
void GFX_GL_IMPL::UnbindObjects()
{
	GFX_GL_IMPL& device = static_cast<GFX_GL_IMPL&>(GetRealGfxDevice());

	// unbind textures
	int texUnits = gGraphicsCaps.maxTexUnits;
	for( int i = 0; i < texUnits; ++i )
		ClearTextureUnitGL (GetGLDeviceState(device), i);
	// unbind shaders
	ShaderLab::SubProgram* programs[kShaderTypeCount] = {0};
	GraphicsHelper::SetShaders (*this, programs, NULL);
	// unbind VBOs
	UnbindVertexBuffersGL();
}



unsigned int GetGLShaderImplTarget( ShaderImplType implType )
{
	switch( implType )
	{
	case kShaderImplVertex: return GL_VERTEX_PROGRAM_ARB;
	case kShaderImplFragment: return GL_FRAGMENT_PROGRAM_ARB;
	}
	DebugAssertIf( true );
	return 0;
}

void InvalidateActiveShaderStateGL( ShaderType type )
{
	GfxDevice& device = GetRealGfxDevice();
	AssertIf( device.GetRenderer() != kGfxRendererOpenGL );
	GFX_GL_IMPL& deviceGL = static_cast<GFX_GL_IMPL&>( device );

	DeviceStateGL& state = GetGLDeviceState(deviceGL);
	state.activeGpuProgram[type] = NULL;
	state.activeGpuProgramParams[type] = NULL;
	state.shaderEnabledImpl[type] = kShaderImplUndefined;
	state.shaderEnabledID[type] = -1;
}


static void InternalSetShader( DeviceStateGL& state, GfxDeviceStats& stats, FogMode fog, ShaderType type, GpuProgram* program, const GpuProgramParameters* params )
{
	if( program )
	{
		GpuProgramGL& shaderGL = static_cast<GpuProgramGL&>(*program);
		ShaderImplType implType = shaderGL.GetImplType();
		GLShaderID id = 0;
		if (implType == kShaderImplBoth) {
			id = static_cast<GlslGpuProgram&>(shaderGL).GetGLProgram(fog, const_cast<GpuProgramParameters&>(*params));
		} else {
			id = static_cast<ArbGpuProgram&>(shaderGL).GetGLProgram(fog);
		}
		// set the shader
		DebugAssert (id > 0);

		ShaderImplType oldType = state.shaderEnabledImpl[type];
		if( oldType != implType )
		{
			// impl type is different, disable old one and enable new one
			if( oldType == kShaderImplBoth )
				OGL_CALL(glUseProgramObjectARB( 0 ));
			else if( oldType != kShaderImplUndefined )
				OGL_CALL(glDisable( GetGLShaderImplTarget(oldType) ));
			else {
				// we don't know which one is currently enabled, so disable everything we have!
				if (type == kShaderVertex)
					OGL_CALL(glDisable(GL_VERTEX_PROGRAM_ARB));
				if (type == kShaderFragment)
					OGL_CALL(glDisable(GL_FRAGMENT_PROGRAM_ARB));
			}

			if( implType == kShaderImplBoth )
			{
				Assert (type == kShaderVertex);
				OGL_CALL(glUseProgramObjectARB( id ));
			}
			else
			{
				GLenum gltarget = GetGLShaderImplTarget( implType );
				OGL_CALL(glEnable( gltarget ));
				OGL_CALL(glBindProgramARB( gltarget, id ));
			}

			state.shaderEnabledImpl[type] = implType;
			state.shaderEnabledID[type] = id;
		}
		else
		{
			// impl type is the same, just use new shader (if different)

			if( state.shaderEnabledID[type] != id )
			{
				if( implType == kShaderImplBoth )
				{
					OGL_CALL(glUseProgramObjectARB( id ));
				}
				else
				{
					OGL_CALL(glBindProgramARB( GetGLShaderImplTarget(implType), id ));
				}
				state.shaderEnabledID[type] = id;
			}
		}
	}
	else
	{
		// clear the shader
		if( state.shaderEnabledID[type] == 0 )
		{
			// shader disabled, do nothing
			DebugAssertIf( state.shaderEnabledImpl[type] != kShaderImplUndefined );
		}
		else if( state.shaderEnabledID[type] == -1 )
		{
			// shader state unknown, disable everything
			DebugAssertIf( state.shaderEnabledImpl[type] != kShaderImplUndefined );
			if( gGraphicsCaps.gl.hasGLSL )
				OGL_CALL(glUseProgramObjectARB( 0 ));
			if( type == kShaderVertex )
			{
				OGL_CALL(glDisable( GL_VERTEX_PROGRAM_ARB ));
			}
			else if( type == kShaderFragment )
			{
				OGL_CALL(glDisable( GL_FRAGMENT_PROGRAM_ARB ));
			}
			else
			{
				AssertIf( true );
			}
			state.shaderEnabledID[type] = 0;
		}
		else
		{
			// some shader enabled, disable just that one
			ShaderImplType oldType = state.shaderEnabledImpl[type];
			DebugAssertIf( oldType == kShaderImplUndefined );
			if( oldType == kShaderImplBoth )
			{
				OGL_CALL(glUseProgramObjectARB( 0 ));
			}
			else
				OGL_CALL(glDisable( GetGLShaderImplTarget(oldType) ));
			state.shaderEnabledImpl[type] = kShaderImplUndefined;
			state.shaderEnabledID[type] = 0;
		}
	}

	// Note: set activeGpuProgram after doing everything above. Above code might be
	// creating shader combinations for Fog on demand, which can reset activeGpuProgram to NULL.
	state.activeGpuProgram[type] = program;
	state.activeGpuProgramParams[type] = params;
}

void GFX_GL_IMPL::SetShadersThreadable(GpuProgram* programs[kShaderTypeCount], const GpuProgramParameters* params[kShaderTypeCount], UInt8 const * const paramsBuffer[kShaderTypeCount])
{
	GpuProgram* vertexProgram = programs[kShaderVertex];
	GpuProgram* fragmentProgram = programs[kShaderFragment];

	GFX_LOG(("%d, %d",
			 (vertexProgram? vertexProgram->GetImplType() : kShaderImplUndefined),
			 (fragmentProgram? fragmentProgram->GetImplType() : kShaderImplUndefined)));

	// GLSL is only supported like this:
	// vertex shader actually is both vertex & fragment linked program
	// fragment shader is unused

	FogMode fogMode = m_FogParams.mode;
	if (vertexProgram && vertexProgram->GetImplType() == kShaderImplBoth)
	{
		Assert (fragmentProgram == 0 || fragmentProgram->GetImplType() == kShaderImplBoth);

		InternalSetShader( STATE, m_Stats, m_FogParams.mode, kShaderVertex, vertexProgram, params[kShaderVertex] );
	}
	else
	{
		Assert ((fragmentProgram ? fragmentProgram->GetImplType() != kShaderImplBoth : true));

		InternalSetShader( STATE, m_Stats, fogMode, kShaderVertex, vertexProgram, params[kShaderVertex] );
		InternalSetShader( STATE, m_Stats, fogMode, kShaderFragment, fragmentProgram, params[kShaderFragment] );
	}

	for (int pt = 0; pt < kShaderTypeCount; ++pt)
	{
		if (programs[pt])
		{
			programs[pt]->ApplyGpuProgram (*params[pt], paramsBuffer[pt]);
			m_BuiltinParamIndices[pt] = &params[pt]->GetBuiltinParams();
		}
		else
		{
			m_BuiltinParamIndices[pt] = &m_NullParamIndices;
		}
	}
}


void GFX_GL_IMPL::CreateShaderParameters( ShaderLab::SubProgram* program, FogMode fogMode )
{
	GpuProgramGL& shaderGL = static_cast<GpuProgramGL&>(program->GetGpuProgram());
	ShaderImplType implType = shaderGL.GetImplType();
	if (implType == kShaderImplBoth) {
		static_cast<GlslGpuProgram&>(shaderGL).GetGLProgram(fogMode, program->GetParams(fogMode));
	} else {
		static_cast<ArbGpuProgram&>(shaderGL).GetGLProgram(fogMode);
	}
}

bool GFX_GL_IMPL::IsShaderActive( ShaderType type ) const
{
	return STATE.shaderEnabledImpl[type] != kShaderImplUndefined;
}

void GFX_GL_IMPL::DestroySubProgram( ShaderLab::SubProgram* subprogram )
{
	GpuProgram& program = subprogram->GetGpuProgram();
	GpuProgramGL& shaderGL = static_cast<GpuProgramGL&>(program);
	ShaderType lastType = kShaderFragment;
	if (shaderGL.GetImplType() == kShaderImplBoth)
		lastType = kShaderVertex;

	for (int i = 0; i < kFogModeCount; ++i)
	{
		GLShaderID id = shaderGL.GetGLProgramIfCreated(FogMode(i));
		for (int j = kShaderVertex; j <= lastType; ++j)
		{
			if (id && STATE.shaderEnabledID[j] == id)
			{
				STATE.activeGpuProgram[j] = NULL;
				STATE.activeGpuProgramParams[j] = NULL;
				STATE.shaderEnabledID[j] = -1;
				STATE.shaderEnabledImpl[j] = kShaderImplUndefined;
			}
		}
	}
	delete subprogram;
}

void GFX_GL_IMPL::DisableLights( int startLight )
{
	GFX_LOG(("%d", startLight));

	DebugAssertIf( startLight < 0 || startLight > kMaxSupportedVertexLights );
	const Vector4f black(0.0F, 0.0F, 0.0F, 0.0F);

	const int maxLights = gGraphicsCaps.maxLights;
	for (int i = startLight; i < maxLights; ++i)
	{
		if( STATE.vertexLights[i].enabled != 0 )
		{
			OGL_CALL(glLightfv (GL_LIGHT0 + i, GL_DIFFUSE, black.GetPtr()));
			OGL_CALL(glDisable( GL_LIGHT0 + i ));

			STATE.vertexLights[i].enabled = 0;
		}
	}

	for (int i = startLight; i < gGraphicsCaps.maxLights; ++i)
	{
		m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Diffuse + i), black);
	}
}

void GFX_GL_IMPL::SetLight( int light, const GfxVertexLight& data)
{
	GFX_LOG(("%d, [{%f, %f, %f, %f}, {%f, %f, %f, %f}, {%f, %f, %f, %f}, %f, %f, %f, %d]",
		light,
		data.position.x, data.position.y, data.position.z, data.position.w,
		data.spotDirection.x, data.spotDirection.y, data.spotDirection.z, data.spotDirection.w,
		data.color.x, data.color.y, data.color.z, data.color.w,
		data.range, data.quadAtten, data.spotAngle, data.type));

	DebugAssert(light >= 0 && light < kMaxSupportedVertexLights);

	VertexLightStateGL& state = STATE.vertexLights[light];

	if (state.enabled != 1)
	{
		OGL_CALL(glEnable( GL_LIGHT0 + light ));
		state.enabled = 1;
	}

	float dir[4];

	// position
	if( data.position.w == 0.0f )
	{
		// negate directional light for OpenGL
		dir[0] = -data.position.x;
		dir[1] = -data.position.y;
		dir[2] = -data.position.z;
		dir[3] = 0.0f;
		OGL_CALL(glLightfv( GL_LIGHT0 + light, GL_POSITION, dir ));
	}
	else
	{
		OGL_CALL(glLightfv( GL_LIGHT0 + light, GL_POSITION, data.position.GetPtr() ));
	}

	// spot direction
	if( data.spotAngle != -1.0f )
		OGL_CALL(glLightfv( GL_LIGHT0 + light, GL_SPOT_DIRECTION, data.spotDirection.GetPtr() ));

	// colors
	OGL_CALL(glLightfv( GL_LIGHT0 + light, GL_DIFFUSE, data.color.GetPtr() ));
	OGL_CALL(glLightfv( GL_LIGHT0 + light, GL_SPECULAR, data.color.GetPtr() ));
	static float zeroColor[4] = { 0, 0, 0, 0 };
	OGL_CALL(glLightfv( GL_LIGHT0 + light, GL_AMBIENT, zeroColor ));

	// attenuation
	if( state.attenQuad != data.quadAtten ) {
		OGL_CALL(glLightf( GL_LIGHT0 + light, GL_CONSTANT_ATTENUATION, 1.0f ));
		OGL_CALL(glLightf( GL_LIGHT0 + light, GL_QUADRATIC_ATTENUATION, data.quadAtten ));
		state.attenQuad = data.quadAtten;
	}

	// angles
	if( state.spotAngle != data.spotAngle )
	{
		if( data.spotAngle == -1.0f )
		{
			// non-spot light
			OGL_CALL(glLightf( GL_LIGHT0 + light, GL_SPOT_CUTOFF, 180.0f ));
		}
		else
		{
			// spot light
			OGL_CALL(glLightf( GL_LIGHT0 + light, GL_SPOT_CUTOFF, data.spotAngle * 0.5f ));
			OGL_CALL(glLightf( GL_LIGHT0 + light, GL_SPOT_EXPONENT, 18.0f - data.spotAngle * 0.1f ));
		}
		state.spotAngle = data.spotAngle;
	}

	SetupVertexLightParams (light, data);
}


void GFX_GL_IMPL::SetAmbient( const float ambient[4] )
{
	GFX_LOG(("{%f, %f, %f, %f}", ambient[0], ambient[1], ambient[2], ambient[3]));
	if( STATE.ambient != ambient )
	{
		OGL_CALL(glLightModelfv( GL_LIGHT_MODEL_AMBIENT, ambient ));
		STATE.ambient.Set( ambient );
	}
}

void GFX_GL_IMPL::EnableFog (const GfxFogParams& fog)
{
	GFX_LOG(("%d, %f, %f, %f, {%f, %f, %f, %f}", fog.mode, fog.start, fog.end, fog.density, fog.color.x, fog.color.y, fog.color.z, fog.color.w));

	DebugAssertIf( fog.mode <= kFogDisabled );
	if( m_FogParams.mode != fog.mode )
	{
		OGL_CALL(glFogi( GL_FOG_MODE, kFogModeGL[fog.mode] ));
		OGL_CALL(glEnable( GL_FOG ));
		m_FogParams.mode = fog.mode;
	}
	if( m_FogParams.start != fog.start )
	{
		OGL_CALL(glFogf( GL_FOG_START, fog.start ));
		m_FogParams.start = fog.start;
	}
	if( m_FogParams.end != fog.end )
	{
		OGL_CALL(glFogf( GL_FOG_END, fog.end ));
		m_FogParams.end = fog.end;
	}
	if( m_FogParams.density != fog.density )
	{
		OGL_CALL(glFogf( GL_FOG_DENSITY, fog.density ));
		m_FogParams.density = fog.density;
	}
	if( m_FogParams.color != fog.color )
	{
		OGL_CALL(glFogfv( GL_FOG_COLOR, fog.color.GetPtr() ));
		m_FogParams.color = fog.color;
	}
}

void GFX_GL_IMPL::DisableFog()
{
	GFX_LOG((""));

	if( m_FogParams.mode != kFogDisabled )
	{
		OGL_CALL(glFogf( GL_FOG_DENSITY, 0.0f ));
		OGL_CALL(glDisable( GL_FOG ));

		m_FogParams.mode = kFogDisabled;
		m_FogParams.density = 0.0f;
	}
}


VBO* GFX_GL_IMPL::CreateVBO()
{
	VBO* vbo = new ARBVBO();
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
	if( STATE.m_DynamicVBO == NULL )
	{
		// DynamicARBVBO is slower! Just don't use it for now.
		//	STATE.m_DynamicVBO = new DynamicARBVBO( 1024 * 1024 ); // initial size: 1MiB VB
		STATE.m_DynamicVBO = new DynamicNullVBO();
	}
	return *STATE.m_DynamicVBO;
}



// ---------- render textures


// defined in RenderTextureGL
RenderSurfaceHandle CreateRenderColorSurfaceGL (TextureID textureID, int width, int height, int samples, TextureDimension dim, UInt32 createFlags, RenderTextureFormat format, GLuint globalSharedFBO);
RenderSurfaceHandle CreateRenderDepthSurfaceGL (TextureID textureID, int width, int height, int samples, UInt32 createFlags, DepthBufferFormat depthFormat, GLuint globalSharedFBO);
void DestroyRenderSurfaceGL (RenderSurfaceHandle& rsHandle);
bool SetRenderTargetGL (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, GLuint globalSharedFBO);
RenderSurfaceHandle GetActiveRenderColorSurfaceGL(int index);
RenderSurfaceHandle GetActiveRenderDepthSurfaceGL();
void ResolveDepthIntoTextureGL (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle, GLuint globalSharedFBO, GLuint helperFBO);
void GrabIntoRenderTextureGL (RenderSurfaceHandle rs, int x, int y, int width, int height, GLuint globalSharedFBO);


static GLuint GetFBO (GLuint* fbo)
{
	if (!(*fbo) && gGraphicsCaps.hasRenderToTexture)
	{
		glGenFramebuffersEXT (1, fbo);
	}
	return *fbo;
}

static GLuint GetSharedFBO (DeviceStateGL& state)
{
	return GetFBO (&state.m_SharedFBO);
}

static GLuint GetHelperFBO (DeviceStateGL& state)
{
	return GetFBO (&state.m_HelperFBO);
}

RenderSurfaceHandle GFX_GL_IMPL::CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags)
{
	GFX_LOG(("%d, %d, %d, %d, %d", textureID.m_ID, width, height, createFlags, format));
	GLuint fbo = GetSharedFBO(STATE);
	RenderSurfaceHandle rs = CreateRenderColorSurfaceGL (textureID, width, height, samples, dim, createFlags, format, fbo);
	#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
	#endif
	return rs;
}
RenderSurfaceHandle GFX_GL_IMPL::CreateRenderDepthSurface (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags)
{
	GFX_LOG(("%d, %d, %d, %d", textureID.m_ID, width, height, depthFormat));
	GLuint fbo = GetSharedFBO(STATE);
	RenderSurfaceHandle rs = CreateRenderDepthSurfaceGL (textureID, width, height, samples, createFlags, depthFormat, fbo);
	#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
	#endif
	return rs;
}
void GFX_GL_IMPL::DestroyRenderSurface (RenderSurfaceHandle& rs)
{
	GFX_LOG((""));

	// Early out if render surface is not created (don't do flush in that case)
	if( !rs.IsValid() )
		return;

	DestroyRenderSurfaceGL (rs);

	#if UNITY_WIN
	// Invalidating state causes problems with Windows OpenGL editor (case 490767).
	int flags = kGLContextSkipInvalidateState;
	#else
	// Not invalidating state makes the cached state not match the actual state on OS X (case 494066).
	int flags = 0;
	#endif

	GraphicsContextHandle ctx = GetMainGraphicsContext();
	if( ctx.IsValid() )
		ActivateGraphicsContext (ctx, false, flags);

	#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
	#endif
}
void GFX_GL_IMPL::SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face)
{
	GFX_LOG(("%d", face));
	#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
	#endif
	SetRenderTargetGL (count, colorHandles, depthHandle, mipLevel, face, GetSharedFBO(STATE));
	// changing render target might mean different color clear flags; so reset current state
	STATE.m_CurrBlendState = NULL;
	#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
	#endif
}
void GFX_GL_IMPL::ResolveDepthIntoTexture (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle)
{
	GFX_LOG((""));
	::ResolveDepthIntoTextureGL (colorHandle, depthHandle, GetSharedFBO(STATE), GetHelperFBO(STATE));
}
void GFX_GL_IMPL::ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle)
{
	void ResolveColorSurfaceGL (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle, GLuint globalSharedFBO, GLuint helperFBO);

	ResolveColorSurfaceGL (srcHandle, dstHandle, GetSharedFBO(STATE), GetHelperFBO(STATE));
}
RenderSurfaceHandle GFX_GL_IMPL::GetActiveRenderColorSurface(int index)
{
	return GetActiveRenderColorSurfaceGL(index);
}
RenderSurfaceHandle GFX_GL_IMPL::GetActiveRenderDepthSurface()
{
	return GetActiveRenderDepthSurfaceGL();
}
void GFX_GL_IMPL::SetSurfaceFlags (RenderSurfaceHandle surf, UInt32 flags, UInt32 keepFlags)
{
}

// ---------- uploading textures

void GFX_GL_IMPL::UploadTexture2D( TextureID texture, TextureDimension dimension, UInt8* srcData, int srcSize, int width, int height, TextureFormat format, int mipCount, UInt32 uploadFlags, int skipMipLevels, TextureUsageMode usageMode, TextureColorSpace colorSpace )
{
	GFX_LOG(("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d", texture.m_ID, dimension, width, height, format, mipCount, uploadFlags, skipMipLevels, usageMode, colorSpace));
 	::UploadTexture2DGL( texture, dimension, srcData, width, height, format, mipCount, uploadFlags, skipMipLevels, usageMode, colorSpace );
}

void GFX_GL_IMPL::UploadTextureSubData2D( TextureID texture, UInt8* srcData, int srcSize, int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace )
{
	GFX_LOG(("%d, %d, %d, %d, %d, %d, %d, %d", texture.m_ID, mipLevel, x, y, width, height, format, colorSpace));
	::UploadTextureSubData2DGL( texture, srcData, mipLevel, x, y, width, height, format, colorSpace );
}

void GFX_GL_IMPL::UploadTextureCube( TextureID texture, UInt8* srcData, int srcSize, int faceDataSize, int size, TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace )
{
	GFX_LOG(("%d, %d, %d, %d, %d, %d, %d", texture.m_ID, faceDataSize, size, format, mipCount, uploadFlags, colorSpace));
	::UploadTextureCubeGL( texture, srcData, faceDataSize, size, format, mipCount, uploadFlags, colorSpace );
}

void GFX_GL_IMPL::UploadTexture3D( TextureID texture, UInt8* srcData, int srcSize, int width, int height, int depth, TextureFormat format, int mipCount, UInt32 uploadFlags )
{
	GFX_LOG(("%d, %d, %d, %d, %d, %d, %d", texture.m_ID, width, height, depth, format, mipCount, uploadFlags ));
	::UploadTexture3DGL( texture, srcData, width, height, depth, format, mipCount, uploadFlags );
}

void GFX_GL_IMPL::DeleteTexture( TextureID texture )
{
	GFX_LOG(("%d", texture.m_ID));
	GLuint texname = (GLuint)TextureIdMap::QueryNativeTexture(texture);
    if(texname == 0)
        return;

	REGISTER_EXTERNAL_GFX_DEALLOCATION(texture.m_ID);
	glDeleteTextures( 1, &texname );

	// invalidate texture unit states that used this texture
	for( int i = 0; i < kMaxSupportedTextureUnits; ++i )
	{
		TextureUnitStateGL& currTex = STATE.textures[i];
		if( currTex.texID == texname )
			currTex.Invalidate();
	}

	TextureIdMap::RemoveTexture(texture);
}


// ---------- context

GfxDevice::PresentMode GFX_GL_IMPL::GetPresentMode()
{
	return kPresentBeforeUpdate;
}
void GFX_GL_IMPL::BeginFrame()
{
	GFX_LOG((""));
	m_InsideFrame = true;
}
void GFX_GL_IMPL::EndFrame()
{
	GFX_LOG((""));
	m_InsideFrame = false;
}
bool GFX_GL_IMPL::IsValidState()
{
	#if UNITY_LINUX
	// The Linux webplayer sometimes appears to lose sync with the GL cache.
	// This is a quick, best-guess test for that which allows us to recover gracefully.
	float temp;
	glGetFloatv (GL_FRONT_FACE, &temp);
	return CompareApproximately(temp,(STATE.appBackfaceMode != STATE.userBackfaceMode)?GL_CCW:GL_CW,0.0001f);
	#endif
	return true;
}

void GFX_GL_IMPL::PresentFrame()
{
	GFX_LOG((""));
	PresentContextGL( GetMainGraphicsContext() );
	#if GFX_DEVICE_VERIFY_ENABLE
	VerifyState();
	#endif
}

void GFX_GL_IMPL::FinishRendering()
{
	GFX_LOG((""));
	OGL_CALL(glFinish());
}


// ---------- immediate mode rendering

template <class T>
void WriteToArray(const T& data, dynamic_array<UInt8>& dest)
{
	int offset = dest.size();
	dest.resize_uninitialized(offset + sizeof(T), true);
	*reinterpret_cast<T*>(&dest[offset]) = data;
}


void GFX_GL_IMPL::ImmediateVertex( float x, float y, float z )
{
	GFX_LOG(("%f, %f, %f", x, y ,z));
	WriteToArray(UInt32(kImmediateVertex), STATE.m_ImmediateVertices);
	WriteToArray(Vector3f(x, y, z), STATE.m_ImmediateVertices);
}

void GFX_GL_IMPL::ImmediateNormal( float x, float y, float z )
{
	GFX_LOG(("%f, %f, %f", x, y ,z));
	WriteToArray(UInt32(kImmediateNormal), STATE.m_ImmediateVertices);
	WriteToArray(Vector3f(x, y, z), STATE.m_ImmediateVertices);
}

void GFX_GL_IMPL::ImmediateColor( float r, float g, float b, float a )
{
	GFX_LOG(("%f, %f, %f, %f", r, g, b, a));
	WriteToArray(UInt32(kImmediateColor), STATE.m_ImmediateVertices);
	WriteToArray(ColorRGBAf(r, g, b, a), STATE.m_ImmediateVertices);
}

void GFX_GL_IMPL::ImmediateTexCoordAll( float x, float y, float z )
{
	GFX_LOG(("%f, %f, %f", x, y ,z));
	WriteToArray(UInt32(kImmediateTexCoordAll), STATE.m_ImmediateVertices);
	WriteToArray(Vector3f(x, y, z), STATE.m_ImmediateVertices);
}

void GFX_GL_IMPL::ImmediateTexCoord( int unit, float x, float y, float z )
{
	GFX_LOG(("%d, %f, %f, %f", unit, x, y ,z));
	WriteToArray(UInt32(kImmediateTexCoord), STATE.m_ImmediateVertices);
	WriteToArray(UInt32(unit), STATE.m_ImmediateVertices);
	WriteToArray(Vector3f(x, y, z), STATE.m_ImmediateVertices);
}

void GFX_GL_IMPL::ImmediateBegin( GfxPrimitiveType type )
{
	GFX_LOG(("%d", type));
	STATE.m_ImmediateMode = type;
	STATE.m_ImmediateVertices.clear();
}

void GFX_GL_IMPL::ImmediateEnd()
{
	GFX_LOG((""));

	int size = STATE.m_ImmediateVertices.size();
	if (size == 0)
		return;

	// when beginning immediate mode, turn off any vertex array components that might be
	// activated
	UnbindVertexBuffersGL();
	ClearActiveChannelsGL();
	ActivateChannelsGL();
	BeforeDrawCall( true );
	glBegin (kTopologyGL[STATE.m_ImmediateMode]);

	const UInt8* src = STATE.m_ImmediateVertices.data();
	const UInt8* end = src + size;
	int vertexCount = 0;
	while (src < end)
	{
		UInt32 type = *reinterpret_cast<const UInt32*>(src);
		src += sizeof(UInt32);
		switch (type)
		{
			case kImmediateVertex:
			{
				const Vector3f& v = *reinterpret_cast<const Vector3f*>(src);
				src += sizeof(Vector3f);
				glVertex3f( v.x, v.y, v.z );
				++vertexCount;
				break;
			}
			case kImmediateNormal:
			{
				const Vector3f& v = *reinterpret_cast<const Vector3f*>(src);
				src += sizeof(Vector3f);
				glNormal3f( v.x, v.y, v.z );
				break;
			}
			case kImmediateColor:
			{
				const ColorRGBAf& col = *reinterpret_cast<const ColorRGBAf*>(src);
				src += sizeof(ColorRGBAf);
				glColor4f( col.r, col.g, col.b, col.a );
				break;
			}
			case kImmediateTexCoordAll:
			{
				const Vector3f& v = *reinterpret_cast<const Vector3f*>(src);
				src += sizeof(Vector3f);
				for( int i = gGraphicsCaps.maxTexCoords; i--; )
				{
					if( STATE.textures[i].texGen <= kTexGenDisabled )
						glMultiTexCoord3fARB( GL_TEXTURE0_ARB + i, v.x, v.y, v.z );
				}
				break;
			}
			case kImmediateTexCoord:
			{
				UInt32 unit = *reinterpret_cast<const UInt32*>(src);
				src += sizeof(UInt32);
				const Vector3f& v = *reinterpret_cast<const Vector3f*>(src);
				src += sizeof(Vector3f);
				glMultiTexCoord3fARB( GL_TEXTURE0_ARB + unit, v.x, v.y, v.z );
				break;
			}
			default:
				ErrorString("Unknown immediate type!");
				break;
		}
	}

	glEnd();
	// NOTE: all immediate mode calls CAN'T call glGetError - this will result in an error!
	// So all of Immediate* calls do not use OGL_CALL macro; instead we're just checking
	// for errors after glEnd.
	GLAssert();

	STATE.m_ImmediateVertices.clear();

	// stats
	switch( STATE.m_ImmediateMode ) {
	case kPrimitiveTriangles:
		m_Stats.AddDrawCall( vertexCount / 3, vertexCount );
		break;
	case kPrimitiveTriangleStripDeprecated:
		m_Stats.AddDrawCall( vertexCount - 2, vertexCount );
		break;
	case kPrimitiveQuads:
		m_Stats.AddDrawCall( vertexCount / 4 * 2, vertexCount );
		break;
	case kPrimitiveLines:
		m_Stats.AddDrawCall( vertexCount / 2, vertexCount );
		break;
	}
}

extern GLint gDefaultFBOGL;


void GLReadPixelsWrapper (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
	// When running in Safari 64-bit, we cannot just perform a normal glReadPixels.
	// This is because we are rendering into an FBO which is allocated by the Safari process.
	// Apparently there are some memory access restrictions, I have not been able to read from that fbo.
	// Also, reading from FBOs with FSAA is not possible.
	// We work around this by allocating an FBO of our own, blitting into that, and reading from there.

	GLuint fbo;
	GLuint fboTexture;
	GLint oldTexture;
	bool useWorkaround = (gDefaultFBOGL != 0);

	if (useWorkaround)
	{
		GLint curFBO;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &curFBO);
		if (curFBO != gDefaultFBOGL)
			useWorkaround = false;
	}
	if 	(useWorkaround)
	{
		if( !gGraphicsCaps.gl.hasFrameBufferBlit )
		{
			ErrorString ("Cannot read from image without framebuffer blit extension!");
			return;
		}

		glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);

		// Create FBO
		glGenTextures(1, &fboTexture);
		glBindTexture(GL_TEXTURE_2D, fboTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glGenFramebuffersEXT(1, &fbo);
		glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo );
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, fboTexture, 0);
		GLAssert();

		// Blit into it
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, gDefaultFBOGL);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, fbo);
		glBlitFramebufferEXT(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
		GLAssert();
	}

	glReadPixels( x, y, width, height, format, type, pixels );

	if 	(useWorkaround)
	{
		// Restore old state
		glBindTexture(GL_TEXTURE_2D, oldTexture);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, gDefaultFBOGL);

		// Destroy FBO
		glDeleteFramebuffersEXT( 1, &fbo );
		glDeleteTextures(1, &fboTexture);
		GLAssert();
	}
}

bool GFX_GL_IMPL::CaptureScreenshot( int left, int bottom, int width, int height, UInt8* rgba32 )
{
	GFX_LOG(("%d, %d, %d, %d", left, bottom, width, height));
	GLReadPixelsWrapper ( left, bottom, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba32 );
	return true;
}

bool GFX_GL_IMPL::ReadbackImage( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY )
{
	GFX_LOG(("%d, %d, %d, %d, %d, %d", left, bottom, width, height, destX, destY));
	return ReadbackTextureGL( image, left, bottom, width, height, destX, destY );
}

void GFX_GL_IMPL::GrabIntoRenderTexture (RenderSurfaceHandle rs, RenderSurfaceHandle rd, int x, int y, int width, int height )
{
	GFX_LOG(("%d, %d, %d, %d", x, y, width, height));
	::GrabIntoRenderTextureGL (rs, x, y, width, height, GetHelperFBO (STATE));
}

RenderTextureFormat	GFX_GL_IMPL::GetDefaultRTFormat() const	{ return kRTFormatARGB32; }
RenderTextureFormat	GFX_GL_IMPL::GetDefaultHDRRTFormat() const	{ return kRTFormatARGBHalf; }

void* GFX_GL_IMPL::GetNativeTexturePointer(TextureID id)
{
	return (void*)TextureIdMap::QueryNativeTexture(id);
}


void GFX_GL_IMPL::SetActiveContext (void* ctx)
{
	GraphicsContextGL* glctx = reinterpret_cast<GraphicsContextGL*> (ctx);
	ActivateGraphicsContextGL (*glctx, kGLContextSkipInvalidateState | kGLContextSkipUnbindObjects | kGLContextSkipFlush);
}


#if ENABLE_PROFILER

GfxTimerQuery* GFX_GL_IMPL::CreateTimerQuery()
{
	TimerQueryGL* query = new TimerQueryGL;
	return query;
}

void GFX_GL_IMPL::DeleteTimerQuery(GfxTimerQuery* query)
{
	delete query;
}

void GFX_GL_IMPL::BeginTimerQueries()
{
	if( !gGraphicsCaps.hasTimerQuery )
		return;

	g_TimerQueriesGL.BeginTimerQueries();
}

void GFX_GL_IMPL::EndTimerQueries()
{
	if (!gGraphicsCaps.hasTimerQuery )
		return;

	g_TimerQueriesGL.EndTimerQueries();
}

bool GFX_GL_IMPL::TimerQueriesIsActive()
{
	if (!gGraphicsCaps.hasTimerQuery )
		return false;

	return g_TimerQueriesGL.IsActive();
}

#endif // ENABLE_PROFILER

// -------- editor-only functions

#if UNITY_EDITOR

void GFX_GL_IMPL::SetAntiAliasFlag( bool aa )
{
	if( aa )
	{
		OGL_CALL(glHint (GL_LINE_SMOOTH_HINT, GL_NICEST));
		OGL_CALL(glEnable (GL_LINE_SMOOTH));
	}
	else
	{
		OGL_CALL(glHint (GL_LINE_SMOOTH_HINT, GL_FASTEST));
		OGL_CALL(glDisable (GL_LINE_SMOOTH));
	}
}


void GFX_GL_IMPL::DrawUserPrimitives( GfxPrimitiveType type, int vertexCount, UInt32 vertexChannels, const void* data, int stride )
{
	if( vertexCount == 0 )
		return;

	AssertIf( !data || vertexCount < 0 || vertexChannels == 0 );

	UnbindVertexBuffersGL();
	ClearActiveChannelsGL();
	int offset = 0;
	for( int i = 0; i < kShaderChannelCount; ++i )
	{
		if( !( vertexChannels & (1<<i) ) )
			continue;
		VertexComponent destComponent = kSuitableVertexComponentForChannel[i];
		SetChannelDataGL( (ShaderChannel)i, destComponent, (const UInt8*)data + offset, stride );
		offset += VBO::GetDefaultChannelByteSize(i);
	}
	ActivateChannelsGL();
	BeforeDrawCall(false);

	OGL_CALL(glDrawArrays(kTopologyGL[type], 0, vertexCount));

	// stats
	switch (type) {
	case kPrimitiveTriangles:
		m_Stats.AddDrawCall( vertexCount / 3, vertexCount );
		break;
	case kPrimitiveQuads:
		m_Stats.AddDrawCall( vertexCount / 4 * 2, vertexCount );
		break;
	case kPrimitiveLines:
		m_Stats.AddDrawCall( vertexCount / 2, vertexCount );
		break;
	case kPrimitiveLineStrip:
		m_Stats.AddDrawCall( vertexCount-1, vertexCount );
		break;
	}
}

int GFX_GL_IMPL::GetCurrentTargetAA() const
{
	int fsaa = 0;
	if( gGraphicsCaps.hasMultiSample )
		glGetIntegerv( GL_SAMPLES_ARB, &fsaa );
	return fsaa;
}

#if UNITY_WIN
	GfxDeviceWindow*	GFX_GL_IMPL::CreateGfxWindow( HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias )
	{
		return new GLWindow(window, width, height, depthFormat, antiAlias);
	}
#endif


#endif // UNITY_EDITOR



// --------------------------------------------------------------------------
// -------- verification of state



#if GFX_DEVICE_VERIFY_ENABLE


void VerifyGLState(GLenum gl, float val, const char *str);
#define VERIFY(s,t) VerifyGLState (s, t, #s " (" #t ")")
void VerifyGLStateI(GLenum gl, int val, const char *str);
#define VERIFYI(s,t) VerifyGLStateI (s, t, #s " (" #t ")")
void VerifyGLState4(GLenum gl, const float *val, const char *str);
#define VERIFY4(s,t) VerifyGLState4 (s, t, #s " (" #t ")")

void VerifyEnabled(GLenum gl, bool val, const char *str);
#define VERIFYENAB(s,t) VerifyEnabled ( s, t, #s " (" #t ")")

void VerifyMaterial (GLenum gl, const float *val, const char *str);
#define VERIFYMAT(s,t) VerifyMaterial ( s, t, #s " (" #t ")")

void VerifyTexParam (int target, GLenum gl, float val, const char *str);
#define VERIFYTP(s,t) VerifyTexParam (target, s, t, #s " (" #t ")")

void VerifyTexEnv (GLenum target, GLenum gl, int val, const char *str);
#define VERIFYTE(g,s,t) VerifyTexEnv (g, s, t, #s " (" #t ")")

void VerifyTexEnv4 (GLenum target, GLenum gl, const float *val, const char *str);
#define VERIFYTE4(g,s,t) VerifyTexEnv4(g, s, t, #s " (" #t ")")

void VerifyTexGen (GLenum coord, GLenum gl, int val, const char *str);
#define VERIFYTG(c,s,t) VerifyTexGen (c, s, t, #s " (" #t ")")


static void VERIFY_PRINT( const char* format, ... )
{
	va_list va;
	va_start (va, format);
	ErrorString( VFormat( format, va ) );
	va_end (va);
}

const float kVerifyDelta = 0.0001f;

void VerifyGLState(GLenum gl, float val, const char *str)
{
	float temp;
	glGetFloatv (gl, &temp);
	if( !CompareApproximately(temp,val,kVerifyDelta) ) {
		VERIFY_PRINT ("%s differs from cache (%f != %f)\n", str, val, temp);
	}
}

void VerifyGLStateI(GLenum gl, int val, const char *str)
{
	int temp;
	glGetIntegerv (gl, &temp);
	if (temp != val) {
		VERIFY_PRINT ("%s differs from cache (0x%x != 0x%x)\n", str, val, temp);
	}
}

void VerifyGLState4 (GLenum gl, const float *val, const char *str)
{
	GLfloat temp[4];
	glGetFloatv (gl, temp);
	if( !CompareApproximately(temp[0],val[0],kVerifyDelta) ||
		!CompareApproximately(temp[1],val[1],kVerifyDelta) ||
		!CompareApproximately(temp[2],val[2],kVerifyDelta) ||
		!CompareApproximately(temp[3],val[3],kVerifyDelta) )
	{
		VERIFY_PRINT ("%s differs from cache (%f,%f,%f,%f) != (%f,%f,%f,%f)\n", str, val[0],val[1],val[2],val[3], temp[0],temp[1],temp[2],temp[3]);
	}
}

void VerifyEnabled(GLenum gl, bool val, const char *str)
{
	bool temp = glIsEnabled (gl) ? true : false;
	if (temp != val) {
		VERIFY_PRINT ("%s differs from cache (%d != %d)\n", str, val, temp);
	}
}

void VerifyMaterial (GLenum gl, const float *val, const char *str)
{
	GLfloat temp[4];
	glGetMaterialfv (GL_FRONT, gl, temp);
	if( !CompareApproximately(temp[0],val[0],kVerifyDelta) ||
		!CompareApproximately(temp[1],val[1],kVerifyDelta) ||
		!CompareApproximately(temp[2],val[2],kVerifyDelta) ||
		!CompareApproximately(temp[3],val[3],kVerifyDelta) )
	{
		VERIFY_PRINT ("%s differs from cache (%f,%f,%f,%f) != (%f,%f,%f,%f)\n",str, val[0],val[1],val[2],val[3], temp[0],temp[1],temp[2],temp[3]);
	}
}

void VerifyTexParam (int target, GLenum gl, float val, const char *str)
{
	float temp;
	glGetTexParameterfv (target, gl, &temp);
	if( !CompareApproximately(temp,val,kVerifyDelta) ) {
		VERIFY_PRINT ("%s differs from cache (%f != %f)\n", str, val, temp);
	}
}

void VerifyTexEnv (GLenum target, GLenum gl, int val, const char *str)
{
	GLint temp;
	glGetTexEnviv (target, gl, &temp);
	if (temp != val) {
		VERIFY_PRINT ("%s differs from cache (%d != %d)\n", str, (int)val, (int)temp);
	}
}

void VerifyTexEnv4( GLenum target, GLenum gl, const float *val, const char *str )
{
	GLfloat temp[4];
	glGetTexEnvfv(target, gl, temp);
	float val0 = clamp01(val[0]);
	float val1 = clamp01(val[1]);
	float val2 = clamp01(val[2]);
	float val3 = clamp01(val[3]);
	if( !CompareApproximately(temp[0],val0,kVerifyDelta) ||
		!CompareApproximately(temp[1],val1,kVerifyDelta) ||
		!CompareApproximately(temp[2],val2,kVerifyDelta) ||
		!CompareApproximately(temp[3],val3,kVerifyDelta) )
	{
		VERIFY_PRINT ("%s differs from cache (%f,%f,%f,%f) != (%f,%f,%f,%f)\n", str, val0,val1,val2,val3, temp[0],temp[1],temp[2],temp[3]);
	}
}


void VerifyTexGen(GLenum coord, GLenum gl, int val, const char *str)
{
	GLint temp;
	glGetTexGeniv (coord, gl, &temp);
	if (temp != val) {
		VERIFY_PRINT ("TexGen %s differs from cache (%d != %d)\n", str, val, (int)temp);
	}
}


void GFX_GL_IMPL::VerifyState()
{
	STATE.Verify();

	if( m_FogParams.mode != kFogUnknown )
	{
		VERIFYENAB( GL_FOG, m_FogParams.mode != 0 );
		//VERIFY( GL_FOG_MODE, GetFogMode()[m_FogParams.mode] ); // TBD
		if( m_FogParams.mode != kFogDisabled )
		{
			VERIFY( GL_FOG_START, m_FogParams.start );
			VERIFY( GL_FOG_END, m_FogParams.end );
			VERIFY( GL_FOG_DENSITY, m_FogParams.density );
			float clampedFogColor[4];
			clampedFogColor[0] = clamp01(m_FogParams.color.x);
			clampedFogColor[1] = clamp01(m_FogParams.color.y);
			clampedFogColor[2] = clamp01(m_FogParams.color.z);
			clampedFogColor[3] = clamp01(m_FogParams.color.w);

			VERIFY4( GL_FOG_COLOR, clampedFogColor );
		}
	}
}


void DeviceStateGL::Verify()
{
	GLAssert();
	if( depthFunc != kFuncUnknown ) {
		VERIFYENAB( GL_DEPTH_TEST, depthFunc != kFuncDisabled );
		if( depthFunc != kFuncDisabled ) {
			VERIFY( GL_DEPTH_FUNC, kCmpFuncGL[depthFunc] );
		}
	}
	if( depthWrite != -1 ) {
		GLboolean temp;
		glGetBooleanv( GL_DEPTH_WRITEMASK, &temp );
		if( temp != depthWrite )
			printf_console( "DepthWrite mismatch (%d != %d)\n", (int)temp, (int)depthWrite );
	}
	if( blending != -1 ) {
		VERIFYENAB( GL_BLEND, blending != 0 );
		if( blending ) {
			VERIFYI( GL_BLEND_SRC, srcBlend );
			VERIFYI( GL_BLEND_DST, destBlend );
			VERIFYI( GL_BLEND_EQUATION_RGB, blendOp );
			if( gGraphicsCaps.hasSeparateAlphaBlend ) {
				VERIFYI( GL_BLEND_SRC_ALPHA, srcBlendAlpha );
				VERIFYI( GL_BLEND_DST_ALPHA, destBlendAlpha );
				VERIFYI( GL_BLEND_EQUATION_ALPHA, blendOpAlpha );
			}

		}
	}

	if( alphaFunc != kFuncUnknown ) {
		VERIFYENAB( GL_ALPHA_TEST, alphaFunc != kFuncDisabled );
		if( alphaFunc != kFuncDisabled ) {
			VERIFY( GL_ALPHA_TEST_FUNC, kCmpFuncGL[alphaFunc] );
			if( alphaValue != -1 )
				VERIFY( GL_ALPHA_TEST_REF, alphaValue );
		}
	}

	if( culling != kCullUnknown ) {
		VERIFYENAB( GL_CULL_FACE, culling != kCullOff );
		switch (culling) {
			case kCullOff: break;
			case kCullFront: VERIFY (GL_CULL_FACE_MODE, GL_FRONT); break;
			case kCullBack:  VERIFY (GL_CULL_FACE_MODE, GL_BACK);  break;
			default: printf_console( "Invalid face cull mode: %d\n", (int)culling ); break;
		}
	}
	VERIFY( GL_FRONT_FACE, (appBackfaceMode != userBackfaceMode) ? GL_CCW : GL_CW );

	if( scissor != -1 ) {
		VERIFYENAB( GL_SCISSOR_TEST, scissor != 0 );
	}

	if( lighting != -1 ) {
		VERIFYENAB( GL_LIGHTING, lighting != 0 );
		if( lighting && colorMaterial == kColorMatDisabled ) {
			if( matAmbient.x != -1.0f )
				VERIFYMAT( GL_AMBIENT, matAmbient.GetPtr() );
			if( matDiffuse.x != -1.0f )
				VERIFYMAT( GL_DIFFUSE, matDiffuse.GetPtr() );
			if( matSpecular.x != -1.0f )
				VERIFYMAT( GL_SPECULAR, matSpecular.GetPtr() );
			if( matEmissive.x != -1.0f )
				VERIFYMAT( GL_EMISSION, matEmissive.GetPtr() );
			if( matShininess != -1.0f )
			{
				GLfloat temp;
				glGetMaterialfv( GL_FRONT, GL_SHININESS, &temp );
				if( !CompareApproximately(temp, matShininess * 128.0f, kVerifyDelta) )
					printf_console( "Shininess mismatch: %f != %f\n", (float)temp, (float)matShininess );
			}
		}
	}

	// verify bound/enabled programs
	for( int type = kShaderVertex; type < kShaderTypeCount; ++type )
	{
		ShaderImplType currType = shaderEnabledImpl[type];
		if( currType == kShaderImplBoth )
		{
			GLhandleARB gl = glGetHandleARB( GL_PROGRAM_OBJECT_ARB );
			if( gl != shaderEnabledID[type] )
			{
				VERIFY_PRINT( "GLSL program differs from cache (%d != %d)\n", gl, shaderEnabledID[type] );
			}
		}
		else if( currType == kShaderImplUndefined )
		{
			AssertIf( shaderEnabledID[type] != -1 && shaderEnabledID[type] != 0 );
			if( shaderEnabledID[type] == 0 )
			{
				if( type == kShaderVertex )
				{
					VERIFYENAB( GL_VERTEX_PROGRAM_ARB, false );
				}
				else
				{
					VERIFYENAB( GL_FRAGMENT_PROGRAM_ARB, false );
				}
			}
		}
		else
		{
			GLenum gltarget = GetGLShaderImplTarget( currType );
			VERIFYENAB( gltarget, true );
			int gl;
			glGetProgramivARB( gltarget, GL_PROGRAM_BINDING_ARB, &gl );
			if( gl != shaderEnabledID[type] )
			{
				VERIFY_PRINT( "Program #%i differs from cache (%d != %d)\n", type, gl, shaderEnabledID[type] );
			}
		}
	}

	// Verify all texture units
	for( int i = 0; i < gGraphicsCaps.maxTexUnits; ++i ) {
		textures[i].Verify(i);
	}
}

static const unsigned long kTextureBindingTable[6] = {0, GL_TEXTURE_BINDING_1D, GL_TEXTURE_BINDING_2D, GL_TEXTURE_BINDING_3D, GL_TEXTURE_BINDING_CUBE_MAP_ARB};

void TextureUnitStateGL::Verify( int texUnit )
{
	if( gGraphicsCaps.maxTexUnits <= 1 ) // do not crash on no-multi-texture machines
		return;

	if( texDim == kTexDimUnknown )
		return;

	glActiveTextureARB(GL_TEXTURE0_ARB + texUnit);

	VERIFYI(GL_TEXTURE_2D, texDim == kTexDim2D);
	VERIFYI(GL_TEXTURE_CUBE_MAP_ARB, texDim == kTexDimCUBE);
	if( gGraphicsCaps.has3DTexture ) {
		VERIFYI(GL_TEXTURE_3D, texDim == kTexDim3D);
	}

	if( color.x != -1 && color.y != -1 && color.z != -1 && color.w != -1 )
		VERIFYTE4( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color.GetPtr() );

	if( texDim != kTexDimNone && texID != -1 )
		VERIFYI( kTextureBindingTable[texDim], texID );

	if( texGen != kTexGenUnknown )
	{
		switch( texGen )
		{
		case kTexGenDisabled:
			VERIFYENAB( GL_TEXTURE_GEN_S, false );
			VERIFYENAB( GL_TEXTURE_GEN_T, false );
			VERIFYENAB( GL_TEXTURE_GEN_R, false );
			break;
		case kTexGenSphereMap:
			VERIFYTG(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
			VERIFYTG(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
			VERIFYENAB( GL_TEXTURE_GEN_S, true );
			VERIFYENAB( GL_TEXTURE_GEN_T, true );
			VERIFYENAB( GL_TEXTURE_GEN_R, false );
			break;
		case kTexGenObject: {
			VERIFYTG(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
			VERIFYTG(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
			VERIFYTG(GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
			VERIFYENAB( GL_TEXTURE_GEN_S, true );
			VERIFYENAB( GL_TEXTURE_GEN_T, true );
			VERIFYENAB( GL_TEXTURE_GEN_R, true );
			break;
		}
		case kTexGenEyeLinear: {
			VERIFYTG(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
			VERIFYTG(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
			VERIFYTG(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
			VERIFYENAB( GL_TEXTURE_GEN_S, true );
			VERIFYENAB( GL_TEXTURE_GEN_T, true );
			VERIFYENAB( GL_TEXTURE_GEN_R, true );
			break;
		}
		case kTexGenCubeReflect:
			VERIFYTG(GL_S, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
			VERIFYTG(GL_T, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
			VERIFYTG(GL_R, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
			VERIFYENAB( GL_TEXTURE_GEN_S, true );
			VERIFYENAB( GL_TEXTURE_GEN_T, true );
			VERIFYENAB( GL_TEXTURE_GEN_R, true );
			break;
		case kTexGenCubeNormal:
			VERIFYTG(GL_S, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP_ARB);
			VERIFYTG(GL_T, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP_ARB);
			VERIFYTG(GL_R, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP_ARB);
			VERIFYENAB( GL_TEXTURE_GEN_S, true );
			VERIFYENAB( GL_TEXTURE_GEN_T, true );
			VERIFYENAB( GL_TEXTURE_GEN_R, true );
			break;
		}
	}
}


#endif // GFX_DEVICE_VERIFY_ENABLE
