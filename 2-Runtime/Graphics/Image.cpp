#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "Image.h"
#include "S3Decompression.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/vector_utility.h"
#include "Runtime/Utilities/BitUtility.h"
#include "External/ProphecySDK/include/prcore/surface.hpp"

#include "Runtime/Math/Color.h"
#include "Runtime/Math/ColorSpaceConversion.h"

using namespace std;

prcore::PixelFormat GetProphecyPixelFormat (TextureFormat format);

void BlitCopyCompressedImage( TextureFormat format, const UInt8* src, int srcWidth, int srcHeight, UInt8* dst, int dstWidth, int dstHeight, bool fillRest )
{
	AssertIf( dstWidth < srcWidth );
	AssertIf( dstHeight < srcHeight );

	int blockBytes, srcBlocksX, srcBlocksY, dstBlocksX, dstBlocksY;

	{
		AssertIf( !IsCompressedDXTTextureFormat(format) && !IsCompressedETCTextureFormat(format)
					&& !IsCompressedATCTextureFormat(format) && !IsCompressedETC2TextureFormat(format)
					&& !IsCompressedEACTextureFormat(format));

		blockBytes = (format == kTexFormatDXT1 || format == kTexFormatETC_RGB4 || format == kTexFormatATC_RGB4
						|| format == kTexFormatETC2_RGB || format == kTexFormatETC2_RGBA1
						|| format == kTexFormatEAC_R || format == kTexFormatEAC_R_SIGNED)
					? 8 : 16;

		srcBlocksX = (srcWidth + 3) / 4;
		srcBlocksY = (srcHeight + 3) / 4;
		dstBlocksX = (dstWidth + 3) / 4;
		dstBlocksY = (dstHeight + 3) / 4;
	}

	int srcRowBytes = srcBlocksX * blockBytes;
	int dstRowBytes = dstBlocksX * blockBytes;

	const UInt8* srcPtr = src;
	UInt8* dstPtr = dst;
	int y;
	for( y = 0; y < srcBlocksY; ++y )
	{
		memcpy( dstPtr, srcPtr, srcRowBytes ); // copy DXT blocks
		if( fillRest )
			memset( dstPtr + srcRowBytes, 0, dstRowBytes - srcRowBytes ); // fill rest with black/transparent
		srcPtr += srcRowBytes;
		dstPtr += dstRowBytes;
	}
	if( fillRest )
		memset( dstPtr, 0, dstRowBytes * (dstBlocksY-srcBlocksY) ); // fill rest with black/transparent
}

void BlitCopyCompressedDXT1ToDXT5( const UInt8* src, int srcWidth, int srcHeight, UInt8* dst, int dstWidth, int dstHeight )
{
	AssertIf( dstWidth < srcWidth );
	AssertIf( dstHeight < srcHeight );

	int blockBytesSrc = 8;
	int blockBytesDst = 16;

	int srcBlocksX = (srcWidth + 3) / 4;
	int srcBlocksY = (srcHeight + 3) / 4;
	int dstBlocksX = (dstWidth + 3) / 4;

	int srcRowBytes = srcBlocksX * blockBytesSrc;
	int dstRowBytes = dstBlocksX * blockBytesDst;

	const UInt8* srcPtr = src;
	UInt8* dstPtr = dst;
	int y;
	for( y = 0; y < srcBlocksY; ++y )
	{
		for( int x = 0; x < srcBlocksX; ++x )
		{
			// set alpha to opaque white
			memset( dstPtr + x * blockBytesDst, 0xFF, blockBytesDst-blockBytesSrc );
			// copy block
			memcpy( dstPtr + x * blockBytesDst + blockBytesSrc, srcPtr + x * blockBytesSrc, blockBytesSrc );
		}
		srcPtr += srcRowBytes;
		dstPtr += dstRowBytes;
	}
}


void PadImageBorder( ImageReference& image, int sizeX, int sizeY )
{
	AssertIf( IsAnyCompressedTextureFormat(image.GetFormat()) );

	int width = image.GetWidth();
	int height = image.GetHeight();
	AssertIf( sizeX < 1 || sizeY < 1 || sizeX > width || sizeY > height );

	int y;
	UInt8* rowptr = image.GetImageData();

	int bpp = GetBytesFromTextureFormat(image.GetFormat());
	AssertIf(bpp > 4); // we only support up to 4 bpp
	UInt8 pixel[4];

	// pad rightmost part by repeating last pixel in each row
	for( y = 0; y < sizeY; ++y )
	{
		UInt8* ptr = rowptr + (sizeX-1) * bpp;

		for( int b = 0; b < bpp; ++b )
			pixel[b] = ptr[b];
		ptr += bpp;
		for( int x = sizeX; x < width; ++x )
		{
			for( int b = 0; b < bpp; ++b )
				ptr[b] = pixel[b];
			ptr += bpp;
		}

		rowptr += image.GetRowBytes();
	}

	// pad bottom part by repeating last row
	UInt8* lastRowPtr = image.GetRowPtr( sizeY - 1 );
	for( int b = 0; b < bpp; ++b )
		pixel[b] = lastRowPtr[ (sizeX-1)*bpp + b ];

	for( y = sizeY; y < height; ++y )
	{
		UInt8* ptr = rowptr;
		memcpy( ptr, lastRowPtr, sizeX * bpp ); // copy
		ptr += sizeX * bpp;
		for( int x = sizeX; x < width; ++x ) // repeat last pixel
		{
			for( int b = 0; b < bpp; ++b )
				ptr[b] = pixel[b];
			ptr += bpp;
		}

		rowptr += image.GetRowBytes();
	}
}


int CalculateMipMapCount3D (int width, int height, int depth)
{
	//AssertIf( !IsPowerOfTwo(width) || !IsPowerOfTwo(height) || !IsPowerOfTwo(depth) );

	// Mip-levels for non-power-of-two textures follow OpenGL's NPOT texture rules: size is divided
	// by two and floor'ed. This allows just to use same old code I think.

	int minSizeLog2 = HighestBit (width);
	minSizeLog2 = max (minSizeLog2, HighestBit (height));
	minSizeLog2 = max (minSizeLog2, HighestBit (depth));

	AssertIf( (width >> minSizeLog2) < 1 && (height >> minSizeLog2) < 1 && (depth >> minSizeLog2) < 1 );
	AssertIf( (width >> minSizeLog2) > 1 && (height >> minSizeLog2) > 1 && (depth >> minSizeLog2) < 1 );

	return minSizeLog2 + 1;
}

int CalculateImageSize (int width, int height, TextureFormat format)
{
	if (format == kTexFormatDXT1 || format == kTexFormatATC_RGB4)
		return ((width + 3) / 4) * ((height + 3) / 4) * 8;
	else if (format == kTexFormatDXT3 || format == kTexFormatDXT5 || format == kTexFormatATC_RGBA8)
		return ((width + 3) / 4) * ((height + 3) / 4) * 16;
	else if (format == kTexFormatPVRTC_RGB4 || format == kTexFormatPVRTC_RGBA4)
		return (max(width, 8) * max(height, 8) * 4 + 7) / 8;
	else if (format == kTexFormatPVRTC_RGB2 || format == kTexFormatPVRTC_RGBA2)
		return (max(width, 16) * max(height, 8) * 2 + 7) / 8;
	else if (format == kTexFormatETC_RGB4 || format == kTexFormatETC2_RGB || format == kTexFormatETC2_RGBA1
				|| format == kTexFormatEAC_R || format == kTexFormatEAC_R_SIGNED)
		return (max(width, 4) * max(height, 4) * 4 + 7) / 8;
	else if (format == kTexFormatETC2_RGBA8 || format == kTexFormatEAC_RG || format == kTexFormatEAC_RG_SIGNED)
		return (max(width, 4) * max(height, 4) * 8 + 7) / 8;

#define STR_(x) #x
#define STR(x) STR_(x)
	// The size of the ASTC block is always 128 bits = 16 bytes, width of image in blocks is ceiling(width/blockwidth)
#define DO_ASTC(bx, by) else if( format == kTexFormatASTC_RGB_##bx##x##by || format == kTexFormatASTC_RGBA_##bx##x##by)\
		return ( (CeilfToInt(((float)width) / ((float)bx))) * (CeilfToInt(((float)height) / ((float)by))) * 16 )

	DO_ASTC(4, 4);
	DO_ASTC(5, 5);
	DO_ASTC(6, 6);
	DO_ASTC(8, 8);
	DO_ASTC(10, 10);
	DO_ASTC(12, 12);

#undef DO_ASTC
#undef STR_
#undef STR

	// ATF Format is a container format and has no known size based on width & height
	else if (format == kTexFormatFlashATF_RGB_DXT1 || format == kTexFormatFlashATF_RGBA_JPG || format == kTexFormatFlashATF_RGB_JPG)
		return 0;
	else
		return GetRowBytesFromWidthAndFormat (width, format) * height;
}

