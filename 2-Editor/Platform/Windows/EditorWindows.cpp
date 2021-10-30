#include "UnityPrefix.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Editor/Platform/Interface/DragAndDrop.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "External/ShaderLab/Library/shaderlab.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "Editor/Src/Undo/UndoManager.h"
#include "Runtime/GfxDevice/d3d/D3D9Context.h"
#include "Runtime/Profiler/GPUProfiler.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "Runtime/Camera/RenderManager.h"
#include "Editor/Platform/Interface/RepaintController.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/Application.h"
#include "PlatformDependent/Win/WinUnicode.h"
#include "resource.h"
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Misc/SystemInfo.h"
#include <windowsx.h>
#include "Editor/Src/TooltipManager.h"
#include "PlatformDependent/Win/WinIME.h"
#include "Runtime/Misc/Player.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Editor/Src/AuxWindowManager.h"
#include "Editor/Src/WebViewWrapper.h"
#include "Runtime/IMGUI/GUIWindows.h"
#include "Runtime/BaseClasses/Cursor.h"
#include <stack>


#define DISABLE_BLIT_PAINT_OPTIMIZATION 0
#define ENABLE_WINDOW_MESSAGES_LOGGING 0
#define ENABLE_CPU_TIMER 0
#define ENABLE_DRAGTAB_DEBUG 0
#define ENABLE_VIEW_SHOT_DUMP 0

#if ENABLE_CPU_TIMER
#include "PlatformDependent/Win/CPUTimer.h"
#endif

#if ENABLE_VIEW_SHOT_DUMP
#include "Runtime/Misc/CaptureScreenshot.h"
static int s_ShotFrames = 0;
#endif

#define IsModalWindow(win) (GetParent(win) == NULL) // FIXME: we might need a better check for this later
static const wchar_t* kContainerWindowClassName = L"UnityContainerWndClass";
static const wchar_t* kPopupWindowClassName = L"UnityPopupWndClass";
static const wchar_t* kGUIViewWindowClassName = L"UnityGUIViewWndClass";
static const char* kIsMainWindowMaximized = "IsMainWindowMaximized";
const int kTimerEvent = 1;

extern bool gInitialized;

extern GUIViews g_GUIViews;
static GUIView *s_CurrentView = NULL;

typedef std::set<ContainerWindow*> ContainerWindows;
static ContainerWindows s_ContainerWindows;

typedef List< ListNode<ContainerWindow> > ContainerWindowList;
static ContainerWindowList s_ZUtilityWindows;

static ContainerWindow* s_LastActiveContainer = NULL;
static GUIView* s_LastActiveView = NULL;

static bool  s_DontSendEventsHack = false;
static bool	 s_IsMainWindowMaximized;
static bool  s_IsGfxWindowValidOnAllGUIViews = true;
Rectf s_UnmaximizedMainWindowRect;


static ABSOLUTE_TIME s_LastInactivateTime = 0;


bool IsD3D9DeviceLost();

double GetTimeSinceStartup();

// WinEditorMain.cpp
void SetMainEditorWindow(HWND);
HWND GetMainEditorWindow();
void DoQuitEditor(); 
extern bool gAlreadyClosing;
bool ProcessMainWindowMessages( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result );
void ModalWinMainMessageLoop(ContainerWindow *win);


// MenuControllerWin.cpp
HMENU GetMainMenuHandle();
void ValidateSubmenu( HMENU menu );
bool ExecuteStandardMenuItem( int id );

// IsPlaying.cpp
bool IsWorldPlaying();

bool IsInputInitialized();
void InputActivate(void);
void InputPassivate(void);
LRESULT ProcessInputMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, BOOL &handled);
void InputSetWindow(HWND window, bool fullscreen);

// InputEventWin.cpp
int GetModifierFlags ();

// EditorUtility.cpp
void ShowInTaskbarIfNoMainWindow(HWND wnd);

/// Set up the event data for mouse position and modifier keys
void SetupEventValues (InputEvent *evt)
{
	evt->modifiers = GetModifierFlags();
}


void InvalidateGraphicsStateInEditorWindows()
{
	// nothing to do; there are no separate contexts for each window
}

void RequestRepaintOnAllViews()
{
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView& view = **i;
		view.RequestRepaint();
	}
}

void ForceRepaintOnAllViews()
{
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView& view = **i;
		view.ForceRepaint();
	}
}

void EnableAllContainerWindows( bool enable )
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: EnableAllContainerWindows enable=%i\n", enable);
	#endif
	ContainerWindows::iterator it, itEnd = s_ContainerWindows.end();
	for( it = s_ContainerWindows.begin(); it != itEnd; ++it ) {
		ContainerWindow* window = *it;
		EnableWindow( window->GetWindowHandle(), enable ? TRUE : FALSE );
	}
}

void ContainerWindow::OnActivateApplication( bool activate )
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING || ENABLE_DRAGTAB_DEBUG
	printf_console("EW: OnActivateApplication wnd=%x act=%i\n", GetWindowHandle (), activate);
	#endif
	ContainerWindowList::iterator it;
	if( activate )
	{
		// Our application brought to foreground: reset script reload lock. Otherwise
		// we might have locked on some mouse down, but never get mouse up because user switched
		// to another app.
		if(GetApplicationPtr())
			GetApplication().ResetReloadAssemblies();

		// When application is activated:
		// . Show all utility windows
		for( it = s_ZUtilityWindows.begin(); it != s_ZUtilityWindows.end(); ++it ) {
			ContainerWindow* window = it->GetData();
			AssertIf( window->GetShowMode() != ContainerWindow::kShowUtility );
			HWND wnd = window->GetWindowHandle();

			// We use HWND_NOTOPMOST to ensure Utility windows are below Save/Open dialogs and above main editor window
			SetWindowPos( wnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW );
		}

		if (!gAlreadyClosing && !m_IsClosing)
		{
			SetPlayerPause( kPlayerRunning );
		}
	}
	else
	{
		// When application is deactivated:
		// . if some window had mouse capture, simulate a mouse up event
		// . hide all utility windows

		if (!gAlreadyClosing && !m_IsClosing)
		{
			if (!GetPlayerRunInBackground())
			{
				SetPlayerPause(kPlayerPausing);
			}
		}

		GUIView* lastView = GetKeyGUIView();
		if( lastView )
			lastView->ReleaseMouseCaptureIfNeeded();

		for( it = s_ZUtilityWindows.begin(); it != s_ZUtilityWindows.end(); ++it ) {
			ContainerWindow* window = it->GetData();
			AssertIf( window->GetShowMode() != ContainerWindow::kShowUtility );
			HWND wnd = window->GetWindowHandle();
			if (IsModalWindow (wnd))
				continue;
			SetWindowPos( wnd, NULL, 0,0,0,0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_HIDEWINDOW );
		}
	}
}

void OnMinimizeApplication( HWND sendingWindow )
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: OnMinimizeApplication wnd=%x\n", sendingWindow);
	#endif
	ContainerWindows::iterator it, itEnd = s_ContainerWindows.end();
	for( it = s_ContainerWindows.begin(); it != itEnd; ++it ) {
		ContainerWindow* window = *it;
		HWND wnd = window->GetWindowHandle();
		if( wnd == sendingWindow )
			continue;
		// When application is minimized:
		// . hide all windows
		SetWindowPos( wnd, NULL, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW );
	}
}

void SetUtilityWindowsTopmost( bool topmost )
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: SetUtilityWindowsTopmost top=%i\n", topmost);
	#endif
	ContainerWindowList::iterator it;
	for( it = s_ZUtilityWindows.begin(); it != s_ZUtilityWindows.end(); ++it ) {
		ContainerWindow* window = it->GetData();
		AssertIf( window->GetShowMode() != ContainerWindow::kShowUtility );
		HWND wnd = window->GetWindowHandle();
		SetWindowPos( wnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE );
	}
}

bool IsGfxWindowValidOnAllGUIViews()
{
	return s_IsGfxWindowValidOnAllGUIViews;
}
void ReleaseGfxWindowOnAllGUIViews()
{
	s_IsGfxWindowValidOnAllGUIViews = false;
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: ReleaseGfxWindowOnAllGUIViews\n");
	#endif
	GUIViews::iterator it, itEnd = g_GUIViews.end();
	for( it = g_GUIViews.begin(); it != itEnd; ++it ) {
		GUIView* window = *it;
		delete window->m_GfxWindow;
		window->m_GfxWindow = NULL;
		window->m_CanBlitPaint = false;
	}
}
void CreateGfxWindowOnAllGUIViews()
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: CreateGfxWindowOnAllGUIViews\n");
	#endif
	GUIViews::iterator it, itEnd = g_GUIViews.end();
	for( it = g_GUIViews.begin(); it != itEnd; ++it ) {
		GUIView* window = *it;
		AssertIf( window->m_GfxWindow );
		HWND wnd = window->GetWindowHandle();
		RECT rect;
		GetClientRect(wnd, &rect);
		window->m_GfxWindow = GetGfxDevice().CreateGfxWindow( wnd, rect.right, rect.bottom, window->m_DepthFormat, window->m_AntiAlias );
		window->m_CanBlitPaint = false;
		window->RequestRepaint();
	}
	s_IsGfxWindowValidOnAllGUIViews = true;
}

bool ResetGfxDeviceIfNeeded()
{
	// If device was in lost state, try to handle that.
	// Do NOT try to handle this periodically in WM_TIMER; in some cases it will cause
	// timer messages not be delivered anymore.
	bool success = true;
	if (IsGfxDevice())
	{
		if (!GetGfxDevice().IsValidState())
		{
			// give the system some time to recover
			::Sleep( 100 );
			if (IsGfxWindowValidOnAllGUIViews())
			{
				ReleaseGfxWindowOnAllGUIViews();
			}
			// Invalid state may be not handled even after calling HandleInvalidState, so this will be called again on the next frame
			success = GetGfxDevice().HandleInvalidState();
		}

		if (GetGfxDevice().IsValidState() && !IsGfxWindowValidOnAllGUIViews())
		{
			CreateGfxWindowOnAllGUIViews();
		}
	}
	return success;
}

void ResetGfxDeviceAndRepaint()
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: ResetGfxDeviceAndRepaint\n");
	#endif
	bool redrawWindows = ResetGfxDeviceIfNeeded();

	// Mark all views as needing repaint, and repaint if device is not lost
	GUIViews::iterator it, itEnd = g_GUIViews.end();
	for( it = g_GUIViews.begin(); it != itEnd; ++it ) {
		GUIView* window = *it;
		window->m_CanBlitPaint = false;
		if( redrawWindows )
			window->DoPaint();
	}
}

