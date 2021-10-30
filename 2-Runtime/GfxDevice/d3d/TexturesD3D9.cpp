#include "UnityPrefix.h"
#include "TexturesD3D9.h"
#include "Runtime/Graphics/TextureFormat.h"
#include "Runtime/Graphics/Image.h"
#include "D3D9Context.h"
#include "Runtime/Allocator/FixedSizeAllocator.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Graphics/S3Decompression.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "D3D9Utils.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/VramLimits.h"
#include "Runtime/GfxDevice/TextureUploadUtils.h"
#include "Runtime/GfxDevice/TextureIdMap.h"
#include "External/ProphecySDK/include/prcore/Surface.hpp"
#include "Runtime/Profiler/MemoryProfiler.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"

struct D3DTexture
{
	explicit D3DTexture( IDirect3DBaseTexture9* tex )
		: texture(tex), wrapMode(D3DTADDRESS_CLAMP), minFilter(D3DTEXF_POINT), magFilter(D3DTEXF_POINT), mipFilter(D3DTEXF_NONE), aniso(1), sRGB(0) { }

	IDirect3DBaseTexture9*	texture;
	D3DTEXTUREADDRESS		wrapMode;
	D3DTEXTUREFILTERTYPE	minFilter;
	D3DTEXTUREFILTERTYPE	magFilter;
	D3DTEXTUREFILTERTYPE	mipFilter;
	int						aniso;
	bool					sRGB;
};

typedef FixedSizeAllocator<sizeof(D3DTexture)>	TextureAllocator;
static TextureAllocator* _TextureAlloc = NULL;

namespace TextureD3D9Alloc
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

static RegisterRuntimeInitializeAndCleanup s_TextureAllocManagerCallbacks(TextureD3D9Alloc::StaticInitialize, TextureD3D9Alloc::StaticDestroy);

static inline intptr_t AllocD3DTexture(IDirect3DBaseTexture9* tex)
{
	return (intptr_t)(new (_TextureAlloc->alloc()) D3DTexture(tex));
}

static inline D3DTexture* QueryD3DTexture(TextureID textureID)
{
	return (D3DTexture*)TextureIdMap::QueryNativeTexture(textureID);
}


static D3DCOLOR ColorToD3D( const float color[4] )
{
	return D3DCOLOR_RGBA( NormalizedToByte(color[0]), NormalizedToByte(color[1]), NormalizedToByte(color[2]), NormalizedToByte(color[3]) );
}


struct FormatDesc {
	TextureFormat		unityformat;
	D3DFORMAT			d3dformat;
	int					bpp;
	prcore::PixelFormat prformat;
};

const static FormatDesc kTextureFormatTable[kTexFormatPCCount+2] = // +1 for A8L8 case
{
	{ kTexFormatPCCount,	D3DFMT_UNKNOWN,	0, prcore::PixelFormat() },
	{ kTexFormatAlpha8,		D3DFMT_A8,		1, prcore::PixelFormat(8,0,0xff) }, // Alpha8
	{ kTexFormatARGB4444,	D3DFMT_A4R4G4B4,	2, prcore::PixelFormat(16,0x00000f00,0x000000f0,0x0000000f,0x0000f000) }, // ARGB4444
	{ kTexFormatRGB24,		D3DFMT_X8R8G8B8,	4, prcore::PixelFormat(32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000) }, // RGB24
	{ kTexFormatRGBA32,		D3DFMT_A8R8G8B8,	4, prcore::PixelFormat(32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000) }, // RGBA32
	{ kTexFormatARGB32,		D3DFMT_A8R8G8B8,	4, prcore::PixelFormat(32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000) }, // ARGB32
	{ kTexFormatARGBFloat,	D3DFMT_UNKNOWN,   0, prcore::PixelFormat() }, // ARGBFloat
	{ kTexFormatRGB565,		D3DFMT_R5G6B5,	2, prcore::PixelFormat(16,0x0000f800,0x000007e0,0x0000001f,0x00000000) }, // RGB565
	{ kTexFormatBGR24,		D3DFMT_X8R8G8B8,	4, prcore::PixelFormat(32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000) }, // BGR24
	{ kTexFormatAlphaLum16,	D3DFMT_L16,		0, prcore::PixelFormat() }, // AlphaLum16
	{ kTexFormatDXT1,		D3DFMT_DXT1,		0, prcore::PixelFormat() }, // DXT1
	{ kTexFormatDXT3,		D3DFMT_DXT3,		0, prcore::PixelFormat() }, // DXT3
	{ kTexFormatDXT5,		D3DFMT_DXT5,		0, prcore::PixelFormat() }, // DXT5
	{ kTexFormatRGBA4444,	D3DFMT_A4R4G4B4,	2, prcore::PixelFormat(16,0x00000f00,0x000000f0,0x0000000f,0x0000f000) }, // RGBA4444

	// following are not Unity formats, but might be used as fallbacks for some unsupported formats
	{ kTexFormatAlphaLum16, D3DFMT_A8L8,		2, prcore::PixelFormat(16,0x00ff,0xff00) }, // A8L8, used on cards that don't support A8; alpha -> alpha
};

