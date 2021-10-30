#include "UnityPrefix.h"
#include "TexturesGL.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Graphics/S3Decompression.h"
#include "UnityGL.h"
#include "GLAssert.h"
#include "TextureIdMapGL.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/VramLimits.h"
#include "Runtime/GfxDevice/TextureUploadUtils.h"


// define to 1 to print lots of texture creation/destroy info
#define DEBUG_GL_TEXTURE 0

static inline bool SupportsS3Compression () { return gGraphicsCaps.hasS3TCCompression; }

const static int kTextureFormatTable[kTexFormatPCCount*4] =
{
	// internal format, internal format sRGB, input format, datatype
	0,			0,						0,			0,
	GL_ALPHA,	GL_ALPHA,				GL_ALPHA,	GL_UNSIGNED_BYTE,				// Alpha8
	GL_RGBA,	GL_RGBA,				GL_BGRA,	GL_UNSIGNED_SHORT_4_4_4_4_REV,	// ARGB4444
	GL_RGB,		GL_SRGB8_EXT,			GL_RGB,		GL_UNSIGNED_BYTE,				// RGB24
	GL_RGBA,	GL_SRGB8_ALPHA8_EXT,	GL_RGBA,	GL_UNSIGNED_BYTE,				// RGBA32
#if UNITY_BIG_ENDIAN
	GL_RGBA,	GL_SRGB8_ALPHA8_EXT,	GL_BGRA,	GL_UNSIGNED_INT_8_8_8_8_REV,	// ARGB32
#else
	GL_RGBA,	GL_SRGB8_ALPHA8_EXT,	GL_BGRA,	GL_UNSIGNED_INT_8_8_8_8,		// ARGB32
#endif
	0,			0,						0,			0,								// ARGBFloat
	GL_RGB,		GL_RGB,					GL_RGB,		GL_UNSIGNED_SHORT_5_6_5,		// RGB565   ///////@TODO: What to do with 16 bit textures?????
	GL_RGB,		GL_SRGB8_EXT,			GL_BGR,		GL_UNSIGNED_BYTE,				// BGR24
	GL_ALPHA16,	GL_ALPHA16,				GL_ALPHA,	GL_UNSIGNED_SHORT,				// AlphaLum16
	GL_COMPRESSED_RGB_S3TC_DXT1_EXT,GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, -1, -1,		// DXT1
	GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, -1, -1,// DXT3
	GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, -1, -1,// DXT5
	GL_RGBA,	GL_RGBA,				GL_RGBA,	GL_UNSIGNED_SHORT_4_4_4_4,		// RGBA4444
};


static void GetUncompressedTextureFormat (int inFormat, bool sRGB, int* internalFormat, int* inputFormat, int* dataType)
{
	DebugAssertIf( IsAnyCompressedTextureFormat(inFormat) );
	*internalFormat = kTextureFormatTable[inFormat*4+(sRGB ? 1 : 0)];
	*inputFormat = kTextureFormatTable[inFormat*4+2];
	*dataType = kTextureFormatTable[inFormat*4+3];
}

static void GetCompressedTextureFormat (int inFormat, bool sRGB, int width, int height, int* internalFormat, int* size)
{
	*internalFormat = kTextureFormatTable[inFormat*4+(sRGB ? 1 : 0)];
	switch (inFormat)
	{
		case kTexFormatDXT1:
		case kTexFormatATC_RGB4:
			*size = ((width + 3) / 4) * ((height + 3) / 4) * 8;
			break;
		case kTexFormatDXT3:
		case kTexFormatDXT5:
		case kTexFormatATC_RGBA8:
			*size = ((width + 3) / 4) * ((height + 3) / 4) * 16;
			break;
		case kTexFormatETC_RGB4:
			*size = 8 * ((width+3)>>2) * ((height+3)>>2); // 8 bytes per 4x4 block
			break;
		default:
			Assert(false && "Texture is not compressed");
	}
}

#if THREADED_LOADING_DEBUG
float totalTextureUploadTime = 0.0F;
double GetTimeSinceStartup();
#endif




