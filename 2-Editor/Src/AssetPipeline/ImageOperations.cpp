#include "UnityPrefix.h"
#include "ImageOperations.h"
#include "ImageConverter.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Threads/JobScheduler.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "External/S3TC_ATI/CompressS3TC.h"
#include "External/Qualcomm_TextureConverter/TextureConverter.h"
#include <vector>
#include <string>
#include "External/Stb/jpge.h"
#include "External/JXR_RefSoft_LC/simple_xr.h"

#if UNITY_WIN 
#include <direct.h> // For _getcwd
#endif


// Importing 4 "Terrain Textures" from std. assets:
// 8 cores, Core i7 920 @ 2.67GHz, Windows 7 64 bit
//   no multithreading: 11.30 s
//   multithreading: 2.82 s
// 2 cores, Core 2 Duo 2.4GHz, OS X 10.6.2
//   no multithreading: 11.40 s
//   multithreading: 6.37 s
#define ENABLE_MULTITHREADED_DXT_COMPRESSION 1
#define PRINT_TEXTURE_COMPRESSION_TIMES 0

#define USE_PVR_STANDALONE_TOOL 1
#if !USE_PVR_STANDALONE_TOOL
	#include "External/PVRTextureTool/PVRTexture.h"
	#include "External/PVRTextureTool/PVRTextureUtilities.h"
#endif

void PerformTextureCompression (int width, int height, void* input, void* output, int compression);

void BlitImageIntoTextureLevel (int mip, Texture2D* tex, UInt8** imageData, const ImageReference& image)
{
	TextureFormat textureFormat = tex->GetTextureFormat();
	int w = image.GetWidth ();
	int h = image.GetHeight ();
	AssertIf(IsAnyCompressedTextureFormat(textureFormat));

	// Blit image to texture, the texture format is automatically converted on blitting
	ImageReference dstImage (w, h, GetRowBytesFromWidthAndFormat (w, textureFormat), textureFormat, *imageData);
	dstImage.BlitImage (image);

	*imageData += CalculateImageSize (w, h, textureFormat);
}


inline void DecodeDXT5nmPixel(UInt8* argb)
{
	float nx = (2.f * (1.0f/255.0f) * (float)argb[0]) - 1.0f;
	float ny = (2.f * (1.0f/255.0f) * (float)argb[1]) - 1.0f;
	float nz = 0.5f + 0.5f * sqrtf(1.0f - nx*nx - ny*ny);
	nz = std::min(255.0f, 255.0f * nz);
	argb[3] = (UInt8)(nz);
	argb[2] = argb[1];
	argb[1] = argb[0];

}

inline void DecodeRGBMPixel(UInt8* argb)
{
	UInt32 alpha = kRGBMMaxRange * argb[0];
	argb[1] = std::min((argb[1] * alpha) / 255, (UInt32)255);
	argb[2] = std::min((argb[2] * alpha) / 255, (UInt32)255);
	argb[3] = std::min((argb[3] * alpha) / 255, (UInt32)255);
}

inline void DecodeDoubleLDRPixel(UInt8* argb)
{
	argb[1] = std::min((UInt32)argb[1] * 2, (UInt32)255);
	argb[2] = std::min((UInt32)argb[2] * 2, (UInt32)255);
	argb[3] = std::min((UInt32)argb[3] * 2, (UInt32)255);
}

inline void CopyAlphaToRGB (UInt8* argb)
{
	argb[1] = argb[0];
	argb[2] = argb[0];
	argb[3] = argb[0];
}

inline void DoNothing (UInt8* argb)
{
}

template <typename FUNC>
void ProcessPixels (int count, UInt8* src, int bpp, FUNC& func)
{
	for (int x = 0; x < count; x++, src += bpp)
	{
		func(src);
		src[0] = 255; // set alpha to opaque
	}
}

const int kThumbnailSize = 16;

void GenerateThumbnailFromTexture (Image& thumbnail, Texture2D const& texture, bool alphaOnly, bool normalMapDXT5nm, bool alphaIsTransparency)
{
	const TextureFormat thumbnailFormat = kTexFormatARGB32;
	const UInt32 bpp = GetBytesFromTextureFormat (thumbnailFormat);
	int actualWidth = kThumbnailSize;
	int actualHeight = kThumbnailSize;
	int offSetX = 0;
	int offSetY = 0;
	if (texture.GetDataWidth() != texture.GetDataHeight() && texture.GetDataWidth() != 0 && texture.GetDataHeight() != 0) 
	{
		float aspectRatio = (float)texture.GetDataWidth() / (float) texture.GetDataHeight();
		if (texture.GetDataWidth() < texture.GetDataHeight()) {
			actualWidth = CeilfToInt(kThumbnailSize * aspectRatio);
			offSetX = (kThumbnailSize - actualWidth) / 2;
		} else 
		{
			actualHeight = CeilfToInt (kThumbnailSize/aspectRatio);
			offSetY = (kThumbnailSize - actualHeight) / 2;
		}
	} 
	Image tempImage = Image();
	tempImage.SetImage (actualWidth, actualHeight, thumbnailFormat, true);
	texture.ExtractImage (&tempImage);

	UInt8* src = tempImage.GetImageData();

	// Set the alpha to 0 for the bottom row of pixels
	// so that we have 1px spacing between thumbnails
	// in the Project view.
	for (int x = 0; x < actualWidth; x++, src += bpp)
	{
		src[0] = 0;
	}

	// Process the rest of thumbnail's pixels
	const int kPixelCount = actualWidth * actualHeight - actualWidth;

	if (normalMapDXT5nm)
		ProcessPixels (kPixelCount, src, bpp, DecodeDXT5nmPixel);
	else if (texture.GetUsageMode() == kTexUsageLightmapRGBM)
		ProcessPixels (kPixelCount, src, bpp, DecodeRGBMPixel);
	else if (texture.GetUsageMode() == kTexUsageLightmapDoubleLDR)
		ProcessPixels (kPixelCount, src, bpp, DecodeDoubleLDRPixel);
	else if (alphaOnly)
		ProcessPixels (kPixelCount, src, bpp, CopyAlphaToRGB);
	else if (!alphaIsTransparency)
		ProcessPixels (kPixelCount, src, bpp, DoNothing);

	thumbnail.SetImage (kThumbnailSize, kThumbnailSize, thumbnailFormat, true);
	thumbnail.ClearImage (ColorRGBA32(0,0,0,0));
	thumbnail.BlitImage(offSetX, offSetY, tempImage);
}


void GrayScaleRGBToAlpha (UInt8* image, int width, int height, int rowbytes)
{
	for (int y=0;y < height;y++)
	{
		UInt8* pixel = image + rowbytes * y;
		for (int x=0;x<width;x++)
		{
			UInt32 gray = (pixel[1] + pixel[2] + pixel[3]) / 3;
			*pixel = gray;
			pixel += 4;
		}
	}
}

static void BlendFloatImageWithColor (ImageReference& ref, const ColorRGBAf& fadeColor, float t, bool skipAlpha)
{
	int height = ref.GetHeight ();
	int width = ref.GetWidth ();
	for (int y=0;y < height;y++)
	{
		float* pixel = reinterpret_cast<float*> (ref.GetRowPtr (y));
		for (int x=0;x<width;x++)
		{
			if (!skipAlpha)
				*pixel = Lerp (*pixel, fadeColor.a, t);
			pixel++;
			*pixel = Lerp (*pixel, fadeColor.r, t); pixel++;
			*pixel = Lerp (*pixel, fadeColor.g, t); pixel++;
			*pixel = Lerp (*pixel, fadeColor.b, t); pixel++;
		}
	}
}

namespace
{
	// We are allocating large arrays of PixelInfo; up to two copies of
	// PixelInfo per source pixel - for a 4k texture that's a lot!
	// So better keep the struct small.
	struct PixelInfo
	{
		ColorRGBA32 col;
		UInt16 x, y;
	};

	void AddTransparentPixel(ImageReference& image, UInt32 x, UInt32 y, ColorRGBA32 targetCol, dynamic_array<PixelInfo>& transparentPixels, dynamic_array<char>& usedPixels)
	{
		ColorRGBA32 col = GetImagePixel(image.GetImageData(), image.GetWidth(), image.GetHeight(), image.GetFormat(), kTexWrapClamp, x, y);
		if (col.a == 0 && !usedPixels[y * image.GetWidth() + x])
		{
			usedPixels[y * image.GetWidth() + x] = 1;

			PixelInfo p;
			p.x = x;
			p.y = y;
			p.col = targetCol;

			transparentPixels.push_back(p);

			SetImagePixel(image, p.x, p.y, kTexWrapClamp, p.col);			
		}
	}

	void AddTransparentPixels(ImageReference& image, UInt32 x, UInt32 y, ColorRGBA32 targetCol, dynamic_array<PixelInfo>& transparentPixels, dynamic_array<char>& usedPixels)
	{
		if (x > 0)
			AddTransparentPixel(image, x - 1, y, targetCol, transparentPixels, usedPixels);
		if (x < image.GetWidth() - 1)
			AddTransparentPixel(image, x + 1, y, targetCol, transparentPixels, usedPixels);
		if (y > 0)
			AddTransparentPixel(image, x, y - 1, targetCol, transparentPixels, usedPixels);
		if (y < image.GetHeight() - 1)
			AddTransparentPixel(image, x, y + 1, targetCol, transparentPixels, usedPixels);
	}
}

void AlphaDilateImage(ImageReference& image)
{
	const int imageWidth = image.GetWidth();
	const int imageHeight = image.GetHeight();
	dynamic_array<char> usedPixels(imageWidth * imageHeight, 0, kMemTempAlloc);
	
	dynamic_array<PixelInfo> transparentPixels(kMemTempAlloc);
	// Reserve some space proportional to source image size, to hold
	// "edge of transparency" pixels.
	transparentPixels.reserve(std::max<size_t>(usedPixels.size()/16, 64));

	int idx = 0;
	for (int y = 0; y < imageHeight; ++y)
	{
		for (int x = 0; x < imageWidth; ++x, ++idx)
		{
			ColorRGBA32 col = GetImagePixel(image.GetImageData(), imageWidth, imageHeight, image.GetFormat(), kTexWrapClamp, x, y);
			if (col.a > 0 && !usedPixels[idx])
			{
				col.a = 0;
				AddTransparentPixels(image, x, y, col, transparentPixels, usedPixels);
			}			
		}
	}

	if (!transparentPixels.empty())
	{
		dynamic_array<PixelInfo> transparentPixels2(kMemTempAlloc);
		transparentPixels2.reserve(usedPixels.size());

		while (!transparentPixels.empty())
		{
			transparentPixels2.resize_uninitialized(0);
			for (dynamic_array<PixelInfo>::iterator it = transparentPixels.begin(), itEnd = transparentPixels.end(); it != itEnd; ++it)
			{
				const PixelInfo& p = *it;
				AddTransparentPixels(image, p.x, p.y, p.col, transparentPixels2, usedPixels);
			}

			transparentPixels.swap(transparentPixels2);
		}
	}
}

