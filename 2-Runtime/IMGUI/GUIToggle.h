#ifndef GUITOGGLE_H
#define GUITOGGLE_H

#include "Runtime/Math/Rect.h"

struct GUIState;
struct GUIContent;
class GUIStyle;

namespace IMGUI
{
	bool GUIToggle (GUIState &state, const Rectf &position, bool value, GUIContent &content, GUIStyle &style, int id);
	bool GUIToggle (GUIState &state, const Rectf &position, bool value, GUIContent &content, GUIStyle &style);
}

#endif
