#include "UnityPrefix.h"

#if ENABLE_LIGHTMAPPER

#include "LightmapperBeast.h"

#include "Runtime/Math/Random/Random.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include "Runtime/Input/TimeManager.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/Utility/Analytics.h"
#include "Editor/Src/AssetPipeline/ImageConverter.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "AssetPipeline/AssetPathUtilities.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Lightmapping.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"
#include "Editor/Src/VersionControl/VCProvider.h"
#include "AssetPipeline/AssetPathUtilities.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Camera/LightManager.h"
#include "Runtime/Core/Callbacks/PlayerLoopCallbacks.h"
#include <FreeImage.h>
#include <time.h>

#if UNITY_WIN
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#endif

#include "External/Beast/builds/include/beastapi/beastframebuffer.h"
#include "External/Beast/builds/include/beastapi/beasttarget.h"
#include "External/Beast/builds/include/beastapi/beasttargetentity.h"

using namespace std;
using namespace BeastUtils;

const char* const LightmapperBeastResults::kLightmapPaths[] = {
	"%s/LightmapFar-%i.exr",		"",							"", // single
	"%s/LightmapFar-%i.exr",		"%s/LightmapNear-%i.exr",	"", // dual
	"%s/LightmapColor-%i.exr",		"%s/LightmapScale-%i.exr",	"", // directional
	"%s/LightmapRNM0-%i.exr",		"%s/LightmapRNM1-%i.exr",	"%s/LightmapRNM2-%i.exr", // rnm
};

inline int HDRToLDR(float hdr)
{
	// Convert from float HDR format to standard 8bit LDR format
	return std::min (std::max ((int)(255.0f * hdr), 0), 255);
}

inline int HDRToDoubleLDR(float hdr)
{
	// Convert from float HDR format to an (extended) 8bit LDR format
	// just multiply here by 0.5, clamp to white and make DOUBLE in the shader.
	return std::min ( (int)(0.5f * 255.0f * hdr), 255 );
}

static bool SaveLightmapToFile (float* data, int width, int height, const string& path, TextureImporter::TextureType textureType)
{
	bool saveAsHDR = (textureType == TextureImporter::kLightmap);

	// Save the lightmap image to disk
	if (saveAsHDR)
	{
		return SaveHDRImageToFile (data, width, height, path);
	}
	else
	{
		// convert to LDR
		dynamic_array<UInt8> dataLDR;
		dataLDR.resize_uninitialized(width * height * 3);

		for (size_t q = 0; q < dataLDR.size (); ++q)
			dataLDR[q] = HDRToLDR (data[q]);

		return SaveImageToFile((UInt8*)(dataLDR.begin()), width, height, kTexFormatRGB24, path, 'PNGf');
	}
}

static void SetTextureImporterSettings (const string& path, TextureImporter::TextureType textureType)
{
	TextureImporter* ti = NULL;

	bool fileExists = IsFileCreated(path);
	if (!fileExists)
	{
		// If there wasn't a texture here before, set the default lightmap import settings:
		// Save a dummy 1x1px texture to be able to get the TextureImporter
		float onepixel[] = { 1.0f, 1.0f, 1.0f };
		SaveHDRImageToFile (onepixel, 1, 1, path);

		AssetInterface::Get().ImportAtPathImmediate(path);

		ti = dynamic_pptr_cast<TextureImporter*>(FindAssetImporterAtAssetPath(path));
		if (ti)
		{
			// Set default texture settings
			TextureSettings& ts = ti->GetTextureSettings();
			ts.m_WrapMode = kTexWrapClamp;
			ts.m_Aniso = 3;
			ts.m_FilterMode = kTexFilterBilinear;

			// Set compression
			ti->SetTextureFormat(kCompressed);
			ti->SetCompressionQuality(kTexCompressionBest);
		}
	}
	else
	{
		// If there was a texture here before, respect user's import settings
		ti = dynamic_pptr_cast<TextureImporter*>(FindAssetImporterAtAssetPath(path));
	}

	if (ti)
	{
		// Always mark it as a lightmap
		ti->SetTextureType(textureType, true);
	}
	else
	{
		ErrorString (Format("Could not access TextureImporter at path %s in the Assets folder.", path.c_str()));
	}
}

template <class T>
bool SaveLightmap(dynamic_array<T>& data, int width, int height, const string& path, bool temp, TextureImporter::TextureType textureType = TextureImporter::kLightmap)
{
	const int channels = 3;
	if (data.size () != width * height * channels)
		return false;

	bool fileExists = IsFileCreated(path);
	if (!fileExists)
	{
		std::string parentFolder = DeleteLastPathNameComponent (path);
		CreateDirectoryRecursive (parentFolder);
	}

	if (!temp)
	{
		// set texture importer settings when saving in the Assets folder
		SetTextureImporterSettings (path, textureType);
	}

	// Save the lightmap image to disk
	bool saved = SaveHDRImageToFile((float*)(data.begin()), width, height, path);

	if (!saved)
	{
		if (fileExists)
		{
			WarningString(Format("Failed to save lightmap at path: %s. Make sure the file is not read-only.", path.c_str()));
		}
		else
		{
			WarningString(Format("Failed to save lightmap at path: %s. Make sure you have permissions to write in that location.", path.c_str()));
		}
	}
	return saved;
}

inline float DesaturateLight(Vector3f light)
{
	//Vector3f desaturator(0.30f, 0.59f, 0.11f); // Grayscale
	Vector3f desaturator(0.22f, 0.707f, 0.071f); // Luminance

	return Dot(light, desaturator);
}

static inline void CopyTextureData(const dynamic_array<float>& texDataSrc, int srcIndex, dynamic_array<UInt8>& texDataDst, int dstIndex)
{
	texDataDst[dstIndex + 0] = HDRToLDR(texDataSrc[srcIndex + 0]);
	texDataDst[dstIndex + 1] = HDRToLDR(texDataSrc[srcIndex + 1]);
	texDataDst[dstIndex + 2] = HDRToLDR(texDataSrc[srcIndex + 2]);
}

static inline void CopyTextureData(const dynamic_array<float>& texDataSrc, int srcIndex, int offset, const ColorRGBAf& ambientLight, dynamic_array<float>& texDataDst, int dstIndex)
{
	srcIndex += offset;
	texDataDst[dstIndex + 0] = texDataSrc[srcIndex + 0] + ambientLight.r;
	texDataDst[dstIndex + 1] = texDataSrc[srcIndex + 1] + ambientLight.g;
	texDataDst[dstIndex + 2] = texDataSrc[srcIndex + 2] + ambientLight.b;
}

static inline void AccumulateIntoNextRNM(dynamic_array<float>& rnmCurrent, dynamic_array<float>& rnmNext, int i)
{
	Vector3f light(rnmCurrent[i], rnmCurrent[i + 1], rnmCurrent[i + 2]);
	float desatLight = DesaturateLight(light);

	if (desatLight < 0.0f)
	{
		rnmNext[i + 0] += rnmCurrent[i + 0];
		rnmNext[i + 1] += rnmCurrent[i + 1];
		rnmNext[i + 2] += rnmCurrent[i + 2];

		rnmCurrent[i + 0] = 0.0f;
		rnmCurrent[i + 1] = 0.0f;
		rnmCurrent[i + 2] = 0.0f;
	}
}

static inline void MakeRNMNonNegative(dynamic_array<float>& rnm0, dynamic_array<float>& rnm1, dynamic_array<float>& rnm2, int i)
{
	AccumulateIntoNextRNM(rnm0, rnm1, i);
	AccumulateIntoNextRNM(rnm1, rnm2, i);
	AccumulateIntoNextRNM(rnm2, rnm0, i);
}

static inline void OverrideBackgroundColor(const dynamic_array<float>& texDataSrc, int srcIndex, int offset, dynamic_array<float>& texDataDst, int dstIndex)
{
	srcIndex += offset;
	float zChannel = texDataSrc[srcIndex];

	if (zChannel < 0.0f)
	{
		texDataDst[dstIndex + 0] = zChannel;
		texDataDst[dstIndex + 1] = zChannel;
		texDataDst[dstIndex + 2] = zChannel;
	}
}