void CycleToContainerWindow( int offset )
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: CycleToContainerWindow off=%i\n", offset);
	#endif
	AssertIf( offset != -1 && offset != 1 );
	ContainerWindows::iterator it = s_ContainerWindows.find(s_LastActiveContainer);
	if( it == s_ContainerWindows.end() )
		it = s_ContainerWindows.begin();
	// nope, still can't do
	if( it == s_ContainerWindows.end() )
		return;

	if( offset > 0 ) {
		// move to next window
		++it;
		if( it == s_ContainerWindows.end() )
			it = s_ContainerWindows.begin();
	} else {
		// move to previous window
		if( it == s_ContainerWindows.begin() )
			it = s_ContainerWindows.end();
		--it;
	}

	AssertIf( it == s_ContainerWindows.end() );

	ContainerWindow* window = *it;
	::SetActiveWindow( window->GetWindowHandle() );
	s_LastActiveContainer = window;
}


void BeginHandles () {
	if (MONO_COMMON.beginHandles)
	{
		GUIState& state = GetGUIState ();
		state.m_OnGUIDepth++;
		ScriptingInvocation(MONO_COMMON.beginHandles).Invoke();
		state.m_OnGUIDepth--;
	}
}

void EndHandles () {
	if (MONO_COMMON.endHandles)
	{
		GUIState& state = GetGUIState ();
		state.m_OnGUIDepth++;
		ScriptingInvocation(MONO_COMMON.endHandles).Invoke();
		state.m_OnGUIDepth--;
	}
}

static bool HandleModifierKeyPresses( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	if( message != WM_KEYUP && message != WM_KEYDOWN && message != WM_SYSKEYUP && message != WM_SYSKEYDOWN )
		return false;

	bool isModifier = false;
	if( message == WM_KEYDOWN || message == WM_SYSKEYDOWN )
	{
		if( (lParam & (1<<30)) == 0 )
			isModifier = true;
	}
	else if( message == WM_KEYUP || message == WM_SYSKEYUP )
	{
		isModifier = true;
	}

	if( !isModifier )
		return false;

	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: CallKeyboardModifiersChanged\n");
	#endif
	if( !gAlreadyClosing )
		CallStaticMonoMethod("EditorApplication", "Internal_CallKeyboardModifiersChanged");
	return true;
}

static void GetRestoredMainWindowDimensions()
{
	s_UnmaximizedMainWindowRect.x = EditorPrefs::GetFloat("RestoredMainWindowSizeX", 0);
	s_UnmaximizedMainWindowRect.y = EditorPrefs::GetFloat("RestoredMainWindowSizeY", 0);
	s_UnmaximizedMainWindowRect.width = EditorPrefs::GetFloat("RestoredMainWindowSizeW", 0);
	s_UnmaximizedMainWindowRect.height = EditorPrefs::GetFloat("RestoredMainWindowSizeH", 0);
}

static void SaveRestoredMainWindowDimensions()
{
	EditorPrefs::SetFloat("RestoredMainWindowSizeX", s_UnmaximizedMainWindowRect.x);
	EditorPrefs::SetFloat("RestoredMainWindowSizeY", s_UnmaximizedMainWindowRect.y);
	EditorPrefs::SetFloat("RestoredMainWindowSizeW", s_UnmaximizedMainWindowRect.width);
	EditorPrefs::SetFloat("RestoredMainWindowSizeH", s_UnmaximizedMainWindowRect.height);
}

static bool IsInterestingWindowMessage( UINT message )
{
	return
		message!=WM_PAINT && message!=WM_NCPAINT &&
		message!=WM_ERASEBKGND &&
		message!=WM_MOUSEMOVE && message!=WM_NCMOUSEMOVE && message!=WM_NCMOUSELEAVE &&
		message!=WM_TIMER &&
		message!=WM_NCHITTEST && message!=WM_SETCURSOR && message!=WM_ENTERIDLE && message!=WM_GETICON;
}

static BOOL CALLBACK FocusFirstChildProc( HWND child, LPARAM lparam )
{
	SetFocus(child);
	return FALSE;
}

static void FocusFirstChild( HWND window )
{
	if( gAlreadyClosing )
		return;
	EnumChildWindows( window, FocusFirstChildProc, NULL );
}


LRESULT CALLBACK ContainerWindow::ContainerWndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	ContainerWindow *self = (ContainerWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
	if( !self ) {
		#if ENABLE_WINDOW_MESSAGES_LOGGING
		//printf_console( "EW: NULL %x ContProc: %s\n", hWnd, winutils::GetWindowsMessageInfo(message,wParam,lParam).c_str() );
		#endif
		return DefWindowProcW( hWnd, message, wParam, lParam );
	}

	#if ENABLE_WINDOW_MESSAGES_LOGGING
	//if( IsInterestingWindowMessage(message) )
	//	printf_console( "EW: %x %x ContProc: %s\n", self, hWnd, winutils::GetWindowsMessageInfo(message,wParam,lParam).c_str() );
	#endif

	switch (message) 
	{
	case WM_ENTERMENULOOP:
		self->m_InMenuLoop = true;
		return 0;
	case WM_EXITMENULOOP:
		self->m_InMenuLoop = false;
		return 0;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:

		HandleModifierKeyPresses(hWnd, message, wParam, lParam);

		if( message == WM_KEYDOWN && wParam == VK_ESCAPE ) {
			// Escape should close utility container windows
			if( self->m_ShowMode == ContainerWindow::kShowUtility ) {
				self->Close();
				return 0;
			}
		}
		if( message == WM_SYSKEYDOWN ) {
			// Alt-F4 should close window
			if( wParam == VK_F4 ) {
				SendMessage(hWnd, WM_CLOSE, 0,0);
				return 0;
			}
		}

		break;
	case WM_DEVICECHANGE:
		{
			BOOL handled;
			LRESULT const result = ProcessInputMessage(hWnd, message, wParam, lParam, handled);
			if (FALSE != handled)
				return result;
		}
		break;
	case WM_ERASEBKGND:
		{
			//printf_console("View %x: ERASE (%ix%i)\n", hWnd, rc.right, rc.bottom);
			if( GetWindowLong(hWnd, GWL_EXSTYLE) & WS_EX_LAYERED ) {
				RECT rc;
				GetClientRect(hWnd, &rc);
				FillRect( (HDC)wParam, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH) );
			}
		}
		// do not erase background
		return 1;
	/*
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps); 
			RECT rc;
			GetClientRect(hWnd, &rc);
			FillRect( hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH) );
			EndPaint(hWnd, &ps);
		}
		return 0;
	*/
	case WM_GETMINMAXINFO:
		{
			MINMAXINFO* minmax = reinterpret_cast<MINMAXINFO*>(lParam);
			// Only set the min tracking size for the main window
			// The main window has no effective maximum size and apparently setting
			// the max track size to a very large value will cover an auto-hidden taskbar
			// so we'll only set the max track size for non-main windows (they can't be maximized)
			minmax->ptMinTrackSize = self->m_MinSize;
			if (self->m_ShowMode != kShowMainWindow) {
				minmax->ptMaxSize.x = self->m_MaxMaximizedRect.right - self->m_MaxMaximizedRect.left;
				minmax->ptMaxSize.y = self->m_MaxMaximizedRect.bottom - self->m_MaxMaximizedRect.top;
				minmax->ptMaxPosition.x = 0;
				minmax->ptMaxPosition.y = 0;
				minmax->ptMaxTrackSize.x = std::min( minmax->ptMaxTrackSize.x, minmax->ptMaxSize.x );
				minmax->ptMaxTrackSize.y = std::min( minmax->ptMaxTrackSize.y, minmax->ptMaxSize.y );
			}
		}
		return 0;
	case WM_WINDOWPOSCHANGED:
		{
			if( !gAlreadyClosing && !self->m_IsClosing && !IsIconic(hWnd) )
			{
				self->OnRectChanged();
			}
		}
		break;
	case WM_SIZE:
		if ( self->m_ShowMode == ContainerWindow::kShowMainWindow)
		{
			if (IsWindowVisible(hWnd))
			{
				bool nowMaximized = wParam == SIZE_MAXIMIZED;
				if (s_IsMainWindowMaximized != nowMaximized)
					s_IsMainWindowMaximized = nowMaximized;
			}
		}
		break;
	case WM_SETFOCUS:
		return 0;
	case WM_ACTIVATEAPP:
		self->OnActivateApplication(wParam != FALSE);
		break; // fall through to main window processing

	case WM_NCACTIVATE:
		
		if( !gInitialized )
			break;

		// Here we are notified that the main window's non-client area needs to update (Non client area is e.g the title bar, menu bar, or window frame)
		// We use this event to detect when to auto-refresh assets because WM_ACTIVATEAPP is inconsistent - its not sent if
		// Unity is unresponsive e.g while stopping from playmode (Fix case 440203)

		if (wParam == FALSE)
		{
			s_LastInactivateTime = GetStartTime ();
		}
		else // wParam == TRUE)
		{
			// Detect when a Unity window is activated from outside our application 
			// and not activated coming from one of Unity's other windows. 
			if (GetElapsedTimeInSeconds (s_LastInactivateTime) > 0.3f) 
			{
				// We only refresh when the window is actually the foreground window
				if (GetForegroundWindow () == hWnd)
				{
					if( GetApplicationPtr() )
					{
						GetApplication().ResetReloadAssemblies();
						GetApplication().AutoRefresh();
					}
				}
			}
		}		
		break;

	case WM_ACTIVATE:
		if( LOWORD(wParam) == WA_INACTIVE )
			break;
		if( !gAlreadyClosing && !self->m_IsClosing )
		{
			HWND hwndPrevious = (HWND)lParam;
			if (hwndPrevious == NULL && s_LastActiveContainer	// The previous window is in another process
				&& LOWORD(wParam) != WA_CLICKACTIVE				// It's not a mouse click
				&& s_LastActiveContainer != self) 
			{
				// Alt-Tab from another application
				// Forward this active message to the previous active container.
				::SetActiveWindow(s_LastActiveContainer->GetWindowHandle());
				return 0;
			}

			s_LastActiveContainer = self;
			#if ENABLE_WINDOW_MESSAGES_LOGGING
			printf_console("EW: %x %x Cont WM_ACTIVATE\n", self, hWnd);
			#endif

			// Restore the focus to the last focused child of this container window
			if (self->GetLastFocusedChild())
				::SetFocus(self->GetLastFocusedChild());
			// Otherwise container window was just created so focus the first child
			else if (self->m_ShowMode != kShowMainWindow)
				FocusFirstChild(hWnd);

			// Put ourselves to front of Z order list
			if( self->m_ShowMode == kShowUtility )
				s_ZUtilityWindows.push_back( self->m_ContainerListNode );
			// Give the dragtab a chance to notice that windows are reordered.
			CallStaticMonoMethod ("EditorApplication", "Internal_CallWindowsReordered");
		}
		return 0;
	case WM_CLOSE:
		{
			bool isMainAndShouldExit = ( self->m_ShowMode == kShowMainWindow && !self->m_CloseFromScriptDontShutdown );
			if( isMainAndShouldExit )
			{
				if( !GetApplication().Terminate() )
					return 0;
				self->m_IsClosing = true;
				DoQuitEditor();
			}
			else
			{
				SetMenu(hWnd, NULL);
				self->CleanupContainerWindow();
				DestroyWindow( hWnd );
			}
		}
		return 0;
	case WM_DESTROY:
		{
			if (self->m_ShowMode == kShowMainWindow)
			{
				SaveRestoredMainWindowDimensions();
				EditorPrefs::SetBool(kIsMainWindowMaximized, s_IsMainWindowMaximized);
			}

			bool isMainAndShouldExit = ( self->m_ShowMode == kShowMainWindow && !self->m_CloseFromScriptDontShutdown );
			if( self->m_Window ) {
				SetWindowLongPtr( self->m_Window, GWLP_USERDATA, NULL );
				self->m_Window = NULL;
			}
			delete self;
			self = NULL;
			if( isMainAndShouldExit )
			{
				LRESULT result;
				ProcessMainWindowMessages( hWnd, message, wParam, lParam, result);
			}
		}
		return 0;
	case WM_INITMENUPOPUP:
		// Enable/disable submenu items before it is shown
		ValidateSubmenu( (HMENU)wParam );
		return 0;

	case WM_ENTERSIZEMOVE:
		// We only want to close aux windows that are rendered on top of this container window
		GUIView* guiViewOfThisContainerWindow = NULL;
		for (GUIViews::iterator i=g_GUIViews.begin(); i != g_GUIViews.end(); i++)
		{
			GUIView* view = *i;
			if (view && view->GetWindow() == self)
			{	
				guiViewOfThisContainerWindow = view;
				break;
			}
		}
		GetAuxWindowManager().CloseViews (guiViewOfThisContainerWindow); 
		return 0;
	}

	if( self->m_ShowMode == kShowMainWindow )
	{
		LRESULT result;
		if( ProcessMainWindowMessages( hWnd, message, wParam, lParam, result) ) {
			return result;
		}
	}

	return DefWindowProcW(hWnd, message, wParam, lParam);
}

