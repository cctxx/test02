#pragma once

#include "Runtime/Threads/Mutex.h"
#include "Runtime/Utilities/dynamic_block_vector.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"

void LogToConsoleImplementation (const std::string& condition, int errorNum, const char* file, int line, int mode, int targetInstanceID, int identifier);
void RemoveLogImplementation (int identifier);
void ShowFirstErrorWithMode (int identifier);
void ClearPlaymodeConsoleLog();

struct MonoMethod;

struct CppLogEntry
{
	std::string condition;
	int errorNum;
	std::string file;
	int line;
	int mode;
	int instanceID;
	int identifier;
};

class EditorMonoConsole
{
	dynamic_block_vector<CppLogEntry> m_LogEntries;
	dynamic_block_vector<CppLogEntry> m_CollapsedLogEntries;
	dynamic_block_vector<int> m_CollapsedLogEntriesCount;
	dynamic_block_vector<CppLogEntry> m_FilteredLogEntries;
	dynamic_block_vector<int> m_FilteredLogEntriesCount;
	dynamic_block_vector<CppLogEntry>* m_CurrentEntriesPtr;

	// Protects m_LogEntries, m_CollapsedLogEntries, m_CurrentEntriesPtr since LogString can be called from any thread.
	// Also protects m_ProgressInfo and m_Progress.
	Mutex m_Mutex;


	int m_ActiveStatusMask;
	std::string m_ActiveStatusString;
	std::string m_ActiveStatusStringStripped;
	int m_ActiveStatusIndex;
	
	bool m_IsGettingEntries;
	bool m_LogHasChanged;
	bool m_LastIsCompiling;
	int  m_ConsoleFlags;
	
	///////@TODO: Performance issue!
	int HasLogEntryInternal(dynamic_block_vector<CppLogEntry>* logEntry, const CppLogEntry& entry);

	bool LogEntryIsOfFilteredLevel(const CppLogEntry& entry);
	void FilterConsoleEntries();
	
	public:
	
	/// All functions marked internal are not thread safe and you must call StartGettingEntries / EndGettingEntries before calling them
	
	
	enum ConsoleMode 
	{ 
		kCollapse = 1 << 0, 
		kClearOnPlay = 1 << 1, 
		kErrorPause = 1 << 2, 
		kVerbose = 1 << 3, 
		kStopForAssert = 1 << 4, 
		kStopForError = 1 << 5,
		kAutoscroll = 1 << 6, 
		kLogLevelLog = 1 << 7, 
		kLogLevelWarning = 1 << 8, 
		kLogLevelError = 1 << 9
	};
	enum DeleteMode { kDeleteNonSticky, kDeleteIdentifier, kDeleteAll };

	EditorMonoConsole ();

	void ClickStatusBar (int count);	

	static void StaticInitialize();
	static void StaticDestroy();

	int StartGettingEntries();
	void EndGettingEntries() { m_Mutex.Unlock(); m_IsGettingEntries = false; }

	void OpenEntryFile (int index);	
	void GetFirstTwoLinesTextAndModeInternal (int index, int& mask, std::string& output);

	CppLogEntry* GetEntryInternal (int index)
	{	
		AssertIf (m_CurrentEntriesPtr == NULL || !m_IsGettingEntries);
		if (index >= 0 && index < m_CurrentEntriesPtr->size())
			return &(*m_CurrentEntriesPtr)[index];
		else
			return NULL;
	}
	
	int GetCount();
	void GetCountsByType (int& outErrorCount, int& outWarningCount, int& outLogCount);

	int GetEntryCount (int index);

	void Clear ();
	void RemoveLogEntries (int identifier);

	int GetStatusViewErrorIndex();

	ScriptingMethodPtr GetStaticMonoConsoleMethod(std::string name);
	
	int GetConsoleFlags() { return m_ConsoleFlags; }
	void SetConsoleFlags(int flags);
	void SetConsoleFlag(int flags, bool value);

	void LogChanged();
	
	void LogToConsoleImplementation (const std::string& condition, int errorNum, const char* file, int line, int mode, int targetInstanceID, int identifier);
	
	void ShowFirstErrorWithMode(int mode);
	
	void UpdateStatusBar ();
	void SetStatusError (const std::string& errorString, int index, int errorMask);
	
	void Tick();
	
	const std::string& GetStatusText () { return m_ActiveStatusStringStripped; }
	int GetStatusMask () { return m_ActiveStatusMask; }

	private:
	bool RemoveEntries(dynamic_block_vector<CppLogEntry>* v, dynamic_block_vector<int>* counts, DeleteMode deleteMode, int param);
};
EditorMonoConsole& GetEditorMonoConsole();