int CalculateMipMapOffset (int width, int height, TextureFormat format, int miplevel)
{
	if (width == 0 || height == 0)
		return 0;

	// Allow NPOT textures as well.
	//AssertIf (!IsPowerOfTwo (width) || !IsPowerOfTwo (height));
	int completeSize = 0;
	for (int i=0;i < miplevel;i++)
		completeSize += CalculateImageSize (std::max (width >> i, 1), std::max (height >> i, 1), format);
	return completeSize;
}

int CalculateImageMipMapSize (int width, int height, TextureFormat format)
{
	return CalculateMipMapOffset( width, height, format, CalculateMipMapCount3D(width, height, 1) );
}



void CreateMipMap (UInt8* inData, int width, int height, int depth, TextureFormat format)
{
	const int bpp = GetBytesFromTextureFormat (format);
	int mipCount = CalculateMipMapCount3D (width, height, depth) - 1;
	UInt8* srcPtr = inData;

	UInt8* tempBuffer = NULL;

	for (int mip = 0; mip < mipCount; ++mip)
	{
		int nextWidth = std::max(width/2,1);
		int nextHeight = std::max(height/2,1);
		int nextDepth = std::max(depth/2,1);

		UInt8* nextMipPtr = srcPtr + width * height * depth * bpp;
		UInt8* dstPtr = nextMipPtr;
		if (depth > 1)
		{
			if (!tempBuffer)
				tempBuffer = ALLOC_TEMP_MANUAL(UInt8, nextWidth*nextHeight*bpp+bpp);

			for (int d = 0; d < nextDepth; ++d)
			{
				// blit two slices of this mip level into two 2x2 smaller images
				ImageReference src1 (width, height, width * bpp, format, srcPtr);
				srcPtr += width * height * bpp;
				ImageReference src2 (width, height, width * bpp, format, srcPtr);
				srcPtr += width * height * bpp;

				ImageReference dst1 (nextWidth, nextHeight, nextWidth * bpp, format, dstPtr); // one directly into dst
				dst1.BlitImage (src1, ImageReference::BLIT_BILINEAR_SCALE);
				ImageReference dst2 (nextWidth, nextHeight, nextWidth * bpp, format, tempBuffer); // one into temp buffer
				dst2.BlitImage (src2, ImageReference::BLIT_BILINEAR_SCALE);

				// now average the two smaller images
				for (int i = 0; i < nextWidth*nextHeight*bpp; ++i)
				{
					unsigned b1 = dstPtr[i];
					unsigned b2 = tempBuffer[i];
					dstPtr[i] = (b1 + b2) / 2;
				}

				dstPtr += nextWidth * nextHeight * bpp;
			}
		}
		else
		{
			ImageReference src (width, height, width * bpp, format, srcPtr);
			ImageReference dst (nextWidth, nextHeight, nextWidth * bpp, format, dstPtr);
			dst.BlitImage (src, ImageReference::BLIT_BILINEAR_SCALE);
		}

		srcPtr = nextMipPtr;
		width = nextWidth;
		height = nextHeight;
		depth = nextDepth;
	}

	FREE_TEMP_MANUAL(tempBuffer);
}



ImageReference::ImageReference (int width, int height, int rowbytes, TextureFormat format, void* image)
{
	m_Width = width;
	m_Height = height;
	m_Format = format;
	m_RowBytes = rowbytes;

	if (image && CheckImageFormatValid (width, height, format))
		m_Image = reinterpret_cast<UInt8*> (image);
	else
		m_Image = NULL;
}

ImageReference::ImageReference (int width, int height, TextureFormat format)
{
	m_Width = width;
	m_Height = height;
	m_Format = format;
	m_RowBytes = GetRowBytesFromWidthAndFormat (m_Width, format);
	m_Image = NULL;
}

void ImageReference::FlipImageY ()
{
	if (m_Image)
	{
		prcore::Surface srcSurface (GetWidth (), GetHeight (), GetRowBytes (), GetProphecyPixelFormat (GetFormat ()), GetImageData ());
		srcSurface.FlipImageY ();
	}
}

#if UNITY_EDITOR
static inline void SwapPixels( UInt8* a, UInt8* b, int bpp ) {
	for( int i = 0; i < bpp; ++i ) {
		UInt8 t = a[i];
		a[i] = b[i];
		b[i] = t;
	}
}

void ImageReference::FlipImageX ()
{
	if (!m_Image)
		return;

	const int width = GetWidth();
	const int height = GetHeight();
	const TextureFormat format = GetFormat();
	if( IsAnyCompressedTextureFormat(format) ) {
		AssertString("FlipImageX does not work on compressed formats");
		return;
	}
	const int bpp = GetBytesFromTextureFormat(format);
	for( int y = 0; y < height; ++y ) {
		UInt8* rowFirst = GetRowPtr(y);
		UInt8* rowLast = rowFirst + (width-1) * bpp;
		for( int x = 0; x < width/2; ++x ) {
			SwapPixels( rowFirst, rowLast, bpp );
			rowFirst += bpp;
			rowLast -= bpp;
		}
	}
}
#endif


void ImageReference::BlitImage (const ImageReference& source, BlitMode mode)
{
	if (m_Image && source.m_Image)
	{
		prcore::Surface srcSurface (source.GetWidth (), source.GetHeight (), source.GetRowBytes (), GetProphecyPixelFormat (source.GetFormat ()), source.GetImageData ());
		prcore::Surface dstSurface (GetWidth (), GetHeight (), GetRowBytes (), GetProphecyPixelFormat (GetFormat ()), GetImageData ());
		dstSurface.BlitImage (srcSurface, (prcore::Surface::BlitMode)mode);
	}
}

void ImageReference::BlitImage (int x, int y, const ImageReference& source)
{
	if (source.m_Image && m_Image)
	{
		prcore::Surface srcSurface (source.GetWidth (), source.GetHeight (), source.GetRowBytes (), GetProphecyPixelFormat (source.GetFormat ()), source.GetImageData ());
		prcore::Surface dstSurface (GetWidth (), GetHeight (), GetRowBytes (), GetProphecyPixelFormat (GetFormat ()), GetImageData ());
		dstSurface.BlitImage (x, y, srcSurface);
	}
}

void ImageReference::ClearImage (const ColorRGBA32& color, ClearMode mode)
{
	if (m_Image)
	{
		prcore::color32 c;
		c.r = color.r;
		c.g = color.g;
		c.b = color.b;
		c.a = color.a;
		prcore::Surface dstSurface (GetWidth (), GetHeight (), GetRowBytes (), GetProphecyPixelFormat (GetFormat ()), GetImageData ());
		dstSurface.ClearImage (c, (prcore::Surface::ClearMode)mode);
	}
}

ImageReference ImageReference::ClipImage (int x, int y, int width, int height) const
{
	if (m_Image)
	{
		x = clamp<int> (x, 0, m_Width);
		y = clamp<int> (y, 0, m_Height);
		width = min<int> (m_Width, x + width) - x;
		height = min<int> (m_Height, y + height) - y;
		width = max<int> (0, width);
		height = max<int> (0, height);

		int bytesPerPixel = GetBytesFromTextureFormat( m_Format );
		return ImageReference( width, height, m_RowBytes, m_Format, m_Image + bytesPerPixel * x + m_RowBytes * y );
	}
	else
		return ImageReference( 0, 0, 0, m_Format, NULL );
}

bool ImageReference::NeedsReformat(int width, int height, TextureFormat format) const
{
	// TODO : it actually doesn't need to reallocate when downsizing!
	return width != m_Width || height != m_Height || format != m_Format;
}

Image::Image (int width, int height, TextureFormat format)
{
	m_Height = height;
	m_Width = width;
	m_Format = format;
	int bpp = GetBytesFromTextureFormat( m_Format );
	m_RowBytes = m_Width * bpp;
	if (CheckImageFormatValid (width, height, format))
		m_Image = (UInt8*)UNITY_MALLOC_ALIGNED(kMemNewDelete, m_RowBytes * m_Height + GetMaxBytesPerPixel( m_Format ), kImageDataAlignment); // allocate one pixel more for bilinear blits
	else
		m_Image = NULL;
}

Image::Image (int width, int height, int rowbytes, TextureFormat format, void* image)
{
	m_Height = height;
	m_Width = width;
	m_Format = format;
	int bpp = GetBytesFromTextureFormat( m_Format );
	m_RowBytes = m_Width * bpp;
	if( CheckImageFormatValid (width, height, format) )
		m_Image = (UInt8*)UNITY_MALLOC_ALIGNED(kMemNewDelete, m_RowBytes * m_Height + GetMaxBytesPerPixel( m_Format ), kImageDataAlignment); // allocate one pixel more for bilinear blits
	else
		m_Image = NULL;

	if (image && m_Image)
		BlitImage (ImageReference (width, height, rowbytes, format, image), BLIT_COPY);
}

