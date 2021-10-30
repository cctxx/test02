#include "UnityPrefix.h"
#include "RenderTextureGLES20.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Graphics/RenderSurface.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "IncludesGLES20.h"
#include "AssertGLES20.h"
#include "DebugGLES20.h"
#include "TextureIdMapGLES20.h"
#include "UnityGLES20Ext.h"

// seems like code is the best place to leave TODO:
// GetCurrentFBImpl - it is called in random places - should just put somehwere (begin frame?)
// discard logic - both color/depth have flags, but only color is used, also - mrt case



#if UNITY_ANDROID
	#include "PlatformDependent/AndroidPlayer/EntryPoint.h"
#endif

#if 1
	#define DBG_LOG_RT_GLES20(...) {}
#else
	#define DBG_LOG_RT_GLES20(...) {printf_console(__VA_ARGS__);printf_console("\n");}
#endif


#if GFX_SUPPORTS_OPENGLES20

#define GL_RT_COMMON_GLES2 1
#include "Runtime/GfxDevice/GLRTCommon.h"
#undef GL_RT_COMMON_GLES2

#if UNITY_IPHONE
	extern "C" bool  UnityDefaultFBOHasMSAA();
	extern "C" void* UnityDefaultFBOColorBuffer();
#endif


struct RenderColorSurfaceGLES2 : public RenderSurfaceBase
{
	GLuint				m_ColorBuffer;
	RenderTextureFormat	format;
	TextureDimension	dim;
};

struct RenderDepthSurfaceGLES2 : public RenderSurfaceBase
{
	GLuint				m_DepthBuffer;
	DepthBufferFormat	depthFormat;
	bool 				depthWithStencil;
};

extern GLint gDefaultFBO;
static bool  gDefaultFboInited = false;

static const unsigned long kOpenGLESTextureDimensionTable[kTexDimCount] = {0, 0, GL_TEXTURE_2D, 0, GL_TEXTURE_CUBE_MAP, 0};

static RenderColorSurfaceGLES2*	s_ActiveColorTarget[kMaxSupportedRenderTargets] = {0};
static int						s_ActiveColorTargetCount						= 0;
static int						s_ActiveMip										= 0;
static CubemapFace				s_ActiveFace									= kCubeFaceUnknown;
static RenderDepthSurfaceGLES2*	s_ActiveDepthTarget								= 0;

static RenderColorSurfaceGLES2	s_BackBufferColor;
static RenderDepthSurfaceGLES2	s_BackBufferDepth;


// at least on ios when changing ext of attachments there is huge perf penalty
// so do fbo per rt-ext

typedef std::pair<unsigned, unsigned>	FBKey;
typedef std::map<FBKey, GLuint>			FBMap;

static FBMap _FBMap;
static GLuint GetFBFromAttachments(RenderColorSurfaceGLES2* color, RenderDepthSurfaceGLES2* depth)
{
	// while it may seems bad that we freely mix rb/texture - we do the distinction per-gpu/per-platform mostly
	// so it is actually ok
	unsigned ckey = color->textureID.m_ID ? color->textureID.m_ID : color->m_ColorBuffer;
	unsigned dkey = depth->textureID.m_ID ? depth->textureID.m_ID : depth->m_DepthBuffer;
	FBKey key = std::make_pair(ckey, dkey);


	FBMap::iterator fbi = _FBMap.find(key);
	if(fbi == _FBMap.end())
	{
		GLuint fb=0;
		GLES_CHK(glGenFramebuffers(1, &fb));

		fbi = _FBMap.insert(std::make_pair(key, fb)).first;
	}

	Assert(fbi != _FBMap.end());
	return fbi->second;
}

// this should be called on context loss
// TODO: need we care about the case of actually deleting FBOs
void ClearFBMapping()
{
	_FBMap.clear();
}

// this is not the best way, but unless we have proper rt->fbo mapping let's live with that
// the reason is to avoid leaks when we destroy rb/tex that is still attached to some FBO
static void OnFBOAttachmentDelete(RenderSurfaceBase* rs)
{
	unsigned key = rs->textureID.m_ID;
	if(key == 0)
		key = rs->colorSurface ? ((RenderColorSurfaceGLES2*)rs)->m_ColorBuffer : ((RenderDepthSurfaceGLES2*)rs)->m_DepthBuffer;

	int curFB = GetCurrentFBGLES2();

	for(FBMap::iterator i = _FBMap.begin() ; i != _FBMap.end() ; )
	{
		if(i->first.first == key || i->first.second == key)
		{
			// make sure we also drop attachments before killing FBO (some drivers may leak otherwise)
			GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, i->second));
			GLES_CHK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0));
			GLES_CHK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0));
			GLES_CHK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0));
			GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, curFB));

			GLES_CHK(glDeleteFramebuffers(1, &i->second));
			FBMap::iterator rem = i;
			++i;
			_FBMap.erase(rem);
		}
		else
		{
			++i;
		}
	}
}

