#include "UnityPrefix.h"
#include "AssetInterface.h"
#include "AssetModificationCallbacks.h"
#include "Runtime/Utilities/CStringHash.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Misc/GarbageCollectSharedAssets.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "AssetDatabase.h"
#include "Runtime/Graphics/Transform.h"
#include "AssetMetaData.h"
#include "Editor/Src/EditorSettings.h"
#include "Editor/Src/EditorUserSettings.h"
#include "Editor/Src/EditorBuildSettings.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Runtime/Misc/PreloadManager.h"
#include "NativeFormatImporter.h"
#include "Editor/Src/HierarchyState.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "LibraryAssetImporter.h"
#include "Configuration/UnityConfigure.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Mono/MonoManager.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Editor/Platform/Interface/AssetProgressbar.h"
#include "Editor/Platform/Interface/BugReportingTools.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/SceneInspector.h"
#include "Editor/Src/Selection.h"
#include "Editor/Src/Utility/MiniAssetIconCache.h"
#include "Editor/Src/Utility/AssetPreviews.h"
#include "Editor/Platform/Interface/RefreshAssets.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/BaseClasses/ManagerContextLoading.h"
#include "Runtime/Misc/SaveAndLoadHelper.h"
#include "BumpMapSettings.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Terrain/Heightmap.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "ShaderImporter.h"
#include "CacheServer/CacheServerCommunicationNodeJS.h"
#include "Editor/Src/File/FileScanning.h"
#include "AssetImportState.h"
#include "MetaFileUtility.h"
#include "DefaultImporter.h"
#include <time.h>
#include "AssetImporterUtil.h" ///@TODO: GetActiveBuildTargetSelectionForAssetImport() should be moved into a seperate file... Do it after aras merges gles emu kill
#include "AssetPathUtilities.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Editor/Src/EditorAssetGarbageCollectManager.h"
#include "AssetImporterPrefabQueue.h"
#include "Editor/Src/VersionControl/VCProvider.h"
#include "Runtime/Misc/ResourceManagerGUIDs.h"
#include "Runtime/Graphics/SubstanceSystem.h"
#include "Runtime/Dynamics/TerrainCollider.h"
#include "AssetHashing.h"

using namespace std;

static int CalculateMostImportantObject (const vector<LibraryRepresentation>& objects);
static int DefaultImporterCanLoadPathName (const string& pathName, int* queue);
void RebuildGUIDCache (const vector_set<UnityGUID>& metaDataGUIDsOnDisk);
static void SynchronizeMeshRenderersAndMaterials ();

static int gLock = 0;
static const char* kAssetDatabasePath = "Library/assetDatabase3";
static const char* kGUIDMapperPath = "Library/guidmapper";

static const char* kAssetsPath = "Assets";
static const char* kMetaDataPath = "Library/metadata";
static const char* kCacheDataPath = "Library/cache";
const char* kExpandedItemsPath = "Library/expandedItems";
const char* kProjectSettingsPath = "ProjectSettings/ProjectSettings.asset";
const char* kEditorSettingsPath = "ProjectSettings/EditorSettings.asset";
const char* kGraphicsSettingsPath = "ProjectSettings/GraphicsSettings.asset";

enum { kExpandedItemsFileID = 1 };

const float kSaveAssetsDuringLongImportTimeout = 30.0F;


static const char* kLibraryPath = "Library";
static const char* kProjectSettingsFolder = "ProjectSettings";
static const char* kSourceAssets = "Assets";
static AssetInterface* gAssetInterface = NULL;

AssetInterface& AssetInterface::Get ()
{
	if (gAssetInterface == NULL)
		gAssetInterface = new AssetInterface ();
	return *gAssetInterface;
}

AssetInterface::AssetInterface ()
	:	m_AssetDatabase (NULL)
	,	m_MaxProgressImmediateImportAssets (0)
	,	m_PostprocessedRefreshedAssets (0)
	,	m_CurrentPostprocessFlags (0)
	, 	m_DelayedRefreshRequested (0)
	,	m_DelayedRefreshOptions (0)
	,	m_CanCancelAssetQueueProcessing(false)
{
	AssetDatabase::SetAssetDatabase (NULL);
}

UnityGUID AssetInterface::CreateFolder (UnityGUID parent, const std::string& newFolderName)
{
	std::string newPathName = GetGUIDPersistentManager ().GenerateUniqueAssetPathName (parent, newFolderName);
	if (!newPathName.empty() && CreateDirectory (newPathName))
		return ImportAtPath (newPathName);

	ErrorString("Failed to create folder");
	return UnityGUID();
}

void AssetInterface::RefreshSerializedSingletons (set<string>* needRefresh, AssetDatabase* database, int options)
{
	bool visibleMetaFile = GetEditorSettings().GetVersionControlRequiresMetaFiles();

	for (int i=0;i<m_SerializedSingletons.size ();i++)
	{
		if (m_SerializedSingletons[i].guid != UnityGUID ())
		{
			DateTime contentModificationDate = GetContentModificationDate (m_SerializedSingletons[i].path);
			UnityGUID curGUID = m_SerializedSingletons[i].guid;

			// Detect if we need to reimport?
			if (database->DoesAssetFileNeedUpdate(m_SerializedSingletons[i].path, contentModificationDate, false, false) )
				needRefresh->insert (m_SerializedSingletons[i].path);

			// make sure we don't force a reimport due to a missing meta file
			database->ForceMarkAsFoundMetaFile (m_SerializedSingletons[i].path, !visibleMetaFile);
		}
	}
}

bool AssetInterface::IsLocked () const
{
	return gLock;
}

void AssetInterface::StartAssetEditing ()
{
	//FatalErrorIf (gLock != 0);
	gLock++;
	if (gLock != 1)
		return;

	m_TimerStats.completeAssetImportTime = START_TIME;
	m_TimerStats.cacheServerRequestedAssets = 0;
	m_TimerStats.cacheServerNotSupportedAssets = 0;
	m_TimerStats.cacheServerDownloadedBytes = 0;
	m_TimerStats.cacheServerFoundAssets = 0;
	m_TimerStats.cacheServerUnavailableAssets = 0;
	
	ABSOLUTE_TIME_INIT (m_TimerStats.cacheServerIntegrateTime);
	ABSOLUTE_TIME_INIT (m_TimerStats.cacheServerMoveFilesTime);
	ABSOLUTE_TIME_INIT (m_TimerStats.cacheServerDeleteFilesTime);
	
	GetPreloadManager().LockPreloading();

	// The library view checks DidChangeAssetsGraphically inside
	// the assetviewcallback to see if it really needs to redraw!
	m_AssetDatabase->ClearDidChangeAssetsGraphically ();
	m_AssetDatabase->RefreshAssetImporterVersionHashesCache();
	
	// Disallow updating anything while importing / refreshing
	m_CanCancelAssetQueueProcessing = false;
	GetApplication().DisableUpdating();

	BeginBusyCursor();

	// Remember selection
	m_SelectionBackup = Selection::GetSelectionID();
	
	m_BeginAssetProgressTime = GetTimeSinceStartup();
}

namespace
{
	void SynchronizeTerrain()
	{
		vector<Object*> filters;
		Object::FindObjectsOfType (Object::StringToClassID ("TerrainCollider"), &filters);
		for (vector<Object*>::iterator i = filters.begin(); i != filters.end(); i++)
		{
			TerrainCollider* tc = static_cast<TerrainCollider*>(*i);
			if (!tc->HasShape())
				tc->AwakeFromLoad (kDefaultAwakeFromLoad);
		}

		// Is this really necessary
		CallStaticMonoMethod ("Terrain", "ReconnectTerrainData");
	}
}

struct SortHashingRequestByAssetPath
{
	bool operator () (const HashAssetRequest& lhsRequest, const HashAssetRequest& rhsRequest) const
	{
		return lhsRequest.assetPath < rhsRequest.assetPath;
	}
};

bool IsAssetFileSupported(int importerClassID)
{
	if (importerClassID == ClassID(LibraryAssetImporter) 
		|| importerClassID == ClassID(DefaultImporter)
		|| importerClassID == ClassID(Object)
		)
		return false;

	return true;
}

void AssetInterface::ComputeHashes(RefreshQueue& assetsToCompute)
{
	if (assetsToCompute.size() <= 0)
		return;

	// Get the list that we need to generate the hashes.
	vector<HashAssetRequest> needComputeHash;
	
	for(RefreshQueue::iterator it = assetsToCompute.begin() ; it != assetsToCompute.end() ; it++)
	{
		const RefreshAsset& asset = *it;

		// Only compute for the asset that is marked as recompute.
		if (!asset.recomputeHash)
			continue;

		string assetPath = GetGUIDPersistentManager ().AssetPathNameFromGUID (asset.guid);

		if ( !IsFileCreated (GetTextMetaDataPathFromAssetPath(assetPath))
			|| !IsAssetFileSupported(asset.importerClassID) )
			continue;

		HashAssetRequest request(asset.guid, assetPath, asset.importerClassID, asset.importerVersionHash);
		needComputeHash.push_back(request);
	}

	if (needComputeHash.size() <= 0)
		return;

	// Try to make the hashing walk all data in a somewhat linear order
	SortHashingRequestByAssetPath sortByAssetPath;
	sort (needComputeHash.begin(), needComputeHash.end(), sortByAssetPath);

	// Compute hashes.
	::ProcessHashRequests(needComputeHash, GetEditorUserBuildSettings().GetActiveBuildTargetSelection());

	for (vector<HashAssetRequest>::iterator it = needComputeHash.begin(); it != needComputeHash.end() ; it++)
	{
		HashAssetRequest& request = *it;

		// Find the RefreshAsset and set hash to it.
		for(RefreshQueue::iterator it = assetsToCompute.begin() ; it != assetsToCompute.end() ; it++)
		{
			const RefreshAsset& asset = *it;

			if(asset.guid == request.guid)
			{
				const_cast<RefreshAsset&>(asset).hash = request.hash;
				break;
			}
		}
	}// end outer for.
}

void AssetInterface::DownloadQueuedAssetsFromCacheServer ()
{
	if (!IsConnectedToCacheServer())
		return;
		
	vector<CachedAssetRequest> requests;
	requests.reserve(m_RefreshQueue.size());
	
	for (RefreshQueue::iterator i=m_RefreshQueue.begin(); i != m_RefreshQueue.end();++i)
	{
		// Do the early reject for kRefreshTextMetaFile as well.
		if ( i->options & kDontImportWithCacheServer || i->options & kRefreshTextMetaFile)
			continue;
		
		requests.push_back(
			CachedAssetRequest(i->guid, GetAssetPathFromGUID(i->guid), i->importerClassID, i->hash)
			);
	}
	
	GetCacheServer().DownloadCachedAssets(&requests[0], requests.size(), &m_TimerStats);
}

bool AssetInterface::CanSwitchToSynchronousImport() const
{
	bool hasOtherThanScripts = false;
	for (RefreshQueue::iterator i = m_RefreshQueue.begin(); i != m_RefreshQueue.end(); ++i)
	{
		if (i->importerClassID != ClassID(MonoImporter))
			hasOtherThanScripts = true;

		if ( !(i->options & kAllowForceSynchronousImport) )
			return false;
	}

	return hasOtherThanScripts;
}

void AssetInterface::SwitchToSynchronousImport()
{
	for (RefreshQueue::iterator i = m_RefreshQueue.begin(); i != m_RefreshQueue.end(); ++i)
	{
		// Changing the options does not change the sorting of the multiset so it is "ok" to do a const cast here
		RefreshAsset& a = const_cast<RefreshAsset&>(*i);
		a.options |= kForceSynchronousImport;
	}
}


