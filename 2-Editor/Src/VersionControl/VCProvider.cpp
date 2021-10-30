#include "UnityPrefix.h"
#include "VCProvider.h"
#include "VCAutoAddAssetPostprocessor.h"
#include "VCCache.h"
#include "VCTask.h"
#include "VCTasks/AddTask.h"
#include "VCTasks/CheckoutTask.h"
#include "VCTasks/DeleteChangeSetsTask.h"
#include "VCTasks/DeleteTask.h"
#include "VCTasks/DiffTask.h"
#include "VCTasks/GetAssetsStatusInChangeSetTask.h"
#include "VCTasks/GetIncomingTask.h"
#include "VCTasks/GetIncomingAssetsInChangeSetTask.h"
#include "VCTasks/GetLatestTask.h"
#include "VCTasks/GetOutgoingTask.h"
#include "VCTasks/GetChangeSetDescriptionTask.h"
#include "VCTasks/LockTask.h"
#include "VCTasks/MergeTask.h"
#include "VCTasks/MoveTask.h"
#include "VCTasks/MoveToChangeSetTask.h"
#include "VCTasks/RevertChangeSetsTask.h"
#include "VCTasks/RevertTask.h"
#include "VCTasks/ResolveTask.h"
#include "VCTasks/StatusTask.h"
#include "VCTasks/SubmitTask.h"
#include "VCTasks/UpdateSettingsTask.h"
#include "VCTasks/FileModeTask.h"
#include "VCTasks/CustomCommandTask.h"

#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/EditorSettings.h"
#include "Editor/Src/EditorUserSettings.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Editor/Platform/Interface/ExternalProcess.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Geometry/TextureAtlas.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/ImageConversion.h"
#include "Runtime/Threads/AtomicOps.h"

VCProvider* VCProvider::instance = NULL;

VCProvider::VCProvider()
: m_WorkerThread(NULL)
, m_ActivePlugin(NULL)
, m_StopThread(false)
, m_Running(false)
, m_PluginScanningStarted(false)
, m_PluginSession(NULL)
, m_ActivePluginIsOnline(kOSOffline)
, m_IDGenerator (0)
, m_ActiveTask(NULL)
, m_SceneInspector(NULL)
, m_OverlayAtlas(NULL)
, m_PendingWindowUpdateScheduled(false)
{
	AssetDatabase::RegisterPostprocessCallback(VCAutoAddAssetPostprocess);
	m_StateCache = UNITY_NEW(VCCache, kMemVersionControl) ();
	ResetPlugin();
}

VCProvider::~VCProvider()
{
	GetSceneTracker().RemoveSceneInspector(m_SceneInspector);
	UNITY_DELETE( m_SceneInspector, kMemVersionControl );

	if (m_WorkerThread != NULL)
	{
		m_WorkerThread->WaitForExit();
		UNITY_DELETE( m_WorkerThread, kMemVersionControl );
		m_WorkerThread = NULL;
	}

	UNITY_DELETE( m_StateCache, kMemVersionControl );
	ResetPlugin();
    instance = NULL;
}

void VCProvider::Initialize()
{
	m_WorkerThread = UNITY_NEW(Thread, kMemVersionControl) ();
	m_WorkerThread->SetName("VersionControlWorker");
	m_WorkerThread->Run(VCProvider::TaskWorker, this);

	// Make sure that newly spawned thread will not stop itself as 
	// the first thing
	m_StopThread = false;
	
	// Wait for worker thread to be up and running
	m_WaitSemaphore.WaitForSignal();
	m_Running = true;

	UpdateSettings();
	
	return;
}

void VCProvider::CleanupQueues()
{
	// Wait for all tasks to complete
	while (true)
	{
		{
			Mutex::AutoLock aLock(m_ActiveTaskQueueMutex);
			if (m_TaskQueue.size() == 0)
				break;
		}

		Thread::Sleep(1);
	}

	Mutex::AutoLock cLock(m_CompletedTaskQueueMutex);
	VCTask* completedTask = DequeueCompletedTask();
	bool anyHasRepaint = false;
	while(completedTask != NULL)
	{
		// Signal any callback about completion
		anyHasRepaint = anyHasRepaint || completedTask->m_RepaintOnDone;
		completedTask->m_RepaintOnDone = false;
		completedTask->Done();
		completedTask->Release();
		
		completedTask = DequeueCompletedTask();
	}

	// TODO: Are we allowed to call repaint at this point ie. are we in progress of domain reload
	if (anyHasRepaint)
	{
		MonoException* exception = NULL;
		CallStaticMonoMethod("EditorApplication", "Internal_RepaintAllViews", NULL, &exception);
	}
}

// This method may be called from main thread
void VCProvider::Stop ()
{
	assert(Thread::CurrentThreadIsMainThread());

	// Prevent further tasks to be added
	m_Running = false;

	m_PluginScanningStarted = false;

	CleanupQueues();

	// Signal the thread there is a new message and it will stop
	// itself
	m_StopThread = true;
	m_QueueSemaphore.Signal();
	
	if (m_WorkerThread && Thread::CurrentThreadIsMainThread())
	{
		m_WorkerThread->WaitForExit();
		UNITY_DELETE( m_WorkerThread, kMemVersionControl );
		m_WorkerThread = NULL;
	}
}

bool VCProvider::HasSupportVCSelection() const
{
	EditorSettings* settings = GetEditorSettingsPtr();
	// For some reason the asset server, deletes the editor settings during initial checkout
	// Remove this when asset server is moved to plugin
	if (settings == NULL)
		return false;

	// If no team license is installed VC integration cannot be supported
	if (!LicenseInfo::Flag (lf_maint_client))
		return false;

	string vc = settings->GetExternalVersionControlSupport();
	bool supportedVC = (vc != ExternalVersionControlAutoDetect && vc != ExternalVersionControlHiddenMetaFiles && 
						vc != ExternalVersionControlAssetServer && vc != ExternalVersionControlVisibleMetaFiles);

	return supportedVC;
}