// Fade towards "neutral" detail/normalmap:
// Towards gray for regular textures, don't touch alpha
// Towards neutral normal for normal maps:
//   in DXT5nm, all components to 0.5
//   in plain normal maps, to 0.5,0.5,1.0, don't touch alpha
void ApplyMipMapFade (ImageReference& floatImage, int mip, int fadeStart, int fadeEnd, bool normalMap, bool dxt5nm)
{
	float t;
	if (fadeStart == fadeEnd)
		t = 1.0F;
	else
		t = clamp01 ((float)(mip - fadeStart) / (float)(fadeEnd - fadeStart));
	ColorRGBAf color;
	if (normalMap && !dxt5nm)
		color = ColorRGBAf (0.5f, 0.5f, 1.0f, 0.0f);
	else
		color = ColorRGBAf (0.5f, 0.5f, 0.5f, 0.5f);
	bool useAlpha = (normalMap && dxt5nm);
	BlendFloatImageWithColor (floatImage, color, t, !useAlpha);
}

template<typename T, bool convertIntToFloat>
static void ReplaceRowHalveImage( const ImageReference& src, int srcRow, ImageReference& dst, int dstRow )
{
	const int width = dst.GetWidth ();

	const int srcNextPixel = src.GetWidth () == 1 ? 0 : 4;
	const int dstNextPixel = dst.GetWidth () == 1 ? 0 : 4;

	srcRow = clamp (srcRow, 0, src.GetHeight()-1);
	dstRow = clamp (dstRow, 0, dst.GetHeight()-1);

	const T* s = (const T*)(src.GetImageData () + src.GetRowBytes() * srcRow);
	const float conversionValue = 1.0f/255.0f;

	float* d = (float*)(dst.GetImageData () + dst.GetRowBytes() * dstRow);
	for (int x=0;x<width;x++)
	{
		if(convertIntToFloat)
		{
			for (int i=0;i<4;i++)
				d[i] = ((float)s[i] + (float)s[srcNextPixel + i]) * 0.5F * conversionValue;
		}
		else
		{
			for (int i=0;i<4;i++)
				d[i] = (s[i] + s[srcNextPixel + i]) * 0.5F;
		}
		d += dstNextPixel;
		s += srcNextPixel * 2;
	}
}
template<typename T, bool convertIntToFloat>
static void ReplaceColumnHalveImage( const ImageReference& src, int srcCol, ImageReference& dst, int dstCol )
{
	const int height = dst.GetHeight ();

	const int srcNextPixel = src.GetWidth () == 1 ? 0 : 4;
	const int srcNextLine = src.GetHeight () == 1 ? 0 : src.GetWidth () * srcNextPixel;

	const int dstNextPixel = dst.GetWidth () == 1 ? 0 : 4;
	const int dstNextLine = dst.GetHeight () == 1 ? 0 : dst.GetWidth () * dstNextPixel;

	srcCol = clamp(srcCol, 0, src.GetWidth()-1);
	dstCol = clamp(dstCol, 0, dst.GetWidth()-1);

	const T* s = (const T*)src.GetImageData () + srcCol * 4;
	float* d = (float*)dst.GetImageData () + dstCol * 4;
	const float conversionValue = 1.0f/255.0f;

	for (int y=0;y<height;y++)
	{
		if(convertIntToFloat)
		{
			for (int i=0;i<4;i++)
				d[i] = ((float)s[i] + (float)s[srcNextLine + i]) * 0.5F * conversionValue;
		}
		else
		{
			for (int i=0;i<4;i++)
				d[i] = (s[i] + s[srcNextLine + i]) * 0.5F;
		}
		d += dstNextLine;
		s += srcNextLine * 2;
	}
}



class MiniHeightMap
{
	float* m_Data;
	int m_Width;
	int m_Height;

public:
	// floating point argb image
	MiniHeightMap (UInt8* image, int width, int height, int rowBytes, float scale)
	{
		m_Data = new float[width * height];
		float heightScale = scale / 3.0F;
		m_Height = height;
		m_Width = width;
		float* hexel = m_Data;
		for (int y=0;y < height;y++)
		{
			float* pixel = reinterpret_cast<float*> (image + rowBytes * y);
			for (int x=0;x<width;x++)
			{
				float gray = (pixel[1] + pixel[2] + pixel[3]);
				*hexel = gray * heightScale;
				hexel++;
				pixel += 4;
			}
		}
	}
	~MiniHeightMap() { delete[] m_Data; }

	float GetHeightRepeating (int x, int y)const
	{
		x = (x + m_Width) % m_Width;
		y = (y + m_Height) % m_Height;
		return m_Data[y * m_Width + x];
	}

	Vector3f CalculateNormalSimple (int x, int y)const
	{
		Vector3f normal;

		float dx, dy;
		dx  = GetHeightRepeating(x-1,y  ) * -4.0F;
		dx += GetHeightRepeating(x+1,y  ) *  4.0F;

		dy  = GetHeightRepeating(x  ,y-1) * -4.0F;
		dy += GetHeightRepeating(x  ,y+1) *  4.0F;

		normal.x = -dx;
		normal.y = -dy;
		normal.z = 1.0F;
		return Normalize (normal);
	}

	Vector3f CalculateNormalSobel (int x, int y)const
	{
		Vector3f normal;
		float dY, dX;
		// Do X sobel filter
		dX  = GetHeightRepeating (x-1, y-1) * -1.0F;
		dX += GetHeightRepeating (x-1, y  ) * -2.0F;
		dX += GetHeightRepeating (x-1, y+1) * -1.0F;
		dX += GetHeightRepeating (x+1, y-1) *  1.0F;
		dX += GetHeightRepeating (x+1, y  ) *  2.0F;
		dX += GetHeightRepeating (x+1, y+1) *  1.0F;

		// Do Y sobel filter
		dY  = GetHeightRepeating (x-1, y-1) * -1.0F;
		dY += GetHeightRepeating (x  , y-1) * -2.0F;
		dY += GetHeightRepeating (x+1, y-1) * -1.0F;
		dY += GetHeightRepeating (x-1, y+1) *  1.0F;
		dY += GetHeightRepeating (x  , y+1) *  2.0F;
		dY += GetHeightRepeating (x+1, y+1) *  1.0F;

		// Cross Product of components of gradient reduces to
		normal.x = -dX;
		normal.y = -dY;
		normal.z = 1.0F;

		normal = Normalize (normal);
		return normal;
	}

	template<bool SOBEL, bool DXT5NM>
	void ExtractNormalMap (UInt8* image, int rowBytes)
	{
		for (int y=0;y<m_Height;y++)
		{
			float* c = reinterpret_cast<float*> (image + rowBytes * y);
			for (int x=0;x<m_Width;x++)
			{
				Vector3f normal;
				if (SOBEL)
					normal = CalculateNormalSobel (x, y);
				else
					normal = CalculateNormalSimple (x, y);

				if (DXT5NM)
				{
					c[0] = (normal.x + 1.0F) * 0.5;
					c[1] = (normal.y + 1.0F) * 0.5;
					c[2] = (normal.y + 1.0F) * 0.5;
					c[3] = (normal.y + 1.0F) * 0.5;
				}
				else
				{
					c[0] = 1.0f;
					c[1] = (normal.x + 1.0F) * 0.5;
					c[2] = (normal.y + 1.0F) * 0.5;
					c[3] = (normal.z + 1.0F) * 0.5;
				}
				c += 4;
			}
		}
	}
};


void ConvertHeightToNormalMap (ImageReference& ref, float scale, bool sobelFilter, bool dxt5nm)
{
	scale = pow (10.0F, scale) * 0.01F - 0.01F;
	scale = scale * (ref.GetWidth () + ref.GetHeight ());

	AssertIf (ref.GetFormat () != kTexFormatARGBFloat);
	MiniHeightMap heightmap (ref.GetImageData (), ref.GetWidth (), ref.GetHeight (), ref.GetRowBytes (), scale);
	if (sobelFilter)
	{
		if (dxt5nm)
			heightmap.ExtractNormalMap<true,true>(ref.GetImageData(), ref.GetRowBytes());
		else
			heightmap.ExtractNormalMap<true,false>(ref.GetImageData(), ref.GetRowBytes());
	}
	else
	{
		if (dxt5nm)
			heightmap.ExtractNormalMap<false,true>(ref.GetImageData(), ref.GetRowBytes());
		else
			heightmap.ExtractNormalMap<false,false>(ref.GetImageData(), ref.GetRowBytes());
	}
}

template<typename T>
void ConvertExternalNormalMapToDXT5nmInternal (ImageReference& ref)
{
	int width = ref.GetWidth();
	int height = ref.GetHeight();
	int rowBytes = ref.GetRowBytes();
	UInt8* image = ref.GetImageData();

	for (int y=0;y<height;y++)
	{
		T* c = reinterpret_cast<T*> (image + rowBytes * y);
		for (int x=0;x<width;x++)
		{
			T red = c[1];
			T green = c[2];
			c[0] = red;
			c[1] = c[2] = c[3] = green;
			c+=4;
		}
	}
}

void ConvertExternalNormalMapToDXT5nm (ImageReference& ref)
{
	if(ref.GetFormat() == kTexFormatARGBFloat)
		ConvertExternalNormalMapToDXT5nmInternal<float>(ref);
	else
		ConvertExternalNormalMapToDXT5nmInternal<UInt8>(ref);
}


inline Vector2f OldSpheremap (Vector3f direction)
{
	direction = Normalize (direction);
	return Vector2f ((direction.x + 1.0F) * 0.5F, (direction.y + 1.0F) * 0.5F);
}

inline Vector2f GLSpheremap (Vector3f direction)
{
	direction = Normalize (direction);
	float m = 2.0 * sqrt (Sqr (direction.x) + Sqr (direction.y) + Sqr (direction.z + 1.0F));
	return Vector2f (direction.x / m + 0.5F, direction.y / m + 0.5F);
}

inline float CalculateAngleStupid (Vector2f normalized)
{
	float mag = Magnitude (normalized);
	if (mag > Vector3f::epsilon)
	{
		normalized /= mag;
		float angle = acos (Abs (normalized.x));
		if (normalized.x > 0.0)
			return 1.0 - angle / kPI;
		else
			return angle / kPI;
	}
	else
		return 0.0F;
}

inline Vector2f NiceSpheremap (Vector3f direction)
{
	direction = Normalize (direction);

	float yAngle = 1.0F - acos (direction.y) / kPI;

	float xAngle = acos (Abs (direction.z)) / kPI;
	if (direction.x > 0.0F)
		xAngle = 1.0F - xAngle;

	return Vector2f (xAngle, yAngle);
}

