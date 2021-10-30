#include "UnityPrefix.h"
#include "RenderTextureGLES30.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "IncludesGLES30.h"
#include "AssertGLES30.h"
#include "DebugGLES30.h"
#include "TextureIdMapGLES30.h"
#include "UtilsGLES30.h"

#if UNITY_ANDROID
	#include "PlatformDependent/AndroidPlayer/EntryPoint.h"
#endif

#if 1
	#define DBG_LOG_RT_GLES30(...) {}
#else
	#define DBG_LOG_RT_GLES30(...) {printf_console(__VA_ARGS__);printf_console("\n");}
#endif

#if GFX_SUPPORTS_OPENGLES30

namespace
{

static const UInt32 kCubeFacesES3[] =
{
	GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};

inline bool IsDepthStencilFormat (UInt32 format)
{
	switch (format)
	{
		case GL_DEPTH24_STENCIL8:
		case GL_DEPTH32F_STENCIL8:
			return true;

		default:
			return false;
	}
}

const char* GetFBOStatusName (UInt32 status)
{
	switch (status)
	{
		case GL_FRAMEBUFFER_COMPLETE:						return "COMPLETE";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:			return "INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:	return "INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:			return "INCOMPLETE_DIMENSIONS";
		case GL_FRAMEBUFFER_UNSUPPORTED:					return "UNSUPPORTED";
		default:											return "unknown error";
	}
}

} // anonymous

// RenderSurfaceGLES30
RenderSurfaceGLES30::RenderSurfaceGLES30 (Type type, UInt32 format, int w, int h, int numSamples)
	: m_type		(type)
	, m_format		(format)
	, m_flags		(0)
{
	RenderSurfaceBase_Init(*this);
	width	= w;
	height	= h;
	samples	= numSamples;
}

// RenderTexture2DGLES30

RenderTexture2DGLES30::RenderTexture2DGLES30 (TextureID texID, UInt32 format, int w, int h)
	: RenderSurfaceGLES30	(kTypeTexture2D, format, w, h, 1)
{
	textureID = texID;

	TransferFormatGLES30	transferFmt	= GetTransferFormatGLES30(format);
	GLuint					glTexID		= TextureIdMapGLES30_QueryOrCreate(textureID);

	Assert(glTexID != 0);

	GetRealGfxDevice().SetTexture(kShaderFragment, 0, 0, textureID, kTexDim2D, std::numeric_limits<float>::infinity());
	GLES_CHK(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, transferFmt.format, transferFmt.dataType, 0));
}

RenderTexture2DGLES30::~RenderTexture2DGLES30 (void)
{
	// \todo [2013-04-29 pyry] Set texture storage to null?
}

void RenderTexture2DGLES30::AttachColor (int ndx, CubemapFace face)
{
	Assert(face == kCubeFaceUnknown);
	GLES_CHK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+ndx, GL_TEXTURE_2D, GetGLTextureID(), 0));
}

void RenderTexture2DGLES30::AttachDepthStencil (CubemapFace face)
{
	// \note Using GL_DEPTH_STENCIL_ATTACHMENT would eliminate one more call, but unfortunately
	//		 that is broken at least on ARM GLES3 EMU.
	Assert(face == kCubeFaceUnknown);

	GLES_CHK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, GetGLTextureID(), 0));

	if (IsDepthStencilFormat(m_format))
		GLES_CHK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, GetGLTextureID(), 0));
}

// RenderTextureCubeGLES30

RenderTextureCubeGLES30::RenderTextureCubeGLES30 (TextureID texID, UInt32 format, int w, int h)
	: RenderSurfaceGLES30	(kTypeTextureCube, format, w, h, 1)
{
	textureID = texID;

	TransferFormatGLES30	transferFmt	= GetTransferFormatGLES30(format);
	GLuint					glTexID		= TextureIdMapGLES30_QueryOrCreate(textureID);

	Assert(glTexID != 0);

	GetRealGfxDevice().SetTexture(kShaderFragment, 0, 0, textureID, kTexDimCUBE, std::numeric_limits<float>::infinity());

	for (int ndx = 0; ndx < 6; ndx++)
		GLES_CHK(glTexImage2D(kCubeFacesES3[ndx], 0, format, width, height, 0, transferFmt.format, transferFmt.dataType, 0));
}

