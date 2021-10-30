#include "UnityPrefix.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/Mono/MonoScript.h"
#include "Application.h"

GUIViews g_GUIViews;
GUIView* GUIView::s_StartGUIView = 0;

bool FrameLastActiveSceneView(const bool lock)
{
	if(lock)
		return MonoObjectToBool(ScriptingInvocation("UnityEditor","SceneView","FrameLastActiveSceneViewWithLock").Invoke());
	
	return MonoObjectToBool(ScriptingInvocation("UnityEditor","SceneView","FrameLastActiveSceneView").Invoke());
}

bool CallGlobalInputEvent (const InputEvent& event)
{
	#if UNITY_WIN && ENABLE_WINDOW_MESSAGES_LOGGING
	printf_console("EW: CallGlobalInputEvent\n");
	#endif
	
	GetGUIState().SetEvent(event);
	GetGUIState().m_OnGUIDepth++;
	ScriptingInvocation(MONO_COMMON.callGlobalEventHandler).Invoke();
	GetGUIState().m_OnGUIDepth--;
	
	return GetGUIState().m_CurrentEvent->type == InputEvent::kUsed;
}

void GUIView::ProjectWindowHasChanged ()
{
	CallMethod ("OnProjectChange");
}

void GUIView::SelectionHasChanged (const std::set<int>& selection)
{
	CallMethod ("OnSelectionChange");
}

void GUIView::DidOpenScene ()
{
	CallMethod ("OnDidOpenScene");
}

void GUIView::HierarchyWindowHasChanged ()
{
	CallMethod ("OnHierarchyChange");
}

void GUIView::TickInspector ()
{
	CallMethod ("OnInspectorUpdate");
}

bool ValidateCommandOnKeyWindowAndMouseOverWindow (const std::string& commandName)
{
	GUIView* mouseOverView = GetMouseOverWindow ();
	if (mouseOverView)
	{
		InputEvent validateEvent = InputEvent::CommandStringEvent (commandName, false);
		mouseOverView->UpdateScreenManager ();
		if (mouseOverView->OnInputEvent(validateEvent))
			return true;
	}
	GUIView* view = GetKeyGUIView ();
	if (view)
	{
		view->UpdateScreenManager ();
		InputEvent validateEvent = InputEvent::CommandStringEvent (commandName, false);
		if (view->OnInputEvent(validateEvent))
			return true;
	}
	
	return false;
}

void GUIView::ClearCursorRects () 
{
	m_CursorRects.clear();
}

void GUIView::AddCursorRect (const Rectf &position, MouseCursor mouseCursor) 
{
	AddCursorRect (position, mouseCursor, 0); 
}

void GUIView::AddCursorRect (const Rectf &position, MouseCursor mouseCursor, int controlID) 
{
	m_CursorRects.push_back( CursorRect(position,mouseCursor,controlID) );
}

bool ValidateCommandOnKeyWindow (const std::string& commandName)
{
	GUIView* view = GetKeyGUIView ();
	if (view)
	{
		#if UNITY_WIN
		view->UpdateScreenManager();
		#endif
		InputEvent validateEvent = InputEvent::CommandStringEvent (commandName, false);
		return view->OnInputEvent(validateEvent);
	}
	return false;
}

bool ExecuteCommandOnAllWindows (const std::string& commandName)
{
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView* view = *i;
		if (view)
		{
			view->UpdateScreenManager();
			InputEvent validateEvent = InputEvent::CommandStringEvent (commandName, false);
			if (view->OnInputEvent(validateEvent))
			{
				InputEvent event = InputEvent::CommandStringEvent (commandName, true);
				return view->OnInputEvent(event);
			}
		}
	}

	return false;
}

void GUIView::RecreateAllOnAAChange ()
{
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView& view = **i;
		if (view.m_AntiAlias == -1)
			view.RecreateContext (view.m_DepthBits, view.m_AntiAlias);
	}
}

void GUIView::RecreateAllOnDepthBitsChange ( int from, int to )
{
	if( from == to )
		return;

	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView* view = *i;
		if (view->m_DepthBits == from)
			view->RecreateContext (to, view->m_AntiAlias);
	}
}
