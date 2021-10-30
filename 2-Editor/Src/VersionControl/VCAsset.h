#pragma once
#include "Runtime/Mono/MonoTypes.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Utilities/GUID.h"
#include <string>

enum States
{
	kNone = 0,
	kLocal = 1, // Asset in on local disk. If none of kSynced/kOutOfSync flags are set then vcs doesn't know about this local file.
	kSynced = 2, // Asset is known to vcs and in sync locally
	kOutOfSync = 4, // Asset is known to vcs and outofsync locally
	kMissing = 8,
	kCheckedOutLocal = 16,
	kCheckedOutRemote = 32,
	kDeletedLocal = 64,
	kDeletedRemote = 128,
	kAddedLocal = 256,
	kAddedRemote = 512,
	kConflicted = 1024,
	kLockedLocal = 2048,
	kLockedRemote = 4096,
	kUpdating = 8192,
	kReadOnly = 16384,
	kMetaFile = 32768,
	kAny = 0xFFFFFFFF
};

class VCAsset
{
public:

	VCAsset();
	explicit VCAsset(std::string const& clientPath);
	VCAsset(VCAsset const& other);
	const VCAsset& operator=(VCAsset const& rhs);

	void Copy(const VCAsset& other);

	States GetState() const;
	void   SetState(States newState);
	bool   HasState(int state) const { return m_State & state; }
	void   AddState(States state) { m_State = (States) (m_State | state); }
	void   RemoveState(States state) { m_State = (States) (m_State & ~state); };
	bool   IsUnderVersionControl() const { return HasState(kAddedLocal | kSynced | kOutOfSync); }
	
	inline std::string const& GetPath() const {return m_Path;}
	void                      SetPath(std::string const& path);
	std::string GetMetaPath() const;
	std::string GetAssetPath() const;

	std::string GetName() const;
	std::string GetFullName () const;

	void PathAppend(std::string const& append);

	// Test if this path is a child of a given parent
	bool IsChildOf(std::string const& parent) const;
	bool IsChildOf(VCAsset const& parent) const;
	bool IsInCurrentProject() const;
	

	bool IsFolder() const { return m_Path[m_Path.size()-1] == '/'; }
	bool IsMeta() const { return m_Meta; }

	bool operator<(const VCAsset& o) const;
	
	UnityGUID m_guid;

private:
	States m_State;
	std::string m_Path;
	std::string m_Action;
	std::string m_HeadAction;

	bool m_Meta;
};

typedef std::set<VCAsset> VCAssetSet;

void GetAncestorFolders(const VCAsset& asset, VCAssetSet& ancestors);

#include <vector>
class VCAssetList : public std::vector<VCAsset>
{
public:
	
	VCAssetList() {}

	template <typename T1, typename T2>
	VCAssetList(T1 it1, T2 it2) : std::vector<VCAsset>(it1, it2) {}
	
	// Add a VCAsset recursively creating new VCAssets as needed
	void AddRecursive(const VCAsset& asset);
	
	// Replace current list with its corresponding meta file
	void ReplaceWithMeta();

	// Get meta files for current list
	void GetMeta(VCAssetList& result) const;

	// Ensure .meta file for all assets are in the list
	void IncludeMeta();

	// Copy current list into result and include associated .meta files
	void CopyWithMeta(VCAssetList& result) const;
    
	// Get non meta files for current list
	void GetNonMeta(VCAssetList& result) const;

	// Remove meta files from current list
	void RemoveMeta();
	
	// Filter a list of assets by a given set of states
	// All assets that have a state set from includeStates is first selected.
	// From this set all assets that have a state from excludeStates set is
	// removed and the result is put into 'result'.
	void Filter(VCAssetList& result, bool includeFolder, 
				int includeStates, int excludeStates = kNone) const;

	// Same as Filter() but just returns the count
    // TODO: This is called quite often so it may be an idea to cache this
	size_t FilterCount(bool includeFolder, 
					   int includeStates, int excludeStates = kNone) const;

    // Create an optimised list of assets by removing children of folders in the same list
	void FilterChildren(VCAssetList& result) const;
	
	// Ensure that there are specific entries for the folders for the paths
	// in the list e.g. if foo/bar/boo.png is in the list then the 
	// entries foo and bar will be inserted as new entries. No meta files
	// will be created for the new folder entries.
	void IncludeAncestors();
	void GetAncestors(VCAssetSet& result) const;
	
	// Sort this asset list by asset paths
	void SortByPath();

	// Include folders that have their .meta files in this list.
	void IncludeFolders();

	// Remove Duplicate entries from the list
	void RemoveDuplicates();

	// Find the oldest ancestor of the asset in this list or end().
	const_iterator FindOldestAncestor(const VCAsset& asset) const;
	
/*
    // Create an optimised list of assets by removing children of folders in the same list
	public VCAssetList FilterChildren();
*/
};

class VCPluginSession;

VCPluginSession& operator<<(VCPluginSession& p, const VCAsset& v);

VCPluginSession& operator>>(VCPluginSession& p, VCAsset& asset);
