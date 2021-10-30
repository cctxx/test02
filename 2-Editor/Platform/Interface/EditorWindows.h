#ifndef EDITORWINDOW_H
#define EDITORWINDOW_H


#include "Runtime/Misc/InputEvent.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Math/Color.h"
#include "Editor/Src/SceneInspector.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/IMGUI/GUIState.h"

class GUIView;
class GfxDeviceWindow;

#if UNITY_WIN
#include "Runtime/Utilities/LinkedList.h"
#endif



#ifdef __OBJC__

#import <Cocoa/Cocoa.h>
#include "Editor/Platform/OSX/Utility/OpenGLView.h"

@interface GUIOpenGLView : OpenGLView
{
@public
	GUIView* m_View;
	bool m_HasFocus;
}
-(GraphicsContextHandle)context;
@end
#endif

/// Validates & Executes the command on the key view
bool ExecuteCommandOnKeyWindow (const std::string& commandName);
/// Validates the command on the view that has the mouse over.
/// If it supports the command, gives it focus and executes the command
bool ExecuteCommandInMouseOverWindow (const std::string& commandName);
/// Validates if the command is supported in the key view
bool ValidateCommandOnKeyWindow (const std::string& commandName);
/// Validates if the command is supported in the key view or mouse over window
bool ValidateCommandOnKeyWindowAndMouseOverWindow (const std::string& commandName);
/// Validates and executes the command on all visible editor windows
bool ExecuteCommandOnAllWindows (const std::string& commandName);
/// Execute frame command on last active scene view
bool FrameLastActiveSceneView(const bool lock);

/// Set up the event data for mouse position and modifier keys
void SetupEventValues (InputEvent *evt);

#if UNITY_WIN
/// Enables or disables all views
void EnableAllViews(bool enable);
#endif


class ContainerWindow;
struct MonoContainerWindowData
{
	UnityEngineObjectMemoryLayout data;	
	ContainerWindow* m_WindowPtr;
	Rectf	m_PixelRect;
	int		m_ShowMode;
	void*	m_Title;
};


class ContainerWindow
{
public:
	enum ShowMode {
		kShowNormalWindow = 0,	// normal window with max/min/close buttons
		kShowPopupMenu = 1,		// popup menu - no title bar
		kShowUtility = 2,			// tool window - floats in the app, and hides when the app deactivates
		kShowNoShadow = 3,		// no shadow/decorations
		kShowMainWindow = 4,		// main Unity window. On Mac does not have close button; on Windows has menu bar etc.
		kShowAuxWindow = 5,		// Popup windows like the color picker, gradient editor, etc. Drawn with black background on Mac
	};

public:
	ContainerWindow ();
	~ContainerWindow ();

	void Init (MonoBehaviour* behaviour, Rectf pixelRect, int showMode, const Vector2f& minSize, const Vector2f& maxSize);
	void BringLiveAfterCreation (bool displayNow, bool setFocus);

	void SetRect (const Rectf &rect);
	Rectf GetRect () const;
	
	/// Closes the window
	void Close();
	void Minimize ();
	void ToggleMaximize ();
	
	bool IsMaximized () { return false; /* @TODO:implement on platform side */}
	static void GetOrderedWindowList ();

	// Note that this function should only be used for short operations as OSX only allows
	// to freeze the display up to one second before it automatically reenables updates. (See NSGraphics.h -> NSDisableScreenUpdates)
	static void SetFreezeDisplay (bool disp);	

	void DisplayAllViews ();
	void EnableAllViews (bool enable);
	
	void SetHasShadow (bool hasShadow);
	void SetTitle (const string &title);	
	void SetMinMaxSizes (const Vector2f &minSize, const Vector2f &maxSize);	
	void MakeModal();

	ShowMode GetShowMode() const { return m_ShowMode; }

	// Fit a rect to the screen of the mouse
	// rect: window rect (in Unity screen space)
	// useMouseScreen: if true=clamp to the screen the mouse is on. false=clamp to the window's screen
	// forceCompletelyVisible: Should the window be completely on-screen or only the title bar
	Rectf FitWindowRectToScreen (const Rectf &rect, bool useMouseScreen, bool forceCompletelyVisible);