AssetInterface::OperationStatus
AssetInterface::StopAssetEditing (AssetInterface::CancelBehaviour cancelBehaviour)
{
	FatalErrorIf (gLock <= 0);
	// Early out if we aren't done yet!
	if (gLock > 1)
	{
		gLock--;
		return kPerformed;
	}

	// If allowed switch to synchronous import mode, to make sure mono behaviors are known before importing other assets
	if (CanSwitchToSynchronousImport())
	{
		SwitchToSynchronousImport();
	}

	// Recompute the hashes if necessary before downloading from cache server.
	ComputeHashes(m_RefreshQueue);

	///TODO: This is a hack. Figure out out how to handle commit to cache server properly.
	bool didConnect = false;
	if (!m_RefreshQueue.empty())
	{
		ConnectToCacheServer();
		DownloadQueuedAssetsFromCacheServer ();
		didConnect = true;
	}
	
	double startTime = GetTimeSinceStartup();

	// This processes all queued assets!
	m_TimerStats.assetImporting = START_TIME;
	OperationStatus opStatus = ProcessAssetsImplementation (startTime, cancelBehaviour);
	m_TimerStats.assetImporting = ELAPSED_TIME(m_TimerStats.assetImporting);
	
	if (didConnect)
	{
		///TODO: This is a hack. Figure out out how to handle commit to cache server properly.
		GetCacheServer().ClearCachedAssetRequests ();
		DisconnectFromCacheServer();
	}
	
	gLock--;
	
	// Remove progress bar	
	ClearAssetProgressbar();

	// Rebuild resources ptrs since that might have changed
	GetResourceManager().SetResourcesNeedRebuild();
	
	// Because we don't do any proper dependency calculation prior to importing shaders,
	// we need to reparse all shaders so that all dependencies can be found.
	ShaderImporter::ReloadAllShadersAfterImport ();

	// When rebuilding assets mesh renderers lose their direct pointer.
	// Also materials might have built bad property sheets (i.e. textures not loaded yet),
	// So we need to recache that.
	SynchronizeMeshRenderersAndMaterials ();

	// We need to reconnect Terrain and TerrainColliders to TerrainData
	SynchronizeTerrain();
	
	// Imported prefabs are applied delayed since they get unloaded during import.
	AssetImportPrefabQueue::Get().ApplyMergePrefabQueue();
	
	// Restore selection from before asset import.
	// While importing assets might get destroyed and then recreated.
	// However EditorController removes them from the selection as soon as they get destroyed.
	GetSceneTracker().SetSelectionID (m_SelectionBackup);
	m_SelectionBackup.clear ();

	// Update views so that we make sure that we get all pipelines executed before any sceneviews draw.
	// We have to flush dirty before since if prefabs changed they will be dirty and thus add/remove
	// themselves from the scene spatial index which makes them appear only after the next pipeline reexecute again
	GetSceneTracker().FlushDirty();
	
	// Update asset views
	// Has to be done after FlushDirty because kDelayRebuildTree is picked up in DidFlushDirty of librarycontroller.
	// Which would make the kDelayRebuildTree happen inside FlushDirty.
	for (AssetViewerCallbacks::iterator i=m_AssetViewerCallbacks.begin ();i != m_AssetViewerCallbacks.end ();i++)
		i->first (i->second);
	if (m_AssetDatabase->DidChangeAssetsGraphically ())	
	{
		GetSceneTracker().ProjectWindowHasChanged();
		GetSceneTracker().ForceReloadInspector();
	}
	
	GetPreloadManager().UnlockPreloading();
	
	// Enable updating and display in order to run execute all dirty filters
	// before a scene view might possibly get drawn
	// Update if needs is called implicitly
	GetApplication().EnableUpdating(true);

	ForceCloseAllOpenFileCaches ();
	
	EndBusyCursor();
		
	m_MaxProgressImmediateImportAssets = 0;
	m_PostprocessedRefreshedAssets = 0;

	// don't perform bumpmap fixing when loading application
	if (GetApplication().IsInitialized())
		BumpMapSettings::Get().PerformUnmarkedBumpMapTexturesFixing();
	
	m_TimerStats.completeAssetImportTime = ELAPSED_TIME(m_TimerStats.completeAssetImportTime);

	float mbPerSecond = m_TimerStats.cacheServerDownloadedBytes / ((double)AbsoluteTimeToSeconds(m_TimerStats.cacheServerDownloading) * 1024 * 1024);
	float hashedMbPerSecond = m_TimerStats.cacheServerHashedBytes / ((double)AbsoluteTimeToSeconds(m_TimerStats.cacheServerHashing) * 1024 * 1024);
	printf_console("\n----- Total AssetImport time: %fs, Asset Import: %fs, CacheServerIntegrate: %fs, CacheServer Download: %fs [%s, %f mb/s], CacheServer Hashing: %fs [%s, %f mb/s]\n",
		AbsoluteTimeToSeconds(m_TimerStats.completeAssetImportTime), AbsoluteTimeToSeconds(SubtractAbsoluteTimeClamp(m_TimerStats.assetImporting, m_TimerStats.cacheServerIntegrateTime)), AbsoluteTimeToSeconds(m_TimerStats.cacheServerIntegrateTime), AbsoluteTimeToSeconds(m_TimerStats.cacheServerDownloading), FormatBytes(m_TimerStats.cacheServerDownloadedBytes).c_str(), mbPerSecond, AbsoluteTimeToSeconds(m_TimerStats.cacheServerHashing), FormatBytes(m_TimerStats.cacheServerHashedBytes).c_str(), hashedMbPerSecond);

	if (m_TimerStats.cacheServerRequestedAssets != 0)
	{
		printf_console("----- Cache Server detail: Total assets requested: %d, Cached Assets: %d, Not available: %d, Not supported: %d.\n", m_TimerStats.cacheServerRequestedAssets, m_TimerStats.cacheServerFoundAssets, m_TimerStats.cacheServerUnavailableAssets, m_TimerStats.cacheServerNotSupportedAssets);
		printf_console("----- Cache Server detail: CacheMove: %fs, CacheDelete: %fs.\n", AbsoluteTimeToSeconds(m_TimerStats.cacheServerMoveFilesTime), AbsoluteTimeToSeconds(m_TimerStats.cacheServerDeleteFilesTime));
	}
	printf_console("\n");
	
	return opStatus;
}

void AssetInterface::RefreshOrphans (const UnityGUID& parentGUID, std::set<std::string >& needRefresh, std::deque<std::string>& updateQueue) 
{
	const Asset* asset = m_AssetDatabase->AssetPtrFromGUID(parentGUID);
	if( NULL != asset && ! asset->children.empty() ) 
	{
		for( vector<UnityGUID>::const_iterator j= asset->children.begin() ; j != asset->children.end(); ++j)
		{
			string path = GetGUIDPersistentManager ().AssetPathNameFromGUID(*j);
			if (needRefresh.end() == needRefresh.find(path))
			{
				needRefresh.insert(path);
				updateQueue.push_back(path);
			}
		}
	}
}

void AssetInterface::CheckAssetsChangedActually(set<string>* needRefresh, set<string>* needAddMetaFile, 
	const AssetImporterVersionHash& importerVersionHashes, AssetHashes& newAssetHashes,
	AssetImportingTimerStats& timerStats)
{
	// Get the list that we need to generate the hashes.
	vector<HashAssetRequest> needCheckHash;
	for (set<string>::iterator i= needRefresh->begin () ; i !=  needRefresh->end () ; i++)
	{
		string assetPath = *i;

		// Skip the asset folder itself.
		if ( strcmp(assetPath.c_str(), kAssetsPath) == 0 )
			continue;

		// If the asset is also in the needAddMetaFile list, we need to update it anyway.
		if( needAddMetaFile->find(assetPath) != needAddMetaFile->end() ||
			!IsFileCreated (GetTextMetaDataPathFromAssetPath(assetPath) )
			)
			continue;

		AssetImporterSelection importerSelection = AssetImporter::FindAssetImporterSelection (assetPath);

		if ( !IsAssetFileSupported(importerSelection.importerClassID) )
			continue;

		const UnityGUID guid = GetGUIDPersistentManager().GUIDFromAnySerializedPath(assetPath);

		if ( IsAssetImporterDirty(guid) )
			continue;

		UInt32 importerVersionHash = 0;
		if (importerVersionHashes.size() > 0)
		{
			Assert(importerVersionHashes.find(importerSelection.importerClassID) != importerVersionHashes.end());
			importerVersionHash = importerVersionHashes.find(importerSelection.importerClassID)->second;
		}
		else
			importerVersionHash = HashImporterVersion(importerSelection.importerClassID, importerSelection.importerVersion);

		HashAssetRequest request(guid, assetPath, importerSelection.importerClassID, importerVersionHash);
		needCheckHash.push_back(request);
	}

	if (needCheckHash.size() <= 0)
		return;

	// Try to make the hashing walk all data in a somewhat linear order
	SortHashingRequestByAssetPath sortByAssetPath;
	sort (needCheckHash.begin(), needCheckHash.end(), sortByAssetPath);

	// Compute hashes.
	timerStats.cacheServerHashing = START_TIME;
	timerStats.cacheServerHashedBytes = ::ProcessHashRequests(needCheckHash, GetEditorUserBuildSettings().GetActiveBuildTargetSelection());
	timerStats.cacheServerHashing = ELAPSED_TIME(timerStats.cacheServerHashing);
	
	printf_console( "-----------Compute hash(es) for %d asset(s).\n", needCheckHash.size() );

	// Check if the asset is really changed by comparing hashes.
	for (vector<HashAssetRequest>::iterator it = needCheckHash.begin(); it != needCheckHash.end() ; it++)
	{
		HashAssetRequest& request = *it;

		if (!m_AssetDatabase->IsAssetAvailable(request.guid))
		{
			newAssetHashes[request.assetPath] = request.hash;
			continue;
		}

		const Asset& asset = m_AssetDatabase->AssetFromGUID(request.guid);

		if ( asset.hash == request.hash)
		{
			// If the content is not changed, remove it from needRefresh.
			needRefresh->erase(request.assetPath);

			// Update the time stamp to avoid it being put in the needRefresh again.
			m_AssetDatabase->UpdateAssetTimeStamp(request.assetPath);

			printf_console( "-----------Asset named %s is skipped as no actual change.\n", request.assetPath.c_str() );
		}
		else
		{
			// Add to the new hash map.
			newAssetHashes[request.assetPath] = request.hash;
		}
	}

	printf_console("\n");
}

