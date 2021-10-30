#include "UnityPrefix.h"
#include "TextureImporter.h"
#include "DefaultImporter.h"
#include "AssetDatabase.h"
#include "AssetImporterUtil.h"
#include "AssetImportState.h"
#include "AssetInterface.h"
#include "ImageConverter.h"
#include "ImageOperations.h"
#include "Runtime/Graphics/CubemapTexture.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Utilities/Word.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/EditorSettings.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Editor/Src/Application.h"
#include "Runtime/Utilities/Argv.h"

// Increase if you want all textures to be reimported / upgraded.
//
// Usually when adding a new feature to the importer, there is no need to increase the number since the feature should probably not auto-enable itself on old import settings.
// However, if you change the way the data is generated (e.g. different compression), you should update version, so it plays nice with cache server
//
// It does not matter what is the exact version, as long as it's just different
// from the version before!
//
// Use current date in YYYYMMDD0 (last digit dummy) format for the version.
// If conflicts merging, just enter current date and increase last digit.
#define UNITY_TEXTURE_IMPORT_VERSION 201303120

int TextureImporter::CanLoadPathName (const string& pathName, int *queue/*=NULL*/)
{
	if (queue != NULL)
		*queue = -1003;
	string ext = ToLower (GetPathNameExtension (pathName));
	if (ext != "jpg" && ext != "jpeg" && ext != "tif" && ext != "tiff" && ext != "tga" &&  ext != "gif" && ext != "png" &&  ext != "psd" && ext != "bmp" && ext != "iff" && ext != "pict" && ext != "pic" && ext != "pct" && ext != "exr")
		return false;
	else
		return true;
}

void TextureImporter::InitializeClass ()
{
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (TextureImporter), UNITY_TEXTURE_IMPORT_VERSION);
	RegisterAllowNameConversion("TextureImporter", "generation", "m_EnableMipMap");
	RegisterAllowNameConversion("TextureImporter", "border", "m_BorderMipMap");
	RegisterAllowNameConversion("TextureImporter", "filter", "m_MipMapMode");
	RegisterAllowNameConversion("TextureImporter", "fadeout", "m_FadeOut");
	RegisterAllowNameConversion("TextureImporter", "fadeoutStart", "m_MipMapFadeDistanceStart");
	RegisterAllowNameConversion("TextureImporter", "fadeoutEnd", "m_MipMapFadeDistanceEnd");
	RegisterAllowNameConversion("TextureImporter", "fadeoutEnd", "m_MipMapFadeDistanceEnd");

	RegisterAllowNameConversion("TextureImporter", "generation", "m_ConvertToNormalMap");
	RegisterAllowNameConversion("TextureImporter", "external", "m_ExternalNormalMap");
	RegisterAllowNameConversion("TextureImporter", "bumpyness", "m_HeightScale");
	RegisterAllowNameConversion("TextureImporter", "filter", "m_NormalMapFilter");

	RegisterAllowNameConversion("TextureImporter", "npotScale", "m_NPOTScale");
	RegisterAllowNameConversion("TextureImporter", "grayscaleToAlpha", "m_GrayScaleToAlpha");

	RegisterAllowNameConversion("GLTextureSettings", "anisoLevel", "m_Aniso");
	RegisterAllowNameConversion("GLTextureSettings", "mipmapBias", "m_MipBias");
}

TextureImporter::TextureImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	ResetTextureImportOutput ();
	m_TextureType = -1;
}


void TextureImporter::ResetTextureImportOutput ()
{
	memset(&m_Output, 0, sizeof(m_Output));
	m_Output.sourceTextureInformation.width = -1;
	m_Output.sourceTextureInformation.height = -1;
	m_Output.sourceTextureInformation.doesTextureContainAlpha = true;
	m_Output.sourceTextureInformation.doesTextureContainColor = true;
}

void TextureImporter::ClearPreviousImporterOutputs ()
{
	ResetTextureImportOutput ();
}

TextureImporter::TextureType TextureImporter::GetTextureType () const
{
	// If we haven't figured out a texture type yet, now is the time.
	if (m_TextureType == -1)
	 {
		 // Try to guess which texture type we have from the settings.
		 TextureType texType = GuessTextureTypeFromSettings (m_AllSettings, m_Output.sourceTextureInformation);
		 if (texType != kAdvanced)
		 {
			 // Be sure by converting back and checking if the settings are different)
			 Settings settings2 = m_AllSettings;
			 settings2.ApplyTextureType (texType, false);
			 // If they don't match, we set the texture type to be advanced.
			 if (!(settings2 == m_AllSettings))
				 texType = kAdvanced;
		 }
		 return texType;
	 }
	return (TextureImporter::TextureType)m_TextureType;
}

void TextureImporter::SetTextureType (int type, bool applyAll)
{
	if (m_TextureType != type)
	{
		m_TextureType = type;
		SetDirty();
	}

	TextureImporter::Settings settings = GetSettings();
	settings.ApplyTextureType((TextureImporter::TextureType)m_TextureType, applyAll);
	SetSettings(settings);

#if ENABLE_SPRITES
	SpriteSheetMetaData spriteSheet = GetSpriteSheetMetaData();
	SetSpriteSheetMetaData(spriteSheet);
#endif
}

TextureImporter::~TextureImporter ()
{
}

void TextureImporter::Reset ()
{
	Super::Reset();

	ResetTextureImportOutput();
	m_AllSettings = Settings();
	m_BuildTargetSettings.clear();

#if ENABLE_SPRITES
	if (GetEditorSettings().GetDefaultBehaviorMode() == EditorSettings::kMode2D)
	{
		m_TextureType = kSprite;
		m_AllSettings.ApplyTextureType((TextureType)m_TextureType, true);
		m_AllSettings.m_SpriteMode = kSpriteModeSingle;

		//WORKAROUND 576518: Default iOS textures to 16bit.
		{
		BuildTargetSettings iOS;
		iOS.m_BuildTarget = GetBuildTargetShortName(kBuild_iPhone);
		iOS.m_CompressionQuality = m_AllSettings.m_CompressionQuality;
		iOS.m_MaxTextureSize = m_AllSettings.m_MaxTextureSize;
		iOS.m_TextureFormat =  k16bit;
		m_BuildTargetSettings.push_back(iOS);
		}
	}
	else
	{
		m_TextureType = -1;
	}
#else
	m_TextureType = -1;
#endif

	m_AllSettings.m_TextureSettings.Invalidate();
#if ENABLE_SPRITES
	m_SpriteSheet = SpriteSheetMetaData::SpriteSheetMetaData();
	m_SpritePackingTag.clear();
#endif
}

void TextureImporter::CheckConsistency()
{
	Super::CheckConsistency();

#if ENABLE_SPRITES
	m_AllSettings.m_SpritePixelsToUnits = std::max(m_AllSettings.m_SpritePixelsToUnits, 0.001f);
	m_AllSettings.m_SpriteExtrude = clamp<UInt32>(m_AllSettings.m_SpriteExtrude, 0, 32);
#endif
}


void TextureImporter::ProcessMipLevel (ImageReference& curFloatImage, const TextureImportInstructions& instructions, const TextureColorSpace outputColorSpace) const
{
	// curFloatImage is in linear space, either convert it to gamma space
	// or convert to normal map or encode the lightmap
	if (outputColorSpace == kTexColorSpaceSRGBXenon)
		LinearToGammaSpaceXenon (curFloatImage);
	else if (outputColorSpace == kTexColorSpaceSRGB)
		SRGB_LinearToGamma (curFloatImage);
	else if (m_AllSettings.m_ConvertToNormalMap)
		ConvertHeightToNormalMap (curFloatImage, m_AllSettings.m_HeightScale, m_AllSettings.m_NormalMapFilter==TextureImporter::kSobelFilter, instructions.usageMode==kTexUsageNormalmapDXT5nm);
	else if (m_AllSettings.m_NormalMap && instructions.usageMode == kTexUsageNormalmapDXT5nm)
		ConvertExternalNormalMapToDXT5nm(curFloatImage);
	else if (instructions.usageMode == kTexUsageLightmapRGBM)
		ConvertImageToRGBM(curFloatImage);
	else if (instructions.usageMode == kTexUsageLightmapDoubleLDR)
		ConvertImageToDoubleLDR(curFloatImage);
}

// When the lightmap doesn't need to be reformatted the highest mip level will be processed in place,
// so srcImage will be modified.
void TextureImporter::ProcessImportedLightmap(ImageReference& srcImage, const TextureImportInstructions& instructions, int mipCount, Texture2D* texture, UInt8* rawImageData) const
{
	int srcWidth = srcImage.GetWidth();
	int srcHeight = srcImage.GetHeight();
	int mipCountFromSrc = CalculateMipMapCount3D (srcWidth, srcHeight, 1);
	int mipCountForResize = CalculateMipMapCount3D (instructions.width, instructions.height, 1);

	dynamic_array<ImageReference*> imageReferences(mipCount, kMemTempAlloc);
	dynamic_array<Image*> images(mipCount, kMemTempAlloc);

	if (srcImage.GetFormat() != kTexFormatARGBFloat)
	{
		// copy the source image into a float texture
		images[0] = new Image(srcWidth, srcHeight, kTexFormatARGBFloat);
		images[0]->BlitImage (srcImage);
		imageReferences[0] = images[0];
	}
	else
	{
		images[0] = NULL;
		imageReferences[0] = &srcImage;
	}

	// user-limited size - resize step by step
	for (int mip = 0; mip < mipCountFromSrc - mipCountForResize; mip++)
	{
		Image* dstMipMap = new Image();
		ComputeNextMipLevelLightmap (*(imageReferences[0]), *dstMipMap);
		delete images[0];
		images[0] = dstMipMap;
		imageReferences[0] = dstMipMap;
	}

	// push
	for (int mip = 1; mip < mipCount; mip++)
	{
		Image* dstMipMap = new Image();
		ComputeNextMipLevelLightmap (*(imageReferences[mip-1]), *dstMipMap);
		images[mip] = dstMipMap;
		imageReferences[mip] = dstMipMap;
	}

	// pull
	for (int mip = mipCount - 2; mip >= 0; mip--)
	{
		ComputePrevMipLevelLightmap(*(imageReferences[mip+1]), *(imageReferences[mip]));
	}

	// encode lightmaps and blit into destination texture
	for (int mip = 0; mip < mipCount; mip++)
	{
		ImageReference* floatMipMap = imageReferences[mip];

		ProcessMipLevel (*floatMipMap, instructions, kTexColorSpaceLinear);
		BlitImageIntoTextureLevel (mip, texture, &rawImageData, *floatMipMap);

		delete images[mip];
	}
}


