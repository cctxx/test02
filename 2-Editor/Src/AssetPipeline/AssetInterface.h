#ifndef ASSETINTERFACE_H
#define ASSETINTERFACE_H

#include <set>
#include <deque>
#include <map>
#include "Runtime/Utilities/GUID.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "MdFourGenerator.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/NonCopyable.h"
#include "AssetImportingTimerStats.h"
#include "AssetDatabaseStructs.h"
class AssetDatabase;
class Object;
class HierarchyState;
class AssetImporter;

class AssetInterface : NonCopyable
{
	private:

	AssetInterface ();

	public:

	enum OperationStatus { kUserCancelled = false, kPerformed = true };
	enum CancelBehaviour { kNoCancel = 0, kAllowCancel = 1 << 0, kClearQueueOnCancel = 1 << 1 };

	/// If shouldRebuildLibrary is set
	bool Initialize (bool shouldRebuildLibrary, int *prevForwardCompatibleVersion);
	void Shutdown ();

	void RewriteAllSerializedFiles ();
	static AssetInterface& Get ();
//	AssetDatabase& GetAssetDatabase () { return *m_AssetDatabase; }

	UnityGUID CreateFolder(UnityGUID parent, const std::string& newFolderName);

	UnityGUID ImportAtPath (const std::string& path, int options = 0);
	void ImportAsset (UnityGUID guid, int options = 0);
	void ImportAssets (const std::set<UnityGUID>& guids, int options = 0);
	void ImportAssets (const std::vector<UnityGUID>& guids, int options = 0);


	// An alernative that uses precalculated MdFour digests
	void ImportAssets (const std::map<UnityGUID,MdFour>& guids, int options = 0);

	// Use this if you need to Import an asset inside another asset importer.
	// This should be rarely necessary
	UnityGUID ImportAtPathImmediate (const std::string& path, int options = 0);

	bool Refresh (int options = 0);
	void RefreshAndSaveAssets ();
	void SaveAssets ();

	bool MoveToTrash (const UnityGUID& guid);
	bool MoveToTrash (const string& path);
	bool DeleteAsset (const string& path);
	bool CopyAsset (const UnityGUID &guid, const std::string& newPath);

	enum CreateAssetMask { kWriteAndImport = 1 << 0, kCreateUniqueGUID = 1 << 1, kDeleteExistingAssets = 1 << 2, kForceImportImmediate = 1 << 3 };
	bool CreateSerializedAsset (Object& object, const UnityGUID& parentGuid, const std::string& assetName, const std::string& assetExtension, int mode);
	bool CreateSerializedAsset (Object& object, const std::string& uniquePathName, int mask);

	void StartAssetEditing ();
	OperationStatus StopAssetEditing (CancelBehaviour behaviour = kNoCancel);

	typedef void AssetViewerCallback (void* userData);
	void AddAssetViewer (AssetViewerCallback* callback, void* userData) { m_AssetViewerCallbacks.insert (std::make_pair (callback, userData)); }
	void RemoveAssetViewer (AssetViewerCallback* callback, void* userData) { m_AssetViewerCallbacks.erase (std::make_pair (callback, userData)); }

	std::set<UnityGUID> GetUpdateQueue ();

	bool IsLocked () const;

	/// A singleton asset is eg. Library/Tagmanager.asset. They can be queried by their className
	Object* GetSingletonAsset (const std::string& className);
	/// Makes sure that all singleton assets are loaded. If the singleton can't be found it is produced.
	void ReloadSingletonAssets ();

	// Help the progress bar update correctly when driving through ImportAtPathImmediate
	// by telling in advance how many assets we will import.
	void SetMaxProgressImmediateImportAssets (int assets) { m_MaxProgressImmediateImportAssets = assets; }

	string MoveAsset (const UnityGUID& guid, const std::string& newAssetPath);
	string MoveAssets (const std::vector<std::string>& assets, const std::string& newParentPath);
	std::string RenameAsset (const UnityGUID& guid, std::string name);

	void SetGlobalPostprocessFlags (int flags) { m_CurrentPostprocessFlags = flags; }
	int GetGlobalPostprocessFlags () { return m_CurrentPostprocessFlags; }

	Object* GetGUIDSerializer() { return m_GUIDSerializer; }

	void Tick(); // Called from the application tick timer to perform delayed actions
	void RefreshDelayed (int options = 0);

	void GetDirtyAssets (std::set<UnityGUID> &dirtyAssets);
	void SaveNonVersionedSingletonAssets ();

	void WriteRevertAssets (const std::set<UnityGUID> &writeAssets, const std::set<UnityGUID> &revertAssets);

	void ImportAssetsWithMismatchingAssetImporterVersion ();

	private:
	void DownloadQueuedAssetsFromCacheServer ();

	void RefreshOrphans (const UnityGUID& parentGUID, std::set<string >& needRefresh, std::deque<std::string>& updateQueue) ;
	void QueueUpdate (const UnityGUID& guid, const MdFour& forcedHash, int options, AssetImporterVersionHash const& hashes,
		AssetHashes* newAssetHashes = NULL);

	void CheckAssetsChangedActually(std::set<string>* needRefresh, std::set<string>* needAddMetaFile,
		const AssetImporterVersionHash& importerVersionHashes, AssetHashes& newAssetHashes,
		AssetImportingTimerStats& timerStats);