bool AssetInterface::Refresh (int options)
{
	options |= kImportAssetThroughRefresh;

	GUIDPersistentManager& pm =GetGUIDPersistentManager();
	
	ABSOLUTE_TIME refreshStart = START_TIME;
	printf_console("Refresh, detecting if any assets need to be imported or removed ... ");
	
	string newProjectPath = GetCocoaCurrentDirectory();
	if (StrICmp (File::GetCurrentDirectory(), newProjectPath) != 0)
	{
		string error;
		// the project has been deleted and restored (with e.g. Time Machine)
		if (newProjectPath.empty() && IsDirectoryCreated(File::GetCurrentDirectory()))
			error = "The whole project folder has been changed externally. This is not allowed!";
		// working directory changed
		else
			error = Format( "The current working directory was changed from your Unity project folder located at '%s' to '%s'. This is not allowed! If you are setting the current working directory from script temporarily, then please make sure to set it back to the Unity project folder immediately after you are done.\n", File::GetCurrentDirectory ().c_str (), newProjectPath.c_str ());
		FatalErrorStringDontReport (error);
	}

	m_AssetDatabase->ResetRefreshFlags();
	
	int metaFileCount = 0;
	set<string> needRefresh;
	set<string> needRefreshMeta;
	set<string> needRemoveMetaFile;
	
	// Refresh Assets folder itself since that is not included in the recursive refresh
	m_AssetDatabase->ForceMarkAsFoundMetaFile (kAssetsPath, !GetEditorSettings().GetVersionControlRequiresMetaFiles());
	if (m_AssetDatabase->DoesAssetFileNeedUpdate(GetActualPathSlow(kAssetsPath), GetContentModificationDate (kAssetsPath), true, false) )
		needRefresh.insert (kAssetsPath);

	// Refresh all paths inside the assets folder
	RecursiveRefresh (GetActualPathSlow(kAssetsPath).c_str(), &needRefresh, &needRefreshMeta, &needRemoveMetaFile, m_AssetDatabase, &metaFileCount, options);

	// Autodetect whether external version control support should be enabled or not if it's currently set to kAutoDetect
	if( GetEditorSettings().GetExternalVersionControlSupport() == ExternalVersionControlAutoDetect ) 
	{
		// TODO: Detect P4 Working Directory

		if (IsDirectoryCreated(".svn"))
			GetEditorSettings().SetExternalVersionControlSupport("Subversion");

		else
		// If there are no *.meta files in the asset folder, disable external version control support
		if ( metaFileCount == 0 ) 
			GetEditorSettings().SetExternalVersionControlSupport(ExternalVersionControlHiddenMetaFiles);
		// otherwise, enable it
		else
			GetEditorSettings().SetExternalVersionControlSupport(ExternalVersionControlVisibleMetaFiles);
		
		// Save the changes to the EditorSettings straight away so the changes will persist after a later asset reload
		if (GetPersistentManager().TestNeedWriteFile(kEditorSettingsPath))
			GetPersistentManager().WriteFile(kEditorSettingsPath);
	}
	
	// Refresh serialized singletons (eg. Library/InputManager.asset)
	RefreshSerializedSingletons (&needRefresh, m_AssetDatabase, options);

	set<string> needRemoval;
	set<string> needAddMetaFile;
	
	m_AssetDatabase->GetNeedRemovalAndAddMetaFile (&needRemoval, &needAddMetaFile);

	// Early out, there is nothing to do.
	if (needRefreshMeta.empty() && needRefresh.empty () && needRemoval.empty () && needRemoveMetaFile.empty() && needAddMetaFile.empty() )
	{
		printf_console("%f seconds (Nothing changed)\n", GetElapsedTimeInSeconds(refreshStart));
		return false;
	}
	
	StartAssetEditing ();

	// needRefreshMeta files should be added to needRefresh in most cases
	// except for some corner cases where case sensitivity does not match
	for( set<string>::iterator i=needRefreshMeta.begin () ; i !=  needRefreshMeta.end () ; i++ )
	{
		const std::string& assetPath = *i;
		// We are not supposed to refresh the asset and the asset is not known in the timestamps -> The asset does not exist
		// But the meta file has been determined as changed -> the meta file does exist
		// ---> We can assume that the meta file needs to be either removed or moved due to it's upper / lower case being incorrect.
		if (needRefresh.count(assetPath) == 0 && !m_AssetDatabase->AssetTimeStampExistsAndRefreshFoundAsset(assetPath))
		{	
			// The asset at that path does exist
			// This means we have a upper case / lower case issue with the meta file.
			// -> Lets move the file to have correct case
			if (IsPathCreated (assetPath))
			{	
				// Doing this detection is kind of ugly.
				string assetPath = GetActualPathSlow(*i);
				string assetMetaPath = GetTextMetaDataPathFromAssetPath(assetPath);
				string actualAssetMetaPath = GetActualPathSlow(assetMetaPath);
				
				if (actualAssetMetaPath != assetMetaPath)
				{
					if (MoveFileOrDirectory (actualAssetMetaPath, assetMetaPath))
					{
						WarningString(Format("Renamed .meta file '%s' to '%s' because the .meta file had an inconsistent case.", actualAssetMetaPath.c_str(), GetLastPathNameComponent(assetMetaPath).c_str()));
					}
					else
					{
						WarningString(Format("Failed to rename .meta file '%s' to '%s' because the .meta file had an inconsistent case. Please rename it to: '%s'", actualAssetMetaPath.c_str(), GetLastPathNameComponent(assetMetaPath).c_str(), GetLastPathNameComponent(assetMetaPath).c_str()));
					}
					needRefresh.insert(assetPath);
				}
				else
				{
					needRefresh.insert(assetPath);
					ErrorString(Format("Refresh case sensitivity failure. Asset: %s ; Metafile: %s", assetPath.c_str(), actualAssetMetaPath.c_str()));
				}
			}
			// -> The asset really does not exist. We can remove the meta file.
			else
			{
				AssertString(Format ("Removing %s because the asset does not exist", assetPath.c_str()));
				needRemoveMetaFile.insert(assetPath);
			}
		}
		else
			needRefresh.insert(assetPath);
	}
	
	AssetImporterVersionHash hashes;
	m_AssetDatabase->GenerateAssetImporterVersionHashes(hashes);

	// Check if asset actually changes.
	AssetHashes newAssetHashes;
	CheckAssetsChangedActually(&needRefresh, &needAddMetaFile, hashes, newAssetHashes, m_TimerStats);

	// Early out again, there is nothing to do.
	if (needRefresh.empty () && needRemoval.empty () && needRemoveMetaFile.empty() && needAddMetaFile.empty() )
	{
		printf_console("%f seconds (Nothing changed)\n", GetElapsedTimeInSeconds(refreshStart));

		StopAssetEditing();

		return false;
	}

	// First, collect the guids of all files that have been removed
	set<UnityGUID> removedGUIDs;
	set<UnityGUID> refreshedGUIDs;
	for (set<string>::iterator i=needRemoval.begin ();i != needRemoval.end ();i++)
	{
		string pathName = *i;
		
		UnityGUID guid;
		if ( pm.PathNameToGUID (pathName, &guid) && guid.IsValid() )
		{
			// Ignore the trashing if we can suddenly find the file again.
			// If the guidmapper doesn't know about the guid it should be removed from the asset database
			// This happens eg. when a file is removed, refresh is done, but the assetdatabase isn't saved because the editor crashes!
			if (!pathName.empty () && IsMetaFile(pathName.c_str(), pathName.size()) && IsPathCreated (pathName))
			{
				printf_console ("Refresh: Not trashing because file was found again %s\n", pathName.c_str ());
				continue;
			}
			removedGUIDs.insert(guid);
		}
		m_AssetDatabase->RemoveAssetTimeStamp(pathName);
	}
	
	// Then, read guid from updated asset metafiles
	map<string, UnityGUID> knownGUIDs;
	map<string, UnityGUID> overriddenGUIDs;
	set<UnityGUID> seen;
	for( set<string>::iterator i= needRefresh.begin () ; i !=  needRefresh.end () ; i++ )
	{
		string pathName = *i;
		needAddMetaFile.erase(*i);

		UnityGUID overriddenGUID = ReadGUIDFromTextMetaData(pathName);
		
		UnityGUID knownGUID;
		if ( pm.PathNameToGUID (pathName, &knownGUID) && knownGUID.IsValid() )
			knownGUIDs.insert(make_pair(pathName, knownGUID));

		// Skip invalid and already seen guids 
		if ( ! overriddenGUID.IsValid() || seen.find(overriddenGUID) != seen.end() || pathName == "Assets")
			continue;

		seen.insert(overriddenGUID);
			
		overriddenGUIDs.insert(make_pair(pathName, overriddenGUID));
	}
	
	// Verify that there are no duplicate guids
	for( map<string, UnityGUID>::iterator i = overriddenGUIDs.begin(); overriddenGUIDs.end() != i ; )
	{
		map<string, UnityGUID>::iterator current = i++; // 'cause we might want to delete the current entry
		
		string oldPath = pm.AssetPathNameFromGUID(current->second);
		
		// if GUID is not already known, then it's OK
		if(  oldPath.empty() ) 
			continue;

		// if oldPath also has a new GUID, then everything is also hunky-dory
		if( overriddenGUIDs.find(oldPath) != overriddenGUIDs.end() ) 
			continue;

		// also if oldPath no longer exists on disk
		if( needRemoval.find(oldPath) != needRemoval.end() )
		{
			m_DidMove.insert(make_pair(current->second, oldPath));
			continue;
		}

		// Allow stealing the GUID if there is no asset on disk that uses it anymore
		if (!IsPathCreated(oldPath))
			continue;

		// All other cases, ignore the GUID as it's already taken
		WarningStringMsg ("The GUID for %s is already in use by %s. Assigning a new guid.", current->first.c_str(), oldPath.c_str());
		current->second = UnityGUID();
	}


	// Note: it might make sense to initialize the update queue at the same time as looking for overridden GUIDs
	deque<string> updateQueue;
	updateQueue.assign(needRefresh.begin (), needRefresh.end ());

	// Update all assets
	for (;!updateQueue.empty (); updateQueue.pop_front() )
	{
		string pathName = updateQueue.front();
		UnityGUID guid;
		map<string, UnityGUID>::iterator found = overriddenGUIDs.find(pathName);
		if (found != overriddenGUIDs.end())
		{
			guid = found->second;

			// If the meta file was found during refresh, but the guid is invalid it means the guid is a duplicate
			// and we need to write a new .meta file with new guid
			if (!guid.IsValid())
			{
				options |= kForceRewriteTextMetaFile;
			}
		}

		/// Meta file has no already know guid.
		/// Create a new one and enforce that we write the text meta file
		if ( ! guid.IsValid() )
		{
			guid = pm.CreateAsset(pathName, kForceCaseSensitivityFromInputPath);
			QueueUpdate (guid, MdFour(), options | kAssetWasModifiedOnDisk, hashes, &newAssetHashes);
		}
		/// Create/Update the asset. Use the GUID from the .meta file.
		else
		{
			pm.ForceCreateDefinedAsset(pathName, guid);

			QueueUpdate (guid, MdFour(), options | kAssetWasModifiedOnDisk, hashes, &newAssetHashes);

			UnityGUID oldGUID = knownGUIDs[pathName];
			if ( oldGUID.IsValid() )
			{
				removedGUIDs.insert(oldGUID);
				// If a directory gets a new guid we will have to update the children so their parent pointer points to the new parent.
				RefreshOrphans(oldGUID, needRefresh, updateQueue);
				
				// We might already be using new guid somewhere (if two assets swap guids), so do the same for the new guid
				// (The guid persistence manager has already expired the old guid, so we have to do it now)
				RefreshOrphans(guid, needRefresh, updateQueue);
			}

		}

		// if this was not moved, it was refreshed
		if (m_DidMove.find(guid) == m_DidMove.end())
			refreshedGUIDs.insert(guid);
	}
	
	// Delete all meta files that are no longer needed.
	for(set<string>::iterator i = needRemoveMetaFile.begin(); i != needRemoveMetaFile.end(); i++)
	{
		const string& assetPath = *i;
		Assert(!IsMetaFile(assetPath.c_str(), assetPath.size()));
		
		std::string metaFilePath = GetTextMetaDataPathFromAssetPath(assetPath);

		// If we are refreshing this asset, don't delete meta file yet, as it will be used to import settings.
		// It will be deleted on import.
		if ( needRefresh.find(assetPath) == needRefresh.end())
		{
			if( IsPathCreated(assetPath) )
			{
				SetFileFlags(metaFilePath, 0, kFileFlagHidden);
				m_AssetDatabase->ForceMarkAsFoundMetaFile(assetPath, true);
			}
			else
			{
				DeleteFile (metaFilePath);
			}
		}
		else
		{
			// If it's also in the refresh list, set the file flag anyway.
			SetFileFlags(metaFilePath, 0, kFileFlagHidden);
			m_AssetDatabase->ForceMarkAsFoundMetaFile(assetPath, true);
		}
	}

	// Do a fast refresh of all assets that need to add the associated .meta file
	for(set<string>::iterator i = needAddMetaFile.begin(); i != needAddMetaFile.end(); i++)
	{
		string pathName=*i;
		UnityGUID guid = GetGUIDPersistentManager().CreateAsset(*i);
		if ( guid.IsValid() )
		{
			QueueUpdate (guid, MdFour(), options | kRefreshTextMetaFile, hashes, &newAssetHashes);
			refreshedGUIDs.insert(guid);
		}
	}
	
	printf_console("%f seconds\n", GetElapsedTimeInSeconds(refreshStart));
	
	// Finally remove Assets that were removed in the Assets folder only if the guids is not found again,
	// which means that the asset wasn't deleted, but moved
	for (set<UnityGUID>::iterator i=removedGUIDs.begin ();i != removedGUIDs.end ();i++)
	{
		if ( refreshedGUIDs.find(*i) == refreshedGUIDs.end() && m_DidMove.find(*i) == m_DidMove.end() )
		{
			printf_console ("Refresh: trashing asset %s\n", GUIDToString(*i).c_str());
			m_AssetDatabase->RemoveAsset (*i, AssetDatabase::kRemoveCacheOnly);
			m_DidRemove.insert (*i);
		}
	}

	// We allow stopping import when opening a project.
	while (1)
	{
		AssetInterface::CancelBehaviour cancelBehaviour = (options & kForceSynchronousImport) != 0 ? AssetInterface::kAllowCancel : AssetInterface::kNoCancel;
		if (StopAssetEditing (cancelBehaviour) == AssetInterface::kUserCancelled)
		{
			if(!DisplayDialog("Cancel Import?", "Do you want to stop asset importing and quit Unity?",
							 "Continue Import", "Force Quit") )
			{
				exit (1);
			}
			
			StartAssetEditing ();
			continue;
		}
		
		break;
	}
	
	ReSerializeAssetsIfNeeded (refreshedGUIDs);
	
	return true;
}

void AssetInterface::RefreshAndSaveAssets ()
{	
	Refresh ();
	SaveAssets ();
	Refresh ();
}

void AssetInterface::RefreshDelayed(int options)
{
	m_DelayedRefreshRequested++;
	m_DelayedRefreshOptions |= options; // TODO: Analyze the proper way to combine refresh options
}

void AssetInterface::Tick() 
{
	if (! m_DelayedRefreshRequested )
		return;
		
	// copy the options to a local variable and clear them. That way calling RefreshDelayed form code that migh get called from Refresh will still work. 
	int options=m_DelayedRefreshOptions;
	m_DelayedRefreshOptions=0;
	m_DelayedRefreshRequested=0;
	
	Refresh(options);
}
		

