#include "UnityPrefix.h"
#include "Runtime/Misc/DeveloperConsole.h"

#if UNITY_HAS_DEVELOPER_CONSOLE

#include "Runtime/IMGUI/GUILabel.h"
#include "Runtime/IMGUI/GUIButton.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Graphics/ScreenManager.h"

#include "Runtime/Threads/ThreadUtility.h"

#include "Runtime/Misc/ReproductionLog.h"


static DeveloperConsole* s_DeveloperConsole = 0;

// This function performs the normal loggin operation with the initialized developer console
void DeveloperConsole_HandleLogFunction (const std::string& condition, const std::string& stackTrace, int type)
{
	// Bas Smit and Elvis Alistar suggested to disable the developer console when we are in reproduction mode.
	// [17:00:40] Bas Smit: I dont see the dev console adding value to the rig
	// [17:00:57] Elvis Alistar: it actual makes the results very difficult to interpret
	// [17:01:07] Elvis Alistar: as most of the runs will be different
#if SUPPORT_REPRODUCE_LOG
	if (GetReproduceMode() != kPlaybackReproduceLog)
#endif
	{
		UnityMemoryBarrier();
		if (s_DeveloperConsole)
			s_DeveloperConsole->HandleLog(condition, stackTrace, static_cast<LogType>(type));
	}
}


// If nothing is created, then CleanupDeveloperConsole () is a no-op
void CleanupDeveloperConsole ()
{
	RegisterLogCallback(NULL, false);

	DeveloperConsole* tmp = 0;
	std::swap(s_DeveloperConsole, tmp); // "non-locking destruction"; it is carried out a little bit later
	
	UnityMemoryBarrier();

	delete tmp;
}

void InitializeDeveloperConsole ()
{
	Assert( NULL == s_DeveloperConsole);
	s_DeveloperConsole = new DeveloperConsole();
	RegisterLogCallback(DeveloperConsole_HandleLogFunction, true);
}

DeveloperConsole& GetDeveloperConsole()
{
	Assert( NULL != s_DeveloperConsole);
	return *s_DeveloperConsole;
}
DeveloperConsole* GetDeveloperConsolePtr()
{
	return s_DeveloperConsole;
}


DeveloperConsole::DeveloperConsole() 
	: m_ConsoleClosed(false)
	, m_LogBuffer()

	// State
	, m_GUIState() 
	, m_Content()

	// Used styles
	, m_LabelStyle(0)
	, m_ButtonStyle(0)
	, m_BoxStyle(0)

	// Titles for GUI elements
	, m_DevelopmentConsoleBoxTitle("Development Console")
#if UNITY_WEBPLAYER_AND_STANDALONE
	, m_OpenLogFileButtonTitle("Open Log File")
#endif
	, m_ClearButtonTitle("Clear")
	, m_CloseButtonTitle("Close")
{}

DeveloperConsole::~DeveloperConsole() 
{
	delete m_LabelStyle;
	delete m_ButtonStyle;
	delete m_BoxStyle;
}

LogBufferEntry::LogBufferEntry(const std::string& condition, const std::string& stripped_stacktrace, LogType log)
	: type(log)
{
	// Concatenate condition and the stacktrace;
	// the only difference between expanded and non-expanded console entry is,
	// actually, just the length of the rendered string
	std::string strace(condition);
	cond_len = strace.length(); // Preserve the length of the condition

	strace += "\n> ";
		strace += stripped_stacktrace;

	// Initialize the debug string
	UTF8ToUTF16String(strace.c_str(), debug_text);
}

void DeveloperConsole::HandleLog(const std::string& condition, const std::string& strippedStacktrace, LogType type)
{
	if (type > kLogLevel && type != LogType_Exception)
		return;

#if SUPPORT_THREADS
	Mutex::AutoLock lock(m_NewEntriesMutex);
#endif

	m_NewEntries.push_back( LogBufferEntry(condition, strippedStacktrace, type));
	if (m_NewEntries.size() > kMaxNumberOfLogMessages)
	{
		m_NewEntries.pop_front(); // Forget the first message
	}
}

ObjectGUIState& DeveloperConsole::GetObjectGUIState()
{
	return m_GUIState;
}

