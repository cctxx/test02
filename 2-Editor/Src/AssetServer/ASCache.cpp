#include "UnityPrefix.h"
#include "ASCache.h"

#include "Editor/Src/AssetPipeline/AssetInterface.h"
//#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "ASController.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"

#include "ASMonoUtility.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Mono/MonoManager.h"
#include "Editor/Src/EditorSettings.h"


using namespace std;
using namespace AssetServer;

static AssetServerCache* s_AssetServerCache = NULL;

#define LogError(x) { ErrorString(x); Controller::Get().SetError(x);}

AssetServerCache& AssetServerCache::Get() { 
	return *s_AssetServerCache; 
}

AssetServerCache::AssetServerCache(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{ 
	s_AssetServerCache = this;
	m_LastItem = m_Items.end();
	m_CountServerChanges=0;
	m_CountClientChanges=0;
	m_CountConflicts=0;
	m_CachesInitialized = false;
	m_LatestServerChangeset = -1;
}

AssetServerCache::~AssetServerCache ()
{}

IMPLEMENT_CLASS_HAS_INIT (AssetServerCache);
void AssetServerCache::InitializeClass() 
{
#if !UNITY_LINUX
	// FIXME, XXX, TODO, HACK: Some AssetServer client stuff breaks during
	// initialization on Linux - disable for now to get batchmode working...
	AssetDatabase::RegisterPostprocessCallback (PostprocessAssetImport);
#endif
}

AssetServerCache::Item& AssetServerCache::GetItem(const UnityGUID& guid) {
	SetDirty();

	if (m_LastItem != m_Items.end() && m_LastItem->first == guid)
		return m_LastItem->second;
	typedef pair<UNITY_MAP(kMemAssetServerCache, UnityGUID, Item)::iterator, bool> ItemPair;
	ItemPair done = m_Items.insert(make_pair(guid, AssetServerCache::Item()));
	if (done.second)
		done.first->second.Reset();
	
	m_LastItem = done.first;
	
	return done.first->second;
}

const AssetServerCache::Item& AssetServerCache::GetItemConst(const UnityGUID& guid) {
	if (m_LastItem != m_Items.end() && m_LastItem->first == guid)
		return m_LastItem->second;
	
	typedef pair<UNITY_MAP(kMemAssetServerCache, UnityGUID, Item)::iterator, bool> ItemPair;
	ItemPair done = m_Items.insert(make_pair(guid, AssetServerCache::Item()));
	if (done.second)
		done.first->second.Reset();
	
	m_LastItem = done.first;
	
	return done.first->second;
}

// when calling this function, asset info needs to still be in a database
void AssetServerCache::AddDeletedItem(const UnityGUID& guid)
{
	DeletedItem newItem;
	AssetServer::Configuration& conf = AssetServer::Configuration::Get();

	AssetServer::Item sitem = conf.GetServerItem(guid);
	AssetServer::Item witem = conf.GetWorkingItem(guid);

	if ((sitem != AssetServer::Item()) && (sitem.parent != kTrashGUID)) // was this item committed to server (and not as a deleted asset)? 
	{
		ASMonoUtility::Get().SetNeedsToRefreshUpdate(); // this is asset that was actually on the server and now update window needs to refresh

		newItem.guid = guid;
		newItem.changeset = witem.changeset;
		newItem.fullPath = conf.CachedPathFromID(sitem.parentFolderID) + (std::string)sitem.name;
		newItem.digest = witem.digest;
		newItem.type = sitem.type;
		newItem.parent = sitem.parent;

		RemoveDeletedItem(guid); // don't allow duplicates

		m_DeletedItems[guid] = newItem;

		SetDirty();
	}
}

bool AssetServerCache::UpdateDeletedItem(const UnityGUID& guid)
{
	AssetServer::Configuration& conf = AssetServer::Configuration::Get();
	DeletedItem item;

	if (GetDeletedItem(guid, item))
	{
		AssetServer::Item sitem = conf.GetServerItem(guid);

		if ((sitem != AssetServer::Item()) && (sitem.parent != kTrashGUID)) // was this item committed to server (and not as a deleted asset)? 
		{
			item.changeset = sitem.changeset;
			item.fullPath = conf.CachedPathFromID(sitem.parentFolderID) + (std::string)sitem.name;
			item.type = sitem.type;
			item.parent = sitem.parent;

			RemoveDeletedItem(guid); // don't allow duplicates

			m_DeletedItems[guid] = item;

			SetDirty();
			return true;
		}
	}

	return false;
}


bool AssetServerCache::IsItemDeleted(const UnityGUID& guid) const
{
	UNITY_MAP(kMemAssetServerCache, UnityGUID, DeletedItem)::const_iterator found = m_DeletedItems.find(guid);
	return found != m_DeletedItems.end();
}

void AssetServerCache::RemoveDeletedItem(const UnityGUID& guid)
{
	UNITY_MAP(kMemAssetServerCache, UnityGUID, DeletedItem)::iterator found = m_DeletedItems.find(guid);

	if (found != m_DeletedItems.end())
	{
		m_DeletedItems.erase(found);
		ASMonoUtility::Get().SetNeedsToRefreshUpdate();
		SetDirty();
	}
}

void AssetServerCache::GetDeletedItems(vector<AssetServerCache::DeletedItem>& items)
{ 
	AssetServer::Configuration& conf = AssetServer::Configuration::Get();
	AssetDatabase& db = AssetDatabase::Get();

	UNITY_MAP(kMemAssetServerCache, UnityGUID, DeletedItem)::iterator next;
	for (UNITY_MAP(kMemAssetServerCache, UnityGUID, DeletedItem)::iterator i = m_DeletedItems.begin(); i != m_DeletedItems.end(); i=next)
	{
		next = i;
		next++;
		DeletedItem item = i->second;

		AssetServer::Item serverItem = conf.GetServerItem(item.guid);

		if (serverItem != AssetServer::Item()) // this file never existed in current project. Most likely user switched Asset Server project
		{
			if ( db.IsAssetAvailable(item.guid) || (serverItem.parent == kTrashGUID))
			{
				m_DeletedItems.erase(i);
				SetDirty();
			}
			else
			{
				items.push_back(item);
			}
		}
	}
}

void AssetServerCache::ClearDeletedItems()
{
	m_DeletedItems.clear();
}

bool AssetServerCache::GetDeletedItem(const UnityGUID& guid, DeletedItem &item)
{
	UNITY_MAP(kMemAssetServerCache, UnityGUID, DeletedItem)::iterator found = m_DeletedItems.find(guid);
	if (found != m_DeletedItems.end())
	{
		AssetServer::Item serverItem = AssetServer::Configuration::Get().GetServerItem(found->second.guid);

		if (serverItem.parent != kTrashGUID)
		{
			item = found->second;
			return true;
		}
		else
			return false;
	}
	return false;
}

IMPLEMENT_OBJECT_SERIALIZE(AssetServerCache)

bool AssetServerCache::ShouldIgnoreInGarbageDependencyTracking ()
{	
	return true;
}

void AssetServerCache::SetCachedDigest (const UnityGUID& guid, const MdFour& digest)  {
	m_Digests.erase(guid);
	m_Digests[guid]=digest;
	SetDirty();
}

void AssetServerCache::RemoveCachedDigest (const UnityGUID& guid)  {
	m_Digests.erase(guid);
	SetDirty();
}

MdFour AssetServerCache::FindCachedDigest (const UnityGUID& guid)  {
	UNITY_MAP(kMemAssetServerCache, UnityGUID, MdFour)::iterator found = m_Digests.find(guid);
	if (found != m_Digests.end() )
	{
		return found->second;
	}
	else
	{
		string pathName = GetAssetPathFromGUID(guid);
		if ( pathName.empty() || ! IsPathCreated(pathName) )
		{
			return MdFour();
		}

		MdFourGenerator gen;
		bool isFolder = IsDirectoryCreated(pathName);
		if (isFolder) 
		{
			gen.Feed("*FOLDER*");
		}
		else
		{
			
			gen.Feed("*FILE*");
			gen.FeedFromFile(pathName);
		}
		if (StrNICmp(pathName.c_str(),"Assets/",7) == 0) {
			string meta = GetTextMetaDataPathFromAssetPath(pathName);
			if ( IsFileCreated(meta) )
				gen.FeedFromFile(meta);
			else // We need a temporary copy of the text meta file to calculate the hash
			{
				gen.Feed(AssetDatabase::Get().GenerateTextMetaData(guid, isFolder));
			}
		}
		MdFour res = gen.Finish();
		m_Digests[guid]=res;	

		SetDirty();
		return res;
	}
}


template<class TransferFunction>
void AssetServerCache::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	AssertIf(transfer.GetFlags() & kPerformUnloadDependencyTracking);
	transfer.Transfer (m_Items, "m_Items");
	TRANSFER( m_DeletedItems );

	TRANSFER( m_LastCommitMessage );
	TRANSFER( m_CommitItemSelection );
	TRANSFER( m_WorkingItemMetaData );
	TRANSFER( m_LatestServerChangeset );

	TRANSFER( m_CachesInitialized );
	TRANSFER( m_ModifiedItems );
}

