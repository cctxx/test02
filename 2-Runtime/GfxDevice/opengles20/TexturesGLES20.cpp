#include "UnityPrefix.h"
#include "TexturesGLES20.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/TextureUploadUtils.h"
#include "Runtime/Graphics/S3Decompression.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Utilities/BitUtility.h"
#include "IncludesGLES20.h"
#include "AssertGLES20.h"
#include "DebugGLES20.h"
#include "TextureIdMapGLES20.h"


#if GFX_SUPPORTS_OPENGLES20

// these are used to fill in table without using too much width
#define GL_ETC1			GL_ETC1_RGB8_OES
#define GL_DXT1			GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_DXT3			GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_DXT5			GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_SRGBDXT1		GL_COMPRESSED_SRGB_S3TC_DXT1_NV
#define GL_SRGBDXT3		GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV
#define GL_SRGBDXT5		GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV
#define GL_PVR1RGB2		GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG
#define GL_PVR1RGBA2	GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG
#define GL_PVR1RGB4		GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG
#define GL_PVR1RGBA4	GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG
#define GL_ATC4			GL_ATC_RGB_AMD
#define GL_ATC8			GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD

#define RESERVED_FORMAT	-1,-1,-1,0,

const static int kTextureFormatTable[kTexFormatTotalCount*4] =
{
	// for GLES format == internal format, so we keep only one
	// format, 		srgb format, 		datatype, 						alternative format
	0,				0,					0,								0,
	GL_ALPHA,		GL_ALPHA,			GL_UNSIGNED_BYTE,				0,					// Alpha8
	GL_RGBA,		GL_RGBA,			GL_UNSIGNED_SHORT_4_4_4_4,		0,					// ARGB4444
	GL_RGB,			GL_SRGB_EXT,		GL_UNSIGNED_BYTE,				0,					// RGB24
	GL_RGBA,		GL_SRGB_ALPHA_EXT,	GL_UNSIGNED_BYTE,				0,					// RGBA32
	-1,				-1,					-1,								kTexFormatRGBA32,	// ARGB32
	GL_RGBA,		GL_SRGB_ALPHA_EXT,	GL_HALF_FLOAT_OES,				kTexFormatRGBA32,	// ARGBFloat
	GL_RGB,			GL_RGB,				GL_UNSIGNED_SHORT_5_6_5,		0,					// RGB565
	-1,				-1,					-1,								kTexFormatRGB24,	// BGR24
	-1,				-1,					-1,								kTexFormatAlpha8,	// AlphaLum16
	GL_DXT1,		GL_SRGBDXT1,		-1,								0,					// DXT1
	GL_DXT3,		GL_SRGBDXT3,		-1,								0,					// DXT3
	GL_DXT5,		GL_SRGBDXT5,		-1,								0,					// DXT5
	GL_RGBA,		GL_RGBA,			GL_UNSIGNED_SHORT_4_4_4_4,		0,					// RGBA4444

	RESERVED_FORMAT /*14*/	RESERVED_FORMAT /*15*/	RESERVED_FORMAT /*16*/	RESERVED_FORMAT /*17*/
	RESERVED_FORMAT /*18*/ 	RESERVED_FORMAT /*19*/	RESERVED_FORMAT /*20*/	RESERVED_FORMAT /*21*/
	RESERVED_FORMAT /*22*/	RESERVED_FORMAT /*23*/	RESERVED_FORMAT /*24*/	RESERVED_FORMAT /*25*/
	RESERVED_FORMAT /*26*/	RESERVED_FORMAT /*27*/ 	RESERVED_FORMAT /*28*/	RESERVED_FORMAT /*29*/

	GL_PVR1RGB2,	-1,					-1,								0,					// PVRTC_RGB2
	GL_PVR1RGBA2,	-1,					-1,								0,					// PVRTC_RGBA2
	GL_PVR1RGB4,	-1,					-1,								0,					// PVRTC_RGB4
	GL_PVR1RGBA4,	-1,					-1,								0,					// PVRTC_RGBA4
	GL_ETC1,		-1,					-1,								0,					// ETC_RGB4
	GL_ATC4,		-1,					-1,								0,					// ATC_RGB4
	GL_ATC8,		-1,					-1,								0,					// ATC_RGBA8
	GL_BGRA_EXT,	GL_BGRA_EXT,		GL_UNSIGNED_BYTE,				0,					// BGRA32
};