static const char* GetFBOStatusError( GLenum status )
{
	Assert( status != GL_FRAMEBUFFER_COMPLETE ); // should not be called when everything is ok...
	switch( status )
	{
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS: return "INCOMPLETE_DIMENSIONS";
		case GL_FRAMEBUFFER_UNSUPPORTED: return "UNSUPPORTED";
		default: return "unknown error";
	}
}
static const char* GetFBOAttachementType(GLint type)
{
	switch (type)
	{
	case GL_RENDERBUFFER: return "GL_RENDERBUFFER";
	case GL_TEXTURE: return "GL_TEXTURE";
	default: return "GL_NONE";
	}
}

static int GetCurrentFBImpl()
{
	GLint curFB;
	GLES_CHK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &curFB));

	return (int)curFB;
}

int GetCurrentFBGLES2()
{
	return GetCurrentFBImpl();
}

void EnsureDefaultFBInitedGLES2()
{
	if(!gDefaultFboInited)
	{
		gDefaultFBO = GetCurrentFBImpl();
		// this will be called from correct thread, so no need to sync or whatever
		extern void GfxDeviceGLES20_InitFramebufferDepthFormat();
		GfxDeviceGLES20_InitFramebufferDepthFormat();

		gDefaultFboInited = true;
	}
}

void DiscardCurrentFBImpl(bool discardColor, bool discardDepth, GLenum target=GL_FRAMEBUFFER)
{
	if(gGraphicsCaps.gles20.hasDiscardFramebuffer)
	{
		// TODO: mrt case?
		GLenum  discardUserAttach[]		= {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
		GLenum  discardSystemAttach[]	= {GL_COLOR_EXT, GL_DEPTH_EXT, GL_STENCIL_EXT};

		const GLenum* discardTarget = discardUserAttach;
		if(s_ActiveColorTarget[0]->backBuffer && GetCurrentFBGLES2() == 0)
			discardTarget = discardSystemAttach;


		if(!discardColor)	discardTarget += 1;

		unsigned discardCount = 0;
		if(discardColor)	discardCount += 1;
		if(discardDepth)	discardCount += 2;

		if(discardCount)
			GLES_CHK(gGlesExtFunc.glDiscardFramebufferEXT(GL_FRAMEBUFFER, discardCount, discardTarget));
	}
}

void ClearCurrentFBImpl(bool clearColor, bool clearDepth)
{
	if(gGraphicsCaps.hasTiledGPU)
	{
		UInt32 clearFlags   = (clearColor ? kGfxClearColor : 0) | (clearDepth ? kGfxClearDepthStencil : 0);
		float  clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};

		GetRealGfxDevice().Clear(clearFlags, clearColor, 1.0f, 0);
	}
}

enum
{
	discardDiscardFB = 0,
	discardClearFB = 1
};

static void DiscardContentsImpl(int discardPhase)
{
	// TODO: mrt?
	if(s_ActiveColorTarget[0] || s_ActiveDepthTarget)
	{
		bool* colorVar = discardPhase == discardDiscardFB ? &s_ActiveColorTarget[0]->shouldDiscard : &s_ActiveColorTarget[0]->shouldClear;
		bool* depthVar = discardPhase == discardDiscardFB ? &s_ActiveDepthTarget->shouldDiscard : &s_ActiveDepthTarget->shouldClear;

		bool color = *colorVar, depth = *depthVar;

		*colorVar = *depthVar = false;

		if(color || depth)
		{
			if(discardPhase == discardDiscardFB)	DiscardCurrentFBImpl(color, depth);
			else									ClearCurrentFBImpl(color, depth);
		}
	}
}


void DiscardContentsGLES2(RenderSurfaceHandle rs)
{
	// TODO: handle bb
	if(rs.IsValid())
	{
		RenderColorSurfaceGLES2* rsgles = reinterpret_cast<RenderColorSurfaceGLES2*>(rs.object);
		// discard only makes sense for current active rt
		rsgles->shouldDiscard = gGraphicsCaps.gles20.hasDiscardFramebuffer && rsgles == s_ActiveColorTarget[0];
		rsgles->shouldClear   = gGraphicsCaps.hasTiledGPU;
	}
}