	// Get the top-left screen coordinate of the content area (Used for dealing with titlebar size)
	Vector2f GetTopleftScreenPosition ();

	MonoContainerWindowData* GetMonoContainerWindowData()
	{
		if (m_Instance.IsValid() && m_Instance->GetInstance())
			return &ExtractMonoObjectData<MonoContainerWindowData> (m_Instance->GetInstance());
		else
			return NULL;
	}

	void CallMethod (const char* methodName)
	{
		if (m_Instance.IsValid() && m_Instance->GetInstance())
			m_Instance->CallMethodInactive(methodName);
	}

	void SetAlpha (float alpha);
	void SetInvisible ();
	void SetIsDragging(bool isDragging);
	bool IsZoomed ();
	
	#if UNITY_WIN
	static LRESULT CALLBACK ContainerWndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
	void CleanupContainerWindow();
	HWND GetWindowHandle() const { return m_Window; }
	void UpdateMaxMaximizedRect();
	bool InMenuLoop();
	void OnActivateApplication( bool activate );
	HWND GetLastFocusedChild() { return m_LastFocusedChild; }
	void SetLastFocusedChild(HWND hWnd) { m_LastFocusedChild = hWnd; }
	#elif UNITY_OSX
	static void HandleSpacesSwitch();
	static void PerformSizeChanges();
	void DoSetRect (const Rectf &rect);
	#elif UNITY_LINUX
	NativeWindow GetWindowHandle () const { return m_Window; }
	void CleanupContainerWindow();
	bool ProcessEvent(void *event);
	#endif
	
	void MoveInFrontOf(ContainerWindow *other);
	void MoveBehindOf(ContainerWindow *other);
	void OnRectChanged();

private:
	PPtr<MonoBehaviour> m_Instance;
	Rectf		m_InternalRect;
	ShowMode	m_ShowMode;
	
	#if UNITY_OSX
		#ifdef __OBJC__
		public:
		NSWindow*	m_Window;
		#else
		void*		m_Window;
		#endif
		bool		m_WantsToResize;
		Rectf		m_NewSize;
	#elif UNITY_WIN
		ListNode<ContainerWindow> m_ContainerListNode;
		HWND	m_Window;
		bool	m_CloseFromScriptDontShutdown;
		bool	m_IsClosing;
		bool	m_InMenuLoop;
		POINT	m_MinSize;
		POINT	m_MaxSize;
		RECT	m_MaxMaximizedRect;
		HWND	m_LastFocusedChild;
	#elif UNITY_LINUX
		NativeWindow m_Window;
		bool	m_IsClosing;
	#else
	#error "Unknown platform"
	#endif
};



struct MonoViewData {
	UnityEngineObjectMemoryLayout data;
	GUIView* m_ViewPtr;
};


#define kGUIViewClearColor 0.76f


class GUIView : public ISceneInspector {
public:
	void Init (MonoBehaviour* behaviour, int depthBits, int antiAlias);
	GUIView ();
	~GUIView ();
	void Repaint ();
	void SetWindow (ContainerWindow *win);
	const ContainerWindow* GetWindow () {return m_Window;}
	void RequestClose ();
	void SetPosition (const Rectf &position);
	Rectf GetPosition();

	void RecreateContext( int depthBits, int antiAlias );

	void CallMethod (const char* methodName);
	static void RepaintAll (bool performAutorepaint);
	static void RecreateAllOnAAChange ();
	static void RecreateAllOnDepthBitsChange ( int from, int to );

	bool OnInputEvent (InputEvent &event);
	
	// The CurrentView is the view that has received the current input event (is NULL outside OnInputEvent)
	static GUIView *GetCurrent ();
	static MonoBehaviour* GetCurrentMonoView ();

