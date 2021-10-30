#ifndef AssetServerController_H
#define AssetServerController_H

#include "Runtime/Utilities/GUID.h"
#include "Editor/Src/AssetPipeline/MdFourGenerator.h"
#include "ASCache.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Mutex.h"
#include "ASProgress.h" 

#include <set>
#include <map>
#include <string>

class AssetDatabase;
class AssetServerCache;

namespace AssetServer {

class Backend;

// Passed to CommitChangeset to upload data to the server
struct UploadItem {
	UnityGUID guid;
	string	name;
	UnityGUID parent;
	MdFour digest;
	ItemType type;
	string path;
	int	predecessor; // The changeset of the asset this asset inherits from
	bool reuseStreams; // If true, the asset will reuse the file streams from the previous version and ignore the contents of newStreams
	std::map<std::string,std::string> newStreams; // Map from stream names to file paths that will be uploaded to the server
};

// Passed to DownloadAssets to download data from the server
struct DownloadItem {
	UnityGUID guid;
	int	changeset; 
	std::map<std::string,std::string> streams; // After the download has finished, this field will contain a map from stream names to file paths
};

/// A map of stream-identifier to path-name
typedef map<string, string> StreamMap;


struct CompareInfo {
	int left;
	int right;
	int convert_binary;
	int autodetect_binary;
	CompareInfo(): left(-1), right(-2), convert_binary(false) {}
	CompareInfo(int l, int r, bool b): left(l), right(r), convert_binary(b) {}
};


enum CreateAssetOptions {
	kCO_None			= 0, 
	kCO_Move			= 00000001, // rename asset if it exists but with the wrong path name - if this is not set, the creation will fail
	kCO_RetainVersion	= 00000002, // don't update the asset version if it's older than the current version
	kCO_UseServerDigest = 00000004, // don't update the digest when importing the newly created asset, but use the one from the server
	kCO_Create			= 00000100, // create asset if required (Note: similar to O_CREAT for open)
	kCO_Exclusive		= 00000200 // asset must not exist (Note: similar to O_EXCL for open)
};

struct CompareFileData
{
	Item item;
	UnityStr label;
	string filename;
	string meta;
	string txtMeta;
	int download_index;
	int changeset;
	CompareFileData(): download_index(-1) {}
};

static const UnityGUID kTrashGUID (0xffffffff,0xffffffff,0xffffffff,0xffffffff);

class Controller {
 public:

	/* Object setup */

	// Get the singleton object
	static Controller& Get ();
	
	// Set project settings
	void Initialize (const std::string& userName, const std::string& connectionSettings, int timeout);
	// Query online state
	bool IsOnline() { return m_Online; }
	
	// Connection settings &c.
	int GetMaximumTimout() const { return m_Timeout; }
	const std::string& GetUserName () const { return m_UserName; }
	const std::string& GetConnectionSettings () const { return m_ConnectionSettings; }

	const UnityStr& GetLastError () const  { return m_ErrorString; }

	int GetLatestServerChangeset();

	/* High-level Asset versioning functions - originally found in MaintClient.cpp */
	
	bool UpdateStatus(); // Synchronizes the locally stored version data with the server - Must be fast when there are no or few changes
	
	// Update from server:

	// (If deleteLocal is true, you should not call UpdateGetConflicts and UpdateSetResolutions)
	bool UpdateBegin(const set<UnityGUID>& candidates, bool deleteLocal = false, bool usedForRevert = true); 
	bool UpdateGetConflicts(set <UnityGUID>* conflicts);
	bool UpdateSetResolutions(const map<UnityGUID, DownloadResolution>& resolutions);
	bool UpdateStartDownload(); // Starts a download of all updates
	float UpdateGetDownloadProgress( string& statusString); // Queries the download progress
	bool UpdateComplete(); // Call this after the download has finished to complete the update

	bool UpdateAbort();
	
	bool InitializeFromCommandLine(const vector<string> &options);
	bool AssetServerUpdateCommandLine(const vector<string> &options);
	bool AssetServerCommitCommandLine(const vector<string> &options);
	
	// Commit changes
	bool CommitBegin(const string& changesetDescription, const set<UnityGUID>& candidates);
	bool CommitStartUpload();
	float CommitGetUploadProgress(string & statusString);
	bool CommitComplete();
	bool CommitAbort();

	
	// Revert working assets
	bool RevertVersion(const UnityGUID& asset, int version);
	bool RevertVersion(const UnityGUID& asset, int version, const string& iname, const UnityGUID& iparent, bool forceParent);

	// Recover deleted
	bool RecoverDeleted(const UnityGUID& asset, int version, const string& name, const UnityGUID& parent);
	
