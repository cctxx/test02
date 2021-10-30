#include "UnityPrefix.h"
#include "CacheServer.h"
#include "Editor/Src/AssetPipeline/AssetHashing.h"
#include "CacheServerCommunicationNodeJS.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Serialize/TransferFunctions/YAMLWrite.h"
#include "Runtime/Serialize/TransferFunctions/YAMLRead.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/Utility/AssetPreviews.h"
#include "Editor/Src/AssetPipeline/ModelImporter.h"

#include "Editor/Src/AssetPipeline/AssetImportingTimerStats.h"

#if UNITY_WIN
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#endif


/*
@TODO:

Fix before trunk merge:
 @TODO: Disabled features that need to be re-implemented: 
 *** verify_permissions in EditorUtility.mm has to be re-enabled & optimized
     (Kill it and handle file read / write functions to popup authoriziation dialog)
 
 *** GUIDMapper file is no longer deleted, this can lead to out of sync situations. Build proper solution... (Merge assetdatabase file with guidmapper)
 
 *** It looks like duplicating an asset does not write the meta file immediately but during the import process?
 
 Windows TODO:
 -> Optimize progress bar display code (Dont repaint unless something has changed)

*  Actually reorder imports based on path or disk layout.
	-> This is useful both for cache server and normal imports...
 
* Test that both progress bar (Asset progressbar and normal progressbar don't leak when being called a lot of times!)

* Importing an fbx file and generating a material will actually load the texture!
 
 
Fix after trunk merge:
 
Assetpipeline and Data Validation TODO:
 * NativeFormat importer assets should not go through the cache server when they are dirty
 * version number support for all AssetImporters and automatically force reimport when it changes
 * version number support for all AssetPostprocessors and automatically force reimport when it changes
 * BumpmapSettings::PerformBumpmap how to handle this with cache server?
 * Import Asset should not use the cache server (Only automatic operations)
 * Have some functionality to validate if two machines generate the same output asset! (Also don't upload in that case...)
 *** On Startup detect if there are any files in metadata / cachedata that do not have the appropriate GUID based name and delete them. Right now don't detect ill formatted files in the metadata / cachedata directory.
 *** Make it so that Apply changes saves the meta file before importing (So that it can use the cache server right away)
 *** Prevent upload when in batchmode (Doesn't generate previews)

 
**** Performance:

 *** We currently scan all meta files twice. One time we read the header of each file. Then we hash them.
 *** Start downloading before hashing is completed. (Dont do any writing to disk though...)

 
Gazillion project:
 Full reimport with nothing cached: [8.9 hours. 534 minutes]
 ----- Total AssetImport time: 32090.812500s, Asset Import only: 30103.158203s, CacheServer Download: 1243.801758s [0 B, 0.000000 mb/s], CacheServer Hashing: 1243.732056s [8.71 GB, 7.170103 mb/s]

 Cached import with most cached?: [2.0 hours. 124 minutes]
 ----- Total AssetImport time: 7480.960449s, Asset Import only: 3821.361572s, CacheServer Download: 2806.795166s [0 B, 0.000000 mb/s], CacheServer Hashing: 2806.779297s [8.71 GB, 3.176627 mb/s]

 Registering custom dll's ... 5.390445 seconds.
 Validating Project structure ... 2.188537 seconds.
 Shader import version has changed; will reimport all shaders...
 Upgrading shader files ...23.570673 seconds.
  Initial Refresh 739.586853 seconds [Scanning all guid's from .meta files]
 
 Most objects in cached import mode take 10-30ms to integrate...

 ** Projected Performance on amazing society project folder (60k individual assets, 10GB of source assets, 4.33GB of )
 Asset integration: 60k * 1ms = 60 seconds
 Cache download: 4.3GB / 50mb/s = 86 seconds
 Hash speed: 10GB / 30mb/s = 333 seconds = 5.5 minutes

 -> Rebuilding GUID cache takes 877 seconds. Come up with a better solution for the GUIDCache, so that we never run in a situation where it has to be recomputed.
 -> Find a good way to turn off all AS server processing & storage when it is not used at all...
 -> When importing a fresh project we first import all assets, then we scan if we need a reimport! Thats really smart! (Joe)
 -> Make it so that Imported assets are unmodifyable after import and can never be changed. When unloading assets, ignore dirty flags on them.
 -> We are currently always writing the asset database after an import operation. (If something in the asset database changed...)
		When doing small operations on assets this is not desirable. For example when moving a file...
 
 ***** Write tests  :

 - Copy dll from ScriptAssemblies folder into empty project folder. All hell breaks loose. Make this not be totally broken... (Not related to cache server)
 - Write test that dll's are found and loaded before any asset importing.

 - Call DisplayProgressbar 100k times with different progress value. Seems that this leaks...

 Manual test:
 - Test that going from a project folder with meta files to commit to asset server and working with it works correctly.
	-> Digest Hashes & postprocess callbacks are disabled!
 
 * Test for moving file onto itself and test for changing case File.Move function

*  ProjectConsistencyChecks.cs are marked knownfailure
 *  CacheServer.cs some tests are marked as known failure
 *  UnityCrashWithoutDataloss.cs several tests are not yet written!!!
 
 * refactor TextureImporter::DoesAssetNeedReimport to do the comparison based on only data in the textureimporter class
 
*/ 

