#include "UnityPrefix.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "UnityGL.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "GLAssert.h"
#include "GLContext.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Graphics/RenderSurface.h"
#include "TexturesGL.h"
#include "TextureIdMapGL.h"

#if UNITY_OSX
#include <OpenGL/OpenGL.h>
#elif UNITY_WIN
#include <windows.h>
#include "PlatformDependent/Win/wglext.h"
#elif UNITY_LINUX
#include <GL/glext.h>
#else
#error "Unknown platform"
#endif

#define GL_RT_COMMON_GL 1
#include "Runtime/GfxDevice/GLRTCommon.h"
#undef GL_RT_COMMON_GL


extern GLint gDefaultFBOGL = 0;

// define to 1 to print lots of activity info
#define DEBUG_RENDER_TEXTURES 0


struct RenderColorSurfaceGL : public RenderSurfaceBase
{
	GLuint		m_ColorBuffer;
	RenderTextureFormat	format;
	TextureDimension dim;
};

struct RenderDepthSurfaceGL : public RenderSurfaceBase
{
	GLuint		m_DepthBuffer;
	DepthBufferFormat depthFormat;
};

const unsigned long* GetGLTextureDimensionTable(); // GfxDeviceGL.cpp


static RenderColorSurfaceGL* s_ActiveColorTargets[kMaxSupportedRenderTargets];
static int s_ActiveColorTargetCount = 0;
static int s_AttachedColorTargetCount = 0;
static RenderDepthSurfaceGL* s_ActiveDepthTarget = NULL;
static int s_ActiveMip = 0;
static CubemapFace s_ActiveFace = kCubeFaceUnknown;

static const char* GetFBOStatusError( GLenum status )
{
	Assert(status != GL_FRAMEBUFFER_COMPLETE_EXT); // should not be called when everything is ok...
	switch( status )
	{
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT: return "INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT: return "INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT: return "INCOMPLETE_DUPLICATE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT: return "INCOMPLETE_DIMENSIONS";
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT: return "INCOMPLETE_FORMATS";
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT: return "INCOMPLETE_DRAW_BUFFER";
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT: return "INCOMPLETE_READ_BUFFER";
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT: return "UNSUPPORTED";
		default: return "unknown error";
	}
}

static void DetachPreviousColorTargets (int startIndex, int endIndex)
{
	// Detach color buffers from the attachments that were not touched this time
	for (int i = startIndex; i < endIndex; ++i)
	{
		GLenum colorAttach = GL_COLOR_ATTACHMENT0_EXT + i;

		// Even though we were attaching a 2D texture, a cubemap face or a render buffer,
		// we can detach either of them like a 2D texture.
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, colorAttach, GL_TEXTURE_2D, 0, 0);
	}
}