void Image::SetImage(SInt32 width, SInt32 height, UInt32 format, bool shrinkAllowed)
{
	// TODO : this size is wrong if it has been shrunk already, but we don't care at the moment,
	// because we use it for calculation of mipmaps only
	const int oldSize = m_RowBytes * m_Height + GetBytesFromTextureFormat( m_Format );

	m_Width = width;
	m_Height = height;
	m_Format = format;
	const int bpp = GetBytesFromTextureFormat( m_Format );
	m_RowBytes = m_Width * bpp;

	const int newSize = m_RowBytes * m_Height + bpp;

	if ((!shrinkAllowed && (oldSize < newSize)) ||
		 (shrinkAllowed && (oldSize != newSize)))
	{
		UNITY_FREE(kMemNewDelete, m_Image);
		m_Image = NULL;
		if( m_Format != 0 && CheckImageFormatValid (m_Width, m_Height, m_Format) )
			m_Image = (UInt8*)UNITY_MALLOC_ALIGNED(kMemNewDelete, m_RowBytes * m_Height + GetMaxBytesPerPixel( m_Format ), kImageDataAlignment); // allocate one pixel more for bilinear blits
	}
}

void Image::SetImage(const ImageReference& src, bool shrinkAllowed)
{
	if (this == &src)
		return;

	SetImage (src.GetWidth(), src.GetHeight(), src.GetFormat(), shrinkAllowed);
	BlitImage (src, BLIT_COPY);
}

void Image::ReformatImage (const ImageReference& image, int width, int height, TextureFormat format, BlitMode mode)
{
	AssertIf(!image.NeedsReformat(width, height, format));

	int bpp = GetBytesFromTextureFormat(format);
	int newRowBytes = width * bpp;
	UInt8* newImageData = NULL;
	if (CheckImageFormatValid (width, height, format))
		newImageData = (UInt8*)UNITY_MALLOC_ALIGNED(kMemNewDelete, height * newRowBytes + GetMaxBytesPerPixel( m_Format ), kImageDataAlignment); // allocate one pixel more for bilinear blits

	ImageReference newImage (width, height, newRowBytes, format, newImageData);
	newImage.BlitImage (image, mode);
	UNITY_FREE(kMemNewDelete, m_Image);

	m_Height = height;
	m_Width = width;
	m_Format = format;
	m_RowBytes = newRowBytes;
	m_Image = newImageData;
}

void Image::ReformatImage (int width, int height, TextureFormat format, BlitMode mode)
{
	if (!NeedsReformat(width, height, format))
		return;

	ReformatImage (*this, width, height, format, mode);
}

bool ImageReference::IsValidImage () const
{
	return m_Image != NULL && CheckImageFormatValid (GetWidth(), GetHeight(), GetFormat());
}

bool CheckImageFormatValid (int width, int height, TextureFormat format)
{
	if (width > 0 && height > 0 && format > 0 && (format <= kTexFormatBGR24 || format == kTexFormatBGRA32 || format == kTexFormatRGBA4444))
		return true;
	else
	{
		if (width < 0) {
			AssertString ("Image invalid width!");
		}
		if (height < 0) {
			AssertString ("Image invalid height!");
		}
		if (format > kTexFormatBGR24 && format != kTexFormatRGBA4444 && format != kTexFormatBGRA32) {
			AssertString ("Image invalid format!");
		}
		return false;
	}
}

prcore::PixelFormat GetProphecyPixelFormat (TextureFormat format)
{
	switch (format)
	{
		case kTexFormatAlpha8:
			return prcore::PixelFormat (8,0,0xff);
		case kTexFormatARGB4444:
			return prcore::PixelFormat (16,0x00000f00,0x000000f0,0x0000000f,0x0000f000);
		case kTexFormatRGBA4444:
			return prcore::PixelFormat (16,0x0000f000,0x00000f00,0x000000f0,0x0000000f);
		case kTexFormatRGB24:
			#if UNITY_BIG_ENDIAN
			return prcore::PixelFormat (24,0x00ff0000,0x0000ff00,0x000000ff,0x00000000);
			#else
			return prcore::PixelFormat (24,0x000000ff,0x0000ff00,0x00ff0000,0x00000000);
			#endif
		case kTexFormatRGBA32:
			#if UNITY_BIG_ENDIAN
			return prcore::PixelFormat (32,0xff000000,0x00ff0000,0x0000ff00,0x000000ff);
			#else
			return prcore::PixelFormat (32,0x000000ff,0x0000ff00,0x00ff0000,0xff000000);
			#endif
		case kTexFormatBGRA32:
			#if UNITY_BIG_ENDIAN
			return prcore::PixelFormat (32,0x0000ff00,0x00ff0000,0xff000000,0x000000ff);
			#else
			return prcore::PixelFormat (32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000);
			#endif
		case kTexFormatARGB32:
			#if UNITY_BIG_ENDIAN
			return prcore::PixelFormat (32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000);
			#else
			return prcore::PixelFormat (32,0x0000ff00,0x00ff0000,0xff000000,0x000000ff);
			#endif
		case kTexFormatARGBFloat:
			return prcore::PixelFormat (128, 2, 4, 8, 1, prcore::PIXELFORMAT_FLOAT32);
		case kTexFormatRGB565:
			return prcore::PixelFormat (16,0x0000f800,0x000007e0,0x0000001f,0x00000000);
		case kTexFormatBGR24:
#if UNITY_BIG_ENDIAN
			return prcore::PixelFormat (24,0x00ff0000,0x0000ff00,0x000000ff,0x00000000);
#else
			return prcore::PixelFormat (24,0x000000ff,0x0000ff00,0x00ff0000,0x00000000);
#endif
		default:
			DebugAssertIf( true );
			return prcore::PixelFormat ();
	}
}

bool operator == (const ImageReference& lhs, const ImageReference& rhs)
{
	if (lhs.GetWidth () != rhs.GetWidth ()) return false;
	if (lhs.GetHeight () != rhs.GetHeight ()) return false;
	if (lhs.GetRowBytes () != rhs.GetRowBytes ()) return false;
	if (lhs.GetFormat () != rhs.GetFormat ()) return false;
	UInt8* lhsData = lhs.GetImageData ();
	UInt8* rhsData = rhs.GetImageData ();
	if (lhsData == NULL || rhsData == NULL)
		return lhsData == rhsData;

	int size = lhs.GetRowBytes () * lhs.GetHeight ();
	for (int i=0;i<size / 4;i++)
	{
		if (*reinterpret_cast<UInt32*> (lhsData) != *reinterpret_cast<UInt32*> (rhsData))
			return false;
		lhsData += 4; rhsData += 4;
	}
	for (int i=0;i<size % 4;i++)
	{
		if (*lhsData != *rhsData)
			return false;
		lhsData++; rhsData++;
	}
	return true;
}

void SwizzleARGB32ToBGRA32 (UInt8* bytes, int imageSize)
{
	for (int i=0;i<imageSize;i+=4)
	{
		UInt8 swizzle[4];
		UInt8* dst = bytes + i;
		memcpy(swizzle, dst, sizeof(swizzle));
		
		dst[0] = swizzle[3];
		dst[1] = swizzle[2];
		dst[2] = swizzle[1];
		dst[3] = swizzle[0];
	}
}

void SwizzleRGBA32ToBGRA32 (UInt8* bytes, int imageSize)
{
	for (int i=0;i<imageSize;i+=4)
	{
		UInt8 swizzle[4];
		UInt8* dst = bytes + i;
		memcpy(swizzle, dst, sizeof(swizzle));

		dst[0] = swizzle[2];
		dst[1] = swizzle[1];
		dst[2] = swizzle[0];
		dst[3] = swizzle[3];
	}
}

void SwizzleBGRAToRGBA32 (UInt8* bytes, int imageSize)
{
	for (int i=0;i<imageSize;i+=4)
	{
		UInt8 swizzle[4];
		UInt8* dst = bytes + i;
		memcpy(swizzle, dst, sizeof(swizzle));
		
		dst[0] = swizzle[2];
		dst[1] = swizzle[1];
		dst[2] = swizzle[0];
		dst[3] = swizzle[3];
	}
}

void SwizzleRGB24ToBGR24 (UInt8* bytes, int imageSize)
{
	for (int i=0;i<imageSize;i+=3)
	{
		UInt8 swizzle[3];
		UInt8* dst = bytes + i;
		memcpy(swizzle, dst, sizeof(swizzle));
		
		dst[0] = swizzle[2];
		dst[1] = swizzle[1];
		dst[2] = swizzle[0];
	}
}

