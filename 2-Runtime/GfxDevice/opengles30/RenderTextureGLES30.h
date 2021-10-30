#pragma once

#if GFX_SUPPORTS_OPENGLES30

#include "Configuration/UnityConfigure.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"
#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/GfxDevice/TextureIdMap.h"
#include "Runtime/Graphics/RenderSurface.h"

#include <map>

//! Render surface base class (implementations in RenderTextureGLES30 or RenderBufferGLES30)
class RenderSurfaceGLES30 : public RenderSurfaceBase
{
public:
	enum Type
	{
		kTypeTexture2D = 0,
		kTypeTextureCube,
		kTypeRenderBuffer,
		kTypeRenderBufferCube,	//!< RenderSurface that contains one RenderBuffer for each cubemap face. Used for MSAA cubemap rendering.
		kTypeDummy,				//!< Dummy surface (no real storage), used when doing depth only.
	};

	enum Flags
	{
		kDiscardOnBind	= (1<<0),	//!< Discard this attachment when bound as render target next time.
	};

	virtual						~RenderSurfaceGLES30		(void) {}

	Type						GetType						(void) const { return m_type;		}
	UInt32						GetFormat					(void) const { return m_format;		}
	int							GetWidth					(void) const { return width;		}
	int							GetHeight					(void) const { return height;		}
	int							GetNumSamples				(void) const { return samples;		}

	UInt32						GetFlags					(void) const	{ return m_flags;	}
	void						SetFlags					(UInt32 flags)	{ m_flags = flags;	}

	virtual void				AttachColor					(int ndx, CubemapFace face)	= 0;
	virtual void				AttachDepthStencil			(CubemapFace face)			= 0;

protected:
								RenderSurfaceGLES30			(Type type, UInt32 format, int width, int height, int numSamples);

	const Type					m_type;			//!< RenderSurface type
	const UInt32				m_format;		//!< Internal format

	UInt32						m_flags;		//!< Flags.

private:
								RenderSurfaceGLES30			(const RenderSurfaceGLES30& other); // Not allowed!
	RenderSurfaceGLES30&		operator=					(const RenderSurfaceGLES30& other); // Not allowed!
};

class DummyRenderSurfaceGLES30 : public RenderSurfaceGLES30
{
public:
								DummyRenderSurfaceGLES30	(int width, int height) : RenderSurfaceGLES30(kTypeDummy, 0, width, height, 1) {}
								~DummyRenderSurfaceGLES30	(void) {}

	virtual void				AttachColor					(int ndx, CubemapFace face)	{}
	virtual void				AttachDepthStencil			(CubemapFace face)			{}
};

class RenderTexture2DGLES30 : public RenderSurfaceGLES30
{
public:
								RenderTexture2DGLES30		(TextureID textureID, UInt32 format, int width, int height);
								~RenderTexture2DGLES30		(void);

	virtual void				AttachColor					(int ndx, CubemapFace face);
	virtual void				AttachDepthStencil			(CubemapFace face);

	TextureID					GetTextureID				(void) const { return textureID; }
	UInt32						GetGLTextureID				(void) const { return (UInt32)TextureIdMap::QueryNativeTexture(textureID); }
};

class RenderTextureCubeGLES30 : public RenderSurfaceGLES30
{
public:
								RenderTextureCubeGLES30		(TextureID textureID, UInt32 format, int width, int height);
								~RenderTextureCubeGLES30	(void);

	virtual void				AttachColor					(int ndx, CubemapFace face);
	virtual void				AttachDepthStencil			(CubemapFace face);

	TextureID					GetTextureID				(void) const { return textureID; }
	UInt32						GetGLTextureID				(void) const { return (UInt32)TextureIdMap::QueryNativeTexture(textureID); }
};

class RenderBufferGLES30 : public RenderSurfaceGLES30
{
public:
								RenderBufferGLES30			(UInt32 format, int width, int height, int numSamples);
								~RenderBufferGLES30			(void);

	virtual void				AttachColor					(int ndx, CubemapFace face);
	virtual void				AttachDepthStencil			(CubemapFace face);

	UInt32						GetBufferID					(void) const { return m_bufferID; }

	//! RenderBuffer specific: remove current buffer handle and do not attempt to destroy it.
	void						Disown						(void);

private:
	UInt32						m_bufferID;
};

class RenderBufferCubeGLES30 : public RenderSurfaceGLES30
{
public:
								RenderBufferCubeGLES30		(UInt32 format, int width, int height, int numSamples);
								~RenderBufferCubeGLES30		(void);

	virtual void				AttachColor					(int ndx, CubemapFace face);
	virtual void				AttachDepthStencil			(CubemapFace face);

	const RenderBufferGLES30&	GetBuffer					(CubemapFace face) const;

private:
	RenderBufferGLES30*			m_buffers[6];
};

//!< FBO attachment container - used as FBO map key and in FramebufferObject
class FramebufferAttachmentsGLES30
{
public:
	enum
	{
		kMaxColorAttachments		= 4
	};

								FramebufferAttachmentsGLES30	(int numColorAttachments, RenderSurfaceGLES30* colorAttachments, RenderSurfaceGLES30* depthStencilAttachment, CubemapFace face);
								FramebufferAttachmentsGLES30	(void);