bool VCProvider::Enabled () const
{
	bool isReady = m_Running && HasSupportVCSelection();
	if (!isReady) 
		return false;

	if (m_ActivePlugin == NULL)
		return false;
	
	// Check if config has all required fields set
	const VCPlugin& p = *m_ActivePlugin;
	if (p.IsNull()) 
		return false;

	const VCConfigFields& fields = p.GetConfigFields();
	for (VCConfigFields::const_iterator i = fields.begin(); i != fields.end(); ++i)
	{
		if (i->IsRequired() && Trim(GetEditorUserSettings().GetConfigValue(i->GetName())).empty())
			return false;
	}
	return true;
}

VCTask* VCProvider::GetActiveTask()
{
	VCTask* t = NULL;
	{
		Mutex::AutoLock alock(m_ActiveTaskMutex);
		t = m_ActiveTask;
	}
	return t;
}

// Worker thread
void* VCProvider::TaskWorker(void* data)
{
	VCProvider* instance = static_cast<VCProvider*>(data);
	instance->m_WaitSemaphore.Signal();

	// This thread never dies, will have to do something about that.
	while(true)
	{
		instance->m_QueueSemaphore.WaitForSignal();

		// The thread may have been stopped by main thread
		if (instance->m_StopThread)
		{
			instance->m_StopThread = false;
			return NULL;
		}

		VCTask* task = instance->DequeueTask();
		
		if (task)
		{
			// scoped lock
			{
				Mutex::AutoLock(instance->m_ActiveTaskMutex);
				instance->m_ActiveTask = task;
			}
			
			Mutex::AutoLock(instance->m_ProcessingTask);
			try 
			{
				// Execute the task. This will not signal the Semaphore because
				// we want the task to be enqueued in the completed queue before
				// that signal is sent from this worker thread.
				if (GetVCProvider().GetPluginSession())
					GetVCProvider().GetPluginSession()->SetProgressListener(NULL);

				task->Execute();
				
				// scoped lock
				{
					Mutex::AutoLock(instance->m_ActiveTaskMutex);
					instance->m_ActiveTask = NULL;
				}
			}
			catch (ExternalProcessException e)
			{
				Mutex::AutoLock(instance->m_ActiveTaskMutex);
				instance->m_ActiveTask = NULL;
				task->SetSuccess(false);
				task->m_Messages.push_back(VCMessage(kSevError, e.Message(), kMAPlugin));
				task->m_Messages.push_back(VCMessage(kSevCommand, "disableAllCommands", kMAPlugin));
				task->m_Messages.push_back(VCMessage(kSevCommand, "offline Version Control executable error", kMAPlugin));
				instance->EndPluginSession();
			}
			catch (VCPluginException e)
			{
				Mutex::AutoLock(instance->m_ActiveTaskMutex);
				instance->m_ActiveTask = NULL;
				task->SetSuccess(false);
				const VCMessages& msgs = e.GetMessages();
				if (msgs.empty()) 
					task->m_Messages.push_back(VCMessage(kSevError, "Caught Version Control Plugin error", kMAPlugin));
				else 
					task->m_Messages.insert(task->m_Messages.end(), msgs.begin(), msgs.end());
				task->m_Messages.push_back(VCMessage(kSevCommand, "disableAllCommands", kMAPlugin));
				task->m_Messages.push_back(VCMessage(kSevCommand, "offline Version Control plugin error", kMAPlugin));
				instance->EndPluginSession();
			}
			catch (...)
			{
				{
					Mutex::AutoLock(instance->m_ActiveTaskMutex);
					instance->m_ActiveTask = NULL;
				}
				instance->QueueCompletedTask(task);
				throw;
			}
			
			instance->QueueCompletedTask(task);
		}

		// This thread may have called Stop and killed itself
		if (instance->m_StopThread)
		{
			instance->m_StopThread = false;
			return NULL;
		}
	}

	return NULL;
}

void VCProvider::QueueTask(VCTask* task)
{
	Mutex::AutoLock myLock(m_ActiveTaskQueueMutex);
	m_TaskQueue.push(task);
	m_QueueSemaphore.Signal();
}

VCTask* VCProvider::DequeueTask()
{
	Mutex::AutoLock myLock(m_ActiveTaskQueueMutex);
	VCTask* task = NULL;
	if (m_TaskQueue.size() > 0)
	{
		task = m_TaskQueue.front();
		m_TaskQueue.pop();
	}
	return task;
}

// Called from worker thread
void VCProvider::QueueCompletedTask(VCTask* task)
{
	{
		Mutex::AutoLock myLock(m_CompletedTaskQueueMutex);
		m_CompletedTaskQueue.push(task);
	}

	// The task must be in the completed queue before signalling done. This
	// is because the main thread may be doing Wait() (which m_Done signals)
	// and will immediately after that go through the completed queue and call
	// Done() on the tasks. If this was not done this then the Done() calls would be 
	// executed in the next tick and code after a Wait() would not know about
	// the task result until after the tick which is not intuitive.
	task->m_Done.Signal();
}

VCTask* VCProvider::DequeueCompletedTask()
{
	Mutex::AutoLock myLock(m_CompletedTaskQueueMutex);
	VCTask* task = NULL;
	if (m_CompletedTaskQueue.size() > 0)
	{
		task = m_CompletedTaskQueue.front();
		m_CompletedTaskQueue.pop();
	}
	return task;
}

void VCProvider::Tick ()
{
	TickInternal();

	// Perform aggregated pending window update. We aggregate these updates because they
	// are expensive roundtrips to the vcs backend and may be triggered several times because
	// TickInternal is also called synchronously by VCTask::Wait().
	if (m_PendingWindowUpdateScheduled)
	{
		m_PendingWindowUpdateScheduled = false;
		ScriptingInvocation invoke ("UnityEditor.VersionControl", "WindowPending", "UpdateAllWindows");
		invoke.Invoke();
	}
}