void Premultiply( ImageReference& image )
{
	if (image.GetFormat() == kTexFormatRGBA32)
	{
		UInt8* data = image.GetImageData();
		int size = image.GetRowBytes() * image.GetHeight() / 4;
		for (int i=0;i<size;i++)
		{
			UInt8* rgba = data + 4 * i;
			rgba[0] = ((int)rgba[0] * (int)rgba[3]) / 255;
			rgba[1] = ((int)rgba[1] * (int)rgba[3]) / 255;
			rgba[2] = ((int)rgba[2] * (int)rgba[3]) / 255;
		}
	}
	else if (image.GetFormat() == kTexFormatARGB32)
	{
		UInt8* data = image.GetImageData();
		int size = image.GetRowBytes() * image.GetHeight() / 4;
		for (int i=0;i<size;i++)
		{
			UInt8* rgba = data + 4 * i;
			rgba[1] = ((int)rgba[1] * (int)rgba[0]) / 255;
			rgba[2] = ((int)rgba[2] * (int)rgba[0]) / 255;
			rgba[3] = ((int)rgba[3] * (int)rgba[0]) / 255;
		}
	}
	else
	{
		ErrorString("Unsupported");
	}
}

inline UInt32 DecodeRGBMChannel(UInt32 val, UInt32 mult, UInt32 mask, UInt8 offset)
{
	UInt32 channel = (val & mask) >> offset;
	channel *= mult;
	channel /= 255 * 2;
	channel = channel > 255 ? 255 : channel;
	channel <<= offset;

	return channel;
}

// Decodes RGBM encoded lightmaps into doubleLDR in place.
// Handles kTexFormatRGBA32 and kTexFormatARGB32 data.
void DecodeRGBM(int width, int height, UInt8* data, int pitch, const prcore::PixelFormat& pf)
{
	UInt8* rowData = data;
	uint32 rmask = pf.GetRedMask();
	uint32 gmask = pf.GetGreenMask();
	uint32 bmask = pf.GetBlueMask();
	uint32 amask = pf.GetAlphaMask();
	uint8 roffset = (int)pf.GetRedOffset() - (int)pf.GetRedBits() + 1;
	uint8 goffset = (int)pf.GetGreenOffset() - (int)pf.GetGreenBits() + 1;
	uint8 boffset = (int)pf.GetBlueOffset() - (int)pf.GetBlueBits() + 1;
	uint8 aoffset = (int)pf.GetAlphaOffset() - (int)pf.GetAlphaBits() + 1;

	for( int r = 0; r < height; ++r )
	{
		uint32* pixel = (uint32*)rowData;
		for( int c = 0; c < width; ++c )
		{
			uint32 val = pixel[c];
			uint32 alpha = ((val & amask) >> aoffset) * kRGBMMaxRange;
			pixel[c] =	DecodeRGBMChannel(val, alpha, rmask, roffset) +
						DecodeRGBMChannel(val, alpha, gmask, goffset) +
						DecodeRGBMChannel(val, alpha, bmask, boffset) +
						(255 << aoffset);
		}
		rowData += pitch;
	}
}


void SetAlphaChannel(int width, int height, UInt8* data, int pitch, const prcore::PixelFormat& pf, UInt8 alpha)
{
	UInt8* rowData = data;
	uint32 amask = pf.GetAlphaMask();
	uint8 aoffset = (int)pf.GetAlphaOffset() - (int)pf.GetAlphaBits() + 1;
	uint32 properAlpha = (alpha << aoffset);

	for( int r = 0; r < height; ++r )
	{
		uint32* pixel = (uint32*)rowData;
		for( int c = 0; c < width; ++c )
		{
			uint32 val = pixel[c];

			// merge bits from two values according to a mask,
			// equivalent to: (val & ~amask) | (properAlpha & amask)
			pixel[c] = val ^ ((val ^ properAlpha) & amask);
		}
		rowData += pitch;
	}
}

void SetAlphaToRedChannel (int width, int height, UInt8* data, int pitch, const prcore::PixelFormat& pf)
{
	UInt8* rowData = data;
	uint32 rmask = pf.GetRedMask();
	uint32 amask = pf.GetAlphaMask();
	uint8 roffset = (int)pf.GetRedOffset() - (int)pf.GetRedBits() + 1;
	uint8 aoffset = (int)pf.GetAlphaOffset() - (int)pf.GetAlphaBits() + 1;

	for( int r = 0; r < height; ++r )
	{
		uint32* pixel = (uint32*)rowData;
		for( int c = 0; c < width; ++c )
		{
			uint32 val = pixel[c];
			uint32 red = (val & rmask) >> roffset;
			pixel[c] = (val & ~amask) | (red << aoffset);
		}
		rowData += pitch;
	}
}

inline UInt8 XenonToNormalSRGBTexture (UInt32 value)
{
	value = UInt32(LinearToGammaSpace(GammaToLinearSpaceXenon (value / 255.0F)) * 255.0F);
	return std::min<UInt32>(value, 255);
}

void XenonToNormalSRGBTexture (int width, int height, UInt8* data, int pitch, const prcore::PixelFormat& pf)
{
	UInt8* rowData = data;
	uint32 rmask = pf.GetRedMask();
	uint32 gmask = pf.GetGreenMask();
	uint32 bmask = pf.GetBlueMask();
	uint32 amask = pf.GetAlphaMask();
	uint8 roffset = (int)pf.GetRedOffset() - (int)pf.GetRedBits() + 1;
	uint8 goffset = (int)pf.GetGreenOffset() - (int)pf.GetGreenBits() + 1;
	uint8 boffset = (int)pf.GetBlueOffset() - (int)pf.GetBlueBits() + 1;
	uint8 aoffset = (int)pf.GetAlphaOffset() - (int)pf.GetAlphaBits() + 1;

	for( int r = 0; r < height; ++r )
	{
		uint32* pixel = (uint32*)rowData;
		for( int c = 0; c < width; ++c )
		{
			uint32 val = pixel[c];
			uint32 r = (val & rmask) >> roffset;
			uint32 g = (val & gmask) >> goffset;
			uint32 b = (val & bmask) >> boffset;
			uint32 a = (val & amask) >> aoffset;

			r = XenonToNormalSRGBTexture (r);
			g = XenonToNormalSRGBTexture (g);
			b = XenonToNormalSRGBTexture (b);

			pixel[c] = (r << roffset) | (g << goffset) | (b << boffset) | (a << aoffset);
		}
		rowData += pitch;
	}
}




// --------------------------------------------------------------------------


const char* kUnsupportedGetPixelOpFormatMessage = "Unsupported texture format - needs to be ARGB32, RGBA32, BGRA32, RGB24, Alpha8 or DXT";
const char* kUnsupportedSetPixelOpFormatMessage = "Unsupported texture format - needs to be ARGB32, RGBA32, RGB24 or Alpha8";

inline int RepeatInt (int x, int width)
{
	if (width == 0) return 0;
	if( x < 0 ) {
		int times = -(x/width)+1;
		x += width * times;
	}
	x = x % width;
	return x;
}

inline int TextureWrap(int x, int max, TextureWrapMode wrapMode)
{
	if (wrapMode == kTexWrapRepeat)
	{
		return RepeatInt(x, max);
	}
	else
	{
		if (max <= 0)
			return 0;
		return clamp(x, 0, max-1);
	}
}

void SetImagePixel (ImageReference& image, int x, int y, TextureWrapMode wrap, const ColorRGBAf& color)
{
	int width = image.GetWidth();
	int height = image.GetHeight();
	if (x < 0 || x >= width || y < 0 || y >= height)
	{
		x = TextureWrap(x, width, wrap);
		y = TextureWrap(y, height, wrap);
	}

	if (image.GetFormat() == kTexFormatARGB32)
	{
		UInt8* pixel = image.GetRowPtr(y) + x * 4;
		pixel[1] = RoundfToIntPos(clamp01(color.r) * 255.0);
		pixel[2] = RoundfToIntPos(clamp01(color.g) * 255.0);
		pixel[3] = RoundfToIntPos(clamp01(color.b) * 255.0);
		pixel[0] = RoundfToIntPos(clamp01(color.a) * 255.0);
	}
	else if (image.GetFormat() == kTexFormatRGBA32)
	{
		UInt8* pixel = image.GetRowPtr(y) + x * 4;
		pixel[0] = RoundfToIntPos(clamp01(color.r) * 255.0);
		pixel[1] = RoundfToIntPos(clamp01(color.g) * 255.0);
		pixel[2] = RoundfToIntPos(clamp01(color.b) * 255.0);
		pixel[3] = RoundfToIntPos(clamp01(color.a) * 255.0);
	}
	else if (image.GetFormat() == kTexFormatRGB24)
	{
		UInt8* pixel = image.GetRowPtr(y) + x * 3;
		pixel[0] = RoundfToIntPos(clamp01(color.r) * 255.0);
		pixel[1] = RoundfToIntPos(clamp01(color.g) * 255.0);
		pixel[2] = RoundfToIntPos(clamp01(color.b) * 255.0);
	}
	else if (image.GetFormat() == kTexFormatRGB565)
	{
		UInt16* pixel = (UInt16 *)(image.GetRowPtr(y) + x * 2);
		UInt16 r = (UInt16)(RoundfToIntPos( clamp01(color.r) * 31.0f));
		UInt16 g = (UInt16)(RoundfToIntPos( clamp01(color.g) * 63.0f));
		UInt16 b = (UInt16)(RoundfToIntPos( clamp01(color.b) * 31.0f));
		pixel[0] = r<<11 | g<<5 | b;
	}
	else if (image.GetFormat() == kTexFormatAlpha8)
	{
		UInt8* pixel = image.GetRowPtr(y) + x;
		pixel[0] = RoundfToIntPos(clamp01(color.a) * 255.0);
	}
	else
	{
		ErrorString(kUnsupportedSetPixelOpFormatMessage);
	}
}


