#include "UnityPrefix.h"
#include "ASController.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/FBXImporter.h"
#include "ASCache.h"
#include "Editor/Src/AssetServer/Backend/ASBackend.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/AssetPipeline/LibraryAssetImporter.h"
#include "Editor/Src/EditorSettings.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Utilities/File.h"
//#include "unistd.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Input/TimeManager.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoManager.h"
#include "Editor/Src/Utility/DiffTool.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/LicenseInfo.h"
#include "ASMonoUtility.h"
#include "Editor/Src/Application.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"

#include <iostream>
using namespace std;
using namespace AssetServer;

#include <set>
#include <map>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>

#if UNITY_WIN
#include <fcntl.h>
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#else
#include <sys/fcntl.h>
#endif

// Use in public Controller methods to log errors
#define LogError(x) { DebugStringToFile (x, 0, __FILE__, __LINE__, kLog | kAssetImportError, 0, GetLogId() );}
#define LogWarning(x) { DebugStringToFile (x, 0, __FILE__, __LINE__, kLog | kAssetImportWarning, 0, GetLogId() );}

#define RETURN_LOG_ERROR( error ) { m_ErrorString=error; LogError(m_ErrorString); return false; }
#define RETURN_ERROR( error ) { m_ErrorString=error; return false; }
#define RETURN_OK { /*m_ErrorString="";*/ return true; } // don't clear last error, it needs to be shown later on
#define RETURN_FAIL return false

// For use inside Controller::UpdateHandle methods
#define LogDownloadError(x) { m_Controller->SetError(UnityStr(x).c_str()); DebugStringToFile (x, 0, __FILE__, __LINE__, kLog | kAssetImportError, 0, m_Controller->GetLogId() );}
#define LogDownloadWarning(x) { DebugStringToFile (x, 0, __FILE__, __LINE__, kLog | kAssetImportWarning, 0, m_Controller->GetLogId() );}
#define DOWNLOAD_LOG_ERROR( error ) { Cleanup(); LogDownloadError(error);  return false; }
#define DOWNLOAD_ERROR( error ) { Cleanup(); return false; }
#define DOWNLOAD_OK { return true; }
#define DOWNLOAD_COMPLETE { Cleanup(); return true; }
#define DOWNLOAD_FAIL { Cleanup();  return false; }

const char* kTempPath = "Temp/";

const char* AssetServer::kAssetStream = "asset";
const char* AssetServer::kAssetResourceStream = "assetRsrc";
const char* AssetServer::kBinMetaStream = "metaData";
const char* AssetServer::kTxtMetaStream = "asset.meta";
const char* AssetServer::kAssetPreviewStream = "preview.png";

// Define a bunch of strings
static const char* unresolved = "No Resolution";
static const char* skipAsset = "Skip Asset";
static const char* trashMyChanges = "Discard My Changes";
static const char* trashServerChanges = "Ignore Server Changes";
static const char* mergeChanges = "Merge Changes";
static const char* renameLocal = "Rename Local Asset";
static const char* renameRemote = "Rename Server Asset";

const char* Controller::GetStringForDownloadResolution(AssetServer::DownloadResolution resolution) {
	// and return reference to the appropriate one
	switch (resolution) {
		case kDLSkipAsset:
			return skipAsset;
		case kDLTrashMyChanges:
			return trashMyChanges;
		case kDLTrashServerChanges:
			return trashServerChanges;
		case kDLMerge:
			return mergeChanges;
		case kDLUnresolved:
		default:
			return unresolved;
	}
}
const char* Controller::GetStringForNameConflictResolution(AssetServer::NameConflictResolution resolution) {
	// and return reference to the appropriate one
	switch (resolution) {
		case kNMRenameLocal:
			return renameLocal;
		case kNMRenameRemote:
			return renameRemote;
		case kNMUnresolved:
		default:
			return unresolved;
	}
}

const char* Controller::GetStringForStatus(AssetServer::Status status) {

	// Define a bunch of strings
	static const char* calculating = "Calculating...";
	static const char* clientOnly = "Added locally";
	static const char* serverOnly = "Added on server";
	static const char* unchanged = "Unchanged";
	static const char* clientModifiedAndNewServerVersion = "Changed locally and new version on server";
	static const char* clientUpdatedToNewServerVersion = "New version on server but have same data locally";
	static const char* newVersionAvailable = "New version on server";
	static const char* ignored = "Excluded from versioning";
	static const char* newLocalVersion = "Changed locally";
	static const char* restoredFromTrash = "Restored from server trash";
	static const char* badState = "An error occured";

	// and return reference to the appropriate one
	switch (status) {
		case kCalculating:
			return calculating;
		case kRestoredFromTrash:
			return restoredFromTrash;
		case kClientOnly:
			return clientOnly;
		case kServerOnly:
			return serverOnly;
		case kUnchanged:
			return unchanged;
		case kConflict:
			return clientModifiedAndNewServerVersion;
		case kSame:
			return clientUpdatedToNewServerVersion;
		case kNewVersionAvailable:
			return newVersionAvailable;
		case kNewLocalVersion:
			return newLocalVersion;
		case kIgnored:
			return ignored;
		case kBadState:
		default:
			return badState;
	}
}

Controller& Controller::Get () {
	static Controller binding;

	return binding;
}

void Controller::Initialize (const string& userName, const string& connectionSettings, int timeout) {
	if ( userName != m_UserName || connectionSettings != m_ConnectionSettings) 
		PgConn::FlushFreeConnections();
	m_Timeout = timeout;
	m_ConnectionSettings = connectionSettings;
	m_UserName = userName;
	m_Online = true;
	m_HasConnectionError = false;
	
	if ( ! m_ConnectionSettings.empty() && GetEditorSettings().GetExternalVersionControlSupport() != ExternalVersionControlAssetServer )
		GetEditorSettings().SetExternalVersionControlSupport( ExternalVersionControlAssetServer );
}

DownloadResolution Controller::GetDownloadResolution(const UnityGUID& guid) {
	return m_AssetServerCache->GetItemConst(guid).downloadResolution;
}
NameConflictResolution Controller::GetNameConflictResolution(const UnityGUID& guid){
	return m_AssetServerCache->GetItemConst(guid).nameConflictResolution;
}
bool Controller::IsMarkedForRemoval(const UnityGUID& guid){
	return m_AssetServerCache->GetItemConst(guid).markedForRemoval;
}

void Controller::SetDownloadResolution(const UnityGUID& guid, DownloadResolution value){
	m_AssetServerCache->GetItem(guid).downloadResolution = value;
}
void Controller::SetNameConflictResolution(const UnityGUID& guid, NameConflictResolution value){
	m_AssetServerCache->GetItem(guid).nameConflictResolution = value;
}
void Controller::SetMarkedForRemoval(const UnityGUID& guid, bool value){
	AssetServerCache::Item & item = m_AssetServerCache->GetItem(guid);
	if (item.markedForRemoval != value) {
		item.markedForRemoval = value;
	}
}

template <typename T, typename O>
struct KeyOfPair : public std::binary_function<pair<T, O>, T, bool> {
        bool operator() (pair<T, O> y, T x) { return x == y.first; }
};

UnityGUID Controller::GetAssetParent(const UnityGUID& guid) {
	if (m_AssetDatabase->IsAssetAvailable(guid))
		return m_AssetDatabase->AssetFromGUID (guid).parent;
	return UnityGUID();
}

string Controller::GetAssetName(const UnityGUID& guid) {
	return GetLastPathNameComponent(GetAssetPathName(guid));
}

string Controller::GetAssetPathName(const UnityGUID& guid) {
	
	return GetGUIDPersistentManager ().AssetPathNameFromGUID (guid);
}

bool Controller::InvokeMergeApp(const string& remoteLabel, const string& remote, const string& localLabel, const string& local, const string& ancestorLabel,  const string& ancestor, const string& target) 
{
	// We support moving the remote (foreign) version out of the way in case that's also the target
	string remoteMoved;
	if (remote == target) {
		remoteMoved = kTempPath + string("Server's version of ") + GetLastPathNameComponent(local);
	
		// Merge file is always temp, so OK to remove one that's already there
		if (IsFileCreated(remoteMoved)) {
			if (! DeleteFile(remoteMoved)) {
				RETURN_ERROR("Could not delete temporary file: " + remoteMoved);
			}
		}
			
		// Move it out of the way
		if (! MoveFileOrDirectory(remote, remoteMoved)) {
			RETURN_ERROR("Could not rename "+remote+" to " + remoteMoved);
		}
	}
	else 
		// Nothing to do in opposite case
		remoteMoved = remote;

	bool manualMergeRequired = true;
	std::string diff3Path;
	#if UNITY_OSX || UNITY_LINUX
	diff3Path = "/usr/bin/diff3";
	#elif UNITY_WIN
	diff3Path = AppendPathName( GetApplicationContentsPath(), "Tools/diff3.exe" );
	#else
	#error "Unknown platform"
	#endif
	if(IsFileCreated(diff3Path) )
	{
		string output;
		vector<string> args;

		args.push_back("--merge");
		args.push_back("--text");

		#if UNITY_OSX || UNITY_LINUX
		args.push_back(local); 
		args.push_back(ancestor);
		args.push_back(remoteMoved);

		// Diff3 outputs the merge file to stdout: 
		bool automergeOK = LaunchTaskArray(diff3Path, &output, args, true);
		#elif UNITY_WIN
		// diff3 does not like spaces on windows
		args.push_back(ToShortPathName(PathToAbsolutePath(local))); 
		args.push_back(ToShortPathName(PathToAbsolutePath(ancestor)));
		args.push_back(ToShortPathName(PathToAbsolutePath(remoteMoved)));

		// Diff3 outputs the merge file to stdout: 
		bool automergeOK = LaunchTaskArray(diff3Path, &output, args, true, AppendPathName( GetApplicationContentsPath(), "Tools"));
		#else
		#error "Unknown platform"
		#endif

		if (automergeOK && WriteStringToFile(output, target, kNotAtomic, kFileFlagDontIndex | kFileFlagTemporary))
			manualMergeRequired = false;
	}
	
	if(manualMergeRequired)
	{
		if (IsFileCreated(target)) {
			if (! DeleteFile(target)) {
				RETURN_ERROR("Could not delete temporary file: " + target);
			}
		}
		
		for(;;)
		{
			std::string errorText = InvokeMergeTool(
				remoteLabel, remoteMoved,
				localLabel, local,
				ancestorLabel, ancestor,
				target );
			if( !errorText.empty() ) {
				RETURN_ERROR(errorText);
			}

			Thread::Sleep( 1.0f ); // Hack to allow the diff tool to show up before opening our dialog
			
			// Now, pop-up an alert to the user to finish the merge before continuing, and wait for user to accept
			if( !DisplayDialog("Accept merge?", 
				Format("Finish merging (and saving) %s before pressing Accept.", GetLastPathNameComponent(local).c_str()),
				"Accept",
				"Cancel") )
			{
				RETURN_ERROR("Manual merge was aborted");
			}
			
			// If target does not exist, the user probably forgot to save the merge before hitting accept.
			// In this case tell the user that he needs to save the merge.
			if( ! IsFileCreated(target) ) {
				if( !DisplayDialog("Could not find merged file",
					Format("Could not find merged file for %s. Please hit save in the merge tool before accepting the merge in Unity.", 
													GetLastPathNameComponent(local).c_str()),
					"Reopen Merge Tool",
					"Cancel") )
				{
					RETURN_ERROR("Manual merge was aborted");
				}
			}
			else { // Everything is fine, so we break out of the loop
				break;
			}
		}
	}

	RETURN_OK;
}



bool Controller::InvokeCompareApp(const string& leftLabel, const string& left, const string& rightLabel, const string& right, const string& ancestorLabel, const string& ancestor)
{
	std::string errorText = InvokeDiffTool(
		leftLabel, left,
		rightLabel, right,
		ancestorLabel, ancestor
	);
	if( !errorText.empty() ) {
		RETURN_ERROR(errorText);
	}
	RETURN_OK;
}


bool Controller::AssetIsDir(const UnityGUID& guid) {
	return IsDirectoryCreated (GetAssetPathName(guid));
}

bool Controller::AssetDirIsEmpty(const UnityGUID& guid) {
	string pathName = GetAssetPathName(guid);
	if (!IsDirectoryCreated (pathName))
		return true;
	set<UnityGUID> tmp;
	m_AssetDatabase->CollectAllChildren (guid, &tmp);
	return tmp.empty();
}

bool Controller::DoesAssetExist(const UnityGUID& guid) {
	return m_AssetDatabase->IsAssetAvailable(guid); 
}

string Controller::TempFilename(const UnityGUID& guid, int version, const string& stream) {
	string assetName = GetAssetName(guid);
	#if UNITY_OSX
	if (kAssetResourceStream == stream) 
		return kTempPath + string("asset_server.") + kAssetStream + '.' + GUIDToString(guid) + "v" + IntToString(version) + ".tmp" + "/..namedfork/rsrc";
	#endif
	return kTempPath + string("asset_server.") + stream + '.' + GUIDToString(guid) + "v" + IntToString(version) + ".tmp";
}

