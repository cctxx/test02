#include "UnityPrefix.h"
#include "ImageConverter.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/LogAssert.h"

#if UNITY_WIN
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#endif

#include <FreeImage.h>

static bool FIBITMAPToImage (FIBITMAP* srcImage, std::auto_ptr<FreeImageWrapper>& image, bool premultiplyAlpha);

/** 
 FreeImage error handler 
 @param fif Format / Plugin responsible for the error 
 @param message Error message 
 */ 
void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) { 
	printf_console("\n*** "); 
	if(fif != FIF_UNKNOWN) { 
		printf_console("%s Format\n", FreeImage_GetFormatFromFIF(fif)); 
	} 
	printf_console("%s", message);
	printf_console(" ***\n"); 
}

bool SaveAndUnload(FREE_IMAGE_FORMAT dstFormat, FIBITMAP* dst, const std::string& inPath)
{
	bool wasSaved = false;

	#if UNITY_WIN
	std::wstring widePath;
	ConvertUnityPathName( inPath, widePath );
	wasSaved = FreeImage_SaveU( dstFormat, dst, widePath.c_str());
	#else
	wasSaved = FreeImage_Save( dstFormat, dst, inPath.c_str() );
	#endif
	FreeImage_Unload( dst );

	return wasSaved;
}

bool SaveImageToFile (UInt8* inData, int width, int height, TextureFormat format, const std::string& inPath, long fileType)
{	
	bool hasAlpha = format == kTexFormatRGBA32;
	if (format != kTexFormatRGB24 && format != kTexFormatRGBA32)
	{
		ErrorString("Image format is not supported");
		return false;
	}
	const int bytes = hasAlpha ? 4 : 3;
	const int pitch = width * bytes;
	
	// Aras: Stupid FreeImage behaves differently depending on the platform!
	// Yes, even on Windows we want RGB images, not BGR
	#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR
	for( int i = 0; i < width*height; ++i )
	{
		UInt8 t = inData[i*bytes+0];
		inData[i*bytes+0] = inData[i*bytes+2];
		inData[i*bytes+2] = t;
	}
	#endif

	FIBITMAP* dst = FreeImage_ConvertFromRawBits (inData, width, height, pitch,
	                                              hasAlpha ? 32 : 24,
	                                              FI_RGBA_RED_MASK,
	                                              FI_RGBA_GREEN_MASK,
	                                              FI_RGBA_BLUE_MASK);

	FREE_IMAGE_FORMAT dstFormat = FIF_PNG;
	switch( fileType )
	{
	case 'TIFF':
		dstFormat = FIF_TIFF;
		break;
	case 'BMPf':
		dstFormat = FIF_BMP;
		break;
	case 'PPMf':
		dstFormat = FIF_PPMRAW;
		break;
	case 'TGAf':
		dstFormat = FIF_TARGA;
		break;
	}

	return SaveAndUnload( dstFormat, dst, inPath );
}

bool SaveHDRImageToFile (float* inData, int width, int height, const std::string& inPath)
{	
	// allocate new RGBF image
	FIBITMAP* dst = FreeImage_AllocateT( FIT_RGBF, width, height );

	BYTE* bits = (BYTE*)FreeImage_GetBits( dst );
	UInt32 pitch = FreeImage_GetPitch( dst );

	for ( int y = 0; y < height; y++ ) 
	{
		FIRGBF *pixel = (FIRGBF*)bits;
		for ( int x = 0; x < width; x++ )
		{
			pixel[x].red = *(inData++);
			pixel[x].green = *(inData++);
			pixel[x].blue = *(inData++);
		}
		// next line
		bits += pitch;
	}
	
	FREE_IMAGE_FORMAT dstFormat = FIF_EXR;

	return SaveAndUnload( dstFormat, dst, inPath );
}

FreeImageWrapper::FreeImageWrapper(FIBITMAP* srcImage, int width, int height, UInt32 pitch, TextureFormat textureFormat)
:	m_SrcImage(srcImage), m_Image(width, height, pitch, textureFormat, FreeImage_GetBits(srcImage))
{
	AssertIf(!m_SrcImage);
}

FreeImageWrapper::~FreeImageWrapper()
{
	AssertIf(!m_SrcImage);
	FreeImage_Unload(m_SrcImage);
}

FIBITMAP* ConvertRGBFToARGBF(FIBITMAP* srcImage)
{
	int width = FreeImage_GetWidth (srcImage);
	int height = FreeImage_GetHeight (srcImage);

	// allocate new RGBAF image
	FIBITMAP* dstImage = FreeImage_AllocateT (FIT_RGBAF, width, height);

	BYTE* srcBits = (BYTE*)FreeImage_GetBits (srcImage);
	BYTE* dstBits = (BYTE*)FreeImage_GetBits (dstImage);

	UInt32 srcPitch = FreeImage_GetPitch (srcImage);
	UInt32 dstPitch = FreeImage_GetPitch (dstImage);

	for (int y = 0; y < height; y++)
	{
		FIRGBF* srcPixel = (FIRGBF*)srcBits;
		FIRGBAF* dstPixel = (FIRGBAF*)dstBits;

		for (int x = 0; x < width; x++)
		{
			// Swizzle to ARGB
			dstPixel[x].red		= 1.0f;
			dstPixel[x].green	= srcPixel[x].red;
			dstPixel[x].blue	= srcPixel[x].green;
			dstPixel[x].alpha	= srcPixel[x].blue;
		}

		// next line
		dstBits += dstPitch;
		srcBits += srcPitch;
	}

	return dstImage;
}

