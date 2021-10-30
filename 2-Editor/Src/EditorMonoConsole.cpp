#include "UnityPrefix.h"
#include "EditorMonoConsole.h"
#include "Runtime/Mono/MonoManager.h"
#include "EditorHelper.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Editor/Platform/Interface/BugReportingTools.h"
#include "Application.h"
#include "Editor/Src/AssetPipeline/MonoCompile.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"
#if UNITY_WIN
#include "Editor/Platform/Windows/Utility/Windows7Taskbar.h"
#endif

///@TODO: Make status icon bounce
///@TODO: When double clicking status line, do not open the console from single click (delay single click and cancel)
///@TODO; Pull console flags from player prefs
///@TODO; Implement frame selected on console when calling ShowConsoleRow

static const char* kConsoleFlagsString = "kMonoEditorConsoleFlagsV2";

EditorMonoConsole::EditorMonoConsole ()
: m_LogEntries(200)
, m_CollapsedLogEntries(200)
, m_CollapsedLogEntriesCount(200)
, m_FilteredLogEntries(200)
, m_FilteredLogEntriesCount(200)
{
	m_IsGettingEntries = false;
	m_LogHasChanged = false;
	m_LastIsCompiling = false;
	m_ConsoleFlags = EditorPrefs::GetInt(kConsoleFlagsString, 898); // Errors, Warnings, Logs, ClearOnPlay all enabled by default.
	
	FilterConsoleEntries();
	m_CurrentEntriesPtr = &m_FilteredLogEntries;
}

void EditorMonoConsole::UpdateStatusBar ()
{
	m_Mutex.Lock();
	int index = m_CurrentEntriesPtr->size() - 1;
	CppLogEntry* error = NULL;
	if (index != -1)
	{
		while (true)
		{
			error = &(*m_CurrentEntriesPtr)[index];
			if ((error->mode & kDisplayPreviousErrorInStatusBar) == 0 || index == 0)
				break;
			index--;
		}
	}	
	m_Mutex.Unlock();
	
	string empty;
	
	const string& errorString = error ? error->condition : empty;
	int errorMask = error ? error->mode : 0;
	SetStatusError (errorString, index, errorMask);
}

void EditorMonoConsole::SetStatusError (const std::string& errorString, int index, int errorMask)
{
	if (m_ActiveStatusMask != errorMask || errorString != m_ActiveStatusString)
	{
		m_ActiveStatusString = errorString;
		// Single line error string only
		string::size_type pos = errorString.find('\n');
		string strippedString;
		if (pos == string::npos)
			m_ActiveStatusStringStripped = errorString;
		else
			m_ActiveStatusStringStripped.assign(errorString.begin(), errorString.begin() + pos);
		
		m_ActiveStatusMask = errorMask;

		ScriptingInvocation (MONO_COMMON.statusBarChanged).Invoke();
	}
	m_ActiveStatusIndex = index;
}

void EditorMonoConsole::ClickStatusBar (int count)
{
	m_Mutex.Lock();
	if (m_ActiveStatusIndex < 0 || m_ActiveStatusIndex >= m_CurrentEntriesPtr->size())
	{
		m_Mutex.Unlock();
		return;
	}
	m_Mutex.Unlock();
		
	if (count == 1)
	{
		void* param[] = { &m_ActiveStatusIndex };
		CallStaticMonoMethod("ConsoleWindow", "ShowConsoleRow", param);
	}
	else if (count == 2)
	{
		OpenEntryFile (m_ActiveStatusIndex);
	}
}


void LogToConsoleImplementation (const std::string& condition, int errorNum, const char* file, int line, int mode, int targetInstanceID, int identifier)
{
	GetEditorMonoConsole().LogToConsoleImplementation(condition, errorNum, file, line, mode, targetInstanceID, identifier);
}

void RemoveLogImplementation (int identifier)
{
	GetEditorMonoConsole().RemoveLogEntries(identifier);
}

void ClearPlaymodeConsoleLog()
{
	EditorMonoConsole& console = GetEditorMonoConsole();

	if ((console.GetConsoleFlags() & EditorMonoConsole::kClearOnPlay) == 0)
		return;

	console.Clear();
}

void ShowFirstErrorWithMode(int mode)
{
	GetEditorMonoConsole().ShowFirstErrorWithMode(mode);
}

void EditorMonoConsole::ShowFirstErrorWithMode(int mode)
{
	m_Mutex.Lock();

	for (int i=0;i<m_CurrentEntriesPtr->size();i++)
	{
		if (mode & (*m_CurrentEntriesPtr)[i].mode)
		{
			m_Mutex.Unlock();
			void* param[] = { &i };
			CallStaticMonoMethod("ConsoleWindow", "ShowConsoleRow", param);
			return;
		}
	}
	
	m_Mutex.Unlock();
}

