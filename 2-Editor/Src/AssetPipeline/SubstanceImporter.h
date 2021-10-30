// This class is responsible for importing *.SBSBIN files as SubstanceArchive objects
//
// SUBSTANCE HOOK
//

#ifndef SBSIMPORTER_H
#define SBSIMPORTER_H


//#pragma warning( disable : 4275, 4251 )

#include "AssetImporter.h"
#include "Runtime/Graphics/SubstanceArchive.h"
#include "Runtime/Graphics/ProceduralMaterial.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/SubstanceSystem.h"

// The linker that will compile our SBSASM file into a SBSBIN for the correct platform
#include "../../External/Allegorithmic/builds/Engines/include/substance/linker/linker.h"

class TiXmlDocument;

struct InputImportSettings
{
	InputImportSettings();
	DECLARE_SERIALIZE( InputImportSettings )

	UnityStr	name;
	SubstanceValue value;
};

struct TextureImportSettings
{
	TextureImportSettings();
	DECLARE_SERIALIZE( InputImportSettings )

	UnityStr name;
	ProceduralOutputType alphaSource;
	int filterMode;
	int aniso;
	int wrapMode;
};

struct ProceduralMaterialInformation
{
	ProceduralMaterialInformation();
	DECLARE_SERIALIZE (ProceduralMaterialInformation)
	
	// Global tiling & offset 
	Vector2f m_Offset;
	Vector2f m_Scale;

	// Material flags
	int m_GenerateAllOutputs;
	int m_AnimationUpdateRate;
};

struct SubstanceBuildTargetSettings
{
	SubstanceBuildTargetSettings();
	DECLARE_SERIALIZE(BuildTargetSettings)

	UnityStr m_BuildTarget;
	int m_TextureWidth;
	int m_TextureHeight;
	int m_TextureFormat;		
	int m_LoadingBehavior;
};

// Class holding the material import settings for each material instance
struct MaterialInstanceSettings
{
	DECLARE_SERIALIZE( MaterialInstanceSettings )

	UnityStr name;                                              // The instance name
	UnityStr prototypeName;                                     // The prototype (substance graph) name
	UnityStr shaderName;                                        // The name of the shader to use for the material
	std::vector<InputImportSettings> inputs;                    // The inputs data
	ProceduralMaterialInformation materialInformation;          // The material infos to apply to generate textures for this material
	UnityPropertySheet materialProperties;                      // The base material properties
	std::vector<TextureImportSettings> textureParameters;       // The texture parameters
	vector<SubstanceBuildTargetSettings> buildTargetSettings;   // The platform settings
};

struct MaterialImportOutput
{
	MaterialImportOutput();
	DECLARE_SERIALIZE( MaterialImportOutput )
	
	SubstanceBuildTargetSettings currentSettings;				// The current settings of the material
	int baked;													// Is the material baked ?
};

class SubstanceImporter : public AssetImporter
{
	// Import settings for each material instance
	vector<MaterialInstanceSettings> m_MaterialInstances;
	vector<MaterialImportOutput> m_MaterialImportOutputs;
	set<UnityStr> m_DeletedPrototypes;

public:		// METHODS
	
	REGISTER_DERIVED_CLASS( SubstanceImporter, AssetImporter )
	DECLARE_OBJECT_SERIALIZE( SubstanceImporter )

	SubstanceImporter( MemLabelId label, ObjectCreationMode mode );

	virtual void AwakeFromLoad( AwakeFromLoadMode awakeMode );

	void ApplyInputImportSettings (ProceduralMaterial& material, const MaterialInstanceSettings& settings, SubstanceInputs& outInputs);

	void GeneratePackagesFromDescription( const string& description, std::vector<PPtr<ProceduralMaterial> >& materials );

	virtual void GenerateAssetData();
	
	vector<string> GetSubstanceNames();

	string GenerateUniqueName (const string& name);
	string GenerateUniqueName (const string& name, const std::vector<string> &prototypes);
	string GenerateUniqueName (const string& name, const std::set<string>* disallowedNames);

	int GetInstanceCount() const { return m_MaterialInstances.size(); }

	static void	InitializeClass();
	static void CleanupClass()			{}

	//////////////////////////////////////////////////////////////////////////
	// Material & Inputs info accessors

	// Sets the material informations about the specified material
	void SetMaterialInformation( ProceduralMaterial& material, ProceduralMaterialInformation information );

	// Gets the material informations about the specified material
	ProceduralMaterialInformation GetMaterialInformation( ProceduralMaterial& material );

	// Get the texture parameters from the material information
	TextureImportSettings& GetTextureParameters( ProceduralMaterial& material, std::string textureName );

	// Get the texture alphaSource parameter from the material information
	ProceduralOutputType GetTextureAlphaSource( ProceduralMaterial& material, std::string textureName );

	// Set the texture parameter on the material information
	void SetTextureAlphaSource( ProceduralMaterial& material, std::string textureName, ProceduralOutputType alphaSource );

	// Callback to notify changes of the texture parameters
	void OnTextureInformationsChanged(ProceduralTexture& texture);