// Takes texData0, taxData1 and texData2 as input and outputs directional lightmaps
// in texData0 (color) and texData1 (scale).
void ConvertRNMIntoDirectional(dynamic_array<float>& texData0, dynamic_array<float>& texData1, dynamic_array<float>& texData2, int destLightmapSize, int destStride)
{
	// combine the three lightmaps (incoming light color per basis vector) into
	// two (average color in one and intensity per basis vector in the other)
	const float dotRNMBasisNormalStraightUp = 0.5773503f;
	const float oneOverThreeTimesDotRNMBasisNormalStraightUp = 1.0f / (3.0f * dotRNMBasisNormalStraightUp);

	const float maxRGBMPrecisionOverTwo = 1.0f / (256.0f * (float)kRGBMMaxRange * 2.0f);

	for (int i = 0; i < destLightmapSize; i += destStride)
	{
		Vector3f light0(texData0[i], texData0[i + 1], texData0[i + 2]);
		Vector3f light1(texData1[i], texData1[i + 1], texData1[i + 2]);
		Vector3f light2(texData2[i], texData2[i + 1], texData2[i + 2]);

		Vector3f averageLight = dotRNMBasisNormalStraightUp * light0 + dotRNMBasisNormalStraightUp * light1 + dotRNMBasisNormalStraightUp * light2;

		texData0[i] = averageLight.x;
		texData0[i + 1]= averageLight.y;
		texData0[i + 2]= averageLight.z;

		float desaturatedAverageLight = DesaturateLight(averageLight);

		float scale0, scale1, scale2;

		if (averageLight.x < 0.0f || averageLight.y < 0.0f || averageLight.z < 0.0f)
		{
			// Empty space, let the texture importer fill it with background color.
			scale0 = scale1 = scale2 = -1.0f;
		}
		else if (averageLight.x < maxRGBMPrecisionOverTwo && averageLight.y < maxRGBMPrecisionOverTwo && averageLight.z < maxRGBMPrecisionOverTwo)
		{
			// A very small desaturatedAverageLight breaks numerical stability.
			// For averageLight values that will be clamped to zero in the RGBM encoded lightmap
			// just set the scales to a decent default - values that decoded with the straight up normal will give 1.
			scale0 = scale1 = scale2 = oneOverThreeTimesDotRNMBasisNormalStraightUp;
		}
		else
		{
			float oneOverScaleAverage = 1.0f / desaturatedAverageLight;

			scale0 = DesaturateLight(light0) * oneOverScaleAverage;
			scale1 = DesaturateLight(light1) * oneOverScaleAverage;
			scale2 = DesaturateLight(light2) * oneOverScaleAverage;

			// We can't encode negative values at this point, but we still sometimes get them at grazing angles.
			// Clamp for now. When we start encoding the direction separately, negative values won't be an issue.
			scale0 = std::max (0.0f, scale0);
			scale1 = std::max (0.0f, scale1);
			scale2 = std::max (0.0f, scale2);
		}

		texData1[i] = scale0;
		texData1[i + 1]= scale1;
		texData1[i + 2]= scale2;
	}
}

// Dual: srcIndex = 3 for far, 0 for near
// Single: srcIndex = 0 for far
static void ReadLightmapSingleOrDual (ILBFramebufferHandle frameBuffer, int width, int height, int sourceStride, int srcOffset, const ColorRGBAf& ambientLight, dynamic_array<float>& texData0, int destStride)
{
	const int zChannelOffset = sourceStride - 1;
	const int sourceBufferWidth = width * sourceStride;
	dynamic_array<float> texDataHDR (sourceBufferWidth, kMemTempAlloc);

	for (int row = 0, j = 0; row < height; row++)
	{
		// copy from the last row to the first one
		BeastCall (ILBReadRegionHDR (frameBuffer, 0, height-row-1, width, height-row, ILB_CS_ALL, texDataHDR.data()));

		for (int i = 0; i < sourceBufferWidth; i += sourceStride, j += destStride)
		{
			CopyTextureData (texDataHDR, i, srcOffset, ambientLight, texData0, j);
			OverrideBackgroundColor (texDataHDR, i, zChannelOffset, texData0, j);
		}
	}
}

static void ReadLightmapDirectional (ILBFramebufferHandle frameBuffer, int width, int height, int sourceStride, const ColorRGBAf& ambientLight, dynamic_array<float>& texData, int destStride, int lightmapOffset)
{
	const int zChannelOffset = sourceStride - 1;
	const int sourceBufferWidth = width * sourceStride;
	dynamic_array<float> texDataHDR (sourceBufferWidth, kMemTempAlloc);

	const float dotRNMBasisNormalStraightUp = 0.5773503f;
	const ColorRGBAf ambientLightForRNM = ambientLight * (1 / (3.0f * dotRNMBasisNormalStraightUp));

	int destLightmapSize = width * height * destStride;
	dynamic_array<float> texDataTemp0 (destLightmapSize, kMemTempAlloc), texDataTemp1 (destLightmapSize, kMemTempAlloc);

	dynamic_array<float>& texData0 = lightmapOffset == 0 ? texData : texDataTemp0;
	dynamic_array<float>& texData1 = lightmapOffset == 0 ? texDataTemp0 : texData;
	dynamic_array<float>& texData2 = texDataTemp1;


	for (int row = 0, j = 0; row < height; row++)
	{
		// copy from the last row to the first one
		BeastCall (ILBReadRegionHDR (frameBuffer, 0, height-row-1, width, height-row, ILB_CS_ALL, texDataHDR.data()));

		for (int i = 0; i < sourceBufferWidth; i+=sourceStride, j+=destStride)
		{
			CopyTextureData (texDataHDR, i, 0, ambientLightForRNM, texData0, j);
			CopyTextureData (texDataHDR, i, 4, ambientLightForRNM, texData1, j);
			CopyTextureData (texDataHDR, i, 8, ambientLightForRNM, texData2, j);

			MakeRNMNonNegative (texData0, texData1, texData2, j);

			OverrideBackgroundColor (texDataHDR, i, zChannelOffset, texData0, j);
			OverrideBackgroundColor (texDataHDR, i, zChannelOffset, texData1, j);
			OverrideBackgroundColor (texDataHDR, i, zChannelOffset, texData2, j);
		}
	}

	ConvertRNMIntoDirectional (texData0, texData1, texData2, destLightmapSize, destStride);
}

const char* kBakeSelectedTempPath = "Temp/BakeSelected";

static std::string GetLightmapTempPath (std::string& lightmapPath)
{
	return AppendPathName (kBakeSelectedTempPath, lightmapPath);
}

static string GetLightmapPath (int lightmapsMode, int lightmapOffset, int lightmapIndex)
{
	const char* format = LightmapperBeastResults::kLightmapPaths[lightmapsMode * LightmapperBeastResults::kLightmapPathsStride + lightmapOffset];
	string path = Format (format, GetSceneBakedAssetsPath ().c_str (), lightmapIndex);
	return path;
}

static void ApplyAO (
	const int lightmapsMode, const int lightmapOffset, ILBTargetHandle target, int width, int height, dynamic_array<float>& texData,
	int destStride, ILBRenderPassHandle renderPassAO, float aoPassAmount)
{
	DebugAssert (destStride == 3);

	// in directional lightmaps mode AO should only be applied to the color lightmap (offset 0)
	if (lightmapsMode == LightmapSettings::kDirectionalLightmapsMode && lightmapOffset > 0)
		return;

	// Get AO data if the AO pass was run
	if (aoPassAmount > 0)
	{
		ILBFramebufferHandle frameBufferAO;
		BeastCall (ILBGetFramebuffer (target, renderPassAO, 0, &frameBufferAO));
		dynamic_array<float> texDataAO (width, kMemTempAlloc);

		for (int row = 0; row < height; row++)
		{
			// copy from the last row to the first one
			BeastCall (ILBReadRegionHDR (frameBufferAO, 0, height-row-1, width, height-row, ILB_CS_R, texDataAO.data ()));

			int rowDataOffset = row * width * destStride;

			for (int i = 0, j = rowDataOffset; i < width; i++, j += destStride)
			{
				float ao = (1.0f - aoPassAmount) + aoPassAmount * texDataAO[i];

				texData[j+0] *= ao;
				texData[j+1] *= ao;
				texData[j+2] *= ao;
			}
		}
		BeastCall (ILBDestroyFramebuffer (frameBufferAO));
	}
}