void UploadTexture2DGL(
					   TextureID tid, TextureDimension dimension, UInt8* srcData, int width, int height,
					   TextureFormat format, int mipCount, UInt32 uploadFlags, int masterTextureLimit, TextureUsageMode usageMode, TextureColorSpace colorSpace)
{
#if THREADED_LOADING_DEBUG
	double begin = GetTimeSinceStartup();
#endif
	
	AssertIf( srcData == NULL );
	AssertIf( (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)) && !IsNPOTTextureAllowed(mipCount > 1) );
	if( dimension != kTexDim2D )
	{
		ErrorString( "Incorrect texture dimension!" );
		return;
	}
	
	bool uploadIsCompressed, decompressOnTheFly;
	HandleFormatDecompression (format, &usageMode, colorSpace, &uploadIsCompressed, &decompressOnTheFly);
	
	TextureFormat uploadFormat;
	if( decompressOnTheFly )
	{
		uploadFormat = kTexFormatRGBA32;
		uploadIsCompressed = false;
	}
	else
	{
		uploadFormat = format;

		if (usageMode == kTexUsageLightmapDoubleLDR || usageMode == kTexUsageNormalmapPlain)
		{
			// make sure we have a format with alpha
			uploadFormat = kTexFormatRGBA32;
		}
		
		uploadIsCompressed = IsAnyCompressedTextureFormat( format );
	}
	
	int baseLevel, maxLevel, texWidth, texHeight;
	size_t textureSize;
	SkipLevelsForMasterTextureLimit (masterTextureLimit, format, uploadFormat, mipCount, uploadIsCompressed, &srcData, &width, &height, &baseLevel, &maxLevel, &texWidth, &texHeight, &textureSize);
	
	if (!glIsTexture(tid.m_ID))
		uploadFlags |= GfxDevice::kUploadTextureDontUseSubImage;
	
#if DEBUG_GL_TEXTURE
	printf_console( "GLDebug texture: upload %i (%ix%i). Unity format: %d\n", tid, texWidth, texHeight ,format );