static void NeedsReformat (const ImageReference& imageReference, int width, int height, TextureFormat format, bool* needsResize, bool* needsReformat)
{
	*needsResize = (width != imageReference.GetWidth() || height != imageReference.GetHeight());
	*needsReformat = (format != imageReference.GetFormat() && imageReference.GetFormat() != kTexFormatARGBFloat);
}

static void SwapWorkBuffers (ImageReference** curImage, ImageReference** nextImage, Image* workBuffers[], int* currentWorkBuffer)
{
	*curImage = workBuffers[*currentWorkBuffer];
	*nextImage = workBuffers[1 - *currentWorkBuffer];
	*currentWorkBuffer = 1 - *currentWorkBuffer;
}

// When the texture doesn't need to be reformatted the highest mip level will be processed in place,
// so srcImage will be modified.
void TextureImporter::ProcessImportedImage (Texture2D* texture, ImageReference& srcImage, const TextureImportInstructions& instructions, int frame) const
{
	int mipCount = texture->CountDataMipmaps();
	UInt8* rawImageData = texture->GetRawImageData(frame);

	texture->SetUsageMode(instructions.usageMode);

	texture->SetStoredColorSpace(instructions.colorSpace);

	int finalWidth = instructions.width;
	int finalHeight = instructions.height;
	int dstFormat = instructions.compressedFormat;

	// Generate alpha from rgb grayscale if desired
	if (m_AllSettings.m_GrayScaleToAlpha)
		GrayScaleRGBToAlpha (srcImage.GetImageData (), srcImage.GetWidth (), srcImage.GetHeight (), srcImage.GetRowBytes ());

	if(m_AllSettings.m_AlphaIsTransparency)
		AlphaDilateImage(srcImage);
	bool isPOT = IsPowerOfTwo(srcImage.GetWidth()) && IsPowerOfTwo(srcImage.GetHeight()) &&
		IsPowerOfTwo(finalWidth) && IsPowerOfTwo(finalHeight);
	bool preservesRatio = ((float)srcImage.GetWidth () / srcImage.GetHeight ()) == ((float)finalWidth / finalHeight);
	bool isLightmap = (instructions.usageMode == kTexUsageLightmapRGBM) || (instructions.usageMode == kTexUsageLightmapDoubleLDR);

	if (isLightmap && isPOT && preservesRatio)
	{
		ProcessImportedLightmap(srcImage, instructions, mipCount, texture, rawImageData);
		return;
	}

	TextureFormat workingFormat;
	switch (instructions.usageMode)
	{
		case kTexUsageLightmapDoubleLDR:
		case kTexUsageLightmapRGBM:
			workingFormat = kTexFormatARGBFloat;
			break;
		case kTexUsageNone:
		case kTexUsageNormalmapPlain:
		case kTexUsageNormalmapDXT5nm:
		default:
			workingFormat = kTexFormatARGB32;
			break;
	}
	if (m_AllSettings.m_ConvertToNormalMap)
		workingFormat = kTexFormatARGBFloat;

	Image workBuffer1;
	Image workBuffer2;
	Image* workBuffers[] = {&workBuffer1, &workBuffer2};

	int currentWorkBuffer = 0;

	ImageReference* curImage = &srcImage;
	ImageReference* nextImage = workBuffers[currentWorkBuffer];

	bool needsResize, needsReformat;
	NeedsReformat (*curImage, finalWidth, finalHeight, workingFormat, &needsResize, &needsReformat);

	if (needsResize || needsReformat)
	{
		workBuffers[currentWorkBuffer]->ReformatImage (*curImage, finalWidth, finalHeight, workingFormat, Image::BLIT_BILINEAR_SCALE);
		SwapWorkBuffers (&curImage, &nextImage, workBuffers, &currentWorkBuffer);
	}

	bool convertToLinearSpaceForMipMapGeneration = m_AllSettings.m_GenerateMipsInLinearSpace && instructions.colorSpace != kTexColorSpaceLinear;

	TextureColorSpace outputColorSpace = instructions.colorSpace == kTexColorSpaceSRGBXenon ? kTexColorSpaceSRGBXenon : (convertToLinearSpaceForMipMapGeneration ? kTexColorSpaceSRGB : kTexColorSpaceLinear);

	// Don't convert from gamma space to linear and back to gamma for the 0th mip level.
	// Better to blit from curImage (gamma space) directly into output texture.
	// This will skip ProcessMipLevel for the 0th level, but it was just converting from linear to gamma.
	bool skipBaseMipProcessing = (outputColorSpace == kTexColorSpaceSRGB);
	if (skipBaseMipProcessing)
		BlitImageIntoTextureLevel (0, texture, &rawImageData, *curImage);

	if (convertToLinearSpaceForMipMapGeneration || instructions.colorSpace == kTexColorSpaceSRGBXenon)
	{
		workBuffers[currentWorkBuffer]->SetImage (finalWidth, finalHeight, kTexFormatARGBFloat, false);
		workBuffers[currentWorkBuffer]->BlitImage (*curImage);
		SRGB_GammaToLinear (*workBuffers[currentWorkBuffer]);

		SwapWorkBuffers (&curImage, &nextImage, workBuffers, &currentWorkBuffer);
	}

	// Compute mip levels
	for (int mip = 0; mip < mipCount; mip++)
	{
		if (mip < mipCount - 1)
		{
			workBuffers[currentWorkBuffer]->SetImage (std::max (1, curImage->GetWidth () / 2), std::max (1, curImage->GetHeight () / 2), kTexFormatARGBFloat, false);
			ComputeNextMipLevel (*curImage, *nextImage, m_AllSettings.m_MipMapMode == kMipFilterKaiser, m_AllSettings.m_BorderMipMap, dstFormat);
		}

		if (!skipBaseMipProcessing || mip > 0)
		{
			// Apply various filters (normal map compression, RGBM conversion)
			ProcessMipLevel (*curImage, instructions, outputColorSpace);

			// Apply mip map fade
			if (m_AllSettings.m_FadeOut && mip >= m_AllSettings.m_MipMapFadeDistanceStart && mip > 0)
				ApplyMipMapFade (*curImage, mip, m_AllSettings.m_MipMapFadeDistanceStart, m_AllSettings.m_MipMapFadeDistanceEnd, m_AllSettings.m_ConvertToNormalMap, instructions.usageMode==kTexUsageNormalmapDXT5nm);

			BlitImageIntoTextureLevel (mip, texture, &rawImageData, *curImage);
		}

		SwapWorkBuffers (&curImage, &nextImage, workBuffers, &currentWorkBuffer);
	}
}

int TextureImporter::GetTextureFormat () const
{
	// We don't have DXT3 in the dialog anymore. So change that to DXT5 to avoid "nothing is selected"
	// in case the texture was DXT3 originally.
	if( m_AllSettings.m_TextureFormat == kTexFormatDXT3 )
		return kTexFormatDXT5;
	else
		return m_AllSettings.m_TextureFormat;
}

struct EqualTarget
{
	BuildTargetPlatform buildTarget;
	string buildTargetName;

	EqualTarget(BuildTargetPlatform t)
	{
		buildTarget = t;
		buildTargetName = GetBuildTargetName(buildTarget);
	}

	bool operator()(const TextureImporter::BuildTargetSettings& bts)
	{
		if (bts.m_BuildTarget == buildTargetName)
			return true;

		UnityStr group = GetBuildTargetGroupName(buildTarget);
		if (!group.empty() && group == bts.m_BuildTarget)
			return true;

		return false;
	}
};

struct EqualTargetByName
{
	string buildTargetName;

	EqualTargetByName(string t)
	{
		buildTargetName = t;
	}

	bool operator()(const TextureImporter::BuildTargetSettings& bts)
	{
		return bts.m_BuildTarget == buildTargetName;
	}
};

void TextureImporter::GetBestTextureSettings (BuildTargetPlatform platform, int* outTextureSize, int* outTextureFormat, int* outCompressionQuality) const
{
	*outTextureSize			= GetMaxTextureSize();
	*outTextureFormat		= GetTextureFormat();
	*outCompressionQuality	= GetCompressionQuality();

	std::vector<BuildTargetSettings>::const_iterator it =
		find_if(m_BuildTargetSettings.begin(), m_BuildTargetSettings.end(), EqualTarget(platform));

	if (it != m_BuildTargetSettings.end())
	{
		*outTextureSize			= it->m_MaxTextureSize;
		*outTextureFormat		= it->m_TextureFormat;
		*outCompressionQuality	= it->m_CompressionQuality;
	}

	//Cursors need to preserve ARGB
	if (m_TextureType == kCursor)
		*outTextureFormat = kTexFormatARGB32;
}