void ReadAllModes (
				   int lightmapsMode, ILBFramebufferHandle frameBuffer, int width, int height,
				   dynamic_array<float>& texData,
				   int destStride, const ColorRGBAf& ambientLight, int lightmapOffset)
{
	int srcStride, srcOffset;
	switch (lightmapsMode)
	{
	case LightmapSettings::kSingleLightmapsMode:
		srcStride = 5;
		ReadLightmapSingleOrDual (frameBuffer, width, height, srcStride, 0, ambientLight, texData, destStride);
		break;
	case LightmapSettings::kDualLightmapsMode:
		srcStride = 7;
		srcOffset = (lightmapOffset == 0) ? 3 : 0; // far lightmap starts at the channel with index 3, near at index 0
		ReadLightmapSingleOrDual (frameBuffer, width, height, srcStride, srcOffset, ambientLight, texData, destStride);
		break;
	case LightmapSettings::kDirectionalLightmapsMode:
		srcStride = 13;
		ReadLightmapDirectional (frameBuffer, width, height, srcStride, ambientLight, texData, destStride, lightmapOffset);
		break;
	}
}

template<class T>
string SaveAllModes (
	int lightmapsMode, int width, int height,
	dynamic_array<T>& texData,
	int lightmapIndex, int lightmapOffset, bool temp)
{
	string pathInAssetsFolder = GetLightmapPath (lightmapsMode, lightmapOffset, lightmapIndex);

	string destinationPath = pathInAssetsFolder;
	if (temp)
		destinationPath = GetLightmapTempPath (destinationPath);

	if (SaveLightmap (texData, width, height, destinationPath, temp))
	{
		// even if we saved to the temp folder, return a path starting in the Assets folder
		// as we'll use it as the excluded path when deleting excess lightmaps
		return pathInAssetsFolder;
	}
	else
	{
		return "";
	}
}

// TODO: copied from ImageConverter.cpp, do sth about it
static bool SaveAndUnload(FREE_IMAGE_FORMAT dstFormat, FIBITMAP* dst, const std::string& inPath)
{
	bool wasSaved = false;

#if UNITY_WIN
	std::wstring widePath;
	ConvertUnityPathName( inPath, widePath );
	wasSaved = FreeImage_SaveU( dstFormat, dst, widePath.c_str() );
#else
	wasSaved = FreeImage_Save( dstFormat, dst, inPath.c_str() );
#endif
	FreeImage_Unload( dst );

	return wasSaved;
}

static void SaveAllModes (FIBITMAP* fiBitmap, const string& path)
{
	if (!fiBitmap)
		return;

	FREE_IMAGE_FORMAT dstFormat = FIF_EXR;
	SaveAndUnload (dstFormat, fiBitmap, path);
}

static void GetFramebuffer (const ILBTargetHandle target, const ILBRenderPassHandle pass, const ILBRenderPassHandle passAO, const float aoAmount, const int stride, const ColorRGBAf& ambientLight, const int lightmapsMode, const int lightmapOffset, dynamic_array<float>& texData, int& framebufferWidth, int& framebufferHeight)
{
	int count;
	BeastCall(ILBGetFramebufferCount(target, &count));
	// this is a texture target, not an atlas texture target, so the count should always be 1
	DebugAssert (count == 1);

	ILBFramebufferHandle frameBuffer;
	BeastCall (ILBGetFramebuffer (target, pass, 0, &frameBuffer));

	BeastCall (ILBGetResolution (frameBuffer, &framebufferWidth, &framebufferHeight));

	// prepare destination
	const int newFramebufferSize = framebufferWidth * framebufferHeight * stride;
	texData.resize_uninitialized (newFramebufferSize);

	// read from the framebuffer into destination
	ReadAllModes (lightmapsMode, frameBuffer, framebufferWidth, framebufferHeight, texData, stride, ambientLight, lightmapOffset);

	BeastCall (ILBDestroyFramebuffer (frameBuffer));

	// apply AO
	ApplyAO (lightmapsMode, lightmapOffset, target, framebufferWidth, framebufferHeight, texData, stride, passAO, aoAmount);
}

string LightmapRenderingPasses::GetLightmapDataForTarget (const ILBTargetHandle target, const int lightmapIndex, const int lightmapOffset, const bool temp = false)
{
	const int destStride = 3;
	dynamic_array<float> texData;
	int framebufferWidth, framebufferHeight;

	GetFramebuffer (target, m_RenderPass, m_RenderPassAO, m_AOPassAmount, destStride, m_AmbientLight, m_LightmapsMode, lightmapOffset, texData, framebufferWidth, framebufferHeight);

	// save
	return SaveAllModes (m_LightmapsMode, framebufferWidth, framebufferHeight, texData, lightmapIndex, lightmapOffset, temp);
}

int LightmapCountForLightmapsMode(int lightmapsMode)
{
	switch (lightmapsMode)
	{
		case LightmapSettings::kSingleLightmapsMode:
			return 1;
		case LightmapSettings::kDualLightmapsMode:
			return 2;
		case LightmapSettings::kDirectionalLightmapsMode:
			return 2;
		default:
			return 1;
	}
}

int LightmapperBeastResults::GetTexturesFromTarget (ILBTargetHandle target)
{
	int count;
	BeastCall(ILBGetFramebufferCount(target, &count));
	// this is a texture target, not an atlas texture target, so the count should always be 1
	DebugAssert (count == 1);

	const int lightmapsMode = m_Lbs->Lrp.GetLightmapsMode ();
	const int lightmapCountForLightmapsMode = LightmapCountForLightmapsMode (lightmapsMode);
	const int lightmapIndex = m_LightmapAssetPaths.size () / lightmapCountForLightmapsMode;

	for (int lightmapOffset = 0; lightmapOffset < lightmapCountForLightmapsMode; lightmapOffset++)
	{
		string path = m_Lbs->Lrp.GetLightmapDataForTarget (target, lightmapIndex, lightmapOffset);
		m_LightmapAssetPaths.push_back (path);
	}

	if (lightmapIndex >= LightmapSettings::kMaxLightmaps)
	{
		ErrorString(Format(
			"There have been more than the maximum of %i lightmaps created. "
			"Objects that use lightmaps past that limit won't update their lightmap indices.",
			LightmapSettings::kMaxLightmaps));
	}

	return lightmapIndex;
}

template <class T>
static void Blit (
	T* srcImage, int srcWidth, int srcPatchX, int srcPatchY,
	T* dstImage, int dstWidth, int dstPatchX, int dstPatchY,
	int patchWidth, int patchHeight)
{
	const int channels = 3;

	int src = srcPatchY*srcWidth + srcPatchX;
	int dst = dstPatchY*dstWidth + dstPatchX;

	for (int patchRow = 0; patchRow < patchHeight; patchRow++)
	{
		memcpy (&dstImage[dst * channels], &srcImage[src * channels], patchWidth * channels * sizeof(T));
		src += srcWidth;
		dst += dstWidth;
	}
}

static void FreeImageLoad (const string &path, FIBITMAP** fiBitmap, int *outWidth, int *outHeight)
{
	*fiBitmap = NULL;

#if UNITY_WIN
	std::wstring widePath;
	ConvertUnityPathName( path, widePath );
#endif

#if UNITY_WIN
	FREE_IMAGE_FORMAT srcFormat = FreeImage_GetFileTypeU( widePath.c_str() );
#else
	FREE_IMAGE_FORMAT srcFormat = FreeImage_GetFileType( path.c_str() );
#endif

#if UNITY_WIN
	(*fiBitmap) = FreeImage_LoadU( srcFormat, widePath.c_str() );
#else
	(*fiBitmap) = FreeImage_Load( srcFormat, path.c_str() );
#endif

	if (*fiBitmap)
	{
		*outWidth = FreeImage_GetWidth(*fiBitmap);
		*outHeight = FreeImage_GetHeight(*fiBitmap);
	}
}

template <class T>
static void SetupAndBlit (T* srcImage, int srcWidth, int srcHeight, Vector2f& srcOffset, Vector2f& srcScale,
						  T* dstImage, int dstWidth, int dstHeight, Vector2f& dstOffset, Vector2f& dstScale)
{
	if (!srcImage || !dstImage)
		return;

	int srcPatchX = FloorfToInt (srcOffset.x * srcWidth);
	int srcPatchY = FloorfToInt (srcOffset.y * srcHeight);
	int patchWidth = CeilfToInt (srcScale.x * srcWidth);
	int patchHeight = CeilfToInt (srcScale.y * srcHeight);

	int dstPatchX = FloorfToInt (dstOffset.x * dstWidth);
	int dstPatchY = FloorfToInt (dstOffset.y * dstHeight);

	// expand the patch by 1 px
	patchWidth += 2;
	patchHeight += 2;
	srcPatchX--;
	srcPatchY--;
	dstPatchX--;
	dstPatchY--;

	// sanitize
	srcPatchX = srcPatchX < 0 ? 0 : srcPatchX;
	srcPatchY = srcPatchY < 0 ? 0 : srcPatchY;
	dstPatchX = dstPatchX < 0 ? 0 : dstPatchX;
	dstPatchY = dstPatchY < 0 ? 0 : dstPatchY;
	// don't read past the end
	patchWidth = srcPatchX + patchWidth > srcWidth ? srcWidth - srcPatchX : patchWidth;
	patchHeight = srcPatchY + patchHeight > srcHeight ? srcHeight - srcPatchY : patchHeight;
	// don't write past the end
	patchWidth = dstPatchX + patchWidth > dstWidth ? dstWidth - dstPatchX : patchWidth;
	patchHeight = dstPatchY + patchHeight > dstHeight ? dstHeight - dstPatchY : patchHeight;

	// blit!
	Blit<T> (	srcImage, srcWidth, srcPatchX, srcPatchY,
		dstImage, dstWidth, dstPatchX, dstPatchY, patchWidth, patchHeight);
}