template<class TransferFunction>
void Item::Transfer (TransferFunction& transfer)
{
	TRANSFER( changeset );
	TRANSFER( guid );
	TRANSFER( name );
	TRANSFER( parent );
	TRANSFER( (int&)type );
	TRANSFER( digest );
	TRANSFER( (int&)origin );
	TRANSFER( oldVersion );
	TRANSFER( parentFolderID );
	TRANSFER( (int&)changeFlags );
}

template<class TransferFunction>
void AssetServerCache::CachedAssetMetaData::Transfer (TransferFunction& transfer)
{
	TRANSFER( guid );
	TRANSFER( pathName );
	TRANSFER( originalChangeset );
	TRANSFER( originalName );
	TRANSFER( originalParent );
	TRANSFER( originalDigest );
}

void AssetServerCache::Item::Reset() 
{ 
	downloadResolution = kDLUnresolved;
	nameConflictResolution = kNMUnresolved;
	markedForRemoval = false;
}


template<class TransferFunction>
void AssetServerCache::Item::Transfer (TransferFunction& transfer)
{

	TRANSFER (markedForRemoval);
	transfer.Align ();
	
 
	// Go to some trouble to transfer enum variables...
	TRANSFER ((int&)downloadResolution);
	TRANSFER ((int&)nameConflictResolution);
}