// THIS is only needed for the hack below...
#include "Editor/Src/AssetPipeline/AssetImporter.h"

bool ExtractExternalRefsFromSerializedFile (const std::string& path, dynamic_block_vector<FileIdentifier>& externals)
{
	ResourceImageGroup group;
	SerializedFile* file = UNITY_NEW(SerializedFile,kMemTempAlloc);
	if (!file->InitializeRead (path, group, 1024 * 4, 2, 0))
	{
		UNITY_DELETE(file,kMemTempAlloc);
		return false;
	}
	externals = file->GetExternalRefs();
	UNITY_DELETE(file,kMemTempAlloc);
	return true;
}

void ExtractExternalReferencesForValidation (const std::string& path, CacheServerAssetInfo& info)
{
	dynamic_block_vector<FileIdentifier> externals(1024,kMemTempAlloc);
	if (!ExtractExternalRefsFromSerializedFile(path, externals))
	{
		ErrorString("Failed to load serialized file for validation " + path);
	}
	
	for (int i=0;i<externals.size();i++)
	{
		if (externals[i].guid != UnityGUID ())
		{
			string assetPath = GetGUIDPersistentManager().AssetPathNameFromGUID(externals[i].guid);
			info.externalReferencesForValidation.push_back (make_pair(externals[i].guid, assetPath));
		}
	}
}

template<class TransferFunction>
void CacheServerAssetInfo::Transfer (TransferFunction& transfer)
{
	transfer.Transfer (mainRepresentation, "mainRepresentation");
	transfer.Transfer (representations, "representations");
	transfer.Transfer (labels, "labels");
	transfer.Transfer (assetImporterClassID, "assetImporterClassID");
	transfer.Transfer (externalReferencesForValidation, "externalReferencesForValidation");
}

static void CacheServerAssetInfoToAsset (const CacheServerAssetInfo& assetInfo, Asset& output)
{
	output.mainRepresentation = assetInfo.mainRepresentation;
	output.representations = assetInfo.representations;
	output.labels = assetInfo.labels;
	output.importerClassId = assetInfo.assetImporterClassID;
}

static void AssetToCacheServerInfo (const Asset& asset, SInt32 importerClassID, const std::string& metaDataPath, CacheServerAssetInfo& output)
{
	output.mainRepresentation = asset.mainRepresentation;
	output.representations = asset.representations;
	output.labels = asset.labels;
	output.assetImporterClassID = importerClassID;
	
	ExtractExternalReferencesForValidation (metaDataPath, output);
}

