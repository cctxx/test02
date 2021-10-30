#include "UnityPrefix.h"
#include "BumpMapSettings.h"
#include "AssetDatabase.h"
#include "Runtime/Shaders/Material.h"
#include "AssetInterface.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "TextureImporter.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Utilities/Argv.h"

SHADERPROP(BumpMap);

static BumpMapSettings* gBumpMapSettings = NULL;

BumpMapSettings& BumpMapSettings::Get ()
{
	if (gBumpMapSettings == NULL)
		gBumpMapSettings = new BumpMapSettings ();
	return *gBumpMapSettings;
}

void BumpMapSettings::PerformUnmarkedBumpMapTexturesFixing()
{
	if (m_UnmarkedBumpMapTextureImporters.empty())
		return;

	std::vector<string> paths;
	for (UnmarkedBumpMapTextureImporters::iterator i = m_UnmarkedBumpMapTextureImporters.begin ();i != m_UnmarkedBumpMapTextureImporters.end ();i++)
	{
		TextureImporter* importer = dynamic_instanceID_cast<TextureImporter *> (*i);
		if (importer != NULL)
			paths.push_back(importer->GetAssetPathName());
	}

	if (IsBatchmode())
	{
		PerformUnmarkedBumpMapTexturesFixingAfterDialog(1);
	}
	else
	{
		void* params[] = { Scripting::StringVectorToMono(paths) };
		CallStaticMonoMethod ("BumpMapSettingsFixingWindow","ShowWindow", params);
	}
}

void BumpMapSettings::PerformUnmarkedBumpMapTexturesFixingAfterDialog(int result)
{
	if (result == 1)
	{
		GUIDPersistentManager& persistentManager = GetGUIDPersistentManager();
		set<UnityGUID> badTextures;

		for (UnmarkedBumpMapTextureImporters::iterator i = m_UnmarkedBumpMapTextureImporters.begin ();i != m_UnmarkedBumpMapTextureImporters.end ();i++)
		{
			TextureImporter* importer = dynamic_pptr_cast<TextureImporter *> (FindAssetImporterForObject (*i));
			if (importer == NULL)
				continue;
			
			importer->FixNormalmap();

			UnityGUID guid;
			string assetPath = importer->GetAssetPathName();
			if (!persistentManager.PathNameToGUID(assetPath, &guid))
				ErrorString("Failed to get GUID for " + assetPath);

			badTextures.insert(guid);
		}

		m_UnmarkedBumpMapTextureImporters.clear();
		AssetInterface::Get().ImportAssets(badTextures);
	}
	else
		m_UnmarkedBumpMapTextureImporters.clear();
}

TextureImporter* GetBumpMapImporterForMaterial(Material& material)
{
	// Find property in saved properties
	UnityPropertySheet::TexEnvMap::iterator found = material.GetSavedProperties().m_TexEnvs.find(kSLPropBumpMap);
	if (found == material.GetSavedProperties().m_TexEnvs.end())
		return NULL;
	
	if (found->second.m_Texture.GetInstanceID() == 0)
		return NULL;
	
	// Get texture importer
	return dynamic_pptr_cast<TextureImporter *> (FindAssetImporterForObject (found->second.m_Texture.GetInstanceID()));
}

void BumpMapSettings::PerformBumpMapCheck(Material& material)
{
	TextureImporter* importer = GetBumpMapImporterForMaterial(material);
	if (importer && !importer->IsNormalMap())
		m_UnmarkedBumpMapTextureImporters.insert(importer->GetInstanceID());
	
	//@TODO: Unload importer & serialized file...
}

bool BumpMapSettings::BumpMapTextureNeedsFixing(Material& material)
{
	TextureImporter* importer = GetBumpMapImporterForMaterial(material);

	if (importer == NULL)
		return false; // we just don't know

	// query if the texture was imported as a normal map
	return !importer->IsNormalMap();
}

void BumpMapSettings::FixBumpMapTexture(Material& material)
{
	TextureImporter* importer = GetBumpMapImporterForMaterial(material);

	Assert(importer);
	if (importer == NULL)
		return;

	if (importer->IsNormalMap())
		return;
	
	
	importer->FixNormalmap();

	AssetInterface::Get().ImportAtPath(importer->GetAssetPathName());
}
