#include "UnityPrefix.h"
#include "UpdateSettingsTask.h"
#include "Editor/Src/MenuController.h"

#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Utilities/FileUtilities.h"


// Allows for dynamic menues for vcs plugins since they
// support different subsets of the entire feature set
class VCSAssetMenuHandler : public MenuInterface
{
	
	virtual bool Validate (const MenuItem &menuItem)
	{
		ScriptingObject* pass = scripting_object_new(GetScriptingTypeRegistry().GetType("UnityEditor", "MenuCommand"));
		ScriptingInvocation invocation("UnityEditorInternal.VersionControl", "ProjectContextMenu", (menuItem.m_Command + "Test").c_str());
		invocation.AddObject(pass);
		invocation.AdjustArgumentsToMatchMethod();
		return MonoObjectToBool(invocation.InvokeChecked());
	}
	
	virtual void Execute (const MenuItem &menuItem)
	{
		ScriptingObject* pass = scripting_object_new(GetScriptingTypeRegistry().GetType("UnityEditor", "MenuCommand"));
		ScriptingInvocation invocation("UnityEditorInternal.VersionControl", "ProjectContextMenu", menuItem.m_Command.c_str());
		invocation.AddObject(pass);
		invocation.AdjustArgumentsToMatchMethod();
		invocation.InvokeChecked();
	}
	
	static VCSAssetMenuHandler * s_Singleton;
	
public:
	static void SetupMenu(const VCPlugin::Traits* traits)
	{
		ASSERT_RUNNING_ON_MAIN_THREAD
		
		if (!s_Singleton)
			s_Singleton = new VCSAssetMenuHandler;
		else
			MenuController::RemoveMenuItem("Assets/Version Control");
		
		static const char* menuItems[] = {
			"Assets/Version Control/Get Latest", "GetLatest",
			"Assets/Version Control/Submit...", "Submit",
			"Assets/Version Control/Check Out", "CheckOut",
			"Assets/Version Control/Mark Add", "MarkAdd",
			"Assets/Version Control/Revert...", "Revert",
			"Assets/Version Control/Revert Unchanged", "RevertUnchanged",
			"Assets/Version Control/Resolve Conflicts...", "Resolve",
			"Assets/Version Control/Lock", "Lock",
			"Assets/Version Control/Unlock", "Unlock",
			"Assets/Version Control/Diff/Against Head...", "DiffHead",
			"Assets/Version Control/Diff/Against Head with .meta...", "DiffHeadWithMeta",
		};
		
		static const size_t menuItemsCount = (sizeof(menuItems)/sizeof(menuItems[0])) / 2;
		
		if (traits)
		{
			for (int i = 0; i < menuItemsCount; i++)
			{
				if (i == 0 && !traits->enablesGetLatestOnChangeSetSubset) continue;
				if (i == 2 && !traits->enablesCheckout) continue;
				if (i == 5 && !traits->enablesRevertUnchanged) continue;
				if (i == 6 && traits->enablesConflictHandlingByPlugin) continue;
				if ((i == 7 || i == 8) && !traits->enablesLocking) continue;
				MenuController::AddMenuItem(menuItems[i*2], menuItems[i*2+1], s_Singleton);
			}
		}
	}
};

VCSAssetMenuHandler* VCSAssetMenuHandler::s_Singleton = NULL;


// Allows for dynamic menues for vcs plugins since they
// support different subsets of the entire feature set
class VCSContextPendingWindowAssetMenuHandler : public MenuInterface
{
	
	virtual bool Validate (const MenuItem &menuItem)
	{
		ScriptingInvocation invocation("UnityEditorInternal.VersionControl", "PendingWindowContextMenu", (menuItem.m_Command + "Test").c_str());
		invocation.AddInt(menuItem.contextUserData);
		invocation.AdjustArgumentsToMatchMethod();
		return MonoObjectToBool(invocation.InvokeChecked());
	}
	
	virtual void Execute (const MenuItem &menuItem)
	{
		ScriptingInvocation invocation("UnityEditorInternal.VersionControl", "PendingWindowContextMenu", menuItem.m_Command.c_str());
		invocation.AddInt(menuItem.contextUserData);
		invocation.AdjustArgumentsToMatchMethod();
		invocation.InvokeChecked();
	}
	
