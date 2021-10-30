#include "UnityPrefix.h"
#include "TexturesD3D11.h"
#include "D3D11Context.h"
#include "D3D11Utils.h"
#include "Runtime/Allocator/FixedSizeAllocator.h"
#include "Runtime/Graphics/TextureFormat.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/GfxDevice/TextureUploadUtils.h"
#include "Runtime/GfxDevice/TextureIdMap.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Graphics/Texture2D.h"
#include "External/ProphecySDK/include/prcore/Surface.hpp"
#include "Runtime/Utilities/InitializeAndCleanup.h"


//#define ENABLE_OLD_TEXTURE_UPLOAD UNITY_WINRT
#define ENABLE_OLD_TEXTURE_UPLOAD 0

typedef FixedSizeAllocator<sizeof(TexturesD3D11::D3D11Texture)>	TextureAllocator;
static TextureAllocator* _TextureAlloc = NULL;

namespace TextureD3D11Alloc
{
	void StaticInitialize()
	{
		_TextureAlloc = UNITY_NEW_AS_ROOT(TextureAllocator(kMemGfxDevice),kMemGfxDevice, "TextureStructs", "");
	}

	void StaticDestroy()
	{
		UNITY_DELETE(_TextureAlloc, kMemGfxDevice);
	}
}

static RegisterRuntimeInitializeAndCleanup s_TextureAllocManagerCallbacks(TextureD3D11Alloc::StaticInitialize, TextureD3D11Alloc::StaticDestroy);

static inline intptr_t AllocD3DTexture(ID3D11Resource* tex, ID3D11ShaderResourceView* srv, ID3D11UnorderedAccessView* uav, bool shadowMap)
{
	return (intptr_t)(new (_TextureAlloc->alloc()) TexturesD3D11::D3D11Texture(tex, srv, uav, shadowMap));
}

static inline TexturesD3D11::D3D11Texture* QueryD3DTexture(TextureID textureID)
{
	return (TexturesD3D11::D3D11Texture*)TextureIdMap::QueryNativeTexture(textureID);
}


static void SwizzleToRGBA (const UInt8* src, UInt8* dst, int width, int height, int dstPitch, TextureFormat format)
{
	if (format == kTexFormatAlphaLum16)
	{
		// Handle AlphaLum16 case. ProphecySDK does not support 16 bit/channel formats,
		// so we blit manually.
		UInt32 rowBytes = GetRowBytesFromWidthAndFormat(width,kTexFormatAlphaLum16);
		const UInt8* srcRowData = src;
		UInt8* destRowData = dst;
		for( int r = 0; r < height; ++r )
		{
			for( int c = 0; c < width; ++c )
			{
				DWORD val = srcRowData[c*2+1];
				((DWORD*)destRowData)[c] = 0xFF000000 | (val<<16) | (val<<8) | (val);
			}
			srcRowData += rowBytes;
			destRowData += dstPitch;
		}
	}
	else
	{
		prcore::Surface srcSurface (width, height, GetRowBytesFromWidthAndFormat(width,format), GetProphecyPixelFormat(format), (void*)src);
		prcore::Surface dstSurface (width, height, dstPitch, prcore::PixelFormat(32,0x000000ff,0x0000ff00,0x00ff0000,0xff000000), dst);
		dstSurface.BlitImage (srcSurface, prcore::Surface::BLIT_COPY);
	}
}


struct FormatDesc11 {
	TextureFormat		unityformat;
	DXGI_FORMAT			d3dformat;
	DXGI_FORMAT			sRGBD3dformat;
};

const static FormatDesc11 kTextureFormatTable[kTexFormatPCCount+1] =
{
	{ kTexFormatPCCount,	DXGI_FORMAT_UNKNOWN,		DXGI_FORMAT_UNKNOWN },
	{ kTexFormatAlpha8,		DXGI_FORMAT_A8_UNORM,		DXGI_FORMAT_A8_UNORM }, // Alpha8
	{ kTexFormatARGB4444,	DXGI_FORMAT_R8G8B8A8_UNORM,	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB }, // ARGB4444
	{ kTexFormatRGB24,		DXGI_FORMAT_R8G8B8A8_UNORM,	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB }, // RGB24
	{ kTexFormatRGBA32,		DXGI_FORMAT_R8G8B8A8_UNORM,	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB }, // RGBA32
	{ kTexFormatARGB32,		DXGI_FORMAT_R8G8B8A8_UNORM,	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB }, // ARGB32
	{ kTexFormatARGBFloat,	DXGI_FORMAT_UNKNOWN,		DXGI_FORMAT_UNKNOWN }, // ARGBFloat
	{ kTexFormatRGB565,		DXGI_FORMAT_R8G8B8A8_UNORM,	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB }, // RGB565
	{ kTexFormatBGR24,		DXGI_FORMAT_R8G8B8A8_UNORM,	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB }, // BGR24
	{ kTexFormatAlphaLum16,	DXGI_FORMAT_R16_UNORM,		DXGI_FORMAT_R16_UNORM }, // AlphaLum16
	{ kTexFormatDXT1,		DXGI_FORMAT_BC1_UNORM,		DXGI_FORMAT_BC1_UNORM_SRGB }, // DXT1
	{ kTexFormatDXT3,		DXGI_FORMAT_BC2_UNORM,		DXGI_FORMAT_BC2_UNORM_SRGB }, // DXT3
	{ kTexFormatDXT5,		DXGI_FORMAT_BC3_UNORM,		DXGI_FORMAT_BC3_UNORM_SRGB }, // DXT5
	{ kTexFormatRGBA4444,	DXGI_FORMAT_R8G8B8A8_UNORM,	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB }, // RGBA4444
};