void Controller::PrepareAssetsForStatus () {
	AssertIf (AssetInterface::Get().IsLocked ());
	AssetInterface::Get().RefreshAndSaveAssets ();
}

void Controller::LockAssets ()
{
	//AssertIf(mono_thread_current() != NULL && mono_method_get_last_managed () != NULL);
	
	AssetInterface& ai = AssetInterface::Get();
	PersistentManager& pm = GetPersistentManager();
	Configuration::Get().EnableWorkingItemCache();

	ai.RefreshAndSaveAssets ();
	ai.StartAssetEditing ();
	ai.SetGlobalPostprocessFlags (kForceSynchronousImport);
	
	// Trash the assetservercache in case we crash in the following
	Object* serverCache = ai.GetSingletonAsset("AssetServerCache");
	string serverCachePath = pm.GetPathName (serverCache->GetInstanceID ());
	pm.DeleteFile (serverCachePath, PersistentManager::kDontDeleteLoadedObjects);
	pm.MakeObjectPersistentAtFileID (serverCache->GetInstanceID (), 1, serverCachePath);
	
	m_NeedsMetaDataUpdate.clear ();
	m_NeedsReimport.clear ();
	m_NewlyCreatedDirectories.clear();
}

void Controller::UnlockAssets (int importFlags)
{
	//AssertIf(mono_thread_current() != NULL && mono_method_get_last_managed () != NULL);

	PersistentManager& pm = GetPersistentManager(); 
	AssetInterface& ai = AssetInterface::Get();
	Configuration::Get().DisableWorkingItemCache();

	// Refresh
	ai.Refresh (importFlags);
	//
	ai.ImportAssets (m_NeedsReimport, importFlags);
	set<UnityGUID> queued = ai.GetUpdateQueue ();

	// update cached items
	AssetServerCache &cache = AssetServerCache::Get();
	for(std::set<UnityGUID>::const_iterator i = m_NeedsMetaDataUpdate.begin(); i != m_NeedsMetaDataUpdate.end(); i++) 
	{
		cache.UpdateCachedCommitItem(*i);
		cache.UpdateCachedMetaDataItem(*i);
	}

	// Rewrite now updated assetservercache
	Object* asc = ai.GetSingletonAsset("AssetServerCache");
	string serverCachePath = pm.GetPathName (asc->GetInstanceID ());
	pm.WriteFile(serverCachePath);

	ai.StopAssetEditing ();
	ai.SetGlobalPostprocessFlags (0);

	m_NewlyCreatedDirectories.clear();
}


bool Controller::MoveAsset (const UnityGUID& guid, const UnityGUID& newParent, const string& newName) {
	string newPath = GetAssetPathName(newParent);
	if (newPath.empty ())
	{
		if (newParent == UnityGUID())
			RETURN_OK;
			
		RETURN_ERROR( "Parent directory doesn't exist. Maybe you need to include more directories in selection while downloading.");
	}

	string error = m_AssetDatabase->MoveAsset (guid, AppendPathName (newPath, newName));
	if (!error.empty ())
		RETURN_ERROR(error);
	RETURN_OK;
}

inline bool DeleteStream (const std::string& target)
{
	if(IsFileCreated(target)) {
		UnloadFileEmptied (target);
		GetPersistentManager ().UnloadStreams ();
		return DeleteFile (target);
	}
	else
		return true;
}

bool Controller::CreateAsset (const Item& info, const StreamMap& inputStreams, int options)
{
	int exclusive = options & (int)kCO_Exclusive ;
	int create = options & (int)kCO_Create ;
	int move = options & (int)kCO_Move ;
	int useServerDigest = options & (int)kCO_UseServerDigest ;
	Item retainOriginal = Item();
	
	bool hasTxtMetaStream = inputStreams.find(kTxtMetaStream) != inputStreams.end();
	bool hasBinMetaStream = inputStreams.find(kBinMetaStream) != inputStreams.end();
	//const string& metaStream = hasTxtMetaStream?kTxtMetaStream:kBinMetaStream;
	
	// Calculate asset path and meta data path
	std::map<UnityGUID,string>::iterator found = m_NewlyCreatedDirectories.find(info.parent);
	string parentPath = (found == m_NewlyCreatedDirectories.end() )?GetAssetPathName (info.parent):found->second;
	
	string assetDest = AppendPathName (parentPath, info.name);
	string binMetaDataDest =  GetMetaDataPathFromGUID (info.guid);
	string txtMetaDataDest = GetTextMetaDataPathFromAssetPath(assetDest);

	bool exists = m_NewlyCreatedDirectories.count(info.guid) > 0 || DoesAssetExist (info.guid);
	if (exists && exclusive ) 		
		RETURN_ERROR("Cannot create asset " + assetDest + " because the asset already exists");
	
	if (!exists && !create ) 		
		RETURN_ERROR("Cannot replace asset " + info.name + " because the asset doesn't exist");
	
	if ( info.parent == UnityGUID() && exists ) { // Allow updating manager assets
		assetDest = GetAssetPathName(info.guid);
		if( assetDest.empty() )
			RETURN_ERROR("Cannot replace asset " + info.name + " Asset path is empty.");
		move=0; // Library assets can not be moved
	}
	else if ( parentPath.empty () || !IsDirectoryCreated (parentPath))
	{
		RETURN_ERROR("Parent path, "+parentPath+", does not exist.");
	}
	
	// When replacing assets either move before replacing or fail if assets needs to be moved
	if (exists) {
		Item working = Configuration::Get().GetWorkingItem(info.guid);
		if (options & (int)kCO_RetainVersion ) {
			
			AssetServerCache::CachedAssetMetaData* meta = AssetServerCache::Get().FindCachedMetaData(working.guid);
			if(meta) {
				retainOriginal.changeset=meta->originalChangeset;
				retainOriginal.name=meta->originalName;
				retainOriginal.parent=meta->originalParent;
				retainOriginal.digest=meta->originalDigest;
			}
		}

		if( info.type == kTDir ) {
			if( move ) {
				if ( ! MoveAsset(info.guid,info.parent,info.name) )
					RETURN_FAIL;
			}
			goto FinishUp; // When replacing a dir asset go directly to the meta data updating
		}

		if( ! move && (working.parent != info.parent || working.name != info.name) ) {
			RETURN_ERROR("Asset current path does not match the path name it is being replaced with");
			
		}
		// Moves old asset to trash, kills cache, updates assetdatabase
		if (!m_AssetDatabase->RemoveAsset (info.guid, AssetDatabase::kDeleteAssets, NULL)) 
		{
			RETURN_ERROR("Could not move old asset to trash.");
		}
 	}
	
	{ // Scope added to enable the jump to FinishUp above

		// Verify that the destination path is not already used
		Item working = Configuration::Get().GetWorkingItemAtLocation(assetDest);
		if ( working && working.guid != info.guid ) { 
			RETURN_ERROR("There is already an asset at " + assetDest);
		}
		
		string assetSource ;
		string txtMetaDataSource = "";
		string binMetaDataSource = "";

		// Verify that the source streams are available
		if (info.type != kTDir ) {
			ErrorIf (inputStreams.find (kAssetStream) == inputStreams.end ());
			
			assetSource = inputStreams.find (kAssetStream)->second;
			if (hasTxtMetaStream)
				txtMetaDataSource = inputStreams.find (kTxtMetaStream)->second;
			if (hasBinMetaStream)
				binMetaDataSource = inputStreams.find (kBinMetaStream)->second;
			
			AssertIf ( assetSource.empty () );
			// Check if assetSource and metaData is available
			if (!IsFileCreated (assetSource))
			{
				RETURN_ERROR("Source asset, " + assetSource + ", does not exist.");
			}	
			if ( !txtMetaDataSource.empty() && !IsFileCreated (txtMetaDataSource))
			{
				RETURN_ERROR("Source asset meta data, " + txtMetaDataSource + ", does not exist.");
			}
			if ( !binMetaDataSource.empty() && !IsFileCreated (binMetaDataSource))
			{
				RETURN_ERROR("Source asset meta data, " + binMetaDataSource + ", does not exist.");
			}
		}
		
		AssertIf(info.type == kTDir && inputStreams.size () > 0);

		// This only applies to binary metaData
		if (!binMetaDataSource.empty() )
		{
			// Create metadata directory
			if (!CreateDirectory (DeleteLastPathNameComponent (binMetaDataDest)))
			{
				RETURN_ERROR("Could not create meta data directory, " + binMetaDataDest);
			}

			// Delete metadatapath mappings, don't use the serialization system since
			// that annulls any ptr connections
			if(! DeleteStream(binMetaDataDest) )
				RETURN_ERROR("Could not delete existing meta data, " + binMetaDataDest)
		
		}
		
		// Create guid <-> pathname mapping
		GetGUIDPersistentManager ().ForceCreateDefinedAsset (GetActualPathSlow(assetDest), info.guid);
		
		// Asset is directory
		if( info.type == kTDir )
		{
			// Try to catch where we're creating ".." folders
			AssertIf(info.name == "..");

			m_NewlyCreatedDirectories[info.guid]=assetDest; // and this is our own mapping
			// Create actual directory
			if (!CreateDirectory (assetDest))
			{
				RETURN_ERROR("Failed to create asset directory at " + assetDest);
			}	
		}
		// Asset is a file asset
		else {
			if (!MoveReplaceFile (assetSource, assetDest))
			{
				RETURN_ERROR("Failed to move asset from " + assetSource + " to " + assetDest);
				
			}	
			if (!txtMetaDataSource.empty() && !MoveReplaceFile (txtMetaDataSource, txtMetaDataDest))
			{
				RETURN_ERROR("Failed to move meta data from " + txtMetaDataSource + " to " + txtMetaDataDest);
				
			}	
			if (!binMetaDataSource.empty() && !MoveReplaceFile (binMetaDataSource, binMetaDataDest))
			{
				RETURN_ERROR("Failed to move meta data from " + binMetaDataSource + " to " + binMetaDataDest);
				
			}	
		}

		m_NeedsReimport[info.guid]=useServerDigest?info.digest:MdFour();
		
		if( exists ) { // This is only done when replacing assets
			// Is this a global manager? Then import it immediately, because global manager should always be available! 
			int temp;
			if (LibraryAssetImporter::CanLoadPathName(assetDest, &temp))
				AssetInterface::Get().ImportAtPathImmediate(assetDest);
		}
	}

	FinishUp:

	////@TODO: Make sure that asset store preview images for new packages still work????
	

	// Setup metadata with new asset server information.
	{
		string binaryMetaData = GetMetaDataPathFromGUID (info.guid);
		
		// Associate new assets with the correct guid
		AssetMetaData* metaData = CreateOrLoadAssetMetaDataAtPath (binaryMetaData);
		AssertIf (metaData == NULL);
		metaData->pathName = assetDest;
		metaData->guid = info.guid;
		metaData->SetDirty ();
		
		int err;
		if ((err=GetPersistentManager ().WriteFile (binaryMetaData)) != kNoError)
		{
			RETURN_ERROR(Format("Failed to update meta data at path %s. (error: %d)", binaryMetaData.c_str(), err));
		}
		
		AssetServerCache::CachedAssetMetaData versionData;
		versionData.pathName = assetDest;
		versionData.guid = info.guid;
		if ( retainOriginal.changeset < info.changeset )
			retainOriginal=info;
		versionData.originalChangeset=retainOriginal.changeset;
		versionData.originalName=retainOriginal.name;
		versionData.originalParent=retainOriginal.parent;
		versionData.originalDigest=retainOriginal.digest;
		
		AssetServerCache::Get().SaveCachedMetaDataItem(versionData);
	}


	RETURN_OK;
}

bool Controller::LocateChildAsset (const UnityGUID& parent, const string& name, UnityGUID* out_guid) {
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	string parentPath = GetAssetPathName(parent);
	string assetPath = AppendPathName( parentPath, name);
	return pm.PathNameToGUID(assetPath, out_guid) && DoesAssetExist(*out_guid) ;
}

bool IsUnityTextSerializedFile (const string& assetPath)
{
	const int magiclen = strlen(kUnityTextMagicString);
	char compare[256];	
	if (ReadFromFile (assetPath, compare, 0, magiclen))
	{
		compare[magiclen] = '\0';
		return (strcmp(compare, kUnityTextMagicString) == 0);
	}
	
	return false;
}