#undef GL_ETC1
#undef GL_DXT1
#undef GL_DXT3
#undef GL_DXT5
#undef GL_PVR1RGB2
#undef GL_PVR1RGBA2
#undef GL_PVR1RGB4
#undef GL_PVR1RGBA4
#undef GL_ATC4
#undef GL_ATC8
#undef RESERVED_FORMAT


static int IsSupportedTextureFormat(TextureFormat inFormat)
{
	return (kTextureFormatTable[inFormat*4+0] > 0) && gGraphicsCaps.supportsTextureFormat[inFormat];
}

static int RemapToAlternativeFormat (TextureFormat inFormat)
{
	int altFormat = kTextureFormatTable[inFormat*4+3];
	return altFormat > 0 ? altFormat : kTexFormatRGBA32;
}

static void GetUncompressedTextureFormat (int inFormat, bool srgb, int* format, int* dataType)
{
	DebugAssertIf( IsAnyCompressedTextureFormat(inFormat) );
	*format   = kTextureFormatTable[inFormat*4 + (srgb?1:0)];
	*dataType = kTextureFormatTable[inFormat*4 + 2];
}

static void GetCompressedTextureFormat (int inFormat, bool srgb, int width, int height, int* internalFormat, int* size)
{
	// srgb support will be enabled only if compressed srgb is supported
	// also we will ever use only supported formats
	*internalFormat = kTextureFormatTable[inFormat*4 + (srgb?1:0)];
	Assert(*internalFormat != -1);

	switch(inFormat)
	{
	case kTexFormatPVRTC_RGB4:
	case kTexFormatPVRTC_RGBA4:
		*size = (std::max(width, 8) * std::max(height, 8) * 4 + 7) / 8;
		break;
	case kTexFormatPVRTC_RGB2:
	case kTexFormatPVRTC_RGBA2:
		*size =  (std::max(width, 16) * std::max(height, 8) * 2 + 7) / 8;
		break;
	case kTexFormatDXT1:
	case kTexFormatATC_RGB4:
		*size = ((width + 3) / 4) * ((height + 3) / 4) * 8;
		break;
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

static TextureFormat GetUploadFormat(TextureFormat format, bool& uploadIsCompressed, bool& decompressOnTheFly)
{
	TextureFormat uploadFormat = format;
	uploadIsCompressed = IsAnyCompressedTextureFormat(format);
	decompressOnTheFly = false;
	if (!IsSupportedTextureFormat(format))
	{
		if( uploadIsCompressed )
		{
			uploadIsCompressed = false;
			decompressOnTheFly = true;
		}

		uploadFormat = RemapToAlternativeFormat(format);
	}

	if( decompressOnTheFly )
	{
		if (IsCompressedPVRTCTextureFormat(format))
		{
			printf_console ("WARNING: PVRTC texture format is not supported, decompressing texture\n");
		}
		else if (IsCompressedDXTTextureFormat(format))
		{
			printf_console ("WARNING: DXT texture format is not supported, decompressing texture\n");
		}
		else if (IsCompressedETCTextureFormat(format))
		{
			printf_console ("WARNING: ETC texture format is not supported, decompressing texture\n");
		}
		else if (IsCompressedATCTextureFormat(format))
		{
			printf_console ("WARNING: ATC texture format is not supported, decompressing texture\n");
		}
	}

	return uploadFormat;
}

#if UNITY_PEPPER
// Unity generates textures as ARGB4444, but GLES only supports RGBA4444.
// On mobile GLES, we swizzle textures at build time, but on NaCl, we use the same data format as used
// in the normal web player, so we cannot do that here. So we swizzle at runtime.
// TODO for 4.0: See if there is any good reason to use ARGB4444 at all, or if we should switch to RGBA4444 completely!
UInt8 *SwizzleRGBA4444 (UInt8 *srcData, UInt8 **decompressBuffer, int width, int height)
{
	if( *decompressBuffer == NULL )
		*decompressBuffer = new UInt8[width * height * 2];

	UInt8 *feedData = *decompressBuffer;
	for (int q = 0; q < width * height; ++q)
	{
		// RGBA4444 <- ARGB4444
		UInt32 argb = *(UInt16*)(srcData + q * 2);
		UInt16 rgba = (argb << 4) | (0x0F & (argb >> 12));

		feedData[q * 2 + 0] = (UInt8)rgba;
		feedData[q * 2 + 1] = (UInt8)(rgba >> 8);
	}
	return feedData;
}
#endif

void UploadTexture2DGLES2(
	TextureID tid, TextureDimension dimension, UInt8* srcData, int width, int height,
	TextureFormat format, int mipCount, UInt32 uploadFlags, int masterTextureLimit, TextureColorSpace colorSpace )
{
	Assert( srcData != NULL );
	AssertIf( (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)) && !IsNPOTTextureAllowed(mipCount > 1) );
	if( dimension != kTexDim2D )
	{
		ErrorStringMsg( "Incorrect texture dimension! (dimension = %d)", dimension );
		return;
	}

	bool uploadIsCompressed, decompressOnTheFly;
	TextureFormat uploadFormat = GetUploadFormat(format, uploadIsCompressed, decompressOnTheFly);

	DBG_TEXTURE_VERBOSE_GLES20("Texture2D #%i (%ix%i) unityFmt: %d, uploadFmt: %d, compressed: %d, decompressOnCpu: %d",
							   tid.m_ID, width, height, format, uploadFormat, (int)uploadIsCompressed, (int)decompressOnTheFly);
	REGISTER_EXTERNAL_GFX_DEALLOCATION(tid.m_ID);

	TextureIdMapGLES20_QueryOrCreate(tid);
	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, tid, dimension, std::numeric_limits<float>::infinity());
	GLES_CHK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

	int maxLevel = mipCount - 1;
	int baseLevel = std::min( masterTextureLimit, maxLevel );

#if UNITY_IPHONE && GL_APPLE_texture_max_level
	if( gGraphicsCaps.hasMipMaxLevel )
	{
		GLES_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL_APPLE, maxLevel));
	}
