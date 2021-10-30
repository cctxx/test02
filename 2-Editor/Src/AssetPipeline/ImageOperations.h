#pragma once

#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Graphics/Texture2D.h"

class ImageReference;
class Image;
class ColorRGBAf;
struct Kernel1D;


void ComputeNextMipLevel (const ImageReference& src, ImageReference& dst, bool kaiserFilter, bool borderMips, TextureFormat dstFormat);
void ComputeNextMipLevelLightmap (const ImageReference& src, Image& dst);
void ComputePrevMipLevelLightmap (const ImageReference& src, ImageReference& dst);

void BlitImageIntoTextureLevel (int mip, Texture2D* tex, UInt8** imageData, const ImageReference& image);

void GenerateThumbnailFromTexture(Image& thumbnail, Texture2D const& constTexture, bool alphaOnly = false, bool normalMapDXT5nm = false, bool alphaIsTransparency = false);


// Given an ARGB32 image, replaces all alpha components with the grayscale value of RGB
void GrayScaleRGBToAlpha (UInt8* image, int width, int height, int rowbytes);

// Solidify
void AlphaDilateImage(ImageReference& image);

void ApplyMipMapFade (ImageReference& floatImage, int mip, int fadeStart, int fadeEnd, bool normalMap, bool dxt5nm);

void ConvertHeightToNormalMap (ImageReference& ref, float scale, bool sobelFilter, bool dxt5nm);
void ConvertExternalNormalMapToDXT5nm (ImageReference& ref);

enum CubemapGenerationMode {
	kGenerateNoCubemap = 0,
	kGenerateSpheremap = 1,
	kGenerateCylindricalmap = 2,
	kGenerateOldSpheremap = 3,
	kGenerateNiceSpheremap = 4,
	kGenerateFullCubemap = 5,
};
enum CubemapLayoutMode {
	kLayoutSingleImage = 0,
	kLayoutFaces = 1,
	kLayoutCross = 2,
	
	kLayoutOrientationMask = (1 << 8),
	kLayoutTypeMask = kLayoutOrientationMask - 1,
};
void GenerateCubemapFaceFromImage (const ImageReference& source, ImageReference& cubemap, CubemapGenerationMode mode, CubemapLayoutMode layout, int face);


void ConvertImageToRGBM (ImageReference& ref);
void ConvertImageToDoubleLDR (ImageReference& ref);

bool DoesTextureContainAlpha (const ImageReference& image, TextureUsageMode usageMode);
bool DoesTextureContainColor (const ImageReference& image);

void SRGB_GammaToLinear (ImageReference& srcImage);
void SRGB_LinearToGamma (ImageReference& srcImage);

void LinearToGammaSpaceXenon (ImageReference& srcImage);

bool CompressTexture (Texture2D& tex, TextureFormat textureFormat, int compressionQuality=kTexCompressionNormal);
