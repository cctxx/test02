#include "UnityPrefix.h"
#include "HighlighterCore.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/IMGUI/GUIManager.h"


UTF16String			HighlighterCore::s_ActiveText = UTF16String();
Rectf				HighlighterCore::s_ActiveRect = Rectf();
bool				HighlighterCore::s_ActiveVisible = false;
HighlightSearchMode	HighlighterCore::s_SearchMode = kHighlightNone;

void HighlighterCore::Handle (const GUIState& state, Rectf position, const UTF16String& text)
{
	InputEvent &evt (*state.m_CurrentEvent);

	if (evt.type == InputEvent::kRepaint && HighlighterCore::s_ActiveText == text) 
	{
		Vector2f scrollNeeded = Vector2f();
		
		if (!s_ActiveVisible)
		{
			// If the highlight rect (with comfortable padding) is not yet visible in a scroll view, scroll towards it
			const float padding = 30.0f;
			Rectf paddedPosition = Rectf(position.x-padding, position.y-padding, position.width+padding*2, position.height+padding*2);
			float scrollSpeed = 10.0f;
			void* arg[] = { &paddedPosition, &scrollSpeed };
			s_ActiveVisible = !MonoObjectToBool(CallStaticMonoMethod ("GUI", "ScrollTowards", arg));
		}
		else
		{
			// If the highlight rect (without padding) was already visible but is no longer, stop the highlight.
			// (ScrollTowards returns true if scrolling is needed, even if no scrolling is applied due to maxDelta of 0.)
			float scrollSpeed = 0.0f;
			void* arg[] = { &position, &scrollSpeed };
			if (MonoObjectToBool(CallStaticMonoMethod ("GUI", "ScrollTowards", arg)))
			{
				s_ActiveRect = Rectf();
				return;
			}
		}
		
		GUIClipState clipState = state.m_CanvasGUIState.m_GUIClipState;

		s_ActiveRect = clipState.Unclip(position);
		Vector2f editorWindowOffset = GetGUIManager().GetEditorGUIInfo();
		s_ActiveRect.x += editorWindowOffset.x;
		s_ActiveRect.y += editorWindowOffset.y;
		
		s_SearchMode = kHighlightNone;
	}
}
