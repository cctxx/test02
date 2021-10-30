#include "UnityPrefix.h"
#include "TexturesGLES30.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/TextureUploadUtils.h"
#include "Runtime/Graphics/S3Decompression.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Utilities/BitUtility.h"
#include "IncludesGLES30.h"
#include "AssertGLES30.h"
#include "DebugGLES30.h"
#include "TextureIdMapGLES30.h"

#include <vector>

#if GFX_SUPPORTS_OPENGLES30

struct TextureFormatInfoGLES30
{
	enum Type
	{
		kTypeReserved	= 0,	//!< Reserved format (should not be used).
		kTypeUncompressed,		//!< Uncompressed, natively supported format.
		kTypeNonNative,			//!< Uncompressed, but not supported format. Requires swizzle to nativeFormat.
		kTypeCompressed			//!< Compressed format. If not supported, always decompressed to kTexFormatRGBA32
	};

	Type			type;					//!< Texture format type, as interpreted by ES3.

	UInt32			linearInternalFormat;	//!< Internal format for linear color space
	UInt32			sRGBInternalFormat;		//!< Internal format for sRGB color space or 0 if not supported

	UInt32			transferFormat;			//!< Transfer format, or 0 if compressed texture
	UInt32			dataType;				//!< Transfer data type, or 0 if compressed texture

	TextureFormat	nativeFormat;			//!< Data must be converted to this format first.
};