const static FormatDesc11 kTextureFormatA8Workaround =
	{ kTexFormatAlpha8, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM };
const static FormatDesc11 kTextureFormatR16Workaround =
	{ kTexFormatAlphaLum16, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM };
const static FormatDesc11 kTextureFormatBGRA32Workaround =
	{ kTexFormatBGRA32, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM };


static const FormatDesc11& GetUploadFormat (TextureFormat inFormat)
{
	// 9.1 doesn't support A8 at all; other 9.x levels only support without mipmaps.
	// To simplify things we just always expand to R8G8B8A8 on 9.x.
	if (gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0 && inFormat == kTexFormatAlpha8)
		return kTextureFormatA8Workaround;

	// 9.1 doesn't support R16 for AlphaLum16, so expand
	if (gGraphicsCaps.d3d11.featureLevel <= kDX11Level9_1 && inFormat == kTexFormatAlphaLum16)
		return kTextureFormatR16Workaround;

	// kTexFormatBGRA32 is one of those "esoteric" formats that is widely used for webcam textures
	if (inFormat == kTexFormatBGRA32)
		return kTextureFormatBGRA32Workaround;

	return kTextureFormatTable[inFormat];
}

TexturesD3D11::TexturesD3D11()
{
	InvalidateSamplers();
}

TexturesD3D11::~TexturesD3D11()
{
	#if !UNITY_EDITOR
	Assert(m_ComputeBuffers.empty());
	Assert(m_StagingTextures.empty());
	#endif
}

void TexturesD3D11::TextureFromShaderResourceView(ID3D11ShaderResourceView* resourceView, ID3D11Texture2D** texture) const
{
	ID3D11Resource* resource = NULL;
	resourceView->GetResource(&resource);
	Assert(resource && "ID3D11Resource is NULL");
	resource->Release();  // GetResource() calls AddRef()
	D3D11_RESOURCE_DIMENSION rType;
	resource->GetType(&rType);
	Assert(rType == D3D11_RESOURCE_DIMENSION_TEXTURE2D && "ID3D11Resource is not Texture2D");
	resource->QueryInterface(texture);
	Assert(texture && "Texture is NULL");
	(*texture)->Release();  // QueryInterface() calls AddRef()
}

intptr_t TexturesD3D11::RegisterNativeTexture(ID3D11ShaderResourceView* resourceView) const
{
	ID3D11Texture2D* texture = NULL;
	TextureFromShaderResourceView(resourceView, &texture);
	return AllocD3DTexture(texture, resourceView, 0, false);
}

void TexturesD3D11::UpdateNativeTexture(TextureID textureID, ID3D11ShaderResourceView* resourceView)
{
	ID3D11Texture2D* texture = NULL;
	TextureFromShaderResourceView(resourceView, &texture);
	D3D11Texture* target = QueryD3DTexture(textureID);
	if(target)
	{
		target->m_Texture = texture;
		target->m_SRV = resourceView;
	}
	else
		AddTexture(textureID, texture,  resourceView, 0, false);
}

void TexturesD3D11::AddTexture (TextureID textureID, ID3D11Resource* texture, ID3D11ShaderResourceView* srv, ID3D11UnorderedAccessView* uav, bool shadowMap)
{
	TextureIdMap::UpdateTexture(textureID, AllocD3DTexture(texture,srv,uav,shadowMap));
}

void TexturesD3D11::RemoveTexture( TextureID textureID )
{
	D3D11Texture* target = QueryD3DTexture(textureID);
	if(target)
	{
		target->~D3D11Texture();
		_TextureAlloc->free(target);
	}
	TextureIdMap::RemoveTexture(textureID);
}

TexturesD3D11::D3D11Texture* TexturesD3D11::GetTexture (TextureID textureID)
{
	return QueryD3DTexture(textureID);
}

void TexturesD3D11::AddComputeBuffer (ComputeBufferID id, const ComputeBuffer11& buf)
{
	m_ComputeBuffers.insert (std::make_pair(id, buf));
}

void TexturesD3D11::RemoveComputeBuffer (ComputeBufferID id)
{
	m_ComputeBuffers.erase (id);
}


ComputeBuffer11* TexturesD3D11::GetComputeBuffer (ComputeBufferID id)
{
	ComputeBufferMap::iterator it = m_ComputeBuffers.find(id);
	return it==m_ComputeBuffers.end() ? NULL : &it->second;
}