	int							numColorAttachments;			//!< Number of valid attachments in color[] array. Rest are zero-filled.
	RenderSurfaceGLES30*		color[kMaxColorAttachments];	//!< Color attachments.
	RenderSurfaceGLES30*		depthStencil;					//!< Depth or depth-stencil attachment, if such is used.
	CubemapFace					cubemapFace;					//!< Applies to cubemap attachments only.
};

class FramebufferObjectGLES30
{
public:
										FramebufferObjectGLES30		(const FramebufferAttachmentsGLES30& attachments);
										~FramebufferObjectGLES30	(void);

	UInt32								GetFboID					(void) const	{ return m_fboID;							}

	const FramebufferAttachmentsGLES30*	GetAttachments				(void) const	{ return &m_attachments;					}

	int									GetNumColorAttachments		(void) const	{ return m_attachments.numColorAttachments;	}
	CubemapFace							GetCubemapFace				(void) const	{ return m_attachments.cubemapFace;			}

	RenderSurfaceGLES30*				GetColorAttachment			(int ndx);
	const RenderSurfaceGLES30*			GetColorAttachment			(int ndx) const;

	RenderSurfaceGLES30*				GetDepthStencilAttachment	(void);
	const RenderSurfaceGLES30*			GetDepthStencilAttachment	(void) const;

	//! Disown and remove fbo handle. Used if destructor should not try to delete fbo for some reason (context lost for example).
	void								Disown						(void);

private:
	UInt32								m_fboID;
	FramebufferAttachmentsGLES30		m_attachments;
};

class RenderBufferParamsGLES30
{
public:
	UInt32		format;
	int			width;
	int			height;
	// \note No numSamples since these are currently used for allocating resolve buffers only.
	//       Sample count may be added if temporary buffers are required for something else.

	RenderBufferParamsGLES30 (UInt32 format_, int width_, int height_)
		: format	(format_)
		, width		(width_)
		, height	(height_)
	{
	}
};

// FramebufferObject map implementation.
//
// Pointers to actual key data is used, since they are cheaper to move around. When inserting,
// key pointer is acquired from FramebufferObject. When searching, it is up to user to provide
// valid pointer (usually from stack).
struct CompareFramebufferAttachmentsGLES30
{
	bool operator() (const FramebufferAttachmentsGLES30* a, const FramebufferAttachmentsGLES30* b) const;
};
typedef std::map<const FramebufferAttachmentsGLES30*, FramebufferObjectGLES30*, CompareFramebufferAttachmentsGLES30> FramebufferObjectMapGLES30;

// RenderBuffer map implementation.
struct CompareRenderBufferParamsGLES30
{
	bool operator() (const RenderBufferParamsGLES30& a, const RenderBufferParamsGLES30& b) const;
};
typedef std::map<RenderBufferParamsGLES30, RenderBufferGLES30*, CompareRenderBufferParamsGLES30> RenderBufferMapGLES30;

class FramebufferObjectManagerGLES30
{
public:
									FramebufferObjectManagerGLES30		(void);
									~FramebufferObjectManagerGLES30		(void);

	//! Mark API objects invalid and clear cache (use on context loss)
	void							InvalidateObjects					(void);

	//!< Destroy all internal and API objects (clear cache and free objects).
	void							Clear								(void);

	//!< Destroy all FBOs where render surface is attached.
	void							InvalidateSurface					(const RenderSurfaceGLES30* surface);

	//! Create (or fetch from cache) framebuffer object to hold given attachment set.
	FramebufferObjectGLES30*		GetFramebufferObject				(const FramebufferAttachmentsGLES30& attachments);

	//! Create (or fetch from cache) temporary render buffer.
	//
	// Temporary render buffers are currently used as resolve buffers.
	// Callee should not store returned buffer anywhere since it will be
	// re-used.
	RenderBufferGLES30*				GetRenderBuffer						(UInt32 format, int width, int height);

private:
	FramebufferObjectMapGLES30		m_fboMap;
	RenderBufferMapGLES30			m_rbufMap;
};

//! Check if surface is in given FBO attachment list.
bool IsInFramebufferAttachmentsGLES30 (const FramebufferAttachmentsGLES30& attachments, const RenderSurfaceGLES30* renderSurface);

//! Bind FBO and setup for drawing
//
// This call setups FBO as rendering target:
//  1) FBO is bound to current context
//  2) If any of attachments have actions deferred to next bind defined, they are executed
//     + kDiscardOnBind: InvalidateFramebuffer() is called for those attachments
//  3) DrawBuffers is set up based on attachments
void BindFramebufferObjectGLES30 (FramebufferObjectGLES30* fbo);

//! Bind default framebuffer (0)
//
// Changes FBO binding to 0 and sets GL_BACK as draw buffer.
void BindDefaultFramebufferGLES30 (void);

//! Get resolve FBO.
//
// \param fboManager			FBO manager
// \param colorFormat			Color buffer format or 0 if not used.
// \param depthStencilFormat	Depth / depth-stencil format or 0 if not used
// \param width
// \param height
//
// Creates FBO for doing resolve. Buffers are allocated using fboManager->GetRenderBuffer()
// and thus will be re-used by subsequent resolve FBOs if formats match.
FramebufferObjectGLES30* GetResolveFramebufferObjectGLES30 (FramebufferObjectManagerGLES30* fboManager, UInt32 colorFormat, UInt32 depthStencilFormat, int width, int height);

#endif // GFX_SUPPORTS_OPENGLES30
