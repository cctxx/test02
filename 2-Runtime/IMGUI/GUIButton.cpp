#include "UnityPrefix.h"
#include "Runtime/IMGUI/GUIButton.h"
#include "Runtime/IMGUI/GUIStyle.h"
#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/IMGUI/IMGUIUtils.h"

static const int kGUIButtonHash = 2001146706;

namespace IMGUI
{

bool GUIButton (GUIState &state, const Rectf &position, GUIContent &content, GUIStyle &style, int id)
{
	InputEvent &evt (*state.m_CurrentEvent);
	switch (GetEventTypeForControl (state, evt, id))
	{
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
		}
		break;
	case InputEvent::kMouseUp:
		if (HasMouseControl (state, id))
		{
			ReleaseMouseControl (state);
				
			// If we got the mousedown, the mouseup is ours as well
			// (no matter if the click was in the button or not)
			evt.Use ();
				
			// toggle the passed-in value if the mouse was over the button & return true
#if ENABLE_NEW_EVENT_SYSTEM
			if (position.Contains (evt.touch.pos))
#else
			if (position.Contains (evt.mousePosition))
#endif
			{
				state.m_OnGUIState.m_Changed = true;
				return true;
			}
		}
		break;
	case InputEvent::kKeyDown:
		if (evt.character == 32 && state.m_MultiFrameGUIState.m_KeyboardControl == id)
		{
			evt.Use ();
			state.m_OnGUIState.m_Changed = true;
			return true;
		}
		break;
	case InputEvent::kMouseDrag:
		if (HasMouseControl (state, id))
			evt.Use ();
		break;
		
	case InputEvent::kRepaint:
		style.Draw (state, position, content, id, false);
		break;
	}
	return false;
}	

bool GUIButton (GUIState &state, const Rectf &position, GUIContent &content, GUIStyle &style)
{
	int id = GetControlID (state, kGUIButtonHash, kNative, position);
	return GUIButton (state, position, content, style, id);
}

	
}
