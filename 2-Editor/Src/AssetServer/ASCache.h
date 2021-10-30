#ifndef ASSETSERVERCACHE_H
#define ASSETSERVERCACHE_H

#include "ASConfiguration.h"
#include "Editor/Src/AssetPipeline/MdFourGenerator.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Editor/Src/AssetPipeline/AssetMetaData.h"

#include <string>
#include <map>

using std::string;
using std::map;
using std::set;

/**
 * Note that conceptually (if not codepath-wise) this is owned by the AssetServer::Controller class,
 * and should only be updated and queried from there.
 */
namespace AssetServer {
	enum DownloadResolution {
		kDLUnresolved = 0,
		kDLSkipAsset = 1,
		kDLTrashMyChanges = 2,
		kDLTrashServerChanges = 3,
		kDLMerge = 4
	};

	enum NameConflictResolution {
		kNMUnresolved = 0,
		kNMRenameLocal = 1,
		kNMRenameRemote = 2
	};

	struct ChangeSummary {
		int serverChanges;
		int clientChanges;
		int conflicts;
		ChangeSummary(int srv, int cl, int cf) : serverChanges(srv), clientChanges(cl), conflicts(cf) {}
	};
}

class AssetServerCache : public Object 
{
 public:

	/// Singleton object
	static AssetServerCache& Get();
	AssetServerCache(MemLabelId label, ObjectCreationMode mode);
	// ~AssetServerCache (); declared-by-macro

	static void InitializeClass ();
	static void CleanupClass () {}

	struct Item {

		AssetServer::DownloadResolution downloadResolution;
		AssetServer::NameConflictResolution nameConflictResolution;
		bool markedForRemoval;
		
		Item()  { Reset (); }
		void Reset();
		
		DECLARE_SERIALIZE(Item)

	 private:
	 	friend class AssetServerCache;

		
	};

	struct DeletedItem {
		int changeset;
		UnityGUID guid;
		UnityGUID parent;
		UnityStr fullPath;
		int type;
		MdFour digest;

		DECLARE_SERIALIZE(DeletedItem)
	};

	class CachedAssetMetaData
	{
	public:
		UnityGUID guid;
		UnityStr pathName;
		UInt32 originalChangeset;
		UnityStr originalName;
		UnityGUID originalParent;
		MdFour originalDigest;
	private:
		MdFour currentDigest;

	public:
		CachedAssetMetaData()
		{
		}

		CachedAssetMetaData(AssetMetaData *src)
		:	guid(src->guid)
		,	pathName(src->pathName)
		,	originalChangeset(src->originalChangeset)
		,	originalName(src->originalName)
		,	originalParent(src->originalParent)
		,	originalDigest(src->originalDigest)
		{ }
		
		DECLARE_SERIALIZE(CachedAssetMetaData)
	};

	// Fetch cache item for read & write (assume write too)
	Item& GetItem(const UnityGUID& guid);

	// Fetch cache item for read (assume no write)
	const Item& GetItemConst(const UnityGUID& guid);

	// Fetch const iterator of items (assume no write)
	const UNITY_MAP(kMemAssetServerCache, UnityGUID, Item)& Items() const { return m_Items; }

	void InvalidateAll() { m_Items.clear(); m_LastItem = m_Items.end(); }
	void Invalidate(const UnityGUID& guid) {
		if (m_LastItem->first == guid) m_LastItem = m_Items.end(); 
		m_Items.erase(guid);
	}

	void GetWorkingItemChangesets(map<UnityGUID, int> &changesets);

	void SetReceived(const UnityGUID& guid) { m_Received.insert(guid); }
	bool IsReceived(const UnityGUID& guid) const { return m_Received.count(guid) > 0; }
	void FlushReceived() { m_Received.clear(); }
	
	void AddDeletedItem(const UnityGUID& guid);
	bool UpdateDeletedItem(const UnityGUID& guid);
	void RemoveDeletedItem(const UnityGUID& guid);
	void GetDeletedItems(vector<DeletedItem>& items);
	bool GetDeletedItem(const UnityGUID& guid, DeletedItem &item);
	bool IsItemDeleted(const UnityGUID& guid) const;

	const UnityStr& GetLastCommitMessage() const { return m_LastCommitMessage; }
	void SetLastCommitMessage(const string& message) { m_LastCommitMessage = message; }
	const set<UnityGUID>& GetCommitSelectionGUIDs() const { return m_CommitItemSelection; }
	void SetCommitSelectionGUIDs(const set<UnityGUID>& selection) { m_CommitItemSelection = selection; }
	void ClearCommitPersistentData() { m_LastCommitMessage.clear(); m_CommitItemSelection.clear(); }
	void ClearDeletedItems();

	const UNITY_MAP(kMemAssetServerCache, UnityGUID, AssetServer::Item) & GetCachedChanges();
	void UpdateCachedItems(const set<UnityGUID> &items);
	void UpdateCachedItems(const map<UnityGUID, string> &items);
	void EraseCachedItems(const set<UnityGUID> &items);
	void InvalidateCachedItems();

	void UpdateCachedCommitItem(UnityGUID guid);
	void UpdateCachedMetaDataItem(UnityGUID guid);
	void SaveCachedMetaDataItem(const CachedAssetMetaData& metaData) { m_WorkingItemMetaData[metaData.guid]=metaData; SetDirty(); }

	bool GetCachesInitialized() { return m_CachesInitialized; }
	CachedAssetMetaData* FindCachedMetaData(UnityGUID guid);

	int GetLatestServerChangeset()	{ return m_LatestServerChangeset; }
	void SetLatestServerChangeset(int changeset) { m_LatestServerChangeset = changeset; SetDirty(); }
	bool InitializeCaches();

	REGISTER_DERIVED_CLASS (AssetServerCache, Object)
	DECLARE_OBJECT_SERIALIZE(AssetServerCache)
	
	bool ShouldIgnoreInGarbageDependencyTracking ();
	
	MdFour FindCachedDigest (const UnityGUID& guid);
	void SetCachedDigest (const UnityGUID& guid, const MdFour& digest);
	void RemoveCachedDigest (const UnityGUID& guid) ;
	
 private:
 	UNITY_MAP(kMemAssetServerCache, UnityGUID, Item) m_Items;
	UNITY_MAP(kMemAssetServerCache, UnityGUID, Item)::iterator m_LastItem;
	set<UnityGUID> m_Received;
	UNITY_MAP(kMemAssetServerCache, UnityGUID, DeletedItem) m_DeletedItems;
	int m_CountServerChanges;
	int m_CountClientChanges;
	int m_CountConflicts;
	UnityStr m_LastCommitMessage;
	set<UnityGUID> m_CommitItemSelection;
	int m_LatestServerChangeset;

	// items caching
	int m_CachesInitialized;
	UNITY_MAP(kMemAssetServerCache, UnityGUID, AssetServer::Item) m_ModifiedItems;
	UNITY_MAP(kMemAssetServerCache, UnityGUID, CachedAssetMetaData) m_WorkingItemMetaData;
	UNITY_MAP(kMemAssetServerCache, UnityGUID, MdFour) m_Digests;

	inline void AddCachedItem(AssetServer::Item item);
	const UNITY_MAP(kMemAssetServerCache, UnityGUID, CachedAssetMetaData) GetWorkingItemsMetaData();
};

void PostprocessAssetImport (const set<UnityGUID>& refreshed, const set<UnityGUID>& added, const set<UnityGUID>& removed, const map<UnityGUID, string>& moved);

#endif