bool Controller::AssetIsBinary(const string& assetPath)
{
	return false;/* Code was not used...
	pair<int, int> typeAndImporter = AssetImporter::FindAssetTypeAndImporterClass (assetPath);
	string assetExtension = ToLower(GetPathNameExtension(assetPath));
	string assetFilename = ToLower(GetLastPathNameComponent(assetPath));

	if (typeAndImporter.second > 0)
	{
		string importerName = Object::ClassIDToString (typeAndImporter.second);

		if (importerName == "MonoImporter" )
			return !StrICmp(assetExtension, "dll");
		if (importerName == "ShaderImporter" || importerName == "TextScriptImporter")
			return false;
		if (importerName != "DefaultImporter")
			return !IsUnityTextSerializedFile (assetPath);
	}

	// Note most text assets are caught by the Shader, TextScript and MonoImporters
	// TODO: do some fancy heuristics to determine if file is a plain text file.
	if( assetExtension == "rtf" || assetExtension == "cpp" || assetExtension == "c" || assetExtension == "h" || assetExtension == "java"  || assetExtension == "prefs" || 
		assetExtension == "mm" || assetExtension == "m" || assetExtension == "pl" || assetExtension == "py" || assetExtension == "pm"  || assetExtension == "ini" ||
		assetExtension == "command" || assetExtension =="bat" || assetFilename == "doxyfile" || assetFilename == "makefile" )
	{
		return false;
	}

	return !IsUnityTextSerializedFile (assetPath);*/
}

bool Controller::AssetIsBinary(const UnityGUID& asset) 
{
	return AssetIsBinary(GetAssetPathName( asset ));
}

bool Controller::RemoveAsset (const UnityGUID& guid) {
	return AssetInterface::Get ().MoveToTrash (guid);
}

StreamMap Controller::GetAssetStreams (const UnityGUID& guid, bool includeBinaryMetaData) {
	StreamMap streams;

	/// Get Asset path
	/// - use file system to convert from lowercase only path to real path with upper&lower case characters
	string assetPath = GetAssetPathName (guid);
	FatalErrorIf (assetPath.empty ());
	if(!IsPathCreated (assetPath))
		FatalErrorString(assetPath + " is neither a folder or a file");
		
	streams[kAssetStream] = assetPath;
	
	if(!IsDirectoryCreated (assetPath) )
	{
		string rsrcPath = assetPath + "/..namedfork/rsrc";
		// Note: FileObject2 does not give us access to the resource fork. Therefore this little workaround using stat.
		struct stat info;
		if( stat(rsrcPath.c_str(), &info) == 0 && info.st_size > 0 )
			streams[kAssetResourceStream] = rsrcPath;
	}

	// Assets not inside the Assets folder do not have text metadata files
	if ( StrNICmp(assetPath.c_str(), "Assets", 6) == 0 )
	{
		/// Get .meta path
		string path = GetTextMetaDataPathFromAssetPath(assetPath);
		if ( IsFileCreated (path) )
			streams[kTxtMetaStream] = path;
		else if (! includeBinaryMetaData )
			FatalErrorString("Could not locate meta file at: " + path);
	}
	
	/// Get metadata path. (Required by ExportPackage)
	if ( includeBinaryMetaData || RequiresExternalImporter(assetPath) )
	{
		string path = GetMetaDataPathFromGUID (guid);
		FatalErrorIf (path.empty ());
		streams[kBinMetaStream] = path;
	}
	
	return streams;
}

string Controller::AssetInfoToPathName (const Item& info)
{
	string parentPath = GetGUIDPersistentManager ().AssetPathNameFromGUID (info.parent);
	return AppendPathName (parentPath, info.name);
}

bool Controller::UpdateStatus() 
{
	if(!m_Online) RETURN_ERROR("Client is off-line. Please go online before accessing the asset server");

	if (!LicenseInfo::Flag (lf_maint_client))
	{
		// make sure nothing works at all when user does not have license
		// don't show any errors, as only way user will get here is by hacking protection (or some weird bug)
		RETURN_FAIL;
	}

	Backend b(GetConnectionSettings(), GetMaximumTimout(), GetLogId() );

	int lastDownloadedChangeset = AssetServerCache::Get().GetLatestServerChangeset();
	int latest = b.GetLatestServerChangeset();

	bool ok = true;

	if (latest != lastDownloadedChangeset)
	{
		AutoLockAssets assetLock(*this);

		if(! b.UpdateConfiguration(Configuration::Get()) ) 
		{
			ok = false;
		}
		else
		{
			AssetServerCache::Get().SetLatestServerChangeset(latest);
		}
	}

	if (ok)
	{
		RETURN_OK;
	}
	else
	{
		RETURN_FAIL;
	}
}

// Revert working assets
bool Controller::RevertVersion(const UnityGUID& asset, int version) {
	return RevertIMPL(asset,version,string(),UnityGUID(), kCO_Move | kCO_RetainVersion | kCO_Create, false); 
}

bool Controller::RevertVersion(const UnityGUID& asset, int version, const string& iname, const UnityGUID& iparent, bool forceParent) {
	return RevertIMPL(asset,version,iname,iparent, kCO_Move | kCO_RetainVersion | kCO_Create, forceParent); 
}

// Revert working assets
bool Controller::RecoverDeleted(const UnityGUID& asset, int version, const string& iname, const UnityGUID& iparent) {
	return RevertIMPL(asset,version,iname,iparent,  kCO_Create | kCO_Exclusive, false ); 
}

 
 
bool Controller::RevertIMPL(const UnityGUID& asset, int version, const string& iname, const UnityGUID& iparent, int createOptions, bool forceParent) {
	if(!m_Online) RETURN_LOG_ERROR("Client is off-line. Please go online before accessing the asset server");
	Backend b(GetConnectionSettings(), GetMaximumTimout(), GetLogId() );
	Configuration& conf = Configuration::Get();

	AutoLockAssets assetLock(*this);

	Item item = conf.GetServerItem(asset, version);
	Item working = conf.GetWorkingItem(asset);
	if( ! item ) RETURN_LOG_ERROR("Unknown asset version");
	
	if ( iname != string() ) item.name=iname;
	if ( iparent != UnityGUID() ) item.parent=iparent;
	
	if( !forceParent && item.parent != UnityGUID() && !DoesAssetExist(item.parent) ) { // if the parent asset directory doesn't exist locally
		item.parent = UnityGUID(0,0,1,0); // create the restored asset in the root asset dorectory (0,0,1,0 is the guid of the Assets directory)
	}
	
	// Rename the restored asset if it conflicts with an existing asset in the current version
	if(!working || working.parent != item.parent || working.name != item.name) {
		set<string> forbidden ;
		conf.GetOtherAssetNamesInDirectory(item.guid, item.parent, &forbidden);
		string uniquePath=GetGUIDPersistentManager ().GenerateUniqueAssetPathName(item.parent, item.name, &forbidden );
		item.name = GetLastPathNameComponent(uniquePath);			
	}
	
	if ( item.type == kTDir) {
		item.changeset = -1;
		if (! CreateAsset (item, StreamMap(), createOptions | kCO_UseServerDigest ) )
			RETURN_LOG_ERROR( Format("Unable to create restored directory %s assetversion %i: %s", AssetInfoToPathName (item).c_str() , item.changeset,  GetLastError().c_str() ));
	}
	else {
		vector<DownloadItem> toDownload;
		DownloadItem tmp;
		tmp.guid = asset;
		tmp.changeset = version;
		toDownload.push_back(tmp);
		
		if(! b.DownloadAssets( toDownload, kTempPath) )
			RETURN_FAIL;
		
		item.changeset = -1;

		if (! CreateAsset(item, toDownload[0].streams, createOptions))
			RETURN_LOG_ERROR( "Unable to create restored file asset (" + item.name + ") received from server: " + GetLastError() );
		
	}
	RETURN_OK;
}

// Compare
bool Controller::CompareFiles(const map<UnityGUID,CompareInfo>& selection) 
{
	if (selection.size() == 0)
		RETURN_LOG_ERROR( "Empty selection" );

	int idx = 0;

	for (map<UnityGUID,CompareInfo>::const_iterator i = selection.begin(); i != selection.end(); i++) 
	{
		idx++;
		DisplayProgressbar("Compare", "Initiating compare...", (float)idx/selection.size());

		if(! CompareSingleFile (i->first, i->second))
		{
			ClearProgressbar();
			RETURN_FAIL;
		}
		Thread::Sleep(0.001f); // To avoid launching more than one instance of FileMerge
	}

	ClearProgressbar();
	RETURN_OK;
}



static std::string GetBinaryConverterPath ()
{
	#if UNITY_OSX || UNITY_LINUX
	const char* kBinaryToTextTool = "Tools/binary2text";
	#elif UNITY_WIN
	const char* kBinaryToTextTool = "Tools/binary2text.exe";
	#else
	#error "Unknown platform"
	#endif
	return AppendPathName (GetApplicationContentsPath (), kBinaryToTextTool);
}


static bool InvokeBinaryConverter (const std::string& inputPath, const std::string& outputPath, std::string& outError)
{
	string converterPath = GetBinaryConverterPath ();
	return LaunchTask (converterPath, &outError, PathToAbsolutePath(inputPath).c_str(), PathToAbsolutePath(outputPath).c_str(), NULL);
}


enum { kStraightAssetFile = 1 << 0, kConvertedAssetFile = 1 << 1, kConvertedMetaData = 1 << 2, kAttemptAssetConversion = 1 << 3 };

bool Controller::ConvertToTextFile (const string& assetPath, const string& metaDataPath, const string& txtMetaPath, const string& outPath, int options)
{
	InputString data;
	string error;
	if (options & kConvertedAssetFile)
	{
		InvokeBinaryConverter (assetPath, outPath, error);
		if (!error.empty ())
			RETURN_LOG_ERROR( error );
		if (!ReadStringFromFile (&data, outPath))
			RETURN_LOG_ERROR( "Failed reading text format asset file!" );
	}
	else if (options & kStraightAssetFile)
	{
		if (!ReadStringFromFile (&data, assetPath))
			RETURN_LOG_ERROR( "Failed reading asset "+ assetPath +" file!" );
	}
	else if (options & kAttemptAssetConversion)
	{
		if (InvokeBinaryConverter (assetPath, outPath, error))
			ReadStringFromFile (&data, outPath);
	}
	
	if (options & kConvertedMetaData)
	{
		InvokeBinaryConverter (metaDataPath, outPath, error);
		if (!error.empty ())
			RETURN_LOG_ERROR( error );
		InputString newData;
		if (!ReadStringFromFile (&newData, outPath))
			RETURN_LOG_ERROR( "Failed reading binary meta data file!" );
		
		data += "\n\n------MetaData------\n\n";
		data.append (newData);
	}
	if (txtMetaPath != "" && IsFileCreated(txtMetaPath) )
	{
		InputString newData;
		if (!ReadStringFromFile (&newData, txtMetaPath))
			RETURN_LOG_ERROR( "Failed reading text format meta data file!" );
		
		data += "\n\n------Import Settings------\n\n";
		data.append (newData);
	}
	if (!WriteStringToFile (std::string(data.c_str(), data.size()), outPath, kProjectTempFolder, kFileFlagDontIndex|kFileFlagTemporary))
		RETURN_LOG_ERROR( "Couldn't write converted text file!" );

	RETURN_OK;
}

void Controller::SetupStreams(UnityGUID guid, CompareFileData &data, vector<DownloadItem> &toDownload, int otherChangeset)
{
	Configuration &conf = Configuration::Get();

	if (data.changeset == -1 )
	{
		StreamMap localAssetStreams = GetAssetStreams(guid);

		data.item = conf.GetWorkingItem(guid);
		data.item.name = NicifyAssetName(data.item.name);
		data.label = "Local copy of " + data.item.name;
		data.filename = localAssetStreams[kAssetStream];
		if (localAssetStreams.find(kTxtMetaStream) != localAssetStreams.end()) 
		{
			data.txtMeta = localAssetStreams[kTxtMetaStream];
		}
		
		else if (localAssetStreams.find(kBinMetaStream) != localAssetStreams.end()) 
		{
			data.meta = localAssetStreams[kBinMetaStream];
		}
		
	}
	else
	{
		// file was "created" or "deleted"
		if (data.changeset == -2)
		{ 
			data.filename = GenerateUniquePathSafe ("Temp/tempFile");
			data.item = conf.GetServerItem(guid, otherChangeset);
			data.item.name = NicifyAssetName(data.item.name);
		} else
		{
			DownloadItem tmp;
			tmp.guid = guid;
			tmp.changeset = data.changeset;

			data.download_index = toDownload.size();
			toDownload.push_back(tmp);

			data.item = conf.GetServerItem(guid, data.changeset);
			data.item.name = NicifyAssetName(data.item.name);
		}
	}
}

void Controller::SetupStreamFilesAndLabels(CompareFileData &data, vector<DownloadItem> &toDownload, string labelString)
{
	if (data.download_index > -1)
	{
		data.filename = toDownload[data.download_index].streams[kAssetStream];
		data.meta = toDownload[data.download_index].streams[kBinMetaStream];
		data.txtMeta = toDownload[data.download_index].streams[kTxtMetaStream];
	}

	if (data.label == "")
		data.label = Format("%s %d of %s", labelString.c_str(), data.changeset, data.item.name.c_str());
}