template<class TransferFunction>
void AssetServerCache::DeletedItem::Transfer (TransferFunction& transfer)
{
	TRANSFER (changeset);
	TRANSFER (guid);
	TRANSFER (parent);
	TRANSFER (fullPath);
	TRANSFER (type);
	TRANSFER (digest);
}

void AssetServerCache::GetWorkingItemChangesets(map<UnityGUID, int> &changesets)
{ 
	if (!InitializeCaches())
		return;

	map<UnityGUID, int>::iterator insertAt = changesets.begin();

	for (UNITY_MAP(kMemAssetServerCache, UnityGUID, CachedAssetMetaData)::iterator i = m_WorkingItemMetaData.begin(); i != m_WorkingItemMetaData.end(); )
	{
		if (!GetGUIDPersistentManager().IsConstantGUID(i->first) && !AssetDatabase::Get().IsAssetAvailable(i->first))
		{
			UNITY_MAP(kMemAssetServerCache, UnityGUID, CachedAssetMetaData)::iterator itmp = i;
			i++;
			m_WorkingItemMetaData.erase(itmp);
			SetDirty();
		}
		else
		{
			insertAt = changesets.insert(insertAt, make_pair(i->first,i->second.originalChangeset));
			i++;
		}
	}
}

AssetServerCache::CachedAssetMetaData* AssetServerCache::FindCachedMetaData(UnityGUID guid)
{
	UNITY_MAP(kMemAssetServerCache, UnityGUID, CachedAssetMetaData)::iterator found = m_WorkingItemMetaData.find(guid);

	if (found == m_WorkingItemMetaData.end())
	{
		UpdateCachedMetaDataItem(guid);
		found = m_WorkingItemMetaData.find(guid);

		if (found == m_WorkingItemMetaData.end())
			return NULL;
	}

	return &found->second;
}