void TexturesD3D11::Upload2DData (const UInt8* dataPtr, TextureFormat dataFormat, int width, int height, bool decompressData, ID3D11Resource* dst, DXGI_FORMAT dstFormat, bool bgra, int dstSubResource)
{
	DX11_LOG_ENTER_FUNCTION("TexturesD3D11::Upload2DData (0x%08x, %d, %d, %d, %s, 0x%08x, %d, %d)",
		dataPtr, dataFormat, width, height,	GetDX11BoolString(decompressData), dst,	dstFormat, dstSubResource);

	if (bgra)
	{
		if (dstFormat == DXGI_FORMAT_R8G8B8A8_UNORM)
			dstFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
		else if (dstFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
			dstFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	}

	HRESULT hr;

	bool isDXT = false;
	int texWidth = width;
	int texHeight = height;
	int texMips = 1;
	if (!decompressData && IsCompressedDXTTextureFormat(dataFormat))
	{
		isDXT = true;
		while (texWidth < 4 || texHeight < 4)
		{
			texWidth *= 2;
			texHeight *= 2;
			++texMips;
		}
	}

	// find or create a staging texture of needed size & format
	UInt64 stagingKey = (UInt64(width) << kStagingWidthShift) | (UInt64(height) << kStagingHeightShift) | (UInt64(dstFormat) << kStagingFormatShift);
	StagingTextureMap::iterator it = m_StagingTextures.find (stagingKey);
	if (it == m_StagingTextures.end())
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = texWidth;
		desc.Height = texHeight;
		desc.MipLevels = texMips;
		desc.ArraySize = 1;
		desc.Format = dstFormat;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;

		ID3D11Device* dev = GetD3D11Device();
		ID3D11Texture2D* texture = NULL;
		hr = dev->CreateTexture2D (&desc, NULL, &texture);
		if (FAILED(hr))
		{
			printf_console ("d3d11: failed to create staging 2D texture w=%i h=%i d3dfmt=%i [%x]\n", width, height, dstFormat, hr);
			return;
		}
		SetDebugNameD3D11 (texture, Format("Staging-Texture2D-%dx%d-fmt%d", width, height, dstFormat));

		it = m_StagingTextures.insert (std::make_pair(stagingKey, texture)).first;
	}

	// copy/convert source data into the staging texture
	const int stagingSubresource = texMips-1;
	ID3D11DeviceContext* ctx = GetD3D11Context();
	D3D11_MAPPED_SUBRESOURCE mapped;
	hr = ctx->Map(it->second, stagingSubresource, D3D11_MAP_WRITE, 0, &mapped);
	if (FAILED(hr))
	{
		printf_console ("d3d11: failed to map staging 2D texture w=%i h=%i d3dfmt=%i [%x]\n", width, height, dstFormat, hr);
		return;
	}

	///@TODO: more format conversions?
	if (decompressData)
	{
		//int tmpWidth = std::max(width,4);
		int tmpHeight = std::max(height,4);
		DecompressNativeTextureFormatWithMipLevel (dataFormat, width, height, 0 /*TODO*/, (const UInt32*)dataPtr, mapped.RowPitch/4, tmpHeight, (UInt32*)mapped.pData);
		//PerformUploadConversions (width, height, rgba, tempBufferPitch, usageMode, colorSpace, GetProphecyPixelFormat(kTexFormatRGBA32)); //@TODO?
	}
	else if (dstFormat == DXGI_FORMAT_R8G8B8A8_UNORM || dstFormat == DXGI_FORMAT_B8G8R8A8_UNORM)
	{
		SwizzleToRGBA (dataPtr, (UInt8*)mapped.pData, width, height, mapped.RowPitch, dataFormat);
	}
	else
	{
		size_t dataSize = CalculateImageSize(width,height,dataFormat);
		const int minSize = isDXT ? 4 : 1;
		int mipWidth = std::max(texWidth >> stagingSubresource, minSize);
		int mipHeight = std::max(texHeight >> stagingSubresource, minSize);
		size_t mappedSize = mapped.RowPitch * mipHeight;
		// pitch for compressed formats is for full row of blocks
		if (isDXT)
		{
			mappedSize /= 4;
			mipHeight /= 4;
		}
		UInt8* mappedData = (UInt8*)mapped.pData;
		if (dataSize == mappedSize)
		{
			memcpy(mappedData, dataPtr, dataSize);
		}
		else
		{
			// For compressed formats, height was already divided by block height, so this works out to operate
			// on full row blocks
			const size_t dataRowPitch = dataSize / mipHeight;
			for (int y = 0; y < mipHeight; ++y)
			{
				memcpy(mappedData, dataPtr, dataRowPitch);
				mappedData += mapped.RowPitch;
				dataPtr += dataRowPitch;
			}
		}
	}
	ctx->Unmap(it->second, stagingSubresource);


	// copy from staging into destination
	ctx->CopySubresourceRegion (dst, dstSubResource, 0, 0, 0, it->second, stagingSubresource, NULL);
	// Sometime CopySubresourceRegion hangs, that's why we have a message here 
	DX11_LOG_OUTPUT("TexturesD3D11::Upload2DData done");
}


