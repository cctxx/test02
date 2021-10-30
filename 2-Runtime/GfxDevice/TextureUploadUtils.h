#pragma once

#include "External/ProphecySDK/include/prcore/surface.hpp"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/S3Decompression.h"
#include "Runtime/Graphics/FlashATFDecompression.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "GfxDevice.h"
#include "VramLimits.h"

extern bool IsNPOTTextureAllowed(bool hasMipMap);

const int kDoubleLDRAlpha = 255 * 0.25f;


inline bool SkipLevelsForMasterTextureLimit (
											 int masterTextureLimit,
											 TextureFormat dataFormat, TextureFormat uploadFormat,
											 int mipCount,
											 bool uploadIsCompressed,
											 UInt8** srcData, int* width, int* height,
											 int* baseLevel, int* maxLevel, int* texWidth, int* texHeight, size_t* textureSize)
{
	bool wasScaledDown = false;
	
	// For compressed textures, stop applying masterTextureLimit if texture size drops below 4
	if( uploadIsCompressed )
	{
		while( masterTextureLimit > 0 && ((*width >> masterTextureLimit) < 4 || (*height >> masterTextureLimit) < 4) )
		{
			--masterTextureLimit;
		}
	}
	
	// skip several levels in data based on masterTextureLimit
	*maxLevel = mipCount - 1;
	*baseLevel = std::min( masterTextureLimit, *maxLevel );
	int level;
	for( level = 0; level < *baseLevel; ++level )
	{
		*srcData += CalculateImageSize (*width, *height, dataFormat);
		AssertIf( *width == 1 && *height == 1 && level != *maxLevel );
		*width = std::max( *width / 2, 1 );
		*height = std::max( *height / 2, 1 );
	}
	
	// Now estimate VRAM usage for the texture
	*textureSize = CalculateImageSize (*width, *height, uploadFormat);
	if (mipCount > 1)
		*textureSize += *textureSize / 3;
	int textureSizeKB = *textureSize / 1024;
	
	const int vramSizeKB = gGraphicsCaps.videoMemoryMB * 1024;
	const GfxDeviceStats::MemoryStats& memoryStats = GetRealGfxDevice().GetFrameStats().GetMemoryStats();
	const int currentVramUsageKB = (memoryStats.screenBytes + memoryStats.renderTextureBytes) / 1024;
	const int allowedVramUsageKB = std::max( (vramSizeKB - currentVramUsageKB) * kVRAMMaxFreePortionForTexture, 1.0f );
	
	// While texture is too large for hardware limits, or too large to sanely fit into VRAM, skip
	// mip levels. If it's non-mipmapped one, we have to reduce it and scale it into size that fits.
	// Don't do fitting into VRAM for Alpha8 textures (those are used for dynamic fonts, and
	// everything can break down if we scale them down).
	*texWidth = *width;
	*texHeight = *height;
	while (
		   *texWidth > gGraphicsCaps.maxTextureSize ||
		   *texHeight > gGraphicsCaps.maxTextureSize ||
		   (textureSizeKB > allowedVramUsageKB && dataFormat != kTexFormatAlpha8))
	{
		printf_console("Texture2D %ix%i won't fit, reducing size (needed mem=%i used mem=%i allowedmem=%i)\n", *texWidth, *texHeight, textureSizeKB, currentVramUsageKB, allowedVramUsageKB);
		if (*baseLevel < *maxLevel)
		{
			*srcData += CalculateImageSize (*texWidth, *texHeight, dataFormat);
			*width = std::max (*width / 2, 1);
			*height = std::max (*height / 2, 1);
			++*baseLevel;
		}
		*texWidth = std::max( *texWidth / 2, 1 );
		*texHeight = std::max( *texHeight / 2, 1 );
		textureSizeKB /= 4;
		*textureSize /= 4;
		wasScaledDown = true;
		if( *texWidth <= 4 && *texHeight <= 4 )
			break;
	}
	
	return wasScaledDown;
}


inline void HandleFormatDecompression (
									   TextureFormat format,
									   TextureUsageMode* usageMode,
									   TextureColorSpace colorSpace,
									   bool* uploadIsCompressed,
									   bool* decompressOnTheFly
									   )
{
	// Figure out whether we'll upload compressed or decompress on the fly
	
	///@TODO: This looks wrong. What the fuck???
	*uploadIsCompressed = IsCompressedDXTTextureFormat(format);
	
	switch (*usageMode)
	{
		case kTexUsageLightmapRGBM:
			// No special processing for RGBM if we support it
			if (*usageMode == kTexUsageLightmapRGBM && gGraphicsCaps.SupportsRGBM())
				*usageMode = kTexUsageNone;
			break;
		case kTexUsageLightmapDoubleLDR:
			// Never any special processing of DoubleLDR in players.
			//
			// In the editor, we'll add a dummy 0.25f alpha channel to the doubleLDR lightmap.
			// When we're in GLES20 emulation mode in the editor we're using pixel shaders compiled
			// for the platform the editor is currently running on and that forces RGBM decoding.
			// So we get (8.0 * 0.25) * color.rgb -- which is doubleLDR decoding.
#if !UNITY_EDITOR
			*usageMode = kTexUsageNone;
#endif
			break;
		case kTexUsageNormalmapPlain:
			// Never any special processing of plain normal maps in players.
			//
			// In the editor, we'll put .r into .a to make it work with shaders that expect DXT5nm
			// encoding.
#if !UNITY_EDITOR
			*usageMode = kTexUsageNone;
#endif
			break;
		default:
			*usageMode = kTexUsageNone;
	}
	
	////@TODO: BIG WTF??? We always decompress on the fly for pvrtc etc atc and wii formats????
	
	// Decompress on the fly when the device cannot handle
	// the compressed format or we have to do any special processing based on usage mode.
	*decompressOnTheFly = (*uploadIsCompressed && (!gGraphicsCaps.hasS3TCCompression || *usageMode != kTexUsageNone)) ||
		IsCompressedPVRTCTextureFormat(format) ||
		IsCompressedETCTextureFormat(format) ||
		IsCompressedATCTextureFormat(format) ||
		IsCompressedETC2TextureFormat(format) ||
		IsCompressedASTCTextureFormat(format) ||
		IsCompressedFlashATFTextureFormat(format);

# if !UNITY_XENON
	//If we are not on Xenon and the texture color space is xenon decompress on the fly
	if (*uploadIsCompressed && colorSpace == kTexColorSpaceSRGBXenon)
	{
		*decompressOnTheFly |= *uploadIsCompressed && colorSpace == kTexColorSpaceSRGBXenon;
	}
#endif
}


