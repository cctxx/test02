#include "UnityPrefix.h"
#include "VCTask.h"
#include "Editor/Src/VersionControl/VCCache.h"
#include "Editor/Src/AssetPipeline/MetaFileUtility.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Threads/AtomicOps.h"

// This function is run in main thread
VCTask::VCTask ()
: m_ResultCode(0)
, m_Success(false)
, m_RepaintOnDone(false)
, m_DoneCallback(NULL)
, m_CompletionAction (kNoAction)
, m_SecondsSpent(-1)
, m_ProgressPct(-1)
, m_NextProgressMsg(NULL)
{
}

// This function is run in main thread
VCTask::~VCTask ()
{
}

// This function is run in main thread
void VCTask::Wait()
{
	m_Done.WaitForSignal();

	// Force handling of vcprovider completed tasks queue
	// in order for the task to have its Done() method called before
	// returning.
	GetVCProvider().TickInternal();
}

// This function is run in main thread
void VCTask::Done ()
{
	Done(true); // per default the this.m_Assetlist contains updated state of assets
}

const string& VCTask::GetDescription() const
{
	static string desc = "";
	return desc;
}

static void ProcessCommandMessages(const VCMessages& msgs)
{
	for (VCMessages::const_iterator i = msgs.begin(); i != msgs.end(); ++i)
	{
		if (i->severity != kSevCommand)
			continue;

		if (BeginsWith(i->message, "enableAllCommands"))
		{
			for (int i = kVCCAdd; i < KVCCInvalidTail; i++)
				GetVCProvider().EnableCommand(static_cast<VCCommandName>(i));
		}
		else if (BeginsWith(i->message, "disableAllCommands"))
		{
			for (int i = kVCCAdd; i < KVCCInvalidTail; i++)
				GetVCProvider().DisableCommand(static_cast<VCCommandName>(i));
		}	
		else if (BeginsWith(i->message, "enableCommand"))
		{
			// The plugin asks us to disable certain commands. This is an idempotent command.
			string enabledCommand = i->message.substr(14);
			VCCommandName cn = StringToVCCommandName(enabledCommand);
			if (cn == kVCCInvalid)
			{
				if (GetVCProvider().HasCustomCommand(enabledCommand))
					GetVCProvider().EnableCustomCommand(enabledCommand);
				else
					WarningString(string("Version control plugin asked to enable unknown command '") + enabledCommand + "'"); 
			}
			else
			{
				GetVCProvider().EnableCommand(cn);
			}
		}
		else if (BeginsWith(i->message, "disableCommand"))
		{
			// The plugin asks us to disable certain commands. This is an idempotent command.
			string disabledCommand = i->message.substr(15);
			VCCommandName cn = StringToVCCommandName(disabledCommand);
			if (cn == kVCCInvalid)
			{
				if (GetVCProvider().HasCustomCommand(disabledCommand))
					GetVCProvider().DisableCustomCommand(disabledCommand);
				else
					WarningString(string("Version control plugin asked to disable unknown command '") + disabledCommand + "'"); 
			}
			else
			{
				GetVCProvider().DisableCommand(cn);
			}
		}
		else if (BeginsWith(i->message, "online"))
		{
			// The plugin got online. This is an idempotent command
			GetVCProvider().SetPluginAsOnline();
		}
		else if (BeginsWith(i->message, "offline"))
		{
			// The plugin got offline. This is an idempotent command
			string reason = i->message.length() > 8 ? i->message.substr(8) : "No reason specified";
			GetVCProvider().SetPluginAsOffline(reason);
		}
		else
		{
			WarningString(string("Version Control plugin send an unknown command '") + i->message + "'");
		}
	}
}

bool VCTask::HasOfflineState(const VCMessages& msgs)
{
	bool gotOfflineState = false;
	for (VCMessages::const_iterator i = msgs.begin(); i != msgs.end(); ++i)
	{
		if (i->severity != kSevCommand)
		{
			continue;
		}
		else if (BeginsWith(i->message, "online"))
		{
			gotOfflineState = false;
		}
		else if (BeginsWith(i->message, "offline"))
		{
			gotOfflineState = true;
		}
	}
	return gotOfflineState;
}

void VCTask::Done (bool assetsShouldUpdateDatabase)
{	
	// Ugly smugly
	if (m_ResultCode == 0 && GetDescription() != "moving")
		m_ResultCode = m_Success ? 1 : 2; // Set because of VCWindowChange progress tracking. TODO: make nicer
	
	// Process any command messages from the plugin
	ProcessCommandMessages(m_Messages);
	
	LogMessages(m_Messages);

	// If the assets result of this task contains the new state of the 
	// local assets then update the database
	if (assetsShouldUpdateDatabase)
		VCCache::StatusCallback(this);

	if (m_NextProgressMsg != 0)
	{
		UNITY_FREE(kMemVersionControl, (void*)m_NextProgressMsg);
		m_NextProgressMsg = NULL;
		m_ProgressMsg = "Done";
	}

	if (m_DoneCallback != NULL)
		(*m_DoneCallback)(this);

	if (m_CompletionAction != kNoAction)
	{
		if (m_CompletionAction == kUpdatePendingWindow)
		{
			GetVCProvider().SchedulePendingWindowUpdate();
		}
		else
		{
			ScriptingInvocation  invoke ("UnityEditor.VersionControl", "WindowPending", "OnVCTaskCompletedEvent");
			invoke.AddObject (CreateManagedTask (this));
			invoke.AddEnum (m_CompletionAction);
			invoke.Invoke ();
		}
	}
	
	if (m_RepaintOnDone)
	{
		MonoException* exception = NULL;
		CallStaticMonoMethod("EditorApplication", "Internal_RepaintAllViews", NULL, &exception);
	}
}