bool AssetInterface::MoveToTrash (const UnityGUID& guid)
{
	StartAssetEditing ();
	bool success = m_AssetDatabase->RemoveAsset (guid, AssetDatabase::kMoveAssetToTrash, &m_DidRemove);
	StopAssetEditing ();
	return success;
}

bool AssetInterface::MoveToTrash (const string& path)
{
	return MoveToTrash (GetGUIDPersistentManager().CreateAsset (path));
}

bool AssetInterface::DeleteAsset (const string& path)
{
	StartAssetEditing ();
	bool success = m_AssetDatabase->RemoveAsset (GetGUIDPersistentManager().CreateAsset (path), AssetDatabase::kDeleteAssets, &m_DidRemove);
	StopAssetEditing ();
	return success;
}

void AssetInterface::QueueUpdate (const UnityGUID& guid, const MdFour& forcedHash, int options, AssetImporterVersionHash const& hashes,
	AssetHashes* newAssetHashes)
{
	FatalErrorIf (!IsLocked ());

	if (options & kImportRecursive)
	{
		string path = GetGUIDPersistentManager ().AssetPathNameFromGUID (guid);
		if (IsDirectoryCreated(path))
		{
			std::set<string> childPaths;
			GetFolderContentsAtPath(path,childPaths);
			for(std::set<string>::const_iterator i=childPaths.begin();i != childPaths.end();i++) 
			{
				if( IsMetaFile(i->c_str(), i->size()))
				   continue;
				   
				QueueUpdate(GetGUIDPersistentManager().CreateAsset(*i), MdFour(), options, hashes, newAssetHashes);
			}
		}
	}

	GUIDToRefreshQueue::iterator found = m_GUIDToRefreshQueue.find (guid);
	if (found == m_GUIDToRefreshQueue.end ())
	{
		string path = GetGUIDPersistentManager ().AssetPathNameFromGUID (guid);
		RefreshAsset asset;
		hash_cstring hash;
		asset.subQueue = hash(path.c_str());
		
		AssetImporterSelection importerSelection = AssetImporter::FindAssetImporterSelection (path);
		
		asset.importerClassID = importerSelection.importerClassID;

		// Bug scenario: 1. Add new scripts ; 2. Add meta file for existed script, the scripts compilation is skipped. 
		// Fixed by separating these scripts into two groups by different queue index.
		if (options & kRefreshTextMetaFile)
			asset.queue = 10000;
		else
			asset.queue = importerSelection.queueIndex;

		asset.guid = guid;

		// Get the new hash.
		if( newAssetHashes != NULL)
		{
			// The newAssetHashes is not NULL during Refresh().
			map<UnityStr, MdFour>::iterator it = newAssetHashes->find(path);

			if( it != newAssetHashes->end() )
				asset.hash = it->second;
		}
		else
		{
			// ImportAssets() needs to recompute the hash. 
			// TODOVZ: Is it always true?
			asset.recomputeHash = true;
		}

		asset.forcedHash = forcedHash;
		asset.options = options;

		// Get the import version hash.
		if (hashes.size() > 0)
		{
			Assert(hashes.find(importerSelection.importerClassID) != hashes.end());
			asset.importerVersionHash = hashes.find(importerSelection.importerClassID)->second;
		}
		else
			asset.importerVersionHash = HashImporterVersion(importerSelection.importerClassID, importerSelection.importerVersion);
		
		RefreshQueue::iterator link = m_RefreshQueue.insert (asset);
		GUIDToRefreshQueue::iterator link2 = m_GUIDToRefreshQueue.insert (make_pair (guid, link)).first;
		const_cast<RefreshAsset&>(*link).link = link2;
		AssertIf (m_RefreshQueue.size () != m_GUIDToRefreshQueue.size ());
	}
	else
	{
		const_cast<RefreshAsset&>(*found->second).options |= options;
		if (forcedHash != MdFour())
			const_cast<RefreshAsset&>(*found->second).forcedHash = forcedHash;
		return;
	}
}

void AssetInterface::UpdateProgress (float individualAssetProgress, float overrideTotalProgress, const std::string& customTitle)
{
	AssetInterface& ai = AssetInterface::Get();
	if (GetTimeSinceStartup() - ai.m_BeginAssetProgressTime < 0.5F)
		return;
	
	float total = ai.m_DidRefresh.size () + ai.m_PostprocessedRefreshedAssets + ai.m_RefreshQueue.size () + ai.m_MaxProgressImmediateImportAssets + 1;
	float processed = ai.m_DidRefresh.size () + ai.m_PostprocessedRefreshedAssets;
	processed += individualAssetProgress;
	
	string title;
	if (individualAssetProgress != 0.0F)
		title = Format ("%s (%d%%)", customTitle.c_str(), RoundfToInt(clamp01(individualAssetProgress) * 100.0F));
	else 
		title = customTitle;
	
	// overrideTotalProgress negative value indicates that no progressbar should be shown.
	if (overrideTotalProgress < 0.0F)
		overrideTotalProgress = processed / total;
	
	UpdateAssetProgressbar(overrideTotalProgress, "Hold on", title, ai.m_CanCancelAssetQueueProcessing);
}

AssetInterface::OperationStatus
AssetInterface::ProcessAssetsImplementation (double& startTime, CancelBehaviour cancelBehaviour)
{
	if (m_RefreshQueue.empty () && m_DidRefresh.empty () && m_DidRemove.empty ())
		return kPerformed;

	m_CanCancelAssetQueueProcessing = (cancelBehaviour & kAllowCancel) != 0;

	bool wasCancelled = false;
	
	// Update assets
	int numberOfImportedAssets = 0;
	m_AssetDatabase->RefreshAssetImporterVersionHashesCache();
	while (!m_RefreshQueue.empty () && !wasCancelled)
	{
		// Pick the first asset from queue
		RefreshAsset asset = *m_RefreshQueue.begin();

		m_GUIDToRefreshQueue.erase(m_RefreshQueue.begin()->link);
		m_RefreshQueue.erase(m_RefreshQueue.begin());
		
		// Calculate parent guid 
		UnityGUID parentGUID;
		std::string warning;
		if (CalculateParentGUID (asset.guid, &parentGUID, &warning))
		{
			// Update Asset - Do we have the same queue priority?
			int options = asset.options;
			if (!m_RefreshQueue.empty () && m_RefreshQueue.begin ()->queue == asset.queue)
				options |= kNextImportHasSamePriority;
			
			// Update asset
			bool didAddAsset = m_AssetDatabase->AssetPtrFromGUID(asset.guid) == NULL;
			m_AssetDatabase->UpdateAsset (asset.guid, parentGUID, asset.forcedHash, options, asset.hash);
			m_DidRefresh.insert (asset.guid);
			if (didAddAsset)
				m_DidAdd.insert(asset.guid);
			
			// Check if the cancel button was pressed
			if (m_CanCancelAssetQueueProcessing && IsAssetProgressBarCancelPressed ())
			{
				if (cancelBehaviour & kClearQueueOnCancel)
				{
					// Clear the m_GUIDToRefreshQueue of assets that are in m_RefreshQueue
					// if it was specified to do so
					while (!m_RefreshQueue.empty ()) {
						m_GUIDToRefreshQueue.erase (m_RefreshQueue.begin ()->link);
						m_RefreshQueue.erase (m_RefreshQueue.begin ());
					}
				}
				
				wasCancelled = true;
			}
			
			numberOfImportedAssets++;
			// Write asset database once in a while 
			float delta = GetTimeSinceStartup() - startTime;
			bool highMemory = EditorAssetGarbageCollectManager::Get()->ShouldCollectDueToHighMemoryUsage();
			if (delta > kSaveAssetsDuringLongImportTimeout || highMemory || wasCancelled)
			{
				ApplyDefaultPostprocess();
				SaveNonVersionedSingletonAssets ();
				
				UnloadUnusedAssetsImmediate(true);
				
				GetSceneTracker().ClearUnloadedDirtyCallbacks();
				
				startTime = GetTimeSinceStartup();
			}
		}
		else
		{
			WarningString(warning);
		}	
	}
	AssertIf (m_RefreshQueue.size () != m_GUIDToRefreshQueue.size ());
	
	// Process again and again until we are done!	
	if (!wasCancelled && !m_RefreshQueue.empty ())
		if (ProcessAssetsImplementation (startTime, cancelBehaviour) == kUserCancelled)
			wasCancelled = true;
	
	ApplyDefaultPostprocess ();
	SaveNonVersionedSingletonAssets ();

	// avoid garbagecollecting if only iterating a few assets
	if (numberOfImportedAssets > 10)
	{
		GetPreloadManager().LockPreloading();
		GarbageCollectSharedAssets(true);
		GetPreloadManager().UnlockPreloading();
	}

	// Process again and again until we are done!	
	if (!wasCancelled && !m_RefreshQueue.empty ())
		ProcessAssetsImplementation (startTime, AssetInterface::kNoCancel);
	
	return wasCancelled ? kUserCancelled : kPerformed;
}

void AssetInterface::ApplyDefaultPostprocess ()
{
	set<UnityGUID> didRefresh, didRemove, didAdd;
	map<UnityGUID, string> didMove;
	didRefresh.swap (m_DidRefresh); 
	didRemove.swap (m_DidRemove);
	didAdd.swap (m_DidAdd);
	didMove.swap(m_DidMove);
	m_PostprocessedRefreshedAssets += didRefresh.size();
	if (!didRefresh.empty () || !didRemove.empty ())
		m_AssetDatabase->Postprocess (didRefresh, didAdd, didRemove, didMove);
}

UnityGUID AssetInterface::ImportAtPathImmediate (const std::string& path, int options)
{
	if (!IsPathCreated (path))
		return UnityGUID ();
	AssertIf(gLock == 0);
	
	// Calculate parent guid 
	UnityGUID guid = GetGUIDPersistentManager ().CreateAsset (path);
	
	UnityGUID parentGUID;
	string warning;
	if (!CalculateParentGUID (guid, &parentGUID, &warning))
	{
		ImportAtPathImmediate (DeleteLastPathNameComponent(path));
		
		warning.clear();
		if (!CalculateParentGUID (guid, &parentGUID, &warning))
		{
			WarningString(warning);
			return UnityGUID();
		}
	}
	
	// Update asset
	m_AssetDatabase->RefreshAssetImporterVersionHashesCache();
	bool didAddAsset = m_AssetDatabase->AssetPtrFromGUID(guid) == NULL;
	m_AssetDatabase->UpdateAsset (guid, parentGUID, MdFour(), options);
	m_DidRefresh.insert (guid);
	if (didAddAsset)
		m_DidAdd.insert(guid);
		
	m_MaxProgressImmediateImportAssets = max(m_MaxProgressImmediateImportAssets-1, 0);
	
	return guid;
}

UnityGUID AssetInterface::ImportAtPath (const std::string& path, int options)
{
	if (!IsPathCreated (path))
		return UnityGUID ();
	
	// Try creating asset for the asset path
	UnityGUID guid = GetGUIDPersistentManager ().CreateAsset (path, kNewAssetPathsUseGetActualPathSlow);

	// Make sure parent assets are already imported.
	UnityGUID parentGUID;
	std::string warning;
	if (!CalculateParentGUID (guid, &parentGUID, &warning))
	{
		std::string parentPath = DeleteLastPathNameComponent(path);
		ImportAtPath (parentPath, options);
	}
	
	if (guid.IsValid ())
	{
		StartAssetEditing ();
		QueueUpdate (guid, MdFour(), options, AssetImporterVersionHash());
		StopAssetEditing ();
	}
	
	return guid;
}

void AssetInterface::ImportAssets (const vector<UnityGUID>& guids, int options)
{
	if (guids.empty ())
		return;
	
	StartAssetEditing ();
	AssetImporterVersionHash hashes;
	m_AssetDatabase->GenerateAssetImporterVersionHashes(hashes);
	for (vector<UnityGUID>::const_iterator i=guids.begin ();i != guids.end ();i++)
		QueueUpdate (*i, MdFour(), options, hashes);
	StopAssetEditing ();
}

void AssetInterface::ImportAssets (const set<UnityGUID>& guids, int options)
{
	if (guids.empty ())
		return;

	StartAssetEditing ();
	AssetImporterVersionHash hashes;
	m_AssetDatabase->GenerateAssetImporterVersionHashes(hashes);
	for (set<UnityGUID>::const_iterator i=guids.begin ();i != guids.end ();i++)
		QueueUpdate (*i, MdFour(), options, hashes);
	StopAssetEditing (((options & kMayCancelImport) != 0 ? AssetInterface::kAllowCancel : AssetInterface::kNoCancel) | AssetInterface::kClearQueueOnCancel);
}

