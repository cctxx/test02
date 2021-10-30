#ifndef PREFAB_REPLACING_H
#define PREFAB_REPLACING_H

#include "PrefabUtility.h"

void ReplacePrefab (GameObject& rootInstanceGameObject, const std::vector<ObjectData>& instanceObjects, const std::vector<ObjectData>& additionalInstantiatedAssets, const std::vector<Object*>& existingPrefabObjects, std::vector<ObjectData>& outputPrefabObjects, ReplacePrefabOptions mode);

#endif