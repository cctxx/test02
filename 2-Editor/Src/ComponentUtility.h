#pragma once

#include <vector>
#include "Runtime/BaseClasses/GameObject.h"

class Object;

bool MoveComponentUp (const std::vector<Object*>& context, bool validateOnly);
bool MoveComponentDown (const std::vector<Object*>& context, bool validateOnly);
bool CopyComponent (const std::vector<Object*>& context, bool validateOnly);
bool PasteComponentAsNew (const std::vector<Object*>& context, bool validateOnly);
bool PasteComponentValues (const std::vector<Object*>& context, bool validateOnly);

bool RemoveComponentUndoable (Unity::Component& component, std::string* error);