#endif

	// xenon, for real?
	bool isSRGB = (colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB);

	UInt8* decompressBuffer = NULL;
	int glesFormat, datatype;
	int uploadedSize = 0;
	int skippedMipCount = 0;
	for( int level = 0; level <= maxLevel; ++level )
	{
		UInt8* feedData;
		int uploadLevel = level - baseLevel - skippedMipCount;

		// Should this level be skipped because of master texture limit?
		if( level < baseLevel )
		{
			feedData = NULL;
		}
		// Should this level be skipped because it is too large for hardware limits?
		else if (width > gGraphicsCaps.maxTextureSize || height > gGraphicsCaps.maxTextureSize)
		{
			skippedMipCount++;
			DBG_TEXTURE_VERBOSE_GLES20("Texture2D #%i mip level %i (%ix%i) is too big, skipping!", tid.m_ID, level, width, height);
			feedData = NULL;
		}
		// Allocate temporary memory and decompress texture
		else if( decompressOnTheFly )
		{
			Assert(uploadFormat == kTexFormatRGBA32);

			int dstWidth = std::max( width, 4 );
			int dstHeight = std::max( height, 4 );
			int decompressedSize = CalculateImageSize( dstWidth, dstHeight, uploadFormat );
			if( decompressBuffer == NULL )
				decompressBuffer = new UInt8[decompressedSize];
			feedData = decompressBuffer;
			DecompressNativeTextureFormat ( format, width, height, (UInt32*)srcData, dstWidth, dstHeight, (UInt32*)feedData );

			if (UNITY_IPHONE && (format == kTexFormatDXT3 || format == kTexFormatDXT5))
			{
				printf_console ("swapping DXT channels\n");

				for (int q = 0; q < width * height; ++q)
				{
					std::swap(feedData[q * 4 + 0], feedData[q * 4 + 3]);
					std::swap(feedData[q * 4 + 1], feedData[q * 4 + 2]);
				}
			}
		}
		// Allocate temporary memory and swizzle texture
		else if (uploadFormat != format)
		{
			int decompressedSize = CalculateImageSize (width, height, uploadFormat);
			if (decompressBuffer == NULL)
				decompressBuffer = new UInt8[decompressedSize];
			feedData = decompressBuffer;

			ImageReference src (width, height, GetRowBytesFromWidthAndFormat (width, format), format, srcData);
			ImageReference dst (width, height, GetRowBytesFromWidthAndFormat (width, uploadFormat), uploadFormat, feedData);
			dst.BlitImage( src );
		}
#if UNITY_PEPPER
		else if (uploadFormat == kTexFormatARGB4444)
		{
			feedData = SwizzleRGBA4444 (srcData, &decompressBuffer, width, height);
		}
#endif
		else
		{
			// Just feed the data
			feedData = srcData;
		}

		// If this level should be skipped, just do nothing
		if( feedData == NULL )
		{
		}
		else
		{
			if (uploadIsCompressed)
			{
				int size;
				GetCompressedTextureFormat (uploadFormat, isSRGB, width, height, &glesFormat, &size);
				if( glesFormat <= 0 )
				{
					ErrorString(Format("Format not supported: %u!", (unsigned)uploadFormat));
					return;
				}

				DBG_TEXTURE_VERBOSE_GLES20("GLESDebug texture: glCompressedTexImage2D: level=%i fmt=%i width=%i height=%i type=%i data=%x",
										   uploadLevel, glesFormat, width, height, datatype, (UInt32)feedData);
				GLES_CHK(glCompressedTexImage2D (GL_TEXTURE_2D, uploadLevel, glesFormat, width, height, 0, size, feedData));
			}
			else
			{
				GetUncompressedTextureFormat (uploadFormat, isSRGB, &glesFormat, &datatype);
				if( glesFormat <= 0 )
				{
					ErrorString(Format("Format not supported: %u!", (unsigned)uploadFormat));
					return;
				}

				int internalFormat = glesFormat;
			#if UNITY_IPHONE
				//APPLE_texture_format_BGRA8888 dictates internal format for GL_BGRA_EXT to be GL_RGBA
				if(glesFormat == GL_BGRA_EXT)
					internalFormat = GL_RGBA;
			#endif

				DBG_TEXTURE_VERBOSE_GLES20("GLESDebug texture: glTexImage2D: level=%i fmt=%i width=%i height=%i type=%i data=%x",
										   uploadLevel, glesFormat, width, height, datatype, (UInt32)feedData);
				GLES_CHK(glTexImage2D (GL_TEXTURE_2D, uploadLevel, internalFormat, width, height, 0, glesFormat, datatype, feedData));
			}
		}
		if( gGraphicsCaps.gles20.needFlushAfterTextureUpload )
			GLES_CHK(glFlush());

		// Go to next mip level
		int levelSize = CalculateImageSize (width, height, format);;
		uploadedSize += levelSize;
		srcData += levelSize;
		AssertIf( width == 1 && height == 1 && level != maxLevel );
		width = std::max( width / 2, 1 );
		height = std::max( height / 2, 1 );
	}
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(tid.m_ID, uploadedSize, tid.m_ID);

	AssertIf( baseLevel > maxLevel );

	if( decompressBuffer )
		delete[] decompressBuffer;
}