bool SetRenderTargetGLES2 (int count, RenderSurfaceHandle* colorHandle, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, GLuint globalSharedFBO)
{
	RenderColorSurfaceGLES2* rcolor0 = reinterpret_cast<RenderColorSurfaceGLES2*>( colorHandle[0].object );
	RenderDepthSurfaceGLES2* rdepth  = reinterpret_cast<RenderDepthSurfaceGLES2*>( depthHandle.object );

	// Exit if nothing to do
	if( s_ActiveColorTargetCount == count && s_ActiveDepthTarget == rdepth && s_ActiveFace == face && s_ActiveMip == mipLevel )
	{
		bool colorSame = true;
		for(int i = 0 ; i < count && colorSame ; ++i)
		{
			if(s_ActiveColorTarget[i] != reinterpret_cast<RenderColorSurfaceGLES2*>(colorHandle[i].object))
				colorSame = false;
		}
		if (colorSame)
			return false;
	}

	EnsureDefaultFBInitedGLES2();
	DiscardContentsImpl(discardDiscardFB);

	AssertIf (!gGraphicsCaps.hasRenderToTexture);
	GetRealGfxDevice().GetFrameStats().AddRenderTextureChange(); // stats


	Assert(colorHandle[0].IsValid() && depthHandle.IsValid());
	Assert(colorHandle[0].object->backBuffer == depthHandle.object->backBuffer);


	GLenum colorAttachStart = GL_COLOR_ATTACHMENT0;
	if(gGraphicsCaps.gles20.hasNVMRT)
		colorAttachStart = GL_COLOR_ATTACHMENT0_NV;

	Assert(gGraphicsCaps.gles20.hasNVMRT || count==1);

	if(!rcolor0->backBuffer)
	{
		GLuint fb = GetFBFromAttachments(rcolor0, rdepth);
		GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, fb));

		GLenum drawBuffers[kMaxSupportedRenderTargets] = {0};
		for(int i = 0 ; i < count ; ++i)
		{
			// when we start to support EXT variant - add check here
			GLenum colorAttach = colorAttachStart + i;
			RenderColorSurfaceGLES2* rcolor = reinterpret_cast<RenderColorSurfaceGLES2*>(colorHandle[i].object);
			Assert(rcolor->colorSurface);

			GLuint targetColorTex = (GLuint)TextureIdMap::QueryNativeTexture(rcolor->textureID);

			drawBuffers[i] = colorAttach;
			if(!IsDepthRTFormat(rcolor->format) && targetColorTex)
			{
				if (rcolor->dim == kTexDimCUBE)
				{
					GLES_CHK(glFramebufferTexture2D(GL_FRAMEBUFFER, colorAttach, GL_TEXTURE_CUBE_MAP_POSITIVE_X + clamp<int>(face,0,5), targetColorTex, mipLevel));
				}
				else
				{
					if(rcolor->samples > 1 && gGraphicsCaps.gles20.hasImgMSAA)
						GLES_CHK(gGlesExtFunc.glFramebufferTexture2DMultisampleIMG(GL_FRAMEBUFFER, colorAttach, GL_TEXTURE_2D, targetColorTex, mipLevel, rcolor->samples));
					else
						GLES_CHK(glFramebufferTexture2D(GL_FRAMEBUFFER, colorAttach, GL_TEXTURE_2D, targetColorTex, mipLevel));
				}
			}
			else
			{
				GLES_CHK(glFramebufferRenderbuffer (GL_FRAMEBUFFER, colorAttach, GL_RENDERBUFFER, rcolor->m_ColorBuffer));
				if(rcolor->m_ColorBuffer == 0)
					drawBuffers[i] = GL_NONE;
			}
		}

		if(gGraphicsCaps.gles20.hasNVMRT)
			gGlesExtFunc.glDrawBuffersNV(count, drawBuffers);

		GLuint targetDepthTex = (GLuint)TextureIdMap::QueryNativeTexture(rdepth->textureID);
		bool needAttachStencil = gGraphicsCaps.hasStencil && rdepth->depthWithStencil;

		// depth surface
		AssertIf (rdepth->colorSurface);
		if (targetDepthTex
#if UNITY_PEPPER
// Workaround for http://code.google.com/p/angleproject/issues/detail?id=211
			 || rdepth->m_DepthBuffer == 0
#endif
		)
		{
			DBG_LOG_RT_GLES20("glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, %d, 0);", rdepth->textureID.m_ID);
			GLES_CHK(glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, targetDepthTex, 0));
			// A driver might not support attaching depth & stencil as textures here.
			if (!gGraphicsCaps.hasRenderTargetStencil)
				needAttachStencil = false;
			if (needAttachStencil)
				GLES_CHK(glFramebufferTexture2D (GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, targetDepthTex, 0));
			else
				GLES_CHK(glFramebufferTexture2D (GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0));
		}
		else
		{
			DBG_LOG_RT_GLES20("glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, %d, 0);", rdepth->m_DepthBuffer);
			GLES_CHK(glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rdepth->m_DepthBuffer));
			if(needAttachStencil)
				GLES_CHK(glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rdepth->m_DepthBuffer));
			else
				GLES_CHK(glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0));
		}

		GLenum status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			int colorParam, depthParam, stencilParam;
			int colorValue, depthValue, stencilValue;
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &colorParam);
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &depthParam);
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &stencilParam);

			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &colorValue);
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depthValue);
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &stencilValue);

			AssertString (Format(
				"FBO fail: %s\nDetailed description:\n"
				"GL_COLOR_ATTACHMENT0 Type:%s Value:%d\n"
				"GL_DEPTH_ATTACHMENT Type:%s Value:%d\n"
				"GL_STENCIL_ATTACHEMENT Type:%s Value:%d\n",
				GetFBOStatusError(status),
				GetFBOAttachementType(colorParam), colorValue,
				GetFBOAttachementType(depthParam), depthValue,
				GetFBOAttachementType(stencilParam), stencilValue));
		}
		Assert(status == GL_FRAMEBUFFER_COMPLETE);
	}
	else
	{
		Assert(rcolor0->backBuffer && rdepth->backBuffer); // OpenGL can't mix FBO and native window at once
		GLES_CHK(glBindFramebuffer( GL_FRAMEBUFFER, gDefaultFBO ));
	}

	// If we previously had a mip-mapped render texture, generate mip levels for it now.
	if (s_ActiveColorTarget[0] &&
		(s_ActiveColorTarget[0]->flags & kSurfaceCreateMipmap) &&
		(s_ActiveColorTarget[0]->flags & kSurfaceCreateAutoGenMips))
	{
		const int textureTarget = kOpenGLESTextureDimensionTable[s_ActiveColorTarget[0]->dim];
		GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, s_ActiveColorTarget[0]->textureID, s_ActiveColorTarget[0]->dim, std::numeric_limits<float>::infinity());
		GLES_CHK(glGenerateMipmap(textureTarget));
	}

	s_ActiveColorTargetCount = count;
	for(int i = 0 ; i < count ; ++i)
		s_ActiveColorTarget[i] = reinterpret_cast<RenderColorSurfaceGLES2*>(colorHandle[i].object);

	s_ActiveDepthTarget = rdepth;
	s_ActiveFace = face;
	s_ActiveMip = mipLevel;

	DiscardContentsImpl(discardClearFB);

	return true;
}