#endif
	GLAssert ();
	REGISTER_EXTERNAL_GFX_DEALLOCATION(tid.m_ID);

	TextureIdMapGL_QueryOrCreate(tid);
	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, tid, dimension, std::numeric_limits<float>::infinity());
	
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	
	//	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
	
	UInt8* decompressBuffer = NULL;
	UInt8* tempBuffer = NULL;
	UInt8* scaleBuffer = NULL;
	
	int bufferPitch;
	
	int internalFormat = 0, datatype = 0, inputFormat = 0, size = 0;
	int uploadedSize = 0;
	for( int level = baseLevel; level <= maxLevel; ++level )
	{
		UInt8* feedSourceData;
		const int uploadLevel = level-baseLevel;
		
		if( decompressOnTheFly )
		{
			ConvertCompressedTextureUpload (width, height, format, srcData, decompressBuffer, bufferPitch, usageMode, colorSpace, level);
			feedSourceData = decompressBuffer;
		}
		// Allocate temporary memory and swizzle texture
		else if (uploadFormat != format || usageMode != kTexUsageNone)
		{
			InitImageBuffer (width, height, decompressBuffer, uploadFormat);
			bufferPitch = GetRowBytesFromWidthAndFormat(width, uploadFormat);
			feedSourceData = decompressBuffer;
			
			prcore::Surface srcSurface (width, height, GetRowBytesFromWidthAndFormat (width, format), GetProphecyPixelFormat(format), srcData);
			prcore::Surface dstSurface (width, height, GetRowBytesFromWidthAndFormat (width, uploadFormat), GetProphecyPixelFormat(uploadFormat), feedSourceData);
			
			if (!ConvertUncompressedTextureUpload(srcSurface, dstSurface, prcore::Surface::BLIT_COPY, uploadFormat, usageMode, colorSpace, width, height, feedSourceData, bufferPitch, GetProphecyPixelFormat(uploadFormat), tempBuffer, bufferPitch))
			{
				dstSurface.BlitImage( srcSurface, prcore::Surface::BLIT_COPY );
			}
		}
		// Just feed the data
		else
		{
			feedSourceData = srcData;
		}
		
		// Don't use SubImage when uploading smaller than 4*4 compressed texture
		if( uploadIsCompressed && (texWidth < 4 || texHeight < 4) )
			uploadFlags |= GfxDevice::kUploadTextureDontUseSubImage;
		
		// Now, if we're uploading non-mipmapped texture that is too large, the actual
		// data has to be downscaled.
		UInt8* feedRealData = feedSourceData;
		if( !uploadIsCompressed && (width != texWidth || height != texHeight) ) {
			if( !scaleBuffer )
				scaleBuffer = new UInt8[CalculateImageSize( texWidth, texHeight, uploadFormat )];
			feedRealData = scaleBuffer;
			prcore::Surface srcSurface( width, height, GetRowBytesFromWidthAndFormat(width,uploadFormat), GetProphecyPixelFormat(uploadFormat), feedSourceData );
			prcore::Surface dstSurface( texWidth, texHeight, GetRowBytesFromWidthAndFormat(texWidth,uploadFormat), GetProphecyPixelFormat(uploadFormat), feedRealData );
			dstSurface.BlitImage( srcSurface, prcore::Surface::BLIT_SCALE );
		}
		
		// Complete texture image
		if (uploadFlags & GfxDevice::kUploadTextureDontUseSubImage)
		{
			if (uploadIsCompressed)
			{
				GetCompressedTextureFormat (uploadFormat, colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB, texWidth, texHeight, &internalFormat, &size);
#if DEBUG_GL_TEXTURE
				printf_console( "GLDebug texture: glCompressedTexImage2DARB: level=%i ifmt=%i width=%i height=%i size=%i data=%x\n", uploadLevel, internalFormat, texWidth, texHeight, size, (UInt32)feedRealData );
#endif
				glCompressedTexImage2DARB (GL_TEXTURE_2D, uploadLevel, internalFormat, texWidth, texHeight, 0, size, feedRealData);
			}
			else
			{
				GetUncompressedTextureFormat (uploadFormat, colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB, &internalFormat, &inputFormat, &datatype);
#if DEBUG_GL_TEXTURE
				printf_console( "GLDebug texture: glTexImage2D: level=%i ifmt=%i width=%i height=%i fmt=%i type=%i data=%x\n", uploadLevel, internalFormat, texWidth, texHeight, inputFormat,datatype, (UInt32)feedRealData );
#endif
				glTexImage2D (GL_TEXTURE_2D, uploadLevel, internalFormat, texWidth, texHeight, 0, inputFormat, datatype, feedRealData);
			}
		}
		// Upload texture with subimage
		else
		{
			if (uploadIsCompressed)
			{
				GetCompressedTextureFormat (uploadFormat, colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB, texWidth, texHeight, &internalFormat, &size);
#if DEBUG_GL_TEXTURE
				printf_console( "GLDebug texture: glCompressedTexSubImage2DARB: level=%i width=%i height=%i fmt=%i size=%i data=%x\n", uploadLevel, texWidth, texHeight, internalFormat, size, (UInt32)feedRealData );
#endif
				glCompressedTexSubImage2DARB (GL_TEXTURE_2D, uploadLevel, 0, 0, texWidth, texHeight, internalFormat, size, feedRealData);
			}
			else
			{
				GetUncompressedTextureFormat (uploadFormat, colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB, &internalFormat, &inputFormat, &datatype);
#if DEBUG_GL_TEXTURE
				printf_console( "GLDebug texture: glTexSubImage2D: level=%i width=%i height=%i fmt=%i type=%i data=%x\n", uploadLevel, texWidth, texHeight, inputFormat,datatype, (UInt32)feedRealData );
#endif
				glTexSubImage2D (GL_TEXTURE_2D, uploadLevel, 0, 0, texWidth, texHeight, inputFormat, datatype, feedRealData);
			}
		}
		uploadedSize += CalculateImageSize( texWidth, texHeight, uploadFormat );
		// Go to next mip level
		AssertIf( width == 1 && height == 1 && level != maxLevel );
		AdvanceToNextMipLevel (format, srcData, width, height, texWidth, texHeight);
	}
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(tid.m_ID,uploadedSize,tid.m_ID);
	AssertIf( baseLevel > maxLevel );
	
	GLAssert ();
	
	delete[] decompressBuffer;
	delete[] tempBuffer;
	delete[] scaleBuffer;
	
	//	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
	
#if THREADED_LOADING_DEBUG
	totalTextureUploadTime += GetTimeSinceStartup() - begin;
	printf_console("Total Texture upload time %f\n", totalTextureUploadTime);
#endif
}


