#pragma once

#include "PrefabUtility.h"

class Renderer;
class Object;


Object* InstantiateAttachedAsset (Object& asset);

void RevertInstantiatedAssetReferencesToParentObject (std::vector<ObjectData>& instanceObjects);
void GetInstantiatedAssetsToObjectArray (const std::vector<ObjectData>& instanceObjects, std::vector<ObjectData>& instancedAssets);