	bool PerformAssetDatabaseConsistencyChecks();
	// Load guid serializer from disk or recreate it from the meta files
	void LoadGUIDSerializer ();
	bool LoadAssetDatabase (bool shouldRebuildLibrary);
	void CleanupCachedAssetFoldersCompletely ();
	void ReSerializeAssetsIfNeeded (const std::set<UnityGUID> &guids);

	AssetDatabase*	m_AssetDatabase;
	Object* m_GUIDSerializer;

	struct SerializedSingleton
	{
		Object* ptr;
		std::string path;
		std::string className;
		UnityGUID guid;
		bool shouldSerialize;
	};
	typedef std::vector<SerializedSingleton> SerializedSingletons;
	SerializedSingletons m_SerializedSingletons;
	/// A serialized singleton is produced with className on startup and persistet in path.
	/// If guid is not UnityGUID() the file will be treated as an Asset (It will have a guid, a meta data and be visible in the assetdatabase)
	/// Serialized Singletons are placed inside the Library folder (eg. Library/TagManager.asset)
	/// if className is "" the file will treated as a non-serialized asset
	void AddSerializedSingleton (const std::string& className, const std::string& path, UnityGUID guid = UnityGUID ());

	void RefreshSerializedSingletons (std::set<string>* needRefresh, AssetDatabase* database, int options);

	std::set<int> m_SelectionBackup;

	static void UpdateProgress (float individualAssetProgress, float totalOverrideProgress, const std::string& customTitle);

	struct RefreshAsset
	{
		int         importerClassID;
		UInt32      importerVersionHash;
		int         queue; // AssetImporters define a order in which assets are imported. (Eg. textures & materials are imported before fbx files)
		int         subQueue; // Used for deterministic import order when the queue matches (uses hash of the path)
		UnityGUID   guid;
		MdFour		hash;
		bool		recomputeHash;
		MdFour      forcedHash; // Hack used for asset server for a maintainable hash
		int options;
		std::map<UnityGUID, std::multiset<RefreshAsset>::iterator>::iterator link;

		friend bool operator < (const RefreshAsset& lhs, const RefreshAsset& rhs)
		{
			if (lhs.queue != rhs.queue)
				return lhs.queue < rhs.queue;
			else
				return CompareGUIDStringLess(lhs.guid, rhs.guid);
		}

		RefreshAsset()
			: importerClassID(0),
			importerVersionHash(0),
			queue(0),
			subQueue(0),
			guid(),
			hash(),
			recomputeHash(false),
			forcedHash(),
			options(0)
		{
		}

		// NB: xcode5 new clang auto-generate bad copy ctor, which crashes in AssetInterface::ProcessAssetsImplementation
		// when you do RefreshAsset asset = *m_RefreshQueue.begin(); [stack corruption]
		// this comment can be removed when we fully switch to new clang (read: never? ;-))
		RefreshAsset(const RefreshAsset& other)
		  : importerClassID(other.importerClassID),
			importerVersionHash(other.importerVersionHash),
			queue(other.queue),
			subQueue(other.subQueue),
			recomputeHash(other.recomputeHash),
			options(other.options)
		{
			guid		= other.guid;
			hash		= other.hash;
			forcedHash	= other.forcedHash;
			link		= other.link;
		}
	};

	typedef std::multiset<RefreshAsset> RefreshQueue;
	typedef std::map<UnityGUID, RefreshQueue::iterator> GUIDToRefreshQueue;

	void ComputeHashes(RefreshQueue& assetsToCompute);

	OperationStatus ProcessAssetsImplementation (double& time, CancelBehaviour cancelBehaviour);
	void ApplyDefaultPostprocess ();

	// Test if we have both scripts and other assets in the refresh queue and if switching is allowed
	bool CanSwitchToSynchronousImport() const;
	// Switch to synchronous import mode
	void SwitchToSynchronousImport();

	GUIDToRefreshQueue m_GUIDToRefreshQueue;
	RefreshQueue m_RefreshQueue;
	int          m_MaxProgressImmediateImportAssets;
	int          m_PostprocessedRefreshedAssets;
	int          m_CurrentPostprocessFlags;
	bool		 m_CanCancelAssetQueueProcessing;

	std::set<UnityGUID> m_DidRefresh;
	std::set<UnityGUID> m_DidRemove;
	std::set<UnityGUID> m_DidAdd;
	// GUID -> previousPathName
	std::map<UnityGUID, string> m_DidMove;
	double         m_BeginAssetProgressTime;
	typedef std::set<std::pair<AssetViewerCallback*, void*> > AssetViewerCallbacks;
	AssetViewerCallbacks m_AssetViewerCallbacks;

	int m_DelayedRefreshRequested;
	int m_DelayedRefreshOptions;

	AssetImportingTimerStats m_TimerStats;
};
ENUM_FLAGS(AssetInterface::CancelBehaviour);


extern const char* kExpandedItemsPath;
extern const char* kProjectSettingsPath;
extern const char* kEditorSettingsPath;
extern const char* kGraphicsSettingsPath;
HierarchyState& GetProjectWindowHierarchyState ();
HierarchyState* GetProjectWindowHierarchyStateIfLoaded ();

std::string GenerateReusableAssetPathName (const UnityGUID& parentGuid, const std::string& name, const std::string& extension);

#endif