const static FormatDesc kTextureFormatETC =
{
	kTexFormatETC_RGB4,		D3DFMT_X8R8G8B8,	4,	prcore::PixelFormat(32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000)
};

const static FormatDesc kTextureFormatATC[2] =
{
	{ kTexFormatATC_RGB4,		D3DFMT_X8R8G8B8,	4, prcore::PixelFormat(32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000) }, // RGB24
	{ kTexFormatATC_RGBA8,		D3DFMT_A8R8G8B8,	4, prcore::PixelFormat(32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000) }, // RGBA32
};


D3DFORMAT GetD3D9TextureFormat( TextureFormat inFormat )
{
	return kTextureFormatTable[inFormat].d3dformat;
}

static const FormatDesc& GetUploadFormat( TextureFormat inFormat, bool forceFallbackFormat = false )
{
	if (forceFallbackFormat)
	{
		return kTextureFormatTable[kTexFormatARGB32];
	}
	else if( inFormat == kTexFormatAlpha8 && !gGraphicsCaps.d3d.hasTextureFormatA8 )
	{
		// A8 not supported: A8L8 or fallback one depending on support
		if( gGraphicsCaps.d3d.hasTextureFormatA8L8 )
			return kTextureFormatTable[ kTexFormatPCCount ]; // return A8L8 option, see table above
		else
			return kTextureFormatTable[kTexFormatARGB32];
	}
	else if( IsCompressedDXTTextureFormat(inFormat) && !gGraphicsCaps.hasS3TCCompression )
	{
		// Compressed format not supported: decompress into fallback format
		return kTextureFormatTable[kTexFormatARGB32];
	}
	else if ( IsCompressedETCTextureFormat(inFormat) )
	{
		return kTextureFormatETC;
	}
	else if ( IsCompressedATCTextureFormat(inFormat) )
	{
		return kTextureFormatATC[ HasAlphaTextureFormat(inFormat)? 1 : 0 ];
	}
	else if (!gGraphicsCaps.d3d.hasBaseTextureFormat[inFormat])
	{
		// This format not supported in general: convert to fallback format
		return kTextureFormatTable[kTexFormatARGB32];
	}

	// All ok, return incoming format
	return kTextureFormatTable[inFormat];
}

intptr_t TexturesD3D9::RegisterNativeTexture(IDirect3DBaseTexture9* texture) const
{
	return AllocD3DTexture(texture);
}

void TexturesD3D9::UpdateNativeTexture(TextureID textureID, IDirect3DBaseTexture9* texture)
{
	D3DTexture* target = QueryD3DTexture(textureID);
	if(target)
		target->texture = texture;
	else
		AddTexture(textureID, texture);
}

void TexturesD3D9::AddTexture( TextureID textureID, IDirect3DBaseTexture9* texture )
{
	TextureIdMap::UpdateTexture(textureID, AllocD3DTexture(texture));
}

void TexturesD3D9::RemoveTexture( TextureID textureID )
{
	D3DTexture* target = QueryD3DTexture(textureID);
	if(target)
	{
		target->~D3DTexture();
		_TextureAlloc->free(target);
	}
	TextureIdMap::RemoveTexture(textureID);
}

IDirect3DBaseTexture9* TexturesD3D9::GetTexture( TextureID textureID ) const
{
	D3DTexture* target = QueryD3DTexture(textureID);
	return target ? target->texture : 0;
}



