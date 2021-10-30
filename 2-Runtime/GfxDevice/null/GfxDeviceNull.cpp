#include "UnityPrefix.h"
#include "GfxDeviceNull.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Graphics/RenderSurface.h"
#include "GfxNullVBO.h"
#include "Runtime/GfxDevice/GfxDeviceWindow.h"
#include "External/shaderlab/Library/program.h"


static DeviceBlendState g_NullBlendState;
static DeviceDepthState g_NullDepthState;
static DeviceStencilState g_NullStencilState;
static DeviceRasterState g_NullRasterState;

static float g_NullWorldMatrix[16];
static float g_NullViewMatrix[16];
static float g_NullProjectionMatrix[16];

static RenderSurfaceBase	_ColorDefaultSurface;
static RenderSurfaceBase	_DepthDefaultSurface;


void GraphicsCaps::InitNull(void)
{
	rendererString = "Null Device";
	vendorString = "Unity Technologies";
	driverVersionString = "1.0";
	fixedVersionString = "NULL 1.0 [1.0]";
	driverLibraryString = "(null)";
	videoMemoryMB = 64.0f;
	rendererID = 0;
	vendorID = 0;

	printf_console( "NullGfxDevice:\n" );
	printf_console( "    Version:  %s\n", fixedVersionString.c_str() );
	printf_console( "    Renderer: %s\n", rendererString.c_str() );
	printf_console( "    Vendor:   %s\n", vendorString.c_str() );

	shaderCaps = kShaderLevel2;

	maxLights = 8;
	hasAnisoFilter = false;
	maxAnisoLevel = 0;
	hasMipLevelBias = false;

	hasMultiSample = false;

	hasBlendSquare = false;
	hasSeparateAlphaBlend = false;

	hasS3TCCompression = false;

	hasAutoMipMapGeneration = false;

	maxTexImageUnits = 2;
	maxTexCoords = 2;


	maxTexUnits = 2;

	maxTextureSize = 1024;
	maxCubeMapSize = 0;
	maxRenderTextureSize = 0;

	for (int i = 0; i < kRTFormatCount; ++i)
	{
		supportsRenderTextureFormat[i] = false;
	}

	has3DTexture = false;
	npotRT = npot = kNPOTNone;

	hasRenderToTexture = false;
	hasRenderToCubemap = false;
	hasRenderTargetStencil = false;
	hasTwoSidedStencil = false;
	hasHighPrecisionTextureCombiners = false;
	hasNativeDepthTexture = false;
	hasStencilInDepthTexture = false;
	hasNativeShadowMap = false;

	needsToSwizzleVertexColors = false;
}


GfxDevice* CreateNullGfxDevice()
{
	::gGraphicsCaps.InitNull();

	return UNITY_NEW_AS_ROOT(GfxDeviceNull(), kMemGfxDevice, "NullGfxDevice","");
}

GfxDeviceNull::GfxDeviceNull(void)
:	dynamicVBO(NULL)
{
	InvalidateState();
	ResetFrameStats();

	m_Renderer = kGfxRendererNull;
	m_UsesOpenGLTextureCoords = false;
	m_UsesHalfTexelOffset = false;
	m_MaxBufferedFrames = -1; // no limiting

	RenderSurfaceBase_InitColor(_ColorDefaultSurface);
	_ColorDefaultSurface.backBuffer = true;
	SetBackBufferColorSurface(&_ColorDefaultSurface);

	RenderSurfaceBase_InitDepth(_DepthDefaultSurface);
	_DepthDefaultSurface.backBuffer = true;
	SetBackBufferDepthSurface(&_DepthDefaultSurface);
}

GfxDeviceNull::~GfxDeviceNull(void)
{
	delete this->dynamicVBO;
}

void GfxDeviceNull::GetMatrix( float outMatrix[16] ) const
{
	for( int i = 0; i < 16; ++i )
		outMatrix[i] = 0.0f;
}


void GfxDeviceNull::GetViewport( int* port ) const
{
	AssertIf(!port);
	port[0] = 0;
	port[1] = 0;
	port[2] = 1;
	port[3] = 1;
}


void GfxDeviceNull::GetScissorRect( int scissor[4] ) const
{
	AssertIf(!scissor);
	scissor[0] = 0;
	scissor[1] = 0;
	scissor[2] = 1;
	scissor[3] = 1;
}

TextureCombinersHandle GfxDeviceNull::CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular )
{
	return TextureCombinersHandle(this); // just have to pass non-NULL to the handle
}

void GfxDeviceNull::DeleteTextureCombiners( TextureCombinersHandle& textureCombiners )
{
	textureCombiners.Reset();
}

void GfxDeviceNull::DestroySubProgram( ShaderLab::SubProgram* subprogram )
{
	delete subprogram;
}

VBO* GfxDeviceNull::CreateVBO()
{
	VBO* vbo = new GfxNullVBO();
	OnCreateVBO(vbo);
	return vbo;
}

void GfxDeviceNull::DeleteVBO( VBO* vbo )
{
	AssertIf(!vbo);
	OnDeleteVBO(vbo);
	delete vbo;
}

DynamicVBO&	GfxDeviceNull::GetDynamicVBO()
{
	if (NULL == this->dynamicVBO)
	{
		this->dynamicVBO = new GfxDynamicNullVBO();
	}

	return *this->dynamicVBO;
}



bool GfxDeviceNull::CaptureScreenshot( int left, int bottom, int width, int height, UInt8* rgba32 )
{
	return true;
}


bool GfxDeviceNull::ReadbackImage( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY )
{
	return true;
}

DeviceBlendState* GfxDeviceNull::CreateBlendState(const GfxBlendState& state)
{
	return & g_NullBlendState;
}

DeviceDepthState* GfxDeviceNull::CreateDepthState(const GfxDepthState& state)
{
	return & g_NullDepthState;
}

DeviceStencilState* GfxDeviceNull::CreateStencilState(const GfxStencilState& state)
{
	return & g_NullStencilState;
}

DeviceRasterState* GfxDeviceNull::CreateRasterState(const GfxRasterState& state)
{
	return & g_NullRasterState;
}

const float* GfxDeviceNull::GetWorldMatrix() const
{
	return g_NullWorldMatrix;
}

const float* GfxDeviceNull::GetViewMatrix() const
{
	return g_NullViewMatrix;
}

const float* GfxDeviceNull::GetProjectionMatrix() const
{
	return g_NullProjectionMatrix;
}



RenderSurfaceHandle GfxDeviceNull::CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags)
{
	return RenderSurfaceHandle(&_ColorDefaultSurface);
}
RenderSurfaceHandle GfxDeviceNull::CreateRenderDepthSurface (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags)
{
	return RenderSurfaceHandle(&_DepthDefaultSurface);
}

// -------- editor only functions

#if UNITY_EDITOR

GfxDeviceWindow*	GfxDeviceNull::CreateGfxWindow( HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias )
{
	return new GfxDeviceWindow(window,width, height, depthFormat, antiAlias);
}

#endif