ColorRGBA32 GetImagePixel (UInt8* data, int width, int height, TextureFormat format, TextureWrapMode wrap, int x, int y)
{
	if (x < 0 || x >= width || y < 0 || y >= height)
	{
		x = TextureWrap (x, width, wrap);
		y = TextureWrap (y, height, wrap);
	}

	if( IsCompressedDXTTextureFormat(format) )
	{
		int texWidth = std::max (width, 4);

		UInt32 uncompressed [16];
		int blockBytes = (format == kTexFormatDXT1 ? 8 : 16);

		const UInt8 *pos = data + ((x/4) + (y/4)*(texWidth/4)) * blockBytes;
		DecompressNativeTextureFormat (format, 4, 4, (const UInt32*)pos, 4, 4, uncompressed);

		const UInt8* pixel = (const UInt8*)(uncompressed + x%4 + 4* (y%4));
		return ColorRGBA32(pixel[0], pixel[1], pixel[2], pixel[3]);
	}
	else if( IsAnyCompressedTextureFormat (format) )
	{
		ErrorString(kUnsupportedGetPixelOpFormatMessage);
	}
	else
	{
		ImageReference image (width, height, GetRowBytesFromWidthAndFormat(width, format), format, data);
		const UInt8* pixel;
		if (format == kTexFormatARGB32)
		{
			pixel = image.GetRowPtr(y) + x * 4;
			return ColorRGBA32 (pixel[1], pixel[2], pixel[3], pixel[0]);
		}
		else if (format == kTexFormatRGBA32)
		{
			pixel = image.GetRowPtr(y) + x * 4;
			return ColorRGBA32 (pixel[0], pixel[1], pixel[2], pixel[3]);
		}
		else if (format == kTexFormatBGRA32)
		{
			pixel = image.GetRowPtr(y) + x * 4;
			return ColorRGBA32 (pixel[2], pixel[1], pixel[0], pixel[3]);
		}
		else if (format == kTexFormatRGB24)
		{
			pixel = image.GetRowPtr(y) + x * 3;
			return ColorRGBA32 (pixel[0], pixel[1], pixel[2], 255);
		}
		else if (format == kTexFormatAlpha8)
		{
			pixel = image.GetRowPtr(y) + x;
			return ColorRGBA32 (255, 255, 255, pixel[0]);
		}
		else if (format == kTexFormatRGBA4444)
		{
			pixel = image.GetRowPtr(y) + x * 2;
			return ColorRGBA32(				
				(pixel[1] & 0xF0) << 4 | (pixel[1] & 0xF0),
				(pixel[1] & 0x0F) << 4 | (pixel[1] & 0x0F),
				(pixel[0] & 0xF0) << 4 | (pixel[0] & 0xF0),
				(pixel[0] & 0x0F) << 4 | (pixel[0] & 0x0F)
				);
		}
		else if (format == kTexFormatARGB4444)
		{
			pixel = image.GetRowPtr(y) + x * 2;
			return ColorRGBA32(				
				(pixel[1] & 0x0F) << 4 | (pixel[1] & 0x0F),
				(pixel[0] & 0xF0) << 4 | (pixel[0] & 0xF0),
				(pixel[0] & 0x0F) << 4 | (pixel[0] & 0x0F),
				(pixel[1] & 0xF0) << 4 | (pixel[1] & 0xF0)
				);
		}
		else if (format == kTexFormatRGB565)
		{
			pixel = image.GetRowPtr(y) + x * 2;
			UInt16 c = *reinterpret_cast<const UInt16*>(pixel);
			UInt8  r = (UInt8)((c >> 11)&0x1F);
			UInt8  g = (UInt8)((c >>  5)&0x3F);
			UInt8  b = (UInt8)((c >>  0)&0x1F);
			return ColorRGBA32 (r<<3|r>>2, g<<2|g>>4, b<<3|b>>2, 255);
		}
		else
		{
			ErrorString(kUnsupportedGetPixelOpFormatMessage);
		}
	}
	return ColorRGBA32(255,255,255,255);
}

ColorRGBAf GetImagePixelBilinear (UInt8* data, int width, int height, TextureFormat format, TextureWrapMode wrap, float u, float v)
{
	u *= width;
	v *= height;

	int xBase = FloorfToInt(u);
	int yBase = FloorfToInt(v);

	float s = u - xBase;
	float t = v - yBase;

	ColorRGBAf colors[4];

	if (IsCompressedDXTTextureFormat(format))
	{
		if (xBase < 0 || xBase+1 >= width || yBase < 0 || yBase+1 >= height)
		{
			for (int i=0;i<4;i++)
			{
				int x = xBase;
				int y = yBase;
				if (i & 1)
					x++;
				if (i & 2)
					y++;

				x = TextureWrap(x, width, wrap);
				y = TextureWrap(y, height, wrap);

				colors[i] = GetImagePixel (data, width, height, format, wrap, x, y);
			}
		}
		else
		{
			GetImagePixelBlock (data, width, height, format, xBase, yBase, 2, 2, colors);
		}
	}
	else if( IsAnyCompressedTextureFormat(format) )
	{
		ErrorString(kUnsupportedGetPixelOpFormatMessage);
		return ColorRGBAf(1.0F,1.0F,1.0F,1.0F);
	}
	else
	{
		ImageReference image (width, height, GetRowBytesFromWidthAndFormat(width, format), format, data);
		for (int i=0;i<4;i++)
		{
			int x = xBase;
			int y = yBase;
			if (i & 1)
				x++;
			if (i & 2)
				y++;

			if (x < 0 || x >= width || y < 0 || y >= height)
			{
				x = TextureWrap(x, width, wrap);
				y = TextureWrap(y, height, wrap);
			}

			UInt8* pixel;
			if (format == kTexFormatARGB32)
			{
				pixel = image.GetRowPtr(y) + x * 4;
				colors[i] = ColorRGBA32 (pixel[1], pixel[2], pixel[3], pixel[0]);
			}
			else if (format == kTexFormatRGBA32)
			{
				pixel = image.GetRowPtr(y) + x * 4;
				colors[i] = ColorRGBA32 (pixel[0], pixel[1], pixel[2], pixel[3]);
			}
			else if (format == kTexFormatBGRA32)
			{
				pixel = image.GetRowPtr(y) + x * 4;
				colors[i] = ColorRGBA32 (pixel[2], pixel[1], pixel[0], pixel[3]);
			}
			else if (format == kTexFormatRGB24)
			{
				pixel = image.GetRowPtr(y) + x * 3;
				colors[i] = ColorRGBA32 (pixel[0], pixel[1], pixel[2], 255);
			}
			else if (format == kTexFormatAlpha8)
			{
				pixel = image.GetRowPtr(y) + x;
				colors[i] = ColorRGBA32 (255, 255, 255, pixel[0]);
			}
			else
			{
				ErrorString(kUnsupportedGetPixelOpFormatMessage);
				return ColorRGBAf(1.0F,1.0F,1.0F,1.0F);
			}
		}
	}
	ColorRGBAf a = Lerp(colors[0], colors[1], s);
	ColorRGBAf b = Lerp(colors[2], colors[3], s);
	return Lerp(a, b, t);
}