void TexturesD3D11::UploadTexture2D(
	TextureID tid, TextureDimension dimension, UInt8* srcData, int width, int height,
	TextureFormat format, int mipCount, UInt32 uploadFlags, int masterTextureLimit, TextureUsageMode usageMode, TextureColorSpace colorSpace )
{
	AssertIf( srcData == NULL );
	AssertIf( (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)) && !IsNPOTTextureAllowed(mipCount > 1) );

	if( dimension != kTexDim2D )
	{
		ErrorString( "Incorrect texture dimension!" );
		return;
	}

	// Nothing to do here. Early out instead of failing, empty textures are serialized by dynamic fonts.
	if( width == 0 || height == 0 )
		return;

	// Figure out whether we'll upload compressed or decompress on the fly
	bool uploadIsCompressed, decompressOnTheFly;
	HandleFormatDecompression (format, &usageMode, colorSpace, &uploadIsCompressed, &decompressOnTheFly);
	if (decompressOnTheFly)
		uploadIsCompressed = false;

	// find the texture
	D3D11Texture* target = QueryD3DTexture(tid);

	// For compressed textures, stop applying masterTextureLimit if texture size drops below 4
	if( uploadIsCompressed )
	{
		while( masterTextureLimit > 0 && ((width >> masterTextureLimit) < 4 || (height >> masterTextureLimit) < 4) )
		{
			--masterTextureLimit;
		}
	}

	// skip several levels in data based on masterTextureLimit
	int maxLevel = mipCount - 1;
	int baseLevel = std::min( masterTextureLimit, maxLevel );
	int level;
	for( level = 0; level < baseLevel; ++level )
	{
		srcData += CalculateImageSize (width, height, format);
		AssertIf( width == 1 && height == 1 && level != maxLevel );
		width = std::max( width / 2, 1 );
		height = std::max( height / 2, 1 );
	}

	const FormatDesc11& uploadFormat = GetUploadFormat(decompressOnTheFly ? kTexFormatRGBA32 : format);
	DXGI_FORMAT d3dFormat = (colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB) ? uploadFormat.sRGBD3dformat : uploadFormat.d3dformat;
	const bool needBGRA = (uploadFlags & GfxDevice::kUploadTextureOSDrawingCompatible);

	// While texture is too large for hardware limits, skip mip levels.
	int texWidth = width, texHeight = height;
	prcore::Surface::BlitMode blitMode = prcore::Surface::BLIT_COPY;
	while( texWidth > gGraphicsCaps.maxTextureSize || texHeight > gGraphicsCaps.maxTextureSize )
	{
		if( baseLevel < maxLevel )
		{
			srcData += CalculateImageSize (texWidth, texHeight, format);
			width = std::max( width / 2, 1 );
			height = std::max( height / 2, 1 );
			++baseLevel;
		}
		texWidth = std::max( texWidth / 2, 1 );
		texHeight = std::max( texHeight / 2, 1 );
		blitMode = prcore::Surface::BLIT_SCALE;
		if( texWidth <= 4 && texHeight <= 4 )
			break;
	}

	ID3D11Device* dev = GetD3D11Device();

	// create texture if it does not exist already
	ID3D11Texture2D* texture = NULL;
	if(!target)
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = texWidth;
		desc.Height = texHeight;
		desc.MipLevels = mipCount - baseLevel;
		desc.ArraySize = 1;
		desc.Format = d3dFormat;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		// GDI-compatible textures require certain restrictions
		if (needBGRA)
		{
			desc.MiscFlags |= D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
			desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			// BGRA format
			if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM)
				desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			else if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
				desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			else {
				AssertString("invalid d3d11 texture format for OS compatible texture");
			}
		}
		HRESULT hr = dev->CreateTexture2D (&desc, NULL, &texture);
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(texture, CalculateImageSize(texWidth, texHeight,format)*(mipCount>1?1.33:1),tid.m_ID); 
		if( FAILED(hr) )
			printf_console( "d3d11: failed to create 2D texture id=%i w=%i h=%i mips=%i d3dfmt=%i [%x]\n", tid, texWidth, texHeight, mipCount-baseLevel, d3dFormat, hr );
		SetDebugNameD3D11 (texture, Format("Texture2D-%d-%dx%d", tid.m_ID, texWidth, texHeight));

		// Create the shader resource view
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MostDetailedMip = 0;
		viewDesc.Texture2D.MipLevels = mipCount - baseLevel;

		ID3D11ShaderResourceView* srView = NULL;
		hr = dev->CreateShaderResourceView (texture, &viewDesc, &srView);
		if (FAILED(hr))
			printf_console ("d3d11: failed to create 2D texture view id=%i [%x]\n", tid, hr);

		SetDebugNameD3D11 (srView, Format("Texture2D-SRV-%d-%dx%d", tid.m_ID, texWidth, texHeight));
		TextureIdMap::UpdateTexture(tid, AllocD3DTexture(texture, srView, NULL, false));
	}
	else
	{
		texture = (ID3D11Texture2D*)target->m_Texture;
	}
	if( !texture )
	{
		AssertString( "failed to create 2D texture" );
		return;
	}

	prcore::PixelFormat pf;

	ID3D11DeviceContext* ctx = GetD3D11Context();

	UInt8* rgba = NULL;
	if (uploadFormat.d3dformat == DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		int tmpWidth = std::max(width,4);
		int tmpHeight = std::max(height,4);
		rgba = new UInt8[tmpWidth*tmpHeight*4];
	}

	// Upload the mip levels
	for( level = baseLevel; level <= maxLevel; ++level )
	{
#if !ENABLE_OLD_TEXTURE_UPLOAD
		if(decompressOnTheFly && IsCompressedFlashATFTextureFormat(format)){
			//Workaround for flash decompression, where we do not know the miplevel sizes.
			Image tempImage (width, height, kTexFormatRGBA32);
			DecompressNativeTextureFormatWithMipLevel(format, width, height, level, (UInt32*)srcData, width, height, (UInt32*)tempImage.GetImageData());
			Upload2DData (tempImage.GetImageData(), kTexFormatRGBA32, width, height, false, texture, uploadFormat.d3dformat, needBGRA, level-baseLevel);
		}else{
		Upload2DData (srcData, format, width, height, decompressOnTheFly, texture, uploadFormat.d3dformat, needBGRA, level-baseLevel);
		}
#else
		if (!uploadIsCompressed)
		{
			int srcPitch = 0;
			const UInt8* finalData = srcData;

			if (decompressOnTheFly)
			{
				int tmpWidth = std::max(width,4);
				int tmpHeight = std::max(height,4);
				DecompressNativeTextureFormatWithMipLevel (format, width, height, level, (UInt32*)srcData, tmpWidth,tmpHeight, (UInt32*)rgba);
				//PerformUploadConversions (width, height, rgba, tempBufferPitch, usageMode, colorSpace, GetProphecyPixelFormat(kTexFormatRGBA32)); //@TODO?
				srcPitch = width * 4;
				finalData = rgba;
			}
			else if (uploadFormat.d3dformat == DXGI_FORMAT_R8G8B8A8_UNORM)
			{
				srcPitch = width * 4;
				SwizzleToRGBA (srcData, rgba, width, height, srcPitch, format);
				finalData = rgba;
			}
			else
			{
				srcPitch = GetRowBytesFromWidthAndFormat(width,format);
			}

			///@TODO: more format conversions?
			ctx->UpdateSubresource (texture, level-baseLevel, NULL, finalData, srcPitch, 0);
		}
		else
		{
			int dxtWidth = std::max(width,4);
			int srcPitch = (format==kTexFormatDXT1) ? dxtWidth*2 : dxtWidth*4;
			if (width == texWidth && height == texHeight)
			{
				ctx->UpdateSubresource (texture, level-baseLevel, NULL, srcData, srcPitch, 0);
			}
			else
			{
				// TODO: fill with garbage?
			}
		}
#endif
		// Go to next level
		srcData += CalculateImageSize (width, height, format);
		AssertIf( width == 1 && height == 1 && level != maxLevel );
		width = std::max( width / 2, 1 );
		height = std::max( height / 2, 1 );
		texWidth = std::max( texWidth / 2, 1 );
		texHeight = std::max( texHeight / 2, 1 );
	}

	delete[] rgba;
}


