#include "UnityPrefix.h"
#include "SpritePackerCache.h"

#if ENABLE_SPRITES
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/File/FileScanning.h"
#include "Runtime/Utilities/FileUtilities.h"

const UInt32 kCacheSizeLimit = (1024 * 1024) * 1536;
const char*  kCachePath = "Library/AtlasCache";



IMPLEMENT_CLASS(CachedSpriteAtlas)
IMPLEMENT_OBJECT_SERIALIZE(CachedSpriteAtlas)

CachedSpriteAtlas::CachedSpriteAtlas(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
}

CachedSpriteAtlas::~CachedSpriteAtlas()
{
}

template<class TransferFunction>
void CachedSpriteAtlas::Transfer (TransferFunction& transfer)
{
	TRANSFER (textures);
	TRANSFER (frames);
}

const SpriteRenderData* CachedSpriteAtlas::FindSpriteRenderData(const UnityGUID& guid, const LocalIdentifierInFileType& localId) const
{
	TCachedRenderDataKey key = make_pair(guid, localId);
	TCachedRenderDataMap::const_iterator it = frames.find(key);
	if (it != frames.end())
		return &it->second;
	return NULL;
}



SpriteAtlasCache::SpriteAtlasCache()
: ObjectFileCache(kCachePath, kCacheSizeLimit)
{
}

SpriteAtlasCache& SpriteAtlasCache::Get()
{
	static SpriteAtlasCache* cache = 0;
	if (!cache)
		cache = new SpriteAtlasCache();
	return *cache;
}

const SpriteRenderData* SpriteAtlasCache::FindSpriteRenderDataMRU(const UnityGUID& guid, const LocalIdentifierInFileType& localId, UnityStr& outAtlasName)
{
	for (TAtlasNameToHash::const_iterator aIt = m_NameToHash.begin(); aIt != m_NameToHash.end(); ++aIt)
	{
		PPtr<CachedSpriteAtlas> atlas(LoadAndGetInstanceID(aIt->second));
		const SpriteRenderData* rd = atlas->FindSpriteRenderData(guid, localId);
		if (rd)
		{
			outAtlasName = UnityStr(aIt->first);
			return rd;
		}
	}

	outAtlasName.clear();
	return NULL;
}

void SpriteAtlasCache::Store(const std::string& atlasName, const MdFour& hash, PPtr<CachedSpriteAtlas> atlas)
{
	TAtlasNameToHash::const_iterator aIt = m_NameToHash.find(atlasName);
	//Assert(aIt == m_NameToHash.end());

	dynamic_array<Object*> generatedAssets (kMemTempAlloc);
	generatedAssets.push_back(atlas);
	for (TTextureList::iterator it = atlas->textures.begin(); it != atlas->textures.end(); ++it)
		generatedAssets.push_back(*it);
	ObjectFileCache::Store(hash, generatedAssets);

	m_NameToHash[atlasName] = hash;
}

void SpriteAtlasCache::Map(const std::string& atlasName, const MdFour& hash)
{
	m_NameToHash[atlasName] = hash;
}

void SpriteAtlasCache::UnMapAll()
{
	m_NameToHash.clear();
}

void SpriteAtlasCache::OnRemoveFile(const MdFour& hash)
{
	ObjectFileCache::OnRemoveFile(hash);

	TAtlasNameToHash::iterator it = m_NameToHash.begin();
	while (it != m_NameToHash.end())
	{
		TAtlasNameToHash::iterator current = it;
		++it;

		if (current->second == hash)
			m_NameToHash.erase(current);
	}
}

void SpriteAtlasCache::GetAvailableAtlases (std::vector<std::string>& names)
{
	names.clear();
	names.reserve(m_NameToHash.size());
	for (TAtlasNameToHash::const_iterator it = m_NameToHash.begin(); it != m_NameToHash.end(); ++it)
		names.push_back(it->first);
}

void SpriteAtlasCache::GetTexturesForAtlas (const std::string& atlasName, std::vector<PPtr<Texture2D> >& textures)
{
	textures.clear();

	TAtlasNameToHash::iterator hit = m_NameToHash.find(atlasName);
	if (hit == m_NameToHash.end())
		return;

	const MdFour& hash = hit->second;
	PPtr<CachedSpriteAtlas> atlas(SpriteAtlasCache::Get().LoadAndGetInstanceID(hash));
	Assert(atlas.IsValid());
	for (TTextureList::iterator it = atlas->textures.begin(); it != atlas->textures.end(); ++it)
		textures.push_back(*it);
}

#endif //ENABLE_SPRITES
