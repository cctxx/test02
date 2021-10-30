#pragma once

#include "D3D9Includes.h"
#include "Runtime/Graphics/TextureFormat.h"
#include "Runtime/Graphics/RenderSurface.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Threads/AtomicOps.h"
#include <map>

class ImageReference;

class TexturesD3D9
{
public:
	TexturesD3D9() {}
	~TexturesD3D9() {}
	bool SetTexture (ShaderType shaderType, int unit, TextureID textureID);
	void SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace );

	void DeleteTexture( TextureID textureID );

	void UploadTexture2D(
		TextureID tid, TextureDimension dimension, UInt8* srcData, int width, int height,
		TextureFormat format, int mipCount, UInt32 uploadFlags, int masterTextureLimit, TextureUsageMode usageMode, TextureColorSpace colorSpace );

	void UploadTextureSubData2D(
		TextureID tid, UInt8* srcData, int mipLevel,
		int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace );

	void UploadTextureCube(
		TextureID tid, UInt8* srcData, int faceDataSize, int size,
		TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace );

	void UploadTexture3D(
		TextureID tid, UInt8* srcData, int width, int height, int depth,
		TextureFormat format, int mipCount, UInt32 uploadFlags );

	void AddTexture( TextureID textureID, IDirect3DBaseTexture9* texture );
	void RemoveTexture( TextureID textureID );
	IDirect3DBaseTexture9* GetTexture( TextureID textureID ) const;

	intptr_t	RegisterNativeTexture(IDirect3DBaseTexture9* texture) const;
	void		UpdateNativeTexture(TextureID textureID, IDirect3DBaseTexture9* texture);
};

struct RenderSurfaceD3D9 : RenderSurfaceBase
{
	RenderSurfaceD3D9()
		: m_Texture(NULL)
		, m_Surface(NULL)
	{
		RenderSurfaceBase_Init(*this);
	}
	void Release() {
		if (m_Texture) {
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_Texture);
			m_Texture->Release();
			m_Texture = NULL;
		}
		if (m_Surface) {
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_Surface);
			m_Surface->Release();
			m_Surface = NULL;
		}
	}
	IDirect3DBaseTexture9*	m_Texture;
	IDirect3DSurface9* m_Surface;
};

struct RenderColorSurfaceD3D9 : public RenderSurfaceD3D9
{
	RenderColorSurfaceD3D9()
		: format(kRTFormatARGB32)
		, dim(kTexDim2D)
	{
		RenderSurfaceBase_InitColor(*this);
	}
	RenderTextureFormat	format;
	TextureDimension dim;
};

struct RenderDepthSurfaceD3D9 : public RenderSurfaceD3D9
{
	RenderDepthSurfaceD3D9()
		: depthFormat(kDepthFormatNone)
	{
		RenderSurfaceBase_InitDepth(*this);
	}
	DepthBufferFormat depthFormat;
};