void TexturesD3D11::UploadTextureSubData2D(
	TextureID tid, UInt8* srcData, int mipLevel,
	int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace )
{
	ID3D11DeviceContext* ctx = GetD3D11Context();
	if (!ctx)
		return;

	Assert(srcData != NULL);
	Assert(!IsAnyCompressedTextureFormat(format));

	D3D11Texture* target = QueryD3DTexture(tid);
	if (!target)
	{
		AssertString("Texture not found");
		return;
	}

	const FormatDesc11& uploadFormat = GetUploadFormat(format);
	ID3D11Resource* texture = (ID3D11Resource*)target->m_Texture;
	Assert(texture);

	D3D11_BOX destRegion;
	destRegion.left = x;
	destRegion.right = x + width;
	destRegion.top = y;
	destRegion.bottom = y + height;
	destRegion.front = 0;
	destRegion.back = 1;

	///@TODO: other format conversions?
	bool releaseUploadData = false;
	UInt8* uploadData = srcData;
	int srcPitch = GetRowBytesFromWidthAndFormat(width,format);
	if (uploadFormat.d3dformat == DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		uploadData = new UInt8[width*height*4];
		SwizzleToRGBA (srcData, uploadData, width, height, width*4, format);
		releaseUploadData = true;
		srcPitch = width*4;
	}
	ctx->UpdateSubresource (texture, mipLevel, &destRegion, uploadData, srcPitch, 0);
	if (releaseUploadData)
		delete[] uploadData;
}