void VCProvider::TickInternal ()
{
	// This ptr is not dereferenced at any point but only used
	// for comparing against another ptr. Keep it that way!
	static VCTask* lastTickTask = NULL;
	static int lastSecondsSpent = -1;
	static int lastProgressPct = -1;
	
	// Always allow completed tasks to get done even if the provider has been
	// shutdown.
	// if (!Enabled())
	// 	 return;

	VCTask* completedTask = DequeueCompletedTask();
	while(completedTask != NULL)
	{
		// Signal any callback about completion
		completedTask->Done();
		completedTask->Release();

		completedTask = DequeueCompletedTask();
	}
	
	m_StateCache->Tick();

	// Update the GUI on progress change
	VCTask* activeTask = GetActiveTask();

	bool taskChanged = lastTickTask != activeTask;
	bool progressChanged = activeTask != NULL &&
		(activeTask->m_ProgressPct != lastProgressPct || 
		 activeTask->m_SecondsSpent != lastSecondsSpent ||
		 activeTask->m_NextProgressMsg != 0);

	if (taskChanged || progressChanged)
	{
		// Now get the correct ptr just to be sure.
		lastTickTask = activeTask;
		if (lastTickTask != NULL)
		{
			lastSecondsSpent = activeTask->m_ProgressPct;
			lastProgressPct	= activeTask->m_SecondsSpent;
			int origValue = AtomicExchange(&(activeTask->m_NextProgressMsg), NULL);
			if (origValue)
			{
				char* msg = reinterpret_cast<char*>(origValue);
				string m(msg);
				activeTask->m_ProgressMsg = m;
				UNITY_FREE(kMemVersionControl, msg);
			}
		}
		else
		{
			lastSecondsSpent = -1;
			lastProgressPct	= -1;
		}
		// Repaint view to make it notice new task progress
		MonoException* exception = NULL;
		CallStaticMonoMethod("EditorApplication", "Internal_RepaintAllViews", NULL, &exception);
	}
}