	void RequestRepaint () {
		m_NeedsRepaint = true;
		#if UNITY_WIN
		m_CanBlitPaint = false;
		#endif
	}
	void SendLayoutEvent (GUIState &state);
	void SetAutoRepaintOnSceneChange (bool autoRepaint) { m_AutoRepaint = autoRepaint; }
	bool GotFocus ();
	void LostFocus ();
	void Focus ();
	void TickInspector ();

	void AddToAuxWindowList ();
	void RemoveFromAuxWindowList ();
	
	virtual void SelectionHasChanged (const std::set<int>& selection);
	virtual void HierarchyWindowHasChanged ();
	virtual void ProjectWindowHasChanged ();
	virtual void DidOpenScene ();

	#if UNITY_WIN
	static LRESULT CALLBACK GUIViewWndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
	void CleanupGUIView();
	HWND GetWindowHandle() const { return m_View; }
	void ReleaseMouseCaptureIfNeeded();

	friend void CreateGfxWindowOnAllGUIViews();
	friend void ReleaseGfxWindowOnAllGUIViews();
	friend void ResetGfxDeviceAndRepaint();

	void DragEvent( int x, int y, DWORD keyFlags, int eventType );

	void ProcessEventMessages( UINT message, WPARAM wParam, LPARAM lParam );

	void MakeVistaDWMHappyDance();

	GfxDeviceWindow* GetGfxWindow() { return m_GfxWindow; }
	#endif

	#if UNITY_LINUX
	NativeWindow GetWindowHandle () const { return m_View; }
	GfxDeviceWindow* GetGfxWindow () { return m_GfxWindow; }
	void CleanupGUIView ();
	void ProcessEventMessages(void *event);
	bool ProcessEvent(void *event);
	#endif

	void SetGameViewRect(const Rectf& rect) { m_GameViewRect = rect; }
	Rectf GetGameViewRect() { return m_GameViewRect; }
	bool IsGameView() const { return m_GameViewRect.Width() != 0.0f && m_GameViewRect.Height() != 0.0f; }
	float GetGameViewPresentTime() const { return m_GameViewPresentTime; }
	
	void SetAsStartView () { s_StartGUIView = this;}
	static void ClearStartView () { s_StartGUIView = 0; }
	static GUIView* GetStartView () {return s_StartGUIView;}

	void SetAsActiveWindow();

	// Called to make sure we're using the correct context
	// inside picking from a scene view.
	void BeginCurrentContext();
	void EndCurrentContext();
	
	
	Vector2f GetSize();
	void ForceRepaint();
	
	#ifdef __OBJC__
	GUIOpenGLView* GetCocoaView () { return m_View; }
	#endif


	enum MouseCursor {
		kArrow = 0, 
		kText = 1, 
		kResizeVertical = 2, 
		kResizeHorizontal = 3,
		kLink = 4,
		kSlideArrow = 5,
		kResizeUpRight = 6,
		kResizeUpLeft = 7,
		kMoveArrow = 8,
		kRotateArrow = 9,
		kScaleArrow = 10,
		kArrowPlus = 11,
		kArrowMinus = 12,
		kPan = 13,
		kOrbit = 14,
		kZoom = 15,
		kFPS = 16,
		kCustomCursor = 17,
		kSplitResizeUpDown = 18,
		kSplitResizeLeftRight = 19,
		kCursorCount // < keep this last!
//		kRepaint = 100; implement later, mkay?
	};
	void ClearCursorRects ();
	void AddCursorRect (const Rectf &position, MouseCursor mouseCursor);
	void AddCursorRect (const Rectf &position, MouseCursor mouseCursor, int controlID);
	#if UNITY_OSX
	void UpdateOSRects ();
	#endif

	// Used by ObjC, but should really be private
	struct CursorRect {
		Rectf position;	// top-left based rectangle
		MouseCursor mouseCursor;
		int controlID;
		CursorRect (const Rectf &pos, MouseCursor cursor)  : position (pos), mouseCursor (cursor) { controlID = 0; }
		CursorRect (const Rectf &pos, MouseCursor cursor, int cID)  : position (pos), mouseCursor (cursor), controlID (cID) { }
	};
	std::vector <CursorRect> m_CursorRects;
	