template <class T>
static void GetDestinationLightmap (const int lightmapsMode, const int index, const int lightmapOffset, const int channels, FIBITMAP** destLightmap, dynamic_array<T>& destination, T** dstImage, int& destLightmapWidth, int& destLightmapHeight, string& assetPath, bool& fileExists)
{
	string lightmapPath = GetLightmapPath (lightmapsMode, lightmapOffset, index);
	lightmapPath = GetLightmapTempPath (lightmapPath);

	fileExists = IsFileCreated (lightmapPath);
	if (fileExists)
	{
		assetPath = lightmapPath;

		FreeImageLoad (assetPath, destLightmap, &destLightmapWidth, &destLightmapHeight);
		if (!(*destLightmap))
		{
			*destLightmap = NULL;
			*dstImage = NULL;
			return;
		}

		*dstImage = (T*)FreeImage_GetBits (*destLightmap);
	}
	else
	{
		destination.resize_uninitialized (destLightmapWidth * destLightmapHeight * channels);
		T* data = destination.data ();
		std::uninitialized_fill (data, data + destination.size (), -1.0f);

		*dstImage = data;
	}
}

template <class T>
static void SaveDestinationLightmap (const int index, const int lightmapOffset, const int lightmapsMode, FIBITMAP* destLightmap, dynamic_array<T>& destination, const int destLightmapWidth, const int destLightmapHeight, const bool notOnDiskYet, const string& assetPath, std::vector<string>& lightmapAssetPaths)
{
	if (notOnDiskYet)
	{
		string path = SaveAllModes (lightmapsMode, destLightmapWidth, destLightmapHeight, destination, index, lightmapOffset, true);
		lightmapAssetPaths.push_back (path);
	}
	else
	{
		// already added to lightmapAssetPaths
		SaveAllModes (destLightmap, assetPath);
	}
}

static string GetOldLightmapPath (const LightmapData& lightmapData, const int lightmapOffset)
{
	// load the old lightmap
	Texture2D* oldLightmapTexture = NULL;
	switch (lightmapOffset)
	{
		default:
		case 0:
			oldLightmapTexture = lightmapData.m_Lightmap;
			break;
		case 1:
			oldLightmapTexture = lightmapData.m_IndirectLightmap;
			break;
		case 2:
			oldLightmapTexture = lightmapData.m_ThirdLightmap;
			break;
	}

	if (!oldLightmapTexture)
	{
		return "";
	}
	return GetAssetPathFromInstanceID (oldLightmapTexture->GetInstanceID());
}

template <class T>
static void GetOldLightmap (const std::vector<LightmapData>& oldLightmaps, const int index, const bool lightmapOffset, FIBITMAP** oldLightmap, T** dstImage, int& oldLightmapWidth, int& oldLightmapHeight, string& assetPath)
{
	*dstImage = NULL;
	*oldLightmap = NULL;

	if (index >= oldLightmaps.size ())
		return;

	assetPath = GetOldLightmapPath (oldLightmaps[index], lightmapOffset);

	if (assetPath == "")
		return;

	FreeImageLoad (assetPath, oldLightmap, &oldLightmapWidth, &oldLightmapHeight);

	if (!(*oldLightmap))
		return;

	*dstImage = (T*)FreeImage_GetBits (*oldLightmap);
}

template <class T>
void GetTexturesBakeSelected (LightmapRenderingPasses& renderingPasses, const std::vector<ILBTargetHandle>& textureTargets,vector<AtlasInfoSourceDest>& atlasInfoSelected, vector<AtlasInfoSourceDest>& atlasInfoNonSelected, std::vector<string>& lightmapAssetPaths, const int maxAtlasSize,const int lightmapOffset, bool keepOldAtlasing)
{
	const std::vector<LightmapData>& oldLightmaps = GetLightmapSettings ().GetLightmaps ();

	int prevFramebufferIndex = -1;
	int prevLightmapIndex = -2;

	int lightmapsMode = renderingPasses.GetLightmapsMode();

	const int channels = 3;
	int32 framebufferWidth, framebufferHeight;
	int oldLightmapWidth, oldLightmapHeight;

	int destLightmapWidth = maxAtlasSize;
	int destLightmapHeight = maxAtlasSize;

	FIBITMAP* oldLightmap = NULL;
	FIBITMAP* destLightmap = NULL;
	string destAssetPath;

	dynamic_array<float> texData, texDataDummy;
	dynamic_array<UInt8> texDataShadow, texDataShadowDummy;

	ILBRenderPassHandle renderPass = renderingPasses.GetRenderPass ();
	ILBRenderPassHandle renderPassAO = renderingPasses.GetRenderPassAO ();
	float aoAmount = renderingPasses.GetAOAmount ();
	ColorRGBAf ambientLight = renderingPasses.GetAmbientLight ();

	if (keepOldAtlasing)
	{
		T* srcImage = NULL;
		T* dstImage = NULL;
		for (int bakeInstanceIndex = 0; bakeInstanceIndex < atlasInfoSelected.size(); bakeInstanceIndex++)
		{
			AtlasInfoSourceDest& a = atlasInfoSelected[bakeInstanceIndex];
			if (a.source.index != prevFramebufferIndex)
			{
				prevFramebufferIndex = a.source.index;
				ILBTargetHandle target = textureTargets[a.source.index];

				GetFramebuffer (target, renderPass, renderPassAO, aoAmount, channels, ambientLight, lightmapsMode, lightmapOffset, texData, framebufferWidth, framebufferHeight);
				srcImage = (T*)texData.data();
			}

			if (a.dest.index != prevLightmapIndex)
			{
				if (prevLightmapIndex != -2)
				{
					// save the previous
					SaveAllModes (oldLightmap, destAssetPath);
					lightmapAssetPaths.push_back (destAssetPath);
				}

				prevLightmapIndex = a.dest.index;

				if (a.dest.index < 0 || a.dest.index > oldLightmaps.size ())
				{
					dstImage = NULL;
					oldLightmap = NULL;
				}
				else
					GetOldLightmap<T> (oldLightmaps, a.dest.index, lightmapOffset, &oldLightmap, &dstImage, oldLightmapWidth, oldLightmapHeight, destAssetPath);
			}

			SetupAndBlit<T> (srcImage, framebufferWidth, framebufferHeight, a.source.offset, a.source.scale, dstImage, oldLightmapWidth, oldLightmapHeight, a.dest.offset, a.dest.scale);
		}

		// save the previous
		SaveAllModes (oldLightmap, destAssetPath);
		lightmapAssetPaths.push_back (destAssetPath);
	}
	else
	{
		bool notOnDiskYet;
		dynamic_array<T> destination;
		T* srcImage = NULL;
		T* dstImage = NULL;

		for (int bakedInstanceIndex = 0; bakedInstanceIndex < atlasInfoSelected.size(); bakedInstanceIndex++)
		{
			AtlasInfoSourceDest& a = atlasInfoSelected[bakedInstanceIndex];
			if (a.source.index != prevFramebufferIndex)
			{
				prevFramebufferIndex = a.source.index;
				ILBTargetHandle target = textureTargets[a.source.index];

				GetFramebuffer (target, renderPass, renderPassAO, aoAmount, channels, ambientLight, lightmapsMode, lightmapOffset, texData, framebufferWidth, framebufferHeight);
				srcImage = (T*)texData.data();
			}

			if (a.dest.index != prevLightmapIndex)
			{
				if (prevLightmapIndex != -2)
					SaveDestinationLightmap<T> (prevLightmapIndex, lightmapOffset, lightmapsMode, destLightmap, destination, destLightmapWidth, destLightmapHeight, notOnDiskYet, destAssetPath, lightmapAssetPaths);

				prevLightmapIndex = a.dest.index;

				bool fileExists;
				GetDestinationLightmap<T> (lightmapsMode, a.dest.index, lightmapOffset, channels, &destLightmap, destination, &dstImage, destLightmapWidth, destLightmapHeight, destAssetPath, fileExists);
				notOnDiskYet = !fileExists;
			}

			SetupAndBlit<T> (srcImage, framebufferWidth, framebufferHeight, a.source.offset, a.source.scale, dstImage, destLightmapWidth, destLightmapHeight, a.dest.offset, a.dest.scale);
		}

		if (atlasInfoSelected.size() > 0)
			SaveDestinationLightmap<T> (prevLightmapIndex, lightmapOffset, lightmapsMode, destLightmap, destination, destLightmapWidth, destLightmapHeight, notOnDiskYet, destAssetPath, lightmapAssetPaths);

		prevFramebufferIndex = -1;
		prevLightmapIndex = -2;

		for (int nonBakedInstanceIndex = 0; nonBakedInstanceIndex < atlasInfoNonSelected.size(); nonBakedInstanceIndex++)
		{
			AtlasInfoSourceDest& a = atlasInfoNonSelected[nonBakedInstanceIndex];
			if (a.source.index != prevFramebufferIndex)
			{
				prevFramebufferIndex = a.source.index;

				string dummyOldLightmapAssetPath;
				GetOldLightmap<T> (oldLightmaps, a.source.index, lightmapOffset, &oldLightmap, &srcImage, oldLightmapWidth, oldLightmapHeight, dummyOldLightmapAssetPath);
			}

			if (a.dest.index != prevLightmapIndex)
			{
				if (prevLightmapIndex != -2)
					SaveDestinationLightmap<T> (prevLightmapIndex, lightmapOffset, lightmapsMode, destLightmap, destination, destLightmapWidth, destLightmapHeight, notOnDiskYet, destAssetPath, lightmapAssetPaths);

				prevLightmapIndex = a.dest.index;

				bool fileExists;
				GetDestinationLightmap<T> (lightmapsMode, a.dest.index, lightmapOffset, channels, &destLightmap, destination, &dstImage, destLightmapWidth, destLightmapHeight, destAssetPath, fileExists);
				notOnDiskYet = !fileExists;
			}

			SetupAndBlit<T> (srcImage, oldLightmapWidth, oldLightmapHeight, a.source.offset, a.source.scale, dstImage, destLightmapWidth, destLightmapHeight, a.dest.offset, a.dest.scale);
		}

		if (atlasInfoNonSelected.size() > 0)
			SaveDestinationLightmap<T> (prevLightmapIndex, lightmapOffset, lightmapsMode, destLightmap, destination, destLightmapWidth, destLightmapHeight, notOnDiskYet, destAssetPath, lightmapAssetPaths);
	}
}