// Ownership of the task is passed to the caller
VCTask* VCProvider::Status(VCAssetList const& list, bool recursively)
{
	if (!IsActive())
		return CreateErrorTask("Status");

	VCTask* task = UNITY_NEW(StatusTask, kMemVersionControl) (list, recursively);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

VCTask* VCProvider::ChangeSetDescription (const VCChangeSetID& id)
{
	if (!IsActive())
		return CreateErrorTask("ChangeSetDescription");

	VCTask* task = UNITY_NEW(GetChangeSetDescriptionTask, kMemVersionControl) (id);
	QueueTask(task);
	return task;
}

VCTask* VCProvider::ChangeSets ()
{
	if (!IsActive())
		return CreateErrorTask("ChangeSets");

	VCTask* task = UNITY_NEW(GetOutgoingTask, kMemVersionControl) ();
	QueueTask(task);
	return task;
}

VCTask* VCProvider::ChangeSetStatus (const VCChangeSetID& id)
{
	if (!IsActive())
		return CreateErrorTask("ChangeSetStatus");

	// Plugins that does not require checkout of asset before editing should
	// put assets changed in memory on the default changeset.
	// If the assets are written to disk already the plugin can detect changes
	// itself. If assets has not been written to disk they are marked dirty
	// in memory and we need to get that state into vccache but also returned
	// from this command in case the changeset ID is for the default changeset.

	VCAssetList dirtyAssets;
	if (m_ActivePlugin != NULL && !m_ActivePlugin->GetTraits().enablesCheckout)
	{
		// Build an asset list of the dirty asset paths
		set<UnityGUID> dirtyGuids;
		AssetInterface::Get().GetDirtyAssets(dirtyGuids);

		// Current scene is a speciel asset regarding dirtyness
		if (GetApplication().GetCurrentSceneGUID() != UnityGUID() &&
			GetApplication().IsSceneDirty())
			dirtyGuids.insert(GetApplication().GetCurrentSceneGUID());
		
		std::vector<UnityStr> paths;
		paths.reserve(dirtyGuids.size());
		GetGUIDPersistentManager().AssetPathNamesFromGUIDs(paths, dirtyGuids);

		dirtyAssets.reserve(paths.size());

		for (vector<UnityStr>::const_iterator i = paths.begin(); i != paths.end(); ++i)
		{
			VCAsset a(*i);
			a.SetState(kCheckedOutLocal);
			dirtyAssets.push_back(a);
		}
	}
	
	VCTask* task = UNITY_NEW(GetAssetsStatusInChangeSetTask, kMemVersionControl) (id, dirtyAssets);
	if (!dirtyAssets.empty())
		task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

VCTask* VCProvider::Incoming ()
{
	if (!IsActive())
		return CreateErrorTask("Incoming");

	VCTask* task = UNITY_NEW(GetIncomingTask, kMemVersionControl) ();
	QueueTask(task);
	return task;
}

VCTask* VCProvider::IncomingChangeSetAssets (const VCChangeSetID& id)
{
	if (!IsActive())
		return CreateErrorTask("IncomingChangeSetAssets");

	VCTask* task = UNITY_NEW(GetIncomingAssetsInChangeSetTask, kMemVersionControl) (id);
	QueueTask(task);
	return task;
}

bool VCProvider::CheckoutIsValid (VCAssetList const& assets)
{
	if (!IsActive())
		return false;

	// Checkout is recursive
	const bool includeFolders = true;
	const int incl = kSynced | kOutOfSync;
	const int excl = kCheckedOutLocal | kLockedLocal | kAddedLocal | kDeletedLocal;

	// First check if any of the immediate assets are checkoutable
	if  (assets.FilterCount(includeFolders, incl, excl) > 0)
		return true;

	// If any of the assets are folders then allow checkout no
	// matter what
	for (VCAssetList::const_iterator i = assets.begin(); i != assets.end(); i++)
	{
		if (i->IsFolder()) return true;
	}		
	return false;
}

// Ownership of the task is passed to the caller
VCTask* VCProvider::Checkout (VCAssetList const& assetList, CheckoutMode mode)
{
	if (!IsActive())
		return CreateErrorTask("Cannot check out files in offline mode");

	VCTask* task = UNITY_NEW(CheckoutTask, kMemVersionControl) (assetList, mode);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}


// If promptIfCheckoutNeeded is the empty string then checkout will be performed without a
// prompt shown.
bool VCProvider::PromptAndCheckoutIfNeeded(const VCAssetList& assetList, const string& promptIfCheckoutNeeded)
{
	// Make sure that all assets are checked out in version control and
	// that we have the most recent status
	if (!Enabled())
		return true;
	
	// Need to get the most recent state of the assets in order to determine
	// if checkout is possible.
	// @TODO: Correct this when VCCache and vcprovider has been merged.
	VCTask* task = Status(assetList, true);
	task->Retain();
	task->Wait();
	
	// Checkout assets that are in vc
	VCAssetList checkoutList;
	for (VCAssetList::const_iterator i = task->GetAssetList().begin(); i != task->GetAssetList().end(); ++i)
	{
		const VCAsset& asset = *i;
		bool isAlreadyEditable = asset.HasState(kCheckedOutLocal | kDeletedLocal |kAddedLocal | kLockedLocal);
		if (!isAlreadyEditable)
		{
			// We want do recursive checkouts in the situation where this asset is
			// a child of the an asset (ie folder asset) in the status request. In that case we just checkout
			// the folder asset instead. This will remove all subassets of a folder in the assetList and only
			// checkout the common folder.
			VCAssetList::const_iterator aiter = assetList.FindOldestAncestor(asset);
			checkoutList.push_back(aiter == assetList.end() ? asset : *aiter);
		}
	}
	task->Release();

	if (checkoutList.empty())
		return true; // nothing todo
	
	if (!promptIfCheckoutNeeded.empty())
	{
		if (!DisplayDialog("Perform checkout?", promptIfCheckoutNeeded, "Checkout", "Cancel"))
			return false;
	}
	
	task = Checkout(checkoutList, kBoth);
	task->SetCompletionAction(kUpdatePendingWindow);
	task->Retain();
	task->Wait();
	task->Release();
	
	if (!task->GetSuccess())
		return false;
	
	// TODO: Get state from checkout assetlist instead of requerying
	// Get new checkout state
	task = Status(checkoutList, true);
	task->Retain();
	task->Wait();
	task->Release();
	return true;
}

bool VCProvider::DeleteChangeSetsIsValid (const VCChangeSets& changesets)
{
	VCChangeSetIDs rs;
	ExtractChangeSetIDs(changesets, rs);
	return DeleteChangeSetsIsValid(rs);
}

bool VCProvider::DeleteChangeSetsIsValid (const VCChangeSetIDs& changes)
{
	if (!IsActive())
		return false;
	
	if (changes.empty()) return false;

/*	VCAssetList assets;
	VCMessages msgs;

	for (VCChangeSetIDs::const_iterator i = changes.begin(); i != changes.end(); ++i)
	{
		// @TODO: This should not call into the plugin if it can be avioded
		// @TODO: Handle error on ChangeStatus and abort
		bool success = VCInterface::ChangeStatus(msgs, assets, *i);
		if (success)
			return true;
		assets.clear();
	}
*/
	return true;
}

VCTask* VCProvider::DeleteChangeSets (VCChangeSets const& changesets)
{
	VCChangeSetIDs rs;
	ExtractChangeSetIDs(changesets, rs);
	return DeleteChangeSets(rs);
}

VCTask* VCProvider::DeleteChangeSets (const VCChangeSetIDs& changes)
{
	if (!IsActive())
		return CreateErrorTask("Cannot delete changesets in offline mode");
	
	VCTask* task = UNITY_NEW(DeleteChangeSetsTask, kMemVersionControl) (changes);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

bool VCProvider::RevertIsValid (VCAssetList const& assets, RevertMode mode)
{
	if (!IsActive())
		return false;

	const bool includeFolders = true;
	int incl = kCheckedOutLocal | kLockedLocal;
	if (mode != kRevertUnchanged)
		incl |= kDeletedLocal | kAddedLocal;

	return assets.FilterCount(includeFolders, incl) > 0;
}

VCTask* VCProvider::RevertChangeSets (VCChangeSets const& changesets, RevertMode mode)
{
	VCChangeSetIDs rs;
	ExtractChangeSetIDs(changesets, rs);
	return RevertChangeSets(rs, mode);
}

VCTask* VCProvider::RevertChangeSets (const VCChangeSetIDs& changes, RevertMode mode)
{
	if(!IsActive())
		return CreateErrorTask("Cannot revert files in offline mode");
	
	if (mode == kRevertKeepModifications)
		return CreateErrorTask("Cannot revert changeset with keeppModifications mode");
		
	if (mode == kRevertUnchanged)
		AssetInterface::Get().SaveAssets();

	GetApplication().DisallowAutoRefresh();
	VCTask* task = UNITY_NEW(RevertChangeSetsTask, kMemVersionControl) (changes, mode == kRevertUnchanged);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

VCTask* VCProvider::Revert (VCAssetList const& assets, RevertMode mode)
{
	if(!IsActive())
		return CreateErrorTask("Cannot revert files in offline mode");

	if (mode == kRevertUnchanged)
		AssetInterface::Get().SaveAssets();

	GetApplication().DisallowAutoRefresh();

	VCTask* task = UNITY_NEW(RevertTask, kMemVersionControl) (assets, mode);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}


bool VCProvider::SubmitIsValid (VCChangeSet* changeset, VCAssetList const& assets)
{
	if (!IsActive())
		return false;

	if (changeset)
		return true;

	const bool includeFolders = true;
	VCAssetList listCount;
	assets.Filter(listCount, includeFolders, kCheckedOutLocal | kLockedLocal | kDeletedLocal | kAddedLocal);

	return listCount.size() > 0;
}

void VCProvider::IncludeMissingAndUnknownAssets(const VCAssetList& assets, VCAssetList& result)
{
	// Make sure that all ancestor folder meta files are under version control.
	// The folders themselves will be auto included by the plugins if missing.
	VCAssetSet _assetsAndAssoc;
	assets.GetAncestors(_assetsAndAssoc);
	
	// Only VCAssetList has ReplaceWithMeta currently
	VCAssetList tmp(_assetsAndAssoc.begin(), _assetsAndAssoc.end());
	_assetsAndAssoc.clear();
	tmp.ReplaceWithMeta();
	_assetsAndAssoc.insert(tmp.begin(), tmp.end());
	tmp.clear();

	// Make sure that all meta files are under version control as well
	_assetsAndAssoc.insert(assets.begin(), assets.end());
	result.insert(result.end(), _assetsAndAssoc.begin(), _assetsAndAssoc.end());
	result.IncludeMeta();

	// Mark all asset that we don't know about
	for (VCAssetList::iterator i = result.begin(); i != result.end(); ++i)
	{
		if (i->GetState() & (kCheckedOutLocal | kLockedLocal | kAddedLocal | kDeletedLocal | kSynced | kOutOfSync | kConflicted))
			continue; // Must be in version control to have such states

		string path = i->GetPath();

		if ( path == "Assets/" )
			path = "Assets";

		bool isCurrentlyUnknownAsset = !GetVCCache().GetAssetByPath(path, *i, false);
		if (isCurrentlyUnknownAsset)
			i->SetState(kNone);
	}
}

VCTask* VCProvider::Submit (VCChangeSet* changeset, VCAssetList const& assets, std::string const& description, bool saveOnly)
{
	if (!IsActive())
		return CreateErrorTask("Cannot submit changesets in offline mode");

	VCAssetList submitAssets;
	IncludeMissingAndUnknownAssets(assets, submitAssets);

	VCTask* task = UNITY_NEW(SubmitTask, kMemVersionControl) (changeset, submitAssets, description, saveOnly, GetEditorUserSettings().GetVCAutomaticAdd());
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

bool VCProvider::AddIsValid (VCAssetList const& assets)
{
	if (!IsActive())
		return false;

	const bool includeFolders = true;
	return assets.FilterCount(includeFolders, kLocal, kAddedLocal | kOutOfSync | kSynced) > 0;
}

VCTask* VCProvider::Add (VCAssetList const& assetList, bool recursive)
{
	if (!IsActive())
		return CreateErrorTask("Cannot add files in offline mode");

	VCTask* task = UNITY_NEW(AddTask, kMemVersionControl) (assetList, recursive);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

VCTask* VCProvider::Delete (VCAssetList const& assetList)
{

	if(!IsActive())
		return CreateErrorTask("Cannot delete files in offline mode");

	VCTask* task = UNITY_NEW(DeleteTask, kMemVersionControl) (assetList);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

VCTask* VCProvider::Move (const VCAsset& src, const VCAsset& dst, bool noLocalFileMove)
{
	if (!IsActive())
		return CreateErrorTask("Cannot rename/move files in offline mode");

	if (src.IsMeta())
		return CreateErrorTask("Cannot rename/move meta files on their own");

	GetApplication().DisallowAutoRefresh();

	VCTask* task = UNITY_NEW(MoveTask, kMemVersionControl) (src, dst, noLocalFileMove);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

VCTask* VCProvider::ChangeSetMove (VCAssetList const& assetList, const VCChangeSetID& id)
{
	if (!IsActive())
		return CreateErrorTask("Cannot move changeset files in offline mode");

	VCTask* task = UNITY_NEW(MoveToChangeSetTask, kMemVersionControl) (assetList, id);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

bool VCProvider::GetLatestIsValid (VCAssetList const& assets)
{
	if (!IsActive())
		return false;

	const bool includeFolders = true;
	return assets.FilterCount(includeFolders, kOutOfSync) > 0;
}

VCTask* VCProvider::GetLatest (VCAssetList const& assets)
{
	if (!IsActive())
		return CreateErrorTask("Cannot get latest files in offline mode");

	GetApplication().DisallowAutoRefresh();
	VCTask* task = UNITY_NEW(GetLatestTask, kMemVersionControl) (assets);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

bool VCProvider::LockIsValid (VCAssetList const& assets)
{
	if (!IsActive())
		return false;

	const bool includeFolders = true;

	int incl = kSynced | kOutOfSync;
	int excl = kLockedLocal | kLockedRemote | kAddedLocal | kDeletedLocal;

	if (m_ActivePlugin->GetTraits().enablesCheckout)
		incl = kCheckedOutLocal;

	return assets.FilterCount(includeFolders, incl, excl) > 0;
}

bool VCProvider::UnlockIsValid (VCAssetList const& assets)
{
	if (!IsActive())
		return false;
	
	const bool includeFolders = true;
	const int incl = kLockedLocal;

	return assets.FilterCount(includeFolders, incl) > 0;
}

VCTask* VCProvider::Lock (VCAssetList const& assets, bool locked)
{

	if (!IsActive())
		return CreateErrorTask("Cannot lock/unlock files in offline mode");

	VCTask* task = UNITY_NEW(LockTask, kMemVersionControl) (assets, locked);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

bool VCProvider::DiffIsValid(const VCAssetList& assets)
{
	if (!IsActive())
		return false;
	
	const bool includeFolders = false;
	return assets.FilterCount(includeFolders, kSynced | kOutOfSync, kAddedLocal) > 0;
}

VCTask* VCProvider::DiffHead(const VCAssetList& assets, bool includingMetaFiles)
{
	if (!IsActive())
		return CreateErrorTask("Cannot diff files against head in offline mode");
		
	VCTask* task = UNITY_NEW(DiffTask, kMemVersionControl) (assets, includingMetaFiles, kDefaultChangeSetID);
	//task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

bool VCProvider::ResolveIsValid(const VCAssetList& assets)
{
	if (!IsActive())
		return false;
	
	VCAssetList filesRecursive;

	for (VCAssetList::const_iterator i = assets.begin(); i != assets.end(); ++i)
	{
		// Check each folders meta data as well as recursing their contents
		filesRecursive.AddRecursive(*i);
	}

	filesRecursive.IncludeMeta();

	// Cannot download a folder but only files so we strip folder assets.
	// Note that folder meta files are included never the less.
	const bool excludeFolders = false;
	VCAssetList filteredAssets;
	filesRecursive.Filter(filteredAssets, excludeFolders, kConflicted);

	return !filteredAssets.empty();
}

VCTask* VCProvider::Resolve(const VCAssetList& assets, ResolveMethod resolveMethod)
{
	if (!IsActive())
		return CreateErrorTask("Cannot diff files against head in offline mode");
	
	VCTask* task = UNITY_NEW(ResolveTask, kMemVersionControl) (assets, resolveMethod);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

VCTask* VCProvider::Merge(const VCAssetList& assets, MergeMethod method)
{
	if (!IsActive())
		return CreateErrorTask("Cannot diff files against head in offline mode");
	
	VCTask* task = UNITY_NEW(MergeTask, kMemVersionControl) (assets, method);
	//task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

bool VCProvider::IsOpenForEdit (VCAsset* asset)
{
	if (!IsActive())
		return true;

	// Test if asset is folder?
	if (m_ActivePlugin == NULL)
		return true;
	
	if (m_ActivePlugin->GetTraits().enablesCheckout)
	{
		bool vcsOpened = asset->GetState() & (kAddedLocal | kCheckedOutLocal | kLockedLocal | kDeletedLocal);
		bool nonVcsFile = !(asset->GetState() & (kSynced | kOutOfSync));
		return vcsOpened || nonVcsFile;
	}

	return true;
}

bool VCProvider::IsActive () const
{
	if (!Enabled()) return false;

	if (m_ActivePlugin == NULL)
		return false;

	if (m_ActivePlugin->GetTraits().requiresNetwork)
		return !GetEditorUserSettings().GetVCWorkOffline() && m_ActivePluginIsOnline == kOSOnline;

	return true;
}

VCTask* VCProvider::Refresh(VCAssetList const& assets, bool recursively)
{
	if (!IsActive())
		return CreateErrorTask("Cannot refresh revisioned files in offline mode");

	VCTask* task = UNITY_NEW(StatusTask, kMemVersionControl) (assets, recursively);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

int VCProvider::GenerateID ()
{
	return AtomicIncrement(&m_IDGenerator);
}

bool HasVCProvider ()
{
	return VCProvider::instance != NULL;
}

VCProvider& GetVCProvider ()
{
	if (VCProvider::instance == NULL)
	{
		VCProvider::instance = UNITY_NEW(VCProvider, kMemVersionControl)();

		// Initialize the worker thread
		VCProvider::instance->Initialize();
	}

	return *VCProvider::instance;
}

VCProvider* GetVCProviderPtr()
{
	return VCProvider::instance;
}

void CleanupVCProvider ()
{
	if (VCProvider::instance != NULL)
	{
		VCProvider::instance->Stop();
		UNITY_DELETE(VCProvider::instance, kMemVersionControl);
	}
}

VCTask* VCProvider::CreateErrorTask(string const& message)
{
	VCTask* task = UNITY_NEW(VCTask, kMemVersionControl) ();
	task->SetSuccess(false);
	QueueCompletedTask(task);
	return task;
}

VCTask* VCProvider::CreateSuccessTask ()
{
	VCTask* task = UNITY_NEW(VCTask, kMemVersionControl) ();
	task->SetSuccess(true);
	QueueCompletedTask(task);
	return task;
}

/*
int VCProvider::GetChangeSetID (VCChangeSet const* changeset)
{
	if (!IsActive())
		return 0;

	return 42;
}
*/

VCTask * VCProvider::UpdateSettings ()
{
	m_ActivePluginOfflineReason.clear();

	string pluginName = GetEditorSettings().GetExternalVersionControlSupport();
	if (pluginName == ExternalVersionControlVisibleMetaFiles ||
		pluginName == ExternalVersionControlAutoDetect ||
		pluginName == ExternalVersionControlHiddenMetaFiles ||
		pluginName == ExternalVersionControlAssetServer)
	{
		m_StateCache->Clear();
		return CreateSuccessTask();
	}
	
	// Clear online status when changing plugin type
	if (pluginName != m_ActivePluginName)
	{
		if (m_StateCache)
			m_StateCache->Clear();
		m_ActiveConfigFields.clear();
		m_ActiveTraits.Reset();
	}

	EditorUserSettings::ConfigValueMap m = GetEditorUserSettings().GetConfigValues();
	
	SetPluginAsUpdating();
	VCTask* task = UNITY_NEW(UpdateSettingsTask, kMemVersionControl) (pluginName, m);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

void VCProvider::EnableCommand(VCCommandName n)
{
	// If the status command is enabled we add a scene inspector to track the dirty state of objects in memory
	// This only used for plugins that does not require checkout
	if (n == kVCCStatus)
	{
		if (m_SceneInspector == NULL)
			m_SceneInspector = UNITY_NEW(VCSceneInspector, kMemVersionControl);

		GetSceneTracker().AddSceneInspector(m_SceneInspector);
	}

	m_DisabledCommands.erase(n);	
}

void VCProvider::DisableCommand(VCCommandName n)
{
	if (n == kVCCStatus && m_SceneInspector != NULL)
		GetSceneTracker().RemoveSceneInspector(m_SceneInspector);

	m_DisabledCommands.insert(n);
}

void VCProvider::DisableCustomCommand(const std::string& n)
{
	m_DisabledCustomCommands.insert(n);
}

void VCProvider::EnableCustomCommand(const std::string& n)
{
	m_DisabledCustomCommands.erase(n);
}

bool VCProvider::IsCustomCommandEnabled( const std::string& n ) const
{
	return m_DisabledCustomCommands.find(n) == m_DisabledCustomCommands.end();
}

bool VCProvider::HasCustomCommand( const std::string& n ) const
{
	for (int i = 0; i < m_ActiveCustomCommands.size(); ++i)
	{
		if (m_ActiveCustomCommands[i].name == n) return true;
	}
	return false;
}

VCTask* VCProvider::StartCustomCommandTask( const std::string& commandName)
{
	if (!IsActive())
		return CreateErrorTask("Cannot call custom commands in offline mode");

	VCCustomCommand command;
	for (int i = 0; i < m_ActiveCustomCommands.size(); ++i)
	{
		if (m_ActiveCustomCommands[i].name == commandName)
		{
			command = m_ActiveCustomCommands[i];
			break;
		}
	}

	if (command.name != commandName)
		return CreateErrorTask(Format("Cannot call unknown custom command '%s'", commandName.c_str()));

	VCTask* task = UNITY_NEW(CustomCommandTask, kMemVersionControl) (command);
	task->EnableRepaintOnDone();
	QueueTask(task);
	return task;
}

void VCProvider::SetPluginAsOnline()
{
	OnlineState old = m_ActivePluginIsOnline;
	m_ActivePluginIsOnline = kOSOnline;
	m_ActivePluginOfflineReason.clear();

	if (old != kOSOnline)
	{
		// Update pending window make it list incoming/outgoing changelists
		SchedulePendingWindowUpdate();
	}
}

void VCProvider::SetPluginAsUpdating()
{
	m_ActivePluginIsOnline = kOSUpdating;
	m_ActivePluginOfflineReason.clear();
}

void VCProvider::SetPluginAsOffline(const std::string& reason)
{
	m_ActivePluginIsOnline = kOSOffline;
	m_ActivePluginOfflineReason = reason;
}


// In worker thread
void VCProvider::SendSettingsToPlugin(VCMessages& msgs)
{
	assert(m_PluginSession);

	VCPluginSession* p = m_PluginSession;

	p->SendCommand("pluginConfig", "projectPath", File::GetCurrentDirectory());
	ReadPluginStatus();
	const VCMessages& ms = p->GetMessages();
	msgs.insert(msgs.end(), ms.begin(), ms.end());
		
	string prefix = "vc";
	string prefixPlugin = prefix + m_ActivePluginName;
	string prefixShared = prefix + "Shared";
	
	for (EditorUserSettings::ConfigValueMap::const_iterator i = m_Settings.begin();
		i != m_Settings.end(); ++i)
	{
		const string k = i->first;
		const string v = i->second;
		
		bool pluginSpecificConfig = k.substr(0,prefixPlugin.length()) == prefixPlugin;
		bool sharedConfig = k.substr(0,prefixShared.length()) == prefixShared;
		if (! (pluginSpecificConfig || sharedConfig) )
			continue;

		p->SendCommand("pluginConfig", k, v);
		ReadPluginStatus();
		const VCMessages& m = p->GetMessages();
		msgs.insert(msgs.end(), m.begin(), m.end());
	}

	p->SendCommand("pluginConfig", "end");
	ReadPluginStatus();
	const VCMessages& m = p->GetMessages();
	msgs.insert(msgs.end(), m.begin(), m.end());
}

void VCProvider::ClearPluginMessages()
{
	VCProvider& provider = GetVCProvider();
	if (provider.m_PluginSession != NULL)
		provider.m_PluginSession->ClearMessages();
}

// In worker thread
VCPluginSession& VCProvider::EnsurePluginIsRunning(VCMessages& msgs)
{
	VCProvider& provider = GetVCProvider();

	if (provider.m_PluginSession != NULL && !provider.m_PluginSession->IsRunning())
	{
		// The process must have been shutdown or crashed. Get ready to restart.
		msgs.push_back(VCMessage(kSevWarning, "Version control plugin disappeared, Restarting.", kMAPlugin));
		provider.EndPluginSession();
	}

	if (provider.m_PluginSession != NULL)
		return *provider.m_PluginSession; // Session is running. All is fine.

	
	m_ActivePlugin = &VCPlugin::CreatePlugin (provider.m_ActivePluginName);
		
	// We own the session
	provider.m_PluginSession = m_ActivePlugin->Launch();
	
	provider.SendSettingsToPlugin(msgs);

	m_ActiveConfigFields = m_ActivePlugin->GetConfigFields();
	m_ActiveTraits = m_ActivePlugin->GetTraits();
	m_ActiveCustomCommands = m_ActivePlugin->GetCustomCommands();
	return *provider.m_PluginSession;
}

void VCProvider::EndPluginSession()
{
	UNITY_DELETE(m_PluginSession, kMemVersionControl);
	m_PluginSession = NULL;
	
	UNITY_DELETE(m_ActivePlugin, kMemVersionControl);
	m_ActivePlugin = NULL;
}

// In worker thread
bool VCProvider::ReadPluginStatus()
{
	// The readline will fetch any remaining errors
	VCPluginSession& p = *instance->m_PluginSession;
	p.SkipLinesUntilEndResponse();

	// For now just write errors, warnings and connect severity to console
	return !ContainsErrors(p.GetMessages(true)); 
}

void VCProvider::ResetPlugin()
{
	EndPluginSession();
	m_Settings.clear();
	m_ActivePluginName = "";
	m_ActiveConfigFields.clear();
	m_ActiveTraits.Reset();
	m_ActiveCustomCommands.clear();
	m_ActivePluginIsOnline = kOSOffline;
	m_ActivePluginOfflineReason.clear();
	m_DisabledCommands.clear();
}

VCTask* VCProvider::SetFileMode( const VCAssetList& assets, FileMode mode )
{
	if (!IsActive())
		return CreateErrorTask("Cannot set mode of files in offline mode");

	VCTask* task = UNITY_NEW(FileModeTask, kMemVersionControl) (assets, mode);
	QueueTask(task);
	return task;
}

static const char* StateToBuiltinOverlayPath(int state)
{
	struct item_t { int state; const char* path; };
	
	static item_t items[] = {
		{ kLocal,				"Icons/P4_Local.png" },
		{ kSynced,				NULL },
		{ kOutOfSync,			"Icons/P4_OutOfSync.png" },
		{ kMissing,				NULL },
		{ kCheckedOutLocal,		"Icons/P4_CheckOutLocal.png" },
		{ kCheckedOutRemote,	"Icons/P4_CheckOutRemote.png" },
		{ kDeletedLocal,		"Icons/P4_DeletedLocal.png" },
		{ kDeletedRemote,		"Icons/P4_DeletedRemote.png" },
		{ kAddedLocal,			"Icons/P4_AddedLocal.png" },
		{ kAddedRemote,			"Icons/P4_AddedRemote.png" },
		{ kConflicted,			"Icons/P4_Conflicted.png" },
		{ kLockedLocal,			"Icons/P4_LockedLocal.png" },
		{ kLockedRemote,		"Icons/P4_LockedRemote.png" }
	};
	
	static const size_t itemCount = sizeof(items) / sizeof(items[0]);
	
	for (size_t i = 0; i < itemCount; ++i)
	{
		if (items[i].state == state)
			return items[i].path;
	}
	return NULL;
}

void VCProvider::LoadOverlayIcons()
{
	
	if (m_ActivePlugin == NULL)
	{
		WarningStringMsg("Cannot load %s overlays when plugin is not running\n", m_ActivePluginName.c_str());
		return;
	}
	const VCPlugin::OverlayIconStateMap& overlays = m_ActivePlugin->GetOverlayIconStateMap();

	m_AtlasStateRects.clear();
	dynamic_array<Texture2D*> textures;
	dynamic_array<int> states;
	set<Texture2D*> editorBundleTextures;
	
	AssetBundle* editorAssetBundle = GetEditorAssetBundle();
	
	const char* kBlankOverlayTag = "blank";
	const char* kDefaultOverlayTag = "default";
	
	for (VCPlugin::OverlayIconStateMap::const_iterator i = overlays.begin(); i != overlays.end(); ++i)
	{
		int state = i->first;
		const string& path = i->second;
		
		if (path.empty() || path == kBlankOverlayTag)
		{
			// No overlay for this state -> No texture.
		}
		else if (path == kDefaultOverlayTag)
		{
			// Use builtin default overlay icons ie. perforce style
			string p = StateToBuiltinOverlayPath(state);

			Texture2D* tex = NULL;

			tex = editorAssetBundle->Get<Texture2D>(p);

			if (!tex)
			{
				WarningStringMsg("Invalid overlay icon for %s at default path %s\n",  m_ActivePluginName.c_str(), p.c_str());
			}
			else
			{
				textures.push_back(tex);
				states.push_back(state);
				editorBundleTextures.insert(tex);
			}
		}
		else if (IsFileCreated(path))
		{
			
			Texture2D* tex = CreateObjectFromCode<Texture2D>(kInstantiateOrCreateFromCodeAwakeFromLoad, kMemTextureCache);
			tex->SetHideFlags(Object::kHideAndDontSave);
			tex->InitTexture (16, 16, kTexFormatRGBA32, Texture2D::kNoMipmap, 1);

			const bool markNonReadable = false;
			int bytes = GetFileLength(path);
			dynamic_array<UInt8> data(bytes, kMemVersionControl); 
			if (ReadFromFile(path, data.data(), 0, bytes))
			{
				if (LoadMemoryBufferIntoTexture( *tex, data.data(), bytes, kLoadImageUncompressed, markNonReadable))
				{
					textures.push_back(tex);
					states.push_back(state);
				}
				else
				{
					WarningStringMsg("Could not load overlay icon for %s at path %s\n", m_ActivePluginName.c_str(), path.c_str());
				}
			}
			else
			{
				WarningStringMsg("Could not read overlay icon for %s at path %s\n", m_ActivePluginName.c_str(), path.c_str());
			}
		}
		else
		{
			WarningStringMsg("No overlay icon for %s at path %s\n", m_ActivePluginName.c_str(), path.c_str());
		}
	}
	
	size_t overlayCount = textures.size();
	dynamic_array<Rectf> outRects(overlayCount, kMemVersionControl);

	if (m_OverlayAtlas != NULL)
		DestroySingleObject(m_OverlayAtlas);

	m_OverlayAtlas = CreateObjectFromCode<Texture2D>(kInstantiateOrCreateFromCodeAwakeFromLoad, kMemTextureCache);
	m_OverlayAtlas->SetHideFlags(Object::kHideAndDontSave);
	// m_OverlayAtlas->InitTexture (16, 16, kTexFormatRGBA32, Texture2D::kNoMipmap, 1);
	
	const int padding = 0;
	const bool upload = true;
	const bool markNoLongerReadable = false;
	bool ok  = PackTextureAtlasSimple(m_OverlayAtlas, 512, overlayCount, textures.data(),
									  outRects.data(), padding, upload, markNoLongerReadable);

	for (int i = 0; i < textures.size(); ++i)
	{
		if (editorBundleTextures.count(textures[i]) == 0)
			DestroySingleObject(textures[i]);
	}

	if (!ok)
	{
		WarningStringMsg("Could not prepare overlays for %s\n", m_ActivePluginName.c_str());
	}
	else
	{
		for (size_t i = 0; i < overlayCount; ++i)
			m_AtlasStateRects[static_cast<States>(states[i])] = outRects[i];
	}
}

Texture2D* VCProvider::GetOverlayAtlas()
{
	return m_OverlayAtlas;
}

Rectf VCProvider::GetAtlasRectForState(States state)
{
	AtlasStateRects::const_iterator i = m_AtlasStateRects.find(state);
	if (i == m_AtlasStateRects.end())
		return Rectf();
	return i->second;
}	

ScriptingArray* VCProvider::GetMonoCustomCommands () const
{
	ScriptingClass* elementType = GetMonoManager ().GetMonoClass ("CustomCommand", "UnityEditor.VersionControl");
	ScriptingArray* array = mono_array_new(mono_domain_get(), elementType, m_ActiveCustomCommands.size());
	for (int i = 0; i < m_ActiveCustomCommands.size(); ++i)
	{
		// Create mono VCMCustomCommand and hook up with our cpp version
		// We create the mono object but we do not call the constructor
		MonoObject* monoMsg = scripting_object_new(GetScriptingTypeRegistry().GetType("UnityEditor.VersionControl", "CustomCommand"));
		ExtractMonoObjectData<VCCustomCommand*>(monoMsg) = UNITY_NEW(VCCustomCommand, kMemVersionControl) (m_ActiveCustomCommands[i]);
		Scripting::SetScriptingArrayElement(array, i, monoMsg);
	}
	return array;
}

void VCProvider::SchedulePendingWindowUpdate()
{
	m_PendingWindowUpdateScheduled = true;
}