void AssetInterface::ImportAssets (const map<UnityGUID,MdFour>& guids, int options)
{
	if (guids.empty ())
		return;

	StartAssetEditing ();
	AssetImporterVersionHash hashes;
	m_AssetDatabase->GenerateAssetImporterVersionHashes(hashes);
	for (map<UnityGUID,MdFour>::const_iterator i=guids.begin ();i != guids.end ();i++)
		QueueUpdate (i->first, i->second, options, hashes);

	StopAssetEditing ();
}

void AssetInterface::ImportAsset (UnityGUID guid, int options)
{
	vector<UnityGUID> assets;
	assets.push_back(guid);
	ImportAssets(assets, options);
}

void RebuildGUIDCache (const vector_set<UnityGUID>& metaDataGUIDsOnDisk, vector_set<UnityGUID>& resurrectedGUIDs)
{
	if (metaDataGUIDsOnDisk.empty())
		return;
	
	double time = GetTimeSinceStartup();
	printf_console("Rebuilding GUID cache ... ");
	vector<string> deletePaths;
	
	for (vector_set<UnityGUID>::const_iterator i=metaDataGUIDsOnDisk.begin();i != metaDataGUIDsOnDisk.end();++i)
	{
		UnityGUID guid = *i;
		string path = GetMetaDataPathFromGUID(guid);
		
		DebugAssert(IsFileCreated (path));
		
		AssetMetaData* metaData = FindAssetMetaDataAtPath (path);
		if (metaData == NULL)
		{
			LogString (Format ("Rebuiling GUID cache: Deleting metadata %s because it is an invalid meta data file!\nMetaData class can't be loaded", path.c_str ()));
			UnloadObject(metaData); deletePaths.push_back (path);
			continue;
		}
		
		if (!IsPathCreated (metaData->pathName))
		{
			LogString (Format ("Rebuilding GUID cache: Deleting metadata %s because the asset doesn't exist anymore.\n MetaData path %s", metaData->pathName.c_str (), path.c_str ()));
			UnloadObject(metaData); deletePaths.push_back (path);
			continue;
		}
		
		string error = GetGUIDPersistentManager ().CreateDefinedAsset (GetActualPathSlow(metaData->pathName), metaData->guid);
		if (!error.empty ())
		{
			LogString ("Rebuilding GUID cache: Deleting metadata " + (std::string)metaData->pathName + " because it is an invalid meta data file.!\n" + error);
			UnloadObject(metaData); deletePaths.push_back (path);
			continue;
		}
		
		resurrectedGUIDs.push_unsorted (guid);

		UnloadObject(metaData);
		GetPersistentManager ().UnloadStream (path);
	}

	GetPersistentManager ().UnloadStreams ();
	for (int i=0;i<deletePaths.size ();i++)
	{
		DeleteFile (deletePaths[i]);
		AssertIf (GetPersistentManager ().CountInstanceIDsAtPath (deletePaths[i]) != 0);
	}
	
	resurrectedGUIDs.sort();
	
	time = GetTimeSinceStartup() - time;
	printf_console("%f seconds.\n", (float)time);
}


static int CalculateMostImportantObject (const vector<LibraryRepresentation>& objects)
{
	int goCount = 0;
	int goIndex = -1;

	for (int i=0;i<objects.size ();i++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (objects[i].object);
		if (go)
		{
			Transform* transform = go->QueryComponent (Transform);
			if (transform == NULL || transform->GetParent () == NULL)
			{
				++goCount;
				goIndex = i;
			}
		}
	}

	if (1 == goCount)
		return goIndex;

	
	int priorities [] = {
		Object::StringToClassID ("Prefab"),
		Object::StringToClassID ("Font"),
		Object::StringToClassID ("DynamicFont"),
		Object::StringToClassID ("Material"),
		Object::StringToClassID ("MovieTexture"),
		Object::StringToClassID ("TerrainData"),
		Object::StringToClassID ("Cubemap"),
		Object::StringToClassID ("Texture2D"),
		Object::StringToClassID ("SubstanceArchive"),		
		Object::StringToClassID ("DefaultAsset"),
		Object::StringToClassID ("AnimatorController")
	};
	
	// Go through other assets in priority order
	for (int j=0;j<ARRAY_SIZE(priorities);j++)
	{
		for (int i=0;i<objects.size ();i++)
		{
			Object& object = *objects[i].object;
			if (object.GetClassID () == priorities[j])
				return i;
		}
	}

	return -1;
}

inline Object* CreateIfNotAvailable (Object* o, const string& name)
{
	int classID = Object::StringToClassID (name);
	if (o && o->IsDerivedFrom (classID))
		return o;

	o = Object::Produce (classID);
	o->Reset ();
	o->AwakeFromLoad (kDefaultAwakeFromLoad);
	AssertIf (o == NULL);
	return o;
}

void AssetInterface::SaveNonVersionedSingletonAssets ()
{
	// Add singleton assets
	for (SerializedSingletons::iterator i=m_SerializedSingletons.begin ();i != m_SerializedSingletons.end ();i++)
	{
		if (i->shouldSerialize)
		{
			if(GetPersistentManager ().TestNeedWriteFile (i->path))
			{
				if (i->guid.IsValid())
					continue;

				// Singletons without a GUID (AssetDatabase and others) are written right away.
				ErrorOSErr (GetPersistentManager ().WriteFile (i->path, BuildTargetSelection::NoTarget(), kDontReadObjectsFromDiskBeforeWriting));
				SetFileFlags(i->path, kFileFlagDontIndex, kFileFlagDontIndex); // don't index singleton assets
			}
		}
	}
}

void AssetInterface::GetDirtyAssets (set<UnityGUID> &dirtyAssets)
{	
	// Add singleton assets
	for (SerializedSingletons::iterator i=m_SerializedSingletons.begin ();i != m_SerializedSingletons.end ();i++)
	{
		if (i->shouldSerialize)
		{
			if(GetPersistentManager ().TestNeedWriteFile (i->path))
			{
				if (i->guid.IsValid())
					dirtyAssets.insert (i->guid);
				SetFileFlags(i->path, kFileFlagDontIndex, kFileFlagDontIndex); // don't index singleton assets
			}
		}
	}
	
	// Add all serialized assets
	m_AssetDatabase->GetDirtySerializedAssets (dirtyAssets);
}

void AssetInterface::WriteRevertAssets (const set<UnityGUID> &writeAssets, const set<UnityGUID> &revertAssets)
{	
	m_AssetDatabase->WriteSerializedAssets (writeAssets);
	SaveNonVersionedSingletonAssets();

	ImportAssets (writeAssets);
	ImportAssets (revertAssets, kAssetWasModifiedOnDisk);
}


bool AssetInterface::CopyAsset (const UnityGUID &guid, const std::string& newPath)
{	
	SaveAssets();
	return AssetDatabase::Get().CopyAsset(guid, newPath);
}

// if a scene is passed, add that to the save confirmation dialog and callbacks,
// and return true if it should be saved, false otherwise.
void AssetInterface::SaveAssets ()
{
	set<UnityGUID> dirtyAssets;
	
	GetDirtyAssets (dirtyAssets);
	set<UnityGUID> saveAssets;
	set<UnityGUID> revertAssets;
	AssetModificationCallbacks::ShouldSaveAssets (dirtyAssets, saveAssets, revertAssets, false);

	WriteRevertAssets (saveAssets, revertAssets);
}

inline Object& ProduceSingletonAsset (const string& name, const string& pathName)
{
	PersistentManager& pm = GetPersistentManager ();
	int id = pm.GetInstanceIDFromPathAndFileID (pathName, 1);
	
	int classID = Object::StringToClassID (name);
	Object* ptr = PPtr<Object> (id);
	if (ptr == NULL || !ptr->IsDerivedFrom (classID))
	{
		ptr = CreateIfNotAvailable(ptr, name);
		pm.MakeObjectPersistentAtFileID (ptr->GetInstanceID (), 1, pathName);
		pm.WriteFile (pathName);
	}

	return *ptr;
}

inline Object& ProduceSingletonAssetDontWrite (const string& name, const string& pathName)
{
	PersistentManager& pm = GetPersistentManager ();
	int id = pm.GetInstanceIDFromPathAndFileID (pathName, 1);
	Object* ptr = CreateIfNotAvailable (PPtr<Object> (id), name);
	pm.MakeObjectPersistentAtFileID (ptr->GetInstanceID (), 1, pathName);
	return *ptr;
}

void AssetInterface::ReloadSingletonAssets ()
{
	// Load all other serialized objects stored in the library folder
	// Write the file to make sure we have all manager assets
	// Otherwise refresh will bitch now because it can't find the files.
	for (SerializedSingletons::iterator i=m_SerializedSingletons.begin ();i != m_SerializedSingletons.end ();i++)
	{
		if (i->shouldSerialize)
			i->ptr = &ProduceSingletonAsset (i->className, i->path);
		else 
		{
			CreateFile (i->path, '?', 'TEXT');
			SetFileFlags(i->path, kFileFlagDontIndex, kFileFlagDontIndex); // don't index singleton assets
		}
		
		// Reload manager context immediately after loading project settings,
		// because other managers depend on it through using the PlayerPrefs class
		if (i->path == kProjectSettingsPath)
			ResetManagerContextFromLoaded();
	}
	
	ResetManagerContextFromLoaded ();
	
	m_AssetDatabase = dynamic_cast<AssetDatabase*> (GetSingletonAsset ("AssetDatabase"));
	AssetDatabase::SetAssetDatabase (m_AssetDatabase);
	AssertIf (m_AssetDatabase == NULL);

	SetEditorBuildSettings (dynamic_cast<EditorBuildSettings*> (GetSingletonAsset ("EditorBuildSettings")));
	SetEditorUserBuildSettings (dynamic_cast<EditorUserBuildSettings*> (GetSingletonAsset ("EditorUserBuildSettings")));
	SetEditorSettings (dynamic_cast<EditorSettings*> (GetSingletonAsset ("EditorSettings")));
	SetEditorUserSettings (dynamic_cast<EditorUserSettings*> (GetSingletonAsset ("EditorUserSettings")));
}

void ExtractGUIDsFromDirectory(const std::string& path, vector_set<UnityGUID>& guids, size_t hintGUIDSize)
{
	dynamic_array<DirectoryEntry> entries(kMemTempAlloc);
	entries.reserve(hintGUIDSize);
	std::string directoryPath = path;
	GetDirectoryContents(directoryPath, entries, kDeepSearch);
	
	directoryPath.reserve(1024);
	for (int i=0;i<entries.size();i++)
	{
		if (entries[i].type == kFile)
		{
			UnityGUID guid = StringToGUID(entries[i].name, strlen(entries[i].name));
			if (guid != UnityGUID())
				guids.push_unsorted(guid);
			else
			{
				string fullPath = AppendPathName(directoryPath, entries[i].name);
				printf_console ("Invalid cached metadata discovered. Will be deleted: '%s'\n", fullPath.c_str());
				DeleteFile(fullPath);
			}
		}
		
		UpdatePathDirectoryOnly (entries[i], directoryPath);
	}
	
	guids.sort();
}

void ExtractGUIDsNotFoundInAssetDatabase(const vector_set<UnityGUID>& rootAssets, const vector_set<UnityGUID>& normalAssets, const vector_set<UnityGUID>& guidsOnDisk, vector_set<UnityGUID>& guidMappingsNotFoundInAssetDatabaseButOnDisk)
{
	// Remove files in meta data / cache that are no longer being used
	for (vector_set<UnityGUID>::const_iterator i=guidsOnDisk.begin();i != guidsOnDisk.end();i++)
	{
		UnityGUID guid = *i;
		if ( guid.IsValid() && normalAssets.count (guid) == 0 && rootAssets.count (guid) == 0)
			guidMappingsNotFoundInAssetDatabaseButOnDisk.push_unsorted(guid);
	}
	
	guidMappingsNotFoundInAssetDatabaseButOnDisk.sort();
}