// TODO: take index into account here too
RenderSurfaceHandle GetActiveRenderColorSurfaceGLES2()
{
	return RenderSurfaceHandle(s_ActiveColorTarget[0]);
}
RenderSurfaceHandle GetActiveRenderDepthSurfaceGLES2()
{
	return RenderSurfaceHandle(s_ActiveDepthTarget);
}

bool IsActiveRenderTargetWithColorGLES2()
{
	return !s_ActiveColorTarget[0] || s_ActiveColorTarget[0]->backBuffer || !IsDepthRTFormat (s_ActiveColorTarget[0]->format);
}


static void CreateFBORenderColorSurfaceGLES (RenderColorSurfaceGLES2& rs)
{
	int textureTarget = kOpenGLESTextureDimensionTable[rs.dim];

	if( !IsDepthRTFormat(rs.format) )
	{
		GLenum internalFormat	= (rs.flags & kSurfaceCreateSRGB) ? RTColorInternalFormatSRGBGLES2(rs.format) : RTColorInternalFormatGLES2(rs.format);
		GLenum textureFormat	= (rs.flags & kSurfaceCreateSRGB) ? RTColorTextureFormatSRGBGLES2(rs.format) : RTColorTextureFormatGLES2(rs.format);
		GLenum textureType		= RTColorTextureTypeGLES2(rs.format);

		// Create texture to render for color
		GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, rs.textureID, rs.dim, std::numeric_limits<float>::infinity());

		if (rs.dim == kTexDimCUBE)
		{
			// cubemap: initialize all faces
			for( int f = 0; f < 6; ++f )
			{
				glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, internalFormat, rs.width, rs.height, 0, textureFormat, textureType, NULL );
			}
			DBG_LOG_RT_GLES20("Creating FBO Color Surface (GL_TEXTURE_CUBE_MAP) [%d] - Width(%d) x Height(%d)", rs.textureID.m_ID, rs.width, rs.height);
			if (rs.flags & kSurfaceCreateMipmap) { // establish mip map chain if needed
				GLES_CHK(glGenerateMipmap( textureTarget ));
		}
		}
		// not the best way around, but oh well
		else if(!rs.textureID.m_ID)
		{
			internalFormat = RBColorInternalFormatGLES2(rs.format);
			GLES_CHK(glGenRenderbuffers(1, &rs.m_ColorBuffer));
			GLES_CHK(glBindRenderbuffer(GL_RENDERBUFFER, rs.m_ColorBuffer));
			if(rs.samples > 1 && gGraphicsCaps.gles20.hasAppleMSAA)
				GLES_CHK(gGlesExtFunc.glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, rs.samples, internalFormat, rs.width, rs.height));
			else
				GLES_CHK(glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, rs.width, rs.height));
		}
		else
		{
			// regular texture: initialize
			GLES_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			GLES_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			GLES_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			GLES_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
			GLES_CHK(glTexImage2D( textureTarget, 0, internalFormat, rs.width, rs.height, 0, textureFormat, textureType, NULL ));

			DBG_LOG_RT_GLES20("Creating FBO Color Surface (GL_TEXTURE_2D) [%d] - Width(%d) x Height(%d)", rs.textureID.m_ID, rs.width, rs.height);
			if (rs.flags & kSurfaceCreateMipmap) { // establish mip map chain if needed
				GLES_CHK(glGenerateMipmap( textureTarget ));
			}
		}
	}
	else
	{
		// Note : For GLES2.0Emu and NaCl we must create RenderBuffer even if no color
	#if UNITY_WIN || UNITY_NACL
		glGenRenderbuffers( 1, &rs.m_ColorBuffer );
		glBindRenderbuffer( GL_RENDERBUFFER, rs.m_ColorBuffer );
		glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA4, rs.width, rs.height );
		DBG_LOG_RT_GLES20("Creating dummy color buffer (GL_RENDERBUFFER) [%d] - Width(%d) x Height(%d)", rs.m_ColorBuffer, rs.width, rs.height);
	#endif
	}
}


