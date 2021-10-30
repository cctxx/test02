#pragma once
#include "VCPlugin.h"
#include "VCAsset.h"
#include "VCChangeSet.h"
#include "VCSceneInspector.h"
#include "Editor/Src/EditorUserSettings.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Semaphore.h"
#include "Runtime/Utilities/NonCopyable.h"
#include "Runtime/Mono/MonoManager.h"
#include "Editor/Src/LicenseInfo.h"
#include <queue>

class Thread;
class VCCache;
class VCSettings;
class VCTask;

enum CheckoutMode
{
	kAsset = 1,
	kMeta = 2,
	kBoth = 3
};

enum ResolveMethod
{
	kUseMine = 1,
	kUseTheirs = 2,
	kUseMerged = 3,
};

enum MergeMethod
{
	kMergeNone = 0,
	kMergeAll = 1,
	kMergeNonConflicting = 2
};

enum OnlineState
{
	kOSUpdating = 0,
	kOSOnline = 1,
	kOSOffline = 2
};

enum RevertMode
{
	kRevertNormal = 0,           // Reverts files to state of the base revision
	kRevertUnchanged = 1,        // Makes sense for vcs that requires checkouts before edit
	kRevertKeepModifications = 2 // Like revertnormal by but does not change the files - just state ie.
};

enum FileMode
{
	kFMBinary = 1,
	kFMText = 2
};

class VCProvider
{
public:
	VCProvider();
	~VCProvider();

	void Initialize();

	VCTask* UpdateSettings();

	VCPlugin* GetActivePlugin () { return m_ActivePlugin; }

	// Load overlay icons into atlas for active plugin type
	void LoadOverlayIcons();
	Texture2D* GetOverlayAtlas();
	Rectf GetAtlasRectForState(States state);
	
	// Disables availability of the specified builtin command.
	void DisableCommand(VCCommandName n);
	void EnableCommand(VCCommandName n);

	// Disables availability of the specified custom command
	void DisableCustomCommand(const std::string& n);
	void EnableCustomCommand(const std::string& n);
	bool IsCustomCommandEnabled(const std::string& n) const;
	bool HasCustomCommand(const std::string& n) const;
	const VCCustomCommands& GetCustomCommands() const { return m_ActiveCustomCommands; }
	VCTask* StartCustomCommandTask(const std::string& commandName);

	void SetPluginAsOnline();
	void SetPluginAsUpdating();
	void SetPluginAsOffline(const std::string& reason);
	OnlineState GetOnlineState() const { return m_ActivePluginIsOnline; }
	std::string OfflineReason() const { return m_ActivePluginOfflineReason; }
	
	void Tick();

	// This indicates if a vc system has been selected in
	// the preferences and that license allows for vc
	bool Enabled() const;

	// Indicates that vc is enabled and has not been set as
	// working offline.
	bool IsActive() const;

	VCTask* Refresh(VCAssetList const& assets, bool recursively);

	VCTask* ChangeSetDescription(const VCChangeSetID& id);
	VCTask* ChangeSets();
	VCTask* Incoming();
	VCTask* ChangeSetStatus(const VCChangeSetID& id);
	VCTask* IncomingChangeSetAssets(const VCChangeSetID& id);
	VCTask* Status(VCAssetList const& assetList, bool recursively);
	bool CheckoutIsValid(VCAssetList const& asset);
	VCTask* Checkout(VCAssetList const& assetList, CheckoutMode mode);
	bool PromptAndCheckoutIfNeeded(const VCAssetList& assetList, const std::string& promptIfCheckoutNeeded);

	bool DeleteChangeSetsIsValid(const VCChangeSets& changesets);
	bool DeleteChangeSetsIsValid(const VCChangeSetIDs& ids);
	
	VCTask* DeleteChangeSets(const VCChangeSets& changesets);
	VCTask* DeleteChangeSets(const VCChangeSetIDs& ids);

	VCTask* Delete(const VCAssetList& assetList);

	bool RevertIsValid(VCAssetList const& assets, RevertMode mode);
	VCTask* RevertChangeSets(VCChangeSets const& changesets, RevertMode mode);
	VCTask* RevertChangeSets(const VCChangeSetIDs& ids, RevertMode mode);

	VCTask* Revert(VCAssetList const& assets, RevertMode mode);

	bool SubmitIsValid(VCChangeSet* changeset, VCAssetList const& assets);
	VCTask* Submit(VCChangeSet* changeset, VCAssetList const& assets, std::string const& description, bool saveOnly);

	bool AddIsValid(VCAssetList const& assets);
	VCTask* Add(VCAssetList const& assetList, bool recursive);

	VCTask* Move(const VCAsset& src, const VCAsset& dst, bool noLocalFileMove = false);
	VCTask* ChangeSetMove(VCAssetList const& assetList, const VCChangeSetID& id);

	bool GetLatestIsValid(VCAssetList const& assets);
	VCTask* GetLatest(VCAssetList const& assets);

