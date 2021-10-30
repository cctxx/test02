#ifndef SHADERIMPORTER_H
#define SHADERIMPORTER_H

#include "AssetImporter.h"
#include "Runtime/Graphics/Texture.h"

class Shader;

class ShaderImporter : public AssetImporter
{
public:
	REGISTER_DERIVED_CLASS (ShaderImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (ShaderImporter)

	ShaderImporter(MemLabelId label, ObjectCreationMode mode);
	// ~ShaderImporter (); declared-by-macro
	
	virtual void GenerateAssetData ();
	virtual void UnloadObjectsAfterImport (UnityGUID guid);
	
	void SetDefaultTextures (const std::vector<std::string>& names, const std::vector<Texture*>& targets);
	PPtr<Texture> GetDefaultTexture (const std::string& name);

	static void InitializeClass ();
	static void CleanupClass () {};

	static void ReloadAllShadersAfterImport ();

	std::vector<std::pair<UnityStr, PPtr<Texture> > > m_DefaultTextures;
};

bool UpgradeShadersIfNeeded (bool willReimportWholeProject);
Shader* CreateShaderAsset (const std::string& source);
void UpdateShaderAsset (Shader* shader, const std::string& source);


#endif