static void CreateFBORenderDepthSurfaceGLES (RenderDepthSurfaceGLES2& rs)
{
	int textureTarget = kOpenGLESTextureDimensionTable[kTexDim2D];

	GLenum depthFormat;
	GLenum genericFormat;
	GLenum typeFormat;

	// TODO 24bit depth should be revisited
	if (rs.depthFormat == kDepthFormat24 && gGraphicsCaps.hasStencil)
	{
		rs.depthWithStencil = true;
		depthFormat = GL_DEPTH24_STENCIL8_OES;
		genericFormat = GL_DEPTH_STENCIL_OES;
		typeFormat = GL_UNSIGNED_INT_24_8_OES;
	}
	else if (rs.depthFormat == kDepthFormat24 && gGraphicsCaps.gles20.has24DepthForFBO)
	{
		depthFormat = GL_DEPTH_COMPONENT24_OES;
		genericFormat = GL_DEPTH_COMPONENT;
		typeFormat = GL_UNSIGNED_BYTE;
	}
	else if (gGraphicsCaps.gles20.hasNLZ)
	{
		depthFormat = 0x8E2C;	// GL_DEPTH_COMPONENT16_NONLINEAR_NV
		genericFormat = GL_DEPTH_COMPONENT;
		typeFormat = GL_UNSIGNED_SHORT;
	}
	else
	{
		depthFormat = GL_DEPTH_COMPONENT16;
		genericFormat = GL_DEPTH_COMPONENT;
		typeFormat = GL_UNSIGNED_SHORT;
	}

	GLuint targetDepthTex = (GLuint)TextureIdMap::QueryNativeTexture(rs.textureID);
	if (!targetDepthTex)
	{
		// Note : For GLES2.0Emu we must create RenderBuffer even if depth format is none
#if !UNITY_WIN
		if( rs.depthFormat != kDepthFormatNone )
#endif
		{
			GLES_CHK(glGenRenderbuffers( 1, &rs.m_DepthBuffer ));
			GLES_CHK(glBindRenderbuffer( GL_RENDERBUFFER, rs.m_DepthBuffer ));

			if(rs.samples > 1)
			{
				if(gGraphicsCaps.gles20.hasAppleMSAA)
					GLES_CHK(gGlesExtFunc.glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, rs.samples, depthFormat, rs.width, rs.height));
				else if(gGraphicsCaps.gles20.hasImgMSAA)
					GLES_CHK(gGlesExtFunc.glRenderbufferStorageMultisampleIMG(GL_RENDERBUFFER, rs.samples, depthFormat, rs.width, rs.height));
			}
			else
			{
				GLES_CHK(glRenderbufferStorage( GL_RENDERBUFFER, depthFormat, rs.width, rs.height ));
			}

			DBG_LOG_RT_GLES20("Creating FBO Depth Surface (GL_RENDERBUFFER) [%d] - Width(%d) x Height(%d)", rs.m_DepthBuffer, rs.width, rs.height);
		}
	}
	else
	{
		// create depth texture
		GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, rs.textureID, kTexDim2D, std::numeric_limits<float>::infinity());
		GLES_CHK(glTexImage2D( textureTarget, 0, genericFormat, rs.width, rs.height, 0, genericFormat, typeFormat, NULL ));

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		if (rs.flags & kSurfaceCreateShadowmap)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_EXT, GL_COMPARE_REF_TO_TEXTURE_EXT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_EXT, GL_LEQUAL);
		}

		DBG_LOG_RT_GLES20("Creating FBO Depth Surface (GL_TEXTURE_2D) [%d] - Width(%d) x Height(%d)", rs.textureID.m_ID, rs.width, rs.height);
	}
}

static RenderColorSurfaceGLES2* CreateRenderColorSurfaceGLES2Impl(void* rs, TextureID textureID, unsigned rbID, int width, int height, TextureDimension dim, UInt32 createFlags, RenderTextureFormat format, int samples)
{
	GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(textureID);
	Assert(rbID == 0 || targetTex == 0);

	if( !gGraphicsCaps.hasRenderToTexture || !gGraphicsCaps.supportsRenderTextureFormat[format] )
		return 0;

	RenderColorSurfaceGLES2* ret = rs ? (RenderColorSurfaceGLES2*)rs : new RenderColorSurfaceGLES2;
	RenderSurfaceBase_InitColor(*ret);
	ret->width = width;
	ret->height = height;
	ret->format = format;
	ret->dim = dim;
	ret->flags = createFlags;
	ret->samples = samples > gGraphicsCaps.gles20.maxSamples ? gGraphicsCaps.gles20.maxSamples : samples;

	ret->textureID = textureID;
	ret->m_ColorBuffer = rbID;

	if(textureID.m_ID && !rbID)
		TextureIdMapGLES20_QueryOrCreate(textureID);

	return ret;
}

