#include "UnityPrefix.h"
#include "Runtime/IMGUI/GUIButton.h"
#include "Runtime/IMGUI/GUIStyle.h"
#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/IMGUI/IMGUIUtils.h"

namespace IMGUI
{
void GUILabel (GUIState &state, const Rectf &position, GUIContent &content, GUIStyle &style)
{
	InputEvent &evt (*state.m_CurrentEvent);
	
	if (evt.type == InputEvent::kRepaint)
	{
		style.Draw (state, position, content, false, false, false, false);
		
		// Is inside label AND inside guiclip visible rect (prevents tooltips on labels that are clipped)
#if ENABLE_NEW_EVENT_SYSTEM
		if (content.m_Tooltip.length != 0 && position.Contains (evt.touch.pos) &&
			state.m_CanvasGUIState.m_GUIClipState.GetVisibleRect().Contains(evt.touch.pos))
#else
		if (content.m_Tooltip.length != 0 && position.Contains (evt.mousePosition) &&
			state.m_CanvasGUIState.m_GUIClipState.GetVisibleRect().Contains(evt.mousePosition))
#endif
			GUIStyle::SetMouseTooltip (state, content.m_Tooltip, position);
	}
}	

} // namespace