	// Compare
	bool CompareFiles(const map<UnityGUID,CompareInfo>& selection);

	
	// Stringification of enums
	static const char* GetStringForStatus(Status status);
	static const char* GetStringForDownloadResolution(DownloadResolution resolution);
	static const char* GetStringForNameConflictResolution(NameConflictResolution resolution);

	/* Metadata & assetserver information cache setters and getters */
	DownloadResolution GetDownloadResolution(const UnityGUID& guid);
	NameConflictResolution GetNameConflictResolution(const UnityGUID& guid);
	bool IsMarkedForRemoval(const UnityGUID& guid);
	
	void SetDownloadResolution(const UnityGUID& guid, DownloadResolution value);
	void SetNameConflictResolution(const UnityGUID& guid, NameConflictResolution value);
	void SetMarkedForRemoval(const UnityGUID& guid, bool value);
	
	/// Get information from about the client's asset database
	UnityGUID GetAssetParent(const UnityGUID& guid);
	std::string GetAssetName(const UnityGUID& guid);
	std::string GetAssetPathName(const UnityGUID& guid);

	bool AssetIsDir(const UnityGUID& guid);
	bool AssetDirIsEmpty(const UnityGUID& guid);
	
	bool DoesAssetExist(const UnityGUID& guid);

	std::string AssetInfoToPathName (const Item& info);
	
	/// Returns the StreamMap for a guid.
	/// The map returned contains:
	///		"asset" -> pathname to the asset file ("Assets/foo.psd")
	///		"asset.meta" -> pathname to the asset text meta file ("Assets/foo.psd.meta")
	///		"preview.png" -> pathname to the asset preview file ("Library/Preview/02/118468468486486")
	/// If includeBinaryMetaData is true, the map returned contains "metaData" -> pathname to the metadata file ("Library/MetaData/02/118468468486486")
	/// This function raises a fatal error if metafiles are not enabled and it is called with includeBinaryMetaData==false
	StreamMap GetAssetStreams (const UnityGUID& guid, bool includeBinaryMetaData=false);
	
	void SetConnectionError (const std::string& error) { m_HasConnectionError = true; m_ErrorString = error; }
	void SetError(const std::string& error) { m_HasConnectionError = false; m_ErrorString = error; }
	const UnityStr& GetErrorString () const { return m_ErrorString; }
	bool HasConnectionError() const { return m_HasConnectionError; }
	void ClearError() { m_HasConnectionError = false; m_ErrorString.clear(); };

	bool IsBusy() const { return m_needToFinishAction != kNTFNone; }
	bool GetLastActionResult() const { return m_lastActionResult; }

	/* Asset maintainance functions */

	/// Lock assets has to be called before changing assets through the maint interface.
	void LockAssets ();
	/// Unlock assets has to be called after changing assets throught the maint interface
	void UnlockAssets (int importFlags = kForceSynchronousImport);
	
	/// Replaces the asset defined by info. The files to create the asset are defined by inputStreams.
	/// inputStreams is a map containing pathnames to the files that make up the asset
	/// the map has to contain:
	///	"asset" -> the asset file (in a project this is stored in the Assets folder)
	///	"metaData" -> the metaData file (in a project this is stored in the Library/MetaData folder)
	/// Returns if the operation was completed successfully
	bool ReplaceAsset (const Item& info, const StreamMap& inputStreams = StreamMap(), int options = kCO_None ) { return CreateAsset(info, inputStreams,options); }

	/// Moves an asset identifier by 'guid', to be a child of 'parent' and have the name 'newName'
	/// Returns if the operation was completed successfully
	bool MoveAsset (const UnityGUID& guid, const UnityGUID& parent, const std::string& newName);

	/// Creates an asset defined by info. The files to create the asset are defined by inputStreams.
	/// inputStreams is a map containing pathnames to the files that make up the asset
	/// If info.type is kTDir, inputStreams must be empty.
	/// the map has to contain:
	///	"asset" -> the asset file (in a project this is stored in the Assets folder)
	///	"metaData" -> the metaData file (in a project this is stored in the Library/MetaData folder)
	/// Returns if the operation was completed successfully
	bool CreateAsset (const Item& info, const StreamMap& inputStreams = StreamMap(), int options = kCO_Create | kCO_Exclusive );

	bool AssetIsBinary(const string& assetPath);
	bool AssetIsBinary(const UnityGUID& asset);

	/// Removes an asset defined by guid. (The asset will be moved to the trash)
	/// Returns if the operation was completed successfully
	bool RemoveAsset (const UnityGUID& guid);

	void RemoveMaintErrorsFromConsole() {
		if(m_AssetServerCache)
			RemoveErrorWithIdentifierFromConsole (m_AssetServerCache->GetInstanceID ());
	}

	// Mono-to-cpp
	void SetAfterActionFinishedCallback(const string& className, const string& functionName);