bool SetRenderTargetGL (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, GLuint globalSharedFBO)
{
	RenderColorSurfaceGL* rcolorZero = reinterpret_cast<RenderColorSurfaceGL*>(colorHandles[0].object);
	RenderDepthSurfaceGL* rdepth = reinterpret_cast<RenderDepthSurfaceGL*>( depthHandle.object );

	#if DEBUG_RENDER_TEXTURES
	printf_console( "RT: SetRenderTargetGL color=%i depth=%i mip=%i face=%i\n",
				   rcolorZero ? rcolorZero->textureID.m_ID : 0,
				   rdepth ? rdepth->textureID.m_ID : 0,
				   mipLevel, face );
	#endif

	// Exit if nothing to do
	if (count == s_ActiveColorTargetCount && s_ActiveDepthTarget == rdepth && s_ActiveMip == mipLevel && s_ActiveFace == face)
	{
		bool colorsSame = true;
		for (int i = 0; i < count; ++i)
		{
			if (s_ActiveColorTargets[i] != reinterpret_cast<RenderColorSurfaceGL*>(colorHandles[i].object))
				colorsSame = false;
		}
		if (colorsSame)
			return false;
	}

	Assert (gGraphicsCaps.hasRenderToTexture);
	GetRealGfxDevice().GetFrameStats().AddRenderTextureChange(); // stats

	// cant mix default fbo and user fbo
	Assert(colorHandles[0].IsValid() && depthHandle.IsValid());
	Assert(colorHandles[0].object->backBuffer == depthHandle.object->backBuffer);


	if (!rcolorZero->backBuffer)
	{
		// color surfaces
		glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, globalSharedFBO);
		GLenum drawBuffers[kMaxSupportedRenderTargets];
		bool readBufferNone = false;
		for (int i = 0; i < count; ++i)
		{
			GLenum colorAttach = GL_COLOR_ATTACHMENT0_EXT + i;
			RenderColorSurfaceGL* rcolor = reinterpret_cast<RenderColorSurfaceGL*>(colorHandles[i].object);
			Assert (rcolor->colorSurface);
			if (rcolor->textureID.m_ID)
			{
			#if DEBUG_RENDER_TEXTURES
				printf_console( "  RT: color buffer texture %i\n", rcolor->textureID.m_ID );
			#endif
				drawBuffers[i] = colorAttach;
				GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(rcolor->textureID);
				if (rcolor->dim == kTexDimCUBE)
				{
					glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, colorAttach, GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + clamp<int>(face,0,5), targetTex, mipLevel );
				}
				else
				{
					glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, colorAttach, GL_TEXTURE_2D, targetTex, mipLevel );
				}
			}
			else
			{
				if (rcolor->m_ColorBuffer)
				{
					#if DEBUG_RENDER_TEXTURES
					printf_console( "  RT: color buffer plain %i\n", rcolor->m_ColorBuffer );
					#endif
					glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, colorAttach, GL_RENDERBUFFER_EXT, rcolor->m_ColorBuffer);
					drawBuffers[i] = colorAttach;
				}
				else
				{
					#if DEBUG_RENDER_TEXTURES
					printf_console( "  RT: color buffer none\n" );
					#endif
					glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, colorAttach, GL_TEXTURE_2D, 0);
					drawBuffers[i] = GL_NONE;
					if (i == 0)
						readBufferNone = true;
				}
			}
		}

		DetachPreviousColorTargets (count, s_AttachedColorTargetCount);
		s_AttachedColorTargetCount = count;

		if (count <= 1 || gGraphicsCaps.maxMRTs <= 1)
			glDrawBuffer (drawBuffers[0]);
		else
			glDrawBuffersARB (count, drawBuffers);
		glReadBuffer (readBufferNone ? GL_NONE : GL_COLOR_ATTACHMENT0_EXT);

		// depth surface
		Assert (!rdepth->colorSurface);
		bool bindDepthTexture = true;
		if (gGraphicsCaps.gl.buggyPackedDepthStencil)
		{
			if (rcolorZero && rcolorZero->textureID.m_ID && rdepth->depthFormat == kDepthFormat24)
			{
				bindDepthTexture = false;
			}
		}

		if (rdepth->textureID.m_ID && bindDepthTexture)
		{
		#if DEBUG_RENDER_TEXTURES
			printf_console( "  RT: depth buffer texture %i\n", rdepth->textureID.m_ID );
		#endif

			GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(rdepth->textureID);

			glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, targetTex, 0);
			if (gGraphicsCaps.hasRenderTargetStencil && rdepth->depthFormat == kDepthFormat24 && !gGraphicsCaps.gl.buggyPackedDepthStencil)
				glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, targetTex, 0);
			else
				glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0);
		}
		else
		{
			#if DEBUG_RENDER_TEXTURES
			printf_console( "  RT: depth buffer plain %i\n", rdepth->m_DepthBuffer );
			#endif
			glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rdepth->m_DepthBuffer);
			if (gGraphicsCaps.hasRenderTargetStencil && rdepth->depthFormat == kDepthFormat24)
				glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rdepth->m_DepthBuffer);
			else
				glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
		}

		#if GFX_DEVICE_VERIFY_ENABLE
		GLenum status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
		if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
		{
			std::string msg = Format("OpenGL failed to activate FBO: %s (color tex %i buf %i fmt %i) (depth tex %i buf %i fmt %i)",
				GetFBOStatusError(status),
				rcolorZero->textureID.m_ID,
				rcolorZero->m_ColorBuffer,
				rcolorZero->format,
				rdepth->textureID.m_ID,
				rdepth->m_DepthBuffer,
				rdepth->depthFormat
			);
			AssertString (msg);
		}
		#endif
	}
	else
	{
		Assert (rcolorZero->backBuffer && rdepth->backBuffer); // OpenGL can't mix FBO and native window at once
		#if DEBUG_RENDER_TEXTURES
		printf_console( "  RT: backbuffer\n" );
		#endif
		glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, gDefaultFBOGL );
	}

	// If we previously had a mip-mapped render texture, generate mip levels
	// for it now. Just using GL_GENERATE_MIPMAP does not work on OS X, and the explicit
	// should probably be faster.
	if (s_ActiveColorTargets[0] &&
		(s_ActiveColorTargets[0]->flags & kSurfaceCreateMipmap) &&
		(s_ActiveColorTargets[0]->flags & kSurfaceCreateAutoGenMips))
	{
		const int textureTarget = GetGLTextureDimensionTable()[s_ActiveColorTargets[0]->dim];
		GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, s_ActiveColorTargets[0]->textureID, s_ActiveColorTargets[0]->dim, std::numeric_limits<float>::infinity());
		glGenerateMipmapEXT(textureTarget);
	}

	for (int i = 0; i < count; ++i)
		s_ActiveColorTargets[i] = reinterpret_cast<RenderColorSurfaceGL*>(colorHandles[i].object);
	s_ActiveColorTargetCount = count;
	s_ActiveDepthTarget = rdepth;
	s_ActiveMip = mipLevel;
	s_ActiveFace = face;
	return true;
}