RenderTextureCubeGLES30::~RenderTextureCubeGLES30 (void)
{
	// \todo [2013-04-29 pyry] Set texture storage to null?
}

void RenderTextureCubeGLES30::AttachColor (int ndx, CubemapFace face)
{
	const int faceIndex = clamp<int>(face,0,5); // can be passed -1 when restoring from previous RT
	GLES_CHK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+ndx, kCubeFacesES3[faceIndex], GetGLTextureID(), 0));
}

void RenderTextureCubeGLES30::AttachDepthStencil (CubemapFace face)
{
	const int faceIndex = clamp<int>(face,0,5); // can be passed -1 when restoring from previous RT

	GLES_CHK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, kCubeFacesES3[faceIndex], GetGLTextureID(), 0));

	if (IsDepthStencilFormat(m_format))
		GLES_CHK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, kCubeFacesES3[faceIndex], GetGLTextureID(), 0));
}

// RenderBufferGLES30

RenderBufferGLES30::RenderBufferGLES30 (UInt32 format, int w, int h, int numSamples)
	: RenderSurfaceGLES30	(kTypeRenderBuffer, format, w, h, numSamples)
	, m_bufferID			(0)
{
	glGenRenderbuffers(1, (GLuint*)&m_bufferID);
	glBindRenderbuffer(GL_RENDERBUFFER, m_bufferID);

	if (numSamples > 1)
		GLES_CHK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, format, width, height));
	else
		GLES_CHK(glRenderbufferStorage(GL_RENDERBUFFER, format, width, height));
}

RenderBufferGLES30::~RenderBufferGLES30 (void)
{
	if (m_bufferID)
		glDeleteRenderbuffers(1, (GLuint*)&m_bufferID);
}

void RenderBufferGLES30::AttachColor (int ndx, CubemapFace face)
{
	Assert(face == kCubeFaceUnknown);
	GLES_CHK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+ndx, GL_RENDERBUFFER, m_bufferID));
}

void RenderBufferGLES30::AttachDepthStencil (CubemapFace face)
{
	Assert(face == kCubeFaceUnknown);

	GLES_CHK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_bufferID));

	if (IsDepthStencilFormat(m_format))
		GLES_CHK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_bufferID));
}

void RenderBufferGLES30::Disown (void)
{
	m_bufferID = 0;
}

// RenderBufferCubeGLES30

RenderBufferCubeGLES30::RenderBufferCubeGLES30 (UInt32 format, int w, int h, int numSamples)
	: RenderSurfaceGLES30	(kTypeRenderBufferCube, format, w, h, numSamples)
{
	memset(&m_buffers[0], 0, sizeof(m_buffers));

	for (int i = 0; i < 6; i++)
		m_buffers[i] = new RenderBufferGLES30(format, width, height, numSamples);
}

RenderBufferCubeGLES30::~RenderBufferCubeGLES30 (void)
{
	for (int i = 0; i < 6; i++)
		delete m_buffers[i];
}

void RenderBufferCubeGLES30::AttachColor (int ndx, CubemapFace face)
{
	const int faceIndex = clamp<int>(face,0,5); // can be passed -1 when restoring from previous RT
	m_buffers[faceIndex]->AttachColor(ndx, kCubeFaceUnknown);
}

void RenderBufferCubeGLES30::AttachDepthStencil (CubemapFace face)
{
	const int faceIndex = clamp<int>(face,0,5); // can be passed -1 when restoring from previous RT
	m_buffers[faceIndex]->AttachDepthStencil(kCubeFaceUnknown);
}

// FramebufferAttachmentsGLES30

FramebufferAttachmentsGLES30::FramebufferAttachmentsGLES30 (int numColorAttachments, RenderSurfaceGLES30* colorAttachments, RenderSurfaceGLES30* depthStencilAttachment, CubemapFace face)
	: numColorAttachments	(numColorAttachments)
	, depthStencil			(depthStencilAttachment)
	, cubemapFace			(face)
{
	for (int ndx = 0; ndx < numColorAttachments; ndx++)
		color[ndx] = &colorAttachments[ndx];

	for (int ndx = numColorAttachments; ndx < kMaxColorAttachments; ndx++)
		color[ndx] = 0;
}

