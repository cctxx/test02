#ifndef IMAGE_H
#define IMAGE_H

#include "TextureFormat.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Allocator/MemoryMacros.h"

class Image;
class ColorRGBA32;
class ColorRGBAf;
class ImageReference;
namespace prcore { class PixelFormat; }

prcore::PixelFormat GetProphecyPixelFormat (TextureFormat format);


// Inside image, leaves sizeX x sizeY portion untouched, fills the rest by
// repeating border pixels.
void PadImageBorder( ImageReference& image, int sizeX, int sizeY );

// Copies source compressed image into (possibly larger) destination compressed image.
void BlitCopyCompressedImage( TextureFormat format, const UInt8* src, int srcWidth, int srcHeight, UInt8* dst, int dstWidth, int dstHeight, bool fillRest );
void BlitCopyCompressedDXT1ToDXT5( const UInt8* src, int srcWidth, int srcHeight, UInt8* dst, int dstWidth, int dstHeight );

// Generates a mipmap chain
void CreateMipMap (UInt8* inData, int width, int height, int depth, TextureFormat format);

// Calculate the amount of mipmaps that can be created from an image of given size.
// (The original image is counted as mipmap also)
int CalculateMipMapCount3D (int width, int height, int depth);

int CalculateImageSize (int width, int height, TextureFormat format);
int CalculateImageMipMapSize (int width, int height, TextureFormat format);
int CalculateMipMapOffset (int width, int height, TextureFormat format, int miplevel);

void Premultiply( ImageReference& image );

// Range of RGBM lightmaps is [0;8]
enum { kRGBMMaxRange = 8 };
void DecodeRGBM (int width, int height, UInt8* data, int pitch, const prcore::PixelFormat& pf);
void SetAlphaChannel (int width, int height, UInt8* data, int pitch, const prcore::PixelFormat& pf, UInt8 alpha);
void SetAlphaToRedChannel (int width, int height, UInt8* data, int pitch, const prcore::PixelFormat& pf);
void XenonToNormalSRGBTexture (int width, int height, UInt8* data, int pitch, const prcore::PixelFormat& pf);

#if UNITY_XENON
enum { kImageDataAlignment = 4096 };
#else
enum { kImageDataAlignment = kDefaultMemoryAlignment };
#endif

class ImageReference
{
protected:
	UInt32	m_Format;
	SInt32	m_Width;
	SInt32	m_Height;
	SInt32	m_RowBytes;
	UInt8*	m_Image;
	
public: 
	
	enum ClearMode
	{
		CLEAR_COLOR			= 1,
		CLEAR_ALPHA			= 2,
		CLEAR_COLOR_ALPHA	= CLEAR_COLOR | CLEAR_ALPHA
	};

	enum BlitMode
	{
		BLIT_COPY,
		BLIT_SCALE,
		BLIT_BILINEAR_SCALE,
	};

	ImageReference () { m_Image = NULL; m_Width = 0; m_Height = 0; m_Format = 0; }
	ImageReference (int width, int height, int rowbytes, TextureFormat format, void* image);	
	ImageReference (int width, int height, TextureFormat format);
	
	// Returns true if the image is exactly the same by camping width, height, and image data
	friend bool operator == (const ImageReference& lhs, const ImageReference& rhs);
	
	// Returns a subpart of the image
	ImageReference ClipImage (int x, int y, int width, int height) const;
	
	UInt8* GetImageData () const		{ return m_Image; }
	int GetRowBytes () const			{ return m_RowBytes; }
	UInt8* GetRowPtr (int y) const 		{ DebugAssertIf(y >= m_Height || y < 0); return m_Image + m_RowBytes * y; }
	int GetWidth () const 				{ return m_Width; }
	int GetHeight () const				{ return m_Height; }
	TextureFormat GetFormat() const		{ return (TextureFormat)m_Format; }
	
	void BlitImage (const ImageReference& source, BlitMode mode = BLIT_COPY);
	void BlitImage (int x, int y, const ImageReference& source);
	