LightmapperBeastResults::LightmapperBeastResults(LightmapperBeastShared* lbs)
{
	m_Lbs = lbs;
}

std::vector<Light*> TreeLightmapper::GetLights(LightType type, int& maxShadowSamples)
{
	maxShadowSamples = 0;
	std::vector<Light*> filteredLights;
	LightManager::Lights& lights = GetLightManager().GetAllLights();
	LightManager::Lights::iterator it, itEnd = lights.end();
	for (it = lights.begin(); it != itEnd; ++it)
	{
		Light* light = &*it;
		if (light->GetType() == type)
		{
			filteredLights.push_back(light);
			maxShadowSamples = std::max(light->GetShadowSamples(), maxShadowSamples);
		}
	}
	return filteredLights;
}

ColorRGBAf TreeLightmapper::CalculateTreeLightmapColor(const std::vector<Light*>& lights, const vector<Quaternionf>& shadowJitter, const Vector3f& position, float maxDistance, float totalLightAmount, float grayscaleAmbient)
{
	// TODO(Kuba): move to PhysicsManager?
	const int kIgnoreRaycastLayer = 1 << 2;
	const int kDefaultRaycastLayers = ~kIgnoreRaycastLayer;

	float shadow = grayscaleAmbient;

	vector<Light*>::const_iterator lightIt, lightItEnd = lights.end();
	for (lightIt = lights.begin(); lightIt != lightItEnd; ++lightIt)
	{
		Light& light = *(*lightIt);
		Vector3f baseShadowDir = RotateVectorByQuat (light.GetGameObject().GetComponent(Transform).GetRotation(), Vector3f(0, 0, -1));
		int cullingMask = light.GetCullingMask();
		float intensity = light.GetColor().GreyScaleValue() * light.GetIntensity();
		int occlusion = 0;
		float t = light.GetShadowAngle() / 90.0f;
		float shadowSamples = std::min(light.GetShadowSamples(), (int)shadowJitter.size());

		for (int i = 0; i < shadowSamples; i++)
		{
			Quaternionf rotation = Slerp(Quaternionf::identity(), shadowJitter[i], t);
			Vector3f temp = RotateVectorByQuat (rotation, baseShadowDir);
			Vector3f shadowDir = Normalize(temp);
			Vector3f shadowPos = position;
			Ray ray(shadowPos, shadowDir);
			RaycastHit hit;
			PhysicsManager& physics = GetPhysicsManager();
			if (physics.Raycast (ray, maxDistance, hit, cullingMask & kDefaultRaycastLayers))
			{
				// Fast path if we are not hitting a trigger
				if (!hit.collider->GetIsTrigger())
				{
					occlusion++;
				}
				// Slow path. We have to go through all raycast hits
				else
				{
					const PhysicsManager::RaycastHits& hits = physics.RaycastAll (ray, maxDistance, cullingMask & kDefaultRaycastLayers);

					PhysicsManager::RaycastHits::const_iterator hitsIt, hitsItEnd = hits.end();
					for (hitsIt = hits.begin(); hitsIt != hitsItEnd; ++hitsIt)
					{
						if (!hitsIt->collider->GetIsTrigger())
						{
							occlusion++;
							break;
						}
					}
				}
			}
		}
		float factor = 1.0f - ((float)occlusion / shadowSamples);
		shadow += factor * intensity / totalLightAmount;
	}

	return ColorRGBAf(shadow, shadow, shadow);
}

// Create as many rotations as the light with the highest shadowSamples value needs.
vector<Quaternionf> TreeLightmapper::SuperSampleShadowJitter (int shadowSamples)
{
	vector<Quaternionf> dirs(shadowSamples);
	Rand rand(3277);

	for (int i = 0; i < shadowSamples; i++)
	{
		float r1 = Random01(rand);
		float z = Random01(rand);

		float x = Cos(2.0f * kPI * r1) * Sqrt(1 - z * z);
		float y = Sin(2.0f * kPI * r1) * Sqrt(1 - z * z);
		dirs[i] = FromToQuaternionSafe(Vector3f(x, y, z), Vector3f::zAxis);
	}
	return dirs;
}

void TreeLightmapper::LightmapTrees(TerrainData* terrainData, const Vector3f terrainPosition)
{
	TreeDatabase& treeDatabase = terrainData->GetTreeDatabase();
	std::vector<TreeInstance>& treeInstances = treeDatabase.GetInstances();
	Vector3f terrainSize = terrainData->GetHeightmap().GetSize();
	float maxDistance = Magnitude(terrainSize) * 3.0f;

	int maxShadowSamples;
	const std::vector<Light*> lights = GetLights(kLightDirectional, maxShadowSamples);
	vector<Quaternionf> shadowJitter = SuperSampleShadowJitter(maxShadowSamples);
	
	CALL_UPDATE_MODULAR(PhysicsRefreshWhenPaused)

	float grayscaleAmbient = GetRenderSettings().GetAmbientLight().GreyScaleValue();
	float totalLightAmount = grayscaleAmbient;

	vector<Light*>::const_iterator it, itEnd = lights.end();
	for (it = lights.begin(); it != itEnd; ++it)
	{
		Light& light = *(*it);
		totalLightAmount += light.GetColor().GreyScaleValue() * light.GetIntensity();
	}

	for (std::vector<TreeInstance>::iterator it = treeInstances.begin(), end = treeInstances.end(); it != end; ++it)
	{
		TreeInstance& treeInstance = *it;

		Vector3f position = BeastUtils::GetTreeInstancePosition(treeInstance, terrainSize, terrainPosition);
		const TreeDatabase::Prototype& prototype = treeDatabase.GetPrototypes()[treeInstance.index];

		// move the position slightly above the tree
		position.y += prototype.treeVisibleHeight * treeInstance.heightScale + 0.1f;
		treeInstance.lightmapColor.Set(CalculateTreeLightmapColor(lights, shadowJitter, position, maxDistance, totalLightAmount, grayscaleAmbient));
	}
}

