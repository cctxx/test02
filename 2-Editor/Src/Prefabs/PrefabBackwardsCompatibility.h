
#ifndef PREFAB_BACKWARDS_COMPATIBILITY_H
#define PREFAB_BACKWARDS_COMPATIBILITY_H

#include <set>
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Prefab.h"

void RemapOverride (EditorExtension& object, const TypeTree& oldTypeTree);
void RemapOverride (dynamic_bitset& override, const TypeTree& oldTypeTree, const TypeTree& newTypeTree);


void RemapOldPrefabOverride (Object& object, const TypeTree& oldTypeTree);
void ReadOldPrefabFormat (Prefab& prefab, EditorExtension& targetObject, EditorExtension& prefabParentObject, EditorExtensionImpl& impl);
void RemapOldPrefabOverrideFromLoading (Object& object, const TypeTree& oldTypeTree);

std::vector<GameObject*> OldPrefabObjectContainerToRootGameObjects (const Prefab::DeprecatedObjectContainer& objects);
void ReadOldRootGameObjectOverrides (GameObject* rootGameObject, Prefab& prefab);
void UpgradeDeprecatedObjectSetAndOverrides (Prefab& prefab);

#endif
