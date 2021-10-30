#pragma once
#include "VCAsset.h"
#include "Runtime/Utilities/GUID.h"

#include <list>
#include <map>
#include <set>
#include <string>

class VCTask;

//// Version control database cache.  This is the central point of asset status used by the project window
//// although it can be used by anything.  It works by caching requests and then refreshing the status 
//// when there are pending operations every update.
////
//// Callbacks to be notified of invalidations or refreshes are provided.  During an invalidation the assets
//// are removed from the cache and listeners notified.  If they request the state of the asset it is
//// placed on the pending queue ready for the next tick.  On the next tick pending assets are refreshed 
//// and listeners are notified of this.
////
//// Allowing systems to invalidate a sub set of the assets helps this work optimally but we need to keep a
//// close look out for bugs where items failed to update.

class VCCache
{
	enum Validity 
	{
		kVValid,   // The cached asset hasn't been invalidated by a call to VCCache::Invalidate()
		kVInvalid, // Cached asset is invalid and new status task should be queued
		kVUpdating // New status is being fetched from backend
	};
	
	struct VCCacheEntry
	{
		VCCacheEntry() : validity(kVInvalid) {}
		VCCacheEntry(const VCAsset& a, Validity v) : asset(a), validity(v) {}
		VCAsset asset;
		Validity validity;
	};

	typedef std::map<UnityGUID, VCCacheEntry> GUID2VCAssetMap;
	typedef std::set<UnityGUID> GUIDSet;
public:
	VCCache();
	~VCCache();

	void GetSelection(VCAssetList& selection);
	bool GetAssetByPath(std::string const& unityPath, VCAsset& vcAsset, bool allowBackendQuery = true);
	bool GetAssetByGUID(UnityGUID const& guid, VCAsset& vcAsset, bool allowBackendQuery = true);

	// Will start async fetch from backend plugins of previously returned asset from VCCache 
	// (e.g. using GetAssetByXXXX()) that was not in cache. 
	// A delay between fetching from backend plugin is used so calling tick rapidly will not
	// create a backend fetch for each call. A backend fetch can be force by setting forcePending
	// to true.
	VCTask* Tick(bool forcePending = false);

	// Empties the cache altogether
	void Clear() { m_Database.clear(); }
	
	// Marks all entries as invalid which will trigger a refetch
	// on next access but still return the cached entry immediately.
	void Invalidate();

	static void StatusCallback(VCTask* task);

	// Callback to keep modified flag up to date
	void SetDirty(UnityGUID guid);
	
	friend VCCache& GetVCCache();

private:
	VCAsset& CreateNewVCAssetEntry (std::string const& unityPath, UnityGUID guid);
	void OnStatus(VCTask* task);

	double m_lastUpdate;

	VCAssetList m_Pending;
	GUID2VCAssetMap m_Database;

	static VCCache* instance;
};

VCCache& GetVCCache();

