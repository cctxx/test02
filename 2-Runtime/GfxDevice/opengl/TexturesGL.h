#pragma once

#include "Runtime/Graphics/TextureFormat.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Configuration/UnityConfigure.h"

class ImageReference;

void UploadTexture2DGL(
	TextureID tid, TextureDimension dimension, UInt8* srcData, int width, int height,
	TextureFormat format, int mipCount, UInt32 uploadFlags, int masterTextureLimit, TextureUsageMode usageMode, TextureColorSpace colorSpace );

void UploadTextureSubData2DGL(
	TextureID tid, UInt8* srcData,
	int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace );

void UploadTextureCubeGL(
	TextureID tid, UInt8* srcData, int faceDataSize, int size,
	TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace );

void UploadTexture3DGL(
	TextureID tid, UInt8* srcData, int width, int height, int depth,
	TextureFormat format, int mipCount, UInt32 uploadFlags );

bool ReadbackTextureGL( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY );
