#ifndef NXWRAPPERUTILITY_H
#define NXWRAPPERUTILITY_H

#include "External/PhysX/builds/SDKs/Foundation/include/NxVec3.h"
#include "External/PhysX/builds/SDKs/Foundation/include/NxVersionNumber.h"
#include "External/PhysX/builds/SDKs/NxCharacter/include/NxExtended.h"

inline NxExtendedVec3 Vec3ToNxExtended (const Vector3f& v) { return NxExtendedVec3(v.x, v.y, v.z); }
inline Vector3f NxExtendedToVec3 (const NxExtendedVec3& v) { return Vector3f(v.x, v.y, v.z); }

inline NxVec3 Vec3ToNx (const Vector3f& v) { return NxVec3(v.x, v.y, v.z); }
inline Vector3f Vec3FromNx (const NxVec3& v) { return Vector3f(v.x, v.y, v.z); }

#endif