inline Vector2f Cylindricalmap2 (Vector3f direction)
{
	direction = Normalize (direction);

	Vector2f uv;
	uv.x = 0.75f - atan2(direction.z, direction.x) / kPI / 2;
	if (uv.x > 1)
		uv.x -= 1;
	uv.y = 1.0f - acos(direction.y) / kPI;
	return uv;
}


inline void CalculateBilinearFactors (float inFracX, float inFracY, float* inBlendFactors)
{
	float OneFracx = 1.0F - inFracX;
	float OneFracy = 1.0F - inFracY;
	inBlendFactors[0] = OneFracx * OneFracy;
	inBlendFactors[1] = inFracX * OneFracy;
	inBlendFactors[2] = OneFracx * inFracY;
	inBlendFactors[3] = inFracX * inFracY;
}

inline ColorRGBAf FetchPixelARGB32 (int x, int y, const ImageReference& source)
{
	UInt8* base = source.GetRowPtr (y) + x * sizeof (ColorRGBA32);
	return *reinterpret_cast<ColorRGBA32*> (base);
}

inline void RemapPixelBilinearARGB32 (const Vector2f& uv, void* destination, const ImageReference& source)
{
	int x1, y1;
	int x0 = FloorfToInt (uv.x);
	int y0 = FloorfToInt (uv.y);
	float fracx = uv.x - (float)x0;
	float fracy = uv.y - (float)y0;

	x0 = clamp (x0, 0, source.GetWidth () - 1);
	x1 = clamp (x0+1, 0, source.GetWidth () - 1);
	y0 = clamp (y0, 0, source.GetHeight () - 1);
	y1 = clamp (y0+1, 0, source.GetHeight () - 1);

	float bilinear[4];
	CalculateBilinearFactors (fracx, fracy, bilinear);

	ColorRGBAf c;
	c  = bilinear[0] * FetchPixelARGB32 (x0, y0, source);
	c += bilinear[1] * FetchPixelARGB32 (x1, y0, source);
	c += bilinear[2] * FetchPixelARGB32 (x0, y1, source);
	c += bilinear[3] * FetchPixelARGB32 (x1, y1, source);

	*reinterpret_cast<ColorRGBA32*> (destination) = c;
}

/* Opengl Cubemap specification
     major axis
      direction     target                        sc     tc    ma
      ----------    ----------------------------- ---    ---   ---
       +rx    TEXTURE_CUBE_MAP_POSITIVE_X_ARB     -rz    -ry   rx
       -rx    TEXTURE_CUBE_MAP_NEGATIVE_X_ARB     +rz    -ry   rx
       +ry    TEXTURE_CUBE_MAP_POSITIVE_Y_ARB     +rx    +rz   ry
       -ry    TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB     +rx    -rz   ry
       +rz    TEXTURE_CUBE_MAP_POSITIVE_Z_ARB     +rx    -ry   rz
       -rz    TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB     -rx    -ry   rz
*/

// Renders one face of a cubemap by looking up every pixel using a user defined uv remap function in another texture
// This is used to convert a spheremap/cylindrical to a cubemap
template<class RemapUVFunction, class WritePixel>
void RenderCubemapFace (ImageReference& cubemapFace, int direction, const ImageReference& input, const RemapUVFunction& remapuv, const WritePixel& writePixel)
{
	const int kCubeXRemap[6] = { 2, 2, 0, 0, 0, 0 };
	const int kCubeYRemap[6] = { 1, 1, 2, 2, 1, 1 };
	const int kCubeZRemap[6] = { 0, 0, 1, 1, 2, 2 };
	const float kCubeXSign[6] = { -1.0F,  1.0F,  1.0F,  1.0F,  1.0F, -1.0F };
	const float kCubeYSign[6] = { -1.0F, -1.0F,  1.0F, -1.0F, -1.0F, -1.0F };
	const float kCubeZSign[6] = {  1.0F, -1.0F,  1.0F, -1.0F,  1.0F, -1.0F };

	int width = cubemapFace.GetWidth ();
	int height = cubemapFace.GetHeight ();
	int sourceWidth = input.GetWidth ();
	int sourceHeight = input.GetHeight ();
	Vector2f sourceSize (sourceWidth, sourceHeight);
	// do the sign scale according to the cubemap specs then flip y sign
	Vector3f signScale = Vector3f (kCubeXSign[direction], kCubeYSign[direction], kCubeZSign[direction]);
	int byteSize = GetBytesFromTextureFormat (cubemapFace.GetFormat ());

	Vector2f invSize (1.0F / (float)width, 1.0F / (float)height);
	for (int y=0;y<height;y++)
	{
		UInt8* dest = cubemapFace.GetRowPtr (y);
		for (int x=0;x<width;x++)
		{
			Vector2f uv = Scale (Vector2f (x, y), invSize) * 2.0F - Vector2f (1.0F, 1.0F);
			Vector3f uvDir = Vector3f (uv.x, uv.y, 1.0F);

			uvDir.Scale (signScale);
			Vector3f worldDir;
			// Rotate the uv to the world direction using a table lookup
			worldDir[kCubeXRemap[direction]] = uvDir[0];
			worldDir[kCubeYRemap[direction]] = uvDir[1];
			worldDir[kCubeZRemap[direction]] = uvDir[2];

			// Map the world direction to a uv coordinate using user specified function
			Vector2f remappedUV = remapuv (worldDir);
			// scale from uv to pixel space
			remappedUV = Scale (remappedUV, sourceSize);

			writePixel (remappedUV, dest, input);
			dest += byteSize;
		}
	}
}

void ExtractCubemapFaceImage (ImageReference& cubemapFace, int direction, const ImageReference& input, bool verticalOrientation = true)
{
	const int sourceWidth = input.GetWidth ();
	const int sourceHeight = input.GetHeight ();

	const int sourceFaceWidth = sourceWidth / (verticalOrientation? 1 : 6);
	const int sourceFaceHeight = sourceHeight / (verticalOrientation? 6 : 1);

	// It looks like we need to swap +Z and -Z (handiness?) compared to standard layout

	// Vertical layout:
	//  X
	//  x
	//  Y
	//  y
	//  z
	//  Z
	const int kVerticalRemap[] = { 5, 4, 3, 2, 0, 1 }; // because source image is upside down!

	// Horizontal layout:
	// XxYyzZ
	const int kHorizontalRemap[] = { 0, 1, 2, 3, 5, 4 };

	const int *kCubeRemap = (verticalOrientation)? kVerticalRemap: kHorizontalRemap;

	ImageReference clipped = input.ClipImage ( verticalOrientation? 0 : kCubeRemap[direction] * sourceFaceWidth,
											  !verticalOrientation? 0 : kCubeRemap[direction] * sourceFaceHeight,
											  sourceFaceWidth, sourceFaceHeight);
	cubemapFace.BlitImage(clipped);

	// because source image is upside down
	// rotate all faces except +Y and -Y
	if (direction != 2 && direction != 3)
	{
		// 180 degree rotation
		cubemapFace.FlipImageY();
		cubemapFace.FlipImageX();
	}
}

void ExtractCubemapCrossImage (ImageReference& cubemapFace, int direction, const ImageReference& input, bool verticalOrientation = true)
{
	const int sourceWidth = input.GetWidth ();
	const int sourceHeight = input.GetHeight ();

	const int crossWidth = (verticalOrientation? 3 : 4);
	const int crossHeight = (verticalOrientation? 4 : 3);

	const int sourceFaceWidth = sourceWidth / crossWidth;
	const int sourceFaceHeight = sourceHeight / crossHeight;

	// Vertical cross:
	//  Y
	// xZX
	//  y
	//  z
	const int kVerticalCrossRemap[] = { 6, 8, 10, 4, 7, 1 }; // because source image is upside down!

	// Horizontal cross:
	//  Y
	// xZXz
	//  y
	const int kHorizontalCrossRemap[] = { 4, 6, 9, 1, 5, 7 };

	const int *kCubeRemap = (verticalOrientation)? kVerticalCrossRemap: kHorizontalCrossRemap;

	const int x = kCubeRemap[direction] % crossWidth;
	const int y = kCubeRemap[direction] / crossWidth;

	ImageReference clipped = input.ClipImage (x * sourceFaceWidth, y * sourceFaceHeight, sourceFaceWidth, sourceFaceHeight);
	cubemapFace.BlitImage(clipped);

	// because source image is upside down
	// rotate all faces except +Z in vertical orienatation
	if (!(verticalOrientation && direction == 5))
	{
		// 180 degree rotation
		cubemapFace.FlipImageY();
		cubemapFace.FlipImageX();
	}
}

void GenerateCubemapFaceFromImage (const ImageReference& source, ImageReference& cubemap, CubemapGenerationMode mode, CubemapLayoutMode layout, int face)
{
	Assert ((mode == kGenerateFullCubemap) ||
			(mode != kGenerateFullCubemap && layout == kLayoutSingleImage));

	switch (mode)
	{
	case kGenerateOldSpheremap: RenderCubemapFace (cubemap, face, source, &OldSpheremap, &RemapPixelBilinearARGB32); break;
	case kGenerateSpheremap: RenderCubemapFace (cubemap, face, source, &GLSpheremap, &RemapPixelBilinearARGB32); break;
	case kGenerateNiceSpheremap: RenderCubemapFace (cubemap, face, source, &NiceSpheremap, &RemapPixelBilinearARGB32); break;
	case kGenerateFullCubemap:
		switch (layout & kLayoutTypeMask)
		{
		case kLayoutSingleImage: cubemap.BlitImage (source); break;
		case kLayoutFaces: ExtractCubemapFaceImage (cubemap, face, source, layout & kLayoutOrientationMask); break;
		case kLayoutCross: ExtractCubemapCrossImage (cubemap, face, source, layout & kLayoutOrientationMask); break;
		} break;
	default: RenderCubemapFace (cubemap, face, source, &Cylindricalmap2, &RemapPixelBilinearARGB32); break;
	}
}


static bool DoesTextureNeedAlphaChannel (UInt8* image, int width, int height, int rowbytes)
{
	long	Usage[4][256];
	memset(Usage, 0, sizeof(Usage));

	// calculate the histogram
	for(int y=0; y<height; y++)
	{
		UInt8* pixel = image + y * rowbytes;
		for(int x=0; x<width; x++)
		{
			Usage[0][pixel[0]]++;
			Usage[1][pixel[1]]++;
			Usage[2][pixel[2]]++;
			Usage[3][pixel[3]]++;
			pixel += 4;
		}
	}

	// count the number of unique values per channel
	int Unique[4] = {0,0,0,0};
	for (int i=0;i<4;i++)
	{
		for(int x=0; x<256; x++)
			Unique[i] += (Usage[i][x] != 0);
	}

	// alpha channel is needed if we have more than one unique value or
	// if we have exactly one alpha value that is different than white
	return Unique[0] != 1 || (Unique[0] == 1 && Usage[0][255] == 0);
}