void UploadTextureSubData2DGL(
							  TextureID tid, UInt8* srcData,
							  int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace )
{
	AssertIf( IsAnyCompressedTextureFormat( format ) );

	GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(tid);

	Assert(targetTex != 0);
	if(targetTex == 0)
		return;
	
	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, tid, kTexDim2D, std::numeric_limits<float>::infinity());
	
	TextureFormat uploadFormat = format;

	UInt8* decompressBuffer = NULL;
	
	UInt8* feedData = srcData;
	// Allocate temporary memory and swizzle texture
	if( uploadFormat != format )
	{
		int decompressedSize = CalculateImageSize (width, height, uploadFormat);
		if (decompressBuffer == NULL)
			decompressBuffer = new UInt8[decompressedSize];
		feedData = decompressBuffer;
		
		ImageReference src (width, height, GetRowBytesFromWidthAndFormat (width, format), format, srcData);
		ImageReference dst (width, height, GetRowBytesFromWidthAndFormat (width, uploadFormat), uploadFormat, feedData);
		dst.BlitImage( src );
	}
	
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	
	int internalFormat, datatype, inputFormat;
	GetUncompressedTextureFormat (uploadFormat, colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB, &internalFormat, &inputFormat, &datatype);
	glTexSubImage2D( GL_TEXTURE_2D, mipLevel, x, y, width, height, inputFormat, datatype, feedData );
	
	GLAssert ();
	
	if( decompressBuffer )
		delete[] decompressBuffer;
}


void UploadTextureCubeGL(
						 TextureID tid, UInt8* srcData, int faceDataSize, int size,
						 TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace )
{
	if (!glIsTexture(tid.m_ID))
		uploadFlags |= GfxDevice::kUploadTextureDontUseSubImage;
	REGISTER_EXTERNAL_GFX_DEALLOCATION(tid.m_ID);

	TextureIdMapGL_QueryOrCreate(tid);
	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, tid, kTexDimCUBE, std::numeric_limits<float>::infinity());
	
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	
	// We have a compressed format but the hardware doesnt support it.
	// - setup format to be uncompressed
	// - dont use client storage (we are using temporary memory to store the texture)
	TextureFormat uploadFormat;
	bool uploadIsCompressed;
	const bool decompressOnTheFly = IsAnyCompressedTextureFormat(format) && !gGraphicsCaps.hasS3TCCompression;
	if( decompressOnTheFly )
	{
		uploadFormat = kTexFormatRGBA32;
		uploadIsCompressed = false;
	}
	else
	{
		uploadFormat = format;
		uploadIsCompressed = IsAnyCompressedTextureFormat( format );
	}
	
	UInt8* decompressBuffer = NULL;
	
	const GLenum faces[6] =
	{
		GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB
	};
	
	int maxLevel = mipCount - 1;
	int uploadSize = 0;
	for (int face=0;face<6;face++)
	{
		int mipSize = size;
		UInt8* data = srcData + face * faceDataSize;
		int internalFormat = 0, datatype = 0, inputFormat = 0, size = 0;
		
		for( int level=0;level<=maxLevel;level++ )
		{
			UInt8* feedData;
			
			// Allocate temporary memory and decompress texture
			if( decompressOnTheFly )
			{
				int dstSize = std::max( mipSize, 4 );
				int decompressedSize = CalculateImageSize( dstSize, dstSize, uploadFormat );
				if( decompressBuffer == NULL )
					decompressBuffer = new UInt8[decompressedSize];
				feedData = decompressBuffer;
				
				DecompressNativeTextureFormat( format, mipSize, mipSize, (UInt32*)data, dstSize, dstSize, (UInt32*)feedData );
			}
			// Allocate temporary memory and swizzle texture
			else if( uploadFormat != format )
			{
				int decompressedSize = CalculateImageSize (mipSize, mipSize, uploadFormat);
				if (decompressBuffer == NULL)
					decompressBuffer = new UInt8[decompressedSize];
				feedData = decompressBuffer;
				
				ImageReference src (mipSize, mipSize, GetRowBytesFromWidthAndFormat (mipSize, format), format, data);
				ImageReference dst (mipSize, mipSize, GetRowBytesFromWidthAndFormat (mipSize, uploadFormat), uploadFormat, feedData);
				dst.BlitImage( src );
			}
			// Just feed the data
			else
			{
				feedData = data;
			}
			
			// Upload
			if( uploadIsCompressed )
			{
				GetCompressedTextureFormat( uploadFormat, colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB, mipSize, mipSize, &internalFormat, &size );
				glCompressedTexImage2DARB (faces[face], level, internalFormat, mipSize, mipSize, 0, size, feedData);
			}
			else
			{
				GetUncompressedTextureFormat( uploadFormat, colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB, &internalFormat, &inputFormat, &datatype );
				glTexImage2D (faces[face], level, internalFormat, mipSize, mipSize, 0, inputFormat, datatype, feedData);
			}
			
			GLAssert ();
			int levelSize = CalculateImageSize( mipSize, mipSize, format );
			uploadSize += levelSize;
			data += levelSize;
			AssertIf( mipSize == 1 && level != maxLevel );
			
			mipSize = std::max( mipSize / 2, 1 );
		}
	}
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(tid.m_ID,uploadSize,tid.m_ID);
	if( decompressBuffer )
		delete[] decompressBuffer;
}