void Controller::HandleNonExistingFile(CompareFileData &data, CompareFileData &otherData)
{
	if (!data.filename.empty())
	{
		if (!IsFileCreated(data.filename))
		{
			// did we actually received something from streams (happens in cases when file was moved both in this and prev changesets)
			if (data.changeset >= 0)
			{
				CopyFileOrDirectory(otherData.filename, data.filename);
				
				if ( !otherData.txtMeta.empty() && !data.txtMeta.empty() )
					CopyFileOrDirectory(otherData.txtMeta, data.txtMeta);
				else if ( !otherData.meta.empty() && !data.meta.empty() )
					CopyFileOrDirectory(otherData.meta, data.meta);
			}else
			{
				CreateFile(data.filename); // empty file for showing
				data.label = Format("Nothing in this version of %s", data.item.name.c_str());
			}
		}
	}
}

bool Controller::ConvertToBinary(CompareFileData &data)
{
	if (data.filename == "")
		return true;

	string tmp;
	tmp = GenerateUniquePathSafe ("Temp/tempFile");

	int conversionFlags = kConvertedMetaData | kAttemptAssetConversion;
	int flags = data.meta == "" ? conversionFlags - kConvertedMetaData : conversionFlags;

	if (!ConvertToTextFile (data.filename, data.meta, data.txtMeta, tmp, flags))
		RETURN_FAIL;

	data.filename = tmp;

	return true;
}

bool Controller::CompareSingleFile (const UnityGUID& guid, const CompareInfo& opts)
{
	static string empty = "";

	if(!m_Online) RETURN_ERROR("Client is off-line. Please go online before accessing the asset server");
	Backend b(GetConnectionSettings(), GetMaximumTimout(), GetLogId(), true );
	Configuration& conf = Configuration::Get();

	CompareFileData left;
	CompareFileData right;
	CompareFileData ancestor;

	left.changeset = opts.left;
	right.changeset = opts.right;

	conf.GetServerItem(guid);
	conf.GetWorkingItem(guid);

	vector<DownloadItem> toDownload;

	SetupStreams(guid, left, toDownload, right.changeset);
	SetupStreams(guid, right, toDownload, left.changeset);
	
	// FIXME: if you know any use cases for this please tell me, merging of conflicts is done directly without this "compare single file" thing
	/*if (opts.conflict_ancestor)
	{
		DownloadItem tmp;
		tmp.guid = guid;
		tmp.changeset = opts.conflict_ancestor;
		ancestor.download_index = toDownload.size();
		toDownload.push_back(tmp);
	}*/
	
	if( toDownload.size() > 0 ) 
	{
		if(! b.DownloadAssets( toDownload, kTempPath) )
			RETURN_FAIL;
	}
	
	SetupStreamFilesAndLabels(left, toDownload, "Version");
	SetupStreamFilesAndLabels(right, toDownload, "Version");
	//SetupStreamFilesAndLabels(ancestor, toDownload, "Common ancestor");

	HandleNonExistingFile(left, right);
	HandleNonExistingFile(right, left);

	bool forceBinary = opts.autodetect_binary && AssetIsBinary(left.item.name == "" ? right.item.name : left.item.name);

	if (opts.convert_binary || forceBinary) 
	{
		if (left.filename != "" && !ConvertToBinary(left)) 
			RETURN_FAIL;
		if (right.filename != "" && !ConvertToBinary(right)) 
			RETURN_FAIL;
		if (ancestor.filename != "" && !ConvertToBinary(ancestor)) 
			RETURN_FAIL;
	}
	
	// remote, local, ancestor
	if (! InvokeCompareApp(
						left.label,
						left.filename,  // remote
						right.label,
						right.filename, // local
						ancestor.label,
						ancestor.filename)) 
	{
		RETURN_FAIL;
	}

	RETURN_OK;
}

// Update from server:
bool Controller::UpdateBegin(const set<UnityGUID>& candidates, bool deleteLocal, bool usedForRevert) 
{
	if(!m_Online) RETURN_ERROR("Client is off-line. Please go online before accessing the asset server");
	if(m_Update)
		delete m_Update;
	
	m_Update=new UpdateHandle(this, candidates, deleteLocal, usedForRevert);
	if( m_Update->IsEmpty() ) {
		delete m_Update;
		m_Update = NULL;
		return false;
	}
	
	bool success =  m_Update->Init();
	if(! success ) {
		delete m_Update;
		m_Update=NULL;
	}
	return success;
}

bool Controller::UpdateGetConflicts(set <UnityGUID>* conflicts){
	if(! m_Update)
		RETURN_LOG_ERROR("You have to call UpdateBegin before calling UpdateGetConflicts");

	bool success =  m_Update->GetConflicts(conflicts); 
	if(! success ) {
		delete m_Update;
		m_Update=NULL;
	}
	return success;
}

bool Controller::UpdateSetResolutions(const map<UnityGUID, AssetServer::DownloadResolution>& resolutions){
	if(! m_Update)
		RETURN_LOG_ERROR("You have to call UpdateBegin before calling UpdateSetResolutions");
	
	bool success =  m_Update->SetResolutions(resolutions);
	if(! success ) {
		delete m_Update;
		m_Update=NULL;
	}
	return success;
	
}

bool Controller::UpdateStartDownload() {
	if(! m_Update)
		RETURN_LOG_ERROR("You have to call UpdateBegin before calling UpdateStartDownload");
	
	
	return  m_Update->Download();

}

float Controller::UpdateGetDownloadProgress(string& text) {
	if(! m_Update)
		RETURN_LOG_ERROR("You have to call UpdateBegin before calling UpdateGetDownloadProgress");
	
	return m_Update->GetProgress(text);
}


bool Controller::UpdateComplete() {
	if(! m_Update)
		RETURN_LOG_ERROR("You have to call UpdateStartDownload before calling UpdateComplete");

	bool success = m_Update->Complete();
	
	delete m_Update;
	m_Update=NULL;

	return success;
}


bool Controller::UpdateAbort() {
	if( m_Update ) {
		delete m_Update;
		m_Update=NULL;
	}
	
	RETURN_OK;
}

void Controller::DoUpdateOnNextTick(bool forceShowConflictResolutions, string backendFunctionForConflictResolutions, const map<UnityGUID, DownloadResolution>& dRes, const vector<UnityGUID>& nConf)
{
	m_needToFinishAction = kNTFUpdate;
	m_downloadResolutions = dRes;
	m_nameConflicts = nConf;
	m_forceShowConflictResolutions = forceShowConflictResolutions;
	m_backendFunctionForConflictResolutions = backendFunctionForConflictResolutions;
}

void Controller::DoCommitOnNextTick(const string& description, const set<UnityGUID>& guids)
{
	m_commitDescription = description;
	m_commitItems = guids;
	m_needToFinishAction = kNTFCommit;
}

void Controller::DoRecoverOnNextTick(const vector<DeletedAsset>& sortedList)
{
	m_needToFinishAction = kNTFRecover;
	m_sortedRecoverList = sortedList;
}

void Controller::DoRevertOnNextTick(int changeset, const UnityGUID& guid)
{
	m_needToFinishAction = kNTFRevert;
	m_revertItem = guid;
	m_revertChangeset = changeset;
}

void Controller::DoUpdateStatusOnNextTick()
{
	m_needToFinishAction = kNTFStatus;
}

void Controller::DoRefreshAssetsOnNextTick()
{
	m_needToFinishAction = kNTFRefresh;
}

void Controller::DoRefreshAssetsAndUpdateStatusOnNextTick()
{
	m_needToFinishAction = kNTFStatusAndRefresh;
}

void Controller::DoUpdateWithoutConflictResolutionOnNextTick(const set<UnityGUID>& guids)
{
	m_updateItems = guids;
	m_needToFinishAction = kNTFUpdateWithoutConflicts;
}

void Controller::DoShowDialogOnNextTick(string title, string text, string button1, string button2)
{
	m_DialogTitle = title;
	m_DialogText = text;
	m_DialogButton1 = button1;
	m_DialogButton2 = button2;
	m_needToFinishAction = kNTFShowDialog;
}

void Controller::CallMonoAfterActionFinishedCallback()
{
	if (m_actionFinishedCallbackClass == "" || m_actionFinishedCallbackFunction == "")
		return;

	string className = m_actionFinishedCallbackClass;
	string functionName = m_actionFinishedCallbackFunction;

	m_actionFinishedCallbackClass = "";
	m_actionFinishedCallbackFunction = "";

	MonoClass* klass = GetMonoManager().GetMonoClass(className.c_str(), "UnityEditor");

	AssertIf(!klass);

	ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(klass,functionName.c_str(), ScriptingMethodRegistry::kStaticOnly);

	AssertIf(!method);

	ScriptingInvocation invocation(method);
	invocation.AddInt((int)m_lastActionResult);
	invocation.Invoke();
}

void Controller::SetAfterActionFinishedCallback(const string& className, const string& functionName)
{
	m_actionFinishedCallbackClass = className;
	m_actionFinishedCallbackFunction = functionName;
}

void Controller::Tick()
{
	if (m_needToFinishAction <= kNTFBusy)
		return;

	// Asset performs actual operations from here.
	// This is called just from Application::TickTimer, and is never re-entrant (since TickTimer
	// checks for re-entrancy and does early out).

	int action = m_needToFinishAction;
	m_needToFinishAction = kNTFBusy;

	ClearError();
	m_actionDidNotFinish = false;

	// Since server operations will modify assets, we don't want changes to bring up warning (unapplied changes)
	// dialogs from inspectors when they find the change, and so on.
	// Set the flag so that inspector can ignore those situations.

	Application& application = GetApplication();
	AssertIf(application.GetIgnoreInspectorChanges());
	application.SetIgnoreInspectorChanges(true);

	switch (action)
	{
	case kNTFUpdate:
		m_lastActionResult = DoUpdate();
		break;
	case kNTFCommit:
		m_lastActionResult = FinishMonoCommit();
		break;
	case kNTFRecover:
		m_lastActionResult = FinishMonoRecover();
		break;
	case kNTFRevert:
		m_lastActionResult = FinishMonoRevert();
		break;
	case kNTFStatus:
		m_lastActionResult = FinishMonoUpdateStatus();
		break;
	case kNTFRefresh:
		m_lastActionResult = FinishMonoRefreshAssets();
		break;
	case kNTFStatusAndRefresh:
		if (ASMonoUtility::Get().GetRefreshUpdate())
			m_lastActionResult = FinishMonoUpdateStatus();

		if (m_lastActionResult)
			m_lastActionResult = FinishMonoRefreshAssets();
		else
			FinishMonoRefreshAssets();
		break;
	case kNTFUpdateWithoutConflicts:
		m_lastActionResult = DoUpdateWithoutConflictResolution(); 
		break;
	case kNTFShowDialog:
		m_lastActionResult = DoShowDialog();
		break;
	}

	application.SetIgnoreInspectorChanges(false);

	if (!m_actionDidNotFinish)
		CallMonoAfterActionFinishedCallback();

	if (m_needToFinishAction == kNTFBusy)
		m_needToFinishAction = kNTFNone;
}

bool Controller::FinishMonoUpdateStatus()
{
	static bool insideStatus = false;

	if (insideStatus) 
	{
		LogError("Recursive DoStatus???");
		return false;
	}

	insideStatus = true;
	bool status = UpdateStatus();
	insideStatus = false;

	Configuration::Get().GetCachedPaths();

	return status;
}

bool Controller::FinishMonoRefreshAssets()
{
	// never save managers while in play mode
	if (!IsWorldPlaying())
	{
		AssetInterface& i = AssetInterface::Get();
		i.SaveAssets();
		i.Refresh();
		return true;
	}

	return true;
}

bool Controller::FinishMonoUpdate()
{
	float progress;
	string progressText;

	if (!UpdateStartDownload())
	{
		UpdateAbort();
		return false;
	}

	while( (progress = UpdateGetDownloadProgress(progressText)) < 0.999 ) 
	{
		Thread::Sleep(0.1f);

		if (DisplayProgressbar( (progress>0.0 || progress < -1 )?"Downloading updates":"Starting update", progressText, progress, true) == kPBSWantsToCancel)
		{
			DisplayProgressbar("Aborting update", "Aborting...", progress, false);
			UpdateAbort();
			Configuration::Get().SetStickyChangeset(-1);
			ClearProgressbar();
			RETURN_LOG_ERROR("Update operation was canceled by user");
		}
	}

	ClearProgressbar();

	bool success = UpdateComplete();

	// Clear resolutions if the download was accepted
	if ( success )
	{
		for (map<UnityGUID, DownloadResolution>::const_iterator i = m_downloadResolutions.begin(); i != m_downloadResolutions.end(); i++)
		{
			if (i->second != kDLSkipAsset)
				SetDownloadResolution(i->first, kDLUnresolved);			
		}

		for (vector<UnityGUID>::const_iterator i = m_nameConflicts.begin(); i != m_nameConflicts.end(); i++)
			SetNameConflictResolution(*i, kNMUnresolved);
	}

	m_downloadResolutions.clear();
	m_nameConflicts.clear();

	Configuration::Get().SetStickyChangeset(-1);

	if (success)
		ASMonoUtility::Get().SetNeedsToRefreshUpdate();

	return success;
}

