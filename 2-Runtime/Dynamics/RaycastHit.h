#pragma once

#include "Runtime/Math/Vector2.h"
#include "Collider.h"

Vector2f CalculateRaycastTexcoord (Collider* collider, const Vector2f& uv, const Vector3f& pos, UInt32 face, int texcoord);