bool ContainerWindow::InMenuLoop()
{
	return m_InMenuLoop;
}

void ContainerWindow::CleanupContainerWindow()
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x CleanupContainerWindow\n", this, m_Window);
	#endif
	if( s_LastActiveContainer == this ) {
		s_LastActiveContainer = NULL;
		// we might be closing ourselves from mouse down, so never get the mouse up
		if(GetApplicationPtr())
			GetApplication().ResetReloadAssemblies();
	}
	m_IsClosing = true;
	s_ContainerWindows.erase(this);
	m_ContainerListNode.RemoveFromList();

	MonoContainerWindowData* data = GetMonoContainerWindowData();
	if (data != NULL)
	{
		data->m_WindowPtr = NULL;
	}
	CallMethod ("InternalCloseWindow");
}


// Windows sends mouse wheel messages to window with focus. In our case, it makes more
// sense to send them to view under mouse (it matches behaviour of Firefox and Chrome as well, for example).
static bool RedirectMouseWheel(HWND window, WPARAM wParam, LPARAM lParam)
{
	// prevent reentrancy
	static bool s_ReentrancyCheck = false;
	if( s_ReentrancyCheck )
		return false;

	GUIView* view = GetMouseOverWindow();
	if( !view )
		return false;

	// send mouse wheel to view under mouse
	s_ReentrancyCheck = true;
	SendMessage( view->GetWindowHandle(), WM_MOUSEWHEEL, wParam, lParam );
	s_ReentrancyCheck = false;
	return true;
}


static ContainerWindow* GetContainerFromView( HWND viewWnd )
{
	HWND parent = GetParent(viewWnd);
	if( parent )
		return (ContainerWindow*)GetWindowLongPtr( parent, GWLP_USERDATA );
	else
		return NULL;
}

struct CursorBuiltinTracker
{
	char* m_ID;
	bool m_Builtin;

	CursorBuiltinTracker (char* id, bool builtin)
	{
		m_ID = id;
		m_Builtin = builtin;
	}
};