void EditorMonoConsole::GetFirstTwoLinesTextAndModeInternal (int index, int& mask, std::string& output)
{
	CppLogEntry* src = GetEntryInternal(index);
	if (src)
	{
		mask = src->mode;
		// Extract two line error
		string::size_type pos = src->condition.find('\n');
		if (pos != string::npos && pos != src->condition.size())
			pos = src->condition.find('\n', pos+1);
		
		// assign to outString
		if (pos != string::npos)
			output.assign (src->condition.begin(), src->condition.begin() + pos);
		else
			output = src->condition;
	}
	else
	{
		mask = 0;
		output.clear();
	}
}

void EditorMonoConsole::OpenEntryFile (int index)
{
	m_Mutex.Lock();
	if (index < 0 || index >= m_CurrentEntriesPtr->size())
	{
		m_Mutex.Unlock();
		return;
	}

	CppLogEntry* entry = &(*m_CurrentEntriesPtr)[index];
	string file = entry->file;
	int line = entry->line;
	int instanceID = entry->instanceID;
	m_Mutex.Unlock();

	// Try opening the file where the error comes from (error->path)
	// Otherwise open the the object that was recorded with the error
	if (!TryOpenErrorFileFromConsole (file, line))
		OpenAsset (instanceID, line);
}

int EditorMonoConsole::StartGettingEntries()
{
	m_IsGettingEntries = true;
	m_Mutex.Lock();
	return m_CurrentEntriesPtr->size();
}

int EditorMonoConsole::GetCount()
{
	Mutex::AutoLock lock(m_Mutex);
	return m_CurrentEntriesPtr->size();
}

void EditorMonoConsole::GetCountsByType (int& outErrorCount, int& outWarningCount, int& outLogCount)
{
	Mutex::AutoLock lock(m_Mutex);
	outErrorCount = outWarningCount = outLogCount = 0;

	// We always want to see the total count independent of the current filter
	dynamic_block_vector<CppLogEntry>* nonFilteredEntries;

	if (m_ConsoleFlags & kCollapse)
		nonFilteredEntries = &m_CollapsedLogEntries;
	else
		nonFilteredEntries = &m_LogEntries;

	size_t size = nonFilteredEntries->size();
	for (size_t i = 0; i < size; i++)
	{
		const CppLogEntry& le = (*nonFilteredEntries)[i];
		if (le.mode & (kFatal | kAssert | kError | kScriptingError | kAssetImportError | kScriptCompileError))
			++outErrorCount;
		else if (le.mode & (kScriptCompileWarning | kScriptingWarning | kAssetImportWarning))
			++outWarningCount;
		else if (le.mode & (kLog | kScriptingLog))
			++outLogCount;
	}
}

int EditorMonoConsole::GetEntryCount (int index)
{
	if (!(m_ConsoleFlags & kCollapse))
		return 1;

	if (m_ConsoleFlags & (kLogLevelLog | kLogLevelWarning | kLogLevelError))
	{
		DebugAssert (index < m_FilteredLogEntriesCount.size());

		return m_FilteredLogEntriesCount[index];
	}
	else
	{
		DebugAssert (index < m_CollapsedLogEntriesCount.size());

		return m_CollapsedLogEntriesCount[index];
	}
}

void EditorMonoConsole::Clear ()
{
	RemoveEntries(&m_LogEntries, NULL, kDeleteNonSticky, 0);
	RemoveEntries(&m_CollapsedLogEntries, &m_CollapsedLogEntriesCount, kDeleteNonSticky, 0);
	RemoveEntries(&m_FilteredLogEntries, &m_FilteredLogEntriesCount, kDeleteNonSticky, 0);
	m_LogHasChanged = true;
}

void EditorMonoConsole::RemoveLogEntries (int identifier)
{
	RemoveEntries(&m_LogEntries, NULL, kDeleteIdentifier, identifier);
	RemoveEntries(&m_CollapsedLogEntries, &m_CollapsedLogEntriesCount, kDeleteIdentifier, identifier);
	RemoveEntries(&m_FilteredLogEntries, &m_FilteredLogEntriesCount, kDeleteIdentifier, identifier);
	m_LogHasChanged = true;
}

int EditorMonoConsole::GetStatusViewErrorIndex()
{
	Mutex::AutoLock lock(m_Mutex);
	for (int i = m_CurrentEntriesPtr->size() - 1; i >= 0; i--)
		if (((*m_CurrentEntriesPtr)[i].mode & kDisplayPreviousErrorInStatusBar) == 0)
			return i;
	
	return -1;
}