RenderSurfaceHandle GetActiveRenderColorSurfaceGL(int index)
{
	return RenderSurfaceHandle(s_ActiveColorTargets[index]);
}
RenderSurfaceHandle GetActiveRenderDepthSurfaceGL()
{
	return RenderSurfaceHandle(s_ActiveDepthTarget);
}

bool IsActiveRenderTargetWithColorGL()
{
	return !s_ActiveColorTargets[0] || s_ActiveColorTargets[0]->backBuffer || !IsDepthRTFormat (s_ActiveColorTargets[0]->format);
}

static int GetBytesFromGLFormat( GLenum fmt )
{
	switch(fmt) {
	case 0: return 0;
	case GL_RGBA4: return 2;
	case GL_RGB5: return 2;
	case GL_RGBA8: return 4;
	case GL_DEPTH_COMPONENT16: return 2;
	case GL_DEPTH_COMPONENT24: return 4; // all hardware uses 4 bytes for 24 bit depth
	case GL_DEPTH_COMPONENT32: return 4;
	case GL_DEPTH24_STENCIL8_EXT: return 4;
	default:
		AssertString("unknown GL format");
		return 0;
	}
}

static void CreateFBORenderColorSurfaceGL (RenderColorSurfaceGL& rs)
{
	int textureTarget = GetGLTextureDimensionTable()[rs.dim];

	if( !IsDepthRTFormat(rs.format) )
	{
		GLenum internalFormat	= (rs.flags & kSurfaceCreateSRGB) ? RTColorInternalFormatSRGBGL(rs.format) : RTColorInternalFormatGL(rs.format);
		GLenum textureFormat	= RTColorTextureFormatGL(rs.format);
		GLenum textureType		= RTColorTextureTypeGL(rs.format);

		// Create texture to render for color
		GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, rs.textureID, rs.dim, std::numeric_limits<float>::infinity());
		if (rs.dim == kTexDimCUBE)
		{
			// cubemap: initialize all faces
			for( int f = 0; f < 6; ++f )
			{
				glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + f, 0, internalFormat, rs.width, rs.height, 0, textureFormat, textureType, NULL );
			}
			if (rs.flags & kSurfaceCreateMipmap)
			{
				// establish mip map chain if needed
				glGenerateMipmapEXT (textureTarget);
			}
		}
		else if (!rs.textureID.m_ID)
		{
			// MSAA render buffer
			glGenRenderbuffersEXT( 1, &rs.m_ColorBuffer );
			glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, rs.m_ColorBuffer );
			for (int samples = std::min(rs.samples, gGraphicsCaps.gl.maxSamples); samples >= 1; samples--)
			{
				if (samples > 1)
				{
					glRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, samples, internalFormat, rs.width, rs.height );
					if (glGetError() == GL_NO_ERROR)
						break;
				}
				else
				{
					glRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, internalFormat, rs.width, rs.height );
					GLAssert();
				}
			}
		}
		else
		{
			// regular texture: initialize
			glTexImage2D( textureTarget, 0, internalFormat, rs.width, rs.height, 0, textureFormat, textureType, NULL );
			if (rs.flags & kSurfaceCreateMipmap)
			{
				// establish mip map chain if needed
				glGenerateMipmapEXT (textureTarget);
			}
		}
	}
	else
	{
		// Depth render texture
		// We're prefer to just have no color buffer, but some GL implementations don't like that
		// (PPC NVIDIA drivers on OS X). So if we have that, we try color buffers starting from
		// low-memory formats.
		if (gGraphicsCaps.gl.forceColorBufferWithDepthFBO)
		{
			glGenRenderbuffersEXT( 1, &rs.m_ColorBuffer );
			glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, rs.m_ColorBuffer );
			glRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, GL_RGBA8, rs.width, rs.height );
		}
	}
}