bool GetImagePixelBlock (UInt8* data, int dataWidth, int dataHeight, TextureFormat format, int x, int y, int blockWidth, int blockHeight, ColorRGBAf* outColors)
{
	// Checks
	if (blockWidth <= 0 || blockHeight <= 0)
	{
		ErrorString ("Width and height must be positive");
		return false;
	}

	// TODO: repeat/crop support
	if (x < 0 || y < 0 || x + blockWidth < 0 || y + blockHeight < 0 || x + blockWidth > dataWidth || y + blockHeight > dataHeight)
	{
		char mad[255];

		if (x < 0)
			sprintf(mad, "Texture rectangle is out of bounds (%d < 0)", x);
		if (y < 0)
			sprintf(mad, "Texture rectangle is out of bounds (%d < 0)", y);
		if (x + blockWidth > dataWidth)
			sprintf(mad, "Texture rectangle is out of bounds (%d + %d > %d)", x, blockWidth, dataWidth);
		if (y + blockHeight > dataHeight)
			sprintf(mad, "Texture rectangle is out of bounds (%d + %d > %d)", y, blockHeight, dataHeight);

		ErrorString(mad);
		return false;
	}

	if (IsCompressedDXTTextureFormat(format))
	{
		int texWidth = std::max (dataWidth, 4);

		int paddedWidth = x+blockWidth-(x&~3);
		if(paddedWidth % 4)
			paddedWidth = (paddedWidth&~3) + 4;
		int paddedHeight = y+blockHeight-(y&~3);
		if(paddedHeight % 4)
			paddedHeight = (paddedHeight&~3) + 4;

		UInt32* uncompressed;
		ALLOC_TEMP(uncompressed, UInt32, paddedWidth * paddedHeight);

		int blockBytes = (format == kTexFormatDXT1 ? 8 : 16);

		for(int line = 0; line< paddedHeight; line+=4)
		{
			UInt8 *pos = data + ((x/4) + ((y+line)/4)*(texWidth/4)) * blockBytes;
			DecompressNativeTextureFormat(format, paddedWidth, 4, (UInt32*)pos, paddedWidth, 4, uncompressed+(line*paddedWidth));
		}

		ColorRGBAf* dest = outColors;
		const UInt8* pixelRow = (UInt8*)(uncompressed + x%4 + (y%4)*paddedWidth);
		for( int iy = 0; iy < blockHeight; ++iy )
		{
			const UInt8* pixel = pixelRow;
			for( int ix = 0; ix < blockWidth; ++ix )
			{
				*dest = ColorRGBA32(pixel[0], pixel[1], pixel[2], pixel[3]);
				pixel += 4;
				++dest;
			}
			pixelRow += paddedWidth * 4;
		}
	}
	else
	{
		ImageReference image (dataWidth, dataHeight, GetRowBytesFromWidthAndFormat(dataWidth, format), format, data);

		ColorRGBAf* dest = outColors;

		if (format == kTexFormatARGB32)
		{
			const UInt8* pixelRow = image.GetRowPtr(y) + x * 4;
			for( int iy = 0; iy < blockHeight; ++iy )
			{
				const UInt8* pixel = pixelRow;
				for( int ix = 0; ix < blockWidth; ++ix )
				{
					*dest = ColorRGBA32(pixel[1], pixel[2], pixel[3], pixel[0]);
					pixel += 4;
					++dest;
				}
				pixelRow += image.GetRowBytes();
			}
		}
		else if (format == kTexFormatRGBA32)
		{
			const UInt8* pixelRow = image.GetRowPtr(y) + x * 4;
			for( int iy = 0; iy < blockHeight; ++iy )
			{
				const UInt8* pixel = pixelRow;
				for( int ix = 0; ix < blockWidth; ++ix )
				{
					*dest = ColorRGBA32(pixel[0], pixel[1], pixel[2], pixel[3]);
					pixel += 4;
					++dest;
				}
				pixelRow += image.GetRowBytes();
			}
		}
		else if (format == kTexFormatBGRA32)
		{
			const UInt8* pixelRow = image.GetRowPtr(y) + x * 4;
			for( int iy = 0; iy < blockHeight; ++iy )
			{
				const UInt8* pixel = pixelRow;
				for( int ix = 0; ix < blockWidth; ++ix )
				{
					*dest = ColorRGBA32(pixel[2], pixel[1], pixel[0], pixel[3]);
					pixel += 4;
					++dest;
				}
				pixelRow += image.GetRowBytes();
			}
		}
		else if (format == kTexFormatRGB24)
		{
			const UInt8* pixelRow = image.GetRowPtr(y) + x * 3;
			for( int iy = 0; iy < blockHeight; ++iy )
			{
				const UInt8* pixel = pixelRow;
				for( int ix = 0; ix < blockWidth; ++ix )
				{
					*dest = ColorRGBA32(pixel[0], pixel[1], pixel[2], 255);
					pixel += 3;
					++dest;
				}
				pixelRow += image.GetRowBytes();
			}
		}
		else if (format == kTexFormatAlpha8)
		{
			const UInt8* pixelRow = image.GetRowPtr(y) + x;
			for( int iy = 0; iy < blockHeight; ++iy )
			{
				const UInt8* pixel = pixelRow;
				for( int ix = 0; ix < blockWidth; ++ix )
				{
					*dest = ColorRGBA32 (255, 255, 255, pixel[0]);
					pixel += 1;
					++dest;
				}
				pixelRow += image.GetRowBytes();
			}
		}
		else if (format == kTexFormatRGB565)
		{
			const UInt8* pixelRow = image.GetRowPtr(y) + x;
			for( int iy = 0; iy < blockHeight; ++iy )
			{
				const UInt8* pixel = pixelRow;
				for( int ix = 0; ix < blockWidth; ++ix )
				{
					UInt16 c = (UInt16)pixel[0] | ((UInt16)pixel[1])<<8;
					UInt8  r = (UInt8)((c >> 11)&31);
					UInt8  g = (UInt8)((c >>  5)&63);
					UInt8  b = (UInt8)((c >>  0)&31);
				
					*dest = ColorRGBA32 (r<<3|r>>2, g<<2|g>>4, b<<3|b>>2, 255);
					pixel += 2;
					++dest;
				}
				pixelRow += image.GetRowBytes();
			}
		}
		else
		{
			ErrorString(kUnsupportedGetPixelOpFormatMessage);
			return false;
		}
	}
	return true;
}


bool SetImagePixelBlock (UInt8* data, int dataWidth, int dataHeight, TextureFormat format, int x, int y, int blockWidth, int blockHeight, int pixelCount, const ColorRGBAf* pixels)
{
	if (IsAnyCompressedTextureFormat(format))
	{
		ErrorString(kUnsupportedSetPixelOpFormatMessage);
		return false;
	}
	if (blockWidth <= 0 || blockHeight <= 0)
	{
		ErrorString ("Width and height must be positive");
		return false;
	}

	int tmp = blockWidth * blockHeight;
	if (blockHeight != tmp / blockWidth || blockWidth * blockHeight > pixelCount ) // check for overflow as well
	{
		ErrorString ("Array size must be at least width*height");
		return false;
	}

	if (x < 0 || y < 0 || x + blockWidth < 0 || y + blockHeight < 0 || x + blockWidth > dataWidth || y + blockHeight > dataHeight)
	{
		ErrorString ("Texture rectangle is out of bounds");
		return false;
	}

	ImageReference image (dataWidth, dataHeight, GetRowBytesFromWidthAndFormat(dataWidth, format), format, data);

	if (format == kTexFormatARGB32)
	{
		UInt8* pixelRow = image.GetRowPtr(y) + x * 4;
		for( int iy = 0; iy < blockHeight; ++iy )
		{
			UInt8* pixel = pixelRow;
			for( int ix = 0; ix < blockWidth; ++ix )
			{
				pixel[1] = NormalizedToByte( pixels->r);
				pixel[2] = NormalizedToByte( pixels->g);
				pixel[3] = NormalizedToByte( pixels->b);
				pixel[0] = NormalizedToByte( pixels->a);
				pixel += 4;
				++pixels;
			}
			pixelRow += image.GetRowBytes();
		}
	}
	else if (format == kTexFormatRGBA32)
	{
		UInt8* pixelRow = image.GetRowPtr(y) + x * 4;
		for( int iy = 0; iy < blockHeight; ++iy )
		{
			UInt8* pixel = pixelRow;
			for( int ix = 0; ix < blockWidth; ++ix )
			{
				pixel[0] = NormalizedToByte( pixels->r);
				pixel[1] = NormalizedToByte( pixels->g);
				pixel[2] = NormalizedToByte( pixels->b);
				pixel[3] = NormalizedToByte( pixels->a);
				pixel += 4;
				++pixels;
			}
			pixelRow += image.GetRowBytes();
		}
	}
	else if (format == kTexFormatRGB24)
	{
		UInt8* pixelRow = image.GetRowPtr(y) + x * 3;
		for( int iy = 0; iy < blockHeight; ++iy )
		{
			UInt8* pixel = pixelRow;
			for( int ix = 0; ix < blockWidth; ++ix )
			{
				pixel[0] = NormalizedToByte(pixels->r);
				pixel[1] = NormalizedToByte(pixels->g);
				pixel[2] = NormalizedToByte(pixels->b);
				pixel += 3;
				++pixels;
			}
			pixelRow += image.GetRowBytes();
		}
	}
	else if (format == kTexFormatAlpha8)
	{
		UInt8* pixelRow = image.GetRowPtr(y) + x;
		for( int iy = 0; iy < blockHeight; ++iy )
		{
			UInt8* pixel = pixelRow;
			for( int ix = 0; ix < blockWidth; ++ix )
			{
				pixel[0] = RoundfToIntPos( clamp01(pixels->a) * 255.0f );
				pixel += 1;
				++pixels;
			}
			pixelRow += image.GetRowBytes();
		}
	}
	else if (format == kTexFormatRGB565)
	{
		UInt8* pixelRow = image.GetRowPtr(y) + x;
		for( int iy = 0; iy < blockHeight; ++iy )
		{
			UInt16* pixel = (UInt16 *)pixelRow;
			for( int ix = 0; ix < blockWidth; ++ix )
			{
				UInt16 r = (UInt16)(RoundfToIntPos(clamp01(pixels->r) * 31.0f));
				UInt16 g = (UInt16)(RoundfToIntPos(clamp01(pixels->g) * 63.0f));
				UInt16 b = (UInt16)(RoundfToIntPos(clamp01(pixels->b) * 31.0f));
				pixel[0] = r<<11 | g<<5 | b;
				pixel += 1;
				++pixels;
			}
			pixelRow += image.GetRowBytes();
		}
	}
	else
	{
		ErrorString(kUnsupportedSetPixelOpFormatMessage);
		return false;
	}

	return true;
}



