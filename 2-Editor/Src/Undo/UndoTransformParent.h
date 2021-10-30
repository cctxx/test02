#pragma once

#include "Runtime/Graphics/Transform.h"

bool SetTransformParentUndo (Transform& transform, Transform* newParent, Transform::SetParentOption option, const std::string& actionName);
bool SetTransformParentUndo (Transform& transform, Transform* newParent, const std::string& actionName);