static void BlitAlphaLum16 (int width, int height, D3DFORMAT d3dFormat, const UInt8* srcData, UInt8* destData, int pitch)
{
	// Handle AlphaLum16 case. ProphecySDK does not support 16 bit/channel formats,
	// so we blit manually.
	UInt32 rowBytes = GetRowBytesFromWidthAndFormat(width,kTexFormatAlphaLum16);
	const UInt8* srcRowData = srcData;
	UInt8* destRowData = destData;
	if( d3dFormat == D3DFMT_L16 )
	{
		for( int r = 0; r < height; ++r )
		{
			memcpy( destRowData, srcRowData, rowBytes );
			srcRowData += rowBytes;
			destRowData += pitch;
		}
	}
	else if( d3dFormat == D3DFMT_L8 )
	{
		for( int r = 0; r < height; ++r )
		{
			for( int c = 0; c < width; ++c )
				destRowData[c] = srcRowData[c*2+1];
			srcRowData += rowBytes;
			destRowData += pitch;
		}
	}
	else
	{
		AssertIf( d3dFormat != D3DFMT_A8R8G8B8 );
		for( int r = 0; r < height; ++r )
		{
			for( int c = 0; c < width; ++c )
			{
				DWORD val = srcRowData[c*2+1];
				((D3DCOLOR*)destRowData)[c] = 0xFF000000 | (val<<16) | (val<<8) | (val);
			}
			srcRowData += rowBytes;
			destRowData += pitch;
		}
	}
}

void InitRGBA32Buffer(int width, int height, UInt8*& buffer, int& srcPitch, prcore::PixelFormat& pf)
{
	int imageSize = CalculateImageSize( width, height, kTexFormatRGBA32 );
	if( buffer == NULL )
		buffer = new UInt8[imageSize];
	srcPitch = GetRowBytesFromWidthAndFormat(width, kTexFormatRGBA32);
	pf = GetProphecyPixelFormat(kTexFormatRGBA32);
}