static const TextureFormatInfoGLES30 s_textureFormatInfos[] =
{
#define _RES											{ TextureFormatInfoGLES30::kTypeReserved, 0, 0, 0, 0, 0 }
#define _NAT(NATIVE)									{ TextureFormatInfoGLES30::kTypeNonNative, 0, 0, 0, 0, NATIVE }
#define _UNC(LINEAR, TRANSFERFMT, DATATYPE)				{ TextureFormatInfoGLES30::kTypeUncompressed, LINEAR, 0, TRANSFERFMT, DATATYPE, 0 }
#define _SRG(LINEAR, SRGB, TRANSFERFMT, DATATYPE)		{ TextureFormatInfoGLES30::kTypeUncompressed, LINEAR, SRGB, TRANSFERFMT, DATATYPE, 0 }
#define _CMP(LINEAR, SRGB)								{ TextureFormatInfoGLES30::kTypeCompressed, LINEAR, SRGB, 0, 0, 0 }

	/* 0							*/ _RES,
	/* kTexFormatAlpha8				*/ _UNC(GL_ALPHA,								GL_ALPHA,				GL_UNSIGNED_BYTE),
	/* kTexFormatARGB4444			*/ _UNC(GL_RGBA4,								GL_RGBA,				GL_UNSIGNED_SHORT_4_4_4_4), // \todo [2013-05-03 pyry] Should we swizzle this?
	/* kTexFormatRGB24				*/ _SRG(GL_RGB8,		GL_SRGB8,				GL_RGB,					GL_UNSIGNED_BYTE),
	/* kTexFormatRGBA32				*/ _SRG(GL_RGBA8,		GL_SRGB8_ALPHA8,		GL_RGBA,				GL_UNSIGNED_BYTE),
	/* kTexFormatARGB32				*/ _NAT(kTexFormatRGBA32),
	/* kTexFormatARGBFloat			*/ _UNC(GL_RGBA16F,								GL_RGBA,				GL_HALF_FLOAT),
	/* kTexFormatRGB565				*/ _UNC(GL_RGB565,								GL_RGB,					GL_UNSIGNED_SHORT_5_6_5),
	/* kTexFormatBGR24				*/ _NAT(kTexFormatRGB24),
	/* kTexFormatAlphaLum16			*/ _UNC(GL_LUMINANCE_ALPHA,						GL_LUMINANCE_ALPHA,		GL_UNSIGNED_BYTE),
	/* kTexFormatDXT1				*/ _CMP(GL_COMPRESSED_RGB_S3TC_DXT1_EXT,		GL_COMPRESSED_SRGB_S3TC_DXT1_NV),
	/* kTexFormatDXT3				*/ _CMP(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,		GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV),
	/* kTexFormatDXT5				*/ _CMP(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,		GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV),
	/* kTexFormatRGBA4444			*/ _UNC(GL_RGBA4,								GL_RGBA,				GL_UNSIGNED_SHORT_4_4_4_4),
	/* kTexReserved1				*/ _RES,
	/* kTexReserved2				*/ _RES,
	/* kTexReserved3				*/ _RES,
	/* kTexReserved4				*/ _RES,
	/* kTexReserved5				*/ _RES,
	/* kTexReserved6				*/ _RES,
	/* reserved (wii) 0				*/ _RES,
	/* reserved (wii) 1				*/ _RES,
	/* reserved (wii) 2				*/ _RES,
	/* reserved (wii) 3				*/ _RES,
	/* reserved (wii) 4				*/ _RES,
	/* reserved (wii) 5				*/ _RES,
	/* reserved (wii) 6				*/ _RES,
	/* reserved (wii) 7				*/ _RES,
	/* kTexReserved11				*/ _RES,
	/* kTexReserved12				*/ _RES,
	/* kTexFormatPVRTC_RGB2			*/ _CMP(GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,		0),
	/* kTexFormatPVRTC_RGBA2		*/ _CMP(GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,	0),
	/* kTexFormatPVRTC_RGB4			*/ _CMP(GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG,		0),
	/* kTexFormatPVRTC_RGBA4		*/ _CMP(GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,	0),
	/* kTexFormatETC_RGB4			*/ _CMP(GL_COMPRESSED_RGB8_ETC2,				0),
	/* kTexFormatATC_RGB4			*/ _CMP(GL_ATC_RGB_AMD,							0),
	/* kTexFormatATC_RGBA8			*/ _CMP(GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD,		0),
	/* kTexFormatBGRA32				*/ _NAT(kTexFormatRGBA32), // \todo [2013-05-03 pyry] Use BGRA extension if possible.
	/* kTexFormatFlashATF_RGB_DXT1	*/ _RES,
	/* kTexFormatFlashATF_RGBA_JPG	*/ _RES,
	/* kTexFormatFlashATF_RGB_JPG	*/ _RES,
	/* kTexFormatEAC_R				*/ _CMP(GL_COMPRESSED_R11_EAC,					0),
	/* kTexFormatEAC_R_SIGNED		*/ _CMP(GL_COMPRESSED_SIGNED_R11_EAC,			0),
	/* kTexFormatEAC_RG				*/ _CMP(GL_COMPRESSED_RG11_EAC,					0),
	/* kTexFormatEAC_RG_SIGNED		*/ _CMP(GL_COMPRESSED_SIGNED_RG11_EAC,			0),
	/* kTexFormatETC2_RGB			*/ _CMP(GL_COMPRESSED_RGB8_ETC2,				0),
	/* kTexFormatETC2_RGBA1			*/ _CMP(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,	0),
	/* kTexFormatETC2_RGBA8			*/ _CMP(GL_COMPRESSED_RGBA8_ETC2_EAC,			0),
	/*kTexFormatASTC_RGB_4x4		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_4x4_KHR,        0),
	/*kTexFormatASTC_RGB_5x5		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_5x5_KHR,        0), 
	/*kTexFormatASTC_RGB_6x6 		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_6x6_KHR,        0),
	/*kTexFormatASTC_RGB_8x8 		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_8x8_KHR,        0),
	/*kTexFormatASTC_RGB_10x10 		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_10x10_KHR,        0),
	/*kTexFormatASTC_RGB_12x12 		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_12x12_KHR,        0),

	/*kTexFormatASTC_RGBA_4x4 		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_4x4_KHR,        0),
	/*kTexFormatASTC_RGBA_5x5 		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_5x5_KHR,        0),
	/*kTexFormatASTC_RGBA_6x6 		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_6x6_KHR,        0),
	/*kTexFormatASTC_RGBA_8x8 		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_8x8_KHR,        0),
	/*kTexFormatASTC_RGBA_10x10		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_10x10_KHR,        0),
	/*kTexFormatASTC_RGBA_12x12		*/ _CMP(GL_COMPRESSED_RGBA_ASTC_12x12_KHR,        0),

#undef _CMP
#undef _SRG
#undef _NAT
#undef _UNC
#undef _RES
};