template <class T>
static void UpdateAtlasingOnBakeInstancesHelper (std::vector<T>& instances, Atlasing& atlasing)
{
	dynamic_array<Vector2f>& scales = atlasing.scales;
	dynamic_array<Vector2f>& offsets = atlasing.offsets;
	dynamic_array<int>& indices = atlasing.indices;

	const int count = instances.size();

	for (int i = 0; i < count; i++)
	{
		PPtr<Renderer> renderer = instances[i].renderer;

		if (renderer.IsNull())
			continue;

		Vector2f& scale = scales[i];
		Vector2f& offset = offsets[i];
		int index = indices[i];

		Vector4f lightmapST(scale.x, scale.y, offset.x, offset.y);

		renderer->SetLightmapIndexInt(index);
		renderer->SetLightmapST(lightmapST);
	}
}

static void UpdateAtlasingOnBakeInstancesBakeSelected (std::vector<LightmapperDestinationInstance>& destinationInstances, Atlasing& destinationAtlasing)
{
	UpdateAtlasingOnBakeInstancesHelper (destinationInstances, destinationAtlasing);
}

void LightmapperBeastResults::UpdateAtlasingOnBakeInstances (int lastLightmapIndex)
{
	UpdateAtlasingOnBakeInstancesHelper (m_Lbs->m_BakedInstances, m_Lbs->m_Atlasing);

	// Instances that influence the scene but have 'Scale in lightmap' set to 0
	// don't get lightmaps assigned.
	vector< PPtr<Object> >::const_iterator it, itEnd = m_Lbs->m_NonBakedInstances.end();
	for (it = m_Lbs->m_NonBakedInstances.begin(); it != itEnd; ++it)
	{
		Renderer* renderer = dynamic_pptr_cast<Renderer*>(*it);
		if (renderer)
		{
			renderer->SetLightmapIndexInt(0xFE);
			continue;
		}
		TerrainData* td = dynamic_pptr_cast<TerrainData*>(*it);
		if (td)
			td->SetLightmapIndexOnUsers(0xFE);

	}

	// update atlasing on terrains
	int terrainLightmapIndex = lastLightmapIndex - m_Lbs->Terrains.size () + 1;
	for(std::vector<TerrainBakeInfo>::iterator iter = m_Lbs->Terrains.begin (); iter != m_Lbs->Terrains.end (); ++iter)
	{
		TerrainData* terrainData = iter->terrainData;
		terrainData->SetLightmapIndexOnUsers (terrainLightmapIndex);
		terrainLightmapIndex++;
	}
}

struct CompareAtlasInfoCombined
{
	bool operator ()(const AtlasInfoSourceDest& lhs, const AtlasInfoSourceDest& rhs)
	{
		return lhs.source.index != rhs.source.index ?
			lhs.source.index > rhs.source.index :
			lhs.dest.index > rhs.dest.index;
	}
};

static void GetSecondUVBoundingBox (Renderer* renderer, Vector2f& scale, Vector2f& offset)
{
	scale.Set(1,1);
	offset.Set(0,0);
	Mesh* mesh = NULL;
	MeshRenderer* meshRenderer = dynamic_cast<MeshRenderer*>(renderer);
	if (!meshRenderer)
	{
		SkinnedMeshRenderer* skinnedMeshRenderer = dynamic_cast<SkinnedMeshRenderer*>(renderer);
		if (skinnedMeshRenderer)
			mesh = skinnedMeshRenderer->GetMesh();
	}
	else
	{
		mesh= meshRenderer->GetSharedMesh();
	}
	if (!mesh)
		return;

	int vertexCount = mesh->GetVertexCount();
	if (vertexCount == 0)
		return;

	dynamic_array<Vector2f> uvs (vertexCount, kMemTempAlloc);
	if (mesh->IsAvailable(kShaderChannelTexCoord1))
		mesh->ExtractUvArray (1, uvs.data());
	else
		mesh->ExtractUvArray (0, uvs.data ());

	Vector2f min = uvs[0];
	Vector2f max = uvs[0];
	for (int i = 0; i < vertexCount; i++)
	{
		Vector2f& v = uvs[i];
		if (v.x < min.x)
			min.x = v.x;
		if (v.y < min.y)
			min.y = v.y;
		if (v.x > max.x)
			max.x = v.x;
		if (v.y > max.y)
			max.y = v.y;
	}

	// clamp
	min.x = clamp(min.x, 0.0f, 1.0f);
	min.y = clamp(min.y, 0.0f, 1.0f);
	max.x = clamp(max.x, 0.0f, 1.0f);
	max.y = clamp(max.y, 0.0f, 1.0f);

	offset = min;
	scale = max - min;
}

static void TransformByUVBounds (const Rectf& uvBounds, const Vector2f& scale, const Vector2f& offset, Vector2f& outScale, Vector2f& outOffset)
{
	outScale.x = scale.x * uvBounds.width;
	outScale.y = scale.y * uvBounds.height;
	outOffset.x = offset.x + uvBounds.x * scale.x;
	outOffset.y = offset.y + uvBounds.y * scale.y;
}

void LightmapperBeastResults::FetchAtlasing(vector<AtlasInfoSourceDest>& atlasInfoSelected, vector<AtlasInfoSourceDest>& atlasInfoNonSelected)
{
	BakedInstances& bakedInstances = m_Lbs->m_BakedInstances;
	dynamic_array<Vector2f>& scales = m_Lbs->m_Atlasing.scales;
	dynamic_array<Vector2f>& offsets = m_Lbs->m_Atlasing.offsets;
	dynamic_array<int>& indices = m_Lbs->m_Atlasing.indices;

	const std::vector<LightmapperDestinationInstance>& destInstances = m_Lbs->m_DestinationInstances;
	const Atlasing& destAtlasing = m_Lbs->m_DestinationAtlasing;

	const int count = bakedInstances.size();
	bool keepOldAtlasing = m_Lbs->m_KeepOldAtlasingInBakeSelected;

	for (int i = 0; i < count; i++)
	{
		Renderer* renderer = bakedInstances[i].renderer;

		if (!renderer)
			continue;

		Vector2f& scale = scales[i];
		Vector2f& offset = offsets[i];
		int index = indices[i];
		const Rectf& uvBounds = bakedInstances[i].uvBounds;

		AtlasInfoSourceDest atlasInfoCombined;
		atlasInfoCombined.source.index = index;
		TransformByUVBounds(uvBounds, scale, offset, atlasInfoCombined.source.scale, atlasInfoCombined.source.offset);

		if (keepOldAtlasing)
		{
			Vector4f lightmapST = renderer->GetLightmapST ();
			int lightmapIndex = renderer->GetLightmapIndexInt ();

			atlasInfoCombined.dest.index = lightmapIndex;
			TransformByUVBounds(uvBounds, Vector2f(&lightmapST.x), Vector2f(&lightmapST.z), atlasInfoCombined.source.scale, atlasInfoCombined.source.offset);
		}
		else
		{
			int j = 0;
			for (; j < destInstances.size (); j++)
			{
				if (destInstances[j].renderer == renderer)
					break;
			}

			if (j < destInstances.size ())
			{
				// found the right renderer at j
				atlasInfoCombined.dest.index = destAtlasing.indices[j];
				TransformByUVBounds(uvBounds, destAtlasing.scales[j], destAtlasing.offsets[j], atlasInfoCombined.dest.scale, atlasInfoCombined.dest.offset);
			}

		}

		atlasInfoSelected.push_back (atlasInfoCombined);
	}

	for (int i = 0; i < destInstances.size (); i++)
	{
		if (destInstances[i].selected)
			continue;

		Renderer* renderer = destInstances[i].renderer;

		Vector4f lightmapST = renderer->GetLightmapST ();
		int lightmapIndex = renderer->GetLightmapIndexInt ();

		AtlasInfoSourceDest atlasInfoCombined;

		const Rectf& uvBounds = destInstances[i].uvBounds;

		atlasInfoCombined.source.index = lightmapIndex;
		TransformByUVBounds(uvBounds, Vector2f(&lightmapST.x), Vector2f(&lightmapST.z), atlasInfoCombined.source.scale, atlasInfoCombined.source.offset);

		atlasInfoCombined.dest.index = destAtlasing.indices[i];
		TransformByUVBounds(uvBounds, destAtlasing.scales[i], destAtlasing.offsets[i], atlasInfoCombined.dest.scale, atlasInfoCombined.dest.offset);

		atlasInfoNonSelected.push_back (atlasInfoCombined);
	}

	std::stable_sort (atlasInfoSelected.begin (), atlasInfoSelected.end (), CompareAtlasInfoCombined ());
	std::stable_sort (atlasInfoNonSelected.begin (), atlasInfoNonSelected.end (), CompareAtlasInfoCombined ());
}