int ShowASConflictResolutionsWindow(const set<UnityGUID> &cconflicting, string backendFunction); // ASMonoUtility.cpp

bool Controller::DoUpdate()
{
	// try updating, see if there are unresolved conflicts, 
	// if not - finish update, else abort update and tell mono to show conflict resolutions window
	AssetDatabase& assetDb = AssetDatabase::Get();
	Configuration& conf = Configuration::Get();

	set<UnityGUID> guids;
	set<UnityGUID> conflicting;
	map<UnityGUID, DownloadResolution> resolutions;

	bool hasConflicts = false;
	bool allResolved = true;

	DisplayProgressbar("Updating", "Initializing...", 0);

	set<UnityGUID> roots = assetDb.GetAllRootGUIDs();
	guids.insert(roots.begin(), roots.end());
	assetDb.CollectAllChildren (kAssetFolderGUID, &guids);

	if (guids.empty())
		guids = assetDb.GetAllRootGUIDs();

	DisplayProgressbar("Updating", "Initializing...", 0.5f);

	if (!UpdateBegin( guids, false, false ))
	{
		ClearProgressbar();
		return false;
	}

	DisplayProgressbar("Updating", "Getting conflicts...", 0);
	if (!UpdateGetConflicts(&conflicting))
	{
		ClearProgressbar();
		return false;
	}

	ClearProgressbar();

	hasConflicts = conflicting.size() != 0;

	for(set<UnityGUID>::const_iterator i = conflicting.begin(); i != conflicting.end(); i++)
	{
		Item item = conf.GetWorkingItem(*i);

		if (item.GetStatus() == kConflict) 
		{
			DownloadResolution res = GetDownloadResolution(*i);
			resolutions[*i] = res;
			if (res == kDLUnresolved)
			{
				allResolved = false;
				break;
			}
		}

		UnityGUID conflictGUID = conf.GetPathNameConflict(*i);
		if( conflictGUID != UnityGUID() ) 
		{
			NameConflictResolution res = GetNameConflictResolution(*i);

			// Duplicate resolution to corresponding server asset
			SetNameConflictResolution(conflictGUID, res);

			if (res == kNMUnresolved)
			{
				allResolved = false;
				break;
			}
		}
	}

	if (allResolved && !(hasConflicts && m_forceShowConflictResolutions))
	{
		if (!UpdateSetResolutions(resolutions))
			return false;

		return FinishMonoUpdate();
	}
	else
	{
		UpdateAbort();
		ShowASConflictResolutionsWindow(conflicting, m_backendFunctionForConflictResolutions);
		m_actionDidNotFinish = true;
		return false;
	}
	return false;
}

bool Controller::DoUpdateWithoutConflictResolution()
{
	if (!UpdateBegin(m_updateItems, true))
		return false;

	return FinishMonoUpdate();
}

bool Controller::DoShowDialog()
{
	return DisplayDialog(m_DialogTitle, m_DialogText, m_DialogButton1, m_DialogButton2);
}

bool Controller::FinishMonoCommit()
{
	float progress;
	string progressText;

	if (!CommitBegin(m_commitDescription, m_commitItems))
	{
		return false;
	}

	if (!CommitStartUpload())
	{
		CommitAbort();
		return false;
	}

	while( (progress = CommitGetUploadProgress(progressText)) < 0.999 ) 
	{
		Thread::Sleep(0.1f);

		if (DisplayProgressbar(progress>0.0?"Uploading assets to server":"Starting commit", progressText, progress, true) == kPBSWantsToCancel)
		{
			DisplayProgressbar("Aborting commit", "Aborting...", progress, false);
			CommitAbort();
			ClearProgressbar();
			RETURN_LOG_ERROR("Commit operation was canceled by user");
		}
	}

	DisplayProgressbar("Committing", "Finishing up...", 1);

	m_Commit->ClearDeleted();

	bool success = CommitComplete();

	ClearProgressbar();

	ASMonoUtility::Get().SetNeedsToRefreshCommit();

	return success;	
}

bool Controller::FinishMonoRecover()
{
	bool allOk = true;

	int index = 0;
	int total = m_sortedRecoverList.size();

	for(vector<DeletedAsset>::const_iterator i = m_sortedRecoverList.begin() ; i != m_sortedRecoverList.end(); i++) 
	{
		DisplayProgressbar("Recovering", Format("Recovering assets (%i/%i)", ++index, total), (float)index/total);

		if (!RecoverDeleted(i->guid, i->changeset, i->name, i->parent ))
			allOk = false;
	}

	ClearProgressbar();

	ASMonoUtility::Get().SetNeedsToRefreshCommit();

	return allOk;
}

bool Controller::FinishMonoRevert()
{
	/*if (m_revertItems.size() == 0) // revert all
	{
		set<UnityGUID> guids; // Passing an empty guid array will get all changesets
		vector<Changeset> assetHistory;

		if(!Configuration::Get().GetHistory(guids, &assetHistory))
			return false;

		Configuration::Get().SetStickyChangeset(m_revertChangeset);
		// TODO: Do the hole update crap here
		Configuration::Get().SetStickyChangeset(-1);
	}else // Revert selected files only
	{*/

	if ( !RevertVersion ( m_revertItem, m_revertChangeset) )
		return false;

	ASMonoUtility::Get().SetNeedsToRefreshCommit();

	return true;
}


Controller::UpdateHandle::UpdateHandle(Controller* controller, const set<UnityGUID>& selection,  bool deleteLocal, bool usedForRevert) 
	: m_Controller(controller)
	, m_DeleteLocal(deleteLocal)
	, m_Candidates(new set<UnityGUID>())
	, m_Conflicts(new set<UnityGUID>())
	, m_Resolutions(new map<UnityGUID, AssetServer::DownloadResolution>)
	, m_Changes(new map<UnityGUID, Item>())
	, m_Locked(false)
	, m_UsedForRevert(usedForRevert)
	, m_Client(new Backend(controller->GetConnectionSettings(), controller->GetMaximumTimout(), controller->GetLogId() ))
{
	m_Candidates->insert(selection.begin(), selection.end());
	
	// never delete root assets that are local only
	if( deleteLocal )
	{
		const set<UnityGUID> roots = AssetDatabase::Get().GetAllRootGUIDs ();
		Configuration& conf = Configuration::Get();
		for (set<UnityGUID>::iterator i = m_Candidates->begin(); i != m_Candidates->end(); /**/ ) {
			if( roots.find(*i) != roots.end() && conf.GetWorkingItem(*i).GetStatus() == kClientOnly )
				m_Candidates->erase(i++);
			else
				++i;
		}
	}
}

Controller::UpdateHandle::~UpdateHandle(){
	Cleanup();
	delete m_Client;
	delete m_Candidates;
	delete m_Conflicts;
	delete m_Resolutions;
	delete m_Changes;
}

int Controller::GetLatestServerChangeset()
{
	if (!m_Online) RETURN_ERROR("Client is off-line. Please go online before accessing the asset server");
	Backend b(GetConnectionSettings(), GetMaximumTimout(), GetLogId() );
	return b.GetLatestServerChangeset();
}


bool Controller::UpdateHandle::Init() {
	if(m_Locked)
		DOWNLOAD_OK;

	if (m_Candidates->size() == 0)
		DOWNLOAD_LOG_ERROR( "Empty selection" );

	CreateDirectory(kTempPath);
	Configuration& conf = Configuration::Get();
	m_Changes->clear();

	if(!m_Client->UpdateConfiguration(conf)) DOWNLOAD_FAIL;

	vector<Item> changeList;
	conf.GetChanges(&changeList , *m_Candidates);
	for(vector<Item>::iterator i=changeList.begin(); i!=changeList.end(); i++)
		m_Changes->insert(make_pair(i->guid,*i));
	m_Controller->LockAssets();
	m_Locked=true;
	
	InitConflicts();
	
	startTime = GetTimeSinceStartup();
	progress = -1;
	progressText = "Starting update from server...";

	DOWNLOAD_OK;
}

void Controller::UpdateHandle::Cleanup() 
{
	if (! m_Locked) return;
	if (m_Thread.IsRunning()) {
		m_Thread.WaitForExit();
	}
		
	m_Controller->UnlockAssets();
	m_Locked=false;
}

bool Controller::UpdateHandle::GetConflicts(set <UnityGUID>* conflicts) {
	if(!Init()) DOWNLOAD_FAIL;

	conflicts->clear();
	conflicts->insert(m_Conflicts->begin(), m_Conflicts->end()); 
	DOWNLOAD_OK;
}

bool Controller::UpdateHandle::SetResolutions(const map<UnityGUID, AssetServer::DownloadResolution>& resolutions) {
	if(!Init()) DOWNLOAD_FAIL;

	m_Resolutions->insert(resolutions.begin(),resolutions.end());
	DOWNLOAD_OK;
}

void Controller::UpdateHandle::InitConflicts() {
	Configuration& conf = Configuration::Get();
	m_Conflicts->clear();
	if( m_DeleteLocal ) 
		return;
		
	for(set<UnityGUID>::iterator i = m_Candidates->begin(); i != m_Candidates->end(); i++) {
		if (ShouldIgnoreAsset(*i))
			continue;
		Item working = conf.GetWorkingItem(*i);
		switch( working.GetStatus() ) {
			case kClientOnly:
			case kRestoredFromTrash:
			case kNewLocalVersion:
				if(conf.GetPathNameConflict(*i) == UnityGUID() ) continue;
				// else fall through and add to conflict list:
			case kConflict:
				m_Conflicts->insert(*i);
				break;
			default:
				break;
		}
	}

	AssetServerCache& cache = AssetServerCache::Get();
	
	// look for deletion conflicts
	for(map<UnityGUID, Item>::const_iterator i = m_Changes->begin(); i != m_Changes->end(); i++)
	{
		Item item = (*i).second;
		if (cache.IsItemDeleted((*i).first) && conf.HasDeletionConflict((*i).first))
			m_Conflicts->insert((*i).first);
	}
	
	return;
}


bool Controller::CheckItemNameValidToDownload (const AssetServer::Item& item)
{
	// Don't check if upload of assets is invalid, instead give warnings on import and disallow uploads. 
	//if( CheckValidFileNameDetail(item.name) == kFileNameInvalid )
	//{
	//	RETURN_LOG_ERROR(Format("Can't download file '%s' because it has invalid file name", item.name.c_str()));
	//}

	return true;
}

inline pair<Item,int> makeItemToIndex(const Item& it, vector<DownloadItem>& c) 
{
	return make_pair(it, c.size()-1);
}