static void CreateFBORenderDepthSurfaceGL (RenderDepthSurfaceGL& rs)
{
	int textureTarget = GetGLTextureDimensionTable()[kTexDim2D];

	GLenum depthFormat;
	GLenum genericFormat;
	GLenum typeFormat;
	if (rs.depthFormat == kDepthFormat16)
	{
		depthFormat = GL_DEPTH_COMPONENT16;
		genericFormat = GL_DEPTH_COMPONENT;
		typeFormat = GL_UNSIGNED_BYTE;
	}
	else if (gGraphicsCaps.hasRenderTargetStencil && rs.depthFormat == kDepthFormat24)
	{
		depthFormat = GL_DEPTH24_STENCIL8_EXT;
		genericFormat = GL_DEPTH_STENCIL_EXT;
		typeFormat = GL_UNSIGNED_INT_24_8_EXT;
	}
	else
	{
		depthFormat = GL_DEPTH_COMPONENT24;
		genericFormat = GL_DEPTH_COMPONENT;
		typeFormat = GL_UNSIGNED_BYTE;
	}
	if (depthFormat == GL_DEPTH_COMPONENT16 && gGraphicsCaps.gl.force24DepthForFBO)
		depthFormat = GL_DEPTH_COMPONENT24;
	if (!rs.textureID.m_ID)
	{
		// create just depth surface
		if( rs.depthFormat != kDepthFormatNone )
		{
			glGenRenderbuffersEXT( 1, &rs.m_DepthBuffer );
			glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, rs.m_DepthBuffer );
			for (int samples = std::min(rs.samples, gGraphicsCaps.gl.maxSamples); samples >= 1; samples--)
			{
				if (samples > 1)
				{
					glRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, samples, depthFormat, rs.width, rs.height );
					if (glGetError() == GL_NO_ERROR)
						break;
				}
				else
				{
					glRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, depthFormat, rs.width, rs.height );
					GLAssert();
				}
			}
		}
	}
	else
	{
		// create depth texture

		// If packed depth stencil is buggy as a texture, create just the depth texture. We still need packed depth
		// stencil for the non-texture case, so do not do this above in the format selection.
		if (gGraphicsCaps.gl.buggyPackedDepthStencil && genericFormat == GL_DEPTH_STENCIL_EXT)
		{
			depthFormat = GL_DEPTH_COMPONENT24;
			genericFormat = GL_DEPTH_COMPONENT;
			typeFormat = GL_UNSIGNED_BYTE;
		}

		GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, rs.textureID, kTexDim2D, std::numeric_limits<float>::infinity());
		glTexImage2D( textureTarget, 0, depthFormat, rs.width, rs.height, 0, genericFormat, typeFormat, NULL );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (rs.flags & kSurfaceCreateShadowmap)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		}
	}
}


