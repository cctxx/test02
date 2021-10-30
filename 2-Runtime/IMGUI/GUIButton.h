#ifndef GUIBUTTON_H
#define GUIBUTTON_H

#include "Runtime/Math/Rect.h"

struct GUIState;
struct GUIContent;
class GUIStyle;

namespace IMGUI
{
	bool GUIButton (GUIState &state, const Rectf &position, GUIContent &content, GUIStyle &style, int id);
	bool GUIButton (GUIState &state, const Rectf &position, GUIContent &content, GUIStyle &style);	
}

#endif