void UploadTextureSubData2DGLES2( TextureID glname, UInt8* srcData,
								 int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace )
{
	Assert( !IsAnyCompressedTextureFormat( format ) );

	bool uploadIsCompressed, decompressOnTheFly;
	TextureFormat uploadFormat = GetUploadFormat(format, uploadIsCompressed, decompressOnTheFly);
	Assert( !uploadIsCompressed );
	Assert( !decompressOnTheFly );

	GLuint targetTex = (GLuint)TextureIdMap::QueryNativeTexture(glname);

	Assert(targetTex != 0);
	if(targetTex == 0)
		return;

	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, glname, kTexDim2D, std::numeric_limits<float>::infinity());
	GLES_CHK(glActiveTexture(GL_TEXTURE0));
	GLES_CHK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

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
#if UNITY_PEPPER
	else if (uploadFormat == kTexFormatARGB4444)
	{
		feedData = SwizzleRGBA4444 (srcData, &decompressBuffer, width, height);
	}
#endif

	// xenon, for real?
	bool isSRGB = (colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB);

	int glesFormat, datatype;
	GetUncompressedTextureFormat (uploadFormat, isSRGB, &glesFormat, &datatype);

	GLES_CHK(glTexSubImage2D( GL_TEXTURE_2D, mipLevel, x, y, width, height, glesFormat, datatype, feedData ));

	if( gGraphicsCaps.gles20.needFlushAfterTextureUpload )
		GLES_CHK(glFlush());

	if( decompressBuffer )
		delete[] decompressBuffer;
}