static void ExtractLightProbeData (LightmapperBeastShared& shared, dynamic_array<Vector3f>& outputPositions, dynamic_array<LightProbeCoefficients>& output)
{
	outputPositions.clear();
	output.clear();

	if (shared.m_LightProbeSourcePositions.size() == 0)
		return;

	// Retrieve point cloud data
	ILBFramebufferHandle fb;
	BeastCall(ILBGetVertexbuffer(shared.m_LightProbeTarget, shared.Lrp.GetRenderPassLightProbes(), shared.m_LightProbeEntity, &fb));
	int32 channels;
	BeastCall(ILBGetChannelCount(fb, &channels));

	// channels contain 9 coefficients for rgb in two configs: full and indirect light
	if (channels != kLightProbeBasisCount*3*2)
	{
		ErrorString(Format("Error: SH pass should output %i coefficients.", kLightProbeBasisCount*3*2));
	}
	else
	{
		int32 width, height, probeCount = shared.m_LightProbeSourcePositions.size();
		BeastCall(ILBGetResolution(fb, &width, &height));
		Assert (height == 1);
		Assert (width == probeCount);

		// Coefficients are stored for a given probe first kLightProbeBasisCount*3 floats for
		// indirect light (dual lightmaps style, i.e. with direct light from baked only and emissive)
		// and then kLightProbeBasisCount*3 float for full light, so offset 1 for single (probes get full lighting)
		// and 0 for dual mode (probes get indirect lighting).
		const int offset = shared.Lrp.GetLightmapsMode() != LightmapSettings::kDualLightmapsMode ? 1 : 0;
		output.resize_uninitialized(probeCount);
		dynamic_array<LightProbeCoefficients> temp;
		temp.resize_uninitialized(probeCount*2);
		BeastCall(ILBReadRegionHDR(fb, 0, 0, probeCount, 1, ILB_CS_ALL, (float*)&temp[0]));
		// The data needs to be laid out a bit differently than beast outputs it
		for (int i = 0; i < probeCount; i++)
		{
			// The temp array with beast output is twice as long, because kLightProbeBasisCount*3 coeffs of
			// indirect lighting are followed by kLightProbeBasisCount*3 coeffs of direct lighting.
			LightProbeCoefficients& src = temp[i*2 + offset];
			LightProbeCoefficients& dst = output[i];

			// For some reason Beast outputs probes rotated by PI around Z (i.e. they would need to be sampled with a normal with negated x and y)
			// To work around this, we negate the 1st, 3rd, 5th and 7th SH coefficient, which undoes the rotation.
			for (int j = 0; j < kLightProbeBasisCount; j++)
				for (int k = 0; k < 3; k++)
					src[k*kLightProbeBasisCount + j] = (1 - 2*(j%2))*src[k*kLightProbeBasisCount + j];

			// change layout from RRRRRRRRRGGGGGGGGGBBBBBBBBB to RGBRGBRGBRGBRGBRGBRGBRGBRGB
			for (int j = 0; j < kLightProbeBasisCount; j++)
				for (int k = 0; k < 3; k++)
					dst[j*3 + k] = src[k*kLightProbeBasisCount + j];
		}

		outputPositions = shared.m_LightProbeSourcePositions;
	}
}

static void AssignLightmaps(const int lightmapsMode, const int count)
{
	string folderPath = GetSceneBakedAssetsPath();
	const int lightmapCountForMode = LightmapCountForLightmapsMode(lightmapsMode);
	std::vector<LightmapData> lightmapDataVector;
	for (int i = 0; i < count; i++)
	{
		LightmapData ld;
		for (int lightmapOffset = 0; lightmapOffset < lightmapCountForMode; lightmapOffset++)
		{
			string path = GetLightmapPath(lightmapsMode, lightmapOffset, i);
			if (IsFileCreated(path))
			{
				Texture2D* lm = dynamic_pptr_cast<Texture2D*>(GetMainAsset(path));
				if (lightmapOffset == 0)
					ld.m_Lightmap = lm;
				else if (lightmapOffset == 1)
					ld.m_IndirectLightmap = lm;
				else if (lightmapOffset == 2)
					ld.m_ThirdLightmap = lm;
			}
		}
		lightmapDataVector.push_back(ld);
	}
	GetLightmapSettings().SetLightmaps(lightmapDataVector);
}

static void LightmapTrees (std::vector<TerrainBakeInfo> terrainTargets)
{
	for(std::vector<TerrainBakeInfo>::iterator iter = terrainTargets.begin (); iter != terrainTargets.end (); ++iter)
	{
		TerrainData* terrainData = iter->terrainData;
		Vector3f terrainPosition = iter->position;

		TreeLightmapper::LightmapTrees (terrainData, terrainPosition);
		terrainData->UpdateUsers (TerrainData::kFlushEverythingImmediately);
	}
}

static void GetTerrainTexturesBakeSelected (LightmapRenderingPasses& renderingPasses, const std::vector<ILBTargetHandle>& textureTargets, std::vector<TerrainBakeInfo>& terrains, std::vector<string>& lightmapAssetPaths, int stride)
{
	int selectedTerrains = 0;
	for(std::vector<TerrainBakeInfo>::iterator terrainBakeInfoIter = terrains.begin (); terrainBakeInfoIter != terrains.end (); ++terrainBakeInfoIter)
	{
		if (terrainBakeInfoIter->selected)
			selectedTerrains++;
	}

	// the last few texture targets are the targets for selected terrains
	int selectedTerrainTextureTargetIndex = textureTargets.size () - selectedTerrains;

	const std::vector<LightmapData>& oldLightmaps = GetLightmapSettings ().GetLightmaps ();
	for(std::vector<TerrainBakeInfo>::iterator terrainBakeInfoIter = terrains.begin (); terrainBakeInfoIter != terrains.end (); ++terrainBakeInfoIter)
	{
		int lightmapIndex = lightmapAssetPaths.size () / stride;
		bool atLeastOneValidLightmap = false;
		for (int lightmapOffset = 0; lightmapOffset < stride; lightmapOffset++)
		{
			string pathInAssetsFolder;
			if (terrainBakeInfoIter->selected)
			{
				pathInAssetsFolder = renderingPasses.GetLightmapDataForTarget (textureTargets[selectedTerrainTextureTargetIndex], lightmapIndex, lightmapOffset, true);
				atLeastOneValidLightmap = true;
			}
			else
			{
				if (terrainBakeInfoIter->lightmapIndex > oldLightmaps.size ())
					continue;

				string oldLightmapPath = GetOldLightmapPath (oldLightmaps[terrainBakeInfoIter->lightmapIndex], lightmapOffset);
				if (oldLightmapPath == "")
					continue;

				pathInAssetsFolder = GetLightmapPath (renderingPasses.GetLightmapsMode (), lightmapOffset, lightmapIndex);
				string destinationPath = GetLightmapTempPath (pathInAssetsFolder);

				CopyFileOrDirectory (oldLightmapPath, destinationPath);

				atLeastOneValidLightmap = true;
			}
			// even if we saved to the temp folder, return a path starting in the Assets folder
			// as we'll use it as the excluded path when deleting excess lightmaps
			lightmapAssetPaths.push_back (pathInAssetsFolder);
		}

		if (atLeastOneValidLightmap)
		{
			TerrainData* terrainData = terrainBakeInfoIter->terrainData;
			terrainData->SetLightmapIndexOnUsers (lightmapIndex);
		}

		if (terrainBakeInfoIter->selected)
			selectedTerrainTextureTargetIndex++;
	}
}

static bool OpenLightmapsForEdit( string folderPath ) 
{
	// TODO: Figure out a way to identify which target files are actually overwritten in order to only
	//       Version Control checkout them and not all lightmap files.
	//       Probably create/use a generic AssetModificationProcessor callback for this.
	set<string> pathCandidatesToCheckout;
	VCAssetList assetsToCheckout;

	GetFolderContentsAtPath(folderPath, pathCandidatesToCheckout);
	for (set<string>::iterator i = pathCandidatesToCheckout.begin(); i != pathCandidatesToCheckout.end(); ++i)
	{
		if (EndsWith(ToLower(*i), ".exr"))
			assetsToCheckout.push_back(VCAsset(*i));
	}

	if (!assetsToCheckout.empty() && !GetVCProvider().PromptAndCheckoutIfNeeded(assetsToCheckout, ""))
	{
		ErrorString("Could not open lightmaps for edit");
		return false;
	}

	return true;
}