void UploadTexture3DGL(
					   TextureID tid, UInt8* srcData, int width, int height, int depth,
					   TextureFormat format, int mipCount, UInt32 uploadFlags )
{
	if (!gGraphicsCaps.has3DTexture)
		return;
	
	if (!glIsTexture(tid.m_ID))
		uploadFlags |= GfxDevice::kUploadTextureDontUseSubImage;
	
#if DEBUG_GL_TEXTURE
	printf_console( "GLDebug texture 3D: upload %i (%ix%ix%i)\n", tid, width, height, depth );
#endif
	REGISTER_EXTERNAL_GFX_DEALLOCATION(tid.m_ID);

	TextureIdMapGL_QueryOrCreate(tid);
	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, tid, kTexDim3D, std::numeric_limits<float>::infinity());
	
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	
	int internalFormat, datatype, inputFormat;
	int maxLevel = mipCount - 1;
	int bpp = GetBytesFromTextureFormat(format);
	int uploadSize = 0;
	
	for (int level=0;level<=maxLevel;level++)
	{
		GetUncompressedTextureFormat (format, false, &internalFormat, &inputFormat, &datatype);
#if DEBUG_GL_TEXTURE
		printf_console( "GLDebug texture 3D: glTexImage3D: level=%i ifmt=%i width=%i height=%i depth=%i fmt=%i type=%i data=%x\n", level, internalFormat, width, height, depth, inputFormat,datatype, (UInt32)srcData );
#endif
		glTexImage3D (GL_TEXTURE_3D, level, internalFormat, width, height, depth, 0, inputFormat, datatype, srcData);
		GLAssert ();
		uploadSize += width*height*depth*bpp;
		
		// Go to next level
		srcData += CalculateImageSize(width, height, format) * depth;
		width = std::max(width / 2, 1);
		height = std::max(height / 2, 1);
		depth = std::max(depth / 2, 1);
	}
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(tid.m_ID,uploadSize,tid.m_ID);
}


void GLReadPixelsWrapper (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);

bool ReadbackTextureGL( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY )
{
	int internalFormat, datatype, inputFormat;
	GetUncompressedTextureFormat( image.GetFormat(), false, &internalFormat, &inputFormat, &datatype );
	
	// The whole image we're reading into can be larger than the rect we read.
	// So setup the alignment.
	glPixelStorei( GL_PACK_ROW_LENGTH, image.GetRowBytes() / GetBytesFromTextureFormat(image.GetFormat()) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	
	switch( image.GetFormat() ) {
		case kTexFormatARGB32:
			GLReadPixelsWrapper(left,bottom,width,height,inputFormat,datatype,image.GetRowPtr(destY) + destX * 4);
			break;
		case kTexFormatRGB24:
			GLReadPixelsWrapper(left,bottom,width,height,inputFormat,datatype,image.GetRowPtr(destY) + destX * 3);
			break;
		case kTexFormatAlpha8:
			GLReadPixelsWrapper(left,bottom,width,height,inputFormat,datatype,image.GetRowPtr(destY) + destX);
			break;
		default:
			AssertString ("Not Supported");
			glPixelStorei( GL_PACK_ROW_LENGTH, 0 );
			return false;
	}
	
	glPixelStorei( GL_PACK_ROW_LENGTH, 0 );
	
	return true;
}