void TextureImporter::ClearPlatformTextureSettings(const std::string& platform)
{
	erase_if(m_BuildTargetSettings, EqualTargetByName(platform));
	SetDirty();
}

void TextureImporter::SetPlatformTextureSettings(const BuildTargetSettings& settings)
{
	std::vector<BuildTargetSettings>::iterator it =
		find_if(m_BuildTargetSettings.begin(), m_BuildTargetSettings.end(), EqualTargetByName(settings.m_BuildTarget));

	if (it != m_BuildTargetSettings.end())
		*it = settings;
	else
		m_BuildTargetSettings.push_back(settings);

	SetDirty();
}

bool TextureImporter::GetPlatformTextureSettings(const std::string& platform, BuildTargetSettings* settings) const
{
	std::vector<BuildTargetSettings>::const_iterator it =
		find_if(m_BuildTargetSettings.begin(), m_BuildTargetSettings.end(), EqualTargetByName(platform));

	if (it != m_BuildTargetSettings.end())
	{
		*settings = *it;
		return true;
	}
	else
	{
		settings->m_BuildTarget = platform;
		settings->m_MaxTextureSize 		= GetMaxTextureSize();
		settings->m_TextureFormat 		= GetTextureFormat();
		settings->m_CompressionQuality	= GetCompressionQuality();
		return false;
	}
}

///@TODO: Remove auto switch of off m_GrayScaleToAlpha

TextureImporter::TextureImportInstructions TextureImporter::CalculateTargetSpecificTextureSettings (const SourceTextureInformation& sourceInformation, const BuildTargetSelection& targetPlatform, std::string* warnings) const
{
	// Pick the right maximum texture size based on the target platform
	int maxTextureSize, selectedTextureFormat, compressionQuality;
	GetBestTextureSettings(targetPlatform.platform, &maxTextureSize, &selectedTextureFormat, &compressionQuality);

	return CalculateTargetSpecificTextureSettings(sourceInformation, selectedTextureFormat, maxTextureSize, compressionQuality, targetPlatform, warnings);
}

bool TextureImporter::DoesTextureFormatRequireSquareTexture(TextureFormat fmt)
{
	return IsCompressedPVRTCTextureFormat(fmt);
}

bool TextureImporter::IsSupportedTextureFormat(TextureFormat fmt, BuildTargetPlatform plat)
{
	if (IsCompressedDXTTextureFormat(fmt) && !IsBuildTargetDXT(plat))
		return false;

	if (IsCompressedFlashATFTextureFormat(fmt) && !IsBuildTargetFlashATF(plat))
		return false;

	if (IsCompressedPVRTCTextureFormat(fmt) && !IsBuildTargetPVRTC(plat))
		return false;

	if (IsCompressedETCTextureFormat(fmt) && !IsBuildTargetETC(plat))
		return false;

	// TODO Figure out a sensible way to do this
	//if (IsCompressedETC2TextureFormat(fmt) && !IsBuildTargetETC2(plat))
	//	 return false;
	
	if (IsCompressedATCTextureFormat(fmt) && !IsBuildTargetATC(plat))
		return false;

	if (!IsTextureFormatSupportedOnFlash(fmt) && plat == kBuildFlash)
		return false;

	return true;
}

static TextureFormat EnsureSupportedTextureFormat(TextureFormat fmt, TextureFormat fallback, BuildTargetPlatform plat)
{
	if (IsCompressedDXTTextureFormat(fmt) && !IsBuildTargetDXT(plat))
	{
		Assert(!IsCompressedDXTTextureFormat(fallback));
		fmt = fallback;
	}
	if (IsCompressedFlashATFTextureFormat(fmt) && !IsBuildTargetFlashATF(plat))
	{
		Assert(!IsCompressedFlashATFTextureFormat(fallback));
		fmt = fallback;
	}
	if (IsCompressedPVRTCTextureFormat(fmt) && !IsBuildTargetPVRTC(plat))
	{
		Assert(!IsCompressedPVRTCTextureFormat(fallback));
		fmt = fallback;
	}
	if (IsCompressedETCTextureFormat(fmt) && !IsBuildTargetETC(plat))
	{
		Assert(!IsCompressedETCTextureFormat(fallback));
		fmt = fallback;
	}
	/*	TODO Figure out a sensible way to do this
	 if (IsCompressedETC2TextureFormat(fmt) && !IsBuildTargetETC2(plat))
	 {
	 Assert(!IsCompressedETC2TextureFormat(fallback));
	 fmt = fallback;
	 }*/
	if (IsCompressedATCTextureFormat(fmt) && !IsBuildTargetATC(plat))
	{
		Assert(!IsCompressedATCTextureFormat(fallback));
		fmt = fallback;
	}
	if (!IsTextureFormatSupportedOnFlash(fmt) && plat == kBuildFlash)
	{
		Assert(IsTextureFormatSupportedOnFlash(fallback));
		fmt = fallback;
	}
	return fmt;
}