	void DoUpdateOnNextTick(bool forceShowConflictResolutions, string backendFunctionForConflictResolutions, const map<UnityGUID, DownloadResolution>& downloadResolutions, const vector<UnityGUID>& nameConflicts);
	void DoCommitOnNextTick(const string& description, const set<UnityGUID>& guids);
	void DoRecoverOnNextTick(const vector<DeletedAsset>& sortedList);
	void DoRevertOnNextTick(int changeset, const UnityGUID& guid);
	void DoUpdateStatusOnNextTick();
	void DoRefreshAssetsOnNextTick();
	void DoRefreshAssetsAndUpdateStatusOnNextTick();
	void DoUpdateWithoutConflictResolutionOnNextTick(const set<UnityGUID>& guids);
	void DoShowDialogOnNextTick(string title, string text, string button1, string button2);

	void Tick();

private:
 	Controller() : m_Online(false), m_AssetDatabase( &AssetDatabase::Get()), m_AssetServerCache(&AssetServerCache::Get ()) {} // Default constructor private -> use Get() instead
	Controller(const Controller& copy); // Disallow copying by making copy constructor private and unimlpemented
	
	/// Used to map parent guids to asset names when moving assets.
	typedef pair<UnityGUID, UnityStr> ParentToName;
	// Used to store a reference into the downloads array.
	typedef pair<Item, int> ItemToIndex;

	
	class UpdateHandle;
	class CommitHandle;

	void SetupStreams(UnityGUID guid, CompareFileData &data, vector<DownloadItem> &toDownload, int otherChangeset);
	void SetupStreamFilesAndLabels(CompareFileData &data, vector<DownloadItem> &toDownload, string labelString);
	void HandleNonExistingFile(CompareFileData &data, CompareFileData &otherData);
	bool ConvertToBinary(CompareFileData &data);

	bool CompareSingleFile (const UnityGUID& guid, const CompareInfo& opts);
	bool ConvertToTextFile (const string& assetPath, const string& metaDataPath, const string& cacheDataPath, const string& outPath, int options);
	bool InvokeMergeApp(const string& remoteLabel, const string& remote, const string& localLabel, const string& local, const string& ancestorLabel,  const string& ancestor, const string& target);
	bool InvokeCompareApp(const string& leftLabel, const string& left, const string& rightLabel,  const string& right, const string& ancestorLabel, const string& ancestor) ;
	bool RevertIMPL(const UnityGUID& asset, int version, const string& name, const UnityGUID& parent, int options, bool forceParent);

	void Received(const UnityGUID& guid) { m_AssetServerCache->SetReceived(guid); }

	/* Conflict resolution settings */

	/// Filename for temporary files
	string TempFilename(const UnityGUID& guid, int version, const string& stream);

	
	/// Saves all dirty files. And refreshes all assets that need refresh.
	void PrepareAssetsForStatus ();

	bool LocateChildAsset (const UnityGUID& parent, const string& name, UnityGUID* out_guid);

	/// Returns an identifier for logging maint errors
	int GetLogId() { return m_AssetServerCache?m_AssetServerCache->GetInstanceID ():0; }

	bool CheckItemNameValidToDownload (const AssetServer::Item& item);

	class AutoLockAssets
	{
	public:
		AutoLockAssets( Controller& app )
		: m_App(&app)
		{
			app.LockAssets();
		}
		~AutoLockAssets()
		{
			m_App->UnlockAssets();
		}
	private:
		Controller*	m_App;
	};

	std::set<UnityGUID> m_NeedsMetaDataUpdate;
	
	int m_Timeout;
	std::string m_UserName;
	std::string m_ConnectionSettings;
	bool m_Online;

	AssetDatabase* m_AssetDatabase;
	AssetServerCache* m_AssetServerCache;
		
	std::map<UnityGUID, MdFour> m_NeedsReimport;
	UnityStr m_ErrorString;
	bool m_HasConnectionError;
	
	std::map<UnityGUID,string> m_NewlyCreatedDirectories; // Used so one can create assets while parent assets have not been imported yet
	
	UpdateHandle* m_Update;
	CommitHandle* m_Commit;

	string m_DialogTitle;
	string m_DialogText;
	string m_DialogButton1;
	string m_DialogButton2;

	// Mono-to-cpp
	enum {kNTFNone, kNTFBusy, kNTFUpdate, kNTFCommit, kNTFRecover, kNTFRevert, kNTFStatus, kNTFUpdateWithoutConflicts, kNTFRefresh, kNTFStatusAndRefresh, kNTFShowDialog};
	int m_needToFinishAction;

	bool m_lastActionResult;

