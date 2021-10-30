#pragma once

#include "Runtime/BaseClasses/GameObject.h"
#include "Editor/Src/Prefabs/PropertyModification.h"
#include "Runtime/Animation/EditorCurveBinding.h"

ClassIDType PropertyModificationToEditorCurveBinding (const PropertyModification& modification, Unity::GameObject& rootGameObject, EditorCurveBinding& outBinding);
void EditorCurveBindingToPropertyModification (Unity::GameObject& rootGameObject, const EditorCurveBinding& binding, PropertyModification& modification);