FramebufferAttachmentsGLES30::FramebufferAttachmentsGLES30 (void)
	: numColorAttachments	(0)
	, depthStencil			(0)
	, cubemapFace			(kCubeFaceUnknown)
{
	memset(&color[0], 0, sizeof(color));
}

// FramebufferObjectGLES30

FramebufferObjectGLES30::FramebufferObjectGLES30 (const FramebufferAttachmentsGLES30& attachments)
	: m_fboID		(0)
	, m_attachments	(attachments)
{
	GLuint oldFbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&oldFbo);

	glGenFramebuffers(1, (GLuint*)&m_fboID);
	GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, m_fboID));

	for (int colorNdx = 0; colorNdx < attachments.numColorAttachments; colorNdx++)
	{
		if (m_attachments.color[colorNdx] && m_attachments.color[colorNdx]->GetType() != RenderSurfaceGLES30::kTypeDummy)
			m_attachments.color[colorNdx]->AttachColor(colorNdx, m_attachments.cubemapFace);
	}

	if (m_attachments.depthStencil)
		m_attachments.depthStencil->AttachDepthStencil(m_attachments.cubemapFace);

	UInt32 status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		const char* statusName = GetFBOStatusName(status);
		ErrorStringMsg("Framebuffer is not complete: %s", statusName);
		Assert(status == GL_FRAMEBUFFER_COMPLETE);
	}

	// \todo [2013-04-30 pyry] We should really fail object construction if fbo is not complete.
	GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, oldFbo));
}

FramebufferObjectGLES30::~FramebufferObjectGLES30 (void)
{
	if (m_fboID)
		glDeleteFramebuffers(1, (GLuint*)&m_fboID);
}

void FramebufferObjectGLES30::Disown (void)
{
	m_fboID = 0;
}

RenderSurfaceGLES30* FramebufferObjectGLES30::GetColorAttachment (int ndx)
{
	Assert(0 <= ndx && ndx <= FramebufferAttachmentsGLES30::kMaxColorAttachments);
	return m_attachments.color[ndx];
}

const RenderSurfaceGLES30* FramebufferObjectGLES30::GetColorAttachment (int ndx) const
{
	Assert(0 <= ndx && ndx <= FramebufferAttachmentsGLES30::kMaxColorAttachments);
	return m_attachments.color[ndx];
}

RenderSurfaceGLES30* FramebufferObjectGLES30::GetDepthStencilAttachment (void)
{
	return m_attachments.depthStencil;
}

const RenderSurfaceGLES30* FramebufferObjectGLES30::GetDepthStencilAttachment (void) const
{
	return m_attachments.depthStencil;
}

// FramebufferObjectManagerGLES30

bool CompareFramebufferAttachmentsGLES30::operator() (const FramebufferAttachmentsGLES30* a, const FramebufferAttachmentsGLES30* b) const
{
	if		(a->numColorAttachments < b->numColorAttachments) return true;
	else if	(a->numColorAttachments > b->numColorAttachments) return false;

	if		(a->depthStencil < b->depthStencil) return true;
	else if	(a->depthStencil > b->depthStencil) return false;

	if		(a->cubemapFace < b->cubemapFace) return true;
	else if	(a->cubemapFace > b->cubemapFace) return false;

	for (int ndx = 0; ndx < a->numColorAttachments; ndx++)
	{
		if		(a->color[ndx] < b->color[ndx]) return true;
		else if	(a->color[ndx] > b->color[ndx]) return false;
	}

	return false; // Equal.
}

bool CompareRenderBufferParamsGLES30::operator() (const RenderBufferParamsGLES30& a, const RenderBufferParamsGLES30& b) const
{
	if		(a.format < b.format) return true;
	else if	(a.format > b.format) return false;

	if		(a.width < b.width) return true;
	else if	(a.width > b.width) return false;

	if		(a.height < b.height) return true;
	else if	(a.height > b.height) return false;

	return false; // Equal.
}