void TexturesD3D9::UploadTexture2D(
	TextureID tid, TextureDimension dimension, UInt8* srcData, int width, int height,
	TextureFormat format, int mipCount, UInt32 uploadFlags, int masterTextureLimit, TextureUsageMode usageMode, TextureColorSpace colorSpace )
{
	IDirect3DDevice9* dev = GetD3DDevice();

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

	bool uploadIsCompressed, decompressOnTheFly;
	HandleFormatDecompression (format, &usageMode, colorSpace, &uploadIsCompressed, &decompressOnTheFly);

	if( decompressOnTheFly )
		uploadIsCompressed = false;

	const FormatDesc& uploadFormat = GetUploadFormat (decompressOnTheFly ? kTexFormatRGBA32 : format, usageMode != kTexUsageNone);
	D3DFORMAT d3dFormat = uploadFormat.d3dformat;

	if( format == kTexFormatAlphaLum16 && !gGraphicsCaps.d3d.hasTextureFormatL16 )
	{
		// AlphaLum16 requires some trickery if hardware does not support L16:
		// first we try to do L8 instead, then fallback to A8R8G8B8.
		if( gGraphicsCaps.d3d.hasTextureFormatL8 )
			d3dFormat = D3DFMT_L8;
		else
			d3dFormat = D3DFMT_A8R8G8B8;
	}

	int baseLevel, maxLevel, texWidth, texHeight;
	size_t textureSize;
	prcore::Surface::BlitMode blitMode = prcore::Surface::BLIT_COPY;
	if (SkipLevelsForMasterTextureLimit (masterTextureLimit, format, uploadFormat.unityformat, mipCount, uploadIsCompressed, &srcData, &width, &height, &baseLevel, &maxLevel, &texWidth, &texHeight, &textureSize))
		blitMode = prcore::Surface::BLIT_SCALE;

	// if we don't support mip maps - don't use them
	if( !(gGraphicsCaps.d3d.d3dcaps.TextureCaps & D3DPTEXTURECAPS_MIPMAP) )
	{
		mipCount = 1;
		baseLevel = 0;
	}

	// create texture if it does not exist already
	IDirect3DTexture9* texture = NULL;

	D3DTexture* target = QueryD3DTexture(tid);
	if(!target)
	{
		HRESULT hr = dev->CreateTexture( texWidth, texHeight, mipCount - baseLevel, 0, d3dFormat, D3DPOOL_MANAGED, &texture, NULL );
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(texture, CalculateImageSize(texWidth, texHeight,format)*(mipCount>1?1.33:1),tid.m_ID);
		if( FAILED(hr) )
			printf_console( "d3d: failed to create 2D texture id=%i w=%i h=%i mips=%i d3dfmt=%i [%s]\n", tid, texWidth, texHeight, mipCount-baseLevel, d3dFormat, GetD3D9Error(hr) );
		TextureIdMap::UpdateTexture(tid, AllocD3DTexture(texture));
	}
	else
	{
		texture = (IDirect3DTexture9*)target->texture;
	}

	if( !texture )
	{
		AssertString( "failed to create 2D texture" );
		return;
	}

	UInt8* decompressBuffer = NULL;
	UInt8* tempBuffer = NULL;
	int bufferPitch;

	// Upload the mip levels
	for( int level = baseLevel; level <= maxLevel; ++level )
	{
		D3DLOCKED_RECT lr;
		HRESULT hr = texture->LockRect( level-baseLevel, &lr, NULL, 0 );
		if( FAILED(hr) )
		{
			printf_console( "d3d: failed to lock level %i of texture %i [%s]\n", level-baseLevel, tid, GetD3D9Error(hr) );
			if( decompressBuffer )
				delete[] decompressBuffer;
			return;
		}

		if( decompressOnTheFly )
		{
			ConvertCompressedTextureUpload (width, height, format, srcData, decompressBuffer, bufferPitch, usageMode, colorSpace, level);

			prcore::Surface srcSurface( width, height, bufferPitch, GetProphecyPixelFormat(kTexFormatRGBA32), decompressBuffer );
			prcore::Surface dstSurface( texWidth, texHeight, lr.Pitch, uploadFormat.prformat, lr.pBits );
			dstSurface.BlitImage( srcSurface, blitMode );
		}
		else if( format == kTexFormatAlphaLum16 )
		{
			BlitAlphaLum16( width, height, d3dFormat, srcData, (UInt8*)lr.pBits, lr.Pitch );
		}
		else if( !uploadIsCompressed )
		{
			prcore::Surface srcSurface( width, height, GetRowBytesFromWidthAndFormat( width,format ), GetProphecyPixelFormat(format), srcData );
			prcore::Surface dstSurface( texWidth, texHeight, lr.Pitch, uploadFormat.prformat, lr.pBits );

			if (!ConvertUncompressedTextureUpload(srcSurface, dstSurface, blitMode, uploadFormat.unityformat, usageMode, colorSpace, width, height, (UInt8*)lr.pBits, lr.Pitch, uploadFormat.prformat, tempBuffer, bufferPitch))
			{
				dstSurface.BlitImage( srcSurface, blitMode );
			}
		}
		else
		{
			if( width == texWidth && height == texHeight )
			{
				BlitCopyCompressedImage( format, srcData, width, height, (UInt8*)lr.pBits, width, height, false );
			}
			else
			{
				// TODO: fill with garbage?
			}
		}

		texture->UnlockRect( level-baseLevel );

		// Go to next level
		AssertIf( width == 1 && height == 1 && level != maxLevel );
		AdvanceToNextMipLevel (format, srcData, width, height, texWidth, texHeight);
	}

	delete[] decompressBuffer;
}