ScriptingMethodPtr EditorMonoConsole::GetStaticMonoConsoleMethod(string name)
{
	if (AssetInterface::Get().IsLocked ()) // don't call mono functions while assets are locked
		return NULL;

	MonoClass* klass = GetMonoManager().GetMonoClass("ConsoleWindow", "UnityEditorInternal");
	
	if (!klass)
		return NULL;

	ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(klass,name.c_str(), ScriptingMethodRegistry::kStaticOnly);
	
	if (!method)
		return NULL;
		
	return method;
}

void EditorMonoConsole::SetConsoleFlag(int flags, bool value)
{
	int newFlags = m_ConsoleFlags;
	if (value)
		newFlags |= flags;
	else
		newFlags &= ~flags;	
	SetConsoleFlags(newFlags);
}

void EditorMonoConsole::SetConsoleFlags(int flags)
{
	if (m_ConsoleFlags == flags)
		return;
		
	m_ConsoleFlags = flags;
	EditorPrefs::SetInt(kConsoleFlagsString, m_ConsoleFlags);

	Mutex::AutoLock lock(m_Mutex);
	
	FilterConsoleEntries();
	m_CurrentEntriesPtr = &m_FilteredLogEntries;
}

void EditorMonoConsole::FilterConsoleEntries()
{
	RemoveEntries(&m_FilteredLogEntries, &m_FilteredLogEntriesCount, kDeleteAll, 0);

	if(m_ConsoleFlags & kCollapse)
	{
		for (int i = 0; i < m_CollapsedLogEntries.size(); i++)
		{
			if(LogEntryIsOfFilteredLevel(m_CollapsedLogEntries[i]))
			{
				m_FilteredLogEntries.push_back(m_CollapsedLogEntries[i]);
				m_FilteredLogEntriesCount.push_back(m_CollapsedLogEntriesCount[i]);
			}
		}  
	}
	else
	{
		for (int i = 0; i < m_LogEntries.size(); i++)
		{
			if(LogEntryIsOfFilteredLevel(m_LogEntries[i]))
			{
				m_FilteredLogEntries.push_back(m_LogEntries[i]);
				m_FilteredLogEntriesCount.push_back(1);
			}
		}  
	}
}

///////@TODO: Performance issue!
int EditorMonoConsole::HasLogEntryInternal(dynamic_block_vector<CppLogEntry>* logEntries, const CppLogEntry& entry)
{
	#define EQ(item) entry.item == (*logEntries)[i].item 
	
	for (int i = 0; i < (*logEntries).size(); i++)
		if (EQ(identifier) && EQ(errorNum) && EQ(mode) && EQ(line) && EQ(instanceID) && EQ(file) && EQ(condition))
			return i;
	
	return -1;
}

bool EditorMonoConsole::LogEntryIsOfFilteredLevel(const CppLogEntry& entry)
{
	const int logFlags = (kLog | kScriptingLog);
	const int warningFlags = (kScriptCompileWarning | kScriptingWarning | kAssetImportWarning);
	const int errorFlags = (kFatal | kAssert | kError | kScriptCompileError| kScriptingError | kAssetImportError);

	if ((m_ConsoleFlags & kLogLevelWarning) && (entry.mode & warningFlags))
		return true;

	if ((m_ConsoleFlags & kLogLevelError) && (entry.mode & errorFlags))
		return true;

	// Have to make sure that the warningflags and errorflags aren't set as kLog is set for everything.
	if ((m_ConsoleFlags & kLogLevelLog) && (entry.mode & logFlags) && !(entry.mode & warningFlags) && !(entry.mode & errorFlags))
		return true;

	return false;
}

bool EditorMonoConsole::RemoveEntries(dynamic_block_vector<CppLogEntry>* v, dynamic_block_vector<int>* counts, DeleteMode deleteMode, int param)
{
	AssertIf(m_IsGettingEntries);
	if (m_IsGettingEntries)
		return false;  
	
	m_Mutex.Lock();
	
	DebugAssert (counts == NULL || counts->size () == v->size ());

	int delIndex = 0;
	for (int i = 0; i < v->size(); i++)
	{
		bool del = false;
		CppLogEntry& entry = (*v)[i];
		
		switch (deleteMode)
		{
			case kDeleteNonSticky: del = (entry.mode & kStickyError) == 0; break;
			case kDeleteIdentifier: del = entry.identifier == param; break;
			case kDeleteAll: del = true; break;
		}
		
		if (!del)
		{
			if (delIndex != i)
			{
				(*v)[delIndex] = (*v)[i];
				if (counts)
					(*counts)[delIndex] = (*counts)[i];
			}
			delIndex++;
		}
	}
	
	if (delIndex == v->size())
	{
		m_Mutex.Unlock();
		return false;
	}
	else
	{
		v->resize(delIndex);
		if (counts)
			counts->resize (delIndex);

		m_Mutex.Unlock();

		return true;
	}
}