// Static assert for array size.
typedef char __gles3TexFormatTableSizeAssert[(int)(sizeof(s_textureFormatInfos) / sizeof(s_textureFormatInfos[0])) == kTexFormatTotalCount ? 1 : - 1];

static const TextureFormatInfoGLES30& GetTextureFormatInfoGLES30 (TextureFormat format)
{
	Assert(0 <= format && format < (int)(sizeof(s_textureFormatInfos)/sizeof(s_textureFormatInfos[0])));
	return s_textureFormatInfos[format];
}

static inline bool IsTextureFormatSupported (TextureFormat format)
{
	return gGraphicsCaps.supportsTextureFormat[format];
}

static const char* GetCompressedTextureFormatName (TextureFormat format)
{
	if (IsCompressedPVRTCTextureFormat(format))
		return "PVRTC";
	else if (IsCompressedDXTTextureFormat(format))
		return "DXT";
	else if (IsCompressedETCTextureFormat(format))
		return "ETC1";
	else if (IsCompressedATCTextureFormat(format))
		return "ATC";
	else if (IsCompressedETC2TextureFormat(format))
		return "ETC2";
	else if (IsCompressedEACTextureFormat(format))
		return "EAC";
	else
		return "UNKNOWN";
}

static void GetDecompressBlockSize (TextureFormat format, int& width, int& height)
{
	// \todo [2013-05-06 pyry] Is there any format that doesn't use 4x4 blocks?
	width	= 4;
	height	= 4;
}

static inline int GetDecompressBlockWidth (TextureFormat format)
{
	int w, h;
	GetDecompressBlockSize(format, w, h);
	return w;
}

static inline int GetDecompressBlockHeight (TextureFormat format)
{
	int w, h;
	GetDecompressBlockSize(format, w, h);
	return h;
}

static inline int AlignToBlockSize (int dim, int blockSize)
{
	if (dim % blockSize != 0)
		return dim + blockSize - (dim % blockSize);
	else
		return dim;
}

static int UploadPyramidCompressed (UInt32			target,
									TextureFormat	format,
									bool			sRGB,
									int				width,
									int				height,
									int				numLevels,
									const UInt8*	data)
{
	Assert(IsTextureFormatSupported(format));

	const TextureFormatInfoGLES30&		formatInfo			= GetTextureFormatInfoGLES30(format);
	const bool							isSRGBUploadOk		= formatInfo.sRGBInternalFormat != 0;
	const UInt32						internalFormat		= (sRGB && isSRGBUploadOk) ? formatInfo.sRGBInternalFormat : formatInfo.linearInternalFormat;
	const UInt8*						curDataPtr			= data;
	int									totalUploadSize		= 0;

	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		const int		levelW			= std::max<int>(width>>levelNdx,	1);
		const int		levelH			= std::max<int>(height>>levelNdx,	1);
		const int		levelSize		= CalculateImageSize(levelW, levelH, format);
		const int		uploadLevel		= levelNdx;

		GLES_CHK(glCompressedTexImage2D(target, levelNdx, internalFormat, levelW, levelH, 0, levelSize, curDataPtr));

		curDataPtr		+= levelSize;
		totalUploadSize	+= levelSize;
	}

	return totalUploadSize;
}