bool Controller::UpdateHandle::Download(){
	if(!Init()) DOWNLOAD_FAIL;
	Configuration& conf = Configuration::Get();

	// Check if all conflicts have been resolved
	for ( set<UnityGUID>::iterator i = m_Conflicts->begin(); i != m_Conflicts->end(); i++) {
		Item working = conf.GetWorkingItem(*i);
		Status status = working.GetStatus();
		bool hasPathNameConflict = conf.GetPathNameConflict(*i) != UnityGUID();
		if( status == kConflict ) {
			map<UnityGUID, AssetServer::DownloadResolution>::iterator found = m_Resolutions->find(*i);

			if(  found == m_Resolutions->end() || found->second == kDLUnresolved ) 
				DOWNLOAD_LOG_ERROR( "Must resolve all conflicts before download." );
		}
		if ( hasPathNameConflict && m_Controller->GetNameConflictResolution(*i) == kNMUnresolved) {
			DOWNLOAD_LOG_ERROR( "Must resolve all conflicts before download." );
		}
		AssertIf( status != kConflict && !hasPathNameConflict);
	}
	
	///// The Big Loop: for each item inside candidate set, perform designated resolution

	set<Item> files;
	set<Item> dirs;

	for (map<UnityGUID,Item>::iterator i = m_Changes->begin(); i != m_Changes->end(); i++) {
		Item item = i->second;
		if (item.type == kTDir)
			dirs.insert(item);
		else
			files.insert(item);
	}

	// Resolve all dirs
	for (set<Item>::iterator i = dirs.begin(); i != dirs.end(); i++)
	{
		string error;
		map<UnityGUID, AssetServer::DownloadResolution>::iterator found;
		Item item = *i;
		Status itemStatus = item.GetStatus();
		
		// Force download if ordered to deleteLocal
		if (m_DeleteLocal) {
			if (itemStatus == kConflict || itemStatus == kNewLocalVersion || itemStatus == kRestoredFromTrash ) {
				if( item.origin != kOrServer)
					item = conf.GetServerItem(item.guid,-1);
				printf_console ("Forcing download of %s (guid: %s, version %d, server digest %s, status %s)\n", item.name.c_str(), GUIDToString(item.guid).c_str(), item.changeset, MdFourToString(item.digest).c_str(), GetStringForStatus(itemStatus));
			
				itemStatus = kNewVersionAvailable;
			}
			else if (itemStatus == kClientOnly) {
				printf_console ("Forcing deletion of asset %s (guid: %s, version %d, server digest %s, status %s)\n", item.name.c_str(), GUIDToString(item.guid).c_str(), item.changeset, MdFourToString(item.digest).c_str(), GetStringForStatus(itemStatus));
				item.parent = kTrashGUID;
				itemStatus = kNewVersionAvailable;

			}
		}

		switch (itemStatus) {

			// Client only: keep as it is, so do nothing 
			case kClientOnly:
				break;

			// Server only: get what it is on the server
			case kServerOnly:
				if( m_Controller->CheckItemNameValidToDownload(item) && (m_UsedForRevert || !item.IsDeleted()))
				{
					m_DirsToCreate[item.guid] = item;
					m_AssetVersions[item.guid] = item;
				}
				break;

			// Unchanged: keep as it is, so do nothing
			case kUnchanged:
				break;

			// Both sides changed, hairy resolution
			case kConflict:
				// ... fetch the resolution for this item
				found = m_Resolutions->find(item.guid);
				if (found != m_Resolutions->end()) {

					// Auto-merge resolution was selected... attempt that
					if ( found->second == kDLMerge) {
						DOWNLOAD_LOG_ERROR( "Merging unsupported for moved directories" );
					}
					// Keep-local resolution was selected, keep as it is
					else if (found->second == kDLSkipAsset) {
						// Do nothing...
					}
					// Keep-local resolution was selected, keep as it is
					else if (found->second == kDLTrashServerChanges) {
						m_AssetVersions[item.guid] = item;
					}
					// Delete-local resolution was selected, fetch server assetversion
					else if (found->second == kDLTrashMyChanges) {
						m_AssetVersions[item.guid] = item;
						/* The following will always result in true, as this is a dir asset
						Status pStatus=item.GetParentStatus();
						Status nStatus=item.GetNameStatus();
						if( pStatus != kUnchanged || nStatus != kUnchanged )  */
						
						m_AssetsToMove[item.guid] = make_pair(item.parent,item.name);
					}
					else {
						DOWNLOAD_LOG_ERROR( "Assert! Unknown resolution choice for an item" );
					}
				}
				else {
					DOWNLOAD_LOG_ERROR( "Assert! No resolution for an item, even when everything was believed to be resolved" );
				}
				break;

			// Client (only) changed but matches server, keep as it is
			case kSame:
				// Do nothing - simply update local version
				m_AssetVersions[item.guid] = item;
				break;

			// New assetversion available, fetch server's assetversion
			case kNewVersionAvailable:
				if( m_Controller->CheckItemNameValidToDownload(item) )
				{
					m_AssetVersions[item.guid] = item;				
					m_AssetsToMove[item.guid] = make_pair(item.parent,item.name);
				}
				break;

			// Changed local assetversion, keep as it is
			case kNewLocalVersion:
				// do nothing
				break;

			case kRestoredFromTrash:
				// this is due to 2.1/2.5b bug that was allowing to delete parent folder without deleting its child assets
				DOWNLOAD_LOG_ERROR( "Trying to update assets in folder that was restored from trash but not committed. Please commit " + item.name + " first."  );
				break;
			default:
				// error case!
				DOWNLOAD_LOG_ERROR( "Assert! Hit impossible case when trying to download directories" );
				break;
		}
	}

	// Resolve all files
	for (set<Item>::iterator i = files.begin(); i != files.end(); i++)
	{
		string error;
		Item item = *i;

		Status itemStatus = item.GetStatus();
		Status digestStatus = item.GetDigestStatus();
		bool itemIsMoved = item.IsMoved();

		// Force download if ordered to deleteLocal
		if (m_DeleteLocal) {
			if (itemStatus == kConflict || itemStatus == kNewLocalVersion || itemStatus == kRestoredFromTrash ) {
				if( item.origin != kOrServer)
					item = conf.GetServerItem(item.guid,-1);
				printf_console ("Forcing download of %s (guid: %s, version %d, server digest %s, status %s)\n", item.name.c_str(), GUIDToString(item.guid).c_str(), item.changeset, MdFourToString(item.digest).c_str(), GetStringForStatus(itemStatus));
			
				itemStatus = kNewVersionAvailable;
				digestStatus = (digestStatus == kConflict ||  digestStatus == kNewLocalVersion)?kNewVersionAvailable:kUnchanged;
			}
			else if (itemStatus == kClientOnly) {
				printf_console ("Forcing deletion of asset %s (guid: %s, version %d, server digest %s, status %s)\n", item.name.c_str(), GUIDToString(item.guid).c_str(), item.changeset, MdFourToString(item.digest).c_str(), GetStringForStatus(itemStatus));
				item.parent = kTrashGUID;
				itemIsMoved = true;
				itemStatus = kNewVersionAvailable;

			}
		}
		
		switch (itemStatus)
		{
			case kClientOnly:
				// do nothing
				break;
			case kServerOnly:
				if( m_Controller->CheckItemNameValidToDownload(item) && (m_UsedForRevert || !item.IsDeleted()))
				{
					DownloadItem tmp;
					tmp.guid = item.guid;
					tmp.changeset = item.changeset;
					m_Downloads.push_back(tmp);
					m_FileContents[item.guid]=makeItemToIndex(item, m_Downloads);
					m_AssetVersions[item.guid] = item;
				}
				break;
			case kIgnored:
			case kUnchanged:
				// do nothing
				break;
			case kConflict:
			{
				map<UnityGUID, AssetServer::DownloadResolution>::iterator found = m_Resolutions->find(item.guid);
				if (found != m_Resolutions->end()) {
					if (found->second == kDLMerge) {
						// Schedule ancestor and server version for download.
						Item working = conf.GetWorkingItem(item.guid);
						if(! working ) 
							DOWNLOAD_LOG_ERROR("Internal error. This should never happen: Got conflict status without a working copy of asset");
							
						DownloadItem ancestor;
						ancestor.guid=working.guid;
						ancestor.changeset=working.changeset;
						m_Downloads.push_back(ancestor);
						
						m_Merges[working.guid]=makeItemToIndex(working, m_Downloads);
						
						DownloadItem latest;
						latest.guid = item.guid;
						latest.changeset = item.changeset;
						m_Downloads.push_back(latest);
						
						m_FileContents[item.guid]=makeItemToIndex(item, m_Downloads);
						
						m_AssetVersions[item.guid] = item; 
						if (itemIsMoved)
							m_AssetsToMove[item.guid] =  make_pair(item.parent,item.name);
					}
					else if (found->second == kDLSkipAsset) {
						// do nothing
					}
					else if (found->second == kDLTrashServerChanges) {
						if (item.IsDeleted())
						{
							// make changeset match the newest one. this way deletion conflict is "solved" and user can commit away the deletion
							AssetServerCache::Get().UpdateDeletedItem(item.guid);
						}else
							m_AssetVersions[item.guid] = item; 
					}
					else if (found->second == kDLTrashMyChanges) {
						if( item.origin != kOrServer || item.IsDeleted())
							item = conf.GetServerItem(item.guid,-1);
						// Don't bother downloading file contents if the file should be deleted anyway.
						if(item.parent != kTrashGUID) {
							DownloadItem tmp;
							tmp.guid = item.guid;
							tmp.changeset = item.changeset;
							m_Downloads.push_back(tmp);
							
							m_FileContents[item.guid]=makeItemToIndex(item, m_Downloads);
						}
				
						m_AssetVersions[item.guid] = item; 
						if (itemIsMoved)
							m_AssetsToMove[item.guid] =  make_pair(item.parent,item.name);
					}
					else {
						LogDownloadError("Bad conflict resolution");
					}
				}
			}
				break;
			case kSame:
				m_AssetVersions[item.guid] = item;
				break;
			case kNewVersionAvailable:
			{
				if( m_Controller->CheckItemNameValidToDownload(item) )
				{
					// Don't bother downloading file contents if the file should be deleted anyway or just moved without updating the contents.
					if(item.parent != kTrashGUID && digestStatus != kUnchanged )
					{
						DownloadItem tmp;
						tmp.guid = item.guid;
						tmp.changeset = item.changeset;
						m_Downloads.push_back(tmp);
						
						m_FileContents[item.guid]=makeItemToIndex(item, m_Downloads);
					}
					m_AssetVersions[item.guid] = item;
					if ( itemIsMoved )
						m_AssetsToMove[item.guid] = make_pair(item.parent,item.name);
				}
			}
			break;
			case kRestoredFromTrash:
			case kNewLocalVersion:
				// do nothing
				break;
			default:
				// error case!
				LogDownloadError("Got invalid asset status from server");
				break;
		}
	}
	
	m_Thread.Run(DownloadThreadEntry,this);
	DOWNLOAD_OK;
}


void* Controller::UpdateHandle::DownloadThreadEntry(void* instance) {
	Controller::UpdateHandle* handle = (Controller::UpdateHandle*)instance;
	handle->m_ThreadResult=handle->DownloadThreadMain();
	return NULL;
}


bool Controller::UpdateHandle::DownloadThreadMain() {
	if(!Init()) {
		UpdateProgress(-1,-1,"Failed...");
		return false;
	}
	
	/* Do actual downloads. */
	int downloadSize = m_Downloads.size();
	if( downloadSize > 0 )
	{
		//float downloadStartTime = GetTimeSinceStartup();

		printf_console ("Downloading %d assets from server:\n", downloadSize) ;
	
		if(! m_Client->DownloadAssets( m_Downloads, kTempPath, this) ) {
			UpdateProgress(-1,-1,"Failed...");
			return false;
		}

		//float seconds = GetTimeSinceStartup()-downloadStartTime;
		//printf_console ("Downloaded %d assets in %02d:%02d.\n", downloadSize, (int)(seconds/60),(int)(seconds)%60 );

	}
	
	UpdateProgress(-1,-1,"Finishing up...");
	return true;
}

bool Controller::UpdateHandle::TryRestoreDeletedParent(Item item)
{
	AssetServerCache::DeletedItem di;
	vector<AssetServerCache::DeletedItem> parents;

	if (!AssetServerCache::Get().GetDeletedItem(item.parent, di))
		return true;

	while (true)
	{
		parents.push_back(di);
		if (!AssetServerCache::Get().GetDeletedItem(di.parent, di))
			break;
	}
	
	for (vector<AssetServerCache::DeletedItem>::const_reverse_iterator i = parents.rbegin(), e = parents.rend (); i != e; i++)
	{
		if (!m_Controller->RevertVersion(i->guid, i->changeset, string(), i->parent, true))
			return false;
	}

	return true;
}