RenderSurfaceHandle CreateRenderColorSurfaceGL (TextureID textureID, int width, int height, int samples, TextureDimension dim, UInt32 createFlags, RenderTextureFormat format, GLuint globalSharedFBO)
{
	RenderSurfaceHandle rsHandle;

	Assert(GetMainGraphicsContext().IsValid());

	if( !gGraphicsCaps.hasRenderToTexture )
		return rsHandle;
	if( !gGraphicsCaps.supportsRenderTextureFormat[format] )
		return rsHandle;

	RenderColorSurfaceGL* rs = new RenderColorSurfaceGL();
	RenderSurfaceBase_InitColor(*rs);
	rs->width = width;
	rs->height = height;
	rs->samples = samples;
	rs->format = format;
	rs->textureID = textureID;
	rs->dim = dim;
	rs->flags = createFlags;
	rs->m_ColorBuffer = 0;

	if(textureID.m_ID)
		TextureIdMapGL_QueryOrCreate(textureID);

	#if DEBUG_RENDER_TEXTURES
	printf_console ("RT: create color id=%i, %ix%i, flags=%i, fmt=%i\n", textureID.m_ID, width, height, createFlags, format);
	#endif

	CreateFBORenderColorSurfaceGL (*rs);

	rsHandle.object = rs;
	return rsHandle;
}


RenderSurfaceHandle CreateRenderDepthSurfaceGL (TextureID textureID, int width, int height, int samples, UInt32 createFlags, DepthBufferFormat depthFormat, GLuint globalSharedFBO)
{
	RenderSurfaceHandle rsHandle;

	Assert(GetMainGraphicsContext().IsValid());

	if( !gGraphicsCaps.hasRenderToTexture )
		return rsHandle;

	RenderDepthSurfaceGL* rs = new RenderDepthSurfaceGL;
	RenderSurfaceBase_InitDepth(*rs);
	rs->width = width;
	rs->height = height;
	rs->samples = samples;
	rs->depthFormat = depthFormat;
	rs->textureID = textureID;
	rs->flags = createFlags;

	rs->m_DepthBuffer = 0;

	#if DEBUG_RENDER_TEXTURES
	printf_console ("RT: create depth id=%i, %ix%i, d=%i flags=%i\n", textureID.m_ID, width, height, depthFormat, createFlags);
	#endif

	if(textureID.m_ID)
		TextureIdMapGL_QueryOrCreate(textureID);

	CreateFBORenderDepthSurfaceGL (*rs);

	rsHandle.object = rs;
	return rsHandle;
}


