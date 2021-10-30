#include "UnityPrefix.h"
#include "Runtime/Misc/BuildSettings.h"

#if ENABLE_UNITYGUI
#include "Runtime/IMGUI/GUIWindows.h"
#include "Runtime/IMGUI/IMGUIUtils.h"
#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/IMGUI/IMGUIUtils.h"
#include "Runtime/IMGUI/GUIStyle.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingObjectWithIntPtrField.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"


#include <algorithm> // for std::sort
namespace IMGUI 
{
	static Vector2f s_DragStartPos (0,0);		// Start of the drag (mousePosition)
	static Vector2f s_DragStartSize (0,0);		// Value at start of drag.

	GUIWindow::GUIWindow ()
	{
		m_Delegate = m_Skin = 0;
		m_Moved = m_ForceRect = false;
		m_ID = 0;
		m_Style = 0;
	}
	
	GUIWindow::~GUIWindow ()
	{
		ReleaseScriptingObjects ();
	}
	
	
	void GUIWindow::ReleaseScriptingObjects ()
	{
		if (m_Delegate)
		{
			scripting_gchandle_free (m_Delegate);
			m_Delegate = 0;
		}
		if (m_Skin)
		{
			scripting_gchandle_free (m_Skin);
			m_Skin = 0;
		}
		if (m_Style)
		{
			scripting_gchandle_free (m_Style);
			m_Style = 0;
		}
	}

	void GUIWindow::OnGUI (GUIState& state)
	{
		InputEvent& evt (*state.m_CurrentEvent);
		// Set up the state that was recorded for this window
		state.m_OnGUIState.m_Color = m_Color;
		state.m_OnGUIState.m_BackgroundColor = m_BackgroundColor;
		state.m_OnGUIState.m_ContentColor = m_ContentColor;
		state.m_OnGUIState.m_Enabled = m_Enabled;
		state.m_CanvasGUIState.m_GUIClipState.SetMatrix (evt, m_Matrix);
		state.m_MultiFrameGUIState.m_Windows->m_CurrentWindow = this;
		
		// Block OnHover calls into the scene if the window contains the mouse
#if ENABLE_NEW_EVENT_SYSTEM
		if (evt.type == InputEvent::kRepaint && m_Position.Contains (evt.touch.pos))
#else
		if (evt.type == InputEvent::kRepaint && m_Position.Contains (evt.mousePosition))
#endif
			state.m_CanvasGUIState.m_IsMouseUsed = true;		
		
		// Disable drawing keyboard focus if window doesn't have focus.
		int hadShowKeyboardControl = state.m_OnGUIState.m_ShowKeyboardControl;
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		state.m_OnGUIState.m_ShowKeyboardControl &= winState->m_FocusedWindow == m_ID;

		// If it's a repaint event, draw the background
		ScriptingObjectPtr style = scripting_gchandle_get_target (m_Style);
		if (style && evt.type == InputEvent::kRepaint)
		{
			GUIStyle* _style = ScriptingObjectWithIntPtrField<GUIStyle> (style).GetPtr();
#if ENABLE_NEW_EVENT_SYSTEM
			_style->Draw (state, m_Position, m_Title, m_Position.Contains (evt.touch.pos), false, state.m_MultiFrameGUIState.m_Windows->m_FocusedWindow == m_ID, false);
#else
			_style->Draw (state, m_Position, m_Title, m_Position.Contains (evt.mousePosition), false, state.m_MultiFrameGUIState.m_Windows->m_FocusedWindow == m_ID, false);
#endif
		}
		
		state.m_CanvasGUIState.m_GUIClipState.Push (*state.m_CurrentEvent, m_Position, Vector2f::zero, Vector2f::zero, false);
		ObjectGUIState* old = state.m_ObjectGUIState;
		state.BeginOnGUI (m_ObjectGUIState);
		
		// No exception handling here on purpose.
		ScriptingInvocation invocation(MONO_COMMON.callGUIWindowDelegate);
		invocation.AddObject(scripting_gchandle_get_target (m_Delegate));
		invocation.AddInt(m_ID);
		invocation.AddObject(scripting_gchandle_get_target (m_Skin));
		invocation.AddInt((int)m_ForceRect);
		invocation.AddFloat(m_Position.width);
		invocation.AddFloat(m_Position.height);
		invocation.AddObject(style);

		state.m_OnGUIState.m_ShowKeyboardControl = winState->m_FocusedWindow == m_ID;

		// we need to catch a log our own exceptions to properly handle ExitGUIException
		ScriptingExceptionPtr exception = NULL;
		invocation.logException = false;

		invocation.Invoke (&exception);

		if (exception)
		{
			// TODO: Kill GUI all the way down to the MonoBehaviour
#if ENABLE_MONO
			void* excparams[] = {exception};
			MonoObject* res = CallStaticMonoMethod("GUIUtility", "EndGUIFromException", excparams);
			if (!MonoObjectToBool(res))
				::Scripting::LogException(exception, 0);
#endif
		}

		state.EndOnGUI ();
		state.m_ObjectGUIState = old;
		state.m_CanvasGUIState.m_GUIClipState.Pop (evt);
		state.m_MultiFrameGUIState.m_Windows->m_CurrentWindow = NULL;

		// make sure that the rest of the script shows keyboard focus 
		state.m_OnGUIState.m_ShowKeyboardControl = hadShowKeyboardControl;
	}