void AssetServerCache::UpdateCachedMetaDataItem(UnityGUID guid)
{
	if (!GetCachesInitialized())
		return;

	AssetMetaData *meta = FindAssetMetaData(guid);
	UNITY_MAP(kMemAssetServerCache, UnityGUID, CachedAssetMetaData)::iterator found = m_WorkingItemMetaData.find(guid);

	if ( meta == NULL ) 
	{
		m_WorkingItemMetaData.erase(guid);
	}
	else if ( found != m_WorkingItemMetaData.end() )
	{
		found->second.pathName = meta->pathName;
		// We deliberately ignore all versioning data in the metadata once we've seen the metadata once
	}
	else
	{
		
		m_WorkingItemMetaData[guid] = CachedAssetMetaData (meta);
	}

	SetDirty();
}

inline void AssetServerCache::AddCachedItem(AssetServer::Item item)
{
	// this actually takes time, so do it here instead of every time when transferring items to mono
	ChangeFlags cf = item.GetChangeFlags();

	if (cf != kCFNone)
	{
		switch (item.GetStatus())
		{
		case AssetServer::kClientOnly:
		case AssetServer::kNewLocalVersion: 
		case AssetServer::kRestoredFromTrash: 
		case AssetServer::kConflict: 
			item.changeFlags = cf;
			item.name = Controller::Get().GetAssetPathName(item.guid);
			m_ModifiedItems[item.guid] = item;
			SetDirty();
			break;
		}
	}
}

void AssetServerCache::UpdateCachedCommitItem(UnityGUID guid)
{
	m_ModifiedItems.erase(guid);
	SetDirty();

	AssetServer::Item item;

	item = Configuration::Get().GetWorkingItem(guid);

	if (item == AssetServer::Item())
		item = Configuration::Get().GetServerItem(guid);

	if (item != AssetServer::Item() && item.parent != kTrashGUID)
		AddCachedItem(item);
}

void AssetServerCache::UpdateCachedItems(const set<UnityGUID> &items)
{
	for (set<UnityGUID>::const_iterator i = items.begin(); i != items.end(); i++)
	{
		UpdateCachedCommitItem(*i);
	}
}

void AssetServerCache::UpdateCachedItems(const map<UnityGUID, string> &items)
{
	for (map<UnityGUID, string>::const_iterator i = items.begin(); i != items.end(); i++)
	{
		UpdateCachedCommitItem(i->first);
	}
}

void AssetServerCache::EraseCachedItems(const set<UnityGUID> &items)
{
	for (set<UnityGUID>::const_iterator i = items.begin(); i != items.end(); i++)
	{
		m_ModifiedItems.erase(*i);
	}
	SetDirty();
}

void AssetServerCache::InvalidateCachedItems()
{ 
	m_ModifiedItems.clear();
	//m_WorkingItemMetaData.clear();
	m_CachesInitialized = false;
	m_LatestServerChangeset = -1;

	ASMonoUtility::Get().SetNeedsToRefreshCommit();
	SetDirty();
};

bool GetChangesProgress(string text, float progress)
{
	if (DisplayProgressbar("Caching", "Building commit item cache. " + text, progress * 0.8f, true) == kPBSWantsToCancel)
		return false;
	return true;
}