TextureImporter::TextureImportInstructions TextureImporter::CalculateTargetSpecificTextureSettings (const SourceTextureInformation& sourceInformation, int selectedTextureFormat, int maxTextureSize, int compressionQuality, const BuildTargetSelection& targetSelection, std::string* warnings) const
{
	// Use recommended texture format, unless the user has overridden it
	TextureFormat finalTextureFormat = selectedTextureFormat;
	if (finalTextureFormat == kCompressed || finalTextureFormat == kTruecolor || finalTextureFormat == k16bit)
		finalTextureFormat = SimpleToFullTextureFormat(finalTextureFormat, this, targetSelection);

	// Ensure that the user has not selected a texture format we don't support on the target Platform
	TextureFormat recommendedTextureFormat = SimpleToFullTextureFormat(kCompressed, this, targetSelection);
	BuildTargetPlatform targetPlatform = targetSelection.platform;
	
	finalTextureFormat = EnsureSupportedTextureFormat (finalTextureFormat, recommendedTextureFormat, targetPlatform);

	// check correct alpha usage:
	// lightmaps and normal maps alpha usage depends on platform
	// so make sure that when we switch platforms we reapply formats
	// make sure we keep compressed/uncompressed, to avoid surprises
	// NB: this will change even user-selected format, but at least it will keep quality (as we cant really fix it for now)
	if(	   (m_AllSettings.m_Lightmap || IsNormalMap())
		&& HasAlphaTextureFormat(finalTextureFormat) != HasAlphaTextureFormat(recommendedTextureFormat)
	  )
	{
		if(IsAnyCompressedTextureFormat(finalTextureFormat))
			finalTextureFormat = recommendedTextureFormat;
		else
			finalTextureFormat = SimpleToFullTextureFormat(kTruecolor, this, targetSelection);
	}


	if (targetPlatform == kBuild_Android && IsAnyCompressedTextureFormat(finalTextureFormat))
	{
		AndroidBuildSubtarget currentSubtarget = GetEditorUserBuildSettings().GetSelectedAndroidBuildSubtarget();

		switch (currentSubtarget)
		{
			default:
			case kAndroidBuildSubtarget_Generic:
				// do nothing
				break;
			case kAndroidBuildSubtarget_DXT:
				finalTextureFormat = HasAlphaTextureFormat(finalTextureFormat) ? kTexFormatDXT5 : kTexFormatDXT1;
				break;
			case kAndroidBuildSubtarget_PVRTC:
				finalTextureFormat = HasAlphaTextureFormat(finalTextureFormat) ? kTexFormatPVRTC_RGBA4 : kTexFormatPVRTC_RGB4;
				break;
			case kAndroidBuildSubtarget_ATC:
				finalTextureFormat = HasAlphaTextureFormat(finalTextureFormat) ? kTexFormatATC_RGBA8 : kTexFormatATC_RGB4;
				break;
			case kAndroidBuildSubtarget_ETC:
				finalTextureFormat = HasAlphaTextureFormat(finalTextureFormat) ? kTexFormatRGBA4444 : kTexFormatETC_RGB4;
				break;
			case kAndroidBuildSubtarget_ETC2:
				finalTextureFormat = HasAlphaTextureFormat(finalTextureFormat) ? kTexFormatETC2_RGBA8 : kTexFormatETC2_RGB;
				break;
			case kAndroidBuildSubtarget_ASTC:
				finalTextureFormat = HasAlphaTextureFormat(finalTextureFormat) ? kTexFormatASTC_RGBA_6x6 : kTexFormatASTC_RGB_6x6;
				break;
		}
	}

	if (targetPlatform == kBuildBB10 && IsAnyCompressedTextureFormat(finalTextureFormat))
	{
		BlackBerryBuildSubtarget currentSubtarget = GetEditorUserBuildSettings().GetSelectedBlackBerryBuildSubtarget();

		switch(currentSubtarget)
		{
			default:
			case kBlackBerryBuildSubtarget_Generic:
				// do nothing
				break;
			case kBlackBerryBuildSubtarget_PVRTC:
				finalTextureFormat = HasAlphaTextureFormat(finalTextureFormat) ? kTexFormatPVRTC_RGBA4 : kTexFormatPVRTC_RGB4;
				break;
			case kBlackBerryBuildSubtarget_ATC:
				finalTextureFormat = HasAlphaTextureFormat(finalTextureFormat) ? kTexFormatATC_RGBA8 : kTexFormatATC_RGB4;
				break;
			case kBlackBerryBuildSubtarget_ETC:
				finalTextureFormat = HasAlphaTextureFormat(finalTextureFormat) ? kTexFormatRGBA4444 : kTexFormatETC_RGB4;
				break;
		}
	}

	if (targetPlatform == kBuildTizen && IsAnyCompressedTextureFormat(finalTextureFormat))
	{
		finalTextureFormat = HasAlphaTextureFormat(finalTextureFormat) ? kTexFormatRGBA4444 : kTexFormatETC_RGB4;
	}

	// We do not support ARGB4444 on iOS/Android/GLES, we simply switch it to RGBA4444
	if (finalTextureFormat == kTexFormatARGB4444 && (targetPlatform == kBuild_Android || targetPlatform == kBuild_iPhone || targetPlatform == kBuildWinGLESEmu || targetPlatform == kBuildBB10 || targetPlatform == kBuildTizen))
		finalTextureFormat = kTexFormatRGBA4444;

	int finalWidth = sourceInformation.width;
	int finalHeight = sourceInformation.height;

	// Scale NPOT?
	if( !IsPowerOfTwo(finalWidth) || !IsPowerOfTwo(finalHeight) )
	{
		switch(m_AllSettings.m_NPOTScale )
		{
			case kNPOTKeep:
				// nothing
				break;
			case kNPOTScaleAuto:
				finalWidth = ClosestPowerOfTwo(finalWidth);
				finalHeight = ClosestPowerOfTwo(finalHeight);
				break;
			case kNPOTScaleUp:
				finalWidth = NextPowerOfTwo(finalWidth);
				finalHeight = NextPowerOfTwo(finalHeight);
				break;
			case kNPOTScaleDown:
				finalWidth = std::max<unsigned long>( NextPowerOfTwo(finalWidth) / 2, 1UL );
				finalHeight = std::max<unsigned long>( NextPowerOfTwo(finalHeight) / 2, 1UL );
				break;
			default:
				AssertString( "Unknown NPOT scale mode" );
				break;
		}
	}

	// Flash has a maximum texture size of 2048, fails to upload textures otherwise.
	if (targetPlatform == kBuildFlash)
		maxTextureSize = min(maxTextureSize, 2048);

	// Constrain to max texture size
	if (finalWidth > maxTextureSize && finalWidth > finalHeight)
	{
		float widthOverMaxSize = float(finalWidth) / maxTextureSize;
		finalWidth = maxTextureSize;
		finalHeight = finalHeight / widthOverMaxSize;
	}
	else if (finalHeight > maxTextureSize)
	{
		float heightOverMaxSize = float(finalHeight) / maxTextureSize;
		finalWidth = finalWidth / heightOverMaxSize;
		finalHeight = maxTextureSize;
	}

	// OpenGL 2D textures need to be at least 2 pixels high and wide
	finalWidth = std::max (2, finalWidth);
	finalHeight = std::max (2, finalHeight);

	// If our texture is smaller than 4 pixels wide we need to use an uncompressed format
	int minCompressedSize = GetMinimumTextureMipSizeForFormat(finalTextureFormat);

	bool forceUncompressedFormat = false;
	if ( finalWidth < minCompressedSize || finalHeight < minCompressedSize )
		forceUncompressedFormat = true;

#if ENABLE_SPRITES
	if (GetQualifiesForSpritePacking())
		forceUncompressedFormat = true;
#endif

	bool texturePOT 	 = IsPowerOfTwo(finalWidth) && IsPowerOfTwo(finalHeight);
	bool textureHaveMips = m_AllSettings.m_EnableMipMap;

	int allowedMultiple = GetTextureSizeAllowedMultiple(finalTextureFormat);

	bool texFormatRequiresPOT =		IsCompressedPVRTCTextureFormat(finalTextureFormat)
								|| 	IsCompressedETCTextureFormat(finalTextureFormat)
								||	IsCompressedFlashATFTextureFormat(finalTextureFormat);


	if (warnings)
		*warnings = "";
	
	if(IsAnyCompressedTextureFormat(finalTextureFormat) && !texturePOT && textureHaveMips)
	{
		if (warnings)
			*warnings = "Only POT texture can be compressed if mip-maps are enabled";
		forceUncompressedFormat = true;
	}

	if((finalWidth % allowedMultiple) != 0 || (finalHeight % allowedMultiple) != 0)
	{
		if (warnings)
			*warnings = Format("Only textures with width/height being multiple of %d can be compressed to %s format",
			                    allowedMultiple, GetCompressionTypeString(finalTextureFormat)
			                  );
		forceUncompressedFormat = true;
	}

	if(texFormatRequiresPOT && !texturePOT)
	{
		if (warnings)
			*warnings = Format("Only POT textures can be compressed to %s format", GetCompressionTypeString(finalTextureFormat));
		forceUncompressedFormat = true;
	}

	static const bool shouldLogImportWarningsFromInspector = false;
	if(shouldLogImportWarningsFromInspector && warnings && warnings->length())
		const_cast<TextureImporter*>(this)->LogImportWarning(warnings->c_str()); //TODO:This function is ridiculous and needs a proper refactor.

	// PVRTC requires square textures
	// NOTE: this is not quite true, but apple drivers enforce it, and lets play safe with android zoo
	// WARNING: do not try tweaking for pvr if we already force uncompressed format.
	if( forceUncompressedFormat == false && DoesTextureFormatRequireSquareTexture(finalTextureFormat) && (finalWidth != finalHeight) )
	{
		// check if we want uncompressed or square compressed by comparing memory size
		TextureFormat uncompressedTextureFormat = HasAlphaTextureFormat (finalTextureFormat)? kTexFormatARGB32: kTexFormatRGB24;

		const int nonSquareWidth = finalWidth;
		const int nonSquareHeight = finalHeight;
		const int uncompressedImageSize = CalculateImageSize (nonSquareWidth, nonSquareHeight, uncompressedTextureFormat);

		// make texture square
		switch(m_AllSettings.m_NPOTScale)
		{
			case kNPOTScaleAuto:
			case kNPOTScaleUp:
			default:
				finalWidth = std::max (nonSquareWidth, nonSquareHeight);
				finalHeight = finalWidth;
				break;
			case kNPOTScaleDown:
				finalWidth = std::min (nonSquareWidth, nonSquareHeight);
				finalHeight = finalWidth;
				break;
		}

		const int compressedImageSize = CalculateImageSize (finalWidth, finalHeight, finalTextureFormat);
		if (uncompressedImageSize < compressedImageSize)
		{
			// Don't show warning when builtin builtin resources. We want no warnings / errors when building those.
			bool showWarning = !HasARGV("buildBuiltinUnityResources");
			if (showWarning && warnings)
			{
				const_cast<TextureImporter*>(this)->LogImportWarning(Format("Only square textures can be compressed to %s format", GetCompressionTypeString(finalTextureFormat))); //TODO:This function is ridiculous and needs a proper refactor.
			}

			finalWidth = nonSquareWidth;
			finalHeight = nonSquareHeight;
			forceUncompressedFormat = true;
		}
	}

	// In all cases we force uncompressed format only if texture dimensions are wrong. For Sprite atlasing it is useful to know the desired format.
	const TextureFormat desiredTextureFormat = finalTextureFormat;

	if (forceUncompressedFormat)
	{
		if (HasAlphaTextureFormat(finalTextureFormat))
			finalTextureFormat = kTexFormatARGB32;
		else
			finalTextureFormat = kTexFormatRGB24;
	}

	int uncompressedTextureFormat = finalTextureFormat;

	if (IsAnyCompressedTextureFormat(finalTextureFormat))
	{
		if (HasAlphaTextureFormat(finalTextureFormat))
			uncompressedTextureFormat = kTexFormatARGB32;
		else
			uncompressedTextureFormat = kTexFormatRGB24;
	}


	TextureImportInstructions instructions;
	instructions.width = finalWidth;
	instructions.height = finalHeight;
	instructions.compressedFormat = finalTextureFormat;
	instructions.recommendedFormat = recommendedTextureFormat;
	instructions.uncompressedFormat = uncompressedTextureFormat;
	instructions.desiredFormat = desiredTextureFormat;
	instructions.usageMode = GetUsageModeForPlatform(targetPlatform);
	instructions.colorSpace = GetColorSpaceForPlatform(targetPlatform);
	instructions.compressionQuality = compressionQuality;

	return instructions;
}

TextureUsageMode TextureImporter::GetUsageModeForPlatform (BuildTargetPlatform targetPlatform) const
{
	if (m_AllSettings.m_Lightmap)
		return DoesTargetPlatformSupportRGBM(targetPlatform) ? kTexUsageLightmapRGBM : kTexUsageLightmapDoubleLDR;
	else if (IsNormalMap())
		return DoesTargetPlatformUseDXT5nm(targetPlatform) ? kTexUsageNormalmapDXT5nm : kTexUsageNormalmapPlain;
	else
		return kTexUsageNone;
}


TextureColorSpace TextureImporter::GetColorSpaceForPlatform (BuildTargetPlatform targetPlatform) const
{
	// NOTE: overwrite some textures to always be linear, we should grey out the property in the GUI as well
	if (m_AllSettings.m_LinearTexture || m_AllSettings.m_ConvertToNormalMap || m_AllSettings.m_NormalMap || m_AllSettings.m_Lightmap)
	{
		return kTexColorSpaceLinear;
	}
	else
	{
		// Only do xenon preprocessing if the desired colorspace in linear...
		if (targetPlatform == kBuildXBOX360 && GetPlayerSettings().GetDesiredColorSpace () == kLinearColorSpace)
			return kTexColorSpaceSRGBXenon;
		else
			return kTexColorSpaceSRGB;
	}
}