template<class T>
void WriteToYamlString (T& sourceData, std::string& output)
{
	YAMLWrite writer (kNeedsInstanceIDRemapping | kYamlGlobalPPtrReference);
	writer.Transfer(sourceData, "BaseName");
	writer.OutputToString(output);
}

template<class T>
void ReadFromYamlString (const std::string& yamlString, T& outputData)
{
	YAMLRead reader (yamlString.c_str(), yamlString.size(), kNeedsInstanceIDRemapping | kYamlGlobalPPtrReference);
	reader.Transfer(outputData, "BaseName");
}

const char* kErrorStrings[] = { "Success", "it was not in the cache", "of a connection error", "of invalid cached data", "of a database error" };

static std::string CacheServerDownloadError (const std::string& assetPath, CacheServerError err)
{
	return Format ("Cacheserver download failed because %s: '%s'", kErrorStrings[err], assetPath.c_str());
}

static std::string CacheServerUploadError (const std::string& assetPath, CacheServerError err)
{
	return Format ("Cacheserver upload failed because %s: '%s'", kErrorStrings[err], assetPath.c_str());
}

static std::string IntegrateCacheServerError (const std::string& assetPath)
{
	return Format ("CacheServer failed to apply cached asset: '%s'", assetPath.c_str());
}

static std::string ValidateCacheServerError (const std::string& assetPath, const std::string& error)
{
	return Format ("CacheServer Asset validation failed '%s'.\n%s", assetPath.c_str(), error.c_str());
}