static bool DoesFloatTextureNeedAlphaChannel (const ImageReference& image)
{
	DebugAssert (image.GetFormat () == kTexFormatARGBFloat);

	int width = image.GetWidth ();
	int height = image.GetHeight ();
	int rowBytes = image.GetRowBytes ();

	UInt8* imageData = image.GetImageData ();

	for (int y = 0; y < height; y++)
	{
		float* pixel = (float*)(imageData + y * rowBytes);
		for (int x = 0; x < width; x++)
		{
			if (*pixel < 1.0f)
			{
				return true;
			}
			pixel += 4;
		}
	}

	return false;
}

bool DoesTextureContainAlpha (const ImageReference& image, TextureUsageMode usageMode)
{
	if (image.GetFormat() == kTexFormatARGB32)
	{
		return DoesTextureNeedAlphaChannel (image.GetImageData (), image.GetWidth(), image.GetHeight(), image.GetRowBytes ());
	}
	else
	{
		if (usageMode == kTexUsageLightmapRGBM)
			return true;
		else
			return DoesFloatTextureNeedAlphaChannel (image);
	}
}
static bool DoesTextureNeedColorUInt8 (UInt8* image, int width, int height, int rowbytes)
{
	// calculate the histogram
	for(int y=0; y < height; y++)
	{
		UInt8* pixel = image + y * rowbytes;
		for(int x = 0; x < width; x++)
		{
			if (pixel[1] != pixel[2] || pixel[2] != pixel[3] || pixel[1] != pixel[3]) return true;
			pixel += 4;
		}
	}
	return false;
}
static bool DoesTextureNeedColorFloat (UInt8* image, int width, int height, int rowbytes)
{
	// calculate the histogram
	for(int y=0; y < height; y++)
	{
		float* pixel = (float*)(image + y * rowbytes);
		for(int x = 0; x < width; x++)
		{
			if (CompareApproximately(pixel[1], pixel[2]) == false ||
				CompareApproximately(pixel[2], pixel[3]) == false ||
				CompareApproximately(pixel[1], pixel[3]) == false) return true;
			pixel += 4;
		}
	}
	return false;
}
bool DoesTextureContainColor (const ImageReference& image)
{
	if (image.GetFormat() == kTexFormatARGB32)
	{
		return DoesTextureNeedColorUInt8 (image.GetImageData (), image.GetWidth(), image.GetHeight(), image.GetRowBytes ());
	}
	else if (image.GetFormat() == kTexFormatARGBFloat)
	{
		return DoesTextureNeedColorFloat (image.GetImageData (), image.GetWidth(), image.GetHeight(), image.GetRowBytes ());
	}
	else
	{
		return true;
	}
}

const float kOneOverRGBMMaxRange = 1.0f / (float)kRGBMMaxRange;

void ConvertImageToRGBM (ImageReference& ref)
{
	Assert(ref.GetFormat() == kTexFormatARGBFloat);
	int width = ref.GetWidth();
	int height = ref.GetHeight();
	int rowBytes = ref.GetRowBytes();
	UInt8* image = ref.GetImageData();

	const float kMinMultiplier = 2.0f * 1e-2f;

	for (int y=0;y<height;y++)
	{
		float* c = reinterpret_cast<float*> (image + rowBytes * y);
		for (int x=0;x<width;x++)
		{
			float r = c[1] * kOneOverRGBMMaxRange;
			float g = c[2] * kOneOverRGBMMaxRange;
			float b = c[3] * kOneOverRGBMMaxRange;
			float a = std::max(std::max(r, g), std::max(b, kMinMultiplier));
			a = ceilf(a * 255.0f) / 255.0f;

			c[0] = std::min(a, 1.0f);
			c[1] = std::min(r / a, 1.0f);
			c[2] = std::min(g / a, 1.0f);
			c[3] = std::min(b / a, 1.0f);
			c+=4;
		}
	}
}

void ConvertImageToDoubleLDR (ImageReference& ref)
{
	Assert(ref.GetFormat() == kTexFormatARGBFloat);
	int width = ref.GetWidth();
	int height = ref.GetHeight();
	int rowBytes = ref.GetRowBytes();
	UInt8* image = ref.GetImageData();

	for (int y=0;y<height;y++)
	{
		float* c = reinterpret_cast<float*> (image + rowBytes * y);
		for (int x=0;x<width;x++)
		{
			// not used
			c[0] = 0.0f;

			c[1] = std::min(c[1] * 0.5f, 1.0f);
			c[2] = std::min(c[2] * 0.5f, 1.0f);
			c[3] = std::min(c[3] * 0.5f, 1.0f);
			c+=4;
		}
	}
}

void ConvertImageToARGB32 (const ImageReference& src, Image& temporary32BitImage, TextureFormat destTextureFormat)
{
	// Create 32 bit image (BGRA for endianess conversion) scale
	// minimum size for a DXTn texture is 4*4
	temporary32BitImage.SetImage(src.GetWidth(), src.GetHeight(), kTexFormatARGB32, false);
	temporary32BitImage.BlitImage (src);
	int minSize = GetMinimumTextureMipSizeForFormat(destTextureFormat);
	temporary32BitImage.ReformatImage (std::max (minSize, src.GetWidth()), std::max (minSize, src.GetHeight()), temporary32BitImage.GetFormat (), ImageReference::BLIT_BILINEAR_SCALE);
}

void PerformTextureCompression (const Image& srcImage, TextureFormat destTextureFormat, UInt32* destImageData)
{
	Assert (srcImage.GetFormat() == kTexFormatARGB32);
#if PRINT_TEXTURE_COMPRESSION_TIMES
	double t1 = GetTimeSinceStartup();
#endif

	PerformTextureCompression (srcImage.GetWidth (), srcImage.GetHeight (), srcImage.GetImageData (), destImageData, destTextureFormat);
#if PRINT_TEXTURE_COMPRESSION_TIMES
	if( srcImage.GetWidth() >= 256 && srcImage.GetHeight() >= 256 )
	{
		double t2 = GetTimeSinceStartup();
		printf_console( "  DXT compression: %i x %i took %.2fs\n", srcImage.GetWidth(), srcImage.GetHeight(), t2-t1 );
	}
#endif
}

// Replaces the border pixels of the destination image.
// The border pixels are filtered only from the src border pixels.
template<typename T, bool convertIntToFloat>
static void ReplaceBorderHalveImage(const ImageReference& src, ImageReference& dst, int borderRows, int borderCols)
{
	// rows
	for (int q = 0; q < borderRows; ++q)
	{
		ReplaceRowHalveImage<T, convertIntToFloat>(src, 0, dst, q);
		ReplaceRowHalveImage<T, convertIntToFloat>(src, src.GetHeight () - 1, dst, dst.GetHeight () - 1 - q);
	}

	// columns
	for (int q = 0; q < borderCols; ++q)
	{
		ReplaceColumnHalveImage<T, convertIntToFloat>(src, 0, dst, q);
		ReplaceColumnHalveImage<T, convertIntToFloat>(src, src.GetWidth () - 1, dst, dst.GetWidth () - 1 - q);
	}
}

template<typename T, bool convertIntToFloat>
void ApplyMipBorder (const ImageReference& srcImage, ImageReference& dstImage, TextureFormat dstFormat)
{
	int borderRows = 1, borderCols = 1;
	if (IsCompressedPVRTCTextureFormat(dstFormat))
	{
		// PVRTC and especially PVRTC2bpp bleeds alot,
		// as a workaround we make much larger borders (equal to half size of the compression block).
		int border = (dstFormat == kTexFormatPVRTC_RGB2 || dstFormat == kTexFormatPVRTC_RGBA2)? 4: 2;

		if (dstImage.GetWidth () <= border*2)
			borderCols = border;

		if (dstImage.GetHeight () <= border*2)
			borderRows = border;
	}

	ReplaceBorderHalveImage<T, convertIntToFloat> (srcImage, dstImage, borderRows, borderCols);
}


template<typename T, bool convertIntToFloat>
static void HalveImageBox (const ImageReference& src, ImageReference& dst)
{
	DebugAssertIf( src.GetWidth() / 2 != dst.GetWidth() && src.GetWidth() != 1 && dst.GetWidth() != 1 );
	DebugAssertIf( src.GetHeight() / 2 != dst.GetHeight() && src.GetHeight() != 1 && dst.GetHeight() != 1 );

	int width = dst.GetWidth ();
	int height = dst.GetHeight ();

	// detect width or height of 1 and make sure we don't go over the bounds
	int oldComp = src.GetWidth () == 1 ? 0 : 4;
	int oldWidthxComp = src.GetHeight () == 1 ? 0 : src.GetWidth () * oldComp;
	int oldWidthxCompPlusComp = oldWidthxComp + oldComp;

	const T* srow = (const T*)src.GetImageData ();
	float* d = (float*)dst.GetImageData ();

	const float conversionValue = 1.0f/255.0f;

	for (int y=0;y<height;y++)
	{
		const T* s = srow;
		for (int x=0;x<width;x++)
		{
			for (int i=0;i<4;i++)
			{
				// Attemp to make gcc and VS produce similar code
				float average;

				average = *(s);
				average += *(s + oldComp);
				average += *(s + oldWidthxComp);
				average += *(s + oldWidthxCompPlusComp);

				if(convertIntToFloat)
					average *= 0.25f * conversionValue;
				else
					average *= 0.25f;
				*d = average;
				s++;
				d++;
			}
			s += 4;
		}
		// March the rows separately from pixels. In non-power-of-2 cases
		// source size might not exactly be 2x the destination size, that's why.
		srow += oldWidthxComp + oldWidthxComp; // next two rows
	}
}

static void HalveImageBoxLightmap (const ImageReference& src, ImageReference& dst)
{
	DebugAssert (src.GetWidth() == dst.GetWidth() * 2 || src.GetWidth() == 1);
	DebugAssert (src.GetHeight()== dst.GetHeight() * 2 || src.GetHeight() == 1);
	DebugAssert (src.GetFormat() == kTexFormatARGBFloat);

	const int width = dst.GetWidth ();
	const int height = dst.GetHeight ();
	const int channels = 4;
	const int samples = 4;

	// detect width or height of 1 and make sure we don't go over the bounds
	const int nextCol = src.GetWidth () == 1 ? 0 : channels;
	const int nextRow = src.GetHeight () == 1 ? 0 : src.GetWidth () * channels;
	const int nextRowNextCol = nextRow + nextCol;

	const float* srow = (const float*)src.GetImageData ();
	float* d = (float*)dst.GetImageData ();

	for (int y = 0; y < height; y++)
	{
		const float* s = srow;
		for (int x = 0; x < width; x++)
		{
			for (int channel = 0; channel < channels; channel++)
			{
				float values[samples];
				values[0] = *(s);
				values[1] = *(s + nextCol);
				values[2] = *(s + nextRow);
				values[3] = *(s + nextRowNextCol);

				float sum = 0.0f;
				int valuesUsed = 0;

				for (int j = 0; j < samples; j++)
				{
					if (values[j] >= 0.0f)
					{
						sum += values[j];
						valuesUsed++;
					}
				}

				*d = (valuesUsed == 0) ? -1.0f : (sum / (float)valuesUsed);

				// jump to next channel
				s++;
				d++;
			}
			// s just went over the channels of the first column,
			// skip over the second column so the kernel can grab new source
			s += nextCol;
		}
		srow += nextRow + nextRow;
	}
}