void GUIView::ProcessCursorRect( int mouseX, int mouseY )
{

	static HCURSOR kCursors[kCursorCount];
	if( !kCursors[0] ) {
		bool vistaOrLater = systeminfo::GetOperatingSystemNumeric() >= 600;
		BOOL useShadow = TRUE;
		SystemParametersInfoW( SPI_GETCURSORSHADOW, 0, &useShadow, 0 );

		#define GET_CURSOR_ID(name) vistaOrLater ? \
				(useShadow ? name##_VISTA_S : name##_VISTA) : \
				(useShadow ? name##_S : name)

		static const CursorBuiltinTracker kCursorsIds[kCursorCount] = { // Matches MouseCursor enum
			CursorBuiltinTracker (IDC_ARROW, true),	// kArrow
			CursorBuiltinTracker (IDC_IBEAM, true),	// kText
			CursorBuiltinTracker (IDC_SIZENS, true),// kResizeVertical
			CursorBuiltinTracker (IDC_SIZEWE,true),	// kResizeHorizontal
			CursorBuiltinTracker (IDC_HAND, true),	// kLink
			CursorBuiltinTracker (MAKEINTRESOURCE(GET_CURSOR_ID(IDC_SLIDE_ARROW)), false),	// kSlideArrow
			CursorBuiltinTracker (IDC_SIZENESW, true),	// kResizeUpRight
			CursorBuiltinTracker (IDC_SIZENWSE, true),	// kResizeUpLeft
			CursorBuiltinTracker (MAKEINTRESOURCE(GET_CURSOR_ID(IDC_MOVE_ARROW)), false), // kMoveArrow 
			CursorBuiltinTracker (MAKEINTRESOURCE(GET_CURSOR_ID(IDC_ROTATE_ARROW)), false), // kRotateArrow
			CursorBuiltinTracker (MAKEINTRESOURCE(GET_CURSOR_ID(IDC_SCALE_ARROW)), false), // kScaleArrow
			CursorBuiltinTracker (MAKEINTRESOURCE(GET_CURSOR_ID(IDC_PLUS_ARROW)), false), // kArrowPlus
			CursorBuiltinTracker (MAKEINTRESOURCE(GET_CURSOR_ID(IDC_MINUS_ARROW)), false), // kArrowMinus
			CursorBuiltinTracker (MAKEINTRESOURCE(GET_CURSOR_ID(IDC_PAN_VIEW)), false), // kPan
			CursorBuiltinTracker (MAKEINTRESOURCE(GET_CURSOR_ID(IDC_ORBIT_VIEW)), false), // kOrbit
			CursorBuiltinTracker (MAKEINTRESOURCE(GET_CURSOR_ID(IDC_ZOOM_VIEW)), false), // kZoom
			CursorBuiltinTracker (MAKEINTRESOURCE(GET_CURSOR_ID(IDC_FPS_VIEW)), false), // kFPS
			CursorBuiltinTracker (IDC_ARROW, true), // kCustomCursor -- default to arrow...
			CursorBuiltinTracker (IDC_SIZENS, true),// kSplitResizeUpDown (same as kResizeVertical)
			CursorBuiltinTracker (IDC_SIZEWE, true)	// kSplitResizeLeftRight (same as kResizeHorizontal)
		};

		HMODULE instance = GetModuleHandleW(NULL);
		for( int i = 0; i < kCursorCount; ++i ) {
			kCursors[i] = ::LoadCursor( kCursorsIds[i].m_Builtin ? NULL : instance, kCursorsIds[i].m_ID);
		}
	}

	int hotControlID = IMGUI::GetHotControl(GetGUIState());
	int newCursor = kArrow;
	size_t n = m_CursorRects.size();
	for( size_t i = 0; i  < n; ++i ) {
		const CursorRect& rect = m_CursorRects[i];
		if( rect.position.Contains(mouseX, mouseY) || (hotControlID == rect.controlID && rect.controlID > 0) ) {
			newCursor = rect.mouseCursor;
			break;
		}
	}

	if (newCursor == kCustomCursor)
		::SetCursor (Cursors::GetHardwareCursor ());
	else
		::SetCursor (kCursors[newCursor]);
}

void GUIView::ProcessEventMessages( UINT message, WPARAM wParam, LPARAM lParam )
{
	// process custom cursor rectangles on mouse move
	if( message == WM_MOUSEMOVE )
	{
		
		float xpos = GET_X_LPARAM(lParam);
		float ypos = GET_Y_LPARAM(lParam);
		ProcessCursorRect(xpos, ypos);
	}


	if( message == WM_MOUSEMOVE && !m_WantsMouseMove )
	{
		bool anyButtonPressed = (wParam & (MK_LBUTTON|MK_RBUTTON|MK_MBUTTON)) != 0;
		if( !anyButtonPressed )
			return;
	}

	UpdateScreenManager();
	HandleModifierKeyPresses(m_View, message, wParam, lParam);

	InputEvent ie(message, wParam, lParam, m_View);


	if( ie.type == InputEvent::kMouseDown )
	{

		GetApplication().LockReloadAssemblies();

		// Check if any popup aux windows steal mousedown
		if (GetAuxWindowManager().OnMouseDown (this))
			return;

		if( ie.clickCount == 1 ) 
		{
			m_HasMouseCapture = true;
			SetCapture( m_View );
		}
	}
	else if( ie.type == InputEvent::kMouseUp )
	{
		if( m_HasMouseCapture ) {
			// set m_HasMouseCapture to false before ReleaseCapture(), so that from inside
			// WM_CAPTURECHANGED we don't try to simulate a mouse up event.
			m_HasMouseCapture = false;
			ReleaseCapture();
		}
		GetApplication().UnlockReloadAssemblies();
	}
	// When a keyboard key is pressed we get TWO keydown events - first one with a keyCode and then one with a character.
	// Make sure we only focus the current window on the first one. Otherwise opening windows on a keydown event with
	// keycode breaks down because the old window is re-focused after the new window is opened.
	// So we only handle keydown events if the keycode is set.
	if( ie.type == InputEvent::kMouseDown || (ie.type == InputEvent::kKeyDown && ie.keycode > 0))
	{
		s_LastActiveView = this;
		if( GetFocus() != m_View )
		{
			SetFocus(m_View);
			// Setting focus to this view might have generated other GUI events (e.g. other view loses
			// focus, generates command events on that etc.). So update screen manager & GUI offset
			// after that.
			UpdateScreenManager();
		}
	}

	// Handle automatic undo grouping AFTER the event has been sent to managed land so GUI code has a chance to clean up dragging state on Escape pressed
	switch (message)
	{
		// keyboard is handled in ContainerWndProc
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_KEYDOWN:
			{
				// If the key pressed is being repeated ignore it
				if ( message != WM_KEYDOWN || (lParam & 0x40000000) == 0)
					HandleAutomaticUndoGrouping(ie);
			}
			break;
	}

	bool processed = OnInputEvent(ie);
	if( !processed && (ie.type == InputEvent::kKeyDown || ie.type == InputEvent::kKeyUp) )
		CallGlobalInputEvent(ie);

	HandleAutomaticUndoOnEscapeKey (ie, processed);

	if( !processed && message == WM_RBUTTONUP ) {
		ie.type = InputEvent::kContextClick;
		UpdateScreenManager();
		OnInputEvent(ie);
	}
	if( !processed ) {
		if( message == WM_CHAR ) {
			if( ToLower((char)ie.character) == 'f') {
				if(ie.modifiers & InputEvent::kShift) {
					ExecuteStandardMenuItem(ID_EDIT_FRAMESELECTEDWITHLOCK);
				} else if(ie.modifiers == 0) {
					ExecuteStandardMenuItem(ID_EDIT_FRAMESELECTED);
				}
			}
		}
		else if( message == WM_KEYDOWN ) {
			if( (ie.modifiers & ~InputEvent::kFunctionKey) == 0 ) {
				if( ie.keycode == SDLK_DELETE ) {
					ExecuteStandardMenuItem(ID_EDIT_SOFTDELETE);
				}
				// Escape should close utility container windows
				if( ie.keycode == SDLK_ESCAPE ) {
					ContainerWindow* container = GetContainerFromView(m_View);
					if( container && container->GetShowMode() == ContainerWindow::kShowUtility ) {
						container->Close();
					}
				}
			}
		} else if( message == WM_SYSKEYDOWN	) {
			// Alt-F4 should close window
			if( (ie.modifiers & InputEvent::kAlt) == InputEvent::kAlt && ie.keycode == SDLK_F4 ) {
				HWND parent = GetParent(m_View);
				if( parent )
					SendMessage(parent, WM_CLOSE, 0,0);
			}
		}
	}
}

void GUIView::MakeVistaDWMHappyDance()
{
	// Looks like Vista has some bug in DWM. Whenever we maximize or dock a view, we must do something magic, otherwise
	// white stuff appears in place of the view.
	// See http://forums.microsoft.com/MSDN/ShowPost.aspx?PostID=4208117&SiteID=1
	
	bool earlierThanVista = systeminfo::GetOperatingSystemNumeric() < 600;
	if( earlierThanVista )
		return;

	// What seems to work is drawing one pixel via GDI.
	// We draw it at (1,1) with usual background color.
	int grayColor = kGUIViewClearColor * 255.0f;
	PAINTSTRUCT ps;
	BeginPaint(m_View, &ps);
	SetPixel(ps.hdc, 1, 1, RGB(grayColor,grayColor,grayColor));
	EndPaint(m_View, &ps);
}


LRESULT CALLBACK GUIView::GUIViewWndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	GUIView *self = (GUIView*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
	if( !( self && self->m_Window ) ) {
		#if ENABLE_WINDOW_MESSAGES_LOGGING
		//printf_console( "EW: NULL %x ViewProc: %s\n", hWnd, winutils::GetWindowsMessageInfo(message,wParam,lParam).c_str() );
		#endif
		return DefWindowProcW( hWnd, message, wParam, lParam );
	}

	LRESULT result;
	if( ProcessIMEMessages( hWnd, message, wParam, lParam, result))
		return result;

	#if ENABLE_WINDOW_MESSAGES_LOGGING
	//if( IsInterestingWindowMessage(message) )
	//	printf_console( "EW: %x %x ViewProc: %s\n", self, hWnd, winutils::GetWindowsMessageInfo(message,wParam,lParam).c_str() );
	#endif

	bool processInput = (!self->m_GameViewRect.IsEmpty() && IsWorldPlaying() && !self->m_Window->InMenuLoop());
	bool focused = (GetFocus() == hWnd);

	if( processInput && focused )
	{
		BOOL handled = FALSE;
		ProcessInputMessage(hWnd, message, wParam, lParam, handled);
	}

	switch (message) 
	{
	case WM_ERASEBKGND:
		// do not erase background
		/*
		{
			RECT rc;
			GetClientRect(hWnd, &rc);
			printf_console("View %x: ERASE (%ix%i)\n", hWnd, rc.right, rc.bottom);
			FillRect( (HDC)wParam, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH) );
		}
		*/
		return 1;
	case WM_MOUSEWHEEL:
		{
			// quick check if mouse is in our window
			RECT rc;
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			GetWindowRect(hWnd, &rc);
			if( !PtInRect(&rc, pt) )
			{
				if( RedirectMouseWheel(hWnd, wParam, lParam) )
					return 0;
			}
			// Otherwise, mouse is in our window. Handle like a regular message.
			AssertIf( self->m_View != hWnd );
			self->ProcessEventMessages( message, wParam, lParam );
		}
		return 0;

	case WM_CAPTURECHANGED:
		/*
		if( self->m_HasMouseCapture && (HWND)lParam != hWnd ) {
			// Lost mouse capture while holding it (e.g. user alt-tabbed).
			self->m_HasMouseCapture = false;
			// Simulate a mouse up event!
			self->ProcessEventMessages( WM_LBUTTONUP, 0, 0 );
		}
		*/
		return 0;

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDBLCLK:
	case WM_KEYUP:
	case WM_KEYDOWN:
	case WM_SYSKEYUP:
	case WM_SYSKEYDOWN:
	case WM_CHAR:
	case WM_MOUSEMOVE:
		AssertIf( self->m_View != hWnd );
		switch (message)
		{
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_XBUTTONDOWN:
				{
					SetFocus(hWnd);

					if (processInput && !focused)
					{
						focused = (GetFocus() == hWnd);

						if (focused)
						{
							BOOL handled = FALSE;
							ProcessInputMessage(hWnd, message, wParam, lParam, handled);
						}
					}
				}
				break;
		}
		self->ProcessEventMessages( message, wParam, lParam );
		return 0;
	case WM_SETCURSOR:
		// We're setting the cursor on every WM_MOUSEMOVE. We eat the WM_SETCURSOR event here so the OS doesn't conflict with us causing cursor garbage flicker.
		return 1;
	case WM_WINDOWPOSCHANGED:
		{
			if( self->m_GfxWindow )
			{
				RECT rect;
				GetClientRect(hWnd, &rect);
				if( self->m_GfxWindow->GetWidth() != rect.right || self->m_GfxWindow->GetHeight() != rect.bottom )
				{
					self->m_GfxWindow->Reshape( rect.right, rect.bottom, self->m_DepthFormat, self->m_AntiAlias );
					self->RequestRepaint();
				}

				// Update the scene so that scripts marked with [ExecuteInEditMode] are able to react to screen size changes
				GetApplication().SetSceneRepaintDirty ();
			}
		}
		return 0;
	case WM_SETFOCUS:
		if (s_LastActiveView != self && IsInputInitialized())
			InputSetWindow (hWnd, GetScreenManager().IsFullScreen());
		s_LastActiveView = self;
		InputActivate();
		self->GotFocus();
		return 0;
	case WM_KILLFOCUS:
		InputPassivate();
		self->LostFocus();
		if (self->m_Window)
			self->m_Window->SetLastFocusedChild(hWnd);
		return 0;
	case WM_ACTIVATE:
		// The WM_SETFOCUS event should be enough.
		// 
		// By the way, we can't receive WM_ACTIVATE maybe because GUIView is
		// created with the style WS_CHILD. (Not mentioned in MSDN, tested with
		// Spy++ on Win7)
		break;
	case WM_PAINT:
		if (!gAlreadyClosing)
		{
			self->DoPaint();
			ValidateRect(hWnd, NULL);
		}
		return 0;
	case WM_CLOSE:
		self->CleanupGUIView();
		delete self;
		DestroyWindow( hWnd );

		return 0;

	case WM_INITMENUPOPUP:
		ValidateSubmenu( (HMENU)wParam );
		return 0;
	}

	return DefWindowProcW(hWnd, message, wParam, lParam);
}

void GUIView::DoPaint(void)
{
	if( m_GfxWindow && m_GfxWindow->BeginRendering() )
	{
		GPU_TIMESTAMP();
		// if parent is set to be transparent, don't do actual paint to the screen
		ContainerWindow* container = GetContainerFromView( m_View );
		if( container ) {
			DWORD exstyle = GetWindowLong(container->GetWindowHandle(), GWL_EXSTYLE);
			if( exstyle & WS_EX_LAYERED )
				m_Transparent = true;
		}

		RECT rc;
		GetClientRect( m_View, &rc );
		//printf_console("View %x: PAINT full (%i %i %ix%i)\n", m_View, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top);
		UpdateScreenManager();
		AssertIf(rc.right != m_GfxWindow->GetWidth() || rc.bottom != m_GfxWindow->GetHeight());

		#if DISABLE_BLIT_PAINT_OPTIMIZATION
		m_CanBlitPaint = false;
		#endif

		if( !m_CanBlitPaint || !m_GfxWindow->CanUseBlitOptimization() )
		{
			m_CanBlitPaint = true;

			ClearCursorRects();

			// Clear back buffer
			GfxDevice& device = GetGfxDevice();
			device.SetViewport( 0, 0, rc.right-rc.left, rc.bottom-rc.top );
			device.DisableScissor();
			device.SetWireframe(false);
			
			//Never set sRGB write for 'normal' windows
			device.SetSRGBWrite(false);

			// Feed input event
			bool oldTextFocus = GetInputManager().GetTextFieldInput();
			GetInputManager().SetTextFieldInput(false);
			OnInputEvent( InputEvent::RepaintEvent(m_View) );
			if (GetKeyGUIView() != this)
				GetInputManager().SetTextFieldInput(oldTextFocus);
		}
		else
		{
			GPU_TIMESTAMP();
			m_GfxWindow->EndRendering( !m_Transparent );
			#if ENABLE_VIEW_SHOT_DUMP
			++s_ShotFrames;
			QueueScreenshot(Format("paneshot-%05i-%x-e.png", s_ShotFrames, m_View));
			UpdateCaptureScreenshot();
			#endif
			//printf_console("View %x: PAINT blit\n", m_View);
		}
	}
	else
	{
		//printf_console("View %x: PAINT later\n", m_View);
		RequestRepaint ();
	}
	//Sleep(50);
}

static ATOM s_ContainerWindowClassAtom;
static ATOM s_PopupWindowClassAtom;
static ATOM s_GUIViewClassAtom;

void InitializeEditorWindowClasses()
{
	s_ContainerWindowClassAtom = winutils::RegisterWindowClass( kContainerWindowClassName, ContainerWindow::ContainerWndProc, CS_HREDRAW | CS_VREDRAW );
	s_PopupWindowClassAtom = winutils::RegisterWindowClass( kPopupWindowClassName, ContainerWindow::ContainerWndProc, CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW );
	s_GUIViewClassAtom = winutils::RegisterWindowClass( kGUIViewWindowClassName, GUIView::GUIViewWndProc, CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS );
}
void ShutdownEditorWindowClasses()
{
	winutils::UnregisterWindowClass( kContainerWindowClassName );
	winutils::UnregisterWindowClass( kPopupWindowClassName );
	winutils::UnregisterWindowClass( kGUIViewWindowClassName );
	s_ContainerWindowClassAtom = 0;
	s_PopupWindowClassAtom = 0;
	s_GUIViewClassAtom = 0;
}

HWND CreateMainWindowForBatchMode()
{
	ContainerWindow* container = new ContainerWindow();
	container->Init( NULL, Rectf(0,0,0,0), ContainerWindow::kShowMainWindow, Vector2f::zero, Vector2f::zero );
	return container->GetWindowHandle();
}



void GUIView::CleanupGUIView()
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x CleanupGUIView\n", this, m_View);
	#endif
	
	if( s_LastActiveView == this ) {
		s_LastActiveView = NULL;
		// we might be closing ourselves from mouse down, so never get the mouse up
		if(GetApplicationPtr())
			GetApplication().ResetReloadAssemblies();
	}

	GetDragAndDrop().UnregisterWindowForDrop( m_View, m_DropData );
	m_DropData = NULL;

	if (m_View)
	{
		// If we are hosting a webkit inside, tell it to go away before we destroy the window
		HWND wrapperWindow = FindWindowExW(m_View, NULL, WebViewWrapper::GetWindowClassName().c_str(), NULL);
		if (wrapperWindow) {
			WebViewWrapper* wrapper = (WebViewWrapper*) GetWindowLongPtr( wrapperWindow, GWLP_USERDATA );
			if ( wrapper )
				wrapper->Unparent();
		}
		
		// Clear userdata from the window handle
		SetWindowLongPtr( m_View, GWLP_USERDATA, NULL );
	}
	if( m_GfxWindow && m_InsideContext )
	{
		m_GfxWindow->EndRendering(false);
		m_InsideContext = false;
	}
	delete m_GfxWindow;
	m_GfxWindow = NULL;
	m_View = NULL;
}

void GUIView::DragEvent( int x, int y, DWORD keyFlags, int eventType )
{
	InputEvent event( x, y, keyFlags, eventType, m_View );
	UpdateScreenManager();
	OnInputEvent( event );
}

void ContainerWindow::MoveInFrontOf(ContainerWindow *other)
{
}

void ContainerWindow::MoveBehindOf(ContainerWindow *other)
{
}

void ContainerWindow::Init (MonoBehaviour* behaviour, Rectf pixelRect, int showMode, const Vector2f& minSize, const Vector2f& maxSize)
{
	// Aux windows are mac only. on windows they look just like normal utility windows.
	if (showMode == kShowAuxWindow)
		showMode = kShowUtility;

	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x ContainerInit mode=%i\n", this, showMode);
	#endif
	m_ShowMode = static_cast<ShowMode>(showMode);
	m_IsClosing = false;
	m_InMenuLoop = false;
	m_CloseFromScriptDontShutdown = false;

	bool shouldMaximize = false;
	if (showMode == kShowMainWindow)
	{
		shouldMaximize = s_IsMainWindowMaximized = EditorPrefs::GetBool(kIsMainWindowMaximized, false);
		GetRestoredMainWindowDimensions();
	}

	RECT rect = { 20, 20, 220, 220 };
	m_InternalRect.x = m_InternalRect.y = m_InternalRect.width = m_InternalRect.height = 0.0f;

	DWORD windowStyle = 0;
	DWORD extendedStyle = 0;
	LPCWSTR windowClass = kContainerWindowClassName;

	switch (showMode) {
	case kShowNormalWindow:
		windowStyle = WS_POPUP | WS_CLIPCHILDREN | WS_THICKFRAME;
		extendedStyle = WS_EX_TOOLWINDOW;
		break;
	case kShowMainWindow:
		windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
		extendedStyle = 0;
		break;
	case kShowPopupMenu:
		windowStyle = WS_POPUP | WS_CLIPCHILDREN;
		extendedStyle = WS_EX_TOOLWINDOW;
		windowClass = kPopupWindowClassName;
		break;
	case kShowNoShadow:
		windowStyle = WS_POPUP | WS_CLIPCHILDREN;
		extendedStyle = WS_EX_TOOLWINDOW;
		break;
	case kShowUtility:
		windowStyle = WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU;
		extendedStyle = WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW;
		break;
	default:
		ErrorString("Unknown container show mode");
		break;
	}

	HWND parentWindow;
	if (showMode == kShowMainWindow)
	{
		parentWindow = NULL;
	}
	else
	{
		parentWindow = GetMainEditorWindow();
	}
	
	bool notSizeable = (minSize==maxSize) && (minSize!=Vector2f::zero);
	if (notSizeable)
		windowStyle &= ~(WS_THICKFRAME);

	AdjustWindowRectEx( &rect, windowStyle, showMode == kShowMainWindow, extendedStyle );
	int extraX = rect.right-rect.left-200;
	int extraY = rect.bottom-rect.top-200;

	m_MinSize.x = minSize.x + extraX;
	m_MinSize.y = minSize.y + extraY;
	m_MaxSize.x = maxSize.x + extraX;
	m_MaxSize.y = maxSize.y + extraY;


	if( showMode == kShowUtility )
		s_ZUtilityWindows.push_back( m_ContainerListNode );
	// Create window
	m_Window = CreateWindowExW( extendedStyle, windowClass, L"", windowStyle,
		rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top,
		parentWindow, NULL, winutils::GetInstanceHandle(), NULL );
	SetWindowLongPtr( m_Window, GWLP_USERDATA, (LONG_PTR)this );
	UpdateMaxMaximizedRect();

	if( pixelRect.width > 10 || pixelRect.height > 10 )
	{
		SetRect(pixelRect);
		if (shouldMaximize)
			SetWindowLong(m_Window, GWL_STYLE, windowStyle | WS_MAXIMIZE);
	}

	m_Instance = behaviour;

	if( showMode == kShowMainWindow )
	{
		SetMainEditorWindow( m_Window );
		HMENU menu = GetMainMenuHandle();
		if( menu )
			SetMenu( m_Window, menu );
		GetApplication().UpdateMainWindowTitle ();
		GetApplication().UpdateDocumentEdited ();
	}

	s_ContainerWindows.insert(this);

	ShowInTaskbarIfNoMainWindow(m_Window);
}

ContainerWindow::ContainerWindow ()
:	m_ContainerListNode(this)
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x Container\n", this);
	#endif
	m_Instance = NULL;
	m_Window = NULL;
	m_LastFocusedChild = NULL;
}