void UploadTextureCubeGLES2(
	TextureID tid, UInt8* srcData, int faceDataSize, int size,
	TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace )
{
	bool uploadIsCompressed, decompressOnTheFly;
	TextureFormat uploadFormat = GetUploadFormat(format, uploadIsCompressed, decompressOnTheFly);
	DBG_TEXTURE_VERBOSE_GLES20("TextureCUBE #%i (%ix%ix6) unityFmt: %d, uploadFmt: %d, compressed: %d, decompressOnCpu: %d",
							   tid.m_ID, size, size, format, uploadFormat, (int)uploadIsCompressed, (int)decompressOnTheFly);
	REGISTER_EXTERNAL_GFX_DEALLOCATION(tid.m_ID);

	TextureIdMapGLES20_QueryOrCreate(tid);
	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, tid, kTexDimCUBE, std::numeric_limits<float>::infinity());
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	// xenon, for real?
	bool isSRGB = (colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB);

	UInt8* decompressBuffer = NULL;

	const GLenum faces[6] =
	{
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	};
	int uploadedSize = 0;
	int maxLevel = mipCount - 1;
	for (int face=0;face<6;face++)
	{
		int mipSize = size;
		UInt8* data = srcData + face * faceDataSize;
		int glesFormat, datatype, size;

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
				//SetUnpackClientStorage (false);
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
#if UNITY_PEPPER
			else if (uploadFormat == kTexFormatARGB4444)
			{
				feedData = SwizzleRGBA4444 (srcData, &decompressBuffer, mipSize, mipSize);
			}
#endif
			// Just feed the data
			else
			{
				feedData = data;
			}

			// Upload
			if( uploadIsCompressed )
			{
				GetCompressedTextureFormat( uploadFormat, isSRGB, mipSize, mipSize, &glesFormat, &size );
				GLES_CHK(glCompressedTexImage2D (faces[face], level, glesFormat, mipSize, mipSize, 0, size, feedData));
			}
			else
			{
				GetUncompressedTextureFormat( uploadFormat, isSRGB, &glesFormat, &datatype );

				int internalFormat = glesFormat;
			#if UNITY_IPHONE
				//APPLE_texture_format_BGRA8888 dictates internal format for GL_BGRA_EXT to be GL_RGBA
				if(glesFormat == GL_BGRA_EXT)
					internalFormat = GL_RGBA;
			#endif

				GLES_CHK(glTexImage2D (faces[face], level, internalFormat, mipSize, mipSize, 0, glesFormat, datatype, feedData));
			}

			if( gGraphicsCaps.gles20.needFlushAfterTextureUpload )
				GLES_CHK(glFlush());

			//GLAssert ();
			int levelSize = CalculateImageSize( mipSize, mipSize, format );
			uploadedSize += levelSize;
			data += levelSize;
			AssertIf( mipSize == 1 && level != maxLevel );

			mipSize = std::max( mipSize / 2, 1 );
		}
	}
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(tid.m_ID, uploadedSize, tid.m_ID);

	if( decompressBuffer )
		delete[] decompressBuffer;
}

