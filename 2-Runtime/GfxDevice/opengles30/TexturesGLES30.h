#ifndef UNITY_TEXTURES_GLES_H_
#define UNITY_TEXTURES_GLES_H_

#include "Configuration/UnityConfigure.h"
#include "Runtime/Graphics/TextureFormat.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

class ImageReference;

// \todo [2013-05-03 pyry] Clean up this interface.

void UploadTexture2DGLES3(
	TextureID glname, TextureDimension dimension, UInt8* srcData, int width, int height,
	TextureFormat format, int mipCount, UInt32 uploadFlags, int masterTextureLimit, TextureColorSpace colorSpace );
void UploadTextureSubData2DGLES3(
	TextureID glname, UInt8* srcData,
	int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace );
void UploadTextureCubeGLES3(
	TextureID tid, UInt8* srcData, int faceDataSize, int size,
	TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace );

#endif // UNITY_TEXTURES_GLES_H_