	bool LockIsValid(VCAssetList const& assets);
	bool UnlockIsValid(VCAssetList const& assets);
	VCTask* Lock(VCAssetList const& assets, bool locked);

	bool DiffIsValid(const VCAssetList& assets);
	VCTask* DiffHead(const VCAssetList& assets, bool includingMetaFiles);
	
	bool ResolveIsValid(const VCAssetList& assets);	
	VCTask* Resolve(const VCAssetList& assets, ResolveMethod resolveMethod);
	VCTask* Merge(const VCAssetList& assets, MergeMethod method);

	VCTask* SetFileMode(const VCAssetList& assets, FileMode mode);

	bool IsOpenForEdit(VCAsset* asset);

	//int GetChangeSetID (VCChangeSet const* changeset);

	VCTask* CreateErrorTask(std::string const& message);
	VCTask* CreateSuccessTask();

	int GenerateID ();
	
private:
	void CleanupQueues();
	void Stop();
	void QueueTask(VCTask* task);
	VCTask* DequeueTask();

	void QueueCompletedTask(VCTask* task);
	VCTask* DequeueCompletedTask();
	void IncludeMissingAndUnknownAssets(const VCAssetList& assets, VCAssetList& result);

	static void* TaskWorker(void* data);

public:
	void EndPluginSession();
	void ClearPluginMessages();
	VCPluginSession& EnsurePluginIsRunning(VCMessages& msgs);
	VCPluginSession* GetPluginSession() const {return m_PluginSession;}
	void SendSettingsToPlugin(VCMessages& msgs);
	static bool ReadPluginStatus();
	void ResetPlugin();

	// Remember to Retain() the task to update ref count and Release() when done.
	VCTask* GetActiveTask();
	
	const string& GetActivePluginName() const {return m_ActivePluginName;}
	void SetActivePluginName(const std::string& activePluginName) {m_ActivePluginName = activePluginName;}

	const VCConfigFields& GetActiveConfigFields() const { return m_ActiveConfigFields; }
	const VCPlugin::Traits& GetActiveTraits() const { return m_ActiveTraits; }
	
	const EditorUserSettings::ConfigValueMap& GetActivePluginSettings() const {return m_Settings;}
	void SetPluginSettings(const EditorUserSettings::ConfigValueMap& settings) {m_Settings = settings;}

	bool HasSupportVCSelection() const;
	bool HasScanningStarted() const {return m_PluginScanningStarted;}

	ScriptingArray* GetMonoCustomCommands () const;
	
	void SchedulePendingWindowUpdate();
	void TickInternal();
	
private:

	Thread* m_WorkerThread;

	Mutex m_ActiveTaskQueueMutex;
	Mutex m_CompletedTaskQueueMutex;
	std::queue<VCTask*> m_TaskQueue;
	std::queue<VCTask*> m_CompletedTaskQueue;

	Semaphore m_WaitSemaphore;
	Semaphore m_QueueSemaphore;
	bool m_StopThread;
	bool m_Running;
	bool m_PluginScanningStarted;
	volatile int m_IDGenerator;
	bool m_PendingWindowUpdateScheduled;
	Mutex m_ProcessingTask;

	Mutex m_ActiveTaskMutex;
	VCTask* m_ActiveTask;

	VCCache* m_StateCache;

	VCPluginSession* m_PluginSession;
	EditorUserSettings::ConfigValueMap m_Settings; // copied from main thread
	std::string m_ActivePluginName; // Name of active plugin
	VCPlugin*        m_ActivePlugin;
	VCConfigFields   m_ActiveConfigFields; // Config fields for a plugin with the name of m_ActivePluginName
	VCPlugin::Traits m_ActiveTraits;       // Traits for a plugin with the name of m_ActivePluginName
	VCCustomCommands m_ActiveCustomCommands; // Custom commands provided by the plugin
	OnlineState m_ActivePluginIsOnline; // The plugin can inform about online/offline state
	std::string m_ActivePluginOfflineReason; 
	
	std::set<VCCommandName> m_DisabledCommands;     // The plugin can enable/disable commands at will
	std::set<std::string> m_DisabledCustomCommands; //

	VCSceneInspector* m_SceneInspector;  // Scene inspector used to track dirty objects in memory for plugins that does not require checkout

	// Custom icons loaded by plugin vendor is loaded into an atlas.
	typedef std::map<States, Rectf> AtlasStateRects;
	Texture2D* m_OverlayAtlas;
	AtlasStateRects m_AtlasStateRects;
	
	static VCProvider* instance;
	friend bool HasVCProvider();
	friend VCProvider& GetVCProvider();
	friend VCProvider* GetVCProviderPtr();
	friend void CleanupVCProvider();
};

bool HasVCProvider ();
VCProvider& GetVCProvider ();
VCProvider* GetVCProviderPtr() ;
void CleanupVCProvider ();
