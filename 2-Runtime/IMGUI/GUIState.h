#ifndef GUISTATE_H
#define GUISTATE_H

#include "Runtime/Math/Color.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/IMGUI/IDList.h"
#include "Runtime/IMGUI/NamedKeyControlList.h"
#include "Runtime/IMGUI/GUIClip.h"

namespace IMGUI
{
	struct GUIWindowState;
	struct GUIWindow;
}


struct InputEvent;
struct MonoObject;
struct GUIGraphicsCacheBlock;
struct UTF16String;

// Lasts multiple frames.
// There is ONE for user's games and one for each Editor Window.
struct MultiFrameGUIState
{
	// Which control has keyboard focus
	int m_KeyboardControl;

	// List of named keyboard controls. NULL if none
	IMGUI::NamedKeyControlList *m_NamedKeyControlList;

	// List of GUI.Window IDLists
	IMGUI::GUIWindowState *m_Windows;
	
	
	IMGUI::NamedControl *GetControlNamed (const std::string &name);
	void AddNamedControl (std::string &name, int id, IMGUI::GUIWindow* window);
	void ClearNamedControls ();
	void Reset ();
	
	MultiFrameGUIState ();
	~MultiFrameGUIState ();	
};

// State that is reset each OnGUI.
struct OnGUIState
{
	OnGUIState ();
	~OnGUIState ();

	// Colors for rendering
	ColorRGBAf m_Color, m_BackgroundColor, m_ContentColor;
	
	// Is the GUI enabled
	int m_Enabled;
	
	// Has the GUI changed
	int m_Changed;
	
	// Depth of the current GUIBehaviour's OnGUI - not used by GUI.Window or EditorWindow
	int m_Depth;
	
	int m_ShowKeyboardControl;
	
	// If IMGUI is rendered inside NewGUI, this is a pointer to the batching object. All rendering commands go into this.
	std::vector<GUIGraphicsCacheBlock>* m_CaptureBlock;
	
	// Name of the next keyboard control. It's a pointer to maintain size with Mono, but owned by OnGUIState
	// Don't set this pointer (unless you're implementing nested OnGUI calls) - use the getters and setters
	std::string *m_NameOfNextKeyboardControl;
	
	UTF16String *m_MouseTooltip, *m_KeyTooltip;

	void SetNameOfNextKeyboardControl (const std::string &name);
	std::string *GetNameOfNextKeyboardControl () { return m_NameOfNextKeyboardControl; }
	
	void SetMouseTooltip (const UTF16String &tooltip);
	UTF16String *GetMouseTooltip () { return m_MouseTooltip; }
	
	void SetKeyTooltip (const UTF16String &tooltip);
	UTF16String *GetKeyTooltip () { return m_KeyTooltip; }
	
	void ClearNameOfNextKeyboardControl ();
	// Reset the values to sane defaults for an OnGUI call
	void BeginOnGUI ();
	void EndOnGUI ();
};

// State that is stored in the MonoBehaviour and pulled from it every OnGUI.
struct ObjectGUIState
{
	IDList m_IDList;			// per monobehaviour + 1 per GUI.Window

	void BeginOnGUI ();
	
	ObjectGUIState (); 
	~ObjectGUIState ();
};

// State that exists per new-style canvas, and per old-style MonoBehaviour with an OnGUI.
struct CanvasGUIState
{
	GUIClipState m_GUIClipState;
	int m_IsMouseUsed;
};

/// This one is always available.
struct EternalGUIState
{
private:
	int m_UniqueID;
	// Which control has touch / mouse focus
public:
	int m_HotControl;			// TODO:should be int[kMaxSupportedPointers]
	bool m_AllowHover;
	
	int GetNextUniqueID ()
	{
		return m_UniqueID++;
	}
	
	EternalGUIState ()
	{
		m_UniqueID = 1;
		m_HotControl = 0;
		m_AllowHover = true;
	}
};
EternalGUIState *GetEternalGUIState ();

// All state used by the GUI (so be it!)
struct GUIState 
{
	GUIState ();
	~GUIState();
	