	void ClearImage (const ColorRGBA32& color, ClearMode mode = CLEAR_COLOR_ALPHA);
	void FlipImageY ();
	#if UNITY_EDITOR
	void FlipImageX ();
	#endif
	
	bool IsValidImage () const;

	bool NeedsReformat(int width, int height, TextureFormat format) const;
};

class Image : public ImageReference
{
public:
	
	DECLARE_SERIALIZE (Image)
	
	Image (int width, int height, TextureFormat format);
	Image (int width, int height, int rowbytes, TextureFormat format, void* image);
	Image () { }

	// TODO : should be removed, because they implicitly have worse performance - use SetImage instead
	Image (const Image& image) : ImageReference() { SetImage (image); }	
	Image (const ImageReference& image) { SetImage (image); }	

	// TODO : should be removed, because they implicitly have worse performance - use SetImage instead
	void operator = (const ImageReference& image) { SetImage (image); }
	void operator = (const Image& image) { SetImage (image); }
	
	~Image () { UNITY_FREE(kMemNewDelete, m_Image); }

	// Reformats given image into itself
	void ReformatImage (const ImageReference& image, int width, int height, TextureFormat format, BlitMode mode = BLIT_COPY);
	// Reformats itself
	void ReformatImage (int width, int height, TextureFormat format, BlitMode mode = BLIT_COPY);

	// To initialize an image use: someImage = ImageReference (width, height, format);

	// shrinkAllowed - controls if memory should be reallocated when Image needs less memory,
	// if shrinkAllowed is set to false it won't reallocate memory if Image needs less memory
	void SetImage(const ImageReference& src, bool shrinkAllowed = true);	
	void SetImage(SInt32 width, SInt32 height, UInt32 format, bool shrinkAllowed);
};

bool CheckImageFormatValid (int width, int height, TextureFormat format);


extern const char* kUnsupportedGetPixelOpFormatMessage;
extern const char* kUnsupportedSetPixelOpFormatMessage;
void SetImagePixel (ImageReference& image, int x, int y, TextureWrapMode wrap, const ColorRGBAf& color);
ColorRGBA32 GetImagePixel (UInt8* data, int width, int height, TextureFormat format, TextureWrapMode wrap, int x, int y);
ColorRGBAf GetImagePixelBilinear (UInt8* data, int width, int height, TextureFormat format, TextureWrapMode wrap, float u, float v);
bool GetImagePixelBlock (UInt8* data, int dataWidth, int dataHeight, TextureFormat format, int x, int y, int blockWidth, int blockHeight, ColorRGBAf* outColors);
bool SetImagePixelBlock (UInt8* data, int dataWidth, int dataHeight, TextureFormat format, int x, int y, int blockWidth, int blockHeight, int pixelCount, const ColorRGBAf* pixels);

void SwizzleARGB32ToBGRA32 (UInt8* bytes, int imageSize);
void SwizzleRGBA32ToBGRA32 (UInt8* bytes, int imageSize);
void SwizzleRGB24ToBGR24 (UInt8* bytes, int imageSize);
void SwizzleBGRAToRGBA32 (UInt8* bytes, int imageSize);

template<class TransferFunction> inline
void Image::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_Format);
	TRANSFER (m_Width);
	TRANSFER (m_Height);
	TRANSFER (m_RowBytes);
	
	unsigned completeImageSize = m_RowBytes * std::max<int>(m_Height, 0);

	transfer.TransferTypeless (&completeImageSize, "image data");
	if (transfer.IsReading ())
	{
		UNITY_FREE(kMemNewDelete, m_Image);
		m_Image = NULL;
		if (completeImageSize != 0 && CheckImageFormatValid (m_Width, m_Height, m_Format))
			m_Image = (UInt8*)UNITY_MALLOC_ALIGNED(kMemNewDelete, completeImageSize, kImageDataAlignment);
	}
	
	transfer.TransferTypelessData (completeImageSize, m_Image);
}

#endif