#if ENABLE_SPRITES
void TextureImporter::GenerateSprites(Texture2D& texture, std::string textureName, TextureImporter& importer)
{
	if (!importer.GetAreSpritesGenerated()) //BUGFIX:566198
		return;

	TextureImporter::SourceTextureInformation sti = importer.GetSourceTextureInformation();
	const bool isPOT = IsPowerOfTwo(sti.width) && IsPowerOfTwo(sti.height);
	if ((m_AllSettings.m_NPOTScale != kNPOTKeep) && !isPOT)
	{
		LogImportWarning("Sprites can not be generated from textures with NPOT scaling.");
		return;
	}

	TextureImportInstructions tti = importer.GetTextureImportInstructions(GetEditorUserBuildSettings().GetActiveBuildTargetSelection().platform);
	if (DoesTextureFormatRequireSquareTexture(tti.desiredFormat) && (sti.width != sti.height))
	{
		LogImportWarning(Format("Sprites can not be generated for non-square textures in %s format.", GetTextureFormatString(tti.desiredFormat)));
		return;
	}

	m_SpriteSheet.GenerateAssetData(texture, textureName, importer);
}
#endif

void TextureImporter::GenerateAssetData ()
{
	try
	{
		UnloadAllLoadedAssetsAtPathAndDeleteMetaData(GetMetaDataPath(), this);

		std::auto_ptr<FreeImageWrapper> freeTypeImage;

		if( !LoadImageAtPath (GetAssetPathName(), freeTypeImage, false) )
			throw std::string("File could not be read");
		const ImageReference& image = freeTypeImage->GetImage();

		m_Output.sourceTextureInformation.width = image.GetWidth();
		m_Output.sourceTextureInformation.height = image.GetHeight();

		BuildTargetSelection target = GetEditorUserBuildSettings().GetActiveBuildTargetSelection();

		// Figure out the source texture contains alpha state.
		m_Output.sourceTextureInformation.doesTextureContainAlpha = DoesTextureContainAlpha (image, GetUsageModeForPlatform(target.platform));

		// Figure out if the source has different RGB components, if they're the same we can use illumination format
		// As this is used only on Wii, return true by default on all other platforms, so texture importing wouldnt slow down there
		m_Output.sourceTextureInformation.doesTextureContainColor = target.platform == kBuildWii ? DoesTextureContainColor (image) : true;

		// Preproces the imported texture
		MonoPreprocessTexture (GetAssetPathName());

		// Calculate the final texture formats / settings we need to convert the texture to
		TextureImportInstructions textureImportInstructions = CalculateTargetSpecificTextureSettings (m_Output.sourceTextureInformation, target, &m_ImportInspectorWarnings);
		m_Output.textureImportInstructions = textureImportInstructions;

		// Create 2D Texture2D
		Texture2D& texture = ProduceAssetObject<Texture2D> ();
		texture.AwakeFromLoad(kDefaultAwakeFromLoad);

		texture.SetAlphaIsTransparency(m_AllSettings.m_AlphaIsTransparency);

		int textureOptions = m_AllSettings.m_EnableMipMap ? Texture2D::kMipmapMask : Texture2D::kNoMipmap;
		texture.InitTexture (textureImportInstructions.width, textureImportInstructions.height, textureImportInstructions.uncompressedFormat, textureOptions, 1);

		ApplyTextureSettings (&texture.GetSettings());

		// Apply all filtering, postprocessing, mipmapping, normal mapping, compression
		ProcessImportedImage (&texture, const_cast<ImageReference&>(image), textureImportInstructions, 0);

		//@TODO: THis can be cleaned up here and should reduce the maximum memory usage
		/////		freeTypeImage

		// Postprocess the imported texture
		MonoPostprocessTexture (texture, GetAssetPathName());

		std::string textureName = DeletePathNameExtension (GetLastPathNameComponent (GetAssetPathName ()));

#if ENABLE_SPRITES
		GenerateSprites(texture, textureName, *this);
#endif

		// Generate cubemap from source image
		if (m_AllSettings.m_GenerateCubemap != kGenerateNoCubemap && IsPowerOfTwo(textureImportInstructions.width) && IsPowerOfTwo(textureImportInstructions.height))
		{
			// Create the cubemap
			Cubemap& cubemap = ProduceAssetObject<Cubemap> ("generatedCubemap");
			cubemap.AwakeFromLoad(kDefaultAwakeFromLoad);


			TextureImportInstructions cubemapInstructions = textureImportInstructions;
			///@TODO:
			// * Don't generate a texture2D on top of the cubemap (stupid)
			// * move the size calculation hack into CalculateTargetSpecificTextureSettings?
			// * make comparison for does need reimport compare against cubemap and results of texture instructions
			int cubeFaceSize;
			int cubemapLayout;
			if (m_AllSettings.m_GenerateCubemap == kGenerateFullCubemap)
			{
				cubemapLayout = 0;

				size_t longAxis = image.GetWidth ();
				size_t shortAxis = image.GetHeight ();
				if (longAxis < shortAxis)
				{
					std::swap (longAxis, shortAxis);
					cubemapLayout |= kLayoutOrientationMask; // set vertical orientation bit
				}

				const float AspectEpsilon = 0.1f;
				const float aspect = (float)longAxis / (float)shortAxis;
				if (CompareApproximately(aspect, 6.0f, AspectEpsilon))
				{
					cubeFaceSize = (std::max) (longAxis / 6, shortAxis);
					cubemapLayout |= kLayoutFaces;
				}
				else if (CompareApproximately(aspect, 4.0f/3.0f, AspectEpsilon))
				{
					cubeFaceSize = (std::max) (longAxis / 4, shortAxis / 3);
					cubemapLayout |= kLayoutCross;
				}
				else
				{
					LogImportWarning("Texture does not match aspect ratio of standard cubemap layouts: cross (4:3, 3:4) or sequence (6:1, 1:6)");
					cubeFaceSize = shortAxis;
					cubemapLayout = kLayoutSingleImage;
				}

				SourceTextureInformation sourceFaceInformation = m_Output.sourceTextureInformation;
				sourceFaceInformation.width = sourceFaceInformation.height = ClosestPowerOfTwo (cubeFaceSize);
				cubemapInstructions = CalculateTargetSpecificTextureSettings (sourceFaceInformation, target, &m_ImportInspectorWarnings);
			}
			else
			{
				cubemapLayout = kLayoutSingleImage;
				cubemapInstructions.width = cubemapInstructions.height = (std::max) (textureImportInstructions.width, textureImportInstructions.height);
				// A face will typically be less than 1/4 of the source image so making the size be 1/2 initially should be plenty.
				cubeFaceSize = std::max (image.GetWidth () / 2, image.GetHeight () / 2);
				// 4x AA should be enough and we want to clamp at 4096 to avoid high propability of getting out of memory errors.
				cubeFaceSize = std::min (cubeFaceSize, std::min (cubemapInstructions.width * 4, 4096));
				// Final scale down will be best if scaling factor is power of two, which implies cubeFaceSize also being power of two.
				cubeFaceSize = ClosestPowerOfTwo (cubeFaceSize);
			}

			textureOptions = m_AllSettings.m_EnableMipMap ? Texture2D::kMipmapMask : Texture2D::kNoMipmap;
			cubemap.InitTexture (cubemapInstructions.width, cubemapInstructions.height, cubemapInstructions.uncompressedFormat, textureOptions, 6);

			// @TODO: Change code to only handle one face at a time to reduce memory usage to a sixth!
			for (int i=0;i<6;i++)
			{
				Image cubemapFace = ImageReference (cubeFaceSize, cubeFaceSize, kTexFormatARGB32);
				GenerateCubemapFaceFromImage (image, cubemapFace,
											  static_cast<CubemapGenerationMode>(m_AllSettings.m_GenerateCubemap), static_cast<CubemapLayoutMode>(cubemapLayout), i);
				// Apply all filtering, postprocessing, mipmapping, normal mapping, compression
				ProcessImportedImage (&cubemap, cubemapFace, cubemapInstructions, i);
			}

			///@TODO: SUPPORT CUBEMAP COMPRESSION
			//ReformatTexture (texture, inalTextureFormat);
			cubemap.UpdateImageDataDontTouchMipmap ();

			if (m_AllSettings.m_SeamlessCubemap)
				cubemap.FixupEdges ();

			texture.SetNameCpp(textureName);
		}

		// Setup thumbnail
		Image thumbnail;
		GenerateThumbnailFromTexture (thumbnail, texture, IsAlphaOnlyTextureFormat(textureImportInstructions.uncompressedFormat), IsNormalMap() && textureImportInstructions.usageMode==kTexUsageNormalmapDXT5nm, m_AllSettings.m_AlphaIsTransparency);
		SetThumbnail (thumbnail, texture.GetInstanceID ());

		if (textureImportInstructions.compressedFormat != textureImportInstructions.uncompressedFormat)
		{
			// Compress the texture
			if (GetAssetImportState().GetCompressAssetsPreference())
				CompressTexture (texture, textureImportInstructions.compressedFormat, textureImportInstructions.compressionQuality);
			else
			{
				m_Output.textureImportInstructions.compressedFormat = textureImportInstructions.uncompressedFormat;
				GetAssetImportState().SetDidImportTextureUncompressed();
			}
		}

		// We have imported a texture for the given target platform.
		// Makes sure that if we imported for mixed platforms, we will check again before making a build.
		GetAssetImportState().SetDidImportAssetForTarget(target);

		texture.UpdateImageDataDontTouchMipmap();

		bool textureIsNotReadable = IsCompressedFlashATFTextureFormat(textureImportInstructions.compressedFormat);

		if (textureIsNotReadable)
			texture.SetIsReadable(false);
		else
			texture.SetIsReadable(m_AllSettings.m_IsReadable);
	}
	catch (const std::string& alert)
	{
		LogImportError ("Could not create texture from " + GetAssetPathName() + ": " + alert);
	}
}