FramebufferObjectManagerGLES30::FramebufferObjectManagerGLES30 (void)
{
}

FramebufferObjectManagerGLES30::~FramebufferObjectManagerGLES30 (void)
{
	Clear();
}

void FramebufferObjectManagerGLES30::InvalidateObjects (void)
{
	// Call disown on all FBOs first.
	for (FramebufferObjectMapGLES30::iterator iter = m_fboMap.begin(); iter != m_fboMap.end(); ++iter)
		iter->second->Disown();

	for (RenderBufferMapGLES30::iterator iter = m_rbufMap.begin(); iter != m_rbufMap.end(); ++iter)
		iter->second->Disown();

	// Clear objects.
	Clear();
}

void FramebufferObjectManagerGLES30::Clear (void)
{
	// \todo [pyry] This actually invalidates keys as well. Is that okay in clear()?
	for (FramebufferObjectMapGLES30::iterator iter = m_fboMap.begin(); iter != m_fboMap.end(); ++iter)
		delete iter->second;
	m_fboMap.clear();

	for (RenderBufferMapGLES30::iterator iter = m_rbufMap.begin(); iter != m_rbufMap.end(); ++iter)
		delete iter->second;
	m_rbufMap.clear();
}

void FramebufferObjectManagerGLES30::InvalidateSurface (const RenderSurfaceGLES30* surface)
{
	std::vector<FramebufferObjectGLES30*> deleteFbos;

	for (FramebufferObjectMapGLES30::iterator iter = m_fboMap.begin(); iter != m_fboMap.end(); ++iter)
	{
		if (IsInFramebufferAttachmentsGLES30(*iter->first, surface))
			deleteFbos.push_back(iter->second);
	}

	for (std::vector<FramebufferObjectGLES30*>::iterator iter = deleteFbos.begin(); iter != deleteFbos.end(); ++iter)
	{
		m_fboMap.erase((*iter)->GetAttachments());
		delete *iter;
	}
}

FramebufferObjectGLES30* FramebufferObjectManagerGLES30::GetFramebufferObject (const FramebufferAttachmentsGLES30& attachments)
{
	// Try to fetch from cache.
	{
		FramebufferObjectMapGLES30::const_iterator fboPos = m_fboMap.find(&attachments);
		if (fboPos != m_fboMap.end())
			return fboPos->second;
	}

	DBG_LOG_RT_GLES30("FramebufferObjectManagerGLES30::GetFramebufferObject(): cache miss, creating FBO");

	// Not found - create a new one and insert.
	{
		FramebufferObjectGLES30* fbo = new FramebufferObjectGLES30(attachments);
		m_fboMap.insert(std::make_pair(fbo->GetAttachments(), fbo));
		return fbo;
	}
}

RenderBufferGLES30* FramebufferObjectManagerGLES30::GetRenderBuffer (UInt32 format, int width, int height)
{
	RenderBufferParamsGLES30 params(format, width, height);

	{
		RenderBufferMapGLES30::const_iterator pos = m_rbufMap.find(params);
		if (pos != m_rbufMap.end())
			return pos->second;
	}

	DBG_LOG_RT_GLES30("FramebufferObjectManagerGLES30::GetRenderBuffer(): cache miss, creating RBO");

	{
		RenderBufferGLES30* buf = new RenderBufferGLES30(format, width, height, 1);
		m_rbufMap.insert(std::make_pair(params, buf));
		return buf;
	}
}

// Utilities

bool IsInFramebufferAttachmentsGLES30 (const FramebufferAttachmentsGLES30& attachments, const RenderSurfaceGLES30* renderSurface)
{
	for (int ndx = 0; ndx < attachments.numColorAttachments; ndx++)
	{
		if (attachments.color[ndx] == renderSurface)
			return true;
	}

	if (attachments.depthStencil == renderSurface)
		return true;

	return false;
}