bool Controller::UpdateHandle::Complete() {
	if(!Init()) DOWNLOAD_FAIL;
	m_Thread.WaitForExit(false);
	if(!m_ThreadResult) DOWNLOAD_FAIL;
	// Insert new dir assets (in slightly convoluted manner, since they have to be properly bound to each other)
	{

		set<UnityGUID> dirsCreated;
	
		// Strange loop: m_DirsToCreate dirsLeft, removing those we process. When we come full circle, 
		// check if nothing was removed which is an error case. If not, just keep circling till we're done.
		map<UnityGUID,Item>::iterator i = m_DirsToCreate.begin();
		int processed = 0;
		while( !m_DirsToCreate.empty() )
		{
			{
				// Create asset. We need to:
				Item info = i->second;
				
				// ... and only actually do it if the parent already exists
				// 		(otherwise loop on, hoping it'll get created later)
				
				string parentPath = m_Controller->GetAssetPathName(info.parent);
				if (dirsCreated.count(info.parent) || m_Controller->DoesAssetExist(info.parent) && parentPath != "" ) 
				{

					
					if(! ResolveExistingAssetAtDestination(info.guid, info.parent, info.name))
						DOWNLOAD_FAIL;

					// ... it's a boy, eh, dir
					printf_console("Creating downloaded directory %s as %s\n", GUIDToString(info.guid).c_str(), info.name.c_str());
					
					
					if (! m_Controller->CreateAsset (info) )
						DOWNLOAD_LOG_ERROR( Format("Unable to create downloaded directory %s assetversion %i: %s", m_Controller->AssetInfoToPathName (info).c_str() , info.changeset,  m_Controller->GetLastError().c_str() ));

					dirsCreated.insert(info.guid);
					
					// import now because later if files are moved into these dirs in same update metadata of dirs will be needed
					string path = AppendPathName(GetAssetPathFromGUID(info.parent), info.name);
					AssetInterface::Get().ImportAtPathImmediate ( path );

					// Remove processed assets guid from the m_DirsToCreate set, without fucking up the i-iterator.
					map<UnityGUID, Item>::iterator toErase = i;
					// Step to next
					i++;
					m_DirsToCreate.erase(toErase);
					// Track and count processed assets (to detect if a loop caught nothing)
					processed++;
				} 
				else
				{
					// Early out if we've got an unbound dir structure from the server.
					if ( m_DirsToCreate.find(info.parent) == m_DirsToCreate.end() )
					{
						// Should we attempt to fix the error instead of aborting? (By for instance creating the missing parent directory inside the asset root, or in a dir called Restored Assets or similar?)
						DOWNLOAD_LOG_ERROR( Format("Unbound directory structure received from server. Asset named %s has non-existing parent GUID %s.", info.name.c_str(), GUIDToString(info.parent).c_str()));
					}
					// Step to next
					i++;
				}
			}

			// When at the end, circle to beginning again
			if (i == m_DirsToCreate.end())
			{
				// ... unless if nothing was done
				if (processed == 0 && !m_DirsToCreate.empty()  ) 
				{
					
					// ... in which case to fail
					DOWNLOAD_LOG_ERROR( "Unbound directory structure(s) received from server.");
				}
				else
				{
					// .. else have another go
					i = m_DirsToCreate.begin();
					processed = 0;
				}			
			}
		}
	}

	
	// Merge conflicting assets before moving them to place
	for (map<UnityGUID,ItemToIndex>::iterator i = m_Merges.begin(); i != m_Merges.end(); i++)
	{
		map<UnityGUID,ItemToIndex>::iterator found = m_FileContents.find(i->first);
		if( found == m_FileContents.end() )
			DOWNLOAD_LOG_ERROR("Internal error - this should not happen: could not locate latest version of asset for merging");
		
		Item working = i->second.first;
		Item server = found->second.first;
		
		DownloadItem ancestor = m_Downloads[i->second.second];
		DownloadItem latest = m_Downloads[found->second.second];

		// remote, local, ancestor, target
						
		if (! m_Controller->InvokeMergeApp(
							Format("Latest version of %s on server", working.name.c_str()),
							latest.streams[kAssetStream],  // remote
							Format("Local copy of %s", working.name.c_str()),
							m_Controller->GetAssetPathName(working.guid), // local
							"Merged result",
							ancestor.streams[kAssetStream],  // ancestor
							latest.streams[kAssetStream] // target - same as latest version, as that one will be used when copying assets from temp
							)) 
			DOWNLOAD_LOG_ERROR(  m_Controller->GetLastError()  );
		
	}

	// Move all moved assets (that we want to move), and erase those that have been moved to trash
	{
		map<string, UnityGUID> alphaAssetsToMove;
		for (map<UnityGUID, ParentToName>::iterator i = m_AssetsToMove.begin(); i != m_AssetsToMove.end(); i++)
			alphaAssetsToMove[m_Controller->GetAssetPathName(i->first)] = i->first;

		for (map<string, UnityGUID>::reverse_iterator i = alphaAssetsToMove.rbegin(); i != alphaAssetsToMove.rend(); i++)
		{
			UnityGUID asset = i->second;
			if(m_Controller->IsMarkedForRemoval(asset))
				m_Controller->SetMarkedForRemoval(asset,false);
			ParentToName dest = m_AssetsToMove[asset];
			if (m_Controller->DoesAssetExist(asset)) {
				if (dest.first == kTrashGUID ) {
					if (! m_DeleteLocal && ! m_Controller->AssetDirIsEmpty(asset) ) {
						LogDownloadError( Format("Could not remove directory '%s' because it contains uncomitted changes.", m_Controller->GetAssetPathName(asset).c_str() ) );
						m_AssetVersions.erase(asset); // Make sure that we do not mark the directory version as being up-to-date.
					}
					else {
						if (! m_Controller->RemoveAsset(asset))
							if (m_Controller->DoesAssetExist(asset))
								DOWNLOAD_LOG_ERROR( "Could not remove '" +  m_Controller->GetAssetPathName(asset) +"'" );
					}
					// Don't try to update file contents if the file has been deleted anyway.
					// This avoids getting a warning about not being able to update asset.
					m_FileContents.erase(asset);
					m_AssetsToMove.erase(asset);
					
				}
				else {
					if (! m_Controller->DoesAssetExist( dest.first ) ) 
					{
						// TODO: show error? this is an error case
						//MaintWarning (Format("Warning skipping updating asset %s as the new parent folder has not been downloaded from server.",i->first.c_str() ));
						m_AssetsToMove.erase(asset);
						m_FileContents.erase(asset);  // Ensure that we never download the file contents without moving the asset as well
						m_AssetVersions.erase(asset); // and also make sure that we do not mark the asset version as being up-to-date.
						continue;
					}

					if(! ResolveExistingAssetAtDestination(asset, dest.first, dest.second))
						DOWNLOAD_FAIL;

					if(! m_Controller->MoveAsset(asset, dest.first, dest.second))
						DOWNLOAD_LOG_ERROR( "Failed to move asset to "+dest.second+": " + m_Controller->GetLastError());
					
					m_AssetsToMove.erase(asset);
				}
			}
		}
	}

	// Create/replace file assets (straightforward, since dirctory structure is correct now.)
	for (map<UnityGUID,ItemToIndex>::iterator i = m_FileContents.begin(); i != m_FileContents.end(); i++)
	{
		// Prepare asset
		Item info = i->second.first;
		int index = i->second.second;
		if( index >= m_Downloads.size() || index < 0 )
			DOWNLOAD_LOG_ERROR("Internal error: Can't create file asset. Download stream index out of bounds.");
		DownloadItem downloaded = m_Downloads[index];
		
		if( downloaded.streams.find(kAssetStream) == downloaded.streams.end() ) {
			string streams;
			for (StreamMap::iterator i = downloaded.streams.begin(); i != downloaded.streams.end(); i++)
				streams += Format(" %s (%s)", i->first.c_str(), i->second.c_str());
			DOWNLOAD_LOG_ERROR("Internal error: Can't create file asset. Did not receive asset stream from server. stream hash contains only: "+streams);
		}
			
		int keepDigestOption = m_Merges.count(info.guid)?kCO_None:kCO_UseServerDigest;

		// Create new or replace old
		if (! m_Controller->DoesAssetExist(info.guid)) {			
			if(!ResolveExistingAssetAtDestination(info.guid, info.parent, info.name))
				DOWNLOAD_FAIL;
			printf_console("Creating downloaded file %s as %s\n", GUIDToString(info.guid).c_str(), info.name.c_str());

			if (! m_Controller->CreateAsset(info, downloaded.streams, kCO_Create | kCO_Exclusive | keepDigestOption))
			{
				if (!TryRestoreDeletedParent(info) || !m_Controller->CreateAsset(info, downloaded.streams, kCO_Create | kCO_Exclusive | keepDigestOption))
					DOWNLOAD_LOG_ERROR( "Unable to create new file asset (" + info.name + ") received from server: " + m_Controller->GetLastError() );
			}
		}
		else {
			info.parent = m_Controller->GetAssetParent(info.guid);
			info.name = m_Controller->GetAssetName(info.guid);

			printf_console("Replacing downloaded file %s (guid %s)\n", info.name.c_str(), GUIDToString(info.guid).c_str());
			if (! m_Controller->CreateAsset(info, downloaded.streams, kCO_Move | keepDigestOption ))
				DOWNLOAD_LOG_ERROR( "Unable to replace file asset version recieved from server: " + m_Controller->GetLastError() );
		}
	}

	AssetServerCache& cache = AssetServerCache::Get();
	// Set versions of updated but unchanged assets
	for (map<UnityGUID, Item>::iterator i = m_AssetVersions.begin(); i != m_AssetVersions.end(); i++) {		
		m_Controller->Received(i->first);

		//if (! m_Controller->DoesAssetExist(i->first))
		//	continue;

		AssetServerCache::CachedAssetMetaData* meta = cache.FindCachedMetaData(i->first);
		if(meta) {
			meta->originalChangeset = i->second.changeset;
			meta->originalName = i->second.name;
			meta->originalParent = i->second.parent;
			meta->originalDigest = i->second.digest;
			m_Controller->m_NeedsMetaDataUpdate.insert(i->first);
			
			cache.SetCachedDigest(i->first, i->second.digest);
		}
	}

	DOWNLOAD_COMPLETE;
	
}

// Checks wether an asset name is already located inside dest and changes the name. If it is not a candidate for a move inside the
// current download, a warning is printed, notifying the user of the new file name.
bool Controller::UpdateHandle::ResolveExistingAssetAtDestination( const UnityGUID& guid, const UnityGUID& parent, UnityStr& name  ) {
		Configuration& conf = Configuration::Get();
		UnityGUID conflicting;

		// Download asset under a different name if nameConflict resolution is set to do so
		if( conf.GetPathNameConflict(guid) != UnityGUID() && m_Controller->GetNameConflictResolution(guid) == kNMRenameRemote ) {
			
			set<string> forbidden ;
			conf.GetOtherAssetNamesInDirectory(guid, parent, &forbidden);
			
			string uniquePath=GetGUIDPersistentManager ().GenerateUniqueAssetPathName(parent, name, &forbidden );
			string renamed = GetLastPathNameComponent(uniquePath);
			name=renamed;
		}

		if( m_Controller->LocateChildAsset(parent, name, &conflicting) ) {

			map<UnityGUID, Item>::iterator found=m_Changes->find(conflicting);
		    bool willBeMoved = false;
			if( found != m_Changes->end()) {
				Status s = found->second.GetParentStatus();
				if ( s == kConflict || s == kNewVersionAvailable || (m_DeleteLocal && s == kNewLocalVersion) )
					willBeMoved=true;
				else {
					s = found->second.GetNameStatus();
					if ( s == kConflict || s == kNewVersionAvailable || (m_DeleteLocal && s == kNewLocalVersion) )
						willBeMoved=true;
				}
			}
			
			string renamed = willBeMoved?("TMP_"+GUIDToString(guid)+"_"+name.c_str()):(std::string)name;
			
			set<string> forbidden ;
			conf.GetOtherAssetNamesInDirectory(guid, parent, &forbidden);
			string uniquePath=GetGUIDPersistentManager ().GenerateUniqueAssetPathName(parent, renamed, &forbidden );
			renamed = GetLastPathNameComponent(uniquePath);
			
			if( ! m_Controller->MoveAsset(conflicting, parent, renamed) ) 
				DOWNLOAD_ERROR( Format("Unable to rename conflicting guid %s: %s",m_Controller->GetAssetPathName(conflicting).c_str() , m_Controller->GetLastError().c_str() ));

			// If conflicting guid is not being moved in the same download operaion, issue a warning
			if( ! willBeMoved && m_Controller->GetNameConflictResolution(conflicting) != kNMRenameLocal) {
				LogDownloadWarning (Format("Warning local asset ''%s/%s'' has same path name as an asset being downloaded from the server. The local asset has been renamed to %s\n", 
m_Controller->GetAssetPathName(parent).c_str(), name.c_str(), m_Controller->GetAssetName(conflicting).c_str()));
			}
		}
		DOWNLOAD_OK;
}


bool Controller::CommitBegin(const string& changesetDescription, const set<UnityGUID>& candidates) {
	if(!m_Online) RETURN_ERROR("Client is off-line. Please go online before accessing the asset server");
	if(m_Commit)
		delete m_Commit;
	
	m_Commit=new CommitHandle(this, changesetDescription, candidates);
	
	bool success =  m_Commit->Init();
	if(! success ) {
		delete m_Commit;
		m_Commit=NULL;
	}
	return success;
}
bool Controller::CommitStartUpload() {
	if(! m_Commit)
		RETURN_LOG_ERROR("You have to call CommitBegin before calling CommitStartUpload");
	
	return m_Commit->Upload();
}

float Controller::CommitGetUploadProgress(string& text) {
	if(! m_Commit)
		RETURN_LOG_ERROR("You have to call CommitBegin before calling CommitGetUploadProgress");
	
	return m_Commit->GetProgress(text);
}


bool Controller::CommitComplete() {
	if(! m_Commit)
		RETURN_LOG_ERROR("You have to call CommitStartUpload before calling CommitComplete");

	bool success = m_Commit->Complete();
	
	delete m_Commit;
	m_Commit=NULL;

	return success;
}


bool Controller::CommitAbort() 
{
	if( m_Commit ) 
	{
		delete m_Commit;
		m_Commit=NULL;
	}
	
	RETURN_OK;
}

Controller::CommitHandle::CommitHandle(Controller* controller, const string& changesetDescription, const set<UnityGUID>& candidates)
	: m_Controller(controller)
	, m_Candidates(new set<UnityGUID>())
	, m_ChangesetDescription(changesetDescription)
	, m_Locked(false)
	, m_NewChangeset(-1)
	, m_Items(new vector<UploadItem>())
	, m_AlphaAssetsToTrash(new map< string, UnityGUID >())
	, m_Client(new Backend(controller->GetConnectionSettings(), controller->GetMaximumTimout(), controller->GetLogId() ))
{
	m_Candidates->insert(candidates.begin(), candidates.end());

}