int TextureImporter::FullToSimpleTextureFormat (TextureFormat textureFormat)
{
	switch (textureFormat)
	{
		// Never been assigned
		case k16bit:
			return k16bit;
		case kTruecolor:
			return kTruecolor;
		case kCompressed:
			return kCompressed;
		// Not really happy about this one
		case kTexFormatAlpha8:
			return kTruecolor;
		case kTexFormatARGB4444:
		case kTexFormatRGBA4444:
		case kTexFormatRGB565:
			return k16bit;
		case kTexFormatRGB24:
		case kTexFormatRGBA32:
		case kTexFormatARGB32:
		case kTexFormatBGR24:
			return kTruecolor;
		// Not really happy about this one
		case kTexFormatARGBFloat:
			return kTruecolor;
		// Not really happy about this one
		case kTexFormatAlphaLum16:
			return kTruecolor;
		case kTexFormatDXT1:
		case kTexFormatDXT3:
		case kTexFormatDXT5:
			return kCompressed;
		case kTexFormatPVRTC_RGB2:
		case kTexFormatPVRTC_RGBA2:
		case kTexFormatPVRTC_RGB4:
		case kTexFormatPVRTC_RGBA4:
			return kCompressed;
		case kTexFormatETC_RGB4:
			return kCompressed;
		case kTexFormatATC_RGB4:
		case kTexFormatATC_RGBA8:
			return kCompressed;
		case kTexFormatFlashATF_RGB_DXT1:
		case kTexFormatFlashATF_RGB_JPG:
		case kTexFormatFlashATF_RGBA_JPG:
			return kCompressed;
		case kTexFormatEAC_R:
		case kTexFormatEAC_R_SIGNED:
		case kTexFormatEAC_RG:
		case kTexFormatEAC_RG_SIGNED:
			return kCompressed;
		case kTexFormatETC2_RGB:
		case kTexFormatETC2_RGBA1:
		case kTexFormatETC2_RGBA8:
			return kCompressed;
		case kTexFormatASTC_RGB_4x4 : 
		case kTexFormatASTC_RGB_5x5  :
		case kTexFormatASTC_RGB_6x6  :
		case kTexFormatASTC_RGB_8x8  :
		case kTexFormatASTC_RGB_10x10 :
		case kTexFormatASTC_RGB_12x12 :
		case kTexFormatASTC_RGBA_4x4:
		case kTexFormatASTC_RGBA_5x5:
		case kTexFormatASTC_RGBA_6x6:
		case kTexFormatASTC_RGBA_8x8:
		case kTexFormatASTC_RGBA_10x10:
		case kTexFormatASTC_RGBA_12x12:
			return kCompressed;

	}
	ErrorString (Format ("Unable to match textureformat to simple format %d", (int)textureFormat));
	return kTruecolor;
}

#define TF(x) const TextureFormat x[kTextureFormatCount] =

enum { kTextureFormatCount = 6 };

// Each line is CompressedRGB, 16bitRGB, truecolorRGB, CompressedRGBA, 16bitRGBA, truecolorRGBA
TF(kDefault)    { kTexFormatDXT1,             kTexFormatRGB565, kTexFormatRGB24, kTexFormatDXT5,              kTexFormatARGB4444, kTexFormatARGB32};

// iPhone (PVRTC 4 bit compression, prefer RGBA over reversed ARGB)
TF(kPhone)      { kTexFormatPVRTC_RGB4,       kTexFormatRGB565, kTexFormatRGB24, kTexFormatPVRTC_RGBA4,       kTexFormatRGBA4444, kTexFormatRGBA32};

// Android -> (ETC & Generic  ... DXT .... PVRTC  ....  ATC
TF(kAndroidETC) { kTexFormatETC_RGB4,         kTexFormatRGB565, kTexFormatRGB24, kTexFormatRGBA4444,          kTexFormatRGBA4444, kTexFormatRGBA32};
TF(kAndroidETC2) { kTexFormatETC2_RGB,        kTexFormatRGB565, kTexFormatRGB24, kTexFormatETC2_RGBA8,        kTexFormatRGBA4444, kTexFormatRGBA32};
TF(kAndroidASTC) { kTexFormatASTC_RGB_6x6,    kTexFormatRGB565, kTexFormatRGB24, kTexFormatASTC_RGBA_6x6,	  kTexFormatRGBA4444, kTexFormatRGBA32};
TF(kAndroidDXT) { kTexFormatDXT1,             kTexFormatRGB565, kTexFormatRGB24, kTexFormatDXT5,              kTexFormatRGBA4444, kTexFormatRGBA32};
TF(kAndroidPVR) { kTexFormatPVRTC_RGB4,       kTexFormatRGB565, kTexFormatRGB24, kTexFormatPVRTC_RGBA4,       kTexFormatRGBA4444, kTexFormatRGBA32};
TF(kAndroidATC) { kTexFormatATC_RGB4,         kTexFormatRGB565, kTexFormatRGB24, kTexFormatATC_RGBA8,         kTexFormatRGBA4444, kTexFormatRGBA32};

// BB10 -> (ETC & Generic  .... PVRTC  ....  ATC
TF(kBB10ETC)    { kTexFormatETC_RGB4,         kTexFormatRGB565, kTexFormatRGB24, kTexFormatRGBA4444,          kTexFormatRGBA4444, kTexFormatRGBA32};
TF(kBB10PVR)    { kTexFormatPVRTC_RGB4,       kTexFormatRGB565, kTexFormatRGB24, kTexFormatPVRTC_RGBA4,       kTexFormatRGBA4444, kTexFormatRGBA32};
TF(kBB10ATC)    { kTexFormatATC_RGB4,         kTexFormatRGB565, kTexFormatRGB24, kTexFormatATC_RGBA8,         kTexFormatRGBA4444, kTexFormatRGBA32};

// Tizen -> (ETC & Generic
TF(kTizenETC)    { kTexFormatETC_RGB4,         kTexFormatRGB565, kTexFormatRGB24, kTexFormatRGBA4444,          kTexFormatRGBA4444, kTexFormatRGBA32};

// Flash
TF(kFlash)      { kTexFormatFlashATF_RGB_JPG, kTexFormatRGB24,  kTexFormatRGB24, kTexFormatFlashATF_RGBA_JPG, kTexFormatRGBA32,   kTexFormatRGBA32};

// WebGL
TF(kWebGL)	    { kTexFormatRGB24,            kTexFormatRGB565, kTexFormatRGB24, kTexFormatARGB32,            kTexFormatARGB4444, kTexFormatARGB32};


const TextureFormat* GetTexturePlatformFormats (BuildTargetSelection selection)
{
	BuildTargetPlatform destinationPlatform = selection.platform;

	if (destinationPlatform == kBuild_iPhone)
		return kPhone;
	else if (destinationPlatform == kBuild_Android)
	{
		switch (selection.subTarget)
		{
			case kAndroidBuildSubtarget_DXT:   return kAndroidDXT;
			case kAndroidBuildSubtarget_PVRTC: return kAndroidPVR;
			case kAndroidBuildSubtarget_ATC:   return kAndroidATC;
			case kAndroidBuildSubtarget_ETC2:  return kAndroidETC2;
			case kAndroidBuildSubtarget_ASTC:  return kAndroidASTC;
			default:                           return kAndroidETC;
		}
	}
	else if (destinationPlatform == kBuildBB10)
	{
		switch (selection.subTarget)
		{
			case kBlackBerryBuildSubtarget_PVRTC: return kBB10PVR;
			case kBlackBerryBuildSubtarget_ATC:   return kBB10ATC;
			default:                        return kBB10ETC;
		}
	}
	else if (destinationPlatform == kBuildTizen)
		return kTizenETC;
	else if (destinationPlatform == kBuildFlash)
		return kFlash;
	else if (destinationPlatform == kBuildWebGL)
		return kWebGL;
	else
		return kDefault;
}

TextureFormat TextureImporter::SimpleToFullTextureFormat (int simpleFormat, TextureType tType, const Settings& settings, bool doesTextureContainAlpha, bool doesTextureContainColor, const BuildTargetSelection& destinationPlatform)
{
	// If we already have a full format, we just return that
	if (simpleFormat >= 0)
		return simpleFormat;

	// flip simpleformat to idx into above array
	simpleFormat = -1 - simpleFormat;

	if (tType == TextureImporter::kCookie)
		return kTexFormatAlpha8;

	// Both doubleLDR and RGBM lightmaps currently need alpha
	bool isLightmapThatNeedsAlpha  = settings.m_Lightmap && DoesTargetPlatformSupportRGBM(destinationPlatform.platform);
	bool isNormalmapThatNeedsAlpha =	(tType == TextureImporter::kNormal || IsNormalMap(settings))
									 &&	DoesTargetPlatformUseDXT5nm(destinationPlatform.platform);

	bool needAlpha =	doesTextureContainAlpha || settings.m_GrayScaleToAlpha
					 || isNormalmapThatNeedsAlpha || isLightmapThatNeedsAlpha;


	const TextureFormat* formats = GetTexturePlatformFormats(destinationPlatform);

	return formats[simpleFormat + (needAlpha ? 3 : 0)];
}