bool DeveloperConsole::IsVisible () const
{
#if SUPPORT_THREADS
	Mutex::AutoLock consoleLock(m_ConsoleMutex); // Required for m_LogBuffer
	Mutex::AutoLock newEntriesLock(m_NewEntriesMutex); // Required for m_NewEntries
#endif
	if (m_ConsoleClosed)
		return false;
	return !m_LogBuffer.empty() || !m_NewEntries.empty();
}

void DeveloperConsole::SetOpen(bool opened)
{
	m_ConsoleClosed = !opened;
}
void DeveloperConsole::Clear()
{
#if SUPPORT_THREADS
	Mutex::AutoLock lock(m_ConsoleMutex);
#endif
	m_LogBuffer.clear();
}
bool DeveloperConsole::HasLogMessages()
{
#if SUPPORT_THREADS
	Mutex::AutoLock lock(m_ConsoleMutex);
#endif
	return !m_LogBuffer.empty();
}

// Initializes an existing instance of a UTF16String in a fast way (avoiding double-copy pattern)
inline static void FastUTF16StringInit(const UTF16String& src, UTF16String& dst)
{
	dst.TakeOverPreAllocatedUTF16Bytes(src.text, src.length);
	dst.owns = false;
}

inline static void SetupLogEntryGUIContent(GUIContent& content, const LogBufferEntry& entry)
{
	FastUTF16StringInit(entry.debug_text, content.m_Text);
	if (entry.cond_len > 0)
	{
		// if the entry is not expanded, we want to show only the part
		// that does not include the stacktrace
		content.m_Text.length = entry.cond_len;
	}
}

bool DeveloperConsole::DoGUI()
{
#if SUPPORT_THREADS
	Mutex::AutoLock lock(m_ConsoleMutex);
#endif
	
	AppendNewEntries();

	// New entries were copied into into m_LogBuffer, so just check that
	if (m_ConsoleClosed || m_LogBuffer.empty())
		return false;

	if (0 == m_LabelStyle) // if this is not initialized, 
    {                       // then all styles need to be fetched
		InitGUIStyles();
	}

	Assert( NULL != m_LabelStyle);
	Assert( NULL != m_ButtonStyle);
	Assert( NULL != m_BoxStyle);

	GUIState &guiState = GetGUIState ();
	guiState.m_CanvasGUIState.m_GUIClipState.BeginOnGUI (*guiState.m_CurrentEvent);
	guiState.BeginOnGUI(GetObjectGUIState());

	// GUI code must go here
	const float screen_width  = GetScreenManager().GetWidth();
	const float screen_height = GetScreenManager().GetHeight();

	const float half_screen_width = 0.5f * screen_width;

	float box_height = DrawBox(guiState, screen_height, half_screen_width);

	Rectf position;
	float label_pos = 0.f;
	GUIStyle& labelStyle = *m_LabelStyle;

	// We output messages in this order: the newest at the bottom and the oldest at the top
	for( std::list<LogBufferEntry>::reverse_iterator rit = m_LogBuffer.rbegin();
		rit != m_LogBuffer.rend(); ++rit)
	{
		SetupLogEntryGUIContent(m_Content, *rit);

		switch (rit->type)
		{
		case LogType_Error:
		case LogType_Assert:
		case LogType_Exception:
			labelStyle.m_Normal.textColor = ColorRGBAf (1.0f, 0.0f, 0.0f, 1.0f); // Color.red;
			break;

		case LogType_Warning:
			labelStyle.m_Normal.textColor = ColorRGBAf (1.0f, 0.0f, 0.0f, 1.0f); // Color.red;
			break;

		default:
			labelStyle.m_Normal.textColor = ColorRGBAf (1.0f, 1.0f, 1.0f, 1.0f); // Color.white;
			break;
		}

		label_pos += rit->label_height; // Label height has been previously computed in DrawBox
		
		position.Set(0,  screen_height - label_pos, half_screen_width, box_height);
		if (IMGUI::GUIButton(guiState, position, m_Content, labelStyle))
		{
			rit->cond_len = -rit->cond_len;
		}
	}

#if UNITY_WEBPLAYER_AND_STANDALONE
	position.Set(half_screen_width, screen_height - 75, 70, 20);
	if (DrawButton(guiState, position, m_OpenLogFileButtonTitle))
	{
		DeveloperConsole_OpenConsoleFile();
	}
#endif // UNITY_WEBPLAYER_AND_STANDALONE

	position.Set(half_screen_width, screen_height - 50, 70, 20);
	if (DrawButton(guiState, position, m_ClearButtonTitle))
	{
		m_LogBuffer.clear();
	}

	position.Set(half_screen_width, screen_height - 25, 70, 20);
	if (DrawButton(guiState, position, m_CloseButtonTitle))
	{
		m_ConsoleClosed = true;
	}

	guiState.EndOnGUI ();
	guiState.m_CanvasGUIState.m_GUIClipState.EndOnGUI (*guiState.m_CurrentEvent);

	return guiState.m_CurrentEvent->type == InputEvent::kUsed;
}