void TexturesD3D9::UploadTextureSubData2D(
	TextureID tid, UInt8* srcData, int mipLevel,
	int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace )
{
	IDirect3DDevice9* dev = GetD3DDevice();
	if( !dev )
		return;

	// if we don't support mip maps and want to change higher level - don't
	if( !(gGraphicsCaps.d3d.d3dcaps.TextureCaps & D3DPTEXTURECAPS_MIPMAP) && mipLevel != 0 )
		return;

	AssertIf( srcData == NULL );
	AssertIf( IsCompressedDXTTextureFormat( format ) );

	// find the texture
	D3DTexture* target = QueryD3DTexture(tid);
	if(target == 0)
	{
		AssertString( "Texture not found" );
		return;
	}

	const FormatDesc& uploadFormat = GetUploadFormat( format );
	IDirect3DTexture9* texture = (IDirect3DTexture9*)target->texture;
	AssertIf( !texture );

	RECT rect;
	rect.left = x;
	rect.top = y;
	rect.right = x + width;
	rect.bottom = y + height;
	D3DLOCKED_RECT lr;
	HRESULT hr = texture->LockRect( mipLevel, &lr, &rect, 0 );
	if( FAILED(hr) )
	{
		printf_console( "d3d: failed to lock sub level %i of texture %i [%s]\n", mipLevel, tid, GetD3D9Error(hr) );
		return;
	}

	// TODO: handle other format conversions

	prcore::Surface srcSurface( width, height, GetRowBytesFromWidthAndFormat(width,format), GetProphecyPixelFormat(format), srcData );
	prcore::Surface dstSurface( width, height, lr.Pitch, uploadFormat.prformat, lr.pBits );
	dstSurface.BlitImage( srcSurface, prcore::Surface::BLIT_COPY );

	texture->UnlockRect( mipLevel );
}


void TexturesD3D9::UploadTextureCube(
	TextureID tid, UInt8* srcData, int faceDataSize, int size,
	TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace )
{
	IDirect3DDevice9* dev = GetD3DDevice();
	if (!dev)
		return;

	// if we don't support cube mip maps - don't use them
	if( !(gGraphicsCaps.d3d.d3dcaps.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP) )
		mipCount = 1;

	const FormatDesc& uploadFormat = GetUploadFormat(format);
	IDirect3DCubeTexture9* texture = NULL;

	D3DTexture* target = QueryD3DTexture(tid);
	if(!target)
	{
		HRESULT hr = dev->CreateCubeTexture( size, mipCount, 0, uploadFormat.d3dformat, D3DPOOL_MANAGED, &texture, NULL );
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(texture, 6*CalculateImageSize(size, size, format)*(mipCount>1?1.33:1),tid.m_ID);
		if( FAILED(hr) )
			printf_console( "d3d: failed to create cubemap id=%i size=%i mips=%i d3dfmt=%i [%s]\n", tid, size, mipCount, uploadFormat.d3dformat, GetD3D9Error(hr) );
		TextureIdMap::UpdateTexture(tid, AllocD3DTexture(texture));
	}
	else
	{
		texture = (IDirect3DCubeTexture9*)target->texture;
	}
	if( !texture )
	{
		AssertString( "failed to create cubemap" );
		return;
	}

	// Upload data
	bool uploadIsCompressed = IsCompressedDXTTextureFormat(format); // TODO: handle when we don't have DXT

	static const D3DCUBEMAP_FACES faces[6] =
	{
		D3DCUBEMAP_FACE_POSITIVE_X,
		D3DCUBEMAP_FACE_NEGATIVE_X,
		D3DCUBEMAP_FACE_POSITIVE_Y,
		D3DCUBEMAP_FACE_NEGATIVE_Y,
		D3DCUBEMAP_FACE_POSITIVE_Z,
		D3DCUBEMAP_FACE_NEGATIVE_Z,
	};

	int maxLevel = mipCount - 1;
	for (int face=0;face<6;face++)
	{
		int mipSize = size;
		UInt8* data = srcData + face * faceDataSize;

		// Upload the mip levels
		for( int level = 0; level <= maxLevel; ++level )
		{
			D3DLOCKED_RECT lr;
			HRESULT hr = texture->LockRect( faces[face], level, &lr, NULL, 0 );
			if( FAILED(hr) )
			{
				printf_console( "d3d: failed to lock level %i of face %i of cubemap %i [%s]\n", level, face, tid, GetD3D9Error(hr) );
				return;
			}

			// TODO: handle DXT decompression on the fly
			// TODO: handle other format conversions

			if( !uploadIsCompressed )
			{
				prcore::Surface srcSurface( mipSize, mipSize, GetRowBytesFromWidthAndFormat(mipSize,format), GetProphecyPixelFormat(format), data );
				prcore::Surface dstSurface( mipSize, mipSize, lr.Pitch, uploadFormat.prformat, lr.pBits );
				dstSurface.BlitImage( srcSurface, prcore::Surface::BLIT_COPY );
			}
			else
			{
				BlitCopyCompressedImage( format, data, mipSize, mipSize, (UInt8*)lr.pBits, mipSize /* TODO */, mipSize, false );
			}

			texture->UnlockRect( faces[face], level );

			// Go to next level
			data += CalculateImageSize( mipSize, mipSize, format );
			AssertIf( mipSize == 1 && level != maxLevel );

			mipSize = std::max( mipSize / 2, 1 );
		}
	}
}