ContainerWindow::~ContainerWindow ()
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x ~Container\n", this);
	#endif

	// When manually closing main window on Windows, the cleanup is different
	// than in normal cases; and the window data is still there.
	if (m_ShowMode != kShowMainWindow)
	{
		// Don't make this a full assert because 
		// it locks up the persistent manager when doing threaded background loading
		DebugAssertIf (!gAlreadyClosing && GetMonoContainerWindowData());
	}
	s_ContainerWindows.erase(this);
}

static bool PointIsOnInvalidScreen(int x, int y)
{
	Vector2f pt;
	pt.x = x;
	pt.y = y;
	Rectf bounds = GetScreenManager().GetBoundsOfDesktopAtPoint(pt);
	return bounds.x > x || bounds.y > y || bounds.x + bounds.width < x || bounds.y + bounds.height < y;
}

Rectf ContainerWindow::FitWindowRectToScreen (const Rectf &rect, bool useMouseScreen, bool forceCompletelyVisible)
{
	if (::IsZoomed(m_Window))
		return rect;

	Rectf r = rect;

	Vector2f pos;
	if (useMouseScreen)
	{
		POINT pointpos;
		GetCursorPos(&pointpos);
		pos.x = pointpos.x;
		pos.y = pointpos.y;
	}
	else
	{
		// Windows itself uses the center of the window to decide which monitor window is actually on when it spans multiple monitors
		pos.x = r.x + r.width / 2;
		pos.y = r.y + r.height / 2;
	}

	// client rect to window rect
	int borderX = ::GetSystemMetrics(SM_CXFRAME);
	int borderY = ::GetSystemMetrics(SM_CYFRAME);

	Rectf bounds = GetScreenManager().GetBoundsOfDesktopAtPoint(pos);

	// window to client rect
	bounds.x += borderX; 
	bounds.y += borderY;
	bounds.width -= borderX * 2;
	bounds.height -= borderY * 2;

	// bound bottom right
	if( r.x + r.width > bounds.x + bounds.width )
		r.x = bounds.x + bounds.width - r.width;
	if( r.y + r.height > bounds.y + bounds.height )
		r.y = bounds.y + bounds.height - r.height;

	// bound top left
	if( r.x < bounds.x )
		r.x = bounds.x;
	if( r.y < bounds.y)
		r.y = bounds.y;

	// bound size if needed
	if (forceCompletelyVisible && r.width > bounds.width)
		r.width  = bounds.width;
	if (forceCompletelyVisible && r.height > bounds.height)
		r.height = bounds.height;

	return r;
}

void ContainerWindow::GetOrderedWindowList ()
{
	#if ENABLE_DRAGTAB_DEBUG
	printf_console("GetOrdererWindowList\n");
	#endif
	DWORD thisProcessId = ::GetCurrentProcessId();
	HWND window = ::GetTopWindow( NULL );
	while( window != NULL )
	{
		DWORD windowProcessId;
		::GetWindowThreadProcessId(window, &windowProcessId);
		if( windowProcessId == thisProcessId )
		{
			WINDOWINFO info;
			info.cbSize = sizeof(info);
			if( ::GetWindowInfo( window, &info ) )
			{
				if( info.atomWindowType == s_ContainerWindowClassAtom )
				{
					ContainerWindow* containerWindow = (ContainerWindow*)::GetWindowLongPtr( window, GWLP_USERDATA );
					if( containerWindow )
						containerWindow->CallMethod("AddToWindowList");
				}
			}
		}
		window = GetNextWindow( window, GW_HWNDNEXT );
	}
}

void GUIView::SetAsActiveWindow()
{
	if( m_GfxWindow )
		m_GfxWindow->SetAsActiveWindow();
}

void GUIView::BeginCurrentContext()
{
	if( m_GfxWindow )
	{
		Assert( !m_InsideContext );
		m_InsideContext = true;
		m_GfxWindow->BeginRendering();
	}
}

void GUIView::EndCurrentContext()
{
	if( m_GfxWindow )
	{
		Assert( m_InsideContext );
		m_InsideContext = false;
		m_GfxWindow->EndRendering( false );
	}
}
Vector2f GUIView::GetSize()
{
	RECT rc; 
	if (GetClientRect(GetWindowHandle(), &rc))
	{
		return Vector2f((float)(rc.right - rc.left), (float)(rc.bottom - rc.top));
	}
	return Vector2f(0.0f, 0.0f);
}
void GUIView::ForceRepaint()
{
	RequestRepaint();
	DoPaint();
}


Vector2f ContainerWindow::GetTopleftScreenPosition ()
{
	POINT pt = { 0, 0 };
	ClientToScreen( m_Window, &pt );
	return Vector2f( pt.x, pt.y );
}