void DeveloperConsole::AppendNewEntries()
{
#if SUPPORT_THREADS
	// We already have a lock on m_ConsoleMutex for writing to m_LogBuffer
	// We need a lock on new entries potentially added from other threads
	Mutex::AutoLock lock(m_NewEntriesMutex);
#endif
	
	while (!m_NewEntries.empty())
	{
		m_LogBuffer.push_back(m_NewEntries.front());
		m_NewEntries.pop_front();
		if (m_LogBuffer.size() > kMaxNumberOfLogMessages)
		{
			m_LogBuffer.pop_front(); // Forget the first message
		}
		m_ConsoleClosed = false; // New log message, the console pops back
	}
}

bool DeveloperConsole::DrawButton(GUIState& guiState, const Rectf& position, const UTF16String& label)
{
	FastUTF16StringInit(label, m_Content.m_Text);
	return IMGUI::GUIButton(guiState, position, m_Content, *m_ButtonStyle);
}

float DeveloperConsole::DrawBox(GUIState& guiState, float height, float width)
{
	const GUIStyle& labelStyle = *m_LabelStyle;

	// Compute how much space log messages themselves would take
	float box_body_height = 0.f;
	for( std::list<LogBufferEntry>::iterator it = m_LogBuffer.begin();
		it != m_LogBuffer.end(); ++it)
	{
		SetupLogEntryGUIContent(m_Content, *it);
		it->label_height = labelStyle.CalcHeight(m_Content, width);
		box_body_height += it->label_height;
	}

	// Accomodate the box title
	GUIStyle& boxStyle = *m_BoxStyle;
	
	FastUTF16StringInit(m_DevelopmentConsoleBoxTitle, m_Content.m_Text);
	float box_title_height = boxStyle.CalcHeight(m_Content, width);

	// Summa summarum: the height of the developer console is its body plus its title
	float dev_console_height = box_body_height + box_title_height;

	Rectf position(0,  height - dev_console_height, width, dev_console_height);
	IMGUI::GUILabel(guiState, position, m_Content, boxStyle);
	return box_body_height;
}


// Hardcoded styles (the values copied from the Editor)

static GUIStyle* CreateLabelIconGUIStyle ()
{
	GUIStyle* style = new GUIStyle ();
	
	style->m_Normal.textColor = ColorRGBAf (1.0f, 1.0f, 1.0f, 1.0f);//ColorRGBAf (0.0f, 0.0f, 0.0f, 1.0f);
	style->m_Hover.textColor = ColorRGBAf (0.0f, 0.0f, 0.0f, 1.0f);
	style->m_OnNormal.textColor = ColorRGBAf (0.0f, 0.0f, 0.0f, 1.0f);

	style->m_Padding.left = 3; // so that the messages do not look glued to the left of the screen
	style->m_Padding.right = 0;
	style->m_Padding.top = 3;
	style->m_Padding.bottom = 3;

	style->m_RichText = true;
	style->m_Alignment = kUpperLeft;
	style->m_Clipping = kOverflow;

	style->m_ImagePosition = kTextOnly;
	style->m_WordWrap = true;

	return style;
}