	MonoBehaviour* GetBehaviour () { return m_Instance; }
	 
	MonoViewData* GetMonoViewData()
	{
		if (m_Instance.IsValid() && m_Instance->GetInstance())
			return &ExtractMonoObjectData<MonoViewData> (m_Instance->GetInstance());
		else
			return NULL;
	}
	void UpdateScreenManager ();
	
	void StealMouseCapture();
	GET_SET (bool, WantsMouseMove, m_WantsMouseMove);
	GET_SET (int, KeyboardControl, m_KeyboardControl);

	void ClearKeyboardControl () { m_KeyboardState.m_KeyboardControl = 0; }
	GET_SET (bool, MouseRayInvisible, m_MouseRayInvisible);
private:
	GUIState& BeginGUIState (InputEvent& evt);
	void EndGUIState (GUIState &state);

	#if UNITY_WIN
	void ProcessCursorRect( int mouseX, int mouseY );
	#endif
	
	#if UNITY_WIN || UNITY_LINUX
	void DoPaint(void);
	#endif
	static GUIView* s_StartGUIView;

private:
	PPtr<MonoBehaviour> m_Instance;
	bool	m_AutoRepaint;
	bool	m_WantsMouseMove;
	// If true, this view is invisible to FindWindowUnderMouse. Used by the tooltip view
	bool	m_MouseRayInvisible;
	bool	m_NeedsRepaint;
	Rectf	m_GameViewRect;
	float	m_GameViewPresentTime;
	int		m_KeyboardControl;
	int		m_AntiAlias;
	int		m_DepthBits;
	GUIKeyboardState m_KeyboardState;
	ContainerWindow *m_Window;
	
	#if UNITY_OSX
		#ifdef __OBJC__
		GUIOpenGLView* m_View;
		#else
		void *m_View;
		#endif
	#elif UNITY_WIN
		HWND				m_View;
		void*				m_DropData;
		GfxDeviceWindow*	m_GfxWindow;
		DepthBufferFormat	m_DepthFormat;
		bool				m_CanBlitPaint;
		bool				m_HasMouseCapture;
		bool				m_Transparent;
		bool				m_InsideContext;
	#elif UNITY_LINUX
		DepthBufferFormat   m_DepthFormat;
		GfxDeviceWindow*    m_GfxWindow;
		NativeWindow        m_View;
		bool                m_InsideContext;
	#else
	#error "Unknown Platform"
	#endif
};

typedef std::set<GUIView*, std::less<GUIView*>, STL_ALLOCATOR(kMemEditorGui, GUIView*) > GUIViews;

GUIView* GetKeyGUIView ();
GUIView* GetMouseOverWindow ();
Vector2f GetMousePosition ();


bool CallGlobalInputEvent (const InputEvent& event);

#if UNITY_WIN
bool IsGfxWindowValidOnAllGUIViews();
void CreateGfxWindowOnAllGUIViews();
void ReleaseGfxWindowOnAllGUIViews();
bool ResetGfxDeviceIfNeeded();
void ResetGfxDeviceAndRepaint();
void InitializeEditorWindowClasses();
HWND CreateMainWindowForBatchMode();
void ShutdownEditorWindowClasses();
void GetGameViewAndRectAndFocus (Rectf* outRect, GUIView** outView, bool* hasFocus);
void EnableAllContainerWindows( bool enable );
void OnMinimizeApplication( HWND sendingWindow );
void CycleToContainerWindow( int offset );
void SetUtilityWindowsTopmost( bool topmost );
#endif

#if defined(__OBJC__)
void GetGameViewAndRectAndFocus (Rectf* outRect, NSView** outView, bool* hasFocus);
void SendMouseMoveEvent (NSEvent* nsevent);
#endif

void InvalidateGraphicsStateInEditorWindows();

void RequestRepaintOnAllViews();
void ForceRepaintOnAllViews();


#endif