#if UNITY_IPHONE
	extern		bool  				IsActiveMSAARenderTargetGLES2();
	extern 		RenderSurfaceBase*	ResolveMSAASetupFBO(void*, int, GLuint, GLuint);
	extern 		void  				ResolveMSAASetupFBO_Cleanup(RenderSurfaceBase*);

	extern "C"	void* 				UnityDefaultFBOColorBuffer();
#endif

bool ReadbackTextureGLES2(ImageReference& image, int left, int bottom, int width, int height, int destX, int destY, GLuint globalSharedFBO, GLuint helperFBO)
{
	bool result = true;

#if UNITY_IPHONE
	RenderSurfaceBase* resolveRS = 0;
	GLint oldFBO = 0;
	if(IsActiveMSAARenderTargetGLES2() && gGraphicsCaps.gles20.hasAppleMSAA)
	{
		GLES_CHK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO));
		resolveRS = ResolveMSAASetupFBO(UnityDefaultFBOColorBuffer(), 0, globalSharedFBO, helperFBO);
	}
#endif

	int glesFormat, datatype;


	// The whole image we're reading into can be larger than the rect we read, so setup the alignment.
	GLES_CHK(glPixelStorei( GL_PACK_ALIGNMENT, 1 ));

	void* dstImagePtr[kTexFormatPCCount] =
	{
		0,									//
		image.GetRowPtr(destY) + destX,		//  kTexFormatAlpha8
		0,									//	kTexFormatARGB4444
		image.GetRowPtr(destY) + destX * 3,	//	kTexFormatRGB24
		image.GetRowPtr(destY) + destX * 4,	//	kTexFormatRGBA32
		image.GetRowPtr(destY) + destX * 4,	//	kTexFormatARGB32
		0,									//	kTexFormatARGBFloat
		image.GetRowPtr(destY) + destX * 2,	//	kTexFormatRGB565
		0,									//	kTexFormatBGR24

		0,									//	kTexFormatAlphaLum16
		0,									//	kTexFormatDXT1
		0,									//	kTexFormatDXT3
		0,									//	kTexFormatDXT5
		0									//	kTexFormatRGBA4444
	};

	GetUncompressedTextureFormat( image.GetFormat(), false, &glesFormat, &datatype );
	void* dstPtr = dstImagePtr[image.GetFormat()];
	switch( image.GetFormat() )
	{
		case kTexFormatRGB565:
		case kTexFormatRGBA32:
		case kTexFormatARGB32:
		case kTexFormatRGB24:
		case kTexFormatAlpha8:
			{
				int readFormat, readType;
				GLES_CHK(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &readFormat));
				GLES_CHK(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &readType));

				// Check if reading format/type matches our image's format/type, and width (as glReadPixels knows nothing about dst pitch)
				if (glesFormat == readFormat && datatype == readType && width == image.GetWidth())
				{
					GLES_CHK(glReadPixels(left, bottom, width, height, glesFormat, datatype, dstPtr));
				}
				else
				{
					// Read as GL_RGBA/GL_UNSIGNED_BYTE, because it's always valid in OpenGL ES, and convert it to specified image format
					int readFormat     = kTexFormatRGBA32;
					int readFormatSize = CalculateImageSize (width, height, readFormat);

					GetUncompressedTextureFormat( readFormat, false, &glesFormat, &datatype );

					UInt8* data = new UInt8[readFormatSize];
					GLES_CHK(glReadPixels(left, bottom, width, height, glesFormat, datatype, data));

					ImageReference src (width, height, GetRowBytesFromWidthAndFormat (width, readFormat), readFormat, data);
					ImageReference dst (width, height, image.GetRowBytes(), image.GetFormat(), dstPtr);
					dst.BlitImage( src );

					delete[]data;
				}
			}
			break;
		default:
			AssertString ("Not Supported");
			result = false;
	}

#if UNITY_IPHONE
	if(resolveRS)
	{
		GLES_CHK(glBindFramebuffer(GL_FRAMEBUFFER, oldFBO));
		ResolveMSAASetupFBO_Cleanup(resolveRS);
	}
#endif

	return result;
}

#endif // GFX_SUPPORTS_OPENGLES20