static int UploadPyramidDecompress (UInt32			target,
									TextureFormat	format,
									bool			sRGB,
									int				width,
									int				height,
									int				numLevels,
									const UInt8*	data)
{
	const TextureFormatInfoGLES30&		formatInfo			= GetTextureFormatInfoGLES30(format);
	const TextureFormat					decompressFormat	= kTexFormatRGBA32;
	const TextureFormatInfoGLES30&		uploadFormat		= GetTextureFormatInfoGLES30(decompressFormat);
	const bool							isSRGBUploadOk		= uploadFormat.sRGBInternalFormat != 0;
	const UInt32						internalFormat		= (sRGB && isSRGBUploadOk) ? uploadFormat.sRGBInternalFormat : uploadFormat.linearInternalFormat;
	const UInt8*						curDataPtr			= data;
	const int							blockW				= GetDecompressBlockWidth(format);
	const int							blockH				= GetDecompressBlockHeight(format);
	int									totalUploadSize		= 0;
	std::vector<UInt8>					decompressBuffer	(CalculateImageSize(AlignToBlockSize(width, blockW), AlignToBlockSize(height, blockH), decompressFormat));

	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		const int		levelW			= std::max<int>(width>>levelNdx,	1);
		const int		levelH			= std::max<int>(height>>levelNdx,	1);
		const int		levelSize		= CalculateImageSize(levelW, levelH, format);
		const int		decompressW		= AlignToBlockSize(levelW, blockW);
		const int		decompressH		= AlignToBlockSize(levelH, blockH);
		const int		decompressSize	= CalculateImageSize(decompressW, decompressH, decompressFormat);

		Assert(decompressSize <= (int)decompressBuffer.size());
		if (!DecompressNativeTextureFormat(format, levelW, levelH, (UInt32*)curDataPtr, decompressW, decompressH, (UInt32*)&decompressBuffer[0]))
			ErrorStringMsg("Decompressing level %d failed!", levelNdx);

		GLES_CHK(glTexImage2D(target, levelNdx, internalFormat, levelW, levelH, 0, uploadFormat.transferFormat, uploadFormat.dataType, &decompressBuffer[0]));

		curDataPtr		+= levelSize;
		totalUploadSize	+= decompressSize;
	}

	return totalUploadSize;
}

static int UploadPyramidConvert (UInt32			target,
								 TextureFormat	srcFormat,
								 TextureFormat	dstFormat,
								 bool			sRGB,
								 int			width,
								 int			height,
								 int			numLevels,
								 const UInt8*	data)
{
	const TextureFormatInfoGLES30&		dstFormatInfo		= GetTextureFormatInfoGLES30(dstFormat);
	const bool							isSRGBUploadOk		= dstFormatInfo.sRGBInternalFormat != 0;
	const UInt32						internalFormat		= (sRGB && isSRGBUploadOk) ? dstFormatInfo.sRGBInternalFormat : dstFormatInfo.linearInternalFormat;
	const UInt8*						curDataPtr			= data;
	int									totalUploadSize		= 0;
	std::vector<UInt8>					convertBuffer		(CalculateImageSize(width, height, dstFormat));

	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		const int		levelW			= std::max<int>(width>>levelNdx,	1);
		const int		levelH			= std::max<int>(height>>levelNdx,	1);
		const int		levelSize		= CalculateImageSize(levelW, levelH, srcFormat);

		ImageReference	src				(levelW, levelH, GetRowBytesFromWidthAndFormat(levelW, srcFormat), srcFormat, (UInt8*)curDataPtr);
		ImageReference	dst				(levelW, levelH, GetRowBytesFromWidthAndFormat(levelW, dstFormat), dstFormat, &convertBuffer[0]);

		dst.BlitImage(src);

		GLES_CHK(glTexImage2D(target, levelNdx, internalFormat, levelW, levelH, 0, dstFormatInfo.transferFormat, dstFormatInfo.dataType, &convertBuffer[0]));

		curDataPtr		+= levelSize;
		totalUploadSize	+= dst.GetRowBytes()*dst.GetHeight();
	}

	return totalUploadSize;
}