static bool ValidateModelImporterAnimationReferences (const std::string& assetPath, const CacheServerAssetInfo& assetInfo)
{
	vector<UnityGUID> necessaryReferences;
	CollectNecessaryReferencedAnimationClipDependencies (assetPath, necessaryReferences);

	for (int i=0;i<necessaryReferences.size();i++)
	{
		bool found = false;
		for (int j=0;j<assetInfo.externalReferencesForValidation.size();j++)
		{
			if (assetInfo.externalReferencesForValidation[j].first == necessaryReferences[i])
			{
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	return true;
}

static bool ValidateCacheServerAssetInfo (const CacheServerData& cacheServerData, const CacheServerAssetInfo& assetInfo, const std::string& assetPath, int suggestedAssetImporterClassID)
{
	if (assetInfo.mainRepresentation.object.GetInstanceID() == 0)
	{
		ErrorString(ValidateCacheServerError(assetPath, "Main representation instanceID is null"));
		return false;
	}
	
	if (assetInfo.assetImporterClassID != suggestedAssetImporterClassID)
	{
		ErrorString(ValidateCacheServerError(assetPath, "Asset importer type does not match"));
		return false;
	}
	
	#define VALIDATE_DOWNLOAD_ALWAYS(path) if (!IsFileCreated(path)) { ErrorString(ValidateCacheServerError(assetPath, "File does not exist " + path)); return false; }
	#define VALIDATE_DOWNLOAD_CAN_BE_EMPTY(path) if (!path.empty() && !IsFileCreated(path)) { ErrorString(ValidateCacheServerError(assetPath, "File does not exist " + path)); return false; }

	// Validate that all files have been downloaded
	VALIDATE_DOWNLOAD_ALWAYS(cacheServerData.metaDataPath);
	
	// Validate that all external references are working
	for (int i=0;i<assetInfo.externalReferencesForValidation.size();i++)
	{
		UnityGUID guid = assetInfo.externalReferencesForValidation[i].first;
		std::string expectedAssetPath = assetInfo.externalReferencesForValidation[i].second;
		
		// There might be external references to builtin resources which have a constant GUID.
		if (GetGUIDPersistentManager().IsConstantGUID(guid))
			continue;
		
		string actualReferencedAssetPath = GetGUIDPersistentManager().AssetPathNameFromGUID(guid);
		string error;
		
		// The GUID has changed (Forgot to commit .meta file)
		if (actualReferencedAssetPath.empty() && !expectedAssetPath.empty())
		{
			UnityGUID newGUID;
			if (GetGUIDPersistentManager().PathNameToGUID (expectedAssetPath, &newGUID) && IsFileCreated(expectedAssetPath))
			{
				error = Format("Referenced file '%s' has mismatching GUID. Did you forget to commit the .meta file?\nCached GUID: '%s' New GUID: '%s'", expectedAssetPath.c_str(), GUIDToString(guid).c_str(), GUIDToString(newGUID).c_str());
				printf_console("%s\n", ValidateCacheServerError (assetPath, error).c_str());
				return false;
			}
		}

		// Expected asset path does not match the actual asset path
		if (expectedAssetPath != actualReferencedAssetPath)
		{
			error = Format("Referenced file '%s' has been moved to '%s'\nGUID: '%s'", expectedAssetPath.c_str(), actualReferencedAssetPath.c_str(), GUIDToString(guid).c_str());
			printf_console("%s\n", ValidateCacheServerError (assetPath, error).c_str());
			return false;
		}

		// Referenced asset does not exist anymore
		// (Using file check instead of assetdatabase since it might not yet have been imported)
		if (!IsFileCreated(expectedAssetPath))
		{
			error = Format("Reference file has been deleted '%s'\nGUID: '%s' ", expectedAssetPath.c_str(), GUIDToString(guid).c_str());
			printf_console("%s\n", ValidateCacheServerError (assetPath, error).c_str());
			return false;
		}
	}

	/// Validate FBX importer animation dependencies
	/// This is not really a true dependency system (We should use a proper dependency system.)
	if (assetInfo.assetImporterClassID == 1041)
	{
		vector<UnityGUID> necessaryReferences;
		CollectNecessaryReferencedAnimationClipDependencies (assetPath, necessaryReferences);
		
		if (!ValidateModelImporterAnimationReferences(assetPath, assetInfo))
		{
			printf_console("%s\n", ValidateCacheServerError (assetPath, "Required @ animation is not in the cached asset.").c_str());
			return false;
		}
	}
	
	//@TODO: Validate if the asset importer allows for not having a cache file
	
	return true;
}

enum CacheServerNotUsedReason { kUseCacheServer, kImporterNotSupported, kNoMetaFile, kImporterIsDirty /*, TODO: kSourceAssetIsDirty*/ };

static CacheServerNotUsedReason SupportCacheServerFileImport (int classID, const UnityGUID& guid, const std::string& assetPath)
{
	//Text script importer.
	//if (classID == 1031)
	//	return false;
	
	// Native format should not go through cache server because files are too small
	// and previews can change due to their dependencies in the project changing. (This is a general problem though, so maybe we should fix it properly)
	//if (classID == 1034)
	//	return kImporterNotSupported;

	// Shader importer is not supported since it needs to be called to new add shaders to the script mapper, so they can be found.
	// Also CGInclude files have to be included in the hash.
	if (classID == 1007)
		return kImporterNotSupported;

	// importer must have a valid importer and not use the fallback...
	if (classID == 0)
		return kImporterNotSupported;
	
	// Library importer is not supported
	if (classID == 1038)
		return kImporterNotSupported;
	
	// Default fallback importer is not supported
	if (classID == 1030)
		return kImporterNotSupported;
	
	// MonoScriptImporter & MonoAssemblyImporter is not supported because compilation has complex dependencies
	if (classID == 1050 || classID == 1035)
		return kImporterNotSupported;

	// If the asset importer is dirty we can not use caching
	if (IsAssetImporterDirty (guid))
		return kImporterIsDirty;

	// .meta file must exist
	if (!IsFileCreated(GetTextMetaDataPathFromAssetPath(assetPath)))
		return kNoMetaFile;
	
	return kUseCacheServer;
}

void CacheServer::UpdateProgress (int idx)
{
	float progress = idx / (float)m_AllRequests.size();
	const char* title = "Downloading from cache server";
	AssetImporter::GetUpdateProgressCallback() (0.0F, progress, title);
}

CachedAssetRequest* CacheServer::PopDownloadRequest ()
{
	CachedAssetRequest* request = NULL; 
	if (!m_DownloadRequests.empty())
	{	
		request = m_DownloadRequests.front();
		m_DownloadRequests.pop_front();
	}
	
	return request;
}

void CacheServer::SendPendingDownloadRequests ()
{
	// Download requests are only added to the end and are always popped from the front
	// Thus we just look through the queue from back to front to find the requests that still need to have their requests sent.
	
	int needRequestIndex = m_DownloadRequests.size();
	for (int i=m_DownloadRequests.size()-1;i>=0;i--)
	{
		if (!m_DownloadRequests[i]->sentDownloadRequest)
			needRequestIndex = min(i, needRequestIndex);
		else
			break;
	}
	
	for (int i=needRequestIndex;i<m_DownloadRequests.size();i++)
	{
		CachedAssetRequest& request = *m_DownloadRequests[i];
		Assert(!request.sentDownloadRequest);
		if (RequestFromCacheServer (request.guid, request.hash) == kCacheServerSuccess)
			request.sentDownloadRequest = true;
	}
}

void CacheServer::ProcessDownloadRequests ()
{
	m_TimerStats->cacheServerDownloading = START_TIME;
	CreateDirectoryRecursive(kCacheServerTempFolder);
	
	int progressCounter = 0;
	while (true)
	{
		SendPendingDownloadRequests();
		CachedAssetRequest* request = PopDownloadRequest();
		
		UpdateProgress(progressCounter);
		progressCounter++;
		
		// Process request
		if (request != NULL)
		{
			Assert(request->sentDownloadRequest);

			// printf_console(" -- CacheServer Request '%s' - '%s'\n", GUIDToString(request->guid).c_str(), GetAssetPathFromGUID(request->guid).c_str());
			
			CacheServerError result = DownloadFromCacheServer (request->guid, request->hash, request->downloadedData);
			
			if (result == kCacheServerSuccess)
			{
				m_TimerStats->cacheServerDownloadedBytes += request->downloadedData.downloadSize;
				m_TimerStats->cacheServerFoundAssets++;
				
				Assert(m_Completed.count(request->guid) == 0);
				m_Completed[request->guid] = request;
			}
			else
			{
				if (result == kCacheServerNotCached)
				{
					m_TimerStats->cacheServerUnavailableAssets++;
					printf_console(" -- No matching cache server file found on server '%s' (GUID: %s HASH: %s)\n", request->assetPath.c_str(), GUIDToString(request->guid).c_str(), MdFourToString(request->hash).c_str());
				}
				else
				{
					WarningString(CacheServerDownloadError (request->assetPath, result));
					DisconnectFromCacheServer();
					break;
				}				
			}
		}
		// Nothing available in the request queue
		else
		{
			break;
		}
	}

	m_TimerStats->cacheServerDownloading = ELAPSED_TIME(m_TimerStats->cacheServerDownloading);
}

CachedAssetRequest* CacheServer::GetCompletedRequest (const UnityGUID& guid)
{
	std::map<UnityGUID, CachedAssetRequest*>::iterator found = m_Completed.find(guid);
	if (found != m_Completed.end())
		return found->second;
	else
		return NULL;
}

struct SortHashingRequestByHexGUID
{
	bool operator () (const CachedAssetRequest* lhsRequest, const CachedAssetRequest* rhsRequest) const
	{
		return CompareGUIDStringLess(lhsRequest->guid, rhsRequest->guid);
	}
};

void CacheServer::DownloadCachedAssets (const CachedAssetRequest* requests, size_t requestSize, AssetImportingTimerStats* stats)
{
	Assert(IsConnectedToCacheServer());
	Assert(m_AllRequests.empty() && m_Completed.empty() && m_DownloadRequests.empty());
	
	m_TimerStats = stats;
	m_TimerStats->cacheServerRequestedAssets = requestSize;
	m_TimerStats->cacheServerNotSupportedAssets = 0;
	// Inject all requests into hashing requests
	m_AllRequests.reserve(requestSize);
	for (int i=0;i<requestSize;i++)
	{
		// Check if the asset supports the cache server?
		
		CacheServerNotUsedReason result = SupportCacheServerFileImport(requests[i].importerClassID, requests[i].guid, requests[i].assetPath);
		if (result != kUseCacheServer)
		{
			if (result == kImporterIsDirty)
				printf_console("Not using cache server because the importer is dirty '%s'.\n", requests[i].assetPath.c_str());
			
			m_TimerStats->cacheServerNotSupportedAssets++;
			continue;
		}
		
		m_AllRequests.push_back(requests[i]);
		m_DownloadRequests.push_back(&m_AllRequests.back());
	}

	// Try to make the requests come in linear order
	SortHashingRequestByHexGUID sortByGUIDHex;
	
	// Sort all download requests by GUID hex since that is the order in which they will be downloaded into the temp directory
	sort (m_DownloadRequests.begin(), m_DownloadRequests.end(), sortByGUIDHex);
	
	// Start processing downloads
	ProcessDownloadRequests ();
}

void CacheServer::ClearCachedAssetRequests ()
{
	///@TODO: Clear out temp folder with any left over assets that were not integrated.
	
	m_AllRequests.clear();
	m_DownloadRequests.clear();
	m_Completed.clear();
	m_TimerStats = NULL;
}

bool CacheServer::CommitToCacheServer (const Asset& asset, AssetImporter& importer, const UnityGUID& selfGUID, const std::string& assetPath, const AssetImporterSelection& importerSelection)
{
	///@TODO: DONT TIME OUT
	if (!IsConnectedToCacheServer ())
		return false;
	
	// Check if cache server is supported for this file
	CacheServerNotUsedReason useCacheServer = SupportCacheServerFileImport(importerSelection.importerClassID, selfGUID, assetPath);
	
	// Warn if there are no meta files. The user has to fix this.
	if (useCacheServer == kNoMetaFile)
	{
		WarningString(Format("Meta file does not exist, commit to cache server failed: '%s'", assetPath.c_str()));
		return false;
	}
	// When the importer is dirty, just log it for debugging purposes when the cacheserver is not working.
	else if (useCacheServer == kImporterIsDirty)
	{
		printf_console("Asset importer is dirty, commit to cache server failed: '%s'", assetPath.c_str());
		return false;
	}
	else if (useCacheServer != kUseCacheServer)
		return false;
	
	// Check that the importer allows uploading (For example if the importer had errors during import...)
	if (!importer.ValidateAllowUploadToCacheServer ())
	{
		printf_console("Not caching '%s' because it was imported in uncompressed asset mode or had an error during import.\n", GetAssetPathFromGUID(selfGUID).c_str());
		return false;
	}
	
	
	CreateDirectoryRecursive(kCacheServerTempFolder);
	
	CacheServerData data;
	data.metaDataPath = GetMetaDataPathFromGUID(selfGUID);

	CacheServerAssetInfo assetInfo;
	AssetToCacheServerInfo(asset, importerSelection.importerClassID, data.metaDataPath, assetInfo);
	WriteToYamlString (assetInfo, data.assetInfoData);
	
	CacheServerError result = UploadToCacheServer(selfGUID, asset.hash, data);
	
	if (result == kCacheServerSuccess)
		return true;
	else
	{
		DisconnectFromCacheServer();
		WarningString(CacheServerUploadError (assetPath, result));
		if (ConnectToCacheServer() != kCacheServerSuccess)
			ErrorString ("Cache Server upload interrupted because the connection was interrupted.");
		return false;
	}
}

////@TODO: Do this properly. If it makes a difference.

bool DeleteFileUnsafeFast (const std::string& path, ABSOLUTE_TIME& time)
{
	ABSOLUTE_TIME begin = START_TIME;

#if UNITY_WIN
	std::wstring widePath;
	ConvertUnityPathName (path, widePath);
	bool result = DeleteFileW (widePath.c_str());
#else
	bool result = (unlink(path.c_str()) == 0);
#endif

	time = COMBINED_TIME(time, ELAPSED_TIME(begin));
	
	return result;
}

bool MoveReplaceUnsafe (const std::string& from, const std::string& to, ABSOLUTE_TIME& time)
{
	ABSOLUTE_TIME begin = START_TIME;

#if UNITY_WIN
	std::wstring wideFrom, wideTo;
	ConvertUnityPathName( PathToAbsolutePath(from), wideFrom );
	ConvertUnityPathName( PathToAbsolutePath(to), wideTo );
	bool result =  MoveFileExW( wideFrom.c_str(), wideTo.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED );
#else
	bool result = (rename(from.c_str(), to.c_str()) == 0);
#endif
	time = COMBINED_TIME(time, ELAPSED_TIME(begin));
	
	return result;
}

bool CacheServer::LoadFromCacheServer (Asset& asset, const UnityGUID& selfGUID, const std::string& assetPath, const AssetImporterSelection& assetImporterSelection, int options)
{
	if ( options & kRefreshTextMetaFile ) 
		return false;
	
	// Not all importers support caching
	if (SupportCacheServerFileImport(assetImporterSelection.importerClassID, selfGUID, assetPath) != kUseCacheServer)
		return false;

	///@TODO: Validate if the asset importer is dirty. If it is dirty cache server can NOT BE USED!!

	ABSOLUTE_TIME cacheServerIntegrateTime = START_TIME;
	
	CachedAssetRequest* request = GetCompletedRequest (selfGUID);
	
	if (request == NULL)
		return false;

	// The asset should never be downloaded from the cache server
	Assert ((options & kDontImportWithCacheServer) == 0);
	
	const CacheServerData& downloadData = request->downloadedData;
	CacheServerAssetInfo assetInfo;
	ReadFromYamlString(downloadData.assetInfoData, assetInfo);

	if (!ValidateCacheServerAssetInfo(downloadData, assetInfo, assetPath, assetImporterSelection.importerClassID))
		return false;
	
	string metaDataPath = GetMetaDataPathFromGUID(selfGUID);
	
	// Unload all data currently in memory for this asset
	UnloadAllLoadedAssetsAtPath(metaDataPath);
	
	// Move meta file into right spot
	if (!MoveReplaceUnsafe(downloadData.metaDataPath, metaDataPath, m_TimerStats->cacheServerMoveFilesTime))
	{
		WarningString(IntegrateCacheServerError(assetPath));
		return false;
	}

	// Apply AssetDatabase info
	CacheServerAssetInfoToAsset (assetInfo, asset);
	asset.type = assetImporterSelection.generatedAssetType;
	
	m_TimerStats->cacheServerIntegrateTime = COMBINED_TIME(ELAPSED_TIME(cacheServerIntegrateTime), m_TimerStats->cacheServerIntegrateTime);
	
	return true;
}


CacheServer* gCacheServer = NULL;
CacheServer& GetCacheServer ()
{
	if (gCacheServer == NULL)
		gCacheServer = new CacheServer();
	
	return *gCacheServer;
}

//////@TODO: Cleanup is never called
void CleanupCacheServer ()
{
	delete gCacheServer;
	gCacheServer = NULL;
}