void ContainerWindow::SetTitle (const string &title)
{
	if( m_ShowMode != kShowMainWindow )
	{
		std::wstring wideTitle;
		ConvertUTF8ToWideString( title, wideTitle );
		SetWindowTextW( m_Window, wideTitle.c_str() );
	}
}

static BOOL CALLBACK BringLiveChildProc( HWND child, LPARAM lparam )
{
	ShowWindow( child, SW_NORMAL );
	return TRUE;
}

void ContainerWindow::BringLiveAfterCreation (bool displayNow, bool setFocus)
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x ContBringLive display=%i\n", this, m_Window, displayNow);
	#endif
	EnumChildWindows( m_Window, BringLiveChildProc, NULL );
	DWORD flags = SWP_NOOWNERZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW;
	if( !setFocus )
		flags |= SWP_NOACTIVATE;
	SetWindowPos( m_Window, HWND_TOP, 0,0,0,0, flags );
}

void ContainerWindow::SetFreezeDisplay (bool freeze)
{
//	if (freeze)
//		NSDisableScreenUpdates();
//	else
//		NSEnableScreenUpdates();
}

static BOOL CALLBACK DisplayAllViewsChildProc( HWND child, LPARAM lparam )
{
	InvalidateRect( child, NULL, FALSE );
	return TRUE;
}

void ContainerWindow::DisplayAllViews ()
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x ContDisplayAllViews=%i\n", this, m_Window);
	#endif
	EnumChildWindows( m_Window, DisplayAllViewsChildProc, NULL );
}

static BOOL CALLBACK EnableAllViewsChildProc( HWND child, LPARAM lparam )
{
	EnableWindow(child, lparam);
	return TRUE;
}

void ContainerWindow::EnableAllViews (bool enable)
{
	EnumChildWindows( m_Window, EnableAllViewsChildProc, enable );
}

void ContainerWindow::SetAlpha (float alpha)
{
}

void ContainerWindow::SetInvisible ()
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x ContSetInvisible\n", this, m_Window);
	#endif
	m_ContainerListNode.RemoveFromList();

	// Ok, setting layered attribute makes the window repaint immediately. Which is bad
	// because it causes recursive GUI events. So hack it out.
	s_DontSendEventsHack = true;

	SetWindowLong( m_Window, GWL_EXSTYLE, GetWindowLong(m_Window, GWL_EXSTYLE) | WS_EX_LAYERED);
	// A completely transparent window won't get mouse events either. So make the window be almost transparent...
	SetLayeredWindowAttributes( m_Window, 0, 1, LWA_ALPHA );

	// also make it topmost so we can grab taskbar & similar stuff
	SetWindowPos( m_Window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );

	s_DontSendEventsHack = false;
}

void ContainerWindow::SetMinMaxSizes (const Vector2f &minSize, const Vector2f &maxSize)
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x ContSetMinMax\n", this, m_Window);
	#endif

	// The passed sizes are client area sizes. We must take any window decorations into account here.
	//printf_console("Container %x set min %i,%i max %i,%i\n", this, (int)minSize.x, (int)minSize.y, (int)maxSize.x, (int)maxSize.y);

	DWORD windowStyle = GetWindowLong(m_Window,GWL_STYLE);
	DWORD originalStyle = windowStyle;
	bool notSizeable = (minSize==maxSize) && (minSize!=Vector2f::zero);
	if (notSizeable)
		windowStyle &= ~WS_THICKFRAME;
	if( windowStyle != originalStyle ) {
		SetWindowLong(m_Window, GWL_STYLE, windowStyle);
		SetWindowPos(m_Window, NULL, 0,0,0,0, SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED );
	}

	RECT rc = { 0, 0, 10, 10 };
	AdjustWindowRectEx( &rc, windowStyle, m_ShowMode==kShowMainWindow, GetWindowLong(m_Window, GWL_EXSTYLE) );
	int extraX = rc.right-rc.left-10;
	int extraY = rc.bottom-rc.top-10;

	m_MinSize.x = minSize.x + extraX;
	m_MinSize.y = minSize.y + extraY;
	m_MaxSize.x = maxSize.x + extraX;
	m_MaxSize.y = maxSize.y + extraY;

	if( GetWindowRect(m_Window, &rc) ) {
		bool changed = false;
		int width = rc.right - rc.left;
		int height = rc.bottom - rc.top;
		width = clamp<int>( width, m_MinSize.x, m_MaxSize.x );
		height = clamp<int>( height, m_MinSize.y, m_MaxSize.y );
		if( width != rc.right - rc.left || height != rc.bottom - rc.top ) {
			SetWindowPos( m_Window, NULL, rc.left, rc.top, width, height, SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_SHOWWINDOW|SWP_NOCOPYBITS );
		}
	}
}

void ContainerWindow::MakeModal()
{
	::EnableAllViews(false);
	EnableWindow(GetMainEditorWindow(), false);
	this->EnableAllViews(true);
	ModalWinMainMessageLoop(this);
	EnableWindow(GetMainEditorWindow(), true);
	::EnableAllViews(true);
}

void ContainerWindow::SetIsDragging(bool isDragging)
{
}

bool ContainerWindow::IsZoomed()
{
	//@might want to implement this at some point for consistency. Right now, it's only needed for the mac side.
	return false;
}

void ContainerWindow::SetRect (const Rectf &rect)
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x ContSetRect %i,%i %ix%i\n", this, m_Window, (int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);
	#endif

	// Check if we're just moving
	RECT oldClientRC;
	GetClientRect( m_Window, &oldClientRC );
	bool isResizing = oldClientRC.right != rect.width || oldClientRC.bottom != rect.height;
	//printf_console("wnd %x old %ix%i new %ix%i %s\n", m_Window, oldClientRC.right, oldClientRC.bottom, (int)rect.width, (int)rect.height, isResizing ? "YES" : "");

	// Passed rectangle indicates desired client area, adjust to get window area.
	RECT rc;
	rc.left = rect.x;
	rc.top = rect.y;
	rc.right = rect.x + rect.width;
	rc.bottom = rect.y + rect.height;
	AdjustWindowRectEx( &rc, GetWindowLong(m_Window, GWL_STYLE), m_ShowMode==kShowMainWindow, GetWindowLong(m_Window, GWL_EXSTYLE) );

	if( isResizing ) {
		GetScreenManager().BoundRectangleToDesktops(rc);
	}

	// enforce min/max size
	if( rc.right-rc.left < m_MinSize.x )
		rc.right = rc.left + m_MinSize.x;
	if( rc.bottom-rc.top < m_MinSize.y )
		rc.bottom = rc.top + m_MinSize.y;
	if( rc.right-rc.left > m_MaxSize.x )
		rc.right = rc.left + m_MaxSize.x;
	if( rc.bottom-rc.top > m_MaxSize.y )
		rc.bottom = rc.top + m_MaxSize.y;

	SetWindowPos( m_Window, NULL, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_NOCOPYBITS );
}

Rectf ContainerWindow::GetRect () const
{
	RECT rc;
	if( GetClientRect( m_Window, &rc ) )
	{
		POINT pt = {0,0};
		ClientToScreen( m_Window, &pt );
		return Rectf( pt.x, pt.y, rc.right-rc.left, rc.bottom-rc.top );
	}
	else
	{
		AssertString("Failed to get window rect");
		return Rectf(10,10,8,8);
	}
}

void ContainerWindow::UpdateMaxMaximizedRect ()
{
	HMONITOR monitor = MonitorFromWindow( m_Window, MONITOR_DEFAULTTOPRIMARY );
	if( monitor ) {
		MONITORINFO info;
		info.cbSize = sizeof(info);
		GetMonitorInfo( monitor, &info );
		m_MaxMaximizedRect = info.rcWork;
	}
}

void ContainerWindow::OnRectChanged()
{
	Rectf rect = GetRect();

	MonoContainerWindowData* data = GetMonoContainerWindowData();

	if (data)
	{
		data->m_PixelRect = rect;

		if (m_ShowMode == kShowMainWindow)
		{
			long style = GetWindowLong(m_Window, GWL_STYLE);
			if (!(WS_MAXIMIZE & style) && !(WS_MINIMIZE & style))
			{
				// catch unmaximizing window, restore unmaximized dimensions
				if (s_IsMainWindowMaximized)
				{
					if (s_UnmaximizedMainWindowRect.width != 0 && s_UnmaximizedMainWindowRect.height != 0)
					{
						if (PointIsOnInvalidScreen(s_UnmaximizedMainWindowRect.x, s_UnmaximizedMainWindowRect.y))
							s_UnmaximizedMainWindowRect = FitWindowRectToScreen(s_UnmaximizedMainWindowRect, false, true);

						SetRect(s_UnmaximizedMainWindowRect);
						data->m_PixelRect = s_UnmaximizedMainWindowRect;
						rect = data->m_PixelRect;
					}
				}
				else
					// unmaximized window is sized/moved, save last known rect
					s_UnmaximizedMainWindowRect = rect;
			}
		}
	}

	if( rect == m_InternalRect )
		return;
	m_InternalRect = rect;

	UpdateMaxMaximizedRect();
	CallMethod ("OnResize");
}

void ContainerWindow::Close()
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x ContClose\n", this, m_Window);
	#endif
	m_CloseFromScriptDontShutdown = true;
	SendMessage(m_Window, WM_CLOSE, 0,0);
}

void ContainerWindow::Minimize () 
{
}

void ContainerWindow::ToggleMaximize () 
{
	UpdateMaxMaximizedRect ();
	ShowWindow( m_Window, ::IsZoomed(m_Window) ? SW_NORMAL : SW_SHOWMAXIMIZED );
}

GUIView* GetKeyGUIView ()
{
	if( s_LastActiveContainer && s_LastActiveView )
		return s_LastActiveView;
	return NULL;
}


GUIView* GUIView::GetCurrent () {
	return s_CurrentView;
}

MonoBehaviour* GUIView::GetCurrentMonoView () {
	if (s_CurrentView)
		return s_CurrentView->m_Instance;
	else
		return NULL;
}

void GUIView::RequestClose ()
{
	HWND parent = GetParent (GetWindowHandle ());
	if( parent )
	{
		SendMessage(parent, WM_CLOSE, 0,0);
	}
}

void GUIView::SendLayoutEvent (GUIState &state) {
	InputEvent::Type originalType = state.m_CurrentEvent->type;

	#if ENABLE_CPU_TIMER
	CPUTimer timerEvent(originalType==InputEvent::kRepaint ? "Repaint layout" : "Other layout");
	#endif
	state.m_CurrentEvent->type = InputEvent::kLayout;
	BeginHandles ();
	m_Instance->DoGUI(MonoBehaviour::kEditorWindowLayout, 1); // don't layout. EndHandles will do this (the layout logic is slightly different from in-game: we always want to use full size of window
	EndHandles();
	state.m_CurrentEvent->type = originalType;
}