bool AssetServerCache::InitializeCaches()
{
	if (GetCachesInitialized())
		return true;

	//
	// working items meta data
	//

	m_WorkingItemMetaData.clear();
	// want to have all assets here, for fast loading of changesets
	vector_set<UnityGUID> guids;
	AssetDatabase::Get().GetAllAssets(guids);

	DisplayProgressbar("Caching", "Building local asset state cache...", 0, true);

	//PersistentManager& manager = GetPersistentManager();

	int total = guids.size();
	int current = 0;
	for (vector_set<UnityGUID>::iterator i = guids.begin(); i != guids.end(); i++)
	{
		if ( *i == UnityGUID (0,0,5,0) ) // ignore deprecated BuildPlayer.prefs asset
			continue; 
		
		AssetMetaData *data;
		int memoryID;

		data = FindAssetMetaDataAndMemoryID(*i, &memoryID);
		if ( data )
			m_WorkingItemMetaData[*i] = CachedAssetMetaData(data);
		current++;

		//TODO: this doesn't seem to free memory
		//manager.MakeObjectUnpersistent(memoryID, kDontDestroyFromFile);

		if (current % 50 == 0)
		{
			if (DisplayProgressbar("Caching", "Building local asset state cache...", (float)current/total, true) == kPBSWantsToCancel)
			{
				LogError("Caching canceled");
				InvalidateCachedItems();
				ClearProgressbar();
				return false;
			}
		}
	}

	//
	// commit items
	//

	vector<AssetServer::Item> changes;
	Configuration::Get().GetChanges(&changes, GetChangesProgress);

	if (DisplayProgressbar("Caching", "Building commit item cache...", 0.8f, true) == kPBSWantsToCancel)
	{
		LogError("Caching canceled");
		InvalidateCachedItems();
		ClearProgressbar();
		return false;
	}

	m_ModifiedItems.clear();

	current = 0;
	total = changes.size();

	for (vector<AssetServer::Item>::const_iterator i = changes.begin(); i != changes.end(); i++)
	{
		AddCachedItem(*i);

		current++;

		if (current % 500 == 0 && DisplayProgressbar("Caching", "Building commit item cache...", 0.8f + current/total/2, true) == kPBSWantsToCancel)
		{
			LogError("Caching canceled");
			InvalidateCachedItems();
			ClearProgressbar();
			return false;
		}
	}

	ClearProgressbar();

	m_CachesInitialized = true;

	SetDirty();
	return true;
}

const UNITY_MAP(kMemAssetServerCache, UnityGUID, AssetServer::Item) & AssetServerCache::GetCachedChanges()
{
	InitializeCaches();
	return m_ModifiedItems;
}

void PostprocessAssetImport (const set<UnityGUID>& refreshed, const set<UnityGUID>& added, const set<UnityGUID>& removed, const map<UnityGUID, string>& moved)
{	
	AssetServerCache& cache = AssetServerCache::Get();
	AssetServer::Controller& maint = AssetServer::Controller::Get();
	set<UnityGUID> reallyRemoved;
	
	for (set<UnityGUID>::const_iterator i = removed.begin(); i != removed.end(); i++) 
	{
		// sometimes we get same assets added and removed at same postprocess call. E.g. EditorUtility.CopySerialized does this (case 353991)
		if (refreshed.find(*i) != refreshed.end() || moved.find(*i) != moved.end())
			continue;

		// removed from cache
		cache.Invalidate(*i);
		cache.UpdateCachedMetaDataItem(*i);
		reallyRemoved.insert(*i);
		cache.RemoveCachedDigest(*i);
	}

	for (map<UnityGUID, string>::const_iterator i = moved.begin(); i != moved.end(); i++) 
	{
		cache.UpdateCachedMetaDataItem(i->first);
	}

	for (set<UnityGUID>::const_iterator i = refreshed.begin(); i != refreshed.end(); i++) 
	{
		cache.RemoveCachedDigest(*i);
		if (! maint.DoesAssetExist(*i))
		{
			if (! cache.IsReceived(*i) ) 
				cache.Invalidate(*i);
		}else
			cache.RemoveDeletedItem(*i); // if deleted item was restored
	}

	cache.FlushReceived();

	// Commit item caching
	ASMonoUtility &monoUtility = ASMonoUtility::Get();

	// no matter which actions happened
	monoUtility.SetNeedsToRefreshCommit();

	// cache Items
	if (cache.GetCachesInitialized())
	{
		cache.UpdateCachedItems(refreshed);
		cache.EraseCachedItems(reallyRemoved);
		cache.UpdateCachedItems(moved);

		CallStaticMonoMethod("ASEditorBackend", "CommitItemsChanged");
	}

}
#undef LogError