RenderSurfaceHandle CreateRenderColorSurfaceGLES2 (TextureID textureID, unsigned rbID, int width, int height, TextureDimension dim, UInt32 createFlags, RenderTextureFormat format, int samples)
{
	RenderColorSurfaceGLES2* rs = CreateRenderColorSurfaceGLES2Impl(0, textureID, rbID, width, height, dim, createFlags, format, samples);
	if(rs)
		CreateFBORenderColorSurfaceGLES(*rs);

	return RenderSurfaceHandle(rs);
}

static RenderDepthSurfaceGLES2* CreateRenderDepthSurfaceGLES2Impl(void* rs, TextureID textureID, unsigned rbID, int width, int height, UInt32 createFlags, DepthBufferFormat depthFormat, int samples)
{
	GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(textureID);
	Assert(rbID == 0 || targetTex == 0);

	if( !gGraphicsCaps.hasRenderToTexture )
		return 0;

	RenderDepthSurfaceGLES2* ret = rs ? (RenderDepthSurfaceGLES2*)rs : new RenderDepthSurfaceGLES2;
	RenderSurfaceBase_InitDepth(*ret);
	ret->depthWithStencil = false;
	ret->width = width;
	ret->height = height;
	ret->depthFormat = depthFormat;
	ret->flags = createFlags;
	ret->samples = samples > gGraphicsCaps.gles20.maxSamples ? gGraphicsCaps.gles20.maxSamples : samples;

	ret->textureID		= textureID;
	ret->m_DepthBuffer	= rbID;

	if(textureID.m_ID && !rbID)
		TextureIdMapGLES20_QueryOrCreate(textureID);

	return ret;
}


RenderSurfaceHandle CreateRenderDepthSurfaceGLES2 (TextureID textureID, unsigned rbID, int width, int height, UInt32 createFlags, DepthBufferFormat depthFormat, int samples)
{
	RenderDepthSurfaceGLES2* rs = CreateRenderDepthSurfaceGLES2Impl(0, textureID, rbID, width, height, createFlags, depthFormat, samples);
	if(rs)
		CreateFBORenderDepthSurfaceGLES(*rs);

	return RenderSurfaceHandle(rs);
}


static void InternalDestroyRenderSurfaceGLES (RenderSurfaceBase* rs)
{
	AssertIf( !rs );

	if (rs->textureID.m_ID)
	{
		GetRealGfxDevice().DeleteTexture( rs->textureID );
		DBG_LOG_RT_GLES20("Destroying GL_TEXTURE_2D/GL_CUBE_MAP %d", rs->textureID.m_ID);
	}
	GLESAssert ();


	RenderSurfaceHandle defaultColor = GetRealGfxDevice().GetBackBufferColorSurface();
	RenderSurfaceHandle defaultDepth = GetRealGfxDevice().GetBackBufferDepthSurface();


	for (int i = 0 ; i < s_ActiveColorTargetCount ; ++i)
	{
		if (s_ActiveColorTarget[i] == rs)
		{
			ErrorString( "RenderTexture warning: Destroying active render texture. Switching to main context." );
			SetRenderTargetGLES2 (1, &defaultColor, defaultDepth, 0, kCubeFaceUnknown, gDefaultFBO);
		}
	}
	if (s_ActiveDepthTarget == rs)
	{
		ErrorString( "RenderTexture warning: Destroying active render texture. Switching to main context." );
		SetRenderTargetGLES2 (1, &defaultColor, defaultDepth, 0, kCubeFaceUnknown, gDefaultFBO);
	}

	OnFBOAttachmentDelete(rs);

	if (rs->colorSurface)
	{
		RenderColorSurfaceGLES2* rsc = static_cast<RenderColorSurfaceGLES2*>(rs);
		if( rsc->m_ColorBuffer )
		{
			GLES_CHK(glDeleteRenderbuffers( 1, &rsc->m_ColorBuffer ));
			DBG_LOG_RT_GLES20("Destroying GL_RENDER_BUFFER %d", rsc->m_ColorBuffer);
		}
	}
	else
	{
		RenderDepthSurfaceGLES2* rsd = static_cast<RenderDepthSurfaceGLES2*>(rs);
		if( rsd->m_DepthBuffer )
		{
			GLES_CHK(glDeleteRenderbuffers( 1, &rsd->m_DepthBuffer ));
			DBG_LOG_RT_GLES20("Destroying GL_RENDER_BUFFER %d", rsd->m_DepthBuffer);
		}
	}
}


