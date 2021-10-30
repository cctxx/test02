#pragma once

// This is the public API for Undo.
// Implementation of these functions lives in their specific implementation files.
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Graphics/Transform.h"

class MonoScript;
namespace Unity { class GameObject; class Component; }

/// This is to be used for all normal modifications.
/// It will automatically record the change as a PropertyModification after the operation is flushed. (Flush happens automatically)
/// This is the smallest possible change to store & it integrates with the animation record mode etc.
void RecordUndoDiff (Object** o, int size, const std::string& actionName);
void RecordUndoDiff (Object* o, const std::string& actionName);

/// Use this when a new object was created and the undo operation should destroy it.
void RegisterCreatedObjectUndo (Object* o, const std::string& actionName);
void RegisterCreatedObjectUndo (PPtr<Object> o, bool isSceneUndo, const std::string& actionName);

/// Use this when the performance overhead of RecordUndoDiff makes it unattractive or when you want to recreate the object which was destroyed.
void RegisterUndo (Object* o, const std::string& actionName, int namePriority = 0);
void RegisterUndo (Object* identifier, Object** o, int size, const std::string& actionName, int namePriority = 0);

void RegisterFullObjectHierarchyUndo (Object* o);

/// Use this to undo destruction of an object (Recreates the object in the undo operation)
void DestroyObjectUndoable (Object* o);

Unity::Component* AddComponentUndoable (Unity::GameObject& go, int classID, MonoScript* script, std::string* error = NULL);

/// Undoable SetTransformParent
bool SetTransformParentUndo (Transform& transform, Transform* newParent, Transform::SetParentOption option, const std::string& actionName);
bool SetTransformParentUndo (Transform& transform, Transform* newParent, const std::string& actionName);

// Register selection state
void RegisterSelectionUndo ();