void TexturesD3D9::UploadTexture3D(
	TextureID tid, UInt8* srcData, int width, int height, int depth,
	TextureFormat format, int mipCount, UInt32 uploadFlags )
{
	IDirect3DDevice9* dev = GetD3DDevice();
	if (!dev || !gGraphicsCaps.has3DTexture)
		return;

	// if we don't support volume mip maps - don't use them
	if( !(gGraphicsCaps.d3d.d3dcaps.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP) )
		mipCount = 1;


	const FormatDesc& uploadFormat = GetUploadFormat( format );
	D3DFORMAT d3dFormat = uploadFormat.d3dformat;
	if( format == kTexFormatAlphaLum16 )
	{
		// AlphaLum16 requires some trickery if hardware does not support L16:
		// first we try to do L8 instead, then fallback to A8R8G8B8.
		if( !gGraphicsCaps.d3d.hasTextureFormatL16 && gGraphicsCaps.d3d.hasTextureFormatL8 )
			d3dFormat = D3DFMT_L8;
		else
			d3dFormat = D3DFMT_A8R8G8B8;
	}

	IDirect3DVolumeTexture9* texture = NULL;

	D3DTexture* target = QueryD3DTexture(tid);
	if(!target)
	{
		HRESULT hr = dev->CreateVolumeTexture( width, height, depth, mipCount, 0, d3dFormat, D3DPOOL_MANAGED, &texture, NULL );
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(texture, depth*CalculateImageSize(width, height, format)*(mipCount>1?1.33:1),tid.m_ID);
		if( FAILED(hr) )
			printf_console( "d3d: failed to create 3D texture id=%i w=%i h=%i d=%i mips=%i d3dfmt=%i [%s]\n", tid, width, height, depth, mipCount, d3dFormat, GetD3D9Error(hr) );
		TextureIdMap::UpdateTexture(tid, AllocD3DTexture(texture));
	}
	else
	{
		texture = (IDirect3DVolumeTexture9*)target->texture;
	}
	if( !texture )
	{
		AssertString( "failed to create 3D texture" );
		return;
	}

	int maxLevel = mipCount - 1;
	for( int level=0; level <= maxLevel; ++level )
	{
		D3DLOCKED_BOX lr;
		HRESULT hr = texture->LockBox( level, &lr, NULL, 0 );
		if( FAILED(hr) )
		{
			printf_console( "d3d: failed to lock level %i of 3D texture %i [%s]\n", level, tid, GetD3D9Error(hr) );
			return;
		}

		UInt8* destData = (UInt8*)lr.pBits;
		const int sliceSize = CalculateImageSize(width, height, format);
		for( int slice = 0; slice < depth; ++slice )
		{
			if( format == kTexFormatAlphaLum16 )
			{
				BlitAlphaLum16 (width, height, d3dFormat, srcData, destData, lr.RowPitch);
			}
			else
			{
				// Regular ProphecySDK blit
				prcore::Surface srcSurface( width, height, GetRowBytesFromWidthAndFormat(width,format), GetProphecyPixelFormat(format), srcData );
				prcore::Surface dstSurface( width, height, lr.RowPitch, uploadFormat.prformat, destData );
				dstSurface.BlitImage( srcSurface, prcore::Surface::BLIT_COPY );
			}
			srcData += sliceSize;
			destData += lr.SlicePitch;
		}

		texture->UnlockBox( level );

		AssertIf( width == 1 && height == 1 && level != maxLevel );

		width = std::max( width / 2, 1 );
		height = std::max( height / 2, 1 );
		depth = std::max( depth / 2, 1 );
	}
}



