#ifndef GUILABEL_H
#define GUILABEL_H

#include "Runtime/Math/Rect.h"

struct GUIState;
struct GUIContent;
class GUIStyle;

namespace IMGUI
{
	void GUILabel (GUIState &state, const Rectf &position, GUIContent &content, GUIStyle &style);
}

#endif