	// Callback to notify changes of the material shader
	void OnShaderModified(ProceduralMaterial& material);

	// Resets the specified material's values to its default values
	void ResetDefaultValues( ProceduralMaterial& _pMaterial );

	vector<ProceduralMaterial*> GetImportedMaterials();

	// States tracking required to have proper reimport with caching
    static bool OnLoadSubstance(ProceduralMaterial& substance);
	static void OnTextureModified(SubstanceSystem::Substance& substance, ProceduralTexture& texture, SubstanceTexture& result);
	TextureFormat GetSubstanceBakedFormat(SubstanceOutputFormat outputFormat) const;
	static void HandleBakingAndThumbnail(ProceduralMaterial& substance, ProceduralTexture& texture, SubstanceTexture& result, TextureFormat format, BuildTargetPlatform targetPlatform, bool doBaking=true);
	static void OnInputModified(ProceduralMaterial& substance, std::string inputName, SubstanceValue& inputValue);
	static void OnSubstanceModified(SubstanceSystem::Substance& substance);
private:
	static SubstanceImporter* FindImporterForSubstance(ProceduralMaterial& substance);
public:

	static std::string FindAssetPathForSubstance(ProceduralMaterial& substance);

	/// Material instancing
	std::string InstanciatePrototype( std::string prototypeName );
	void DeleteMaterialInstance( ProceduralMaterial& instance );
	bool RenameMaterialInstance( ProceduralMaterial& instance, std::string name );
	std::string CloneMaterialInstance( const ProceduralMaterial& source );

	/// Needed for backward compatibility with old meta files
	virtual void ApplyImportSettingsDeprecated( const YAMLMapping* settings ) ;
	
	/// Check if it needs to be reimported for the targetPlatform, with dependencies, or is it already in the right format
	static bool DoesAssetNeedReimport (const string& assetPath, BuildTargetPlatform targetPlatform, bool unload, bool requireCompressedAssets);
	
	/// Can this importer import this file at path?
	/// @implement this to support reimport assets 
	static int CanLoadPathName(const std::string& path, int* q = NULL);
	
	// Platform settings
	SubstanceBuildTargetSettings GetPlatformSettings (const std::string& materialName, BuildTargetPlatform platform) const;
	void ClearPlatformTextureSettings (const std::string& materialName, const std::string& platform);	
	void SetPlatformTextureSettings (const std::string& materialName, const std::string& platform, int width, int height, int format, int loadingBehavior);	
	bool GetPlatformTextureSettings (const std::string& materialName, const std::string& platform, int* width, int* height, int* format, int* loadingBehavior) const;	
	
	// Export to bitmap
	void ExportBitmaps (ProceduralMaterial& material);

	// Texture generation (baking & thumbnail)
	static void SUBSTANCE_CALLBACK OnTextureGenerated(SubstanceHandle* handle, unsigned int outputIndex, size_t jobUserData);
	struct GeneratedTexture
	{
		SubstanceHandle* handle;
		unsigned int outputIndex;
		ProceduralTexture* texture;
	};
	static std::vector<GeneratedTexture> m_GeneratedTextures;

    static bool IsSubstanceParented (ProceduralTexture& texture, ProceduralMaterial& material);
	
	// Discard modifications when switching between playmode / editmode
public:
	static void OnEnterPlaymode();
	static void OnLeavePlaymode();
	static bool CheckPlayModeAndSkip(ProceduralMaterial& material);
private:
	void ImportModifiedMaterials();
	static bool m_IsPlaying;
	static std::set<int> m_ModifiedMaterials;
	static std::set<ProceduralMaterial*> m_MaterialsAddedToTheScene;

private:
    static SubstanceImporter* m_CurrentImporter;
	static int m_CurrentInstance;
protected:

	ProceduralMaterial* GenerateSubstanceMaterial (TiXmlDocument& xmlData, SubstanceArchive& packageSBS, SubstanceHandle* substanceHandle, const std::string& prototypeName,
		const std::string& materialName, MaterialInstanceSettings* settings, MaterialImportOutput* import, BuildTargetPlatform targetPlatform, bool forceUncompressed);
	InputImportSettings* FindMaterialInputImportSetting( const std::string& materialName, const std::string& name );
	InputImportSettings& FindOrCreateInputImportSetting( ProceduralMaterial& material, const std::string& name );

	MaterialInstanceSettings* FindMaterialInstanceSettings( const std::string& name );
	MaterialInstanceSettings* FindPrototypeSettings( const std::string& name );
	MaterialInstanceSettings& FindOrCreateMaterialSettings( const std::string& name, std::string prototypeName="");
	TextureImportSettings* FindTextureSettings(ProceduralMaterial& material, MaterialInstanceSettings& materialSettings, std::string textureName);
	TextureImportSettings& FindOrCreateTextureSettings(ProceduralMaterial& material, MaterialInstanceSettings& materialSettings, std::string textureName);
};

extern std::string ProceduralPropertyTypeToUnityShaderPropertyName (ProceduralOutputType type);

#endif
