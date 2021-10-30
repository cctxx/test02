#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_SUBSTANCE

#include <string>
#include <map>
#include <set>
#include "Runtime/Graphics/ProceduralMaterial.h"

struct SubstanceCache
{
    struct CachedTexture
    {
        CachedTexture();
        ~CachedTexture();
    
		SubstanceTexture texture;
        TextureFormat format;
        size_t size;
		int width;
		int height;
		bool modified;
    };

	SubstanceCache();
    void Clear(bool force_rebuild=false);
	void SafeClear(bool force_rebuild=false);
	void Lock();
	void Unlock();

	// Delete all textures from a given ProceduralMaterial from the SubstanceCache
	void DeleteCachedTextures(ProceduralMaterial& material);

	// Delete a single texture from a given ProceduralMaterial from the SubstanceCache
	void DeleteCachedTexture(ProceduralMaterial& material, std::string& texName);

	static void StaticDestroy();

	std::set< SInt32 > cachedIDs;
    std::string assetPath;
    std::map< std::string, CachedTexture > textures;
	Mutex mutex;
};

SubstanceCache& GetSubstanceCache ();

#endif
