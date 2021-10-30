#ifndef TEXTUREIMPORTER_H
#define TEXTUREIMPORTER_H

#include "AssetImporter.h"
#include "SpriteFrameMetaData.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Graphics/Texture2D.h"

class Image;
class Texture2D;
class FreeImageWrapper;
struct BuildTargetSelection;
////@TODO: RENAME TEXTURE IMPORTER PROPERTIES TO MATCH ACCESSORS

class TextureImporter : public AssetImporter
{
public:
	struct SourceTextureInformation
	{
		int width;
		int height;
		bool doesTextureContainAlpha;
		bool doesTextureContainColor;

		SourceTextureInformation()
		{
			width = -1;
			height = -1;
			doesTextureContainAlpha = true;
			doesTextureContainColor = true;
		}


		DECLARE_SERIALIZE(SourceTextureInformation)
	};

	struct TextureImportInstructions
	{
		TextureFormat     compressedFormat;
		TextureFormat     uncompressedFormat;
		TextureFormat     recommendedFormat;
		TextureFormat     desiredFormat;
		TextureUsageMode  usageMode;
		TextureColorSpace colorSpace;
		int               width;
		int               height;
		int               compressionQuality;

		DECLARE_SERIALIZE(TextureImportInstructions)
	};

	struct BuildTargetSettings
	{
		UnityStr        m_BuildTarget;
		int           m_MaxTextureSize;
		int           m_TextureFormat;
		int           m_CompressionQuality;

		BuildTargetSettings ();

		DECLARE_SERIALIZE(BuildTargetSettings)
	};

