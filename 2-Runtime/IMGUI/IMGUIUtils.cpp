#include "UnityPrefix.h"
#include "Runtime/IMGUI/IMGUIUtils.h"
#include "Runtime/IMGUI/TextMeshGenerator2.h"

const float s_TabWidth = 16;
extern float s_GUIStyleIconSizeX;

namespace IMGUI
{
	InputEvent::Type GetEventType (const GUIState &state, const InputEvent &event)
	{
		InputEvent::Type type = event.type;
		if (!state.m_OnGUIState.m_Enabled)
		{
			if (type == InputEvent::kRepaint || type == InputEvent::kLayout || type == InputEvent::kUsed)
				return type;
			return InputEvent::kIgnore;
		}
		
		if (state.m_CanvasGUIState.m_GUIClipState.GetEnabled())
			return type;
		
		if (type == InputEvent::kMouseDown || type == InputEvent::kMouseUp || type == InputEvent::kDragPerform || type == InputEvent::kDragUpdated)
			return InputEvent::kIgnore;
		
		return type;
	}
	
	
	InputEvent::Type GetEventTypeForControl (const GUIState &state, const InputEvent &event, int controlID)
	{
		InputEvent::Type m_Type = event.type;
	
		// if we have no hot control, just return the usual
		if (GetHotControl(state) == 0)
			return GetEventType (state, event);
		switch (m_Type)
		{
			// Mouse events follow GUIUtility.hotControl
			case InputEvent::kMouseDown:
			case InputEvent::kMouseUp:
			case InputEvent::kMouseMove:
			case InputEvent::kMouseDrag:
				if (!GetEnabled(state))
					return InputEvent::kIgnore;
				if (GetGUIClipEnabled(state) || HasMouseControl (state, controlID))
					return m_Type;
				return InputEvent::kIgnore;
			
			// Key events follow keyboard control
			case InputEvent::kKeyDown:
			case InputEvent::kKeyUp:
			case InputEvent::kScrollWheel: // Not sure about scrollwheel. For now we map it to behave like keyboard events.

				if (!GetEnabled(state))
					return InputEvent::kIgnore;
				if (GetGUIClipEnabled(state) || HasMouseControl (state, controlID) || GetKeyboardControl(state) == controlID)
					return m_Type;
				return InputEvent::kIgnore;
				
			// Repaint, Layout, Used, DragUpdated, DragPerform, Ignore
			default:
				return m_Type;
		}
	}

	
	TextMeshGenerator2* GetGenerator (const Rectf &contentRect, const GUIContent &content, Font& font, TextAnchor alignment, bool wordWrap, bool richText, ColorRGBA32 color, int fontSize, int fontStyle, ImagePosition imagePosition)
	{	
		if (!wordWrap)
			return &TextMeshGenerator2::Get (content.m_Text, &font, alignment, kAuto, 0.0f, s_TabWidth, 1.0f, richText, true, color, fontSize, fontStyle);
		
		Texture *image = content.m_Image;	
		float textWidth = contentRect.Width();
		switch (imagePosition) {
			case kImageLeft:
				// Todo: Subtract width of the icon
				if (image != NULL)
				{
					Vector2f imageSize = Vector2f (image->GetDataWidth(), image->GetDataHeight());
					
					//float imageScale = clamp (min (contentRect.Width() / imageSize.x, contentRect.Height() / imageSize.y), 0.0f, 1.0f);
					//contentRect.width -=  Roundf (imageSize.x * imageScale);
					
					if (s_GUIStyleIconSizeX == 0)
						textWidth -= Roundf (imageSize.x * clamp (std::min (contentRect.Width() / imageSize.x, contentRect.Height() / imageSize.y), 0.0f, 1.0f));
					else
						textWidth -= s_GUIStyleIconSizeX;
				}
				break;
			case kImageOnly:
				return NULL;
			case kImageAbove:
			case kTextOnly:
				// These two require no special handling
				break;
		}
		
		return &TextMeshGenerator2::Get (content.m_Text, &font, alignment, kAuto, textWidth, s_TabWidth, 1.0f, richText, true, color, fontSize, fontStyle);
	}
}	// namespace IMGUI