void CreateAllGUIDParentDirectories(const std::string& path)
{
	const char kHexToLiteral[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

	string fullpath = AppendPathName(path, "00");
	for (int i=0;i<16;i++)
	{
		for (int j=0;j<16;j++)
		{
			fullpath[fullpath.size() - 1] = kHexToLiteral[i];
			fullpath[fullpath.size() - 2] = kHexToLiteral[j];
			FatalErrorIf(!CreateDirectoryRecursive(fullpath));
		}
	}	
}

bool AssetInterface::PerformAssetDatabaseConsistencyChecks()
{
	///@TODO: validate allRootAssets...
	
	vector_set<UnityGUID> allNormalAssets;
	vector_set<UnityGUID> allRootAssets;
	m_AssetDatabase->GetAllAssets(allRootAssets, allNormalAssets);

	bool success = true;

	// Get a conservative estimate of how many guids we'll be scanning
	int hintGUIDSize = allNormalAssets.size() + allRootAssets.size();
	hintGUIDSize += 100 + hintGUIDSize / 10;
	
	// Find all meta data guids in the library folder
	vector_set<UnityGUID> metaDataGUIDsOnDisk, metaDataNotInAssetDatabase;
	ExtractGUIDsFromDirectory(GetMetaDataDirectoryPath(), metaDataGUIDsOnDisk, hintGUIDSize);
	ExtractGUIDsNotFoundInAssetDatabase(allRootAssets, allNormalAssets, metaDataGUIDsOnDisk, metaDataNotInAssetDatabase);
	
	// - And figure out which metadata files are on disk but not in the assetdatabase.
	//   This can happen Unity crashes during import (An asset import completed, but the database has not yet been written)
	//   In that case we read the metafile contents and recreate the GUIDmapping from the meta file.
	vector_set<UnityGUID> guidsResurrectedFromMetaDataFiles;
	RebuildGUIDCache(metaDataNotInAssetDatabase, guidsResurrectedFromMetaDataFiles);
	
	// Make sure that all guids that we know about actually do have a meta file and cache file
	for (vector_set<UnityGUID>::iterator i=allNormalAssets.begin();i != allNormalAssets.end();i++)
	{
		// Check if the GUID - Asset path mapping is valid
		const string& assetPath = GetAssetPathFromGUID(*i);
		if (assetPath.empty() && !GetGUIDPersistentManager().IsConstantGUID(*i))
		{
			WarningString (Format("Path mapping for Asset with GUID %s is missing! Cleaning up.", GUIDToString(*i).c_str()));
			success &= m_AssetDatabase->RemoveFromHierarchy(*i);
			continue;
		}
		
		// Make sure the file exists on disk
		if (metaDataGUIDsOnDisk.count(*i) == 0)
		{
			// Asset exists, but the meta file is now missing -> Perform a Reimport.
			if( IsPathCreated( assetPath ) )
			{
				WarningString(assetPath + " is missing its meta data file. Reimporting asset.");
			}
			// Asset no longer exists and the meta file is missing too (This will get cleaned up on application shut down)
			else
			{
				WarningString(assetPath + " is missing and it's meta data file is missing too.");
			}
			
			success &= m_AssetDatabase->RemoveFromHierarchy(*i);
			continue;
		}
	}
	
	// Repair time stamp inconsistencies
	m_AssetDatabase->ValidateTimeStampInconsistency ();
	
	if (!m_AssetDatabase->VerifyAssetDatabase())
		return false;
	
	return success;
}

void UpgradeSingletonAssets()
{
	if (IsDirectoryCreated(kLibraryPath))
	{
		if (!IsDirectoryCreated(kProjectSettingsFolder))
		{
			// Upgrade from beta3
			if (IsDirectoryCreated("Library/Versioned"))
			{
				CopyFileOrDirectory("Library/Versioned", kProjectSettingsFolder);
			}
			else
			{
				CreateDirectory(kProjectSettingsFolder);
				if (IsPathCreated("Library/InputManager.asset"))
					CopyFileOrDirectory("Library/InputManager.asset", "ProjectSettings/InputManager.asset");

				if (IsPathCreated("Library/TagManager.asset"))
					CopyFileOrDirectory("Library/TagManager.asset", "ProjectSettings/TagManager.asset");

				if (IsPathCreated("Library/ProjectSettings.asset"))
					CopyFileOrDirectory("Library/ProjectSettings.asset", kProjectSettingsPath);

				if (IsPathCreated("Library/AudioManager.asset"))
					CopyFileOrDirectory("Library/AudioManager.asset", "ProjectSettings/AudioManager.asset");

				if (IsPathCreated("Library/TimeManager.asset"))
					CopyFileOrDirectory("Library/TimeManager.asset", "ProjectSettings/TimeManager.asset");

				if (IsPathCreated("Library/DynamicsManager.asset"))
					CopyFileOrDirectory("Library/DynamicsManager.asset", "ProjectSettings/DynamicsManager.asset");

				if (IsPathCreated("Library/QualitySettings.asset"))
					CopyFileOrDirectory("Library/QualitySettings.asset", "ProjectSettings/QualitySettings.asset");

				if (IsPathCreated("Library/NetworkManager.asset"))
					CopyFileOrDirectory("Library/NetworkManager.asset", "ProjectSettings/NetworkManager.asset");

				if (IsPathCreated("Library/EditorBuildSettings.asset"))
					CopyFileOrDirectory("Library/EditorBuildSettings.asset", "ProjectSettings/EditorBuildSettings.asset");

				if (IsPathCreated("Library/EditorSettings.asset"))
					CopyFileOrDirectory("Library/EditorSettings.asset", kEditorSettingsPath);

				if (IsPathCreated("Library/NavMeshLayers.asset"))
					CopyFileOrDirectory("Library/NavMeshLayers.asset", "ProjectSettings/NavMeshLayers.asset");		
			}
		}
	}
}

bool AssetInterface::Initialize (bool shouldRebuildLibrary, int *prevForwardCompatibleVersion)
{
	#define AddAssetExtension(e) { NativeFormatImporter::RegisterNativeFormatExtension (e); }

	UpgradeSingletonAssets();
	
	AddAssetExtension("shaderGraph");
	AddAssetExtension("shaderSubGraph");
	AddAssetExtension("GUISkin");
	// Register Directly modified assets
	NativeFormatImporter::RegisterNativeFormatExtension ("mat");
	NativeFormatImporter::RegisterNativeFormatExtension ("anim");	
	NativeFormatImporter::RegisterNativeFormatExtension ("cubemap");	
	NativeFormatImporter::RegisterNativeFormatExtension ("prefab");	
	NativeFormatImporter::RegisterNativeFormatExtension ("flare");	
	NativeFormatImporter::RegisterNativeFormatExtension ("fontsettings");	
	NativeFormatImporter::RegisterNativeFormatExtension ("webCamTexture");	
	NativeFormatImporter::RegisterNativeFormatExtension ("renderTexture");	
	NativeFormatImporter::RegisterNativeFormatExtension ("physicMaterial");	
#if ENABLE_2D_PHYSICS
	NativeFormatImporter::RegisterNativeFormatExtension ("physicsMaterial2D");	
#endif
	NativeFormatImporter::RegisterNativeFormatExtension ("shaderGraph");	
	NativeFormatImporter::RegisterNativeFormatExtension ("shaderSubGraph");	
	NativeFormatImporter::RegisterNativeFormatExtension ("GUISkin");	
	NativeFormatImporter::RegisterNativeFormatExtension ("asset");				
	NativeFormatImporter::RegisterNativeFormatExtension ("controller");
	NativeFormatImporter::RegisterNativeFormatExtension ("animset");	// deprecated, now overrideController
	NativeFormatImporter::RegisterNativeFormatExtension ("overrideController");	
	NativeFormatImporter::RegisterNativeFormatExtension ("mask");	
	NativeFormatImporter::RegisterNativeFormatExtension ("ht");
	NativeFormatImporter::RegisterNativeFormatExtension ("colors");
	NativeFormatImporter::RegisterNativeFormatExtension ("gradients");
	NativeFormatImporter::RegisterNativeFormatExtension ("curves");
	NativeFormatImporter::RegisterNativeFormatExtension ("curvesNormalized");
	NativeFormatImporter::RegisterNativeFormatExtension ("particleCurves");
	NativeFormatImporter::RegisterNativeFormatExtension ("particleCurvesSigned");
	NativeFormatImporter::RegisterNativeFormatExtension ("particleDoubleCurves");
	NativeFormatImporter::RegisterNativeFormatExtension ("particleDoubleCurvesSigned");


	GetGUIDPersistentManager ().RegisterConstantGUID ("Assets", kAssetFolderGUID);
	GetGUIDPersistentManager ().RegisterConstantGUID (kEditorResourcePath, GetEditorResourcesGUID());
	GetGUIDPersistentManager ().RegisterConstantGUID (kResourcePath, GetBuiltinResourcesGUID());
	GetGUIDPersistentManager ().RegisterConstantGUID (kDefaultExtraResourcesPath, GetBuiltinExtraResourcesGUID());


	// Register asset database and scriptmapper
	AddSerializedSingleton ("AssetDatabase",    kAssetDatabasePath);
	AddSerializedSingleton ("GUIDSerializer",    kGUIDMapperPath);
	AddSerializedSingleton ("ScriptMapper",     "Library/ScriptMapper");
	
	// Real assets (Get uploaded to the server)
	AddSerializedSingleton ("InputManager",     "ProjectSettings/InputManager.asset",    UnityGUID (0,0,2,0));
	AddSerializedSingleton ("TagManager",       "ProjectSettings/TagManager.asset",      UnityGUID (0,0,3,0));
	AddSerializedSingleton ("PlayerSettings",   kProjectSettingsPath,           UnityGUID (0,0,4,0));
	AddSerializedSingleton ("",                 "Library/BuildPlayer.prefs",     UnityGUID (0,0,5,0));
	AddSerializedSingleton ("AudioManager",     "ProjectSettings/AudioManager.asset",    UnityGUID (0,0,6,0));
	AddSerializedSingleton ("TimeManager",      "ProjectSettings/TimeManager.asset",     UnityGUID (0,0,7,0));
	AddSerializedSingleton ("PhysicsManager",  "ProjectSettings/DynamicsManager.asset", UnityGUID (0,0,8,0));
	AddSerializedSingleton ("QualitySettings",  "ProjectSettings/QualitySettings.asset", UnityGUID (0,0,9,0));
	AddSerializedSingleton ("NetworkManager",  "ProjectSettings/NetworkManager.asset", UnityGUID (0,0,10,0));
	AddSerializedSingleton ("EditorBuildSettings",     "ProjectSettings/EditorBuildSettings.asset", UnityGUID (0,0,11,0));
	AddSerializedSingleton ("EditorSettings",     kEditorSettingsPath, UnityGUID (0,0,12,0));
	AddSerializedSingleton ("NavMeshLayers",     "ProjectSettings/NavMeshLayers.asset", UnityGUID (0,0,20,0));
	#if ENABLE_2D_PHYSICS
	AddSerializedSingleton ("Physics2DSettings", "ProjectSettings/Physics2DSettings.asset", UnityGUID (0,0,21,0));
	#endif
	AddSerializedSingleton ("GraphicsSettings",     kGraphicsSettingsPath, UnityGUID (0,0,22,0));
	
	// Locally serialized singletons (Don't get uploaded to the server)
	AddSerializedSingleton ("MonoManager", "Library/MonoManager.asset");
	AddSerializedSingleton ("AssetServerCache", "Library/AssetServerCacheV3");
	AddSerializedSingleton ("BuildSettings", "Library/BuildSettings.asset");
	AddSerializedSingleton ("InspectorExpandedState", "Library/InspectorExpandedItems.asset");
	AddSerializedSingleton ("AnnotationManager", "Library/AnnotationManager");
	AddSerializedSingleton ("EditorUserBuildSettings", "Library/EditorUserBuildSettings.asset");
	AddSerializedSingleton ("EditorUserSettings", "Library/EditorUserSettings.asset");
	
	AssetImporter::SetUpdateProgressCallback(UpdateProgress);
	
	AssetDatabase::RegisterPostprocessCallback (MonoPostprocessAllAssets);
	AssetDatabase::RegisterPostprocessCallback (PostprocessAssetsUpdateCachedIcon);
	AssetDatabase::RegisterPostprocessCallback (PostprocessAssetsUpdatePreviews);
	
	AssetImportPrefabQueue::InitializeClass ();
	
	// Register most important objects
	AssetDatabase::RegisterCalculateMostImportantCallback (CalculateMostImportantObject);
	
	// Creates assets and library directory
	CreateDirectory (kSourceAssets);
	CreateDirectory (kLibraryPath);
	CreateDirectory (kProjectSettingsFolder);
	
	string projectPath = File::GetCurrentDirectory ();
	
	// Force Rebuild library if the assetdatabase is gone
	// (deletes, guid mappings, asset datbase, and all cached files)
	if (!shouldRebuildLibrary && !IsFileCreated (kAssetDatabasePath))
	{
		LogString ("Rebuilding Library because the asset database could not be found!");
		shouldRebuildLibrary = true;
	}

	// User upgraded to a new incompatible version of unity. Rebuild libarary
	PlayerSettings& projectSettings = static_cast<PlayerSettings&> (ProduceSingletonAssetDontWrite ("PlayerSettings", kProjectSettingsPath));
	*prevForwardCompatibleVersion = projectSettings.unityForwardCompatibleVersion;

	/// Upgrade to a new version and be backwards incompatible
	if (projectSettings.unityRebuildLibraryVersion != UNITY_REBUILD_LIBRARY_VERSION && projectSettings.unityForwardCompatibleVersion < UNITY_ASK_FOR_UPGRADE_VERSION)
	{
		string text = Format ("Your project was created with an older version of Unity. (%s)\n"
					  "Unity needs to rebuild the project, this will take a few minutes.\n"
					  "After upgrading the project, you can not open the project with older versions of Unity anymore.", projectPath.c_str ());
		
		if (IsHumanControllingUs() && !DisplayDialog ("Upgrading project!", text, "Continue", "Quit"))
			ExitDontLaunchBugReporter ();
		
		projectSettings.unityRebuildLibraryVersion = UNITY_REBUILD_LIBRARY_VERSION;
		projectSettings.unityForwardCompatibleVersion = UNITY_FORWARD_COMPATIBLE_VERSION;
		projectSettings.SetDirty ();
		shouldRebuildLibrary = true;
	}
	else
	{
		// Need to rebuild project
		if (projectSettings.unityRebuildLibraryVersion != UNITY_REBUILD_LIBRARY_VERSION)
		{
			if (!shouldRebuildLibrary)
			{
				string text = Format ("Your project was created with an incompatible version of Unity. (%s)\n"
							  "Upgrading the project might take a few minutes.\n", projectPath.c_str ());
				
				if (IsHumanControllingUs() && !DisplayDialog ("Upgrading project!", text, "Continue", "Quit"))
					ExitDontLaunchBugReporter ();

			}
			
			projectSettings.unityRebuildLibraryVersion = UNITY_REBUILD_LIBRARY_VERSION;
			projectSettings.SetDirty ();
			shouldRebuildLibrary = true;
		}
		
		// If we are running an older version of unity. Ask user to quit
		if (UNITY_FORWARD_COMPATIBLE_VERSION != projectSettings.unityForwardCompatibleVersion)
		{
			if (UNITY_FORWARD_COMPATIBLE_VERSION < projectSettings.unityForwardCompatibleVersion)
			{
				string text = Format ("Your project was created with a newer version of Unity. (%s)\n"
									  "You should upgrade to the latest version of Unity.\n"
									  "You risk data loss if you continue!", projectPath.c_str ());
									  
				if (IsHumanControllingUs() && DisplayDialog ("Project incompatible, New version of Unity required", text, "Quit", "Continue and risk data loss"))
					ExitDontLaunchBugReporter ();
			}
			else if (projectSettings.unityForwardCompatibleVersion < UNITY_ASK_FOR_UPGRADE_VERSION)
			{
				string text = Format ("Upgrading project (%s)\n"
								  "After upgrading the project, you can not open the project with older versions of Unity anymore.",
								  projectPath.c_str ());
								  
				if (IsHumanControllingUs() && !DisplayDialog ("Upgrading project", text, "Continue", "Quit"))
					ExitDontLaunchBugReporter ();
			}
			
			projectSettings.unityForwardCompatibleVersion = UNITY_FORWARD_COMPATIBLE_VERSION;
			projectSettings.SetDirty ();
		}
	}

	bool rebuildingAssets = LoadAssetDatabase (shouldRebuildLibrary);
	m_AssetDatabase->ReadFailedAssets();
	
	// Make sure that all GUID directories ('Library/metadata/01' all 256 are pre-created, so they don't need to be created or validated at import time)
	CreateAllGUIDParentDirectories(GetMetaDataDirectoryPath());
	
	// Up to Unity 3.5 we had a previews & cache directory.
	// Now all results from an asset importer are in Library/metadata
	DeleteFileOrDirectory("Library/previews");
	DeleteFileOrDirectory("Library/cache");
	
	return rebuildingAssets;
}

void AssetInterface::ReSerializeAssetsIfNeeded (const set<UnityGUID> &guids)
{
	if (GetEditorSettings().GetSerializationMode() == EditorSettings::kMixed)
		return; //nothing to do.

	bool didBackupScene = false;
	string backupScenePath;
	set<UnityGUID> dirtyAssets, revertAssets;
	vector<std::string> scenesToReserialize;
	
	int totalAssets = guids.size();
	int processed = 0;
	for( set<UnityGUID>::const_iterator i = guids.begin () ; i != guids.end () ; i++ )
	{
		processed++;
		const UnityGUID &guid = *i;
		if (m_AssetDatabase->IsAssetAvailable(guid))
		{
			const Asset &asset = m_AssetDatabase->AssetFromGUID(guid);
			if (asset.type == kSerializedAsset)
			{
				string path = GetGUIDPersistentManager().AssetPathNameFromGUID (guid);
				if (IsSerializedFileTextFile(path) != (GetEditorSettings().GetSerializationMode() == EditorSettings::kForceText))
				{
					UpdateAssetProgressbar((processed/(float)totalAssets), "Hold on", "Re-serializing Assets...");
					if (asset.mainRepresentation.object.IsValid())
						asset.mainRepresentation.object->SetDirty();
					dirtyAssets.insert (guid);
					
					// For GameManagers, asset.mainRepresentation does not point to the actual object.
					// So search m_SerializedSingletons to SetDirty on this. There must be any easier way to do this.
					if (path.find ("ProjectSettings") == 0)
					{
						for (int j=0;j<m_SerializedSingletons.size();j++)
						{
							if (m_SerializedSingletons[j].guid == *i)
							{
								if (m_SerializedSingletons[j].ptr != NULL)
									m_SerializedSingletons[j].ptr->SetDirty();
								break;
							}
						}
					}
				}
			}
			else if (asset.mainRepresentation.classID == ClassID(SceneAsset))
			{
				string path = GetGUIDPersistentManager().AssetPathNameFromGUID (guid);
				Assert (GetPathNameExtension(path) == "unity");
				if (IsSerializedFileTextFile(path) != (GetEditorSettings().GetSerializationMode() == EditorSettings::kForceText))
				{
					UpdateAssetProgressbar((processed/(float)totalAssets), "Hold on", "Re-serializing Assets...");
					if (!didBackupScene)
					{
						backupScenePath = GetApplication().GetCurrentScene();
						GetApplication().SaveSceneInternal(kCurrentSceneBackupPath, kNoTransferInstructionFlags);
						didBackupScene = true;
					}
					dirtyAssets.insert(guid);
					scenesToReserialize.push_back(path);
				}
			}
		}
	}

	FileMode mode = (GetEditorSettings().GetSerializationMode() == EditorSettings::kForceText) ?
			kFMText : kFMBinary;
	
	// Tell e.g. version control about changes to serialization mode
	// When going to text mode we need to filter some assets that does support
	// text serialization such as Terrain.
	if (mode == kFMText)
	{
		set<UnityGUID> fileModeChangeAssets;
		PersistentManager& pm = GetPersistentManager();
		for (set<UnityGUID>::const_iterator i = dirtyAssets.begin(); 
			 i != dirtyAssets.end(); ++i)
		{
			const Asset &asset = m_AssetDatabase->AssetFromGUID(*i);
			if (!pm.IsClassNonTextSerialized(asset.mainRepresentation.classID))
				fileModeChangeAssets.insert(*i);
		}
		// This will also checkout the asset if needed
		AssetModificationCallbacks::FileModeChanged(fileModeChangeAssets, mode);
	}
	else
	{
		// This will also checkout the asset if needed
		AssetModificationCallbacks::FileModeChanged(dirtyAssets, mode);
	}

	WriteRevertAssets (dirtyAssets, revertAssets);

	for (int i = 0; i < scenesToReserialize.size(); ++i)
	{
		string path = scenesToReserialize[i];
		GetApplication().OpenScene (path);
		TransferInstructionFlags options = kNoTransferInstructionFlags;
		if (GetEditorSettings().GetSerializationMode() == EditorSettings::kForceText)
			options |= kAllowTextSerialization;
		GetApplication().SaveSceneInternal (path, options);
	}
	
	if (didBackupScene)
	{
		AssetInterface::Get ().ImportAtPath (backupScenePath);
		GetApplication().SetOpenPath(backupScenePath);
		GetApplication().LoadSceneInternal(kCurrentSceneBackupPath, 0);
	}
	
	ClearAssetProgressbar();
}

// Rewrites all serialized files to disk if they don't match the current serialization mode.
void AssetInterface::RewriteAllSerializedFiles ()
{
	std::set<UnityGUID> allAssets;
	m_AssetDatabase->GetAllAssets (allAssets);
	ReSerializeAssetsIfNeeded (allAssets);
}

bool AssetInterface::LoadAssetDatabase (bool shouldRebuildLibrary)
{
	// Need to rebuild library for outside reason. Do it
	if (shouldRebuildLibrary)
	{
		// Delete asset database and guid mapper so we don't even try to load it
		DeleteFileOrDirectory(kAssetDatabasePath);
		DeleteFileOrDirectory(kGUIDMapperPath);
		
		CleanupCachedAssetFoldersCompletely ();
		shouldRebuildLibrary = true;
	}
	
	ABSOLUTE_TIME loadGUIDPathMappingTime = START_TIME;
	
	printf_console("Loading GUID <-> Path mappings ... ");
	// Load guid serializer from disk or recreate it from the meta files
	LoadGUIDSerializer ();
	printf_console("%f seconds.\n", GetElapsedTimeInSeconds(loadGUIDPathMappingTime));

	printf_console("Loading Asset Database ... ");
	ABSOLUTE_TIME loadAssetDatabaseTime = START_TIME;

	// Try loading asset database - if it fails, just cleanup and return
	int assetDatabaseID = GetPersistentManager ().GetInstanceIDFromPathAndFileID (kAssetDatabasePath, 1);
	if (dynamic_instanceID_cast<AssetDatabase*> (assetDatabaseID) == NULL)
	{
		printf_console("[ Failed ] %f seconds\n", GetElapsedTimeInSeconds(loadAssetDatabaseTime));

		CleanupCachedAssetFoldersCompletely();
		shouldRebuildLibrary = true;
	}
	
	printf_console(" %f seconds\n", GetElapsedTimeInSeconds(loadAssetDatabaseTime));

	// Assetdatabase has successfully loaded - make sure the asset database singleton pointer is registered
	ReloadSingletonAssets ();
		
	ABSOLUTE_TIME consistencyChecksTime = START_TIME;

	printf_console("AssetDatabase consistency checks ... ");
	// Loading asset database worked.
	// Perform consistency checks. Warn user and reimport.
	bool consistencyChecksSuccesful = PerformAssetDatabaseConsistencyChecks();

	printf_console(" %f seconds\n", GetElapsedTimeInSeconds(consistencyChecksTime));
	
	if ( !consistencyChecksSuccesful )
	{
		string projectPath = File::GetCurrentDirectory ();

		string text = Format ("The project at %s contains inconsistencies in the asset database.\n"
							  "You must reimport the project in order to get the asset database into a consistent state.\n"
							  "Depending on project size, rebuilding the project may take a while complete.\n", projectPath.c_str ());
		
		if (!IsBatchmode() && ! DisplayDialog ("Project reimport required", text, "Reimport Project", "Quit"))
			ExitDontLaunchBugReporter ();
		
		CleanupCachedAssetFoldersCompletely();
		shouldRebuildLibrary = true;
	}
	
	
	return shouldRebuildLibrary;
}

void AssetInterface::CleanupCachedAssetFoldersCompletely ()
{
	// Clear cache path
	DeleteFileOrDirectory(kCacheDataPath);

	// Create meta data and cache path
	CreateDirectory (kMetaDataPath);
	CreateDirectory (kCacheDataPath);

	// Cleanup anything loaded from assetdatabase / guidmapper and delete files
	GetPersistentManager ().DeleteFile (kAssetDatabasePath, PersistentManager::kDeleteLoadedObjects);
	GetPersistentManager ().DeleteFile (kGUIDMapperPath, PersistentManager::kDeleteLoadedObjects);

	// This just deleted GUIDMapper, so we've lost all constant GUID mappings (assets folder & manager files).
	// Reinitialize it.
	LoadGUIDSerializer();

	// Reload singleton assets
	ReloadSingletonAssets ();
	
	// We cleared all caches. So there is no need to rescan for platform dependent reimport of assets.
	GetAssetImportState ().SetImportedForTarget (GetEditorUserBuildSettings().GetActiveBuildTargetSelection());
}

void AssetInterface::LoadGUIDSerializer ()
{
	// Load GUID serializer
	PersistentManager& pm = GetPersistentManager ();
	int id = pm.GetInstanceIDFromPathAndFileID (kGUIDMapperPath, 1);
	m_GUIDSerializer = CreateIfNotAvailable (PPtr<Object> (id), "GUIDSerializer");
	pm.MakeObjectPersistentAtFileID (m_GUIDSerializer->GetInstanceID (), 1, kGUIDMapperPath);
	
	// Register all constant GUID's so they are available when loading the asset database
	GetGUIDPersistentManager ().CreateAsset ("Assets");
	for (int i=0;i<m_SerializedSingletons.size();i++)
	{
		if (m_SerializedSingletons[i].guid != UnityGUID())
			GetGUIDPersistentManager().CreateAsset(m_SerializedSingletons[i].path);
	}
}

void AssetInterface::Shutdown ()
{
	Refresh ();
	SaveAssets ();

	GetPersistentManager ().WriteFile (kExpandedItemsPath);

	// Cleanup guids
	vector_set<UnityGUID> allAssets;
	m_AssetDatabase->GetAllAssets (allAssets);
	GetGUIDPersistentManager ().CleanupUnusedGUIDs (allAssets);
	
	// Write guid mapper
	GetPersistentManager ().WriteFile (kGUIDMapperPath, BuildTargetSelection::NoTarget(), kDontReadObjectsFromDiskBeforeWriting);
	
	// Temporary hack to make project upgrade warnings work against 3.4.
	// Storing upgrade information in the project settings is not a good solution,
	// so we should really just have a dedicated text file for detecting project upgrade issues.
	CopyFileOrDirectory(AppendPathName(kProjectSettingsFolder, "ProjectSettings.asset"), "Library/ProjectSettings.asset");
	
	StopPreloadManager();
	
	GetBuiltinExtraResourceManager().DestroyAllResources();
	GetBuiltinResourceManager().DestroyAllResources();

	CleanupVCProvider();

	CleanupEngine();

	AssetImportPrefabQueue::CleanupClass ();

	CleanupMono ();
	CleanupPersistentManager();

	// editor console logging accesses persistent manager right now.
	RegisterLogToConsole(NULL);
}

set<UnityGUID> AssetInterface::GetUpdateQueue ()
{
	set<UnityGUID> queue;
	for (GUIDToRefreshQueue::iterator i=m_GUIDToRefreshQueue.begin ();i != m_GUIDToRefreshQueue.end ();i++)
		queue.insert (i->first);
	return queue;
}


std::string GenerateReusableAssetPathName (const UnityGUID& parentGuid, const std::string& name, const std::string& extension)
{
	CreateDirectory (kSourceAssets);

	string folder;
	string pathName = GetGUIDPersistentManager ().AssetPathNameFromGUID (parentGuid);
	if (IsDirectoryCreated (pathName))
		folder = pathName;
	else
		folder = DeleteLastPathNameComponent (pathName);

	// Create unique pathName in folder with name and extension
	string newPathName = AppendPathName (folder, name);
	newPathName = AppendPathNameExtension (newPathName, extension);
	newPathName = GenerateUniquePathSafe (newPathName);
	UnityGUID guid = GetGUIDPersistentManager ().CreateAsset (newPathName);
	if ( guid.IsValid() )
		return newPathName;
	else
		return string();
}

bool AssetInterface::CreateSerializedAsset (Object& object, const string& uniquePathName, int mask)
{
	if (!IsDirectoryCreated(DeleteLastPathNameComponent(uniquePathName)))
	{
		ErrorString (Format("Parent directory must exist before creating asset at %s.", uniquePathName.c_str()));
		return false;
	}

	if (!CheckValidFileName(GetLastPathNameComponent(uniquePathName)))
	{
		ErrorStringMsg ("'%s' is not a valid asset file name.", uniquePathName.c_str());
		return false;
	}
	
	UnityGUID guid = GetGUIDPersistentManager().CreateAsset (uniquePathName, kNewAssetPathsUseGetActualPathSlow);
	
	if (uniquePathName.empty () || ! guid.IsValid() )
	{
		ErrorString ("Couldn't create asset file!");
		UnityBeep ();
		return false;
	}

	if (object.IsPersistent())
	{
		ErrorStringObject (Format("Couldn't create asset file because the %s '%s' is already an asset at '%s'!", object.GetClassName().c_str(), object.GetName(), GetAssetPathFromObject(&object).c_str()), &object);
		UnityBeep ();
		return false;
	}

	if (dynamic_pptr_cast<GameObject*> (&object))
	{
		ErrorStringObject (Format("Couldn't create asset file because '%s' is a GameObject! Use the PrefabUtility class instead.", object.GetName()), &object);
		UnityBeep ();
		return false;
	}
	
	if (mask & (kDeleteExistingAssets | kWriteAndImport))
	{
		// Make sure the object won't disappear if StartAssetEditing()
		// ends up triggering a GC run.
		SetPreventGarbageCollectionOfAsset (object.GetInstanceID ());
		
		StartAssetEditing();

		ClearPreventGarbageCollectionOfAsset (object.GetInstanceID ());

		if (mask & kDeleteExistingAssets)
			AssetDatabase::Get().RemoveAsset (guid, AssetDatabase::kDeleteAssets, &m_DidRemove);	
	}
	
	// Produce object and set name
	string name = GetLastPathNameComponent (uniquePathName);
	name = DeletePathNameExtension (name);
	object.SetName (name.c_str());
	
	// Persist object
	int fileID = object.GetClassID () * kMaxObjectsPerClassID;
	GetPersistentManager ().MakeObjectPersistentAtFileID (object.GetInstanceID (), fileID, uniquePathName);
	
	if (mask & kWriteAndImport)
	{
		AssetModificationCallbacks::WillCreateAssetFile (uniquePathName);
		
		int options = GetEditorSettings().GetSerializationMode() == EditorSettings::kForceText ? kAllowTextSerialization : 0;

		// Write file
		ErrorOSErr (GetPersistentManager ().WriteFile (uniquePathName, BuildTargetSelection::NoTarget(), options));
		
		// Perform immediate import only if we are inside of an asset importer
		ImportAtPathImmediate (uniquePathName);
	}

	if (mask & (kDeleteExistingAssets | kWriteAndImport))
		StopAssetEditing();

	return true;
}

bool AssetInterface::CreateSerializedAsset (Object& object, const UnityGUID& parentGuid, const string& assetName, const string& assetExtension, int mask)
{
	// Create new unused pathname
	string uniquePathName;
	if (mask & kCreateUniqueGUID)
		uniquePathName = GetGUIDPersistentManager ().GenerateUniqueAssetPathName (parentGuid, AppendPathNameExtension(assetName, assetExtension) );
	else
		uniquePathName = GenerateReusableAssetPathName (parentGuid, assetName, assetExtension);
	
	return CreateSerializedAsset(object, uniquePathName, mask);
}

Object* AssetInterface::GetSingletonAsset (const string& className)
{
	SerializedSingletons::iterator i;
	for (i=m_SerializedSingletons.begin ();i != m_SerializedSingletons.end ();++i)
	{
		if (i->className == className)
			break;
	}

	if (i == m_SerializedSingletons.end ())
		return NULL;
	else
		return i->ptr;
}

void AssetInterface::AddSerializedSingleton (const string& className, const string& path, UnityGUID guid)
{
	SerializedSingleton temp;
	temp.className = className;
	temp.path = path;
	temp.ptr = NULL;
	temp.guid = guid;
	temp.shouldSerialize = !className.empty ();
	m_SerializedSingletons.push_back (temp);
	
	if (guid != UnityGUID ())
	{
		GetGUIDPersistentManager ().RegisterConstantGUID (path, guid);
		
		if (temp.shouldSerialize)
			LibraryAssetImporter::RegisterSupportedPath (path);
	}
}

string AssetInterface::MoveAsset (const UnityGUID& guid, const std::string& newAssetPath)
{
	StartAssetEditing();
	string error = m_AssetDatabase->MoveAsset(guid, newAssetPath);

	StopAssetEditing();

	return error;
}

string AssetInterface::MoveAssets( const std::vector<std::string>& assets, const std::string& newParentPath )
{
	StartAssetEditing();
	GUIDPersistentManager& guidPM = GetGUIDPersistentManager ();
	string result = "";
	for (int i=0;i<assets.size();i++)
	{
		string oldPath = GetActualPathSlow(assets[i]);

		// Get the old asset path with original lower/upper case formatting
		if (IsPathCreated (oldPath))
		{
			// Move asset to new location
			string newAssetPath = AppendPathName (newParentPath, GetLastPathNameComponent (oldPath));	
			UnityGUID guid = guidPM.CreateAsset (oldPath);
			string error = MoveAsset (guid, newAssetPath);
			if (error != "")
				result = error;
		}
	}

	StopAssetEditing();
	return result;
}


std::string AssetInterface::RenameAsset (const UnityGUID& guid, std::string name)
{
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset == NULL)
		return "The source asset could not be found";

	if (name.empty() || name.find_last_not_of (' ') == string::npos)
		return "Can't rename to empty name";

	// Calculate the new asset path
	string oldAssetPath = GetGUIDPersistentManager ().AssetPathNameFromGUID (guid);
	string folderPath = DeleteLastPathNameComponent (oldAssetPath);
	if (folderPath.empty ())
		return "No folder in asset's path";
	
	string newAssetPath = AppendPathName (folderPath, name);

	if (asset->type != kFolderAsset)
		newAssetPath = AppendPathNameExtension (newAssetPath, GetPathNameExtension (oldAssetPath));
	
	// Just return success in case we are changing the asset name to the exact same name as before
	if( newAssetPath == oldAssetPath )
		return string();
		
	string error = AssetDatabase::Get().ValidateMoveAsset (guid, newAssetPath);
	if (error.empty())
	{
		string error = MoveAsset (guid, newAssetPath);

		if (error == "")
			ImportAtPath(newAssetPath);

		return error;
	}
	else
	{
		return error;
	}
}

