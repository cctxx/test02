#ifndef UNITY_TEXTURES_GLES_H_
#define UNITY_TEXTURES_GLES_H_

#include "Configuration/UnityConfigure.h"
#include "Runtime/Graphics/TextureFormat.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

class ImageReference;

void UploadTexture2DGLES2(
	TextureID glname, TextureDimension dimension, UInt8* srcData, int width, int height,
	TextureFormat format, int mipCount, UInt32 uploadFlags, int masterTextureLimit, TextureColorSpace colorSpace );
void UploadTextureSubData2DGLES2(
	TextureID glname, UInt8* srcData,
	int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace );
void UploadTextureCubeGLES2(
	TextureID tid, UInt8* srcData, int faceDataSize, int size,
	TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace );
bool ReadbackTextureGLES2(ImageReference& image, int left, int bottom, int width, int height, int destX, int destY, unsigned int globalSharedFBO, unsigned int helperFBO);

#endif // UNITY_TEXTURES_GLES_H_