	Rectf DoWindow (GUIState& state, int id, const Rectf &clientRect, ScriptingObjectPtr delegate, GUIContent& title, ScriptingObjectPtr style, ScriptingObjectPtr guiSkin, bool forceRectOnLayout, bool isModal)
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		if (winState == NULL)
			state.m_MultiFrameGUIState.m_Windows = winState = new GUIWindowState ();

		GUIWindow* win = winState->GetWindow (id);
		if (!win)
		{
			if(isModal && winState->m_ModalWindow != NULL)
			{				
				DebugStringToFile ("You cannot show two modal windows at once", 0,  __FILE__, __LINE__, kError);
				return clientRect;
			}
			win = new GUIWindow();
			win->m_ID = id;
			win->m_Depth = -1;
			
			if(isModal)
			{
				winState->m_ModalWindow = win;
			}
			else
			{
				winState->m_WindowList.push_back(win);
				winState->m_LayersChanged = true;
			}
		}
		
		if(isModal)
		{
			if(winState->m_ModalWindow == NULL)
			{
				winState->m_ModalWindow = win;
				
				// If window is in the window list, remove it.
				GUIWindowState::WindowList::iterator i = std::find(winState->m_WindowList.begin(),
																   winState->m_WindowList.end(),
																   win);
				if(i != winState->m_WindowList.end())
				{
					winState->m_WindowList.erase(i);
					winState->m_LayersChanged = true;
				}
			}
			else if(winState->m_ModalWindow != win)
			{
				// This can happen if you already have a modal window open, and attempt
				// to show an already-created window as a modal window.
				DebugStringToFile ("Attempting to show modal windows at once; the newer windows will not be modal", 0,  __FILE__, __LINE__, kError);
			}
		}
		
		if (!win->m_Moved)
			win->m_Position = clientRect;
		else
			win->m_Moved = false;
		
		win->m_Title = title;

		win->ReleaseScriptingObjects ();
		win->m_Style = scripting_gchandle_new (style);
		win->m_Delegate = scripting_gchandle_new (delegate);
		win->m_Skin = scripting_gchandle_new (guiSkin);
		
