#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_SPRITES
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Graphics/SpriteFrame.h"
#include "Editor/Src/AssetPipeline/MdFourGenerator.h"
#include "Editor/src/FileCache/ObjectFileCache.h"

typedef std::vector<PPtr<Texture2D> > TTextureList;

typedef std::pair<UnityGUID, LocalIdentifierInFileType> TCachedRenderDataKey;
typedef std::map<TCachedRenderDataKey, SpriteRenderData> TCachedRenderDataMap;

struct CachedSpriteAtlas : public NamedObject
{
	REGISTER_DERIVED_CLASS (CachedSpriteAtlas, NamedObject)
	DECLARE_OBJECT_SERIALIZE (CachedSpriteAtlas)

	CachedSpriteAtlas(MemLabelId label, ObjectCreationMode mode);
	// ~CachedSpriteAtlas(); declared-by-macro

	TTextureList         textures;
	TCachedRenderDataMap frames;

	const SpriteRenderData* FindSpriteRenderData(const UnityGUID& guid, const LocalIdentifierInFileType& localId) const;
};

class SpriteAtlasCache : public ObjectFileCache
{
public:
	SpriteAtlasCache();
	static SpriteAtlasCache& Get();

	void Store(const std::string& atlasName, const MdFour& hash, PPtr<CachedSpriteAtlas> atlas);
	void Map(const std::string& atlasName, const MdFour& hash);
	void UnMapAll();

	const SpriteRenderData* FindSpriteRenderDataMRU(const UnityGUID& guid, const LocalIdentifierInFileType& localId, UnityStr& outAtlasName);

	void GetAvailableAtlases(std::vector<std::string>& names);
	void GetTexturesForAtlas(const std::string& atlasName, std::vector<PPtr<Texture2D> >& textures);

private:
	typedef std::map<std::string, MdFour> TAtlasNameToHash;
	TAtlasNameToHash m_NameToHash;

	virtual void OnRemoveFile(const MdFour& hash); 
};

#endif // ENABLE_SPRITES