void ComputePrevMipLevelLightmap (const ImageReference& src, ImageReference& dst)
{
	DebugAssert (dst.GetWidth() == src.GetWidth() * 2 || dst.GetWidth() == 1);
	DebugAssert (dst.GetHeight()== src.GetHeight() * 2 || dst.GetHeight() == 1);
	DebugAssert (dst.GetFormat() == kTexFormatARGBFloat);

	const int width = src.GetWidth ();
	const int height = src.GetHeight ();
	const int channels = 4;

	// detect width or height of 1 and make sure we don't go over the bounds
	const int nextCol = dst.GetWidth () == 1 ? 0 : channels;
	const int nextRow = dst.GetHeight () == 1 ? 0 : dst.GetWidth () * channels;
	const int nextRowNextCol = nextRow + nextCol;

	float* drow = (float*)dst.GetImageData ();
	const float* s = (const float*)src.GetImageData ();

	for (int y = 0; y < height; y++)
	{
		float* d = drow;
		for (int x = 0; x < width; x++)
		{
			for (int channel = 0; channel < channels; channel++)
			{
				// If the destination value is the background replace it with the source.
				// The smallest mip is guaranteed not to be the background if the source
				// lightmap contained at least one pixel that was not the background.
				if (*(d) < 0.0f)
					*(d) = *s;
				if (*(d + nextCol) < 0.0f)
					*(d + nextCol) = *s;
				if (*(d + nextRow) < 0.0f)
					*(d + nextRow) = *s;
				if (*(d + nextRowNextCol) < 0.0f)
					*(d + nextRowNextCol) = *s;

				// jump to next channel
				d++;
				s++;
			}
			// d just went over the channels of the first column,
			// skip over the second column so the kernel can grab new source
			d += nextCol;
		}
		drow += nextRow + nextRow;
	}
}

inline int GetReflected (int i, int width)
{
    while ((i < 0) || (i > (width - 1))) {
        if (i < 0) i = -i -1;
        if (i >= width) i = width + width - i - 1;
    }
	DebugAssertIf( i < 0 || i >= width );
	return i;
}


struct Kernel1D {
	Kernel1D(int width);
	~Kernel1D();

	void operator = (const Kernel1D& kernel);

	void Normalize();

	float *data;
	int nsamples;
};

template<typename T, bool convertIntToFloat>
static void HalveImageFloat (const ImageReference& srcImage, ImageReference& dstImage, const Kernel1D& kernel)
{
	enum { kComponentCount = 4 };

	AssertIf( dstImage.GetFormat () != kTexFormatARGBFloat );
	int width = srcImage.GetWidth ();
	int height = srcImage.GetHeight ();

	int halfWidth = std::max (width / 2, 1);
	int halfHeight = std::max (height / 2, 1);
	AssertIf( halfWidth != dstImage.GetWidth() || halfHeight != dstImage.GetHeight() );

	Image tempImage( halfWidth, height, kTexFormatARGBFloat ); // 2x narrower

	int kernelSize = kernel.nsamples;
	int filter_offset = -kernelSize / 2 + 1;

	T* srcData = reinterpret_cast<T*> (srcImage.GetImageData ());
	float* dstData = reinterpret_cast<float*> (dstImage.GetImageData ());
	float* tempData = reinterpret_cast<float*> (tempImage.GetImageData ());

	const float conversionValue = 1.0f/255.0f;

	int srcRowBytes = srcImage.GetRowBytes () / sizeof (T);
	int dstRowBytes = dstImage.GetRowBytes () / sizeof (float);
	int tempRowBytes = tempImage.GetRowBytes () / sizeof (float);

	int i, j, k;

	// Here be extra careful and step over destination pixels, accessing source as index*2.
	// If done the other way around (step over source and access destination as index/2) we get
	// out of bounds errors with NPOT sizes.

	// horizontal pass srcData -> tempData
	for( j = 0; j < height; j++ )
	{
		int destRowIndex = j * tempRowBytes;
		int srcRowIndex = j * srcRowBytes;
		for( i = 0; i < halfWidth; ++i )
		{
			float sum[kComponentCount] = { 0.0F, 0.0F, 0.0F, 0.0F };

			for (k = 0; k < kernelSize; k++)
			{
				int src_i = i*2 + k + filter_offset;
				int src_index = GetReflected (src_i, width) * kComponentCount + srcRowIndex;
				DebugAssertIf( src_index >= width * height * kComponentCount );
				for( int comp = 0; comp < kComponentCount; ++comp )
				{
					if(convertIntToFloat)
						sum[comp] += kernel.data[k] * srcData[src_index + comp] * conversionValue;
					else
						sum[comp] += kernel.data[k] * srcData[src_index + comp];
				}
			}

			int destIndex = destRowIndex + i * kComponentCount;
			DebugAssertIf( destIndex >= halfWidth * height * kComponentCount );
			for( int comp = 0; comp < kComponentCount; ++comp )
				tempData[destIndex + comp] = sum[comp];
		}
	}

	// vertical pass tempData -> dstData
	for( i = 0; i < halfWidth; ++i )
	{
		for( j = 0; j < halfHeight; ++j )
		{
			float sum[kComponentCount] = { 0.0F, 0.0F, 0.0F, 0.0F };

			for (k = 0; k < kernelSize; k++)
			{
				int src_j = j*2 + k + filter_offset;
				int src_index = i * kComponentCount + GetReflected (src_j, height) * tempRowBytes;
				DebugAssertIf( src_index >= halfWidth * height * kComponentCount );
				for( int comp = 0; comp < kComponentCount; ++comp )
					sum[comp] += kernel.data[k] * tempData[src_index + comp];
			}

			int dest_index = i * kComponentCount + j * dstRowBytes;
			DebugAssertIf( dest_index >= halfWidth * halfHeight * kComponentCount );
			for( int comp = 0; comp < kComponentCount; ++comp )
				dstData[dest_index + comp] = sum[comp];
		}
	}
}

Kernel1D::Kernel1D(int _width) {
	nsamples = _width;
	data = new float[nsamples];
	for (int i = 0; i < nsamples; i++) data[i] = 0;
}

Kernel1D::~Kernel1D() {
	delete [] data;
}

void Kernel1D::operator = (const Kernel1D& kernel)
{
	nsamples = kernel.nsamples;
	delete [] data;
	data = new float[nsamples];
	for (int i = 0; i < nsamples; i++) data[i] = kernel.data[i];
}

void Kernel1D::Normalize() {
	float sum = 0.0F;
	for (int i = 0; i < nsamples; i++) sum += data[i];

	AssertIf (sum == 0.0F);
	if (sum == 0.0F) return;

	for (int i = 0; i < nsamples; i++) data[i] /= sum;
}


inline double sinc(double x) {
    if (x == 0.0) return 1.0;
    return sin(kPI * x) / (kPI * x);
}

inline double
bessel0(double x) {
  const double EPSILON_RATIO = 1E-16;
  double xh, sum, pow, ds;
  int k;

  xh = 0.5 * x;
  sum = 1.0;
  pow = 1.0;
  k = 0;
  ds = 1.0;
  while (ds > sum * EPSILON_RATIO) {
    ++k;
    pow = pow * (xh / k);
    ds = pow * pow;
    sum = sum + ds;
  }

  return sum;
}

inline double kaiser(double alpha, double half_width, double x) {
    double ratio = (x / half_width);
    return bessel0(alpha * sqrt(1 - ratio * ratio)) / bessel0(alpha);
}

static void FillKaiserFilter(Kernel1D *filter, double alpha, double additional_stretch)
{
	int width = filter->nsamples;
	AssertIf( width & 1 );

	float half_width = (float)(width / 2);
	float offset = -half_width;
	float nudge = 0.5f;

	float STRETCH = additional_stretch;

	for (int i = 0; i < width; i++)
	{
		float x = (i + offset) + nudge;

		double sinc_value = sinc(x * STRETCH);
		double window_value = kaiser(alpha, half_width, x * STRETCH);

		filter->data[i] = sinc_value * window_value;
	}

	filter->Normalize();
}


template<typename T, bool convertIntToFloat>
static void HalveImageFloatKaiser (const ImageReference& srcImage, ImageReference& dstImage)
{
	const int kGoodFilterWidth = 10;
	const float kKaiserAlpha = 4.0;
	Kernel1D kernel (kGoodFilterWidth);
	FillKaiserFilter (&kernel, kKaiserAlpha, 1.0);
	HalveImageFloat<T, convertIntToFloat> (srcImage, dstImage, kernel);
}


void ComputeNextMipLevel (const ImageReference& src, ImageReference& dst, bool kaiserFilter, bool borderMips, TextureFormat dstFormat)
{
	// For NPOT textures dest size might not be exactly 2x the source size. Box filter
	// (and the others) will lose the last row/column of pixels, so instead we revert to full
	// bilinear here.
	if( src.GetWidth() % 2 != 0 || src.GetHeight() % 2 != 0 )
	{
		dst.BlitImage( src, ImageReference::BLIT_BILINEAR_SCALE );
	}
	else
	{
		// Next mip is exactly 2x smaller. Do full filters here.
		if(src.GetFormat() == kTexFormatARGBFloat)
		{
			if (kaiserFilter)
				HalveImageFloatKaiser<float, false> (src, dst);
			else
				HalveImageBox<float, false> (src, dst);
		}
		else
		{
			if (kaiserFilter)
				HalveImageFloatKaiser<UInt8, true> (src, dst);
			else
				HalveImageBox<UInt8, true> (src, dst);
		}
	}

	if (borderMips)
	{
		if(src.GetFormat() == kTexFormatARGBFloat)
			ApplyMipBorder<float, false> (src, dst, dstFormat);
		else
			ApplyMipBorder<UInt8, true> (src, dst, dstFormat);
	}
}

void ComputeNextMipLevelLightmap (const ImageReference& src, Image& dst)
{
	dst.SetImage (std::max (1, src.GetWidth () / 2), std::max (1, src.GetHeight () / 2), src.GetFormat (), false);
	HalveImageBoxLightmap (src, dst);
}


void LinearToGammaSpaceXenon (ImageReference& srcImage)
{
	Assert (srcImage.GetFormat () == kTexFormatARGBFloat);

	for (int y=0;y<srcImage.GetHeight ();y++)
	{
		float* pixel = reinterpret_cast<float*> (srcImage.GetRowPtr (y));
		int width = srcImage.GetWidth ();
		for (int x=0;x<width;x++)
		{
			// Touch only RGB
			pixel++;
			for (int component=0;component<3;component++)
			{
				*pixel = LinearToGammaSpaceXenon (*pixel);
				pixel++;
			}
		}
	}
}


