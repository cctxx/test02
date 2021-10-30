#include "UnityPrefix.h"
#include "MiniAssetIconCache.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Editor/Src/EditorResources.h"

static map<int, Texture2D*> ms_HackedCache;

void UpdateCachedIcon (int instanceID)
{
	map<int, Texture2D*>::iterator found = ms_HackedCache.find(instanceID);
	if (found != ms_HackedCache.end())
	{
		DestroySingleObject(found->second);
		ms_HackedCache.erase(found);
	}
}

Texture* GetCachedAssetDatabaseIcon (const string& assetPath)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	UnityGUID guid;
	if (!pm.PathNameToGUID(assetPath, &guid))
		return NULL;

	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset)
		return GetCachedAssetDatabaseIcon(guid, *asset, asset->mainRepresentation);
	else
		return NULL;
}

void PostprocessAssetsUpdateCachedIcon (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, string>& moved)
{
	if (ms_HackedCache.empty())
		return;

	for (std::set<UnityGUID>::const_iterator i=refreshed.begin();i != refreshed.end();i++)
	{
		const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(*i);
		if (asset)
		{
			UpdateCachedIcon (asset->mainRepresentation.object.GetInstanceID());
			for (int j=0;j<asset->representations.size();j++)
			{
				UpdateCachedIcon (asset->representations[j].object.GetInstanceID());
			}
		}
	}
}

Texture* GetCachedAssetDatabaseIcon (const UnityGUID& guid, const Asset& asset, const LibraryRepresentation& rep)
{

	if (rep.thumbnail.GetImageData () != NULL)
	{
		if (ms_HackedCache.count(rep.object.GetInstanceID()))
			return ms_HackedCache[rep.object.GetInstanceID()];
		else
		{
			if (ms_HackedCache.size() > 200)
			{
				for (map<int, Texture2D*>::iterator i=ms_HackedCache.begin();i != ms_HackedCache.end();i++)
					DestroySingleObject(i->second);
				ms_HackedCache.clear();
			}

			Texture2D* texture = CreateObjectFromCode<Texture2D>(kDefaultAwakeFromLoad, kMemTextureCache);
			texture->SetHideFlags(Object::kHideAndDontSave);
			texture->SetImage (rep.thumbnail, Texture2D::kNoMipmap);
			texture->UpdateImageDataDontTouchMipmap ();
			ms_HackedCache[rep.object.GetInstanceID()] = texture;
			return texture;
		}
	}

	if (asset.type == kFolderAsset)
	{
		return Texture2DNamed (EditorResources::kFolderIconName);
	}
	else if (rep.classID == ClassID (GameObject))
	{
		if (asset.type == kCopyAsset)
			return Texture2DNamed ("PrefabModel Icon");
		else
			return Texture2DNamed ("PrefabNormal Icon");
	}
	else if (rep.classID == ClassID (MonoScript))
	{
		MonoScript* script = dynamic_pptr_cast<MonoScript*> (rep.object);
		return TextureForScript (script);
	}
	else if (rep.classID == ClassID(MonoBehaviour))
	{
		if (!rep.scriptClassName.empty())
		{
			// Remove namespace. Used e.g for UnityEngine.GUISkin to fetch 'GUISkin Icon'
			int pos = rep.scriptClassName.rfind (".");
			const char* iconName = (pos != string::npos) ? (rep.scriptClassName.c_str() + pos + 1) : (rep.scriptClassName.c_str());
			Texture2D* tex = Texture2DNamed(Format ("%s Icon", iconName));
			if (tex != NULL)
				return tex;
		}
		return Texture2DNamed("ScriptableObject Icon");
	}

	return TextureForClass (rep.classID);
}