// --------------------------------------------------------------------------


#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"


SUITE (ImageOpsTests)
{

TEST (RepeatInt)
{
	// valdemar: seems like a CW compiler bug, error: (10139) division by 0
#if !UNITY_WII
	// handle zero width
	CHECK_EQUAL (0, RepeatInt (7,0));
#endif
	// handle positive args
	CHECK_EQUAL (7, RepeatInt (7,13));
	CHECK_EQUAL (0, RepeatInt (13,13));
	CHECK_EQUAL (1, RepeatInt (170,13));
	// handle negative args
	CHECK_EQUAL (12, RepeatInt (-1,13));
	CHECK_EQUAL (0, RepeatInt (-13,13));
}

TEST (TextureWrap)
{
	// valdemar: seems like a CW compiler bug, error: (10139) division by 0
#if !UNITY_WII
	// handle zero width
	CHECK_EQUAL (0, TextureWrap (7,0,kTexWrapClamp));
	CHECK_EQUAL (0, TextureWrap (7,0,kTexWrapRepeat));
#endif
	// repeat mode
	CHECK_EQUAL (7, TextureWrap (7,13,kTexWrapRepeat));
	CHECK_EQUAL (1, TextureWrap (170,13,kTexWrapRepeat));
	CHECK_EQUAL (12, TextureWrap (-1,13,kTexWrapRepeat));
	// clamp mode
	CHECK_EQUAL (7, TextureWrap (7,13,kTexWrapClamp));
	CHECK_EQUAL (0, TextureWrap (-1,13,kTexWrapClamp));
	CHECK_EQUAL (12, TextureWrap (13,13,kTexWrapClamp));
}

TEST (SetGetImagePixelARGB)
{
	UInt8 data[4][4];
	memset (data, 13, sizeof(data));
	ImageReference image (2, 2, 8, kTexFormatARGB32, data);
	SetImagePixel (image, 0, 0, kTexWrapRepeat, ColorRGBAf(1.0f,0.5f,0.3f,0.2f)); // sets [0]
	CHECK (data[0][1]==255 && data[0][2]==128 && data[0][3]==77 && data[0][0]==51);
	SetImagePixel (image, 3, 8, kTexWrapRepeat, ColorRGBAf(0.1f,0.2f,0.3f,0.4f)); // sets [1] due to repeat
	CHECK (data[1][1]==26 && data[1][2]==51 && data[1][3]==77 && data[1][0]==102);
	SetImagePixel (image, -3, 1, kTexWrapClamp, ColorRGBAf(0.3f,0.4f,0.5f,0.6f)); // sets [2] due to clamp
	CHECK (data[2][1]==77 && data[2][2]==102 && data[2][3]==128 && data[2][0]==153);

	CHECK (data[3][1]==13 && data[3][2]==13 && data[3][3]==13 && data[3][0]==13); // [3] left untouched

	CHECK(ColorRGBA32(ColorRGBAf(1.0f,0.5f,0.3f,0.2f)) == GetImagePixel (&data[0][0], 2, 2, image.GetFormat(), kTexWrapRepeat, 2, 2)); // gets [0] due to repeat
	CHECK(ColorRGBA32(ColorRGBAf(0.1f,0.2f,0.3f,0.4f)) == GetImagePixel (&data[0][0], 2, 2, image.GetFormat(), kTexWrapRepeat, 5, -2)); // gets [1] due to repeat
	CHECK(ColorRGBA32(ColorRGBAf(0.3f,0.4f,0.5f,0.6f)) == GetImagePixel (&data[0][0], 2, 2, image.GetFormat(), kTexWrapClamp, -1, 1)); // gets [2] due to clamp
}

TEST (SetGetImagePixelRGBA)
{
	UInt8 data[4][4];
	memset (data, 13, sizeof(data));
	ImageReference image (2, 2, 8, kTexFormatRGBA32, data);
	SetImagePixel (image, 0, 0, kTexWrapRepeat, ColorRGBAf(1.0f,0.5f,0.3f,0.2f)); // sets [0]
	CHECK (data[0][0]==255 && data[0][1]==128 && data[0][2]==77 && data[0][3]==51);
	SetImagePixel (image, 3, 8, kTexWrapRepeat, ColorRGBAf(0.1f,0.2f,0.3f,0.4f)); // sets [1] due to repeat
	CHECK (data[1][0]==26 && data[1][1]==51 && data[1][2]==77 && data[1][3]==102);
	SetImagePixel (image, -3, 1, kTexWrapClamp, ColorRGBAf(0.3f,0.4f,0.5f,0.6f)); // sets [2] due to clamp
	CHECK (data[2][0]==77 && data[2][1]==102 && data[2][2]==128 && data[2][3]==153);

	CHECK (data[3][1]==13 && data[3][2]==13 && data[3][3]==13 && data[3][0]==13); // [3] left untouched

	CHECK(ColorRGBA32(ColorRGBAf(1.0f,0.5f,0.3f,0.2f)) == GetImagePixel (&data[0][0], 2, 2, image.GetFormat(), kTexWrapRepeat, 2, 2)); // gets [0] due to repeat
	CHECK(ColorRGBA32(ColorRGBAf(0.1f,0.2f,0.3f,0.4f)) == GetImagePixel (&data[0][0], 2, 2, image.GetFormat(), kTexWrapRepeat, 5, -2)); // gets [1] due to repeat
	CHECK(ColorRGBA32(ColorRGBAf(0.3f,0.4f,0.5f,0.6f)) == GetImagePixel (&data[0][0], 2, 2, image.GetFormat(), kTexWrapClamp, -1, 1)); // gets [2] due to clamp
}

TEST (SetGetImagePixelRGB)
{
	UInt8 data[4][3];
	memset (data, 13, sizeof(data));
	ImageReference image (2, 2, 6, kTexFormatRGB24, data);
	SetImagePixel (image, 0, 0, kTexWrapClamp, ColorRGBAf(1.0f,0.5f,0.3f,0.2f)); // sets [0]
	CHECK (data[0][0]==255 && data[0][1]==128 && data[0][2]==77);
	SetImagePixel (image, 1, 0, kTexWrapClamp, ColorRGBAf(0.1f,0.2f,0.3f,0.4f)); // sets [1]
	CHECK (data[1][0]==26 && data[1][1]==51 && data[1][2]==77);
	SetImagePixel (image, 0, 1, kTexWrapClamp, ColorRGBAf(0.3f,0.4f,0.5f,0.6f)); // sets [2]
	CHECK (data[2][0]==77 && data[2][1]==102 && data[2][2]==128);

	CHECK (data[3][0]==13 && data[3][1]==13 && data[3][2]==13); // [3] left untouched

	CHECK(ColorRGBA32(ColorRGBAf(1.0f,0.5f,0.3f,1)) == GetImagePixel (&data[0][0], 2, 2, image.GetFormat(), kTexWrapRepeat, 2, 2)); // gets [0] due to repeat
	CHECK(ColorRGBA32(ColorRGBAf(0.1f,0.2f,0.3f,1)) == GetImagePixel (&data[0][0], 2, 2, image.GetFormat(), kTexWrapRepeat, 5, -2)); // gets [1] due to repeat
	CHECK(ColorRGBA32(ColorRGBAf(0.3f,0.4f,0.5f,1)) == GetImagePixel (&data[0][0], 2, 2, image.GetFormat(), kTexWrapClamp, -1, 1)); // gets [2] due to clamp
}

TEST (SetGetImagePixelAlpha)
{
	UInt8 data[4];
	memset (data, 13, sizeof(data));
	ImageReference image (2, 2, 2, kTexFormatAlpha8, data);
	SetImagePixel (image, -3, -2, kTexWrapClamp, ColorRGBAf(1.0f,0.5f,0.3f,0.2f)); // sets [0] due to clamp
	CHECK (data[0]==51);
	SetImagePixel (image, 1, -4, kTexWrapRepeat, ColorRGBAf(0.1f,0.2f,0.3f,0.4f)); // sets [1] due to repeat
	CHECK (data[1]==102);
	SetImagePixel (image, -4, 7, kTexWrapRepeat, ColorRGBAf(0.3f,0.4f,0.5f,0.6f)); // sets [2] due to repeat
	CHECK (data[2]==153);

	CHECK (data[3]==13); // [3] left untouched

	CHECK(ColorRGBA32(ColorRGBAf(1,1,1,0.2f)) == GetImagePixel (&data[0], 2, 2, image.GetFormat(), kTexWrapRepeat, 2, 2)); // gets [0] due to repeat
	CHECK(ColorRGBA32(ColorRGBAf(1,1,1,0.4f)) == GetImagePixel (&data[0], 2, 2, image.GetFormat(), kTexWrapRepeat, 5, -2)); // gets [1] due to repeat
	CHECK(ColorRGBA32(ColorRGBAf(1,1,1,0.6f)) == GetImagePixel (&data[0], 2, 2, image.GetFormat(), kTexWrapClamp, -1, 1)); // gets [2] due to clamp
}

TEST (SetGetImagePixelRGB565)
{
	UInt16 data[4];
	memset (data, 0xab, sizeof(data));
	ImageReference image (2, 2, 4, kTexFormatRGB565, data);
	SetImagePixel (image, 0, 0, kTexWrapClamp, ColorRGBAf(1.0f,0.0f,0.0f,0.2f)); // sets [0]
	CHECK (data[0]==0xf800);
	SetImagePixel (image, 1, 0, kTexWrapClamp, ColorRGBAf(0.0f,1.0f,0.0f,0.4f)); // sets [1]
	CHECK (data[1]==0x07e0);
	SetImagePixel (image, 0, 1, kTexWrapClamp, ColorRGBAf(0.0f,0.0f,1.0f,0.6f)); // sets [2]
	CHECK (data[2]==0x001f);
	CHECK (data[3]==0xabab); // [3] still left untouched

	ColorRGBAf gray(14.0f/31.0f, 31.0f/63.0f, 16.0f/31.0f, 1.0f);
	SetImagePixel (image, 1, 1, kTexWrapClamp, gray); // sets [3]
	CHECK (data[3]==0x73f0);

	UInt8* srcData = reinterpret_cast<UInt8*>(&data[0]);
	CHECK(ColorRGBA32(ColorRGBAf(1.0f,0.0f,0.0f,1)) == GetImagePixel (srcData, 2, 2, image.GetFormat(), kTexWrapRepeat, 2, 2)); // gets [0] due to repeat
	CHECK(ColorRGBA32(ColorRGBAf(0.0f,1.0f,0.0f,1)) == GetImagePixel (srcData, 2, 2, image.GetFormat(), kTexWrapRepeat, 5, -2)); // gets [1] due to repeat
	CHECK(ColorRGBA32(ColorRGBAf(0.0f,0.0f,1.0f,1)) == GetImagePixel (srcData, 2, 2, image.GetFormat(), kTexWrapClamp, -1, 1)); // gets [2] due to clamp
	CHECK(ColorRGBA32(gray) == GetImagePixel (srcData, 2, 2, image.GetFormat(), kTexWrapClamp, 2, 2)); // gets [3] due to clamp
}

TEST (SetImagePixelBlockARGB)
{
	UInt8 data[16][16][4];
	memset (data, 13, sizeof(data));
	ImageReference image (16, 16, 16*4, kTexFormatARGB32, data);

	ColorRGBAf color (1,0,1,0);
	SetImagePixelBlock (&data[0][0][0], 16, 16, kTexFormatARGB32, 15,15,1,1, 1, &color);
	CHECK (data[15][15][1]==255 && data[15][15][2]==0 && data[15][15][3]==255 && data[15][15][0]==0);
}

#if UNITY_EDITOR
TEST (BlitBilinearARGBToFloat)
{
	Image src (128, 256, kTexFormatARGB32);
	Image dst (16, 16, kTexFormatARGB32);
	// ProphecySDK's bilinear blitter from integer source to floating point destination
	// when sizes are different allocates temporary intermediate image, and blit there
	// can result in access violation for the last pixel. This unit test would crash then, especially
	// on Macs. ProphecySDK modified to allocate one pixel more for temporary images, just like
	// we do for ours.
	dst.ReformatImage (src, 128, 128, kTexFormatARGBFloat, Image::BLIT_BILINEAR_SCALE);
}
#endif

TEST (CreateMipMap2x2)
{
	ColorRGBA32 data[4+1+1]; // 2x2, 1x1, one extra for out-of-bounds check
	memset (data, 13, sizeof(data));
	data[0] = ColorRGBA32(255,255,255,255);
	data[1] = ColorRGBA32(255,255,255,0);
	data[2] = ColorRGBA32(255,255,0,0);
	data[3] = ColorRGBA32(255,0,0,0);
	CreateMipMap ((UInt8*)data, 2, 2, 1, kTexFormatARGB32);

	// next mip level of 1x1 size
	CHECK(ColorRGBA32(255,191,127,63) == data[4]);

	// data after that should be untouched
	CHECK(ColorRGBA32(13,13,13,13) == data[5]);
}
TEST (CreateMipMap4x1)
{
	ColorRGBA32 data[4+2+1+1]; // 4x1, 2x1, 1x1, one extra for out-of-bounds check
	memset (data, 13, sizeof(data));
	data[0] = ColorRGBA32(255,255,255,255);
	data[1] = ColorRGBA32(255,255,255,0);
	data[2] = ColorRGBA32(255,255,0,0);
	data[3] = ColorRGBA32(255,0,0,0);
	CreateMipMap ((UInt8*)data, 4, 1, 1, kTexFormatARGB32);

	// next mip level of 2x1 size
	CHECK(ColorRGBA32(255,255,255,127) == data[4]);
	CHECK(ColorRGBA32(255,127,0,0) == data[5]);

	// next mip level of 1x1 size
	CHECK(ColorRGBA32(255,191,127,63) == data[6]);

	// data after that should be untouched
	CHECK(ColorRGBA32(13,13,13,13) == data[7]);
}
TEST (CreateMipMap4x1x2)
{
	ColorRGBA32 data[8+2+1+1]; // 4x1x2, 2x1x1, 1x1x1, one extra for out-of-bounds check
	memset (data, 13, sizeof(data));
	data[0] = ColorRGBA32(255,255,255,255);
	data[1] = ColorRGBA32(255,255,255,0);
	data[2] = ColorRGBA32(255,255,0,0);
	data[3] = ColorRGBA32(255,0,0,0);
	data[4] = ColorRGBA32(128,128,128,128);
	data[5] = ColorRGBA32(128,128,128,0);
	data[6] = ColorRGBA32(128,128,0,0);
	data[7] = ColorRGBA32(128,0,0,0);
	CreateMipMap ((UInt8*)data, 4, 1, 2, kTexFormatARGB32);

	// next mip level, 2x1x1 size
	CHECK(ColorRGBA32(191,191,191,95) == data[8]);
	CHECK(ColorRGBA32(191,95,0,0) == data[9]);

	// next mip level, 1x1x1 size
	CHECK(ColorRGBA32(191,143,95,47) == data[10]);

	// data after that should be untouched
	CHECK(ColorRGBA32(13,13,13,13) == data[11]);
}
TEST (CreateMipMap4x1x3)
{
	ColorRGBA32 data[12+2+1+1]; // 4x1x2, 2x1x1, 1x1x1, one extra for out-of-bounds check
	memset (data, 13, sizeof(data));
	data[0] = ColorRGBA32(255,255,255,255);
	data[1] = ColorRGBA32(255,255,255,0);
	data[2] = ColorRGBA32(255,255,0,0);
	data[3] = ColorRGBA32(255,0,0,0);
	data[4] = ColorRGBA32(128,128,128,128);
	data[5] = ColorRGBA32(128,128,128,0);
	data[6] = ColorRGBA32(128,128,0,0);
	data[7] = ColorRGBA32(128,0,0,0);
	data[8] = ColorRGBA32(64,64,64,64);
	data[9] = ColorRGBA32(64,64,64,0);
	data[10] = ColorRGBA32(64,64,0,0);
	data[11] = ColorRGBA32(64,0,0,0);
	CreateMipMap ((UInt8*)data, 4, 1, 3, kTexFormatARGB32);

	// next mip level, 2x1x1 size
	CHECK(ColorRGBA32(191,191,191,95) == data[12]);
	CHECK(ColorRGBA32(191,95,0,0) == data[13]);

	// next mip level, 1x1x1 size
	CHECK(ColorRGBA32(191,143,95,47) == data[14]);

	// data after that should be untouched
	CHECK(ColorRGBA32(13,13,13,13) == data[15]);
}


} // SUITE

#endif // ENABLE_UNIT_TESTS
