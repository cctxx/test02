#include "UnityPrefix.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"
#include "Editor/Src/AssetPipeline/SubstanceCache.h"
#include "Editor/Src/AssetPipeline/SubstanceImporter.h"

#if ENABLE_SUBSTANCE

SubstanceCache::CachedTexture::CachedTexture() :
	size(0),
	modified(false)
{
    texture.buffer = NULL;
}

SubstanceCache::CachedTexture::~CachedTexture()
{
    if (texture.buffer!=NULL)
        UNITY_FREE(kMemSubstance, texture.buffer);
}

SubstanceCache::SubstanceCache()
{
}

void SubstanceCache::Clear(bool force_rebuild)
{
	for (std::set<SInt32>::iterator it=cachedIDs.begin() ; it!=cachedIDs.end() ; ++it)
	{
		ProceduralMaterial* substance = static_cast<ProceduralMaterial*>(Object::IDToPointerThreadSafe(*it));
		if (substance!=NULL)
		{
			for (ProceduralMaterial::PingedTextures::iterator t=substance->GetPingedTextures().begin();
				t!=substance->GetPingedTextures().end();++t)
			{
				ProceduralTexture* texture = *t;
				if (texture!=NULL)
				{
					texture->EnableFlag(ProceduralTexture::Flag_Cached, false);
					if (force_rebuild)
						texture->Invalidate();
				}
			}
			if (force_rebuild)
				substance->RebuildTexturesImmediately();
		}
	}	
    assetPath = "";
    textures.clear();
	cachedIDs.clear();
}

void SubstanceCache::SafeClear(bool force_rebuild)
{
	Lock();
	Clear(force_rebuild);
	Unlock();
}

void SubstanceCache::Lock()
{
	mutex.Lock();
}

void SubstanceCache::Unlock()
{
	mutex.Unlock();
}

SubstanceCache* gSubstanceCache = NULL;

static RegisterRuntimeInitializeAndCleanup s_SubstanceCacheCallbacks(NULL, SubstanceCache::StaticDestroy);

void SubstanceCache::DeleteCachedTextures(ProceduralMaterial& material)
{
	string path = SubstanceImporter::FindAssetPathForSubstance(material);
	Lock();
	if (assetPath == path)
	{
		for (ProceduralMaterial::Textures::iterator i = material.GetTextures().begin() ; i != material.GetTextures().end() ; ++i)
		{
			std::string texName = (*i)->GetName();
			std::map<std::string, SubstanceCache::CachedTexture>::iterator it = textures.find(texName);
			if (it != textures.end())
			{
				textures.erase(it);
			}
		}
	}
	Unlock();
}

void SubstanceCache::DeleteCachedTexture(ProceduralMaterial& material, std::string& texName)
{
	string path = SubstanceImporter::FindAssetPathForSubstance(material);
	Lock();
	if (assetPath == path)
	{
		std::map<std::string, SubstanceCache::CachedTexture>::iterator it = textures.find(texName);
		if (it != textures.end())
		{
			textures.erase(it);
		}
	}
	Unlock();
}



void SubstanceCache::StaticDestroy()
{
	if (gSubstanceCache)
	{
		std::map<std::string, CachedTexture> &textures = gSubstanceCache->textures;
		for (std::map<std::string,CachedTexture>::iterator it=textures.begin() ; it != textures.end() ; ++it)
		{
			if (it->second.texture.buffer != NULL)
			{
				UNITY_FREE(kMemSubstance, it->second.texture.buffer);
				it->second.texture.buffer = NULL;
			}
		}
		UNITY_DELETE(gSubstanceCache, kMemSubstance);
	}
}

SubstanceCache& GetSubstanceCache ()
{
	if (!gSubstanceCache)
	{
		gSubstanceCache = UNITY_NEW (SubstanceCache, kMemSubstance);
	}

	return *gSubstanceCache;
}

#endif