static Texture2D* GetResourceTexture(const char* texture_name)
{
	return reinterpret_cast<Texture2D*>(
		GetBuiltinResourceManager ().GetResource (ClassID(Texture2D), texture_name));
}

static GUIStyle* CreateButtonGUIStyle ()
{
	GUIStyle* style = new GUIStyle ();

	const float normal_color = 230.0f / 255.0f;
	style->m_Normal.textColor  = ColorRGBAf (normal_color, normal_color, normal_color, 1.0f);
	style->m_Normal.background =  GetResourceTexture("GameSkin/button.png");

	style->m_Hover.textColor = ColorRGBAf (1.0f, 1.0f, 1.0f, 1.0f);
	style->m_Hover.background =  GetResourceTexture("GameSkin/button hover.png");

	style->m_Active.textColor =  style->m_Normal.textColor;
	style->m_Active.background =  GetResourceTexture("GameSkin/button active.png");

	style->m_Focused.textColor = style->m_Hover.textColor;

	style->m_OnNormal.textColor = style->m_Normal.textColor;
	style->m_OnNormal.background = GetResourceTexture("GameSkin/button on.png");

	style->m_OnHover.textColor = style->m_Hover.textColor;
	style->m_OnHover.background = GetResourceTexture("GameSkin/button on hover.png");

	style->m_OnActive.textColor = style->m_Normal.textColor;
	style->m_OnActive.background = GetResourceTexture("GameSkin/button active.png");

	style->m_Border.left = 6;
	style->m_Border.right = 6;
	style->m_Border.top = 6;
	style->m_Border.bottom = 4;

	style->m_Margin.left = 4;
	style->m_Margin.right = 4;
	style->m_Margin.top = 4;
	style->m_Margin.bottom = 4;

	style->m_Padding.left = 6;
	style->m_Padding.right = 6;
	style->m_Padding.top = 3;
	style->m_Padding.bottom = 3;

	style->m_RichText = false;
	style->m_FontSize = 10; // make button captions a little bit smaller
	style->m_Alignment = kMiddleCenter;

	style->m_StretchWidth  = true;
	style->m_StretchHeight = false;

	return style;
}

static GUIStyle* CreateBoxGUIStyle ()
{
	GUIStyle* style = new GUIStyle ();

	const float normal_color = 204.0f / 255.0f;
	style->m_Normal.textColor  = ColorRGBAf (normal_color, normal_color, normal_color, 1.0f);
	style->m_Normal.background =  GetResourceTexture("GameSkin/box.png");

	style->m_Border.left = 6;
	style->m_Border.right = 6;
	style->m_Border.top = 6;
	style->m_Border.bottom = 6;

	style->m_Margin.left = 4;
	style->m_Margin.right = 4;
	style->m_Margin.top = 4;
	style->m_Margin.bottom = 4;

	style->m_Padding.left = 4;
	style->m_Padding.right = 4;
	style->m_Padding.top = 4;
	style->m_Padding.bottom = 4;

	style->m_RichText = false;
	style->m_FontStyle = 1; // The box title is bold
	style->m_Alignment = kUpperCenter;

	style->m_StretchWidth  = true;
	style->m_StretchHeight = false;

	return style;
}

void DeveloperConsole::InitGUIStyles()
{
	m_LabelStyle  = CreateLabelIconGUIStyle();
	m_ButtonStyle = CreateButtonGUIStyle();
	m_BoxStyle    = CreateBoxGUIStyle();
}

#endif // UNITY_HAS_DEVELOPER_CONSOLE


#if UNITY_WIN
#include <ShellAPI.h>
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#endif

void DeveloperConsole_OpenConsoleFile () 
{
	std::string path = GetConsoleLogPath();
#if UNITY_OSX
	system (("open \""+path+"\"").c_str());
#elif UNITY_WIN && !UNITY_WINRT
	std::wstring widePath;
	ConvertUnityPathName(path, widePath);
	
	int res = (int)::ShellExecuteW( NULL, L"open", widePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
	if (res <= 32)
		res = (int)::ShellExecuteW( NULL, L"edit", widePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
	ErrorString ("Opening Console File is not supported on this platform.");
#endif
}