static int UploadPyramid (UInt32		target,
						  TextureFormat	format,
						  bool			sRGB,
						  int			width,
						  int			height,
						  int			numLevels,
						  const UInt8*	data)
{
	const TextureFormatInfoGLES30&		formatInfo			= GetTextureFormatInfoGLES30(format);
	const bool							isSRGBUploadOk		= formatInfo.sRGBInternalFormat != 0;
	const UInt32						internalFormat		= (sRGB && isSRGBUploadOk) ? formatInfo.sRGBInternalFormat : formatInfo.linearInternalFormat;
	const UInt8*						curDataPtr			= data;
	int									totalUploadSize		= 0;

	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		const int	levelW		= std::max<int>(width>>levelNdx,	1);
		const int	levelH		= std::max<int>(height>>levelNdx,	1);
		const int	levelSize	= CalculateImageSize(levelW, levelH, format);

		GLES_CHK(glTexImage2D(target, levelNdx, internalFormat, levelW, levelH, 0, formatInfo.transferFormat, formatInfo.dataType, curDataPtr));

		curDataPtr		+= levelSize;
		totalUploadSize	+= levelSize;
	}

	return totalUploadSize;
}

void UploadTexture2DGLES3 (TextureID			texID,
						   TextureDimension		dimension,
						   UInt8*				srcData,
						   int					width,
						   int					height,
						   TextureFormat		format,
						   int					mipCount,
						   UInt32				uploadFlags,
						   int					skipMipLevels,
						   TextureColorSpace	colorSpace)
{
	Assert(dimension == kTexDim2D); // \todo [2013-05-03 pyry] Remove parameter.

	const TextureFormatInfoGLES30&	formatInfo		= GetTextureFormatInfoGLES30(format);
	const bool						isSRGB			= colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB;
	const bool						isCompressed	= formatInfo.type == TextureFormatInfoGLES30::kTypeCompressed;
	const bool						decompress		= isCompressed && !IsTextureFormatSupported(format);
	const bool						convertToNative	= formatInfo.type == TextureFormatInfoGLES30::kTypeNonNative;
	int								totalUploadSize	= 0;

	Assert(!decompress || !convertToNative);
	Assert(skipMipLevels < mipCount);

	if (decompress)
		printf_console("WARNING: %s compressed texture format not supported, decompressing!\n", GetCompressedTextureFormatName(format));
	else if (convertToNative)
		printf_console("WARNING: no native support for texture format %d, converting to %d!\n", format, formatInfo.nativeFormat);

	// Create and bind texture.
	TextureIdMapGLES30_QueryOrCreate(texID);
	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, texID, dimension, std::numeric_limits<float>::infinity());

	// \todo [2013-05-03 pyry] Select unpack alignment based on pixel format
	GLES_CHK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

	// Upload parameters.
	const int		baseLevelW		= std::max<int>(1, width >> skipMipLevels);
	const int		baseLevelH		= std::max<int>(1, height >> skipMipLevels);
	const int		numLevelsUpload	= mipCount-skipMipLevels;
	const UInt8*	baseLevelPtr	= srcData;

	// Adjust data pointer by number of levels skipped.
	// \todo [2013-045-03 pyry] Skip levels if they are too large for HW.
	for (int levelNdx = 0; levelNdx < skipMipLevels; levelNdx++)
	{
		int levelW		= std::max<int>(1, width >> levelNdx);
		int levelH		= std::max<int>(1, height >> levelNdx);
		int levelSize	= CalculateImageSize(levelW, levelH, format);

		baseLevelPtr += levelSize;
	}

	// Call upload.
	if (isCompressed)
	{
		if (decompress)
			totalUploadSize = UploadPyramidDecompress(GL_TEXTURE_2D, format, isSRGB, baseLevelW, baseLevelH, numLevelsUpload, baseLevelPtr);
		else
			totalUploadSize = UploadPyramidCompressed(GL_TEXTURE_2D, format, isSRGB, baseLevelW, baseLevelH, numLevelsUpload, baseLevelPtr);
	}
	else
	{
		if (convertToNative)
			totalUploadSize = UploadPyramidConvert(GL_TEXTURE_2D, format, formatInfo.nativeFormat, isSRGB, baseLevelW, baseLevelH, numLevelsUpload, baseLevelPtr);
		else
			totalUploadSize = UploadPyramid(GL_TEXTURE_2D, format, isSRGB, baseLevelW, baseLevelH, numLevelsUpload, baseLevelPtr);
	}

	GLES_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numLevelsUpload-1));

	REGISTER_EXTERNAL_GFX_DEALLOCATION(texID.m_ID);
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(texID.m_ID, totalUploadSize, texID.m_ID);
}