void DestroyRenderSurfaceGLES2 (RenderSurfaceHandle& rsHandle)
{
	if(rsHandle.object->backBuffer)
		return;

	RenderSurfaceBase* rs = rsHandle.object;
	InternalDestroyRenderSurfaceGLES (rs);
	delete rs;
	rsHandle.object = NULL;
}

void ResolveMSAA(GLuint dstTex, GLuint srcRB, GLuint globalSharedFBO, GLuint helperFBO)
{
	if(gGraphicsCaps.gles20.hasAppleMSAA)
	{
		GLint oldFBO;
		GLES_CHK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO));

		GLES_CHK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE, globalSharedFBO));
		GLES_CHK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER_APPLE, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0));
		GLES_CHK(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER_APPLE, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0));
		GLES_CHK(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER_APPLE, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0));
		ClearCurrentFBImpl(true, false);

		GLES_CHK(glBindFramebuffer(GL_READ_FRAMEBUFFER_APPLE, helperFBO));
		GLES_CHK(glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER_APPLE, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, srcRB));
		GLES_CHK(gGlesExtFunc.glResolveMultisampleFramebufferAPPLE());

		DiscardCurrentFBImpl(true, false, GL_READ_FRAMEBUFFER_APPLE);
		GLES_CHK(glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER_APPLE, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0));

		GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, oldFBO));
	}
}

RenderSurfaceBase* ResolveMSAASetupFBO(void* screenRS, int format, GLuint globalSharedFBO, GLuint helperFBO)
{
	RenderSurfaceHandle ret;

	RenderColorSurfaceGLES2* screen = (RenderColorSurfaceGLES2*)screenRS;
	if(gGraphicsCaps.gles20.hasAppleMSAA)
	{
		TextureID texid = GetRealGfxDevice().CreateTextureID();
		ret = CreateRenderColorSurfaceGLES2(texid, 0, screen->width, screen->height, kTexDim2D, 0, (RenderTextureFormat)format, 1);

		GLuint gltex = (GLuint)TextureIdMap::QueryNativeTexture(texid);
		ResolveMSAA(gltex, screen->m_ColorBuffer, globalSharedFBO, helperFBO);

		GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, globalSharedFBO));
		GLES_CHK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gltex, 0));
	}

	return ret.object;
}

void ResolveMSAASetupFBO_Cleanup(RenderSurfaceBase* rs)
{
	RenderSurfaceHandle handle(rs);

	if(handle.IsValid())
		DestroyRenderSurfaceGLES2(handle);
}


void ResolveColorSurfaceGLES2(RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle, GLuint globalSharedFBO, GLuint helperFBO)
{
	// TODO: unify all the places we do resolve

	Assert (srcHandle.IsValid());
	Assert (dstHandle.IsValid());
	RenderColorSurfaceGLES2* src = reinterpret_cast<RenderColorSurfaceGLES2*>(srcHandle.object);
	RenderColorSurfaceGLES2* dst = reinterpret_cast<RenderColorSurfaceGLES2*>(dstHandle.object);
	if (!src->colorSurface || !dst->colorSurface)
	{
		WarningString("RenderTexture: Resolving non-color surfaces.");
		return;
	}

	GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(dst->textureID);
	if (!src->m_ColorBuffer || !targetTex)
	{
		WarningString("RenderTexture: Resolving NULL buffers.");
		return;
	}

	ResolveMSAA(targetTex, src->m_ColorBuffer, globalSharedFBO, helperFBO);
}

void GrabIntoRenderTextureGLES2 (RenderSurfaceHandle rsHandle, RenderSurfaceHandle rdHandle, int x, int y, int width, int height, GLuint globalSharedFBO, GLuint helperFBO)
{
	if(!rsHandle.IsValid() || rsHandle.object->backBuffer)
		return;

	RenderColorSurfaceGLES2* rs = reinterpret_cast<RenderColorSurfaceGLES2*>(rsHandle.object);

#if UNITY_IPHONE
	GLint oldFBO = 0;
	RenderSurfaceBase* resolveRS = 0;

	if(IsActiveMSAARenderTargetGLES2() && gGraphicsCaps.gles20.hasAppleMSAA)
	{
		RenderColorSurfaceGLES2* screen = (RenderColorSurfaceGLES2*)UnityDefaultFBOColorBuffer();

		bool fullScreenResolve = (x==0 && y==0 && width == screen->width && height == screen->height);
		if(fullScreenResolve)
		{
			GLuint texColor = (GLuint)TextureIdMap::QueryNativeTexture(rs->textureID);
			ResolveMSAA(texColor, screen->m_ColorBuffer, globalSharedFBO, helperFBO);

			return;
		}
		else
		{
			// intentional fall-through - we setup resolved FBO
			GLES_CHK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO));
			resolveRS = ResolveMSAASetupFBO(screen, rs->format, globalSharedFBO, helperFBO);
		}
	}
