#pragma once

#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Math/ColorSpaceConversion.h"

/* Important note about endianess.
   Endianess needs to be swapped for the following formats:
   kTexFormatARGBFloat, kTexFormatRGB565, kTexFormatARGB4444, (assuming for this too: kTexFormatRGBA4444)
*/

UInt32 GetBytesFromTextureFormat( TextureFormat inFormat );
UInt32 GetMaxBytesPerPixel( TextureFormat inFormat );
int GetRowBytesFromWidthAndFormat( int width, TextureFormat format );
bool IsValidTextureFormat (TextureFormat format);


inline bool IsCompressedDXTTextureFormat( TextureFormat format )
{
	return format >= kTexFormatDXT1 && format <= kTexFormatDXT5;
}

inline bool IsCompressedPVRTCTextureFormat( TextureFormat format )
{
	return format >= kTexFormatPVRTC_RGB2 && format <= kTexFormatPVRTC_RGBA4;
}

inline bool IsCompressedETCTextureFormat( TextureFormat format )
{
	return format == kTexFormatETC_RGB4;
}

inline bool IsCompressedEACTextureFormat( TextureFormat format )
{
	return format >= kTexFormatEAC_R && format <= kTexFormatEAC_RG_SIGNED;
}

inline bool IsCompressedETC2TextureFormat( TextureFormat format )
{
	return format >= kTexFormatETC2_RGB && format <= kTexFormatETC2_RGBA8;
}

inline bool IsCompressedATCTextureFormat( TextureFormat format )
{
	return format == kTexFormatATC_RGB4 || format == kTexFormatATC_RGBA8;
}
inline bool IsCompressedFlashATFTextureFormat( TextureFormat format )
{
	return format == kTexFormatFlashATF_RGB_DXT1 || format == kTexFormatFlashATF_RGBA_JPG || format == kTexFormatFlashATF_RGB_JPG;
}
inline bool Is16BitTextureFormat(TextureFormat format )
{
	return format == kTexFormatARGB4444 || format == kTexFormatRGBA4444 || format == kTexFormatRGB565;
}

inline bool IsCompressedASTCTextureFormat( TextureFormat format)
{
	return format >= kTexFormatASTC_RGB_4x4 && format <= kTexFormatASTC_RGBA_12x12;
}

inline bool IsTextureFormatSupportedOnFlash(TextureFormat format )
{
	return IsCompressedFlashATFTextureFormat(format) ||
		format == kTexFormatARGB32 ||
		format == kTexFormatRGBA32 ||
		format == kTexFormatRGB24 ||
		format == kTexFormatAlpha8;
}

inline bool IsAnyCompressedTextureFormat( TextureFormat format )
{
	return     IsCompressedDXTTextureFormat(format) || IsCompressedPVRTCTextureFormat(format)
			|| IsCompressedETCTextureFormat(format) || IsCompressedATCTextureFormat(format)
			|| IsCompressedFlashATFTextureFormat (format) || IsCompressedEACTextureFormat(format)
			|| IsCompressedETC2TextureFormat(format) || IsCompressedASTCTextureFormat(format);
}

bool IsAlphaOnlyTextureFormat( TextureFormat format );

int GetTextureSizeAllowedMultiple( TextureFormat format );
int GetMinimumTextureMipSizeForFormat( TextureFormat format );
bool IsAlphaOnlyTextureFormat( TextureFormat format );

TextureFormat ConvertToAlphaTextureFormat (TextureFormat format);

bool HasAlphaTextureFormat( TextureFormat format );

bool IsDepthRTFormat( RenderTextureFormat format );
bool IsHalfRTFormat( RenderTextureFormat format );

const char* GetCompressionTypeString (TextureFormat format);
const char* GetTextureFormatString (TextureFormat format);
const char* GetTextureColorSpaceString (TextureColorSpace colorSpace);

TextureColorSpace ColorSpaceToTextureColorSpace(BuildTargetPlatform platform, ColorSpace colorSpace);

std::pair<int,int> RoundTextureDimensionsToBlocks (TextureFormat fmt, int w, int h);