inline void InitImageBuffer (int width, int height, UInt8*& buffer, TextureFormat textureFormat)
{
	int imageSize = CalculateImageSize( width, height, textureFormat );
	if( buffer == NULL )
		buffer = new UInt8[imageSize];
}


inline void PerformUploadConversions (
									  int width, int height,
									  UInt8* dstBuffer, int dstPitch,
									  TextureUsageMode usageMode,
									  TextureColorSpace colorSpace,
									  const prcore::PixelFormat& pf
									  )
{
	if (usageMode == kTexUsageLightmapRGBM)
		DecodeRGBM (width, height, dstBuffer, dstPitch, pf);
#if UNITY_EDITOR
	if (usageMode == kTexUsageLightmapDoubleLDR)
		SetAlphaChannel (width, height, dstBuffer, dstPitch, pf, kDoubleLDRAlpha);
	if (usageMode == kTexUsageNormalmapPlain)
		SetAlphaToRedChannel (width, height, dstBuffer, dstPitch, pf);
	if (colorSpace == kTexColorSpaceSRGBXenon)
		XenonToNormalSRGBTexture(width, height, dstBuffer, dstPitch, pf);
#endif
}


inline void ConvertCompressedTextureUpload (
											int width, int height,
											TextureFormat format,
											const UInt8* srcData,
											UInt8*& decompressBuffer, int& tempBufferPitch,
											TextureUsageMode usageMode,
											TextureColorSpace colorSpace,
											int mipLevel)
{
	int dstWidth = std::max( width, 4 );
	int dstHeight = std::max( height, 4 );
	
	InitImageBuffer (dstWidth, dstHeight, decompressBuffer, kTexFormatRGBA32);
	tempBufferPitch = GetRowBytesFromWidthAndFormat(width, kTexFormatRGBA32);
	
	DecompressNativeTextureFormatWithMipLevel (format, width, height, mipLevel, (UInt32*)srcData, dstWidth, dstHeight, (UInt32*)decompressBuffer);
	
	PerformUploadConversions (width, height, decompressBuffer, tempBufferPitch, usageMode, colorSpace, GetProphecyPixelFormat(kTexFormatRGBA32));
}


inline bool ConvertUncompressedTextureUpload (
											  prcore::Surface& srcSurface,
											  prcore::Surface& dstSurface,
											  prcore::Surface::BlitMode blitMode,
											  TextureFormat uploadFormat,
											  TextureUsageMode usageMode,
											  TextureColorSpace colorSpace,
											  int width, int height, UInt8* inplaceData, int pitch, const prcore::PixelFormat& pf,
											  UInt8*& tempBuffer, int& tempBufferPitch
											  )
{
	if (usageMode != kTexUsageNone || colorSpace == kTexColorSpaceSRGBXenon)
	{
		if (uploadFormat == kTexFormatRGBA32 || uploadFormat == kTexFormatARGB32)
		{
			// Copy to locked rect
			dstSurface.BlitImage( srcSurface, blitMode );
			// Process in place
			PerformUploadConversions (width, height, inplaceData, pitch, usageMode, colorSpace, pf);
		}
		else
		{
			InitImageBuffer (width, height, tempBuffer, kTexFormatRGBA32);
			tempBufferPitch = GetRowBytesFromWidthAndFormat(width, kTexFormatRGBA32);
			
			// Copy to a temporary buffer so we can process the texture in place
			prcore::Surface tempSurface( width, height, tempBufferPitch, pf, tempBuffer );
			tempSurface.BlitImage( srcSurface, blitMode );
			
			// Process in place in the temp surface
			PerformUploadConversions (width, height, tempBuffer, tempBufferPitch, usageMode, colorSpace, pf);
			
			// Copy to locked rect
			dstSurface.BlitImage( tempSurface, blitMode );
		}
		return true;
	}
	return false;
}


inline void AdvanceToNextMipLevel (TextureFormat format, UInt8*& srcData, int& width, int& height, int& texWidth, int& texHeight)
{
	srcData += CalculateImageSize (width, height, format);
	width = std::max( width / 2, 1 );
	height = std::max( height / 2, 1 );
	texWidth = std::max( texWidth / 2, 1 );
	texHeight = std::max( texHeight / 2, 1 );
}