void EditorMonoConsole::Tick()
{
	//Update spinning wheel
	bool isCompiling = IsCompiling();
	if (isCompiling || isCompiling != m_LastIsCompiling)
	{
		ScriptingInvocation(MONO_COMMON.statusBarChanged).Invoke();
		m_LastIsCompiling = isCompiling;
	}
	
	if (!m_LogHasChanged)
		return;
		
	if (GetMonoManagerPtr ())
	{
		UpdateStatusBar ();

		ScriptingMethodPtr method = MONO_COMMON.consoleLogChanged;
		if (method)
		{
			m_LogHasChanged = false;
			ScriptingInvocation(method).Invoke();
			return;
		}
	}
	m_LogHasChanged = true;
}

void EditorMonoConsole::LogToConsoleImplementation (const std::string& condition, int errorNum, const char* file, int line, int mode, int targetInstanceID, int identifier)
{
	SET_ALLOC_OWNER(this);
	CppLogEntry entry;
	
	entry.condition = condition;
	entry.errorNum = errorNum;
	entry.file = file;
	entry.line = line;
	entry.mode = mode;
	entry.identifier = identifier;
	entry.instanceID = targetInstanceID;
	
	m_Mutex.Lock();
	m_LogEntries.push_back(entry);
	
	int logEntryIndex = HasLogEntryInternal(&m_CollapsedLogEntries, entry);
	if (logEntryIndex == -1)
	{
		m_CollapsedLogEntries.push_back(entry);
		m_CollapsedLogEntriesCount.push_back(1);
	}
	else
		m_CollapsedLogEntriesCount[logEntryIndex]++;

	if (LogEntryIsOfFilteredLevel(entry))
	{
		if(m_ConsoleFlags & kCollapse)
		{
			logEntryIndex = HasLogEntryInternal(&m_FilteredLogEntries, entry);
			if (logEntryIndex == -1)
			{
				m_FilteredLogEntries.push_back(entry);
				m_FilteredLogEntriesCount.push_back(1);
			}
			else
				m_FilteredLogEntriesCount[logEntryIndex]++;
		}
		else
		{
			m_FilteredLogEntries.push_back(entry);
			m_FilteredLogEntriesCount.push_back(1);
		}
	}

	m_Mutex.Unlock();
	
	m_LogHasChanged = true;

	if (mode & kFatal)
	{
		if (IsBatchmode())
			CheckBatchModeErrorString("Fatal error! " +  condition);
		
		DisplayDialog ("Fatal error!", condition, "Quit", ""); // TODO: show error sign

		if (mode & kReportBug)
			LaunchBugReporter (kFatalError);
		else
			ExitDontLaunchBugReporter ();
	}
	else if ((mode & kError) && (m_ConsoleFlags & kStopForError))
	{
		if (!IsBatchmode() && !DisplayDialog("Error!", condition, "Continue", "Don't show again")) // TODO: show error sign
			SetConsoleFlag(kStopForError, false);
	}
	else if ((mode & kAssert) && (m_ConsoleFlags & kStopForAssert))
	{
		if (!IsBatchmode() && !DisplayDialog("Assert!", condition, "Continue", "Don't show again")) // TODO: show error sign
			SetConsoleFlag(kStopForAssert, false);
	}
	
	// Pause when playing, pause pref is set and never stop for logs (only real errors)
	if (IsWorldPlaying() && (m_ConsoleFlags & kErrorPause))
	{
		if ((mode & (kAssert | kLog | kScriptingLog | kScriptingWarning)) == 0 || (mode & kScriptingError) == kScriptingError)
			PauseEditor ();
	}
}

EditorMonoConsole* gConsole = NULL;

void EditorMonoConsole::StaticInitialize()
{
}

void EditorMonoConsole::StaticDestroy()
{
	UNITY_DELETE (gConsole, kMemManager);
}

static RegisterRuntimeInitializeAndCleanup s_EditorMonoConsoleCallbacks(EditorMonoConsole::StaticInitialize, EditorMonoConsole::StaticDestroy);

EditorMonoConsole& GetEditorMonoConsole ()
{
	if(!gConsole)
		gConsole = UNITY_NEW_AS_ROOT (EditorMonoConsole(), kMemManager, "ConsoleWindowBackend", "");

	return *gConsole;
}