void AssetInterface::ImportAssetsWithMismatchingAssetImporterVersion ()
{
	AssetImporterVersionHash hashes;
	vector<UnityGUID> assetsToImport;
	m_AssetDatabase->GenerateAssetImporterVersionHashes(hashes);
	m_AssetDatabase->GetAssetsThatNeedsReimporting(hashes, assetsToImport);

	ImportAssets (assetsToImport, kForceSynchronousImport);
}



// TODO: it does more already, rename it?
static void SynchronizeMeshRenderersAndMaterials()
{
	vector<Object*> filters;
	Object::FindObjectsOfType (Object::StringToClassID ("Renderer"), &filters);
	Object::FindObjectsOfType (Object::StringToClassID ("Material"), &filters);
	
	// lightmap settings contains lightmap textures in Maps tab
	Object::FindObjectsOfType (Object::StringToClassID ("LightmapSettings"), &filters);
	// GuiTexture needs additional processing of textures
	Object::FindObjectsOfType (Object::StringToClassID ("GUITexture"), &filters);
	Object::FindObjectsOfType (Object::StringToClassID ("ParticleSystem"), &filters);

	for (vector<Object*>::iterator i = filters.begin(); i != filters.end(); i++)
		(**i).AwakeFromLoad (kDefaultAwakeFromLoad);

	// Above core rebuilds substances if required, we need to make sure all processes are finished here
#if ENABLE_SUBSTANCE
	GetSubstanceSystem ().WaitFinished ();
#endif
}

HierarchyState& GetProjectWindowHierarchyState ()
{
	int id = GetPersistentManager ().GetInstanceIDFromPathAndFileID (kExpandedItemsPath, kExpandedItemsFileID);
	HierarchyState* state = PPtr<HierarchyState> (id);
	if (state == NULL)
		state = CreateObjectFromCode<HierarchyState>();
	GetPersistentManager ().MakeObjectPersistentAtFileID (state->GetInstanceID (), kExpandedItemsFileID, kExpandedItemsPath);
	return *state;
}

HierarchyState* GetProjectWindowHierarchyStateIfLoaded ()
{
	int id = GetPersistentManager ().GetInstanceIDFromPathAndFileID (kExpandedItemsPath, kExpandedItemsFileID);
	HierarchyState* state = dynamic_pptr_cast<HierarchyState*> (Object::IDToPointer (id));
	return state;
}

