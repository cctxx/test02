#pragma once

#include "Runtime/Math/Rect.h"
class Object;
struct MonoObject;

void DisplayObjectContextPopupMenu (const Rectf &pos, std::vector<Object*> &context, int contextData);

void DisplayCustomContextPopupMenu (const Rectf &pos, const std::vector<std::string>& enums, const std::vector<bool> &enabled, const std::vector<int>& selected, MonoObject* monoDelegate, MonoObject* monoUserData);

void PrepareObjectContextPopupMenu (std::vector<Object*> &context, int contextData);

void CleanupCustomContextMenuHandles ();