#endif

	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, rs->textureID, kTexDim2D, std::numeric_limits<float>::infinity());
	GLES_CHK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

	bool needsReadPixelsFallback = false;
#if UNITY_WIN
	// GLES 2.0 Emulator seems to have a bug, in that it can only do glCopyTexSubImage2D for power of two textures.
	needsReadPixelsFallback = !IsPowerOfTwo(rs->width) || !IsPowerOfTwo(rs->height);
#endif
#if UNITY_ANDROID
	// on pre-honeycomb devices AND in case of EGL_ANDROID_framebuffer_target
	// we may end up with alphabits=0 explicitely, which, on some devices, cases FB to be considered RGB
	{
		GLint rbits=0, gbits=0, bbits=0, abits=0;
		CHECK(glGetIntegerv(GL_RED_BITS,   &rbits));
		CHECK(glGetIntegerv(GL_GREEN_BITS, &gbits));
		CHECK(glGetIntegerv(GL_BLUE_BITS,  &bbits));
		CHECK(glGetIntegerv(GL_ALPHA_BITS, &abits));

		if(rbits==8 && gbits==8 && bbits==8 && abits==0)
			needsReadPixelsFallback = true;
	}
#endif

	if (needsReadPixelsFallback)
	{
		UInt8* data = NULL;
		switch (rs->format)
		{
		case kRTFormatARGB32:
				data = new UInt8[width*height*4];
				GLES_CHK(glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data));
				GLES_CHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));
			break;

		default:
			ErrorStringMsg ("Unsupported render texture format :%d", rs->format);
				break;
		}
		delete [] data;
	}
	else
	{
		GLES_CHK(glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, x, y, width, height));
	}

#if UNITY_IPHONE
	if(resolveRS)
	{
		ResolveMSAASetupFBO_Cleanup(resolveRS);
		GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, oldFBO));
	}
#endif
}

RenderTextureFormat GetCurrentFBColorFormatGLES20()
{
	if( s_ActiveColorTarget[0] )
		return s_ActiveColorTarget[0]->format;

	return QueryFBColorFormatGLES2();
}

void InitBackBufferGLES2(RenderSurfaceBase** outColor, RenderSurfaceBase** outDepth)
{
	RenderSurfaceBase_InitColor(s_BackBufferColor);
	s_BackBufferColor.backBuffer = true;

	RenderSurfaceBase_InitDepth(s_BackBufferDepth);
	s_BackBufferDepth.backBuffer = true;

	*outColor = s_ActiveColorTarget[0]	= &s_BackBufferColor;
	*outDepth = s_ActiveDepthTarget		= &s_BackBufferDepth;
}

void SetBackBufferGLES2()
{
	s_ActiveColorTarget[0]	= &s_BackBufferColor;
	s_ActiveColorTargetCount= 0;
	s_ActiveMip				= 0;
	s_ActiveFace			= kCubeFaceUnknown;

	s_ActiveDepthTarget		= &s_BackBufferDepth;

	EnsureDefaultFBInitedGLES2();
	GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, gDefaultFBO));
}


#if UNITY_IPHONE
bool IsActiveMSAARenderTargetGLES2()
{
	return s_ActiveColorTarget[0] && !s_ActiveColorTarget[0]->backBuffer ? s_ActiveColorTarget[0]->samples > 1 : UnityDefaultFBOHasMSAA();
}

void* UnityCreateUpdateExternalColorSurfaceGLES2(void* surf, unsigned texid, unsigned rbid, int width, int height, bool is32bit)
{
	TextureID tex = surf ? ((RenderSurfaceBase*)surf)->textureID : GetUncheckedGfxDevice().CreateTextureID();
	TextureIdMap::UpdateTexture(tex, texid);

	// TODO for android we will probably need to properly distinct 24/32
	RenderTextureFormat format = is32bit ? kRTFormatARGB32 : kRTFormatRGB565;
	return CreateRenderColorSurfaceGLES2Impl(surf, tex, rbid, width, height, kTexDim2D, 0, format, 1);
}
void* UnityCreateUpdateExternalDepthSurfaceGLES2(void* surf, unsigned texid, unsigned rbid, int width, int height, bool is24bit)
{
	TextureID tex = surf ? ((RenderSurfaceBase*)surf)->textureID : GetUncheckedGfxDevice().CreateTextureID();
	TextureIdMap::UpdateTexture(tex, texid);

	DepthBufferFormat format = is24bit ? kDepthFormat24 : kDepthFormat16;
	return CreateRenderDepthSurfaceGLES2Impl(surf, tex, rbid, width, height, 0, format, 1);
}

void UnityDestroyExternalColorSurfaceGLES2(void* surf)
{
	delete (RenderColorSurfaceGLES2*)surf;
}
void UnityDestroyExternalDepthSurfaceGLES2(void* surf)
{
	delete (RenderDepthSurfaceGLES2*)surf;
}
#endif


#endif // GFX_SUPPORTS_OPENGLES20