	static VCSContextPendingWindowAssetMenuHandler * s_Singleton;
	
public:
	static void SetupMenu(const VCPlugin::Traits* traits)
	{
		ASSERT_RUNNING_ON_MAIN_THREAD
		
		if (!s_Singleton)
			s_Singleton = new VCSContextPendingWindowAssetMenuHandler;
		else
			MenuController::RemoveMenuItem("CONTEXT/Pending");
		
		static const char* menuItems[] = {
			"CONTEXT/Pending/Submit...", "Submit",
			"CONTEXT/Pending/Revert...", "Revert",
			"CONTEXT/Pending/Revert Unchanged", "RevertUnchanged",
			"CONTEXT/Pending/Resolve Conflicts...", "Resolve",
			"CONTEXT/Pending/Lock", "Lock",
			"CONTEXT/PendingUnlock", "Unlock",
			"CONTEXT/Pending/Diff/Against Head...", "DiffHead",
			"CONTEXT/Pending/Diff/Against Head with .meta...", "DiffHeadWithMeta",
#if UNITY_WIN
			"CONTEXT/Pending/Show in Explorer", "ShowInExplorer",
#elif UNITY_OSX
			"CONTEXT/Pending/Reveal in Finder", "ShowInExplorer",
#elif UNITY_LINUX
			"CONTEXT/Pending/Open Containing Folder", "ShowInExplorer",
#else
#error "Unknown platform"
#endif
			"CONTEXT/Pending/New Changeset...", "NewChangeSet",
		};
		
		static const size_t menuItemsCount = (sizeof(menuItems)/sizeof(menuItems[0])) / 2;
		
		if (traits)
		{
			for (int i = 0; i < menuItemsCount; i++)
			{
				if (i == 2 && !traits->enablesRevertUnchanged) continue;
				if (i == 3 && traits->enablesConflictHandlingByPlugin) continue;
				if ((i == 4 || i == 5) && !traits->enablesLocking) continue;
				if (i == 9 && !traits->enablesChangelists) continue;
				MenuController::AddMenuItem(menuItems[i*2], menuItems[i*2+1], s_Singleton);
			}
		}
	}
};

VCSContextPendingWindowAssetMenuHandler* VCSContextPendingWindowAssetMenuHandler::s_Singleton = NULL;

// Allows for dynamic menues for vcs plugins since they
// support different subsets of the entire feature set
class VCSContextPendingWindowChangeSetMenuHandler : public MenuInterface
{
	
	virtual bool Validate (const MenuItem &menuItem)
	{
		ScriptingInvocation invocation("UnityEditorInternal.VersionControl", "ChangeSetContextMenu", (menuItem.m_Command + "Test").
									   c_str());
		invocation.AddInt(menuItem.contextUserData);
		invocation.AdjustArgumentsToMatchMethod();
		return MonoObjectToBool(invocation.InvokeChecked());
	}
	
	virtual void Execute (const MenuItem &menuItem)
	{
		ScriptingInvocation invocation("UnityEditorInternal.VersionControl", "ChangeSetContextMenu", menuItem.m_Command.c_str());
		invocation.AddInt(menuItem.contextUserData);
		invocation.AdjustArgumentsToMatchMethod();
		invocation.InvokeChecked();
	}

	static VCSContextPendingWindowChangeSetMenuHandler * s_Singleton;
	
public:
	static void SetupMenu(const VCPlugin::Traits* traits)
	{
		ASSERT_RUNNING_ON_MAIN_THREAD
		
		if (!s_Singleton)
			s_Singleton = new VCSContextPendingWindowChangeSetMenuHandler;
		else
			MenuController::RemoveMenuItem("CONTEXT/Change");
		
		static const char* menuItems[] = {
			"CONTEXT/Change/Submit...", "Submit",
			"CONTEXT/Change/Revert...", "Revert",
			"CONTEXT/Change/Revert Unchanged", "RevertUnchanged",
			"CONTEXT/Change/Resolve Conflicts...", "Resolve",
			"CONTEXT/Change/New Changeset...", "NewChangeSet",
			"CONTEXT/Change/Edit Changeset...", "EditChangeSet",
			"CONTEXT/Change/Delete Empty Changeset", "DeleteChangeSet",
		};
		
		static const size_t menuItemsCount = (sizeof(menuItems)/sizeof(menuItems[0])) / 2;
		
		if (traits)
		{
			for (int i = 0; i < menuItemsCount; i++)
			{
				if (i == 2 && !traits->enablesRevertUnchanged) continue;
				if (i == 3 && traits->enablesConflictHandlingByPlugin) continue;
				if ((i >= 4 && i <= 6) && !traits->enablesChangelists) continue;
				MenuController::AddMenuItem(menuItems[i*2], menuItems[i*2+1], s_Singleton);
			}
		}
	}
};

VCSContextPendingWindowChangeSetMenuHandler* VCSContextPendingWindowChangeSetMenuHandler::s_Singleton = NULL;



UpdateSettingsTask::UpdateSettingsTask(const string& pluginName, const EditorUserSettings::ConfigValueMap& m) : m_PluginName(pluginName), m_Settings(m)
{
}