TextureFormat TextureImporter::SimpleToFullTextureFormat (int simpleFormat, const TextureImporter *importer, const BuildTargetSelection& destinationPlatform)
{
	return SimpleToFullTextureFormat (	simpleFormat, importer->GetTextureType(), importer->GetSettings(),
										importer->GetSourceTextureInformation().doesTextureContainAlpha,
										importer->GetSourceTextureInformation().doesTextureContainColor,
										destinationPlatform);
}



static void OverrideTextureSettings (const TextureSettings& source, TextureSettings *target)
{
	if (source.m_FilterMode != -1)
		target->m_FilterMode = source.m_FilterMode;
	if (source.m_Aniso != -1)
		target->m_Aniso = source.m_Aniso;
	if (source.m_MipBias != -1)
		target->m_MipBias = source.m_MipBias;
	if (source.m_WrapMode != -1)
		target->m_WrapMode = source.m_WrapMode;
}

void TextureImporter::ApplyTextureSettings (TextureSettings *target)
{
	target->m_WrapMode = 0;		// Set up for repeat.
	target->m_Aniso = 1;		// No aniso by default.
	target->m_MipBias = 0.0f;		// No mipmap bias by default.
	target->m_FilterMode = (m_AllSettings.m_FadeOut || m_AllSettings.m_ConvertToNormalMap) ? kTexFilterTrilinear : kTexFilterBilinear;
	// override any user settings.
	OverrideTextureSettings (m_AllSettings.m_TextureSettings, target);
}

bool TextureImporter::ValidateAllowUploadToCacheServer()
{
	if (DoesTextureStillNeedToBeCompressed (GetAssetPathName()))
		return false;

	return Super::ValidateAllowUploadToCacheServer();
}

bool DoesTextureStillNeedToBeCompressed (const string& assetPath)
{
	return TextureImporter::DoesAssetNeedReimport(assetPath, GetEditorUserBuildSettings().GetActiveBuildTargetSelection(), false);
}

bool TextureImporter::DoesAssetNeedReimport (const string& assetPath, const BuildTargetSelection& targetPlatform, bool unload)
{
	if (!CanLoadPathName(assetPath))
		return false;

	string metaDataPath = GetMetaDataPathFromAssetPath (assetPath);

	TextureImporter* importer = dynamic_pptr_cast<TextureImporter*> (FindAssetImporterAtPath (metaDataPath));
	bool requireReimport = false;

	if (importer)
	{
		TextureImportInstructions textureImportInstructions = importer->CalculateTargetSpecificTextureSettings (importer->m_Output.sourceTextureInformation, targetPlatform, &importer->m_ImportInspectorWarnings);
		TextureImportInstructions lastInstructions = importer->m_Output.textureImportInstructions;

		if (textureImportInstructions.compressedFormat != lastInstructions.compressedFormat ||
			textureImportInstructions.width != lastInstructions.width ||
			textureImportInstructions.height != lastInstructions.height ||
			textureImportInstructions.usageMode != lastInstructions.usageMode ||
			textureImportInstructions.colorSpace != lastInstructions.colorSpace ||
			textureImportInstructions.compressionQuality != lastInstructions.compressionQuality)
			requireReimport = true;
	}

	if (unload)
	{
		if (importer && !importer->IsPersistentDirty())
			UnloadObject(importer);

		GetPersistentManager().UnloadStream(metaDataPath);
	}

	return requireReimport;
}

TextureImporter::SourceTextureInformation TextureImporter::GetSourceTextureInformation () const
{
	return m_Output.sourceTextureInformation;
}

template<class T>
void TextureImporter::BuildTargetSettings::Transfer (T& transfer)
{
	transfer.Transfer(m_BuildTarget, "m_BuildTarget", kTransferAsArrayEntryNameInMetaFiles);
	TRANSFER(m_MaxTextureSize);
	TRANSFER(m_TextureFormat);
	TRANSFER(m_CompressionQuality);
}


template<class TransferFunction>
void TextureImporter::SourceTextureInformation::Transfer (TransferFunction& transfer)
{
	TRANSFER(width);
	TRANSFER(height);
	TRANSFER(doesTextureContainAlpha);
	TRANSFER(doesTextureContainColor);
}


template<class TransferFunction>
void TextureImporter::TextureImportOutput::Transfer (TransferFunction& transfer)
{
	TRANSFER(textureImportInstructions);
	TRANSFER(sourceTextureInformation);
}

template<class TransferFunction>
void TextureImporter::TextureImportInstructions::Transfer (TransferFunction& transfer)
{
	TRANSFER_ENUM(compressedFormat);
	TRANSFER_ENUM(uncompressedFormat);
	TRANSFER_ENUM(recommendedFormat);
	TRANSFER_ENUM(desiredFormat);
	TRANSFER_ENUM(usageMode);
	TRANSFER_ENUM(colorSpace);
	TRANSFER(width);
	TRANSFER(height);
	TRANSFER(compressionQuality);
}

template<class T>
void TextureImporter::Settings::Transfer (T& transfer)
{
	transfer.BeginMetaGroup ("mipmaps");
	TRANSFER (m_MipMapMode);
	TRANSFER (m_EnableMipMap);
	TRANSFER (m_LinearTexture);
	transfer.Transfer (m_GenerateMipsInLinearSpace, "correctGamma");
	TRANSFER (m_FadeOut);
	TRANSFER (m_BorderMipMap);
	TRANSFER (m_MipMapFadeDistanceStart);
	TRANSFER (m_MipMapFadeDistanceEnd);
	transfer.EndMetaGroup ();

	transfer.BeginMetaGroup ("bumpmap");
	TRANSFER (m_ConvertToNormalMap);
	transfer.Transfer (m_NormalMap, "m_ExternalNormalMap");

	TRANSFER (m_HeightScale);
	TRANSFER (m_NormalMapFilter);
	transfer.EndMetaGroup ();

	TRANSFER (m_IsReadable);
	TRANSFER (m_GrayScaleToAlpha);
	TRANSFER (m_GenerateCubemap);
	TRANSFER (m_SeamlessCubemap);
	TRANSFER (m_TextureFormat);

	TRANSFER (m_MaxTextureSize);
	TRANSFER (m_TextureSettings);
	TRANSFER (m_NPOTScale);
	TRANSFER (m_Lightmap);
	TRANSFER (m_CompressionQuality);

#if ENABLE_SPRITES
    TRANSFER (m_SpriteMode);

	TRANSFER (m_SpriteExtrude);
	TRANSFER (m_SpriteMeshType);
	TRANSFER (m_Alignment);
	TRANSFER (m_SpritePivot);
	TRANSFER (m_SpritePixelsToUnits);
	#if ENABLE_SPRITECOLLIDER
	TRANSFER (m_SpriteColliderAlphaCutoff);
	TRANSFER (m_SpriteColliderDetail);
	#endif
#endif

	TRANSFER (m_AlphaIsTransparency);
}

template<class T>
void TextureImporter::Transfer (T& transfer)
{
	// Changes: m_AutomaticTextureFormat
	// Changes: <3.0
	// Version: 3.0 (23-03-10): Platform BuildTargetSettings

	Super::Transfer (transfer);
	transfer.SetVersion(2);
	m_AllSettings.Transfer (transfer);

	TRANSFER(m_TextureType);

	if (transfer.IsOldVersion(1) && !transfer.AssetMetaDataOnly())
	{
		bool automaticTextureFormatDeprecated;
		transfer.Transfer (automaticTextureFormatDeprecated, "m_AutomaticTextureFormat");
		if (automaticTextureFormatDeprecated)
			m_AllSettings.m_TextureFormat = -1;
	}

	// Build target settings
	transfer.Align();
	TRANSFER(m_BuildTargetSettings);

	// Make sure that import settings type is always enforced
	if (transfer.IsReadingBackwardsCompatible())
		m_AllSettings.ApplyTextureType((TextureImporter::TextureType)m_TextureType, false);

#if ENABLE_SPRITES
	// Sprites
	TRANSFER(m_SpriteSheet);
	TRANSFER(m_SpritePackingTag);
#endif

	/// This is strictly cached data,
	/// optimally it shouldn't be part of the texture importer
	if (!transfer.AssetMetaDataOnly())
	{
		TRANSFER(m_Output);
	}

	PostTransfer (transfer);
}


IMPLEMENT_CLASS_HAS_INIT (TextureImporter)
IMPLEMENT_OBJECT_SERIALIZE (TextureImporter)