void TexturesD3D11::UploadTextureCube(
	TextureID tid, UInt8* srcData, int faceDataSize, int size,
	TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace )
{
	ID3D11Device* dev = GetD3D11Device();
	if (!dev)
		return;

	if (gGraphicsCaps.buggyMipmappedCubemaps)
		mipCount = 1;

	// find the texture
	D3D11Texture* target = QueryD3DTexture(tid);

	// create texture if it does not exist already
	const FormatDesc11& uploadFormat = GetUploadFormat( format );
	bool sRGBUpload =  (colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB);

#if ENABLE_OLD_TEXTURE_UPLOAD
	// Due to confirmed Microsoft DirectX Runtime bug we can't supply cubemap subresources via UpdateSubresource() function
	// Prepare subresource data for CreateTexture2D() instead

	const int maxLevel = mipCount - 1;

	size_t uploadBufferSize = 0;
	TextureFormat formatForSizeCalc = (uploadFormat.d3dformat == DXGI_FORMAT_R8G8B8A8_UNORM) ? kTexFormatARGB32 : format;
	int mipSizeForCalc = size;
	for (int level = 0; level <= maxLevel; ++level)
	{
		uploadBufferSize += CalculateImageSize (mipSizeForCalc, mipSizeForCalc, formatForSizeCalc);
		mipSizeForCalc = std::max(mipSizeForCalc / 2, 1);
	}
	uploadBufferSize *= 6;

	UInt8* rawData = new UInt8[uploadBufferSize];
	UInt8* rawDataPtr = rawData;
	D3D11_SUBRESOURCE_DATA subresourceData[6*20];  //2^20 should be enough
	{
		bool uploadIsCompressed = IsCompressedDXTTextureFormat(format);

		ID3D11DeviceContext* ctx = GetD3D11Context();

		for (int face=0;face<6;face++)
		{
			int mipSize = size;
			UInt8* data = srcData + face * faceDataSize;

			// Upload the mip levels
			for (int level = 0; level <= maxLevel; ++level)
			{
				const UInt32 nLevelSize = CalculateImageSize( mipSize, mipSize, format );
				UInt32 levelRawSize = nLevelSize;

				///@TODO: handle format conversions
				int pitch = GetRowBytesFromWidthAndFormat(mipSize,format);
				if (uploadFormat.d3dformat == DXGI_FORMAT_R8G8B8A8_UNORM)
				{
					pitch = mipSize * 4;
					SwizzleToRGBA (data, rawDataPtr, mipSize, mipSize, pitch, format);
					levelRawSize = mipSize*mipSize*4;
				}
				else
				{
					memcpy (rawDataPtr, data, nLevelSize);
				}

				subresourceData[face*mipCount + level].pSysMem = rawDataPtr;
				subresourceData[face*mipCount + level].SysMemPitch = pitch;
				subresourceData[face*mipCount + level].SysMemSlicePitch = 0;

				// Go to next level
				data += nLevelSize;
				rawDataPtr += levelRawSize;
				AssertIf( mipSize == 1 && level != maxLevel );
				mipSize = std::max( mipSize / 2, 1 );
			}
		}
	}
#endif

	ID3D11Texture2D* texture = NULL;
	if (!target)
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = size;
		desc.Height = size;
		desc.MipLevels = mipCount;
		desc.ArraySize = 6;
		desc.Format = sRGBUpload ? uploadFormat.sRGBD3dformat : uploadFormat.d3dformat;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
#if ENABLE_OLD_TEXTURE_UPLOAD
		HRESULT hr = dev->CreateTexture2D (&desc, subresourceData, &texture);
#else
		HRESULT hr = dev->CreateTexture2D (&desc, NULL, &texture);
#endif
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(texture, 6 * CalculateImageSize(size, size, format)*(mipCount>1?1.33:1),tid.m_ID); 
		if (FAILED(hr))
			printf_console ("d3d11: failed to create Cube texture id=%i s=%i mips=%i d3dfmt=%i [%x]\n", tid, size, mipCount, sRGBUpload ? uploadFormat.sRGBD3dformat : uploadFormat.d3dformat, hr);
		SetDebugNameD3D11 (texture, Format("TextureCube-%d-%dx%d", tid.m_ID, size));

		// Create the shader resource view
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		viewDesc.Texture2D.MostDetailedMip = 0;
		viewDesc.Texture2D.MipLevels = mipCount;

		ID3D11ShaderResourceView* srView = NULL;
		hr = dev->CreateShaderResourceView (texture, &viewDesc, &srView);
		if (FAILED(hr))
			printf_console ("d3d11: failed to create Cube texture view id=%i [%x]\n", tid, hr);

		SetDebugNameD3D11 (srView, Format("TextureCube-SRV-%d-%d", tid.m_ID, size));

		TextureIdMap::UpdateTexture(tid, AllocD3DTexture(texture, srView, NULL, false));
	}
	else
	{
		texture = (ID3D11Texture2D*)target->m_Texture;
	}
#if ENABLE_OLD_TEXTURE_UPLOAD
	delete[] rawData;
#endif

	if (!texture)
	{
		AssertString( "failed to create cubemap" );
		return;
	}
#if !ENABLE_OLD_TEXTURE_UPLOAD
	// Upload data
	// Note: DX11 Runtime on 9.x levels has a bug where setting cubemap data via UpdateSubresource doesn't work (only first
	// subresource is ever updated). We use staging resources for upload here, but keep in mind if at some point you want
	// to switch away from them.
	const int maxLevel = mipCount - 1;
	for (int face=0;face<6;face++)
	{
		int mipSize = size;
		UInt8* data = srcData + face * faceDataSize;

		// Upload the mip levels
		for (int level = 0; level <= maxLevel; ++level)
		{
			Upload2DData (data, format, mipSize, mipSize, false, texture, uploadFormat.d3dformat, false, D3D11CalcSubresource(level,face,mipCount));

			data += CalculateImageSize(mipSize, mipSize, format);
			Assert(mipSize != 1 || level == maxLevel);
			mipSize = std::max(mipSize/2, 1);
		}
	}
#endif
}