	MultiFrameGUIState  m_MultiFrameGUIState;
	OnGUIState			m_OnGUIState;
	ObjectGUIState*		m_ObjectGUIState;		// The state owned by the monobehaviour we're calling on
	CanvasGUIState		m_CanvasGUIState;		// The state for the surface we're rendering into
	EternalGUIState*	m_EternalGUIState;
	InputEvent*         m_CurrentEvent;			// C++ side. Maps to the memory INSIDE the ManagedCurrentEvent. 
	
	InputEvent			m_BackupEvent;

	int					m_OnGUIDepth;			// How deep we are inside OnGUI calls. 0 = we haven't called at all
	
	// Whether the current event has been marked as used
#if ENABLE_NEW_EVENT_SYSTEM
	bool GetIsEventUsed() const { return (m_CurrentEvent != NULL) && m_CurrentEvent->type == InputEvent::kUsed; }

	// Begin and End simply make it possible to use Event.current from C# outside of OnGUI code
	void BeginUsingEvents () { ++m_OnGUIDepth; }
	void EndUsingEvents() { --m_OnGUIDepth; }
#endif

	// Call this to intialize a game frame or window frame - not for each event we send into the system.
	void BeginFrame ();
	
	// Call this when we're done with one OS-level frame.
	void EndFrame ();
	
	// Call this to intialize ONE onGUI call. Must be called between e.g. layout and repaint events
	// Assumes all states have been assigned correctly.
	void BeginOnGUI (ObjectGUIState &objectGUIState);
	
	/// Call this to end an OnGUI call. It will call all substates with EndOnGUI, so they can clean up.
	void EndOnGUI ();


	int GetControlID (int hint, FocusType focusType, const Rectf &rect);
	int GetControlID (int hint, FocusType focusType);

	// Handle Named Controls
	void SetNameOfNextKeyboardControl (const std::string &name) { m_OnGUIState.SetNameOfNextKeyboardControl (name); }
	std::string GetNameOfFocusedControl ();
	int GetIDOfNamedControl (const std::string &name);
	void FocusKeyboardControl (const std::string &name);

	//Not sure where this should go. It's called by GUIManager & EditorWindows. For now, I'll put it here...
	void CycleKeyboardFocus (std::vector<IDList *> &IDListsToSearch, bool searchForward);
	int GetNextKeyboardControlID (std::vector<IDList *> &IDListsToSearch, bool searchForward);

	void SetEvent (const InputEvent& event);
	void SetObjectGUIState (ObjectGUIState &objectGUIState);
	
	// Make a copy of the current managed gui state (to implement nesting)
	// When done with an OnGUI call, call PopAndDelete to restore.
	static GUIState* GetPushState ();
	static void PopAndDelete (GUIState *pushState);

	// Move all GUI state into dest, NULLing all pointer fields in this
	// Used by GetPushState & PopAndDelete
	void MoveAllDataTo (GUIState &dest, bool saving);
	
	// Called from C# whenever the user assigns into Event.current. 
	// Don't call this from C++;
	void Internal_SetManagedEvent (void *event);
};

GUIState &GetGUIState ();

// Struct for saving GUI State between in-game frames. Each editor window is also a separate "world" in this regard.
// So E.g. Keyboard control names are shared between all scripts in a game, but each editor window has it's own separate world.
struct GUIKeyboardState
{
	// Which controlID has focus last frame
	int		m_KeyboardControl;
	// Should we show the keyboard control? false if editor window (or webplayer I guess) doesn't have focus.
	int		m_ShowKeyboardControl;
	// IDLists for each window. Null if none
	IMGUI::GUIWindowState* m_Windows;

	IMGUI::NamedKeyControlList *m_NamedKeyControlList;

	int		m_FocusedGUIWindow;

	void	LoadIntoGUIState (GUIState &dest);
	void	SaveFromGUIState (GUIState &src);
	
	void	EndFrame ();
	
	void	Reset ();
	
	GUIKeyboardState ();
	~GUIKeyboardState ();
};

void InitGUIState ();
void CleanupGUIState ();
#endif