void VCTask::CreateFoldersFromMetaFiles()
{
	// Make sure that folder meta files have their associated folders created if needed.
	// E.g. perforce might need that.
	InputString str;
	for (VCAssetList::const_iterator i = m_Assetlist.begin(); i != m_Assetlist.end(); ++i)
	{
		if (!i->IsMeta())
			continue;
		
		string dirPath = i->GetAssetPath(); 
		if (IsPathCreated(dirPath))
			continue; 

		if (!IsTextMetaDataForFolder(dirPath))
			continue;
	
		CreateDirectory(dirPath);
	}
}

ScriptingArray* VCTask::GetMonoAssetList () const
{
	ScriptingClass* elementType = GetMonoManager ().GetMonoClass ("Asset", "UnityEditor.VersionControl");
	ScriptingArray* array = mono_array_new(mono_domain_get(), elementType, m_Assetlist.size());
	for (int i = 0; i < m_Assetlist.size(); ++i)
	{
		// Create mono VCAsset and hook up with our cpp version
		// We create the mon0 object but we do not call the constructor
		MonoObject* monoAsset = scripting_object_new(GetScriptingTypeRegistry().GetType("UnityEditor.VersionControl", "Asset"));
		ExtractMonoObjectData<VCAsset*>(monoAsset) = UNITY_NEW(VCAsset, kMemVersionControl) (m_Assetlist[i]);
		Scripting::SetScriptingArrayElement(array, i, monoAsset);
	}
	return array;
}

ScriptingArray* VCTask::GetMonoChangeSets () const
{                       
	ScriptingClass* elementType = GetMonoManager ().GetMonoClass ("ChangeSet", "UnityEditor.VersionControl");
	ScriptingArray* array = mono_array_new(mono_domain_get(), elementType, m_ChangeSets.size());
	for (int i = 0; i < m_ChangeSets.size(); ++i)
	{
		MonoObject* monoChangeSet = scripting_object_new(GetScriptingTypeRegistry().GetType("UnityEditor.VersionControl", "ChangeSet"));
		ExtractMonoObjectData<VCChangeSet*>(monoChangeSet) = UNITY_NEW(VCChangeSet, kMemVersionControl) (m_ChangeSets[i]);
		Scripting::SetScriptingArrayElement(array, i, monoChangeSet);
	}
	return array;
}

ScriptingArray* VCTask::GetMonoMessages () const
{
	ScriptingClass* elementType = GetMonoManager ().GetMonoClass ("Message","UnityEditor.VersionControl");
	ScriptingArray* array = mono_array_new(mono_domain_get(), elementType, m_Messages.size());
	for (int i = 0; i < m_Messages.size(); ++i)
	{
		// Create mono VCMessage and hook up with our cpp version
		// We create the mon0 object but we do not call the constructor
		MonoObject* monoMsg = scripting_object_new(GetScriptingTypeRegistry().GetType("UnityEditor.VersionControl", "Message"));
		ExtractMonoObjectData<VCMessage*>(monoMsg) = UNITY_NEW(VCMessage, kMemVersionControl) (m_Messages[i]);
		Scripting::SetScriptingArrayElement(array, i, monoMsg);
	}
	return array;
}


void VCTask::EnableRepaintOnDone ()
{
	m_RepaintOnDone = true;
}

// This function is run in main thread
void VCTask::SetSuccess (bool success)
{
	m_Success = success;
}

void VCTask::SetCompletionAction (CompletionAction completionAction)
{
	Assert(m_CompletionAction == kNoAction);
	m_CompletionAction = completionAction;
}

void VCTask::OnProgress(int pctDone, int totalTimeSpent, const std::string& msg)
{
	m_SecondsSpent = totalTimeSpent;
	m_ProgressPct = pctDone;
	
	// Make thread safe string assignment.
	if (!msg.empty())
	{
		char* cmsg = (char*) UNITY_MALLOC(kMemVersionControl, msg.length() + 1);
		memcpy(cmsg, msg.c_str(), msg.length()+1);
#if UNITY_64
#error "Cannot AtomicExchange int for pointer on 64bit architecture"
#endif
		// Receiving end is in VCProvider.cpp
		int origValue = AtomicExchange(&m_NextProgressMsg, reinterpret_cast<int>(cmsg));
		if (origValue)
		{
			// Main thread did not get a chance to get the previous message and we should free it
			UNITY_FREE(kMemVersionControl, reinterpret_cast<void*>(origValue));
		}
	}
}

void VCTask::Retain ()
{
	m_RefCounter.Retain();
}

void VCTask::Release ()
{
	if (m_RefCounter.Release ())
	{
		VCTask* task = this; 
		UNITY_DELETE(task, kMemVersionControl);
	}
}


ScriptingObjectPtr CreateManagedTask(VCTask* task)
{
	ScriptingObjectPtr ptr = scripting_object_new(GetScriptingTypeRegistry().GetType("UnityEditor.VersionControl", "Task"));
	ExtractMonoObjectData<VCTask*>(ptr) = task;
	task->Retain();
	
	return ptr;
}
