#ifndef GUIWINDOWS_H
#define GUIWINDOWS_H

#include "Runtime/Math/Rect.h"
#include "Runtime/IMGUI/GUIContent.h"
#include "Runtime/IMGUI/IMGUIUtils.h"

struct GUIContent;
class GUIStyle;

namespace IMGUI
{
	struct GUIWindow 
	{
		int m_ID;
		// The ID list used by this window
		ObjectGUIState m_ObjectGUIState;
		Rectf m_Position;
		// Sorting depth
		int m_Depth;
		// What's the title?
		GUIContent m_Title;
		// Was this window referenced this frame? (used to clean up unused windows at end-of-frame)
		bool m_Used;
		// Was it moved with a DragWindow? If so, we need to use our internal rect instead of the one passed in to us
		bool m_Moved;
		bool m_ForceRect;

		// Mono Object handles
		int m_Delegate;
		int m_Skin;
		int m_Style;
		
		// GUIState GUI.window time:
		ColorRGBAf m_Color, m_BackgroundColor, m_ContentColor;
		Matrix4x4f m_Matrix;
		bool m_Enabled;
		
		void LoadFromGUIState (GUIState &state);
		void SetupGUIValues (GUIState &state);
		void OnGUI (GUIState &state);
		void ReleaseScriptingObjects ();
		
		GUIWindow ();		
		~GUIWindow ();
	};
	
	Rectf DoWindow (GUIState &state, int windowId, const Rectf &clientRect, ScriptingObjectPtr delegate, GUIContent& title, ScriptingObjectPtr style, ScriptingObjectPtr guiSkin, bool forceRectOnLayout, bool isModal = false);
	void DragWindow (GUIState &state, const Rectf &rect);

	void BeginWindows (GUIState &state, bool setupClipping, bool ignoreModalWindow = true);
	void EndWindows (GUIState &state, bool ignoreModalWindow = true);
	void RepaintModalWindow(GUIState &state);

	void MoveWindowFromLayout (GUIState &state, int windowID, const Rectf &rect);
	Rectf GetWindowRect (GUIState &state, int windowID);

	Rectf GetWindowsBounds (GUIState &state);
	
	void BringWindowToFront (GUIState &state, int windowID);
	void BringWindowToBack (GUIState &state, int windowID);	
	void FocusWindow (GUIState &state, int windowID);
	
	// Get the window that has focus, or NULL
	GUIWindow *GetFocusedWindow (GUIState &state);
	
	struct GUIWindowState
	{
		GUIWindowState ();
		~GUIWindowState ();
		typedef std::vector<GUIWindow*> WindowList;
		WindowList m_WindowList;
		int m_FocusedWindow;
		bool m_LayersChanged;
		
		// The window we're currently calling OnGUI on, or NULL
		GUIWindow* m_CurrentWindow;
		
		// The current modal window being displayed, or NULL if there are no modal windows this frame
		GUIWindow* m_ModalWindow;
		
		GUIWindow* GetWindow (int windowId);
		void SortWindows ();
		GUIWindow* FindWindowUnderMouse (GUIState &state);
		
		// Release all GC handles. We call this at the end of every frame in order to make sure we don't leak anything
		// (they only need to get remembered WITHIN one layout/event cycle)
		void ReleaseScriptingObjects ();
	};
}




#endif