void TexturesD3D11::UploadTexture3D(
	TextureID tid, UInt8* srcData, int width, int height, int depth,
	TextureFormat format, int mipCount, UInt32 uploadFlags )
{
	ID3D11Device* dev = GetD3D11Device();
	if (!dev)
		return;

	if (gGraphicsCaps.buggyMipmapped3DTextures)
		mipCount = 1;

	// find the texture
	D3D11Texture* target = QueryD3DTexture(tid);

	// create texture if it does not exist already
	const FormatDesc11& uploadFormat = GetUploadFormat(format);

	ID3D11Texture3D* texture = NULL;
	if (!target)
	{
		D3D11_TEXTURE3D_DESC desc;
		desc.Width = width;
		desc.Height = height;
		desc.Depth = depth;
		desc.MipLevels = mipCount;
		desc.Format = uploadFormat.d3dformat;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		HRESULT hr = dev->CreateTexture3D (&desc, NULL, &texture);
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(texture, depth * CalculateImageSize(width, height, format)*(mipCount>1?1.33:1),tid.m_ID); 
		if (FAILED(hr))
			printf_console ("d3d11: failed to create 3D texture id=%i s=%ix%ix%i mips=%i d3dfmt=%i [%x]\n", tid, width, height, depth, mipCount, uploadFormat.d3dformat, hr);
		SetDebugNameD3D11 (texture, Format("Texture3D-%d-%dx%dx%d", tid.m_ID, width, height, depth));

		// Create the shader resource view
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		viewDesc.Texture3D.MostDetailedMip = 0;
		viewDesc.Texture3D.MipLevels = mipCount;

		ID3D11ShaderResourceView* srView = NULL;
		hr = dev->CreateShaderResourceView (texture, &viewDesc, &srView);
		if (FAILED(hr))
			printf_console ("d3d11: failed to create 3D texture view id=%i [%x]\n", tid, hr);

		SetDebugNameD3D11 (srView, Format("Texture3D-SRV-%d-%dx%dx%d", tid.m_ID, width, height, depth));

		TextureIdMap::UpdateTexture(tid, AllocD3DTexture(texture, srView, NULL, false));
	}
	else
	{
		texture = (ID3D11Texture3D*)target->m_Texture;
	}
	if (!texture)
	{
		AssertString("failed to create 3D texture");
		return;
	}

	// Upload data
	bool uploadIsCompressed = IsCompressedDXTTextureFormat(format);

	ID3D11DeviceContext* ctx = GetD3D11Context();

	int maxLevel = mipCount - 1;

	UInt8* rgba = NULL;
	if (uploadFormat.d3dformat == DXGI_FORMAT_R8G8B8A8_UNORM)
		rgba = new UInt8[width*height*depth*4];

	for (int level = 0; level <= maxLevel; ++level)
	{
		///@TODO: handle format conversions
		const UInt8* finalData = srcData;
		int pitch = GetRowBytesFromWidthAndFormat(width,format);
		if (uploadFormat.d3dformat == DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			const UInt8* srcSlice = srcData;
			UInt8* dstSlice = rgba;
			for (int d = 0; d < depth; ++d)
			{
				SwizzleToRGBA (srcSlice, dstSlice, width, height, width*4, format);
				srcSlice += pitch * height;
				dstSlice += 4 * width * height;
			}
			finalData = rgba;
			pitch = width * 4;
		}

		ctx->UpdateSubresource (texture, level, NULL, finalData, pitch, pitch * height);

		// Go to next level
		srcData += CalculateImageSize(width, height, format) * height;
		width = std::max(width / 2, 1);
		height = std::max(height / 2, 1);
		depth = std::max(depth / 2, 1);
	}
	delete[] rgba;
}

static D3D11_TEXTURE_ADDRESS_MODE s_D3DWrapModes[kTexWrapCount] = {
	D3D11_TEXTURE_ADDRESS_WRAP,
	D3D11_TEXTURE_ADDRESS_CLAMP,
};
static D3D11_FILTER s_D3DFilters[kTexFilterCount] = {
	D3D11_FILTER_MIN_MAG_MIP_POINT,
	D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
	D3D11_FILTER_MIN_MAG_MIP_LINEAR,
};
static D3D11_FILTER s_D3DFiltersShadow[kTexFilterCount] = {
	D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
	D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
	D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
};

ID3D11SamplerState* TexturesD3D11::GetSampler(BuiltinSamplerState sampler)
{
	D3D11Sampler state;
	switch (sampler) {
	case kSamplerPointClamp:
		state.filter = kTexFilterNearest;
		state.wrap = kTexWrapClamp;
		break;
	case kSamplerLinearClamp:
		state.filter = kTexFilterBilinear;
		state.wrap = kTexWrapClamp;
		break;
	case kSamplerPointRepeat:
		state.filter = kTexFilterNearest;
		state.wrap = kTexWrapRepeat;
		break;
	case kSamplerLinearRepeat:
		state.filter = kTexFilterBilinear;
		state.wrap = kTexWrapRepeat;
		break;
	default: AssertString("unknown builtin sampler type");
	}
	return GetSampler (state);
}


ID3D11SamplerState* TexturesD3D11::GetSampler(const D3D11Sampler& texSampler)
{
	SamplerMap::iterator sit = m_Samplers.find(texSampler);
	if (sit != m_Samplers.end())
		return sit->second;

	ID3D11Device* dev = GetD3D11Device();
	ID3D11SamplerState* sampler = NULL;

	D3D11_SAMPLER_DESC desc;
	if (texSampler.flags & kSamplerShadowMap)
		desc.Filter = s_D3DFiltersShadow[texSampler.filter];
	else if (texSampler.anisoLevel > 1 && gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0)
		desc.Filter = D3D11_FILTER_ANISOTROPIC;
	else 
		desc.Filter = s_D3DFilters[texSampler.filter];
	desc.AddressU = desc.AddressV = desc.AddressW = s_D3DWrapModes[texSampler.wrap];
	desc.MipLODBias = texSampler.bias;
	desc.MaxAnisotropy = texSampler.anisoLevel;
	desc.ComparisonFunc = (gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0) ? D3D11_COMPARISON_LESS_EQUAL : D3D11_COMPARISON_LESS;
	desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 0.0f;
	desc.MinLOD = -FLT_MAX;
	desc.MaxLOD = FLT_MAX;
	HRESULT hr = dev->CreateSamplerState (&desc, &sampler);
	SetDebugNameD3D11 (sampler, Format("SamplerState-%d-%d", texSampler.filter, texSampler.wrap));

	sit = m_Samplers.insert (std::make_pair(texSampler,sampler)).first;
	return sit->second;
}