static void InternalDestroyRenderSurfaceGL (RenderSurfaceBase* rs)
{
	Assert(rs);

	#if DEBUG_RENDER_TEXTURES
	printf_console( "RT: destroy id=%i, %ix%i\n", rs->textureID.m_ID, rs->width, rs->height );
	#endif

	if (rs->textureID.m_ID)
		GetRealGfxDevice().DeleteTexture( rs->textureID );
	GLAssert ();

	RenderSurfaceHandle defaultColor = GetRealGfxDevice().GetBackBufferColorSurface();
	RenderSurfaceHandle defaultDepth = GetRealGfxDevice().GetBackBufferDepthSurface();


	if (s_ActiveDepthTarget == rs)
	{
		ErrorString( "RenderTexture warning: Destroying active render texture. Switching to main context." );
		SetRenderTargetGL (1, &defaultColor, defaultDepth, 0, kCubeFaceUnknown, gDefaultFBOGL);
	}
	for (int i = 0; i < s_ActiveColorTargetCount; ++i)
	{
		if (s_ActiveColorTargets[i] == rs)
		{
			ErrorString( "RenderTexture warning: Destroying active render texture. Switching to main context." );
			SetRenderTargetGL (1, &defaultColor, defaultDepth, 0, kCubeFaceUnknown, gDefaultFBOGL);
		}
	}
	if (rs->colorSurface)
	{
		RenderColorSurfaceGL* rsc = static_cast<RenderColorSurfaceGL*>(rs);
		if( rsc->m_ColorBuffer )
			glDeleteRenderbuffersEXT( 1, &rsc->m_ColorBuffer );
	}
	else
	{
		RenderDepthSurfaceGL* rsd = static_cast<RenderDepthSurfaceGL*>(rs);
		if( rsd->m_DepthBuffer )
			glDeleteRenderbuffersEXT( 1, &rsd->m_DepthBuffer );
	}
}

static void AssertFBOIsCurrentFBO (GLuint currentFBO)
{
#if !UNITY_RELEASE
	// verify that globalSharedFBO is currently bound,
	// don't query OpenGL in release mode though
	GLint realCurrentFBO;
	glGetIntegerv (GL_FRAMEBUFFER_BINDING_EXT, &realCurrentFBO);
	DebugAssert (realCurrentFBO == currentFBO);
#endif
}

void ResolveColorSurfaceGL (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle, GLuint globalSharedFBO, GLuint helperFBO)
{
	Assert (srcHandle.IsValid());
	Assert (dstHandle.IsValid());
	RenderColorSurfaceGL* src = reinterpret_cast<RenderColorSurfaceGL*>(srcHandle.object);
	RenderColorSurfaceGL* dst = reinterpret_cast<RenderColorSurfaceGL*>(dstHandle.object);
	if (!src->colorSurface || !dst->colorSurface)
	{
		WarningString("RenderTexture: Resolving non-color surfaces.");
		return;
	}

	GLuint targetTex = (GLuint)TextureIdMapGL_QueryOrCreate(dst->textureID);
	if (!src->m_ColorBuffer || !targetTex)
	{
		WarningString("RenderTexture: Resolving NULL buffers.");
		return;
	}
	
	DebugAssert(gGraphicsCaps.gl.hasFrameBufferBlit);
	AssertFBOIsCurrentFBO(globalSharedFBO);
	GLuint currentFBO = globalSharedFBO;

	// attach source render buffer
	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, currentFBO );
	glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, src->m_ColorBuffer );

	// attach destination texture
	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, helperFBO );
	glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, targetTex, 0 );

	// perform blit
	glBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, currentFBO );
	glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, helperFBO );
	
	// set read & draw buffers to be from FBO color attachemnts
	glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);
	glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);

	glBlitFramebufferEXT( 0, 0, src->width, src->height, 0, 0, dst->width, dst->height, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	GLAssert();
	
	// clean up helperFBO
	glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0 );

	// restore the previously bound FBO
	glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, currentFBO);
}

void DestroyRenderSurfaceGL (RenderSurfaceHandle& rsHandle)
{
	if(rsHandle.object->backBuffer)
		return;

	RenderSurfaceBase* rs = rsHandle.object;
	InternalDestroyRenderSurfaceGL (rs);
	delete rs;
	rsHandle.object = NULL;
}

