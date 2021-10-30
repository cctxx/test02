#ifndef IMGUIUtils_H
#define IMGUIUtils_H

#include "Runtime/Math/Color.h"
#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/IMGUI/GUIContent.h"
#include "Runtime/IMGUI/GUIStyle.h"

struct InputEvent;
struct GUIState;
class TextMeshGenerator2;
namespace IMGUI
{
	inline ColorRGBAf GetColor (const GUIState &state)							{ return state.m_OnGUIState.m_Color; }
	inline void SetColor (GUIState &state, const ColorRGBAf &color)				{ state.m_OnGUIState.m_Color = color; }

	inline ColorRGBAf GetBackgroundColor (const GUIState &state)				{ return state.m_OnGUIState.m_BackgroundColor; }
	inline void SetBackgroundColor (GUIState &state, const ColorRGBAf &color)	{ state.m_OnGUIState.m_BackgroundColor = color; }

	inline ColorRGBAf GetContentColor (const GUIState &state)					{ return state.m_OnGUIState.m_ContentColor; }
	inline void SetContentColor (GUIState &state, const ColorRGBAf &color)		{ state.m_OnGUIState.m_ContentColor = color; }
	
	inline bool GetEnabled (const GUIState &state)								{ return state.m_OnGUIState.m_Enabled; }
	inline void SetEnabled (GUIState &state, bool enab)							{ state.m_OnGUIState.m_Enabled = enab; }
	
	inline bool GetChanged (const GUIState &state)								{ return state.m_OnGUIState.m_Changed; }
	inline void SetChanged (GUIState &state, bool changed)						{ state.m_OnGUIState.m_Changed = changed; }
	
	inline int GetDepth (const GUIState &state)									{ return state.m_OnGUIState.m_Depth; }
	inline void SetDepth (GUIState &state, int depth)							{ state.m_OnGUIState.m_Depth = depth; }
	
	inline int GetHotControl (const GUIState &state)							{ return state.m_EternalGUIState->m_HotControl; }
	inline void SetHotControl (GUIState &state, int hotControl)					{ state.m_EternalGUIState->m_HotControl = hotControl; }
	
	inline int GetKeyboardControl (const GUIState &state)						{ return state.m_MultiFrameGUIState.m_KeyboardControl; }
	inline void SetKeyboardControl (GUIState &state, int keyControl)			{ state.m_MultiFrameGUIState.m_KeyboardControl = keyControl; }
	
	inline InputEvent &GetCurrentEvent (GUIState &state)						{ return *state.m_CurrentEvent; }
	
	inline bool GetGUIClipEnabled (const GUIState &state)						{ return state.m_CanvasGUIState.m_GUIClipState.GetEnabled();}

	/// Get the type of the current event taking clipping and enabled flag into account from GUIState.
	InputEvent::Type GetEventType (const GUIState &state, const InputEvent &event);
	
	/// Get the type of the current event considering that a specific control is asking.
	/// This is more agressive than the old C# one as it will also cull mouse events, assuming that hotControl is being used.
	InputEvent::Type GetEventTypeForControl (const GUIState &state, const InputEvent &event, int controlID); 	
	
	inline int GetControlID (GUIState &state, int hash, FocusType focusType)	
		{ return state.GetControlID (hash, focusType); }
	
	inline int GetControlID (GUIState &state, int hash, FocusType focusType, const Rectf& rect)
		{ return state.GetControlID (hash, focusType, rect); }
	
	
	inline void GrabMouseControl (GUIState &state, int controlID)				{ state.m_EternalGUIState->m_HotControl = controlID; }
	inline void ReleaseMouseControl (GUIState &state)							{ state.m_EternalGUIState->m_HotControl = 0; }
	inline bool HasMouseControl (const GUIState &state, int controlID)			{ return state.m_EternalGUIState->m_HotControl == controlID; }	

	inline bool AreWeInOnGUI (const GUIState &state)							{ return state.m_OnGUIDepth > 0; }

	TextMeshGenerator2* GetGenerator (const Rectf &contentRect, const GUIContent &content, Font& font, TextAnchor alignment, bool wordWrap, bool richText, ColorRGBA32 color, int fontSize, int fontStyle, ImagePosition imagePosition);
}
#endif
