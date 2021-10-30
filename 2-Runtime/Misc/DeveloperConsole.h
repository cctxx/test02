#pragma once

#include "Runtime/Utilities/LogAssert.h" // for LogType

// Can't get the DeveloperConsole to work on iOS, Android or XBox 
// (fails with Can't add script behaviour DeveloperConsole. The scripts file name does not match the name of the class defined in the script!)
// No time to look into, need to merge core. Fix & Enabled later.
#define UNITY_HAS_DEVELOPER_CONSOLE \
	!(UNITY_FLASH || UNITY_WEBGL) && GAMERELEASE && UNITY_DEVELOPER_BUILD && !(UNITY_IPHONE || UNITY_XENON || UNITY_ANDROID || UNITY_PS3)


#if UNITY_HAS_DEVELOPER_CONSOLE

#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/IMGUI/GUIStyle.h"
#include "Runtime/IMGUI/GUIContent.h"

#if SUPPORT_THREADS
#include "Runtime/Threads/Mutex.h"
#endif

struct LogBufferEntry
{
	// The sign bit is used to see whether a log entry is expanded or not
	int cond_len;

	LogType type;

	// This is only used as cache, so that label height 
	// computation would not be done twice
	float label_height;

	UTF16String debug_text;
	
	// Constructs the object and initializes UTF16String members
	LogBufferEntry(const std::string& condition, const std::string& stripped_stacktrace, LogType log);

	// Moves the ownership of UTF16String memory to this object
	LogBufferEntry(const LogBufferEntry& rhs)
		: cond_len(rhs.cond_len)
		, type(rhs.type)
	{
		fast_string_init(rhs);
	}

	// Moves the ownership of UTF16String memory to this object
	LogBufferEntry& operator=(const LogBufferEntry& rhs)
	{
		fast_string_init(rhs);
		cond_len = rhs.cond_len;
		type     = rhs.type;
		return *this;
	}

private:
	static void change_memory_ownership_impl(UTF16String& src, UTF16String& dst) 
	{
		dst.TakeOverPreAllocatedUTF16Bytes(src.text, src.length);
		dst.owns = src.owns; // it is possible that src DOES NOT own the string
		src.owns = false; // if dst owns, then src must not own anymore;
		                  // if dst does not own, then src does not own either, so this assignment is a no-op.
	}

	void fast_string_init(const LogBufferEntry& rhs) 
	{
		change_memory_ownership_impl(*const_cast<UTF16String*>(&rhs.debug_text), debug_text);
	}
};


class DeveloperConsole : private NonCopyable
{
public:
	static const LogType kLogLevel = LogType_Assert;

	// The number of log messages that are displayed
	static const unsigned int kMaxNumberOfLogMessages = 10u;

	// Interface that is needed by the GUIManager in order to render the console, etc.
	bool IsVisible() const;

	void SetOpen(bool opened);

	void Clear();

	bool HasLogMessages();

	bool DoGUI();

	ObjectGUIState& GetObjectGUIState();

private:
	// Developer console should be instantiated only and only from InitializeDeveloperConsole,
	// thus its constructor is made private. For all other functions, for example,
	// functions in LogAssert.h, the only accessible member functions are the static ones.
	friend void InitializeDeveloperConsole ();
	friend void CleanupDeveloperConsole ();

	DeveloperConsole();
	~DeveloperConsole();

private:
	friend void DeveloperConsole_HandleLogFunction (const std::string& condition, const std::string &stackTrace, int type);

	void HandleLog(const std::string& condition, const std::string& strippedStacktrace, LogType logType);

private:
	// Helper member functions
	void InitGUIStyles();

	void AppendNewEntries();

	bool DrawButton(GUIState& guiState, const Rectf& position, const UTF16String& label);
	float DrawBox(GUIState& guiState, float height, float width);

private:
	bool m_ConsoleClosed;
	std::list<LogBufferEntry> m_LogBuffer;
	std::list<LogBufferEntry> m_NewEntries;
	ObjectGUIState m_GUIState;
	GUIContent m_Content;

	// GUI styles that are used in DeveloperConsole.
	// Initial values are currently copied from the default skin and hardcoded.
	GUIStyle* m_LabelStyle;
	GUIStyle* m_ButtonStyle;
	GUIStyle* m_BoxStyle;

	// Labels are cached, so that they are not allocated and copied on each DoGUI invocation
	const UTF16String m_DevelopmentConsoleBoxTitle;
#if UNITY_WEBPLAYER_AND_STANDALONE
	const UTF16String m_OpenLogFileButtonTitle;
#endif
	const UTF16String m_ClearButtonTitle;
	const UTF16String m_CloseButtonTitle;

#if SUPPORT_THREADS
	mutable Mutex m_ConsoleMutex;
	mutable Mutex m_NewEntriesMutex;
#endif
};


DeveloperConsole& GetDeveloperConsole();
DeveloperConsole* GetDeveloperConsolePtr();
void CleanupDeveloperConsole ();
void InitializeDeveloperConsole ();

void DeveloperConsole_HandleLogFunction (const std::string& condition, const std::string &stackTrace, int type);

#endif // UNITY_HAS_DEVELOPER_CONSOLE

// This function is used in UnityEngineDebug.cpp
void DeveloperConsole_OpenConsoleFile();