void LightmapperBeastResults::RetrieveLightmaps()
{
	const int lightmapsMode = m_Lbs->Lrp.GetLightmapsMode ();
	const int stride = LightmapCountForLightmapsMode (lightmapsMode);
	
	string folderPath = GetSceneBakedAssetsPath();
	
	if (!OpenLightmapsForEdit(folderPath))
		return;

	AssetInterface& ai = AssetInterface::Get();
	ai.StartAssetEditing();
	{
		try
		{
			
			AssertIf(folderPath == "");
			CreateDirectory(folderPath);

			if (m_Lbs->m_SelectedOnly)
			{
				vector<AtlasInfoSourceDest> atlasInfoSelected, atlasInfoNonSelected;
				FetchAtlasing (atlasInfoSelected, atlasInfoNonSelected);

				bool keepOldAtlasing = m_Lbs->m_KeepOldAtlasingInBakeSelected;
				std::vector<string> tempLightmapAssetsPaths;

				for (int lightmapOffset = 0; lightmapOffset < stride; lightmapOffset++)
					GetTexturesBakeSelected<float> (m_Lbs->Lrp, m_Lbs->TextureTargets, atlasInfoSelected, atlasInfoNonSelected, keepOldAtlasing ? m_LightmapAssetPaths : tempLightmapAssetsPaths, m_Lbs->m_MaxAtlasSize, lightmapOffset, keepOldAtlasing);

				GetTerrainTexturesBakeSelected (m_Lbs->Lrp, m_Lbs->TextureTargets, m_Lbs->Terrains, tempLightmapAssetsPaths, stride);

				if (!keepOldAtlasing)
				{
					// delete lightmaps that won't be overriden
					DeleteLightmapAssets (&tempLightmapAssetsPaths, stride);

					for (int i = 0; i < tempLightmapAssetsPaths.size (); i++)
					{
						string to = tempLightmapAssetsPaths[i];
						if (to == "")
							continue;

						string from = GetLightmapTempPath (to);

						if (!DeleteFile (to))
						{
							// file didn't exist, so set sensible settings
							SetTextureImporterSettings (to, TextureImporter::kLightmap);
							DeleteFile (to);
						}

						MoveFileOrDirectory (from, to);

						m_LightmapAssetPaths.push_back(to);
					}

					UpdateAtlasingOnBakeInstancesBakeSelected (m_Lbs->m_DestinationInstances, m_Lbs->m_DestinationAtlasing);
				}
			}
			else if (!m_Lbs->m_LockAtlas)
			{
				ClearLightmaps (!m_Lbs->m_SelectedOnly);

				int lastLightmapIndex;

				// fetch all texture targets (both for renderers and terrains)
				for (int i = 0; i < m_Lbs->TextureTargets.size (); i++)
					lastLightmapIndex = GetTexturesFromTarget (m_Lbs->TextureTargets[i]);

				BeastUtils::ClearLightmapIndices ();

				UpdateAtlasingOnBakeInstances (lastLightmapIndex);

				// delete old lightmaps (so all lightmaps except for the ones newly created)
				DeleteLightmapAssets (&m_LightmapAssetPaths, stride);
			}
			else
			{
				ClearLightmaps(false);
				// fetch all texture targets (both for renderers and terrains)
				for (int i = 0; i < m_Lbs->TextureTargets.size(); i++)
				{
					if (m_Lbs->TextureTargetsUsed[i])
						GetTexturesFromTarget (m_Lbs->TextureTargets[i]);
					else
					{
						// create an empty slot at this index
						string empty = "";
						for (int i = 0; i < stride; i++)
							m_LightmapAssetPaths.push_back(empty);
					}
				}
			}

			// import all lightmaps
			for (int i = 0; i < m_LightmapAssetPaths.size (); i++)
				ai.ImportAtPath (m_LightmapAssetPaths[i]);
		}
		catch (BeastException& be)
		{
			ai.StopAssetEditing();
			throw be;
		}
	}
	ai.StopAssetEditing();

	if (!m_Lbs->m_SelectedOnly)
	{
		// fill in LightmapSettings.lightmaps with references to the assets
		std::vector<LightmapData> lightmapDataVector;
		for (int i = 0; i < m_LightmapAssetPaths.size(); i += stride)
		{
			LightmapData ld;
			ld.m_Lightmap = dynamic_pptr_cast<Texture2D*>(GetMainAsset(m_LightmapAssetPaths[i]));
			if (stride > 1)
				ld.m_IndirectLightmap = dynamic_pptr_cast<Texture2D*>(GetMainAsset(m_LightmapAssetPaths[i + 1]));
			if (stride > 2)
				ld.m_ThirdLightmap = dynamic_pptr_cast<Texture2D*>(GetMainAsset(m_LightmapAssetPaths[i + 2]));
			lightmapDataVector.push_back(ld);
		}
		GetLightmapSettings().SetLightmaps(lightmapDataVector);
	}
	else
	{
		AssignLightmaps (lightmapsMode, m_LightmapAssetPaths.size () / stride);
	}

	GetLightmapSettings().SetBakedColorSpace(GetActiveColorSpace());

	// lightmap trees
	LightmapTrees (m_Lbs->Terrains);

	// go over all baked lights and mark them as actually baked
	for (size_t i = 0; i < m_Lbs->m_BakedLights.size(); ++i)
	{
		Light* l = m_Lbs->m_BakedLights[i];
		if (!l)
			continue; // might be deleted while we were baking
		l->SetActuallyLightmapped (true);
	}
	// go over all non baked lights and mark them as actually not baked
	for (size_t i = 0; i < m_Lbs->m_NonBakedLights.size(); ++i)
	{
		Light* l = m_Lbs->m_NonBakedLights[i];
		if (!l)
			continue; // might be deleted while we were baking
		l->SetActuallyLightmapped (false);
	}

	GetLightmapEditorSettings().UpdateResolutionOnBakeSuccess();
}

void LightmapperBeastResults::RetrieveLightProbes()
{
	// No matter what light probe asset is currently referenced by the scene, we still want to create (/overwrite)
	// the asset at the default location and write the new data into it.
	// We don't want to mess up any other light probe assets that the scene might currently be referencing.

	dynamic_array<LightProbeCoefficients> lightprobeOutput;
	dynamic_array<Vector3f> lightprobePositions;
	ExtractLightProbeData (*m_Lbs, lightprobePositions, lightprobeOutput);

	if (lightprobePositions.empty())
	{
		// Delete the LightProbes.asset - there's no need to keep it around if we have 0 probes and we would have to zero it out anyway
		LightProbeUtils::Clear ();
	}
	else
	{
		string assetDirectory = GetSceneBakedAssetsPath();
		if(!CreateDirectory(assetDirectory))
		{
			ErrorString( Format("Could not create asset directory %s.", assetDirectory.c_str()) );
		}
		else
		{
			LightProbes* lightProbes = CreateObjectFromCode<LightProbes>(kInstantiateOrCreateFromCodeAwakeFromLoad);
			DebugAssert(lightProbes);
			if (lightProbes)
			{
				AssetInterface::Get().CreateSerializedAsset (*lightProbes, AppendPathName(assetDirectory, LIGHT_PROBE_ASSET_NAME), AssetInterface::kDeleteExistingAssets | AssetInterface::kWriteAndImport);
				GetLightmapSettings().SetLightProbes(lightProbes);
				lightProbes->SetBakedData(lightprobePositions, lightprobeOutput);
			}
		}
	}
}

void LightmapperBeastResults::Retrieve()
{
	if (!m_Lbs->m_LightProbesOnly)
		RetrieveLightmaps();

	RetrieveLightProbes();

	///@TODO: this should use class AnalyticsProcessTracker instead!
	// Track bake time in seconds (includes scene export time)
	float bakeTime = GetTimeSinceStartup() - m_Lbs->m_BakeStartTime;
	AnalyticsTrackEvent("LightMapper", "BakeTime", "", RoundfToInt(bakeTime));

	BeastCall(ILBDestroyJob(m_Lbs->Job));
}

#endif // #if ENABLE_LIGHTMAPPER