void SRGB_GammaToLinear (ImageReference& srcImage)
{
	Assert (srcImage.GetFormat () == kTexFormatARGBFloat);

	for (int y=0;y<srcImage.GetHeight ();y++)
	{
		float* pixel = reinterpret_cast<float*> (srcImage.GetRowPtr (y));
		int width = srcImage.GetWidth ();
		for (int x=0;x<width;x++)
		{
			// Touch only RGB
			pixel++;
			for (int component=0;component<3;component++)
			{
				*pixel = GammaToLinearSpace (*pixel);
				pixel++;
			}
		}
	}
}

void SRGB_LinearToGamma (ImageReference& srcImage)
{
	Assert (srcImage.GetFormat () == kTexFormatARGBFloat);

	for (int y=0;y<srcImage.GetHeight ();y++)
	{
		float* pixel = reinterpret_cast<float*> (srcImage.GetRowPtr (y));
		int width = srcImage.GetWidth ();
		for (int x=0;x<width;x++)
		{
			// Touch only RGB
			pixel++;
			for (int component=0;component<3;component++)
			{
				*pixel = LinearToGammaSpace(*pixel);
				pixel++;
			}
		}
	}
}




// --------------------------------------------------------------------------
//  texture compression


struct TextureCompressionData
{
	int width, height;
	void *input, *output;
	int compression;
};

void *TextureCompressionJob (void *voidData)
{
	TextureCompressionData &data = *(TextureCompressionData*)voidData;
	ConvertRawImageToS3TC (data.width, data.height, data.input, data.output, data.compression);
	return NULL;
}

#define MAX_TEXTURE_COMPRESSION_JOBS 128


void PerformTextureCompression (int width, int height, void* input, void* output, int compression)
{
	#if ENABLE_MULTITHREADED_DXT_COMPRESSION
	// DXT data is 4x4 blocks, so height cannot be split smaller than 4.
	int numberOfJobs = std::min(MAX_TEXTURE_COMPRESSION_JOBS, height/4);
	int sectionHeight = height/numberOfJobs;
	sectionHeight -= sectionHeight % 4;

	int imageSectionSize = sizeof(UInt32)*width*sectionHeight;
	int compressedSectionSize = CalculateCompressedSize(width, sectionHeight, compression);

	JobScheduler& js = GetJobScheduler();
	JobScheduler::JobGroupID group = js.BeginGroup (numberOfJobs);

	TextureCompressionData jobs[MAX_TEXTURE_COMPRESSION_JOBS];

	for (int i=0; i<numberOfJobs; i++)
	{
		jobs[i].width = width;
		if (i == numberOfJobs-1)
			jobs[i].height = height - sectionHeight * i;
		else
			jobs[i].height = sectionHeight;
		jobs[i].input = input;
		jobs[i].output = output;
		jobs[i].compression = compression;
		input = (UInt8*)input + imageSectionSize;
		output = (UInt8*)output + compressedSectionSize;
		js.SubmitJob (group, TextureCompressionJob, jobs+i, NULL);
	}

	js.WaitForGroup (group);

	#else

	ConvertRawImageToS3TC (width, height, input, output, compression);

	#endif
}

static bool CompressToPVR(const ImageReference& src, Image& buffer, TextureFormat textureFormat, int compressionQuality, UInt8* compressedTextureBuffer)
{
#if USE_PVR_STANDALONE_TOOL

	buffer.FlipImageY();

	if (!SaveImageToFile(buffer.GetImageData(), buffer.GetWidth(), buffer.GetHeight(), kTexFormatRGBA32, "Temp/PVRT-Feed.png", 'PNGf'))
		return false;

	std::vector<std::string>	args;

	args.push_back("-f");
	if(textureFormat == kTexFormatPVRTC_RGB4 || textureFormat == kTexFormatPVRTC_RGBA4)
		args.push_back("PVRTC1_4");
	else
		args.push_back("PVRTC1_2");

	args.push_back("-q");
	switch(compressionQuality)
	{
		case kTexCompressionFast:	args.push_back("pvrtcfast");	break;
		case kTexCompressionNormal:	args.push_back("pvrtcnormal");	break;
		case kTexCompressionBest:	args.push_back("pvrtcbest");	break;

		default:					args.push_back("pvrtcnormal");	break;
	}

	args.push_back("-i");
	args.push_back("Temp/PVRT-Feed.png");
	args.push_back("-o");
	args.push_back("Temp/PVRT.pvr");

	DeleteFile("Temp/PVRT.pvr");

#if UNITY_WIN
	static const char* exe_name = "Tools/PVRTexTool.exe";
#else
	static const char* exe_name = "Tools/PVRTexTool";
#endif
	std::string appPath = AppendPathName(GetApplicationContentsPath(), exe_name);
	if(LaunchTaskArray (appPath, NULL, args, true) == false || IsFileCreated("Temp/PVRT.pvr") == false)
		return false;

	File file;
	file.Open("Temp/PVRT.pvr", File::kReadPermission);/// error handling
	int length = GetFileLength("Temp/PVRT.pvr");
	int expectedImageSize = CalculateImageSize (src.GetWidth(), src.GetHeight(), textureFormat);
	Assert(expectedImageSize + 52 == length || (expectedImageSize < 64 && length == 64 + 52)); // small files are padded up
	file.Read(52, compressedTextureBuffer, expectedImageSize); // 52 bytes == header which we skip!
	file.Close();

#else

	pvrtexture::CPVRTextureHeader	srcHeader(pvrtexture::PVRStandard8PixelType.PixelTypeID, buffer.GetWidth(), buffer.GetHeight());
	pvrtexture::CPVRTexture 		pvrTexture(srcHeader);

	::memcpy(pvrTexture.getDataPtr(), buffer.GetImageData(), pvrTexture.getHeader().getDataSize(0));

	pvrtexture::PixelType dstPVRFormat = pvrtexture::PixelType(ePVRTPF_PVRTCI_4bpp_RGBA);
	switch(textureFormat)
	{
		case kTexFormatPVRTC_RGB4:	dstPVRFormat = pvrtexture::PixelType(ePVRTPF_PVRTCI_4bpp_RGB);	break;
		case kTexFormatPVRTC_RGBA4:	dstPVRFormat = pvrtexture::PixelType(ePVRTPF_PVRTCI_4bpp_RGBA);	break;
		case kTexFormatPVRTC_RGB2:	dstPVRFormat = pvrtexture::PixelType(ePVRTPF_PVRTCI_2bpp_RGB);	break;
		case kTexFormatPVRTC_RGBA2:	dstPVRFormat = pvrtexture::PixelType(ePVRTPF_PVRTCI_2bpp_RGBA);	break;
	}

	pvrtexture::ECompressorQuality dstPVRQuality = pvrtexture::ePVRTCNormal;
	switch(compressionQuality)
	{
		case kTexCompressionFast:	dstPVRQuality = pvrtexture::ePVRTCFast;		break;
		case kTexCompressionNormal:	dstPVRQuality = pvrtexture::ePVRTCNormal;	break;
		case kTexCompressionBest:	dstPVRQuality = pvrtexture::ePVRTCBest;		break;
	}

	if( !pvrtexture::Transcode(pvrTexture, dstPVRFormat, ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB, dstPVRQuality) )
		return false;

	// we need to copy needed amount of data
	// TODO: this is wrong - we blit-scaled full image in there
	int expectedImageSize = CalculateImageSize(src.GetWidth(), src.GetHeight(), textureFormat);
	::memcpy(compressedTextureBuffer, pvrTexture.getDataPtr(), expectedImageSize);

#endif

	return true;
}


// Handle ETC1 texture compression. Split out from CompressSingleImageTexture as the function was getting rather large.
static void HandleETC1TextureCompression(ImageReference &src, TextureFormat textureFormat, int compressionQuality, UInt8* compressedTextureBuffer, const Texture2D* reportTex)
{
	// \todo [2010-02-09 petri] Should be a warning instead of an assert!
	AssertIf(HasAlphaTextureFormat(textureFormat));

	// Create 24-bit image (RGB for endianess conversion) scale
	// minimum size for an ETC texture is 4*4
	Image tmpImage (src.GetWidth(), src.GetHeight(), kTexFormatRGB24);
	tmpImage.BlitImage (src);

	int minSize = GetMinimumTextureMipSizeForFormat(textureFormat);
	tmpImage.ReformatImage (std::max (minSize, src.GetWidth()), std::max (minSize, src.GetHeight()), tmpImage.GetFormat (), ImageReference::BLIT_BILINEAR_SCALE);

#if PRINT_TEXTURE_COMPRESSION_TIMES
	double t1 = GetTimeSinceStartup();
#endif

	tmpImage.FlipImageY();
	DeleteFile("Temp/ETC-Feed.ppm");
	if (!SaveImageToFile(tmpImage.GetImageData(), tmpImage.GetWidth(), tmpImage.GetHeight(), tmpImage.GetFormat (), "Temp/ETC-Feed.ppm", 'PPMf'))
	{
		ErrorStringObject("Failed writing conversion file during compression", reportTex);
	}

	std::vector<std::string>	args;

	args.push_back("-s");
	args.push_back("fast");
	args.push_back("-e");
	args.push_back("perceptual");

	DeleteFile("Temp/ETC-Result.pkm");
	std::string output;

#if UNITY_WIN
	args.push_back("Temp\\ETC-Feed.ppm");
	args.push_back("Temp\\ETC-Result.pkm");
	std::string appPath = AppendPathName(GetApplicationContentsPath(), "Tools/etcpack.exe");
#elif UNITY_OSX || UNITY_LINUX
	args.push_back("Temp/ETC-Feed.ppm");
	args.push_back("Temp/ETC-Result.pkm");
	std::string appPath = AppendPathName(GetApplicationContentsPath(), "Tools/etcpack");
#else
#error Unknown Platform
#endif

	if (LaunchTaskArray (appPath, &output, args, false) == false)
	{
		ErrorStringMsg("Failed to execute %s", appPath.c_str());
	}

	if (IsFileCreated("Temp/ETC-Result.pkm"))
	{
		File file;
		int length = GetFileLength("Temp/ETC-Result.pkm");
		int expectedImageSize = CalculateImageSize (src.GetWidth(), src.GetHeight(), textureFormat);
		AssertIf(expectedImageSize + 16 != length);
		file.Open("Temp/ETC-Result.pkm", File::kReadPermission);/// error handling
		file.Read(16, compressedTextureBuffer, expectedImageSize); // 16 bytes == header which we skip!
		file.Close();
	}
	else
	{
		ErrorStringObject("Failed to generate ETC texture", reportTex);
	}

	DeleteFile("Temp/ETC-Feed.ppm");
	DeleteFile("Temp/ETC-Result.pkm");

#if PRINT_TEXTURE_COMPRESSION_TIMES
	if( src.GetWidth() >= 256 && src.GetHeight() >= 256 )
	{
		double t2 = GetTimeSinceStartup();
		printf_console( "  ETC compression: %i x %i took %.2fs\n", src.GetWidth(), src.GetHeight(), t2-t1 );
	}
#endif
}