void ResolveDepthIntoTextureGL (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle, GLuint globalSharedFBO, GLuint helperFBO)
{
	RenderDepthSurfaceGL* rdepth = reinterpret_cast<RenderDepthSurfaceGL*>(depthHandle.object);

	// use the full size of the depth buffer, sub-rects are not needed and might not work on some hardware
	GLint x = 0;
	GLint y = 0;
	GLint width = rdepth->width;
	GLint height = rdepth->height;

	if (gGraphicsCaps.gl.hasFrameBufferBlit)
	{
		AssertFBOIsCurrentFBO (globalSharedFBO);
		GLuint currentFBO = globalSharedFBO;

		// bind helper FBO
		glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, helperFBO);

		// attach the color buffer if needed (see gGraphicsCaps.gl.forceColorBufferWithDepthFBO)
		RenderColorSurfaceGL* rcolor = reinterpret_cast<RenderColorSurfaceGL*>(colorHandle.object);
		const bool attachColorBuffer = (!rcolor->textureID.m_ID && rcolor->m_ColorBuffer);

		if (attachColorBuffer)
			glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, rcolor->m_ColorBuffer);

		// attach the depth buffer
		GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(rdepth->textureID);
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, targetTex, 0);
		glDrawBuffer (GL_NONE);
		glReadBuffer (GL_NONE);

		// check the FBO
		GLenum framebufferStatus = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
		AssertMsg (framebufferStatus == GL_FRAMEBUFFER_COMPLETE_EXT, GetFBOStatusError (framebufferStatus));

		// blit
		glBindFramebufferEXT (GL_READ_FRAMEBUFFER_EXT, currentFBO);
		glBindFramebufferEXT (GL_DRAW_FRAMEBUFFER_EXT, helperFBO);
		glBlitFramebufferEXT (x, y, x + width, y + height, x, y, x + width, y + height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

		// clean up helperFBO
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0);
		if (attachColorBuffer)
			glFramebufferRenderbufferEXT (GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, 0);

		// restore the previously bound FBO
		glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, currentFBO);
	}
	else
	{
		GetRealGfxDevice ().SetTexture (kShaderFragment, 0, 0, rdepth->textureID, kTexDim2D, std::numeric_limits<float>::infinity ());
		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, x, y, x, y, width, height);
	}
	GLAssert();
}

void GrabIntoRenderTextureGL (RenderSurfaceHandle rsHandle, int x, int y, int width, int height, GLuint helperFBO)
{
	if( !rsHandle.IsValid() )
		return;

	RenderColorSurfaceGL* rs = reinterpret_cast<RenderColorSurfaceGL*>( rsHandle.object );

	// If we are not using the back buffer, we cannot copy using glTexSubImage2D
	// when we have a proxy FBO with FSAA. This also affects Safari with
	// the CoreAnimation drawing model.
	// So, instead we blit from the proxy FBO to a helper FBO with our output texture attached.
	// (Related issue in GLReadPixelsWrapper())
	GLint curFBO;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &curFBO);
	if (gDefaultFBOGL != 0 && gDefaultFBOGL == curFBO && gGraphicsCaps.gl.hasFrameBufferBlit)
	{
		GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(rs->textureID);

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, helperFBO);
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, targetTex, 0 );

		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, gDefaultFBOGL);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, helperFBO);

		// Ensure that color0 is always set here,
		// because we attach to color0 when setting up the FBO for scaled fullscreen mode
		glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);
		glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);

		glBlitFramebufferEXT(x, y, x+width, y+height, x, y, x+width, y+height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// clean up helperFBO
		glFramebufferTexture2DEXT (GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0 );

		glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, curFBO );
	}
	else
	{
		GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, rs->textureID, kTexDim2D, std::numeric_limits<float>::infinity());
		glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, x, y, width, height);
	}
	GLAssert ();
}


RenderSurfaceBase* CreateBackBufferColorSurfaceGL()
{
	RenderColorSurfaceGL* rs = new RenderColorSurfaceGL();
	::memset(rs, 0x00, sizeof(RenderColorSurfaceGL));
	RenderSurfaceBase_InitColor(*rs);
	rs->backBuffer = true;

	return rs;
}
RenderSurfaceBase* CreateBackBufferDepthSurfaceGL()
{
	RenderDepthSurfaceGL* rs = new RenderDepthSurfaceGL();
	::memset(rs, 0x00, sizeof(RenderDepthSurfaceGL));
	RenderSurfaceBase_InitDepth(*rs);
	rs->backBuffer = true;

	return rs;
}