void UpdateSettingsTask::Execute()
{
	VCProvider& provider = GetVCProvider();
	provider.ClearPluginMessages();
	VCPluginSession* p = provider.GetPluginSession();

	m_Success = true;

	bool isRunning = p && p->IsRunning();
	bool restartPlugin = m_PluginName != provider.GetActivePluginName() && isRunning;

	provider.SetActivePluginName(m_PluginName);
	provider.SetPluginSettings(m_Settings);

	if (restartPlugin)
	{
		// We know the process is running at this point
		provider.EndPluginSession();
		p = provider.GetPluginSession();
		isRunning =  p && p->IsRunning(); // should be false
		if (isRunning)
		{
			m_Messages.push_back(VCMessage(kSevError, "Not able to stop version control plugin when trying to restart it.", kMAPlugin));
			m_Success = false;
			return;
		}
	}
		
	if (!isRunning)
	{
		// The following call will call SendSettingsToPlugin
		provider.EnsurePluginIsRunning(m_Messages);
		p = provider.GetPluginSession();
		isRunning =  p && p->IsRunning(); // should be true
		if (!isRunning)
		{
			m_Messages.push_back(VCMessage(kSevError, "Not able to start version control plugin when updating plugin settings.", kMAPlugin));
			m_Success = false;
		}
		else
		{
			// Setting has been send by EnsurePluginRunning
			if (!HasOfflineState(m_Messages))
				IntegrityFix();
		}
		return;
	}

	provider.SendSettingsToPlugin(m_Messages);

	p = provider.GetPluginSession();
	isRunning =  p && p->IsRunning(); // should be true
	if (!isRunning)
	{
		m_Messages.push_back(VCMessage(kSevError, "Not able to send version control plugin updated settings.", kMAPlugin));
		m_Success = false;
	}
	
	m_Messages = provider.GetPluginSession()->GetMessages();
	if (!HasOfflineState(m_Messages))
		IntegrityFix();
}

// The default changeset should always included non-versioned files
// in the ProjectSettings folder.
void UpdateSettingsTask::IntegrityFix()
{
	VCProvider& provider = GetVCProvider();
	VCPluginSession* p = provider.GetPluginSession();
	
	// Check unversioned files in projectsettings/
	// Cannot just use recursive status since e.g. p4 doesn't return status
	// for local only files.
	p->SendCommand("status");
	VCAssetList assets;
	
	set<string> paths;
	GetDeepFolderContentsAtPath("ProjectSettings/", paths);
	for (set<string>::const_iterator i = paths.begin(); i != paths.end(); ++i)
		assets.push_back(VCAsset(*i));

	*p << assets;
	assets.clear();
	*p >> assets;
	
	GetVCProvider().ReadPluginStatus();
	const VCMessages& sm = p->GetMessages();
	m_Messages.insert(m_Messages.end(), sm.begin(), sm.end());

	bool isOnline = provider.GetOnlineState() == kOSOnline;

	// Check message for online state change
	for (VCMessages::const_iterator i = m_Messages.begin(); i != m_Messages.end(); ++i)
	{
		if (i->severity != kSevCommand)
		{
			continue;
		}
		else if (BeginsWith(i->message, "online"))
		{
			isOnline = true;
		}
		else if (BeginsWith(i->message, "offline"))
		{
			isOnline = false;
		}
	}

	if (!isOnline)
		return;
		
	VCAssetList addList;
	
	for (VCAssetList::const_iterator i = assets.begin(); i != assets.end(); ++i)
	{
		bool unresolved = i->GetState() & kConflicted;
		bool localFile = i->GetState() & kLocal;
		bool deletedHead = i->GetState() & kDeletedRemote;
		bool addedLocal = i->GetState() & kAddedLocal;
		bool checkedOut = i->GetState() & kCheckedOutLocal;
		
		if ( (localFile || deletedHead) && !addedLocal && !unresolved && !checkedOut)
		{
			// Unversioned asset in ProjectSettings/
			// Force it added
			addList.push_back(*i);
		}
	}
	
	assets.clear();
	
	if (!addList.empty())
	{
		p->SendCommand("add");
		*p << addList;
		*p >> assets;
		GetVCProvider().ReadPluginStatus();
		const VCMessages& am = p->GetMessages();
		m_Messages.insert(m_Messages.end(), am.begin(), am.end());
	}
}


void UpdateSettingsTask::Done()
{
	VCTask::Done(false);
				
	// Setup menu entries according to what the plugin supports
	if (GetVCProvider().GetActivePlugin() != NULL)
	{
		const VCPlugin::Traits& traits = GetVCProvider().GetActivePlugin()->GetTraits();
		VCSAssetMenuHandler::SetupMenu(&traits);
		VCSContextPendingWindowAssetMenuHandler::SetupMenu(&traits);
		VCSContextPendingWindowChangeSetMenuHandler::SetupMenu(&traits);
	}
	else
	{
		VCSAssetMenuHandler::SetupMenu(NULL);
		VCSContextPendingWindowAssetMenuHandler::SetupMenu(NULL);
		VCSContextPendingWindowChangeSetMenuHandler::SetupMenu(NULL);
	}
	
	GetVCProvider().LoadOverlayIcons();
}
