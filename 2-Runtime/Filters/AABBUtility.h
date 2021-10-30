#ifndef AABBUTILITY_H
#define AABBUTILITY_H

#include "Runtime/Geometry/AABB.h"

class Transform;
class Vector3f;
namespace Unity { class GameObject; }

bool EXPORT_COREMODULE CalculateLocalAABB (Unity::GameObject& go, AABB* aabb);

AABB CalculateWorldAABB (Unity::GameObject& go);
bool CalculateWorldAABB (Unity::GameObject& go, AABB* aabb);

bool CalculateAABBSkinned (Transform& transform, AABB& aabb);
bool CalculateAABBCornerVertices (Unity::GameObject& go, Vector3f* vertices);

#endif