TextureImporter::TextureType TextureImporter::GuessTextureTypeFromSettings (const Settings &s, const SourceTextureInformation &sourceInfo)
{
	// These will always send us into advanced - early out on them.
	if (s.m_IsReadable || s.m_FadeOut || s.m_GenerateMipsInLinearSpace)
		return kAdvanced;
	bool hasAlpha = HasAlphaTextureFormat (s.m_TextureFormat);
	// Could this be a normal map?

	if (IsNormalMap(s))
	{
		// check all the other things that are required for this to be a standard normal map
		if (s.m_EnableMipMap && !s.m_BorderMipMap && !s.m_GrayScaleToAlpha && s.m_GenerateCubemap == 0 && hasAlpha && s.m_NPOTScale == kNPOTScaleAuto)
			return kNormal;
		return kAdvanced;
	}

	// GUI or sprite?
	if (!s.m_EnableMipMap)
	{
		if (!s.m_GrayScaleToAlpha && s.m_GenerateCubemap == 0 && s.m_NPOTScale == kNPOTKeep)
		{
			#if ENABLE_SPRITES
			if (!s.m_LinearTexture)
				return kSprite;
			#endif
			return kGUI;
		}
		return kAdvanced;
	}

	// Check for a cookie
	if (IsAlphaOnlyTextureFormat (s.m_TextureFormat)  && s.m_NPOTScale == kNPOTScaleAuto)
	{
		// point light cookie
		if (s.m_GenerateCubemap != 0 && !s.m_BorderMipMap)
			return kCookie;
		// Directional
		if (s.m_TextureSettings.m_WrapMode == 1 && s.m_BorderMipMap)
			return kCookie;
		if (s.m_TextureSettings.m_WrapMode == 0 && !s.m_BorderMipMap)
			return kCookie;

		return kAdvanced;
	}


	// Reflection
	if (s.m_GenerateCubemap != 0 && s.m_NPOTScale == kNPOTScaleAuto)
	{
		if (!s.m_GrayScaleToAlpha && !s.m_BorderMipMap)
		{
			return kReflection;
		}
		return kAdvanced;
	}

	return s.m_NPOTScale == kNPOTScaleAuto ? kImage : kAdvanced;
}

void TextureImporter::FixNormalmap ()
{
	if (GetNormalmap())
			return;
	SetTextureType(kNormal, false);

	// Make sure base texture format is compressed
	SetTextureFormat(kCompressed);

	// Make sure all target overrides are compressed
	for (std::vector<BuildTargetSettings>::iterator i = m_BuildTargetSettings.begin(); i != m_BuildTargetSettings.end(); i++)
		i->m_TextureFormat = kCompressed;

	SetDirty();
}

TextureImporter::Settings::Settings ()
{
	memset (this, 0, sizeof(*this));

	m_EnableMipMap = true;
	m_MipMapMode = kMipFilterBox;
	m_GenerateMipsInLinearSpace = false;
	m_LinearTexture = false; ///@TODO: Verify that this will still default textures to use m_LinearTexture and image format
	m_BorderMipMap = false;
	m_IsReadable = false;
	m_Lightmap = false;

	m_FadeOut = false;
	m_MipMapFadeDistanceStart = 1;
	m_MipMapFadeDistanceEnd = 3;

	m_ConvertToNormalMap = false;
	m_NormalMap = false;
	m_HeightScale = 0.25F;
	m_NormalMapFilter = 0;

	m_GrayScaleToAlpha = false;

	m_GenerateCubemap = 0;
	m_SeamlessCubemap = false;

	m_TextureFormat = -1;
	m_RecommendedTextureFormat = kTexFormatDXT1;

	m_MaxTextureSize = 1024;
	m_NPOTScale = kNPOTScaleAuto;

	m_CompressionQuality = kTexCompressionNormal;

#if ENABLE_SPRITES
	m_SpriteMode = 0;
	m_SpriteExtrude = kSpriteDefaultExtrude;
	m_SpriteMeshType = kSpriteMeshTypeTight;
	m_Alignment = kSA_Center;
	m_SpritePivot = Vector2f(0.5f, 0.5f);
	m_SpritePixelsToUnits = 100.0f;
	#if ENABLE_SPRITECOLLIDER
	m_SpriteColliderAlphaCutoff = kSpriteEditorDefaultAlphaTolerance;
	m_SpriteColliderDetail = kSpriteEditorDefaultDetail;
	#endif
#endif

	m_AlphaIsTransparency = false;
}

bool operator == (const TextureImporter::Settings& lhs, const TextureImporter::Settings& rhs)
{
	int result = memcmp(&lhs, &rhs, sizeof(TextureImporter::Settings));
	return result == 0;
}

TextureImporter::BuildTargetSettings::BuildTargetSettings ()
{
	m_MaxTextureSize = 1024;
	m_TextureFormat = kCompressed;
	m_CompressionQuality = kTexCompressionNormal;
}

#if ENABLE_SPRITES
bool TextureImporter::GetAreSpritesGenerated() const
{
	TextureType tt = GetTextureType();
	bool result = (tt == kSprite || tt == kAdvanced) && (GetSpriteMode() != kSpriteModeNone);
	return result;
}

bool TextureImporter::GetQualifiesForSpritePacking() const
{
	bool canAtlas = GetAreSpritesGenerated() && (!GetSpritePackingTag().empty()) && (GetMipmapEnabled() == false) && (GetIsReadable() == false);
	if (canAtlas)
	{
		// Also prevent atlasing of textures inside Resources folder
		std::vector<std::string> parts;
		Split(ToLower(GetAssetPathName()), "/\\", parts);
		for (std::vector<std::string>::const_iterator it = parts.begin(); it != parts.end(); ++it)
		{
			if (*it == "resources")
			{
				canAtlas = false;
				break;
			}
		}
	}

	return canAtlas;
}

TextureImporter::TextureImportInstructions TextureImporter::GetTextureImportInstructions(BuildTargetPlatform platform)
{
	BuildTargetSelection target(platform, 0);
	return CalculateTargetSpecificTextureSettings(m_Output.sourceTextureInformation, target, NULL);
}
#endif

void TextureImporter::Settings::ApplyTextureType (TextureType textureType, bool applyAll)
{
	switch (textureType)
	{
		case kImage:
			m_Lightmap = m_ConvertToNormalMap = m_NormalMap = false;
			m_GenerateCubemap = kGenerateNoCubemap;
			m_SeamlessCubemap = false;
			m_EnableMipMap = true;
			m_GenerateMipsInLinearSpace = false;
			m_LinearTexture = false;
			m_BorderMipMap = false;
			m_NPOTScale = kNPOTScaleAuto;
			if (applyAll)
			{
				m_GrayScaleToAlpha = false;
				m_AlphaIsTransparency = false;
			}
			break;
		case kNormal:
			m_Lightmap = false;
			m_NormalMap = true;
			if (applyAll)
				m_ConvertToNormalMap = true;
			m_GenerateCubemap = kGenerateNoCubemap;
			m_SeamlessCubemap = false;
			m_EnableMipMap = true;
			m_GenerateMipsInLinearSpace = false;
			m_LinearTexture = true;
			m_BorderMipMap = false;
			m_GrayScaleToAlpha = false;
			m_NPOTScale = kNPOTScaleAuto;
			m_AlphaIsTransparency = false;
			break;
		case kCursor: // Combine with Sprite and fall through to GUI as the rest is the same
		case kSprite:
			if (textureType == kCursor)
			{
				m_IsReadable = true;
			}
			else
			{
				m_IsReadable = false;
				if (m_SpriteMode == kSpriteModeNone)
					m_SpriteMode = kSpriteModeSingle;
			}
		case kGUI:
			m_Lightmap = m_NormalMap = m_ConvertToNormalMap = false;
			m_GenerateCubemap = kGenerateNoCubemap;
			m_SeamlessCubemap = false;
			m_EnableMipMap = false;
			m_TextureSettings.m_Aniso = 1;
			m_GenerateMipsInLinearSpace = false;
			m_LinearTexture = (textureType!=kSprite);
			m_BorderMipMap = false;
			m_TextureSettings.m_WrapMode = kTexWrapClamp;
			m_NPOTScale = kNPOTKeep;
			m_AlphaIsTransparency = true;
			if (applyAll)
				m_GrayScaleToAlpha = false;
			break;
		case kReflection:
			if (applyAll)
			{
				m_GenerateCubemap = kGenerateFullCubemap;
				m_SeamlessCubemap = false;
				m_GrayScaleToAlpha = false;
				m_TextureSettings.m_WrapMode = kTexWrapClamp;
			}
			m_Lightmap = m_ConvertToNormalMap = m_NormalMap = false;
			m_EnableMipMap = true;
			m_GenerateMipsInLinearSpace = false;
			m_LinearTexture = false;
			m_BorderMipMap = false;
			m_NPOTScale = kNPOTScaleAuto;
			m_AlphaIsTransparency = false;
			break;
		case kCookie:
			m_Lightmap = m_ConvertToNormalMap = m_NormalMap = false;
			m_EnableMipMap = true;
			m_LinearTexture = false;
			m_GenerateMipsInLinearSpace = false;
			if (applyAll)
			{
				m_GenerateCubemap = kGenerateNoCubemap;
				m_SeamlessCubemap = false;
				m_BorderMipMap = true;
				m_TextureSettings.m_WrapMode = kTexWrapClamp;
				m_TextureSettings.m_Aniso = 0;
				m_GrayScaleToAlpha = false;
			}
			m_NPOTScale = kNPOTScaleAuto;
			m_AlphaIsTransparency = false;
			break;
		case kLightmap:
			m_Lightmap = true;
			m_ConvertToNormalMap = m_NormalMap = false;
			m_GenerateCubemap = kGenerateNoCubemap;
			m_SeamlessCubemap = false;
			m_EnableMipMap = true;
			m_LinearTexture = true;
			m_GenerateMipsInLinearSpace = false;
			m_BorderMipMap = false;
			m_NPOTScale = kNPOTScaleAuto;
			m_AlphaIsTransparency = false;
			if (applyAll)
			{
				m_GrayScaleToAlpha = false;
				m_TextureSettings.m_WrapMode = kTexWrapClamp;
			}
			break;
		case kAdvanced:
			break;
	}

#if ENABLE_SPRITES
    if (textureType != kAdvanced && textureType != kSprite)
        m_SpriteMode = kSpriteModeNone;
#endif
}