void BindFramebufferObjectGLES30 (FramebufferObjectGLES30* fbo)
{
	Assert(fbo != 0);

	GLenum	drawBuffers			[FramebufferAttachmentsGLES30::kMaxColorAttachments];
	GLenum	discardBuffers		[FramebufferAttachmentsGLES30::kMaxColorAttachments+1];
	int		drawBufferNdx		= 0;
	int		discardBufferNdx	= 0;

	for (int ndx = 0; ndx < fbo->GetNumColorAttachments(); ndx++)
	{
		RenderSurfaceGLES30* attachment = fbo->GetColorAttachment(ndx);

		if (attachment)
			drawBuffers[drawBufferNdx++] = GL_COLOR_ATTACHMENT0+ndx;

		if (attachment && (attachment->GetFlags() & RenderSurfaceGLES30::kDiscardOnBind))
		{
			if(fbo->GetFboID() == 0)
				discardBuffers[discardBufferNdx++] = GL_COLOR;
			else
				discardBuffers[discardBufferNdx++] = GL_COLOR_ATTACHMENT0+ndx;
			attachment->SetFlags(attachment->GetFlags() & ~RenderSurfaceGLES30::kDiscardOnBind);
		}
	}

	if (fbo->GetDepthStencilAttachment())
	{
		RenderSurfaceGLES30* depthStencilAttachment = fbo->GetDepthStencilAttachment();
		if (depthStencilAttachment->GetFlags() & RenderSurfaceGLES30::kDiscardOnBind && depthStencilAttachment->GetType() != RenderSurfaceGLES30::kTypeDummy)
		{
			if(fbo->GetFboID() == 0)
			{
				discardBuffers[discardBufferNdx++] = GL_DEPTH;
				discardBuffers[discardBufferNdx++] = GL_STENCIL;
			}
			else
			{
				discardBuffers[discardBufferNdx++] = GL_DEPTH_ATTACHMENT;
				discardBuffers[discardBufferNdx++] = GL_STENCIL_ATTACHMENT;
			}

			depthStencilAttachment->SetFlags(depthStencilAttachment->GetFlags() & ~RenderSurfaceGLES30::kDiscardOnBind);
		}
	}

	GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, fbo->GetFboID()));
	GLES_CHK(glDrawBuffers(drawBufferNdx, &drawBuffers[0]));

	if (discardBufferNdx > 0)
		GLES_CHK(glInvalidateFramebuffer(GL_FRAMEBUFFER, discardBufferNdx, &discardBuffers[0]));
}

void BindDefaultFramebufferGLES30 (void)
{
	GLuint defaultDrawBuffer = GL_BACK;
	GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	GLES_CHK(glDrawBuffers(1, &defaultDrawBuffer));
}

FramebufferObjectGLES30* GetResolveFramebufferObjectGLES30 (FramebufferObjectManagerGLES30* fboManager, UInt32 colorFormat, UInt32 depthStencilFormat, int width, int height)
{
	RenderBufferGLES30*				colorBuf	= colorFormat			!= 0 ? fboManager->GetRenderBuffer(colorFormat,			width, height) : 0;
	RenderBufferGLES30*				depthBuf	= depthStencilFormat	!= 0 ? fboManager->GetRenderBuffer(depthStencilFormat,	width, height) : 0;
	FramebufferAttachmentsGLES30	attachments;

	attachments.color[0]			= colorBuf;
	attachments.depthStencil		= depthBuf;
	attachments.numColorAttachments	= colorBuf ? 1 : 0;

	return fboManager->GetFramebufferObject(attachments);
}

// back buffer
RenderSurfaceBase* CreateBackBufferColorSurfaceGLES3()
{
	DummyRenderSurfaceGLES30* rs = new DummyRenderSurfaceGLES30(0,0);
	RenderSurfaceBase_InitColor(*rs);
	rs->backBuffer = true;

	return rs;
}
RenderSurfaceBase* CreateBackBufferDepthSurfaceGLES3()
{
	DummyRenderSurfaceGLES30* rs = new DummyRenderSurfaceGLES30(0,0);
	RenderSurfaceBase_InitDepth(*rs);
	rs->backBuffer = true;

	return rs;
}


#endif // GFX_SUPPORTS_OPENGLES30