	bool m_forceShowConflictResolutions;
	string m_backendFunctionForConflictResolutions;
	map<UnityGUID, DownloadResolution> m_downloadResolutions;
	vector<UnityGUID> m_nameConflicts;
	vector<DeletedAsset> m_sortedRecoverList;
	set<UnityGUID> m_updateItems;
	set<UnityGUID> m_commitItems;
	string m_commitDescription;
	UnityGUID m_revertItem;
	int m_revertChangeset;
	string m_actionFinishedCallbackClass;
	string m_actionFinishedCallbackFunction;
	bool m_actionDidNotFinish;

	void CallMonoAfterActionFinishedCallback();
	bool FinishMonoUpdate();
	bool FinishMonoCommit();
	bool FinishMonoRecover();
	bool FinishMonoRevert();
	bool FinishMonoUpdateStatus();
	bool FinishMonoRefreshAssets();
	bool DoUpdate();
	bool DoUpdateWithoutConflictResolution();
	bool DoShowDialog();
	
	class BaseHandle : public  ProgressCallback {
	  protected:
		Mutex m_ProgressMutex;
		double startTime;
		float progress;
		string progressText;
		Thread m_Thread;
	  public:
		float GetProgress(string& text)
		{
			m_ProgressMutex.Lock();
			float retval=progress;
			text=progressText;
			m_ProgressMutex.Unlock();
			return retval;
		}
		bool ShouldAbort()
		{
			return m_Thread.IsQuitSignaled();
		}
		
	};
	
	class UpdateHandle : public BaseHandle {
	  private:
		
		Controller* m_Controller;
		Backend* m_Client;
		bool m_Locked;
		bool m_DeleteLocal;
		set<UnityGUID>* m_Candidates;
		set<UnityGUID>* m_Conflicts;
		map<UnityGUID, DownloadResolution>* m_Resolutions;
		bool m_ThreadResult;
		bool m_UsedForRevert; // deleted item downloading works differently for reverting
		
		// Changes to reconcile
		map<UnityGUID, Item>* m_Changes;
		
		// The following set of variables are populated inside Download() and used by Complete()
		map<UnityGUID,Item> m_DirsToCreate;
		vector<DownloadItem> m_Downloads; // Will get populated with asset versions that should  be downloaded.
		map<UnityGUID,ItemToIndex> m_FileContents; // Reference into the downloads array
		map<UnityGUID,ItemToIndex> m_Merges; // Reference into the downloads array for assets that need merging. The index points to the ancestor and the lastest version is stored in filecontents
		map<UnityGUID, Item> m_AssetVersions;
		map<UnityGUID, ParentToName> m_AssetsToMove;

		void Cleanup();
		bool ResolveExistingAssetAtDestination( const UnityGUID& guid, const UnityGUID& parent, UnityStr& name  );
		void InitConflicts();
		static void* DownloadThreadEntry(void* instance);
		bool DownloadThreadMain();
		bool TryRestoreDeletedParent(Item item);
	  public:
		virtual void UpdateProgress(SInt64 bytes, SInt64 total, const std::string& currentFileName); // Inherited from ProgressCallback
		// Starts a new download operation - locks the server
		UpdateHandle(Controller* controller, const set<UnityGUID>& candidates, bool deleteLocal = false, bool usedForRevert = true);

		bool Init();
		bool GetConflicts(set <UnityGUID>* conflicts);
		bool SetResolutions(const map<UnityGUID, DownloadResolution>& resolutions);
		bool Download();
		bool Progress(float* progress, string* status);
		bool Complete();
		
		bool IsEmpty() const { return m_Candidates == NULL || m_Candidates->empty(); }
		
		~UpdateHandle();
	};

	class CommitHandle : public BaseHandle {
	  private:
		
		Controller* m_Controller;
		Backend* m_Client;
		bool m_Locked;
		int m_NewChangeset;
		
		vector<UploadItem>* m_Items;
		map< string, UnityGUID >* m_AlphaAssetsToTrash;

		
		string m_ChangesetDescription;
		set<UnityGUID>* m_Candidates;
		bool m_ThreadResult;

		void Cleanup();
		static void* UploadThreadEntry(void* instance);
		bool UploadThreadMain();
		
	  public:
		virtual void UpdateProgress(SInt64 bytes, SInt64 total, const std::string& currentFileName); // Inherited from ProgressCallback
		// Starts a new download operation - locks the server
		CommitHandle(Controller* controller, const string& changesetDescription, const set<UnityGUID>& candidates);

		bool Init();
		bool Upload();
		bool Progress(float* progress, string* status);
		bool Complete();

		void ClearDeleted();

		~CommitHandle();
	};

};

extern const char* kAssetStream;
extern const char* kAssetResourceStream;
extern const char* kBinMetaStream;
extern const char* kTxtMetaStream;
extern const char* kAssetPreviewStream;

}

#endif