void SwizzleRGBAFToARGBF(FIBITMAP* srcImage)
{
	DebugAssert (FreeImage_GetImageType (srcImage) == FIT_RGBAF);

	int width = FreeImage_GetWidth (srcImage);
	int height = FreeImage_GetHeight (srcImage);

	BYTE* srcBits = (BYTE*)FreeImage_GetBits (srcImage);
	UInt32 srcPitch = FreeImage_GetPitch (srcImage);

	for (int y = 0; y < height; y++)
	{
		FIRGBAF* srcPixel = (FIRGBAF*)srcBits;

		for (int x = 0; x < width; x++)
		{
			// Swizzle RGBA to ARGB
			float alpha = srcPixel[x].alpha;
			srcPixel[x].alpha	= srcPixel[x].blue;
			srcPixel[x].blue	= srcPixel[x].green;
			srcPixel[x].green	= srcPixel[x].red;
			srcPixel[x].red		= alpha;
		}

		// next line
		srcBits += srcPitch;
	}
}

bool LoadImageFromBuffer (const UInt8* data, size_t size, std::auto_ptr<FreeImageWrapper>& image, bool premultiplyAlpha)
{
	FIMEMORY* memory = FreeImage_OpenMemory((BYTE*)data, size);
	
	FREE_IMAGE_FORMAT srcFormat = FreeImage_GetFileTypeFromMemory( memory, size );

	if ( srcFormat == FIF_UNKNOWN )
	{
		FreeImage_CloseMemory(memory);
		return false;
	}
	
	FIBITMAP* bitmap = FreeImage_LoadFromMemory(srcFormat, memory);
	bool result = FIBITMAPToImage (bitmap, image, premultiplyAlpha);
	FreeImage_CloseMemory(memory);
	
	return result;
}

bool LoadImageAtPath (const string &path, std::auto_ptr<FreeImageWrapper>& image, bool premultiplyAlpha)
{
	#if UNITY_WIN
	std::wstring widePath;
	ConvertUnityPathName( path, widePath );
	#endif

	FreeImage_SetOutputMessage(FreeImageErrorHandler); 

	#if UNITY_WIN
	FREE_IMAGE_FORMAT srcFormat = FreeImage_GetFileTypeU( widePath.c_str() );
	#else
	FREE_IMAGE_FORMAT srcFormat = FreeImage_GetFileType( path.c_str() );
	#endif
	if ( srcFormat == FIF_UNKNOWN )
		return false;

	#if UNITY_WIN
	FIBITMAP* srcImage = FreeImage_LoadU( srcFormat, widePath.c_str() );
	#else
	FIBITMAP* srcImage = FreeImage_Load( srcFormat, path.c_str() );
	#endif
	
	return FIBITMAPToImage(srcImage, image, premultiplyAlpha);
}

/// Converts srcImage to ARGBF (if the source was in FIT_RGBF or FIT_RGBAF format) or ARGB32 (otherwise).
static bool FIBITMAPToImage (FIBITMAP* srcImage, std::auto_ptr<FreeImageWrapper>& image, bool premultiplyAlpha)
{
	DebugAssert (FreeImage_IsLittleEndian ());

	if (!srcImage)
		return false;

	FREE_IMAGE_TYPE srcType = FreeImage_GetImageType (srcImage);

	int width = FreeImage_GetWidth (srcImage);
	int height = FreeImage_GetHeight (srcImage);

	UInt32 pitch = 0;
	TextureFormat textureFormat = 0;

	if (srcType == FIT_RGBF || srcType == FIT_RGBAF)
	{
		if (srcType == FIT_RGBF)
		{
			FIBITMAP* prevImage = srcImage;

			srcImage = ConvertRGBFToARGBF (srcImage);
			srcType = FIT_RGBAF;

			FreeImage_Unload (prevImage);
		}
		else if (srcType == FIT_RGBAF)
		{
			SwizzleRGBAFToARGBF (srcImage);
		}

		pitch = FreeImage_GetPitch (srcImage);
		textureFormat = kTexFormatARGBFloat;
	}
	else
	{
		if (premultiplyAlpha)
		{
			if (!FreeImage_PreMultiplyWithAlpha (srcImage))
			{
				FreeImage_Unload (srcImage);
				return false;
			}
		}
		if (FreeImage_GetBPP (srcImage) != 32)
		{
			FIBITMAP* prevImage = srcImage;
			srcImage = FreeImage_ConvertTo32Bits (srcImage);
			FreeImage_Unload (prevImage);
		
			if (!srcImage)
				return false;
		}
				
		BYTE* bits = (BYTE*)FreeImage_GetBits (srcImage);
		pitch = FreeImage_GetPitch (srcImage);

		// Must be in ARGB format (increasing byte-order)
		for ( int y = 0; y < height; y++ )
		{
			DWORD* pixel = (DWORD*)bits;
			for (int x = 0; x < width; x++)
			{
				DWORD c = pixel[ x ];
				pixel[ x ] = (((c & FI_RGBA_ALPHA_MASK) >> FI_RGBA_ALPHA_SHIFT) << 0)
							+ (((c & FI_RGBA_RED_MASK) >> FI_RGBA_RED_SHIFT) << 8)
							+ (((c & FI_RGBA_GREEN_MASK) >> FI_RGBA_GREEN_SHIFT) << 16)
							+ (((c & FI_RGBA_BLUE_MASK) >> FI_RGBA_BLUE_SHIFT) << 24);
			}
			
			bits += pitch;
		}

		textureFormat = kTexFormatARGB32;
	}

	// image is taking ownership of srcImage
	image.reset (new FreeImageWrapper (srcImage, width, height, pitch, textureFormat));

	return true;
}