bool TexturesD3D11::SetTexture (ShaderType shaderType, int unit, int sampler, TextureID textureID, float bias)
{
	D3D11Texture* target = QueryD3DTexture(textureID);
	if (!target)
		return false;

	D3D11Texture& tex = *target;
	// Do not bind texture if it's currently being used as a render target
	if (g_D3D11CurrColorRT && g_D3D11CurrColorRT->m_Texture == tex.m_Texture && !tex.m_UAV)
		return false;

	if (bias != std::numeric_limits<float>::infinity())
		tex.m_Sampler.bias = bias;

	// set sampler state
	ID3D11SamplerState* smp = GetSampler(tex.m_Sampler);

	ID3D11DeviceContext* ctx = GetD3D11Context();
	switch (shaderType) {
	case kShaderVertex:
		ctx->VSSetShaderResources (unit, 1, &tex.m_SRV);
		if (sampler >= 0 && m_ActiveD3DSamplers[kShaderVertex][sampler] != smp)
		{
			ctx->VSSetSamplers (sampler, 1, &smp);
			m_ActiveD3DSamplers[kShaderVertex][sampler] = smp;
		}
		break;
	case kShaderFragment:
		ctx->PSSetShaderResources (unit, 1, &tex.m_SRV);
		if (sampler >= 0 && m_ActiveD3DSamplers[kShaderFragment][sampler] != smp)
		{
			ctx->PSSetSamplers (sampler, 1, &smp);
			m_ActiveD3DSamplers[kShaderFragment][sampler] = smp;
		}
		break;
	case kShaderGeometry:
		ctx->GSSetShaderResources (unit, 1, &tex.m_SRV);
		if (sampler >= 0 && m_ActiveD3DSamplers[kShaderGeometry][sampler] != smp)
		{
			ctx->GSSetSamplers (sampler, 1, &smp);
			m_ActiveD3DSamplers[kShaderGeometry][sampler] = smp;
		}
		break;
	case kShaderHull:
		ctx->HSSetShaderResources (unit, 1, &tex.m_SRV);
		if (sampler >= 0 && m_ActiveD3DSamplers[kShaderHull][sampler] != smp)
		{
			ctx->HSSetSamplers (sampler, 1, &smp);
			m_ActiveD3DSamplers[kShaderHull][sampler] = smp;
		}
		break;
	case kShaderDomain:
		ctx->DSSetShaderResources (unit, 1, &tex.m_SRV);
		if (sampler >= 0 && m_ActiveD3DSamplers[kShaderDomain][sampler] != smp)
		{
			ctx->DSSetSamplers (sampler, 1, &smp);
			m_ActiveD3DSamplers[kShaderDomain][sampler] = smp;
		}
		break;
	default: AssertString ("unknown shader type");
	}

	return true;
}

void TexturesD3D11::InvalidateSamplers()
{
	memset (m_ActiveD3DSamplers, 0, sizeof(m_ActiveD3DSamplers));
}


void TexturesD3D11::SetTextureParams( TextureID textureID, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace )
{
	D3D11Texture* target = QueryD3DTexture(textureID);
	if (!target)
		return;

	D3D11Sampler& s = target->m_Sampler;
	s.filter = filter;
	s.wrap = wrap;
	s.anisoLevel = anisoLevel;
	if (hasMipMap)
		s.flags |= kSamplerHasMipMap;
	else
		s.flags &= ~kSamplerHasMipMap;
}


void TexturesD3D11::DeleteTexture( TextureID textureID )
{
	D3D11Texture* target = QueryD3DTexture(textureID);
	if( !target )
		return;

	// texture can be null if texture creation failed. At least don't make it crash here
	if (target->m_Texture)
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(target->m_Texture);
		ULONG refCount = target->m_Texture->Release();
		DebugAssert(!refCount);
	}
	if (target->m_UAV)
	{
		ULONG refCount = target->m_UAV->Release();
		DebugAssert(!refCount);
	}
	if (target->m_SRV)
	{
		ULONG refCount = target->m_SRV->Release();
		DebugAssert(!refCount);
	}
	target->~D3D11Texture();
	_TextureAlloc->free(target);
	TextureIdMap::RemoveTexture(textureID);
}

void TexturesD3D11::ClearTextureResources()
{
	for (SamplerMap::iterator it = m_Samplers.begin(); it != m_Samplers.end(); ++it)
	{
		if (it->second)
			it->second->Release();
	}
	m_Samplers.clear();

	for (StagingTextureMap::iterator it = m_StagingTextures.begin(), itEnd = m_StagingTextures.end(); it != itEnd; ++it)
	{
		if (it->second)
			it->second->Release();
	}
	m_StagingTextures.clear();
}