// Handle ETC2 texture compression. Split out from CompressSingleImageTexture as the function was getting rather large.
static void HandleETC2TextureCompression(ImageReference &src, TextureFormat textureFormat, int compressionQuality, UInt8* compressedTextureBuffer, const Texture2D* reportTex)
{
	bool hasAlpha = HasAlphaTextureFormat(textureFormat);

	// Create 24-bit image (RGB for endianess conversion) scale
	// minimum size for an ETC texture is 4*4
	Image tmpImage (src.GetWidth(), src.GetHeight(), hasAlpha ? kTexFormatRGBA32 : kTexFormatRGB24 );
	tmpImage.BlitImage (src);

	int minSize = GetMinimumTextureMipSizeForFormat(textureFormat);
	tmpImage.ReformatImage (std::max (minSize, src.GetWidth()), std::max (minSize, src.GetHeight()), tmpImage.GetFormat (), ImageReference::BLIT_BILINEAR_SCALE);

#if PRINT_TEXTURE_COMPRESSION_TIMES
	double t1 = GetTimeSinceStartup();
#endif

	tmpImage.FlipImageY();
	DeleteFile("Temp/ETC-Feed.tga");
	if (!SaveImageToFile(tmpImage.GetImageData(), tmpImage.GetWidth(), tmpImage.GetHeight(), tmpImage.GetFormat (), "Temp/ETC-Feed.tga", 'TGAf'))
	{
		ErrorStringObject("Failed writing conversion file during compression", reportTex);
	}

	std::vector<std::string>	args;

	DeleteFile("Temp/Compressed/ETC-Feed.pkm");
	std::string output;


	char cwd[256] = "";

#if UNITY_WIN
	if(_getcwd(cwd, sizeof(cwd)) == NULL)
	{
		ErrorString("Failed to get current working directory?");
	}

	// Copy convert.exe to temp directory, otherwise we'll get trouble when installed in Program Files
	if(!IsFileCreated(AppendPathName(cwd, "Temp\\etcpack2.exe")))
	{
		CopyFileOrDirectory(AppendPathName(GetApplicationContentsPath(), "Tools\\etcpack2.exe"), AppendPathName(cwd, "Temp\\etcpack2.exe"));
	}
	if(!IsFileCreated(AppendPathName(cwd, "Temp\\convert.exe")))
	{
		CopyFileOrDirectory(AppendPathName(GetApplicationContentsPath(), "Tools\\convert.exe"), AppendPathName(cwd, "Temp\\convert.exe"));
	}
	 
	args.push_back(AppendPathName(cwd, "Temp\\ETC-Feed.tga"));
	args.push_back(AppendPathName(cwd, "Temp\\Compressed"));
	std::string appPath = AppendPathName(cwd, "Temp\\etcpack2.exe");
#elif UNITY_OSX || UNITY_LINUX
	getcwd(cwd, sizeof(cwd));

	// Copy convert.exe to temp directory, otherwise we'll get trouble when installed in Program Files
	if(!IsFileCreated("Temp/etcpack2"))
	{
#if UNITY_OSX
		CopyFileOrDirectory(AppendPathName(GetApplicationContentsPath(), "Tools/etcpack2"), "Temp/etcpack2");
#else
		CopyFileOrDirectory(AppendPathName(GetApplicationContentsPath(), "Tools/etcpack2_linux"), "Temp/etcpack2");
#endif
	}
	if(!IsFileCreated("Temp/convert"))
	{
#if UNITY_OSX
		CopyFileOrDirectory(AppendPathName(GetApplicationContentsPath(), "Tools/convert"), "Temp/convert");
#else
		CopyFileOrDirectory(AppendPathName(GetApplicationContentsPath(), "Tools/convert_linux"), "Temp/convert");
#endif
	}


	args.push_back("ETC-Feed.tga");
	args.push_back("Compressed");
	std::string appPath = AppendPathName(cwd, "Temp/etcpack2");
#else
#error Unknown Platform
#endif

	args.push_back("-s");
	args.push_back("fast");
	args.push_back("-e");
	args.push_back("perceptual");
	args.push_back("-c");
	args.push_back("etc2");
	args.push_back("-f");
	switch(textureFormat)
	{
		case kTexFormatETC2_RGB: args.push_back("RGB"); break;
		case kTexFormatETC2_RGBA1: args.push_back("RGBA1"); break;
		case kTexFormatETC2_RGBA8: args.push_back("RGBA8"); break;
	}


	if (LaunchTaskArray (appPath, &output, args, true, AppendPathName(cwd, "Temp")) == false)
	{ 
		ErrorStringMsg("Failed to execute %s", appPath.c_str());
	}

	if (IsFileCreated("Temp/Compressed/ETC-Feed.pkm"))
	{
		File file;
		int length = GetFileLength("Temp/Compressed/ETC-Feed.pkm");
		int expectedImageSize = CalculateImageSize (src.GetWidth(), src.GetHeight(), textureFormat);
		AssertIf(expectedImageSize + 16 != length);
		file.Open("Temp/Compressed/ETC-Feed.pkm", File::kReadPermission);/// error handling
		file.Read(16, compressedTextureBuffer, expectedImageSize); // 16 bytes == header which we skip!
		file.Close();
	}
	else
	{
		ErrorStringObject("Failed to generate ETC texture", reportTex);
	}

	DeleteFile("Temp/ETC-Feed.tga");
	DeleteFile("Temp/Compressed/ETC-Feed.pkm");

#if PRINT_TEXTURE_COMPRESSION_TIMES
	if( src.GetWidth() >= 256 && src.GetHeight() >= 256 )
	{
		double t2 = GetTimeSinceStartup();
		printf_console( "  ETC2 compression: %i x %i took %.2fs\n", src.GetWidth(), src.GetHeight(), t2-t1 );
	}
#endif
}

static void HandleASTCTextureCompression(ImageReference &src, TextureFormat textureFormat, int compressionQuality, UInt8* compressedTextureBuffer, const Texture2D* reportTex)
{
	bool hasAlpha = HasAlphaTextureFormat(textureFormat);

	// Create 24-bit image (RGB for endianess conversion) scale
	// minimum size for an ETC texture is 4*4
	Image tmpImage (src.GetWidth(), src.GetHeight(), hasAlpha ? kTexFormatRGBA32 : kTexFormatRGB24 );
	tmpImage.BlitImage (src);

	int minSize = GetMinimumTextureMipSizeForFormat(textureFormat);
	tmpImage.ReformatImage (std::max (minSize, src.GetWidth()), std::max (minSize, src.GetHeight()), tmpImage.GetFormat (), ImageReference::BLIT_BILINEAR_SCALE);

#if PRINT_TEXTURE_COMPRESSION_TIMES
	double t1 = GetTimeSinceStartup();
#endif

	DeleteFile("Temp/ASTC-Feed.tga");
	if (!SaveImageToFile(tmpImage.GetImageData(), tmpImage.GetWidth(), tmpImage.GetHeight(), tmpImage.GetFormat (), "Temp/ASTC-Feed.tga", 'TGAf'))
	{
		ErrorStringObject("Failed writing conversion file during compression", reportTex);
	}

	std::vector<std::string>	args;

	DeleteFile("Temp/ASTC-Res.pkm");
	std::string output;
	args.push_back("-cl");

#if UNITY_WIN
	args.push_back("Temp\\ASTC-Feed.tga");
	args.push_back("Temp\\ASTC-Res.pkm");
	std::string appPath = AppendPathName(GetApplicationContentsPath(), "Tools\\astcenc.exe");
#elif UNITY_OSX || UNITY_LINUX
	args.push_back("Temp/ASTC-Feed.tga");
	args.push_back("Temp/ASTC-Res.pkm");

	std::string appPath = AppendPathName(GetApplicationContentsPath(), "Tools/astcenc");
#else
#error Unknown Platform
#endif

	switch(textureFormat)
	{
#define STR_(x) #x
#define STR(x) STR_(x)
#define DO_ASTC(bx,by) case kTexFormatASTC_RGB_##bx##x##by :  case kTexFormatASTC_RGBA_##bx##x##by : args.push_back(STR(bx) "x" STR(by)); break

		DO_ASTC(4, 4);
		DO_ASTC(5, 5);
		DO_ASTC(6, 6);
		DO_ASTC(8, 8);
		DO_ASTC(10, 10);
		DO_ASTC(12, 12);

#undef DO_ASTC
#undef STR
#undef STR_
	}

	args.push_back("-medium");

	if (LaunchTaskArray (appPath, &output, args, true) == false)
	{ 
		ErrorStringMsg("Failed to execute %s", appPath.c_str());
	}

	if (IsFileCreated("Temp/ASTC-Res.pkm"))
	{
		File file;
		int length = GetFileLength("Temp/ASTC-Res.pkm");
		int expectedImageSize = CalculateImageSize (src.GetWidth(), src.GetHeight(), textureFormat);
		AssertIf(expectedImageSize + 16 != length);
		file.Open("Temp/ASTC-Res.pkm", File::kReadPermission);/// error handling
		file.Read(16, compressedTextureBuffer, expectedImageSize); // 16 bytes == header which we skip!
		file.Close();
	}
	else
	{
		ErrorStringObject("Failed to generate ASTC texture", reportTex);
	}

	DeleteFile("Temp/ASTC-Feed.tga");
	DeleteFile("Temp/ASTC-Res.pkm");

#if PRINT_TEXTURE_COMPRESSION_TIMES
	if( src.GetWidth() >= 256 && src.GetHeight() >= 256 )
	{
		double t2 = GetTimeSinceStartup();
		printf_console( "  ASTC compression: %i x %i took %.2fs\n", src.GetWidth(), src.GetHeight(), t2-t1 );
	}
#endif
}