bool GUIView::OnInputEvent (InputEvent &event)
{
	if (event.type == InputEvent::kIgnore)
		return false;

	// Check if GUIView has already been destroyed...
	if (g_GUIViews.find (this) == g_GUIViews.end () || !m_Instance.IsValid())
		return false;

	if( s_DontSendEventsHack || !IsWindowEnabled(GetParent(m_View)) ) {
		if( event.type == InputEvent::kRepaint && m_GfxWindow )
			m_GfxWindow->EndRendering( !m_Transparent );
		return false;
	}

#if DEBUGMODE
	int eventType = event.type;
	int guiDepth = GetGUIState ().m_OnGUIDepth;
#endif

	// If we're being called from a nested OnGUI call, push the GUIState
	GUIState* tempState = NULL;
	GUIView* oldCurrentView = NULL;
	if (GetGUIState().m_OnGUIDepth > 0)
	{
		tempState = GUIState::GetPushState();
		oldCurrentView = s_CurrentView;
	}
	s_CurrentView = this;

	m_KeyboardState.m_ShowKeyboardControl = GetKeyGUIView() == this;

	InputEvent::Type originalType = event.type;

	GUIState &state = BeginGUIState (event);

	SendLayoutEvent (state);

	int handleTab = 0;
	if (event.type == InputEvent::kKeyDown && (event.character == '\t' || event.character == 25))
		handleTab = ((event.modifiers & InputEvent::kShift) == 0) ? 1 : -1;

	// It's possible for the view to be destroyed now
	if (s_CurrentView == NULL)
		return false;

	BeginHandles();

	//MonoBehaviour.DoGUI will erase EditorMouseOffset, so we save it here. 
	//We need it when processing the CursorRects, since they don't have any offset applied to them.
	int mouseCursorOffsetX, mouseCursorOffsetY;
	HWND mouseCusorOffsetWindow;
	GetEditorMouseOffset( &mouseCursorOffsetX, &mouseCursorOffsetY, &mouseCusorOffsetWindow );

	bool result = m_Instance->DoGUI(MonoBehaviour::kEditorWindowLayout, 1);

	/*
	It's possible that EditorGUIUtility.AddCursorRect have been called containing a rect that contains the mouse.
	Since on windows we handle the cursor change ourselves, we should check after every AddCursorRect call if the
	mouse is already on inside the passed rect. We do it here, to avoid cursors flicker in the case of overlapping Rects
	*/
	if (originalType == InputEvent::kRepaint && GetMouseOverWindow() == this)
	{
		Vector2f mousePos = event.mousePosition;
		Vector2f winSize = GetSize();
		if ( mousePos.x > 0 && mousePos.x < winSize.x &&  mousePos.y > 0 && mousePos.y < winSize.y )
		{
			//If this window has a offset set, we need to remove the offset, since the CursorRect do not have offsets applied.
			if (this->GetWindowHandle() == mouseCusorOffsetWindow) 
			{
				mousePos.x -= mouseCursorOffsetX;
				mousePos.y -= mouseCursorOffsetY;
			}
			ProcessCursorRect( mousePos.x, mousePos.y );
		}
	}
	EndHandles();

	bool currentViewGone = (s_CurrentView == NULL); // we might be deleted by now!

	if (handleTab != 0)
	{
		// Build the list of IDLists to cycle through
		std::vector<IDList*> keyIDLists;
		IMGUI::GUIWindow* focusedWindow = IMGUI::GetFocusedWindow (state);
		if (focusedWindow)
			keyIDLists.push_back (&focusedWindow->m_ObjectGUIState.m_IDList);
		else
			keyIDLists.push_back (&m_Instance->GetObjectGUIState().m_IDList);

		state.CycleKeyboardFocus(keyIDLists, handleTab == 1);
		event.Use ();
		result = true;
	}
	
	if (!currentViewGone)
	{
		if (originalType == InputEvent::kRepaint && m_GfxWindow)
		{
			const bool isGameView = IsGameView();
			double time0;
			if (isGameView)
				time0 = GetTimeSinceStartup();
			m_GfxWindow->EndRendering( !m_Transparent );
			if (isGameView)
				m_GameViewPresentTime = GetTimeSinceStartup() - time0;

#if ENABLE_VIEW_SHOT_DUMP
			++s_ShotFrames;
			QueueScreenshot(Format("paneshot-%05i-%x-F.png", s_ShotFrames, m_View));
			UpdateCaptureScreenshot();
#endif
		}

		// If the event was used, we want to repaint.
		// Do this before starting drags or showing menus, as they might delete this view!
		if (result) RequestRepaint ();
		EndGUIState(state);
	}
	
	/// Delay start any drags. This is to avoid reentrant behaviour on UnityGUI,
	/// because drags seem to create a second message loop.
	GetDragAndDrop().ApplyQueuedStartDrag();

	// Display context menu if script code generated one.
	// Don't do that for
	//	- repaint events (those can happen from inside already shown context menu on Windows).
	//  - WM_KEYDOWN since this will usually generate a WM_CHAR and it will get passed to the newly created menu, generating a BEEP.
	//     In this case, we should wait for the WM_CHAR event to show the menu.
	bool validKeyDownEvent = originalType != InputEvent::kKeyDown || event.character != '\0';
	if (originalType != InputEvent::kRepaint && validKeyDownEvent) {
		if( HasDelayedContextMenu() )
		{
			GetApplication().ResetReloadAssemblies();
			// Showing menu will sent capture changed event, we don't want to show menu 2nd time from that
			if (!currentViewGone)
				m_HasMouseCapture = false;
			ShowDelayedContextMenu ();
			// After showing context menu, repaint this view if it still exists
			if( g_GUIViews.find(this) == g_GUIViews.end() )
				currentViewGone = true;
			if (!currentViewGone)
				RequestRepaint();
		}
	}

	// Might be shutting down here already; calling further functions will crash
	if (gAlreadyClosing)
		return result;

	EndHandles();
	s_CurrentView = NULL;
	
	if (event.type == InputEvent::kMouseDown || event.type == InputEvent::kMouseMove)
	{
		GetTooltipManager().SendEvent(event);
	}

	if (tempState)
		GUIState::PopAndDelete (tempState);

	s_CurrentView = oldCurrentView;

	if (s_CurrentView == NULL)
	{	
		// After done with top-level event processing of this view, set screen parameters to the game view
		bool gameHasFocus;
		Rectf gameRect, gameCameraRect;
		GetScreenParamsFromGameView(true, false, &gameHasFocus, &gameRect, &gameCameraRect);
	}

#if DEBUGMODE
	if (GetGUIState ().m_OnGUIDepth != guiDepth)
		ErrorStringObject (Format ("OnGUIDepth changed: was %d is %d. Event type was %d", guiDepth, GetGUIState ().m_OnGUIDepth, eventType), m_Instance);
#endif
	
	return result;
}


GUIView::GUIView () 
{
	m_MouseRayInvisible = false;
}

GUIState& GUIView::BeginGUIState (InputEvent &event)
{
	GUIState& state = GetGUIState();
	m_KeyboardState.LoadIntoGUIState (state);
	state.SetEvent (event);
	state.BeginFrame ();
	return state;
}

void GUIView::EndGUIState (GUIState& state)
{
	m_KeyboardState.SaveFromGUIState (state);
	m_KeyboardState.EndFrame ();
}

void GUIView::Init (MonoBehaviour* behaviour, int depthBits, int antiAlias)
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x ViewInit\n", this);
	#endif
	m_CanBlitPaint = false;
	m_HasMouseCapture = false;
	m_Transparent = false;
	m_InsideContext = false;

	DWORD windowStyle = WS_CHILD;
	DWORD extendedStyle = WS_EX_TOOLWINDOW;

	Rectf pixelRect;
	pixelRect.x = 0;
	pixelRect.y = 0;
	pixelRect.width = 32;
	pixelRect.height = 32;

	// Passed rectangle indicates desired client area, adjust to get window area.
	RECT rc;
	rc.left = pixelRect.x;
	rc.top = pixelRect.y;
	rc.right = pixelRect.x + pixelRect.width;
	rc.bottom = pixelRect.y + pixelRect.height;
	AdjustWindowRectEx( &rc, windowStyle, FALSE, extendedStyle );

	m_AutoRepaint = false;
	m_NeedsRepaint = false;
	m_WantsMouseMove = false;

	// Create window
	m_View = CreateWindowExW( extendedStyle, kGUIViewWindowClassName, L"",
		windowStyle,
		rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top,
		HWND_MESSAGE, NULL, winutils::GetInstanceHandle(), NULL );
	SetWindowLongPtr( m_View, GWLP_USERDATA, (LONG_PTR)this );
	//printf_console("Create view %x\n", this);

	m_DepthBits = depthBits;
	m_DepthFormat = DepthBufferFormatFromBits(depthBits);
	m_AntiAlias = antiAlias;
	m_GfxWindow = GetGfxDevice().CreateGfxWindow( m_View, pixelRect.width, pixelRect.height, m_DepthFormat, m_AntiAlias);

	m_Instance = behaviour;

	//AddSceneRepaintView( m_Window );
	m_DropData = GetDragAndDrop().RegisterWindowForDrop( m_View, this );

	g_GUIViews.insert(this);
	GetSceneTracker().AddSceneInspector(this);
	m_GameViewRect = Rectf(0,0,0,0);
	m_GameViewPresentTime = 0.0f;
}

GUIView::~GUIView ()
{
#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x ~View\n", this);
#endif
	
	// If we're the current executing GUIView, NULL out the lists in GUIManager
	GUIState &guiState = GetGUIState ();
	if (guiState.m_ObjectGUIState == &m_Instance->GetObjectGUIState())
	{
		EndGUIState (guiState);
		guiState.m_ObjectGUIState = NULL;
	}	
	
	GetSceneTracker().RemoveSceneInspector(this);
	if( s_CurrentView == this ) {
		s_CurrentView = NULL;
		// we might be closing ourselves from mouse down, so never get the mouse up
		if(GetApplicationPtr())
			GetApplication().ResetReloadAssemblies();
	}
	
	MonoViewData* viewData = GetMonoViewData();
	if (viewData)
		viewData->m_ViewPtr = NULL;

	if( s_LastActiveView == this )
		s_LastActiveView = NULL;
	if( m_View || m_GfxWindow )
	{
		HWND view = m_View; // CleanupGUIView will set m_View to null, so remember the original value
		CleanupGUIView();
		if( view )
			DestroyWindow( view );
	}
	ErrorIf( m_View || m_GfxWindow );
	g_GUIViews.erase (this);
}


void GUIView::RecreateContext( int depthBits, int antiAlias )
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x ViewRecreateCtx\n", this, m_View);
	#endif
	m_DepthBits = depthBits;
	m_DepthFormat = DepthBufferFormatFromBits(depthBits);
	m_AntiAlias = antiAlias;
	if( m_GfxWindow ) {
		m_GfxWindow->Reshape( m_GfxWindow->GetWidth(), m_GfxWindow->GetHeight(), m_DepthFormat, m_AntiAlias );
	}
	RequestRepaint();
}

