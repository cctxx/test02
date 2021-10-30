#ifndef __HIGHLIGHTERCORE_H__
#define __HIGHLIGHTERCORE_H__

#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/IMGUI/TextUtil.h"
#include "Runtime/IMGUI/GUIState.h"
struct InputEvent;

/// This needs to be in Sync with HighlightSearchMode in C#
enum HighlightSearchMode
{
	kHighlightNone = 0,
	kHighlightAuto = 1,
	kHighlightByIdentifier = 2,
	kHighlightByPrefixLabel = 3,
	kHighlightByContent = 4
};

class HighlighterCore
{
	
public:
	static UTF16String				s_ActiveText;
	static Rectf					s_ActiveRect;
	static bool						s_ActiveVisible;
	static HighlightSearchMode		s_SearchMode;
	
	static void Handle (const GUIState& state, Rectf position, const UTF16String& text);
};

#endif // __HIGHLIGHTERCORE_H__