		win->m_Used = true;
		win->m_Enabled = state.m_OnGUIState.m_Enabled;
		win->m_Color = state.m_OnGUIState.m_Color;
		win->m_BackgroundColor = state.m_OnGUIState.m_BackgroundColor;
		win->m_ContentColor = state.m_OnGUIState.m_ContentColor;
		win->m_Matrix = state.m_CanvasGUIState.m_GUIClipState.GetMatrix();
		win->m_ForceRect = forceRectOnLayout;

		#if !GAMERELEASE
		if (state.m_MultiFrameGUIState.m_Windows->m_CurrentWindow)
			ErrorString("GUI Error: You called GUI.Window inside a another window's function. Ensure to call it in a OnGUI code path.");
		#endif

		return win->m_Position;
		
	}
	
	void DragWindow (GUIState &state, const Rectf &position) 
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		GUIWindow* win = winState ? winState->m_CurrentWindow : NULL;
		if (win == NULL) 
		{
			ErrorString ("Dragwindow can only be called within a window callback");
			return;
		}
		
		int id = IMGUI::GetControlID (state, 0, kPassive);

		InputEvent& evt (*state.m_CurrentEvent);
		
		switch (GetEventTypeForControl (state, evt, id)) {
		case InputEvent::kMouseDown:
			// If the mouse is inside the button, we say that we're the hot control
#if ENABLE_NEW_EVENT_SYSTEM
			if (position.Contains (evt.touch.pos))
#else
			if (position.Contains (evt.mousePosition))
#endif
			{
				GrabMouseControl (state, id);
				evt.Use ();
				//Matrix4x4f mat = win->m_Matrix;
				Vector2f mouseAbs = state.m_CanvasGUIState.m_GUIClipState.GetAbsoluteMousePosition();
				Vector3f windowAbs = win->m_Matrix.MultiplyPoint3 (Vector3f (win->m_Position.x, win->m_Position.y, 0));
				s_DragStartPos = mouseAbs - Vector2f (windowAbs.x, windowAbs.y);
				s_DragStartSize = Vector2f (win->m_Position.width, win->m_Position.height);
			}
			break;			
		case InputEvent::kMouseUp:
			if (GetHotControl (state) == id)
			{
				ReleaseMouseControl (state);
				evt.Use ();
			}
			break;
		case InputEvent::kMouseDrag:
			if (GetHotControl (state) == id)
			{
				Matrix4x4f mat;
				Matrix4x4f::Invert_Full (win->m_Matrix, mat);
				Vector2f mouseAbs = state.m_CanvasGUIState.m_GUIClipState.GetAbsoluteMousePosition();
				
				Vector3f deltaPos (mouseAbs.x - s_DragStartPos.x, mouseAbs.y - s_DragStartPos.y, 0); 
				deltaPos = mat.MultiplyPoint3 (deltaPos);
				win->m_Position = Rectf (deltaPos.x, deltaPos.y, s_DragStartSize.x, s_DragStartSize.y);
				
				win->m_Moved = true;
				evt.Use ();
			}
			break;			
		}
		
	}

	struct GUIStatePropertiesCache
	{
		Matrix4x4f mat;
		ColorRGBAf color;
		ColorRGBAf contentColor;
		ColorRGBAf backgroundColor;
		bool enabled;
	};

	void CacheGUIStateProperties (GUIState &state, GUIStatePropertiesCache &cache)
	{
		// Cache some GUIState properties to restore after we're done doing windows
		cache.mat = state.m_CanvasGUIState.m_GUIClipState.GetMatrix();
		cache.color = state.m_OnGUIState.m_Color;
		cache.contentColor = state.m_OnGUIState.m_ContentColor;
		cache.backgroundColor = state.m_OnGUIState.m_BackgroundColor;
		cache.enabled = state.m_OnGUIState.m_Enabled;
	}

	void RestoreGUIStateProperties (GUIState &state, InputEvent &evt, GUIStatePropertiesCache &cache)
	{
		// Restore previous GUIState properties
		state.m_CanvasGUIState.m_GUIClipState.SetMatrix (evt, cache.mat);
		state.m_OnGUIState.m_Color = cache.color;
		state.m_OnGUIState.m_ContentColor = cache.contentColor;
		state.m_OnGUIState.m_BackgroundColor = cache.backgroundColor;
		state.m_OnGUIState.m_Enabled = cache.enabled;
	}

	void BeginWindows (GUIState &state, bool setupClipping, bool ignoreModalWindow)
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		InputEvent& evt (*state.m_CurrentEvent);
		if (winState == NULL)
			return;

		GUIStatePropertiesCache oldProperties;
		CacheGUIStateProperties (state, oldProperties);

		if (setupClipping)
			state.m_CanvasGUIState.m_GUIClipState.BeginOnGUI (evt);
		
		if (winState->m_LayersChanged)
			winState->SortWindows();
		
		// The window we want to pass the event on to (mouse & key mainly)
		GUIWindow* win = NULL;
		
		switch (evt.type) {
			case InputEvent::kLayout:
				// Before we process any events... Mark all windows as unused (we mark them as used when doing the layout event for all scripts)
				for (GUIWindowState::WindowList::iterator i = winState->m_WindowList.begin(); i != winState->m_WindowList.end(); i++)
					(*i)->m_Used = false;
				
				if(!ignoreModalWindow && winState->m_ModalWindow != NULL)
					winState->m_ModalWindow->m_Used = false;
			break;
			
		// Dragging events go to the window UNDER the mouse
		case InputEvent::kDragUpdated:
		case InputEvent::kDragPerform:
		case InputEvent::kDragExited:
			if (!ignoreModalWindow && winState->m_ModalWindow != NULL)
				win = winState->m_ModalWindow;
			else
				win = winState->FindWindowUnderMouse (state);
			break;
					 
		// If we have a hot control, we send mouseUp/mouseDrag event to the active window.
		// If not, we send it to window under mouse.
		case InputEvent::kMouseUp: 
		case InputEvent::kMouseDrag:
		case InputEvent::kMouseMove:
			 if (!ignoreModalWindow && winState->m_ModalWindow != NULL)
				 win = winState->m_ModalWindow;
			 else if (GetHotControl (state) == 0)
				 win = winState->FindWindowUnderMouse (state);
			 else
				 win = winState->GetWindow (winState->m_FocusedWindow);
			 break;

		// Scroll wheel goes to window under mouse
		// TODO: Maybe not the same for windows
		case InputEvent::kScrollWheel:
			if (!ignoreModalWindow && winState->m_ModalWindow != NULL)
				win = winState->m_ModalWindow;
			else
				win = winState->FindWindowUnderMouse (state);
			break;
		// mouse events should pick a specific window & bring that forwards...
		case InputEvent::kMouseDown:
			winState->m_FocusedWindow = -1;
			if(!ignoreModalWindow && winState->m_ModalWindow != NULL)
				win = winState->m_ModalWindow;
			else
				win = winState->FindWindowUnderMouse (state);
				
			// If somebody got moved to front - we need to go over all windows and reset the window depth
			if (win) {
				win->m_Depth = -1;
				winState->m_FocusedWindow = win->m_ID;
				winState->SortWindows ();
			}
			break;
		case InputEvent::kRepaint:
			if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_1_a1))
				state.m_EternalGUIState->m_AllowHover = ((ignoreModalWindow || winState->m_ModalWindow == NULL) && winState->FindWindowUnderMouse (state) == NULL);
			// We handle all repainting in EndWindows (so we can draw in reverse order on top of other stuff)
			return;
		default:
			if(!ignoreModalWindow && winState->m_ModalWindow != NULL)
				win = winState->m_ModalWindow;
			else
				win = winState->GetWindow (winState->m_FocusedWindow);
			break;
		}
			
		// Pass the event on to the window 
		if (win != NULL && win->m_Delegate != 0)
		{
			win->OnGUI (state);
			
			// Some events should not be passed down to the GUI or windows underneath the handled window
			if(!ignoreModalWindow && winState->m_ModalWindow != NULL)
			{
				// If this is a scrollwheel or mousedown event, OR
				// If this is a mouse move/drag or mouseup event AND we have no active control
				// Ignore the event.
				if(evt.type == InputEvent::kScrollWheel || evt.type == InputEvent::kMouseDown)
				{
					evt.type = InputEvent::kIgnore;
				}
				else if( (evt.type == InputEvent::kMouseMove
						  || evt.type == InputEvent::kMouseDrag
						  || evt.type == InputEvent::kMouseUp)
						&& IMGUI::GetHotControl (state) == 0)
				{
					evt.type = InputEvent::kIgnore;
				}
			}
		}
		
		RestoreGUIStateProperties (state, evt, oldProperties);

		if (setupClipping)
			state.m_CanvasGUIState.m_GUIClipState.EndOnGUI (*state.m_CurrentEvent);
	}
	
	void EndWindows (GUIState &state, bool ignoreModalWindow)
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		if (winState == NULL)
			return;

		GUIStatePropertiesCache oldProperties;
		CacheGUIStateProperties (state, oldProperties);
		
		InputEvent& evt (*state.m_CurrentEvent);
		switch (evt.type) {
		case InputEvent::kLayout: 
		{
			// Remove unused windows (they have to be marked as Used during the Layout event.
			bool clearFocus = true;
			for (int i = winState->m_WindowList.size(); i--;)
			{
				GUIWindow* win = winState->m_WindowList[i];
				if (!win->m_Used)
				{
					delete win;
					winState->m_WindowList.erase(winState->m_WindowList.begin() + i);
					winState->m_LayersChanged = true;
				} else {
					if (win->m_ID == winState->m_FocusedWindow)
						clearFocus = false;
				}
			}
			
			if(!ignoreModalWindow && winState->m_ModalWindow != NULL && !winState->m_ModalWindow->m_Used)
			{
				delete winState->m_ModalWindow;
				winState->m_ModalWindow = NULL;
			}

			if (clearFocus)
				winState->m_FocusedWindow = -1;

			if (winState->m_LayersChanged)
				winState->SortWindows ();
			
			// Always run modal windows first
			if(!ignoreModalWindow && winState->m_ModalWindow != NULL)
				winState->m_ModalWindow->OnGUI(state);
			
			for (GUIWindowState::WindowList::iterator i = winState->m_WindowList.begin(); i != winState->m_WindowList.end(); i++)
			{
				// Send the layout event to the user's input code (also does the layouting from the C# delegate wrapper)
				(*i)->OnGUI (state);				
			}
			break;
		}
		case InputEvent::kRepaint:
			GUIWindow* windowUnderMouse;
				
			if(winState->m_ModalWindow != NULL)
				windowUnderMouse = winState->m_ModalWindow;
			else
				windowUnderMouse = winState->FindWindowUnderMouse (state);

			for (int i = winState->m_WindowList.size(); i--;)
			{
				GUIWindow* win = winState->m_WindowList[i];
				if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_1_a1))
					state.m_EternalGUIState->m_AllowHover = (win == windowUnderMouse && winState->m_ModalWindow == NULL);
				win->OnGUI (state);
			}
				
			if(ignoreModalWindow || winState->m_ModalWindow == NULL)
			{
				// Re-enable hovering for always-on-top normal GUIs when there's no modal GUI
				state.m_EternalGUIState->m_AllowHover = true;
			}
			else
			{
				// Disable hovering for the non-modal GUI when we have one.
				// Repainting happens in RepaintModalWindow.
				state.m_EternalGUIState->m_AllowHover = false;
			}
				
			break;
		}

		RestoreGUIStateProperties (state, evt, oldProperties);
		
		// Release objects if we don't have a modal window. If we do, this will be done later.
		if (evt.type != InputEvent::kLayout && (ignoreModalWindow || winState->m_ModalWindow == NULL))
			winState->ReleaseScriptingObjects();
	}
	
	void RepaintModalWindow(GUIState& state)
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		if (winState == NULL)
			return;
		
		GUIStatePropertiesCache oldProperties;
		CacheGUIStateProperties (state, oldProperties);
		
		InputEvent& evt (*state.m_CurrentEvent);
		if(evt.type == InputEvent::kRepaint)
		{
			state.m_EternalGUIState->m_AllowHover = true;
			
			// Always run modal windows last so they paint on top.
			if(winState->m_ModalWindow != NULL)
				winState->m_ModalWindow->OnGUI(state);
		}
		
		if(evt.type != InputEvent::kLayout)
		{
			winState->ReleaseScriptingObjects();
		}		
	}
	
	GUIWindow *GetFocusedWindow (GUIState &state)
	{
		if (state.m_MultiFrameGUIState.m_Windows)
			return state.m_MultiFrameGUIState.m_Windows->GetWindow (state.m_MultiFrameGUIState.m_Windows->m_FocusedWindow);
		return NULL;
	}


	GUIWindowState::GUIWindowState ()
	{
		m_FocusedWindow = -1;
		m_LayersChanged = false;
		m_CurrentWindow = NULL;
		m_ModalWindow = NULL;
	}

	
	GUIWindowState::~GUIWindowState ()
	{
		for (GUIWindowState::WindowList::iterator i = m_WindowList.begin(); i != m_WindowList.end(); i++)
			delete *i;
		
		if(m_ModalWindow != NULL)
		{
			delete m_ModalWindow;
			m_ModalWindow = NULL;
		}
	}
	
	GUIWindow* GUIWindowState::GetWindow (int windowId)
	{
		for (GUIWindowState::WindowList::iterator i = m_WindowList.begin(); i != m_WindowList.end(); i++)
		{
			if ((*i)->m_ID == windowId)
				return *i;
		}
		
		if(m_ModalWindow != NULL && m_ModalWindow->m_ID == windowId)
		{
			return m_ModalWindow;
		}

		return NULL;
	}

	static bool SortTwoWindows(const GUIWindow* a, const GUIWindow* b)
	{
		return a->m_Depth < b->m_Depth;
	}
	
	void GUIWindowState::SortWindows () 
	{
		std::sort (m_WindowList.begin(), m_WindowList.end(), SortTwoWindows);
		for (int i = 0; i < m_WindowList.size(); i++)
			m_WindowList[i]->m_Depth = i;
	}
	
	
	GUIWindow* GUIWindowState::FindWindowUnderMouse (GUIState &state)
	{
		InputEvent evt (*state.m_CurrentEvent);
		
#if ENABLE_NEW_EVENT_SYSTEM
		if(m_ModalWindow != NULL && m_ModalWindow->m_Position.Contains(evt.touch.pos))
#else
		if(m_ModalWindow != NULL && m_ModalWindow->m_Position.Contains(evt.mousePosition))
#endif
		{
			return m_ModalWindow;
		}
		
		for (GUIWindowState::WindowList::iterator i = m_WindowList.begin(); i != m_WindowList.end(); i++)
		{
			state.m_CanvasGUIState.m_GUIClipState.SetMatrix (evt, (*i)->m_Matrix); 
#if ENABLE_NEW_EVENT_SYSTEM
			if ((*i)->m_Position.Contains (evt.touch.pos))
#else
			if ((*i)->m_Position.Contains (evt.mousePosition)) 
#endif
				return *i;
		}
		return NULL;
	}
	
	void GUIWindowState::ReleaseScriptingObjects ()
	{
		for (WindowList::iterator i = m_WindowList.begin(); i != m_WindowList.end(); i++)
			(*i)->ReleaseScriptingObjects ();
		
		if(m_ModalWindow != NULL)
		{
			m_ModalWindow->ReleaseScriptingObjects();
		}
	}
	
	void MoveWindowFromLayout (GUIState &state, int windowID, const Rectf &rect)
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		AssertIf (!winState);

		GUIWindow* win = winState->GetWindow(windowID);
		if (win && win->m_Position != rect)
		{
			win->m_Position = rect;
			win->m_Moved = true;
		}
	}
	
	Rectf GetWindowRect (GUIState &state, int windowID)
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		AssertIf (!winState);
	
		GUIWindow* win = winState->GetWindow (windowID);
		if (win)
			return win->m_Position;
		return Rectf (0,0,0,0);
	}

	Rectf GetWindowsBounds (GUIState &state)
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		if (!winState)
			return Rectf(0,0,0,0);

		GUIWindowState::WindowList &windows = winState->m_WindowList;

		Rectf bounds (std::numeric_limits<float>::max (), std::numeric_limits<float>::max (), -std::numeric_limits<float>::max (), -std::numeric_limits<float>::max ());
		for (GUIWindowState::WindowList::const_iterator i = windows.begin (); i != windows.end (); i++)
		{
			Rectf& windowRect = (*i)->m_Position;

			bounds.SetLeft (std::min (bounds.x, windowRect.x));
			bounds.SetTop (std::min (bounds.y, windowRect.y));
			bounds.SetRight (std::max (bounds.GetXMax (), windowRect.GetXMax ()));
			bounds.SetBottom (std::max (bounds.GetYMax (), windowRect.GetYMax ()));
		}
		
		if(winState->m_ModalWindow != NULL)
		{
			Rectf& windowRect = winState->m_ModalWindow->m_Position;
			bounds.SetLeft (std::min (bounds.x, windowRect.x));
			bounds.SetTop (std::min (bounds.y, windowRect.y));
			bounds.SetRight (std::max (bounds.GetXMax (), windowRect.GetXMax ()));
			bounds.SetBottom (std::max (bounds.GetYMax (), windowRect.GetYMax ()));
		}

		return bounds;
	}

	void BringWindowToFront (GUIState &state, int windowID)
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		if (!winState)
			return;

		// Modal windows are on top by definition.
		if(winState->m_ModalWindow != NULL && winState->m_ModalWindow->m_ID == windowID)
			return;
		
		GUIWindow* win = winState->GetWindow (windowID);
		if (win)
		{
			int minDepth = 0;
			for (GUIWindowState::WindowList::iterator i = winState->m_WindowList.begin(); i != winState->m_WindowList.end(); i++)
			{
				if ((*i)->m_Depth < minDepth)
					minDepth = (*i)->m_Depth;
			}
			win->m_Depth = minDepth - 1;
			winState->m_LayersChanged = true; 
		}
	}
	
	void BringWindowToBack (GUIState &state, int windowID)
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		if (!winState)
			return;

		// Modal windows are on top by definition.
		if(winState->m_ModalWindow != NULL && winState->m_ModalWindow->m_ID == windowID)
			return;		
		
		GUIWindow* win = winState->GetWindow (windowID);
		if (win)
		{
			int maxDepth = 0;
			for (GUIWindowState::WindowList::iterator i = winState->m_WindowList.begin(); i != winState->m_WindowList.end(); i++)
			{
				if ((*i)->m_Depth > maxDepth)
					maxDepth = (*i)->m_Depth;
			}
			win->m_Depth = maxDepth + 1;
			winState->m_LayersChanged = true; 
		}
	}
	
	void FocusWindow (GUIState &state, int windowID)
	{
		GUIWindowState* winState = state.m_MultiFrameGUIState.m_Windows;
		if (!winState)
			return;

		// Modal windows must be focused if they exist.
		if(winState->m_ModalWindow != NULL && winState->m_ModalWindow->m_ID != windowID)
			return;

		winState->m_FocusedWindow = windowID;		
	}	
}
#endif