bool TexturesD3D9::SetTexture (ShaderType shaderType, int unit, TextureID textureID)
{
	IDirect3DDevice9* dev = GetD3DDevice();

	D3DTexture* target = QueryD3DTexture(textureID);
	if(target)
	{
		const D3DTexture& texture = *target;
		DWORD d3dUnit = GetD3D9SamplerIndex (shaderType, unit);
		D3D9_CALL(dev->SetTexture( d3dUnit, texture.texture ));
		// TODO: caching of those!
		D3D9_CALL(dev->SetSamplerState( d3dUnit, D3DSAMP_ADDRESSU, texture.wrapMode ));
		D3D9_CALL(dev->SetSamplerState( d3dUnit, D3DSAMP_ADDRESSV, texture.wrapMode ));
		D3D9_CALL(dev->SetSamplerState( d3dUnit, D3DSAMP_ADDRESSW, texture.wrapMode ));
		D3D9_CALL(dev->SetSamplerState( d3dUnit, D3DSAMP_MINFILTER, texture.minFilter ));
		D3D9_CALL(dev->SetSamplerState( d3dUnit, D3DSAMP_MAGFILTER, texture.magFilter ));
		D3D9_CALL(dev->SetSamplerState( d3dUnit, D3DSAMP_MIPFILTER, texture.mipFilter ));
		D3D9_CALL(dev->SetSamplerState( d3dUnit, D3DSAMP_MAXANISOTROPY, texture.aniso ));
		D3D9_CALL(dev->SetSamplerState( d3dUnit, D3DSAMP_SRGBTEXTURE, texture.sRGB ));
		return true;
	}
	else
	{
		// Ok, just don't complain here. Mostly with render textures, once in a while it
		// happens that RT is not created yet, and someone tries to render with it.
		// Just silently ignore that case.
		//ErrorString( Format("SetTexture with unknown texture %i", textureID) );
		return false;
	}
}

static D3DTEXTUREADDRESS s_D3DWrapModes[kTexWrapCount] = {
	D3DTADDRESS_WRAP,
	D3DTADDRESS_CLAMP,
};
static D3DTEXTUREFILTERTYPE s_D3DMinMagFilters[kTexFilterCount] = {
	D3DTEXF_POINT,
	D3DTEXF_LINEAR,
	D3DTEXF_LINEAR,
};
static D3DTEXTUREFILTERTYPE s_D3DMipFilters[kTexFilterCount] = {
	D3DTEXF_POINT,
	D3DTEXF_POINT,
	D3DTEXF_LINEAR,
};


void TexturesD3D9::SetTextureParams( TextureID textureID, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace )
{
	D3DTexture* target = QueryD3DTexture(textureID);
	if(!target)
		return;

	D3DTexture& texture = *target;
	AssertIf( !texture.texture );

	if( gGraphicsCaps.hasAnisoFilter && texDim != kTexDim3D )
		texture.aniso = std::min( anisoLevel, gGraphicsCaps.maxAnisoLevel );
	else
		texture.aniso = 1;
	texture.wrapMode = s_D3DWrapModes[wrap];

	if( !hasMipMap && filter == kTexFilterTrilinear )
		filter = kTexFilterBilinear;

	texture.minFilter = texture.magFilter = s_D3DMinMagFilters[filter];
	if( texture.aniso > 1 )
	{
		texture.minFilter = D3DTEXF_ANISOTROPIC;
		// some cards (notably GeForces) can do min anisotropic filter, but not mag anisotropic filter
		if( gGraphicsCaps.d3d.d3dcaps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC )
			texture.magFilter = D3DTEXF_ANISOTROPIC;
	}
	texture.mipFilter = s_D3DMipFilters[filter];

	//sRGB
	texture.sRGB = colorSpace == kTexColorSpaceSRGB || colorSpace == kTexColorSpaceSRGBXenon;
	// actual setting of sampler states will happen in SetTexture
}


void TexturesD3D9::DeleteTexture( TextureID textureID )
{
	D3DTexture* target = QueryD3DTexture(textureID);
	if(!target)
		return;

	// texture can be null if texture creation failed. At least don't make it crash here
	if( target->texture )
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(target->texture);
		ULONG refCount = target->texture->Release();
		AssertIf( refCount != 0 );
	}
	TextureIdMap::RemoveTexture(textureID);
}