Controller::CommitHandle::~CommitHandle(){
	Cleanup();
	delete m_Client;
	delete m_Candidates;
	delete m_Items;
	delete m_AlphaAssetsToTrash;
}

bool Controller::CommitHandle::Init() {
	if(m_Locked)
		DOWNLOAD_OK;

	if (m_Candidates->size() == 0)
		DOWNLOAD_LOG_ERROR( "Empty selection" );

	if (GetApplication().IsSceneDirty())
	{
		// check if trying to commit currently open dirty scene
		string scenePath = GetApplication().GetCurrentScene();
		UnityGUID sceneGUID;

		if (GetGUIDPersistentManager().PathNameToGUID(scenePath, &sceneGUID))
		{
			if (m_Candidates->find(sceneGUID) != m_Candidates->end())
			{
				int result = DisplayDialogComplex("Committing unsaved open scene", Format("Scene '%s' that is being committed has unsaved changes.\nDo you want scene to be saved before committing?", scenePath.c_str()), "Save", "Don't Save", "Cancel");

				switch (result)
				{
				case 0:
					if (!GetApplication().SaveCurrentSceneDontAsk())
						return false;
					else
						break;
				case 1:
					break;
				case 2:
					return false;
				}
			}
		}
	}

	Configuration& conf = Configuration::Get();

	if(!m_Client->UpdateConfiguration(conf)) RETURN_FAIL;
	
	m_Controller->LockAssets();
	m_Locked=true;
	
	startTime=GetTimeSinceStartup();
	progress=-1;
	progressText="Starting commit...";

	DOWNLOAD_OK;
}

void Controller::CommitHandle::Cleanup() 
{
	if (! m_Locked) return;
	if (m_Thread.IsRunning()) {
		m_Thread.WaitForExit();
	}
		
	m_Controller->UnlockAssets();
	m_Locked=false;
}

bool Controller::CommitHandle::Upload(){
	if(!Init()) DOWNLOAD_FAIL;
	Configuration& conf = Configuration::Get();
	
	for(set<UnityGUID>::const_iterator i=m_Candidates->begin(); i!=m_Candidates->end(); i++) 
	{
		Item working = conf.GetWorkingItem(*i);
		Item ancestor = conf.GetServerItem(*i,working.changeset);
		UploadItem item;
		item.guid=*i;
		item.name=working.name;
		item.parent=working.parent;
		item.digest=working.digest;
		item.type=working.type;
		item.path=m_Controller->AssetInfoToPathName(working);
		item.predecessor=ancestor.changeset;
		item.reuseStreams=false;

		if( CheckValidFileNameDetail(item.name) == kFileNameInvalid)
		{
			string s = Format("Won't upload file '%s' because it has invalid file name. Commit aborted.", item.name.c_str());
			Controller::Get().SetError(s);
			DebugStringToFile (s, 0, __FILE__, __LINE__, kLog | kAssetImportError, NULL, Controller::Get().GetLogId() );
			return false;
		}

		if( working.parent == kTrashGUID ) {
			(*m_AlphaAssetsToTrash)[item.path]=*i;
			item.name = Format("%s (DEL_%s)", item.name.c_str(), GUIDToString(item.guid).c_str());
		}

		switch ( working.GetStatus() ){
			// Same as unchanged
			case kIgnored:
			// Unchanged:
			case kUnchanged:
			// Client (only) changed but matches server
			case kSame:
				continue;
			// New assetversion available,
			case kNewVersionAvailable:
			// Both sides changed, must resolve by doing download first
			case kConflict:
				DOWNLOAD_LOG_ERROR("Commit failed because some of the files being committed are not up to date. Please update all assets first.");
			// Client only: create and upload it as-is
			case kClientOnly:
			// Changed local assetversion, upload changes
			case kRestoredFromTrash:
			case kNewLocalVersion:
				if (working.GetDigestStatus() == kUnchanged)
					item.reuseStreams=true ; // only file name or location has changed, so simply reuse the file contents from the previous version.
				else
					item.newStreams=m_Controller->GetAssetStreams(item.guid);
				break;
			// Server only:
			case kServerOnly:
				if (working.IsDeleted()) // file that is committed to server but deleted locally
					item.reuseStreams=true ; // only file name or location has changed, so simply reuse the file contents from the previous version.
				else
					continue; // impossible?
				break;
			default:
				// error case! (although we just skip the asset)
				//LogDownloadError("Got invalid asset status from server");
				continue;
		
		} 
		
		m_Items->push_back(item);
	}

	
	m_Thread.Run(UploadThreadEntry,this);
	DOWNLOAD_OK;
}


void* Controller::CommitHandle::UploadThreadEntry(void* instance) {
	Controller::CommitHandle* handle = (Controller::CommitHandle*)instance;
	handle->m_ThreadResult=handle->UploadThreadMain();
	return NULL;
}

bool Controller::CommitHandle::UploadThreadMain() {
	if(!Init()){
		UpdateProgress(-1,-1,"Failed...");
		return false;
	}
	
	// m_NewChangeset will contain the new changeset after CommitChangeset returns
	if( ! m_Client->CommitChangeset(*m_Items, m_ChangesetDescription, &m_NewChangeset, this ) ){
		UpdateProgress(-1,-1,"Failed...");
		return false;
	}

	UpdateProgress(-1,-1,"Finishing up...");

	return true;
}

void Controller::CommitHandle::ClearDeleted()
{
	// FIXME: <-- if it crashes here - user sees his "deleted and committed" assets in commit window again... 
	// and can commit again, wont hurt, but still sucks
	AssetServerCache& cache = AssetServerCache::Get();
	for (vector<UploadItem>::const_iterator i = m_Items->begin(); i != m_Items->end(); i++)
		if (i->parent == kTrashGUID)
			cache.RemoveDeletedItem(i->guid);
}

bool Controller::CommitHandle::Complete() 
{
	if(!Init()) DOWNLOAD_FAIL;

	m_Thread.WaitForExit(false);
	if(!m_ThreadResult) DOWNLOAD_FAIL;

	AssetServerCache& cache = AssetServerCache::Get();
	// Update the version info stored for the updated assets.
	for(vector<UploadItem>::iterator i = m_Items->begin(); i != m_Items->end(); i++) 
	{
		AssetServerCache::CachedAssetMetaData* meta = cache.FindCachedMetaData(i->guid);
		if(meta) {
			meta->originalChangeset = m_NewChangeset;
			meta->originalName = i->name;
			meta->originalParent = i->parent;
			meta->originalDigest = i->digest;
			m_Controller->m_NeedsMetaDataUpdate.insert(i->guid);
			
			//cache.SetDirty ();

		}
	}
		
	return m_Client->UpdateConfiguration(Configuration::Get());
}

static string FormatETA(double s) {
	if(s < 0)
		return "";
	if (s < 5)
		return "Any time now...";
	if (s < 15)
		return "Few seconds to go";
	if (s < 60)
		return "Less than a minute left";
	if (s < 90)
		return "1 minute left";
	if (s < 3600)
		return Format("%.0f minutes left", s / 60.0);
	if (s < 5400)
		return "1 hour left";
	return Format("Done in %.0f hours", s / 3600.0);
		
}

void Controller::UpdateHandle::UpdateProgress(SInt64 bytes, SInt64 total, const std::string& currentFileName ){
	m_ProgressMutex.Lock();
	if(total < 0 && bytes < 0)
	{
		progress = 1.0;
		progressText = "Finishing up...";
	}
	else if( total == 0 && bytes == 0 )
	{
		progress = 0.0f;
		progressText = currentFileName;
	}
	else
	{
		progress = total>0?(float)bytes/(float)total:bytes>0?-2:-1;
		double current = GetTimeSinceStartup() ;
		if(startTime == 0)
			startTime = current;
		double elapsed = current - startTime;
		double left = (progress > 0 && elapsed > 0)?(elapsed / progress)-elapsed:-1;
		progressText =  "Downloading " + FormatBytes(bytes) + " of " + (total>0 ? FormatBytes(total) + ". " : total == 0 ? "(calculating total size...) " : "(unknown total size) " ) + (elapsed > 2?FormatETA(left):string());
	}
	m_ProgressMutex.Unlock();

}
void Controller::CommitHandle::UpdateProgress(SInt64 bytes, SInt64 total, const std::string& currentFileName ){
	m_ProgressMutex.Lock();

	if(total < 0 && bytes < 0) {
		progress = 1.0;
		progressText = "Finishing up...";
	}
	else {
		progress = total>0?(float)bytes/(float)total:-1;
		double current = GetTimeSinceStartup() ;
		if(startTime == 0)
			startTime = current;
		double elapsed = current - startTime;
		double left = (progress > 0 && elapsed > 0)?(elapsed / progress)-elapsed:-1;
		
		progressText = currentFileName + " " + (elapsed > 2?FormatETA(left):string());
	}
	m_ProgressMutex.Unlock();

}

// @TODO: Better handling for asset server parameters
bool Controller::InitializeFromCommandLine(const vector<string> &options)
{
	printf_console ("Asset server action with params:\n");
	for( size_t i = 0; i < options.size(); ++i ) {
		printf_console( "  #%i: %s\n", (int)i, options[i].c_str() );
	}
	
	bool hasRevision = options.size() == 6 && options[4] == "r";
	// Expected format: IP[:port] projectName username password [r <revision>]
	if (options.size() >= 4 || hasRevision)
	{
		string hostInfo = options[0];
		string ip = hostInfo;
		string port = "10733";
		string::size_type seperator = hostInfo.find(":", 0);
		if (seperator != string::npos)
		{
			ip = hostInfo.substr(0,seperator);
			port = hostInfo.substr(seperator+1);
		}

		string userName = options[2];
		string password = options[3];
		string projectname = options[1];
		
		string dbname = AssetServer::Configuration::Get().GetDatabaseName(ip, userName, password, port, projectname);
		if (dbname.empty())
		{
			printf_console("Failed to get database name from server - %s\n", GetErrorString().c_str());
			return false;
		}
		
		string connectionString =
		"host='" + ip +
		"' user='" + userName +
		"' password='" + password + 
		"' dbname='" + dbname + 
		"' port='" + port + 
		"' sslmode=disable ";
		
		Initialize (options[2].c_str(), connectionString, 60);

		if (hasRevision)
		{
			int revision = StringToInt(options[5]);

			if (revision == 0)
			{
				printf_console("Please specify a valid revision number\n");
				return false;
			}

			Configuration::Get().SetStickyChangeset(revision);
		}
	}
	else
	{
		printf_console("\tError parsing asset server information. Should be of the form \"IP[:port] projectName username password [r <revision>]\"\n");
		return false;
	}
	return true;
}

bool Controller::AssetServerCommitCommandLine(const vector<string> &options)
{
	if (options.size() == 6)
	{
		string fileName = options[4];
		string commitMessage = options[5];
		
		UnityGUID guid;
		if (GetGUIDPersistentManager().PathNameToGUID (fileName, &guid))
		{
			set<UnityGUID> guids;
			guids.insert(guid);
			AssetServer::Controller::Get().DoCommitOnNextTick(commitMessage, guids);
			Controller::Tick();
		}
		else
		{
			printf_console("Failed to find asset GUID for file %s.\n", fileName.c_str());
			return false;
		}
	}
	return true;
}

bool Controller::AssetServerUpdateCommandLine(const vector<string> &options)
{
	set<UnityGUID> guids;
	guids = AssetDatabase::Get().GetAllRootGUIDs ();
	// @TODO: Investigate
	// Force add project settings. For some reason when using -createProject and then -asUpdate the guid for project settings is not part of the all root guids
	// why?
	guids.insert(StringToGUID("00000000000000004000000000000000"));

	if( !UpdateStatus() )
	{
		printf_console("Failed to update status for server connection - %s\n",GetErrorString().c_str());
		return false;
	}

	AssetServerCache::Get().InitializeCaches();

	if( !UpdateBegin( guids, true, false ) ) 
	{
		printf_console("Failed to begin the update from server - %s\n",GetErrorString().c_str());
		return false;
	}

	if (!UpdateStartDownload())
	{
		printf_console("Failed to start the update from server - %s\n",GetErrorString().c_str());
		return false;
	}

	if (!UpdateComplete())
	{
		printf_console("Failed to finish download from server - %s\n",GetErrorString().c_str());
		return false;
	}
	
	return true;
}



#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (Binary2TextTests)
{
	TEST (Binary2TextAvailable)
	{
		string path = GetBinaryConverterPath ();
		CHECK (IsFileCreated (path));
	}

	TEST (CanHandleInvalidFile)
	{
		CreateDirectory ("Temp");
		string error;
		CHECK(WriteStringToFile ("abc", "Temp/infile", kNotAtomic, kFileFlagDontIndex | kFileFlagTemporary));
		// input file invalid, binary2text should exit with error code
		CHECK(!InvokeBinaryConverter ("Temp/infile", "Temp/outfile", error));
	}
}

#endif // ENABLE_UNIT_TESTS

#undef LogError
#undef LogWarning