static void CompressSingleImageTexture (ImageReference& src, TextureFormat textureFormat, int compressionQuality, UInt8* compressedTextureBuffer, const Texture2D* reportTex)
{
	if (IsCompressedDXTTextureFormat(textureFormat))
	{
		// Create 32 bit image (BGRA for endianess conversion) scale
		// minimum size for a DXTn texture is 4*4
		Image temporary32BitImage;
		ConvertImageToARGB32 (src, temporary32BitImage, textureFormat);

		// Clear the alpha for dx1 (The compressor generates artifacts otherwise and we dont need the alpha in there anyway)
		if (textureFormat == kTexFormatDXT1)
			temporary32BitImage.ClearImage (ColorRGBA32 (255, 255, 255, 255), ImageReference::CLEAR_ALPHA);

		PerformTextureCompression (temporary32BitImage, textureFormat, (UInt32*)compressedTextureBuffer);
	}
	else if (IsCompressedPVRTCTextureFormat(textureFormat))
	{
		Assert(IsPowerOfTwo(src.GetWidth()) && IsPowerOfTwo(src.GetHeight()));

		Image temporary32BitImage (src.GetWidth(), src.GetHeight(), kTexFormatRGBA32);
		temporary32BitImage.BlitImage (src);

		int minSize = GetMinimumTextureMipSizeForFormat(textureFormat);
		temporary32BitImage.ReformatImage (std::max (minSize, src.GetWidth()), std::max (minSize, src.GetHeight()), temporary32BitImage.GetFormat (), ImageReference::BLIT_BILINEAR_SCALE);

		if (textureFormat == kTexFormatPVRTC_RGB2 || textureFormat == kTexFormatPVRTC_RGB4)
			temporary32BitImage.ClearImage (ColorRGBA32 (255, 255, 255, 255), ImageReference::CLEAR_ALPHA);

		#if PRINT_TEXTURE_COMPRESSION_TIMES
		double t1 = GetTimeSinceStartup();
		#endif

		if( !CompressToPVR(src, temporary32BitImage, textureFormat, compressionQuality, compressedTextureBuffer) )
			ErrorStringObject("Failed PVR compression", reportTex);

	#if PRINT_TEXTURE_COMPRESSION_TIMES
		if( src.GetWidth() >= 256 && src.GetHeight() >= 256 )
		{
			double t2 = GetTimeSinceStartup();
			printf_console( "  PVRT compression: %i x %i took %.2fs\n", src.GetWidth(), src.GetHeight(), t2-t1 );
		}
	#endif
	}
	else if (IsCompressedETCTextureFormat(textureFormat))
	{
		HandleETC1TextureCompression(src, textureFormat, compressionQuality, compressedTextureBuffer, reportTex);
	}
	else if (IsCompressedETC2TextureFormat(textureFormat))
	{
		HandleETC2TextureCompression(src, textureFormat, compressionQuality, compressedTextureBuffer, reportTex);
	}
	else if (IsCompressedEACTextureFormat(textureFormat))
	{
		// TODO
		ErrorString("EAC texture formats not yet supported");
	}
	else if(IsCompressedASTCTextureFormat(textureFormat))
	{
		HandleASTCTextureCompression(src, textureFormat, compressionQuality, compressedTextureBuffer, reportTex);
	}
	else if (IsCompressedATCTextureFormat(textureFormat))
	{
		Image temporary32BitImage (src.GetWidth(), src.GetHeight(), kTexFormatRGBA32);
		temporary32BitImage.BlitImage (src);

		int minSize = GetMinimumTextureMipSizeForFormat(textureFormat);
		temporary32BitImage.ReformatImage (std::max (minSize, src.GetWidth()), std::max (minSize, src.GetHeight()), temporary32BitImage.GetFormat (), ImageReference::BLIT_BILINEAR_SCALE);

		bool doAlpha = true;
		// Clear the alpha (The compressor generates artifacts otherwise and we dont need the alpha in there anyway)
		if (textureFormat == kTexFormatATC_RGB4)
		{
			temporary32BitImage.ClearImage (ColorRGBA32 (255, 255, 255, 255), ImageReference::CLEAR_ALPHA);
			doAlpha = false;
		}

	#if PRINT_TEXTURE_COMPRESSION_TIMES
		double t1 = GetTimeSinceStartup();
	#endif

		TQonvertImage src, dst;
		src.nWidth = temporary32BitImage.GetWidth();
		src.nHeight = temporary32BitImage.GetHeight();
		src.nFormat = Q_FORMAT_RGBA_8888;
		src.pFormatFlags = NULL;
		src.nDataSize = CalculateImageSize (src.nWidth, src.nHeight, kTexFormatRGBA32);
		src.pData = temporary32BitImage.GetImageData();

		dst.nWidth = src.nWidth;
		dst.nHeight = src.nHeight;
		dst.nFormat = doAlpha ? Q_FORMAT_ATC_RGBA_INTERPOLATED_ALPHA : Q_FORMAT_ATC_RGB;
		dst.pFormatFlags = NULL;
		dst.nDataSize = CalculateImageSize (dst.nWidth, dst.nHeight, textureFormat);
		dst.pData = compressedTextureBuffer;

		int nRet = Qonvert(&src, &dst);
		if (nRet != Q_SUCCESS)
		{
			ErrorStringMsg("CompressATC failed with nRet = %i", nRet);
			ErrorStringObject("Failed to generate ATC texture", reportTex);
		}

	#if PRINT_TEXTURE_COMPRESSION_TIMES
		if( src.GetWidth() >= 256 && src.GetHeight() >= 256 )
		{
			double t2 = GetTimeSinceStartup();
			printf_console( "  ATC compression: %i x %i took %.2fs\n", src.GetWidth(), src.GetHeight(), t2-t1 );
		}
	#endif
	}
	else if (IsCompressedFlashATFTextureFormat(textureFormat) )
	{
		// Blit image to texture, the texture format is automatically converted on blitting
		ImageReference dstImage (src.GetWidth(), src.GetHeight(), GetRowBytesFromWidthAndFormat (src.GetWidth(), textureFormat), textureFormat, compressedTextureBuffer);
		dstImage.BlitImage (src);
	}
	else
	{
		// Blit image to texture, the texture format is automatically converted on blitting
		ImageReference dstImage (src.GetWidth(), src.GetHeight(), GetRowBytesFromWidthAndFormat (src.GetWidth(), textureFormat), textureFormat, compressedTextureBuffer);
		dstImage.BlitImage (src);
	}
}

static bool CompressTextureWithMultipleImages (Texture2D& tex, TextureFormat compressedFormat, int compressionQuality)
{
	////@TODO: Move this to a better place
	int imageSize;
	if( tex.HasMipMap() )
		imageSize = CalculateImageMipMapSize( tex.GetDataWidth (), tex.GetDataHeight (), compressedFormat );
	else
		imageSize = CalculateImageSize( tex.GetDataWidth (), tex.GetDataHeight (), compressedFormat );

	UInt8* compressedImageBase = tex.AllocateTextureData (imageSize * tex.GetImageCount(), compressedFormat);
	UInt8* compressedImageData = compressedImageBase;
	for (int f=0;f<tex.GetImageCount();f++)
	{
		for (int m=0;m<tex.CountDataMipmaps();m++)
		{
			ImageReference srcImage;

			if (!tex.GetWriteImageReference(&srcImage, f, m))
			{
				tex.DeallocateTextureData(compressedImageData);
				return false;
			}

			CompressSingleImageTexture (srcImage, compressedFormat, compressionQuality, compressedImageData, &tex);
			compressedImageData += CalculateImageSize (srcImage.GetWidth(), srcImage.GetHeight(), compressedFormat);
		}
	}

	tex.InitTextureInternal (tex.GetDataWidth(), tex.GetDataHeight(), compressedFormat, imageSize, compressedImageBase, tex.HasMipMap() ? Texture2D::kMipmapMask : Texture2D::kNoMipmap, tex.GetImageCount());

	//////@TODO: Flash cubemaps interpretation of imageSize should probably be very different.
	///////////  We should change imageSize to be the total image size????

	return true;
}


static bool ConvertCompressedToJPGForFlash (Texture2D& texture, TextureFormat compressedFormat, int compressionQuality)
{
	 return false;
}

bool CompressTexture (Texture2D& tex, TextureFormat compressedFormat, int compressionQuality)
{
	compressionQuality = clamp(compressionQuality, 0, 100);

	if (IsCompressedFlashATFTextureFormat(compressedFormat))
		return ConvertCompressedToJPGForFlash (tex, compressedFormat, compressionQuality);
	else
		return CompressTextureWithMultipleImages (tex, compressedFormat, compressionQuality);
}


#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (ImageOperationTests)
{
#if UNITY_LITTLE_ENDIAN
	TEST (KaiserMipmapOneColor)
	{
		float src[16*4];
		for(int i = 0; i < 16; ++i)
		{
			src[i*4+0] = 0.3f;
			src[i*4+1] = 0.7f;
			src[i*4+2] = 0.6f;
			src[i*4+3] = 0.8f;
		}

		Image tmpImage (4, 4, 64, kTexFormatARGBFloat, src);
		Image dstImage( 2, 2, kTexFormatARGBFloat );
		HalveImageFloatKaiser<float, false>(tmpImage, dstImage);

		float* dst = reinterpret_cast<float*> (dstImage.GetImageData ());
		for (int i = 0; i < 4; ++i) {
			CHECK_CLOSE (dst[i*4+0], 0.3f, 1e-5f);
			CHECK_CLOSE (dst[i*4+1], 0.7f, 1e-5f);
			CHECK_CLOSE (dst[i*4+2], 0.6f, 1e-5f);
			CHECK_CLOSE (dst[i*4+3], 0.8f, 1e-5f);
		}
	}

	TEST (KaiserMipmap)
	{
		float src[16*4];
		float average[4] = {0.f,0.f,0.f,0.f};
		for(int i = 0; i < 16; ++i)
		{
			src[i*4+0] = fmod(0.3f * i, 1.0f);
			src[i*4+1] = fmod(0.7f * i, 1.0f);
			src[i*4+2] = fmod(0.6f * i, 1.0f);
			src[i*4+3] = fmod(0.8f * i, 1.0f);
			for(int j = 0; j < 4; j++)
				average[j] += src[i*4+j];
		}
		for(int j = 0; j < 4; j++)
			average[j] /= 16.f;

		Image tmpImage (4, 4, 64, kTexFormatARGBFloat, src);
		Image dstImage( 2, 2, kTexFormatARGBFloat );
		HalveImageFloatKaiser<float, false>(tmpImage, dstImage);

		float* dst = reinterpret_cast<float*> (dstImage.GetImageData ());
		float expected[16] = {
			0.17287397f, 0.58993816f, 0.13251127f, 0.13251123f,
			0.70892626f, 0.40856266f, 0.50078815f, 0.72058970f,
			0.82562715f, 0.29186144f, 0.73418993f, 0.51438844f,
			0.042573184f, 0.45963746f, 0.13251148f, 0.13251117f };
		float expectedAverage[4] = {0.f,0.f,0.f,0.f};
		for (int i = 0; i < 4; ++i) {
			CHECK_CLOSE (dst[i*4+0], expected[i*4+0], 1e-5f);
			CHECK_CLOSE (dst[i*4+1], expected[i*4+1], 1e-5f);
			CHECK_CLOSE (dst[i*4+2], expected[i*4+2], 1e-5f);
			CHECK_CLOSE (dst[i*4+3], expected[i*4+3], 1e-5f);
			for(int j = 0; j < 4; j++)
				expectedAverage[j] += dst[i*4+j];
		}

		for(int j = 0; j < 4; j++)
			expectedAverage[j] /= 4.f;
		for(int j = 0; j < 4; j++)
			CHECK_CLOSE (average[j], expectedAverage[j], 1e-5f);
	}

#endif
} // SUITE

#endif // ENABLE_UNIT_TESTS