void UploadTextureSubData2DGLES3 (TextureID			texID,
								  UInt8*			srcData,
								  int				mipLevel,
								  int				x,
								  int				y,
								  int				width,
								  int				height,
								  TextureFormat		format,
								  TextureColorSpace	colorSpace)
{
	const TextureFormatInfoGLES30&	formatInfo		= GetTextureFormatInfoGLES30(format);
	const bool						isSRGB			= colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB;
	const bool						isCompressed	= formatInfo.type == TextureFormatInfoGLES30::kTypeCompressed;
	const bool						decompress		= isCompressed && !IsTextureFormatSupported(format);
	const bool						convertToNative	= formatInfo.type == TextureFormatInfoGLES30::kTypeNonNative;
	const TextureFormatInfoGLES30&	uploadInfo		= convertToNative ? GetTextureFormatInfoGLES30(formatInfo.nativeFormat) : formatInfo;
	int								totalUploadSize	= 0;
	const UInt32					target			= GL_TEXTURE_2D;

	Assert(!decompress || !convertToNative);

	if (decompress)
		printf_console("WARNING: %s compressed texture format not supported, decompressing!\n", GetCompressedTextureFormatName(format));
	else if (convertToNative)
		printf_console("WARNING: no native support for texture format %d, converting to %d!\n", format, formatInfo.nativeFormat);

	// Create and bind texture.
	TextureIdMapGLES30_QueryOrCreate(texID);
	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, texID, kTexDim2D, std::numeric_limits<float>::infinity());

	// \todo [2013-05-03 pyry] Select unpack alignment based on pixel format
	GLES_CHK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

	if (isCompressed)
	{
		if (decompress)
		{
			const int				blockW				= GetDecompressBlockWidth(format);
			const int				blockH				= GetDecompressBlockHeight(format);
			const int				decompressW			= AlignToBlockSize(width, blockW);
			const int				decompressH			= AlignToBlockSize(height, blockH);
			std::vector<UInt8>		decompressBuffer	(CalculateImageSize(decompressW, decompressH, format));

			if (!DecompressNativeTextureFormat(format, width, height, (const UInt32*)srcData, decompressW, decompressH, (UInt32*)&decompressBuffer[0]))
				ErrorString("Decompressing texture data failed!");

			GLES_CHK(glTexSubImage2D(target, mipLevel, x, y, width, height, uploadInfo.transferFormat, uploadInfo.dataType, &decompressBuffer[0]));
		}
		else
		{
			const int dataSize = CalculateImageSize(width, height, format);
			GLES_CHK(glCompressedTexSubImage2D(target, mipLevel, x, y, width, height, uploadInfo.linearInternalFormat, dataSize, srcData));
		}
	}
	else
	{
		if (convertToNative)
		{
			std::vector<UInt8>	convertBuffer	(CalculateImageSize(width, height, formatInfo.nativeFormat));
			ImageReference		src				(width, height, GetRowBytesFromWidthAndFormat(width, format), format, srcData);
			ImageReference		dst				(width, height, GetRowBytesFromWidthAndFormat(width, formatInfo.nativeFormat), formatInfo.nativeFormat, &convertBuffer[0]);

			dst.BlitImage(src);
			
			GLES_CHK(glTexSubImage2D(target, mipLevel, x, y, width, height, uploadInfo.transferFormat, uploadInfo.dataType, &convertBuffer[0]));
		}
		else
			GLES_CHK(glTexSubImage2D(target, mipLevel, x, y, width, height, uploadInfo.transferFormat, uploadInfo.dataType, srcData));
	}
}