	REGISTER_DERIVED_CLASS (TextureImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (TextureImporter)

	TextureImporter (MemLabelId label, ObjectCreationMode mode);
	// ~TextureImporter (); declared-by-macro
	virtual void Reset ();
	virtual void CheckConsistency();
	virtual void GenerateAssetData ();
	virtual void ClearPreviousImporterOutputs ();

	static void InitializeClass ();
	static void CleanupClass () {}

	TextureSettings &GetTextureSettings () { return m_AllSettings.m_TextureSettings; }
	void ApplyTextureSettings (TextureSettings *target);

	// The chosen Texture format
	int GetTextureFormat () const;
	void SetTextureFormat (int format) { if (m_AllSettings.m_TextureFormat == format) return;  m_AllSettings.m_TextureFormat = format; SetDirty(); }

	void ClearPlatformTextureSettings(const std::string& platform);
	void SetPlatformTextureSettings(const BuildTargetSettings& settings);
	bool GetPlatformTextureSettings(const std::string& platform, BuildTargetSettings* settings) const;

	enum MipFilter { kMipFilterBox = 0, kMipFilterKaiser = 1 };
	enum NPOTScale { kNPOTKeep = 0, kNPOTScaleAuto, kNPOTScaleUp, kNPOTScaleDown };
	enum { kSimpleFilter = 0, kSobelFilter = 1 };
	enum TextureType {
		kImage = 0,
		kNormal = 1,
		kGUI = 2,
		kReflection = 3,
		kCookie = 4,
		kAdvanced = 5,
		kLightmap = 6,
		kCursor = 7,
		kSprite = 8,
	};

	GET_SET_COMPARE_DIRTY (int, MaxTextureSize, m_AllSettings.m_MaxTextureSize);
	GET_SET_COMPARE_DIRTY (bool, MipmapEnabled, m_AllSettings.m_EnableMipMap);
	GET_SET_COMPARE_DIRTY (int, MipmapMode, m_AllSettings.m_MipMapMode);
	GET_SET_COMPARE_DIRTY (bool, GenerateMipsInLinearSpace, m_AllSettings.m_GenerateMipsInLinearSpace);
	GET_SET_COMPARE_DIRTY (bool, BorderMipmap, m_AllSettings.m_BorderMipMap);
	GET_SET_COMPARE_DIRTY (bool, Fadeout, m_AllSettings.m_FadeOut);
	GET_SET_COMPARE_DIRTY (int, MipmapFadeDistanceStart, m_AllSettings.m_MipMapFadeDistanceStart);
	GET_SET_COMPARE_DIRTY (int, MipmapFadeDistanceEnd, m_AllSettings.m_MipMapFadeDistanceEnd);
	GET_SET_COMPARE_DIRTY (bool, GrayscaleToAlpha, m_AllSettings.m_GrayScaleToAlpha);
	GET_SET_COMPARE_DIRTY (int, GenerateCubemap, m_AllSettings.m_GenerateCubemap);
	GET_SET_COMPARE_DIRTY (bool, IsReadable, m_AllSettings.m_IsReadable);
	GET_SET_COMPARE_DIRTY (int, NPOTScale, m_AllSettings.m_NPOTScale);
	GET_SET_COMPARE_DIRTY (bool, Lightmap, m_AllSettings.m_Lightmap);
	GET_SET_COMPARE_DIRTY (bool, ConvertToNormalmap, m_AllSettings.m_ConvertToNormalMap);
	GET_SET_COMPARE_DIRTY (bool, Normalmap, m_AllSettings.m_NormalMap);
	GET_SET_COMPARE_DIRTY (int, NormalmapFilter, m_AllSettings.m_NormalMapFilter);
	GET_SET_COMPARE_DIRTY (float, NormalmapHeightScale, m_AllSettings.m_HeightScale);
	GET_SET_COMPARE_DIRTY (bool, LinearTexture, m_AllSettings.m_LinearTexture);
	GET_SET_COMPARE_DIRTY (int, AnisoLevel, m_AllSettings.m_TextureSettings.m_Aniso);
	GET_SET_COMPARE_DIRTY (int, FilterMode, m_AllSettings.m_TextureSettings.m_FilterMode);
	GET_SET_COMPARE_DIRTY (int, WrapMode, m_AllSettings.m_TextureSettings.m_WrapMode);
	GET_SET_COMPARE_DIRTY (float, MipMapBias, m_AllSettings.m_TextureSettings.m_MipBias);
	GET_SET_COMPARE_DIRTY (int, CompressionQuality, m_AllSettings.m_CompressionQuality);
	GET_SET_COMPARE_DIRTY (int, AlphaIsTransparency, m_AllSettings.m_AlphaIsTransparency);

	TextureType GetTextureType () const;
	void SetTextureType (int type, bool applyAll);

	virtual bool ValidateAllowUploadToCacheServer();

	/// Does the texture need to be reimported for the targetPlatform or is it already in the right format
	static bool DoesAssetNeedReimport (const string& assetPath, const BuildTargetSelection& targetPlatform, bool unload);

	/// Can this importer import this file at path?
	/// @implement this to support reimport assets
	static int CanLoadPathName(const std::string& path, int* q = NULL);

	SourceTextureInformation GetSourceTextureInformation () const;

	struct Settings
	{
		Settings ();

		friend bool operator == (const Settings& lhs, const Settings& rhs);

		void ApplyTextureType (TextureType textureType, bool applyAll);
		template<class T>
		void Transfer (T& transfer);

		int    m_MipMapMode;
		int    m_EnableMipMap; // bool
		int    m_GenerateMipsInLinearSpace; // bool
		int    m_FadeOut; // bool
		int    m_BorderMipMap; // bool
		int    m_MipMapFadeDistanceStart;
		int    m_MipMapFadeDistanceEnd;

		int    m_ConvertToNormalMap; // bool
		int    m_NormalMap; // bool
		float  m_HeightScale;
		int    m_NormalMapFilter;
		int    m_GrayScaleToAlpha; // bool
		int    m_IsReadable; // bool
		int    m_TextureFormat;
		int    m_RecommendedTextureFormat;
		int    m_MaxTextureSize;
		int    m_NPOTScale;
		int    m_Lightmap; // bool
		int    m_LinearTexture; // bool
		int    m_CompressionQuality;

#if ENABLE_SPRITES
		int    m_SpriteMode; // SpriteMode
		UInt32 m_SpriteExtrude;
		int    m_SpriteMeshType; // SpriteMeshType
		int    m_Alignment;
		Vector2f m_SpritePivot; // [0;1] in sprite definition rectangle space
		float  m_SpritePixelsToUnits;
		#if ENABLE_SPRITECOLLIDER
		int    m_SpriteColliderAlphaCutoff; // [0;254]
		float  m_SpriteColliderDetail;
		#endif
#endif

		int    m_GenerateCubemap;
		int    m_SeamlessCubemap; // bool

		int	   m_AlphaIsTransparency; // bool
		TextureSettings m_TextureSettings;
	};
	GET_SET_COMPARE_DIRTY (Settings&, Settings, m_AllSettings);
#if ENABLE_SPRITES
	GET_SET_COMPARE_DIRTY (SpriteSheetMetaData&, SpriteSheetMetaData, m_SpriteSheet);
#endif

    Settings GetSettings () { return m_AllSettings; }

	static void SetPlatformFormat(BuildTargetPlatform destinationPlatform, int simpleFormat, bool needAlpha, TextureFormat nativeFormat);
	static int FullToSimpleTextureFormat (TextureFormat textureFormat);
	static TextureFormat SimpleToFullTextureFormat (int textureFormat, const TextureImporter *importer, const BuildTargetSelection& destinationPlatform);
	static TextureFormat SimpleToFullTextureFormat (int simpleFormat, TextureType tType, const Settings& settings, bool doesTextureContainAlpha, bool doesTextureContainColor, const BuildTargetSelection& destinationPlatform);
	static bool IsSupportedTextureFormat(TextureFormat fmt, BuildTargetPlatform plat);
	static bool DoesTextureFormatRequireSquareTexture(TextureFormat fmt);

	static bool IsNormalMap(Settings s) {return s.m_NormalMap || s.m_ConvertToNormalMap;}
	bool IsNormalMap() const {return IsNormalMap(m_AllSettings);}

	/// Fix normal map & make sure texture format is ok.
	void FixNormalmap ();

#if ENABLE_SPRITES
    enum SpriteMode { kSpriteModeNone = 0, kSpriteModeSingle = 1, kSpriteModeManual = 2 };

	GET_SET_COMPARE_DIRTY (SpriteMode, SpriteMode, m_AllSettings.m_SpriteMode);
	GET_SET_COMPARE_DIRTY (UInt32, SpriteExtrude, m_AllSettings.m_SpriteExtrude);
	GET_SET_COMPARE_DIRTY (SpriteMeshType, SpriteMeshType, m_AllSettings.m_SpriteMeshType);
	GET_SET_COMPARE_DIRTY (std::string, SpritePackingTag, m_SpritePackingTag);
	GET_SET_COMPARE_DIRTY (float, SpritePixelsToUnits, m_AllSettings.m_SpritePixelsToUnits);
	GET_SET_COMPARE_DIRTY (Vector2f, SpritePivot, m_AllSettings.m_SpritePivot);
	#if ENABLE_SPRITECOLLIDER
	GET_SET_COMPARE_DIRTY (int, SpriteColliderAlphaCutoff, m_AllSettings.m_SpriteColliderAlphaCutoff);
	GET_SET_COMPARE_DIRTY (float, SpriteColliderDetail, m_AllSettings.m_SpriteColliderDetail);
	#endif
	SpriteSheetMetaData* GetSpriteSheet() { return &m_SpriteSheet; }
	bool GetQualifiesForSpritePacking() const;
	bool GetAreSpritesGenerated() const;
	
	TextureImportInstructions GetTextureImportInstructions(BuildTargetPlatform platform);
#endif

	const std::string&	GetImportInspectorWarning()	{ return m_ImportInspectorWarnings; }

private:
	struct TextureImportOutput
	{
		DECLARE_SERIALIZE(TextureImportOutput)

		TextureImportInstructions textureImportInstructions;
		SourceTextureInformation  sourceTextureInformation;
	};

	void ResetTextureImportOutput ();

	void GenerateImageAssetData (std::auto_ptr<FreeImageWrapper>& image);
	void ProcessImportedImage (Texture2D* texture, ImageReference& srcImage, const TextureImportInstructions& instructions, int frame) const;
	void ProcessImportedLightmap (ImageReference& srcImage, const TextureImportInstructions& instructions, int mipCount, Texture2D* texture, UInt8* rawImageData) const;
	void ProcessMipLevel (ImageReference& curFloatImage, const TextureImportInstructions& instructions, const TextureColorSpace outputColorSpace) const;

#if ENABLE_SPRITES
	void GenerateSprites(Texture2D& texture, std::string textureName, TextureImporter& importer);
#endif

	TextureImportInstructions CalculateTargetSpecificTextureSettings (const SourceTextureInformation& sourceInformation, const BuildTargetSelection& targetPlatform, std::string* warnings) const;
	TextureImportInstructions CalculateTargetSpecificTextureSettings (const SourceTextureInformation& sourceInformation, int selectedTextureFormat, int maxTextureSize, int compressionQuality, const BuildTargetSelection& targetPlatform, std::string* warnings) const;
	TextureUsageMode GetUsageModeForPlatform (BuildTargetPlatform targetPlatform) const;
	TextureColorSpace GetColorSpaceForPlatform (BuildTargetPlatform targetPlatform) const;

	static TextureType GuessTextureTypeFromSettings (const Settings &settings, const SourceTextureInformation &sourceInfo);
	static void ApplyTextureTypeToSettings (TextureType textureType, Settings *settings, bool applyAll);

	void GetBestTextureSettings (BuildTargetPlatform platform, int* textureSize, int* textureFormat, int* compressionQuality) const;

	int	                        m_TextureType;
    Settings                    m_AllSettings;
	vector<BuildTargetSettings> m_BuildTargetSettings;
#if ENABLE_SPRITES
	SpriteSheetMetaData         m_SpriteSheet;
	UnityStr                    m_SpritePackingTag;
#endif

	// Output of the texture importer. Used by import settings.
	TextureImportOutput         m_Output;

	// this will be shown in inspector
	std::string					m_ImportInspectorWarnings;
};

enum SimpleTextureFormat
{
	kCompressed = -1,
	k16bit = -2,
	kTruecolor = -3
};

/// Gets the respective format supported alpha
int ConvertToAlphaFormat (int format);

void HandleCompressTexturesOnImportOnProjectLoad();
void SetApplicationSettingCompressTexturesOnImport(bool value);
bool GetApplicationSettingCompressTexturesOnImport();
bool DoesTextureStillNeedToBeCompressed(const string& assetPath);
void CompressTexturesStillRequiringCompression ();

#endif