void GUIView::RepaintAll (bool performAutorepaint)
{
	AssertIf(!GetApplication().MayUpdate());

	GUIView** needRepaint;
	ALLOC_TEMP(needRepaint, GUIView*, g_GUIViews.size());
	int repaintCount = 0;

	// Collect all windows that need a repaint and set the repaint flag
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView& view = **i;
		bool shouldAutorepaint = (view.m_AutoRepaint && performAutorepaint);
		if (view.m_NeedsRepaint || shouldAutorepaint)
		{
			needRepaint[repaintCount++] = &view;
			if (shouldAutorepaint)
				view.m_CanBlitPaint = false;
		}
		view.m_NeedsRepaint = false;
	}

	// Actually repaint the collected windows
	for (int idx = 0; idx < repaintCount; idx++)
	{
		GUIView& view = *needRepaint[idx];
		// Make sure the window was not closed in the mean time
		if (g_GUIViews.count(&view) == 0)
			continue;

		#if ENABLE_WINDOW_MESSAGES_LOGGING
		printf_console("EW: %x %x ViewRepaintAll\n", &view, view.m_View);
		#endif

		// RedrawWindow must not be called here because it has structured exception handler that consumes all exceptions.
		// This in turn prevents Mono from delivering NullReferenceException (not to mention messing up internal state).

		//RedrawWindow( view.m_View, NULL, NULL, RDW_INTERNALPAINT | RDW_UPDATENOW );
		view.DoPaint();
	}

	GetApplication().PerformDrawHouseKeeping();
}

void GUIView::Repaint () {
	//printf_console("View %i repaint\n", m_View);
	RequestRepaint();
}



void GUIView::Focus()
{
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x ViewFocus\n", this, m_View);
	#endif
	if( m_View )
	{
		// Mac version would bring window to front, this will actually only flash the windows icon in taskbar, 
		// but afaik that's all you can do on windows without hacking
		if (::GetForegroundWindow() != m_Window->GetWindowHandle())
			::SetForegroundWindow ( m_Window->GetWindowHandle() );
		SetFocus( m_View );
	}
}

void GUIView::LostFocus ()
{
	if (GetApplication().MayUpdate())
		CallMethod ("OnLostFocus");

	GetAuxWindowManager ().OnLostFocus (this);
}

bool GUIView::GotFocus ()
{
	if (gAlreadyClosing)
		return false;
	
	if (!m_Instance.IsValid())
		return false;
	
	if (!m_Instance->GetInstance())
		return false;
	
	ScriptingMethodPtr method = m_Instance->FindMethod("OnFocus");
	if (!method)
		return false;

	ScriptingInvocation invocation(method);
	invocation.object = m_Instance->GetInstance();
	
	if (!MonoObjectToBool(invocation.Invoke()))
		return false;

	GetAuxWindowManager().OnGotFocus (this);
	return true;
}

void GUIView::AddToAuxWindowList ()
{
	GetAuxWindowManager().AddView (this, GetCurrent());
}

void GUIView::RemoveFromAuxWindowList ()
{
	GetAuxWindowManager().RemoveView (this);
}

void GUIView::SetWindow (ContainerWindow *win) {
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x ViewSetWindow %x (h=%x)\n", this, m_View, win, win ? win->GetWindowHandle() : 0);
	#endif
	//printf_console ("ContainerWindow: %x\n", win);
	ErrorIf( !m_View );

	m_Window = win;

	if( win && win->GetWindowHandle() != m_View ) {
		HWND parentWindowHandle = win->GetWindowHandle();
		// Setting parent to the same parent still causes Windows to change focus.
		// So don't do it.
		if( ::GetParent(m_View) != parentWindowHandle )
			::SetParent(m_View, parentWindowHandle);
		//bool parentVisible = ::GetWindowLong(parentWindowHandle, GWL_STYLE) & WS_VISIBLE;
		//printf_console("Set container %x on view %x, visible %i\n", win, this, parentVisible);
		//TP: This fix was made in Sweden, so it may be a bit uncertain... (nevermind)
		::ShowWindow(m_View, SW_SHOWNORMAL);
	}
	else if( !win ) {
		::SetParent(m_View, HWND_MESSAGE);
	}
}

void GUIView::SetPosition (const Rectf &position) {
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: %x %x ViewSetPosition %i,%i %ix%i\n", this, m_View, (int)position.x, (int)position.y, (int)position.width, (int)position.height);
	#endif
	RECT rc;
	rc.left = position.x;
	rc.top = position.y;
	rc.right = position.x + position.width;
	rc.bottom = position.y + position.height;
	SetWindowPos( m_View, NULL, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_FRAMECHANGED|SWP_NOOWNERZORDER|SWP_NOACTIVATE|SWP_NOZORDER );
}

void GUIView::CallMethod (const char* methodName) {
	#if ENABLE_WINDOW_MESSAGES_LOGGING
	//printf_console("EW: %x %x ViewCallMethod %s\n", this, m_View, methodName);
	#endif
	if( gAlreadyClosing )
		return;
	if (m_Instance.IsValid() && m_Instance->GetInstance())
		m_Instance->CallMethodInactive(methodName);
}

void GetGameViewAndRectAndFocus (Rectf* outGuiRect, GUIView** outView, bool* hasFocus)
{
	GUIView* view = NULL;
	
	if (GUIView::GetStartView() && GUIView::GetStartView()->GetWindowHandle())
	{
		view = GUIView::GetStartView();
	}
	else
	{
		// Go through all views and check if we have a game view (with a valid size)
		for (GUIViews::iterator it=g_GUIViews.begin(); it != g_GUIViews.end(); it++)
		{
			GUIView* curView = *it;
			if (curView->IsGameView () && curView->GetWindowHandle())
			{
				view = curView;
				break;
			}
		}
	}

	if (view && view->IsGameView ())
	{
		GUIView* keyView = GetKeyGUIView();
		*hasFocus = !keyView || (keyView == view);
		*outView = view;
		*outGuiRect = view->GetGameViewRect();
	}
	else
	{
		*hasFocus = false;
		*outView = NULL;
		*outGuiRect = Rectf(0,0,640,480);
	}
}


void GUIView::UpdateScreenManager()
{
	RECT rect = { 0, 0, 10, 10 };
	GetClientRect( m_View, &rect );
	const int width = rect.right - rect.left;
	const int height = rect.bottom - rect.top;

	ScreenManagerWin& screen = GetScreenManager();
	screen.SetWindow( m_View );
	screen.SetupScreenManagerEditor( width, height );
	GetRenderManager().SetWindowRect( Rectf (0,0,width,height) );

	// Pass screen offsets to UnityGUI (for popping up popup menus at the correct place)
	if( !gAlreadyClosing ) {
		POINT pt = {0,0};
		ClientToScreen( m_View, &pt );
		GetGUIManager().SetEditorGUIInfo( Vector2f(pt.x, pt.y) );
	}
}

bool ExecuteCommandOnKeyWindow (const std::string& commandName)
{
	GUIView* view = GetKeyGUIView ();
	if (view)
	{
		view->UpdateScreenManager();

		// If it's a game view, convert some commands to raw key events instead.
		if( !view->GetGameViewRect().IsEmpty() )
		{
			HWND wnd = view->GetWindowHandle();
			// This is not entirely robust, but should catch most combinations.
			// For a proper fix, we should not use Windows accelerators at all (because they are translated before
			// everything else), and instead translate accelerators ourselves after events that no one eats.
			if( commandName == "Copy" ) {
				InputEvent evt = InputEvent::SimulateKeyPressEvent( wnd, SDLK_c, InputEvent::kControl );
				return view->OnInputEvent( evt );
			}
			if( commandName == "Paste" ) {
				InputEvent evt = InputEvent::SimulateKeyPressEvent( wnd, SDLK_v, InputEvent::kControl );
				return view->OnInputEvent( evt );
			}
			if( commandName == "Cut" ) {
				InputEvent evt = InputEvent::SimulateKeyPressEvent( wnd, SDLK_x, InputEvent::kControl );
				return view->OnInputEvent( evt );
			}
		}

		InputEvent validateEvent = InputEvent::CommandStringEvent (commandName, false);
		if (view->OnInputEvent(validateEvent))
		{
			InputEvent event = InputEvent::CommandStringEvent (commandName, true);
			return view->OnInputEvent(event);
		}
	}
	return false;
}

GUIView* GetMouseOverWindow()
{
	POINT pt;
	if( !GetCursorPos(&pt) )
		return NULL;

	HWND windowUnderMouse = WindowFromPoint(pt);
	if( !windowUnderMouse )
		return NULL;

	GUIViews::iterator it, itEnd = g_GUIViews.end();
	for( it = g_GUIViews.begin(); it != itEnd; ++it ) {
		GUIView* view = *it;
		if (view->GetMouseRayInvisible())
			continue;
		HWND window = view->GetWindowHandle();
		if( window != windowUnderMouse )
			continue;
		RECT rc;
		GetWindowRect(window, &rc);
		if( PtInRect(&rc, pt) )
			return view;
	}

	return NULL;
}


Vector2f GetMousePosition()
{
	POINT pt;
	if( !GetCursorPos(&pt) )
		return Vector2f(0,0);

	return Vector2f(pt.x, pt.y);

}

bool ExecuteCommandInMouseOverWindow (const std::string& commandName)
{
	GUIView* view = GetMouseOverWindow();
	if( !view )
		return false;

	// Validate event
	view->UpdateScreenManager();
	InputEvent validateEvent = InputEvent::CommandStringEvent (commandName, false);
	if( !view->OnInputEvent(validateEvent) )
		return false;

	// Focus view
	s_LastActiveView = view;
	if( GetFocus() != view->GetWindowHandle() )
		SetFocus(view->GetWindowHandle());

	// Send command
	InputEvent event = InputEvent::CommandStringEvent (commandName, true, view->GetWindowHandle());
	return view->OnInputEvent(event);
}

void GUIView::StealMouseCapture() {
	if( m_View ) {
		m_HasMouseCapture = true;

		// This would send mouse up to the window that loses the mouse. We don't want that.
		s_DontSendEventsHack = true;
		SetCapture( m_View );
		s_DontSendEventsHack = false;
	}
}

void GUIView::ReleaseMouseCaptureIfNeeded()
{
	if( !m_HasMouseCapture || gAlreadyClosing )
		return;

	// Window deactivated while holding mouse capture (e.g. user alt-tabbed).
	m_HasMouseCapture = false;
	// Simulate a mouse up event!
	ProcessEventMessages( WM_LBUTTONUP, 0, 0 );
}

void EnableAllViews(bool enable)
{
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView* view = *i;
		if (view)
			EnableWindow (view->GetWindowHandle(), enable);
	}
}