void UploadTextureCubeGLES3 (TextureID			texID,
							 UInt8*				srcData,
							 int				faceDataSize,
							 int				size,
							 TextureFormat		format,
							 int				mipCount,
							 UInt32				uploadFlags,
							 TextureColorSpace	colorSpace)
{
	const TextureFormatInfoGLES30&	formatInfo		= GetTextureFormatInfoGLES30(format);
	const bool						isSRGB			= colorSpace == kTexColorSpaceSRGBXenon || colorSpace == kTexColorSpaceSRGB;
	const bool						isCompressed	= formatInfo.type == TextureFormatInfoGLES30::kTypeCompressed;
	const bool						decompress		= isCompressed && !IsTextureFormatSupported(format);
	const bool						convertToNative	= formatInfo.type == TextureFormatInfoGLES30::kTypeNonNative;
	int								totalUploadSize	= 0;

	Assert(!decompress || !convertToNative);

	if (decompress)
		printf_console("WARNING: %s compressed texture format not supported, decompressing!\n", GetCompressedTextureFormatName(format));
	else if (convertToNative)
		printf_console("WARNING: no native support for texture format %d, converting to %d!\n", format, formatInfo.nativeFormat);

	// Create and bind texture.
	TextureIdMapGLES30_QueryOrCreate(texID);
	GetRealGfxDevice().SetTexture (kShaderFragment, 0, 0, texID, kTexDimCUBE, std::numeric_limits<float>::infinity());

	// \todo [2013-05-03 pyry] Select unpack alignment based on pixel format
	GLES_CHK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

	// \todo [2013-045-03 pyry] Skip levels if they are too large for HW.
	const int		baseLevelOffset	= 0;
	const int		baseLevelW		= size;
	const int		baseLevelH		= size;
	const int		numLevelsUpload	= mipCount;

	const GLenum faces[6] =
	{
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	};

	for (int faceNdx = 0; faceNdx < 6; faceNdx++)
	{
		const UInt8*	facePtr		= (srcData + faceNdx*faceDataSize) + baseLevelOffset;
		const UInt32	target		= faces[faceNdx];

		if (isCompressed)
		{
			if (decompress)
				totalUploadSize = UploadPyramidDecompress(target, format, isSRGB, baseLevelW, baseLevelH, numLevelsUpload, facePtr);
			else
				totalUploadSize = UploadPyramidCompressed(target, format, isSRGB, baseLevelW, baseLevelH, numLevelsUpload, facePtr);
		}
		else
		{
			if (convertToNative)
				totalUploadSize = UploadPyramidConvert(target, format, formatInfo.nativeFormat, isSRGB, baseLevelW, baseLevelH, numLevelsUpload, facePtr);
			else
				totalUploadSize = UploadPyramid(target, format, isSRGB, baseLevelW, baseLevelH, numLevelsUpload, facePtr);
		}
	}

	GLES_CHK(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, numLevelsUpload-1));

	REGISTER_EXTERNAL_GFX_DEALLOCATION(texID.m_ID);
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(texID.m_ID, totalUploadSize, texID.m_ID);
}

#endif // GFX_SUPPORTS_OPENGLES30
