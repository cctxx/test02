#include "UnityPrefix.h"
#include "IRaycast.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include <limits>

// Test if ray intersects at least one of the aabbs
bool IRaycast::IntersectAny( const Ray& ray, float maxDistance, AABB* shapeBounds, size_t shapeCount )
{
	const Vector3f halfDir = ray.GetDirection() * maxDistance * 0.5;
	const AABB rayBounds(ray.GetOrigin() + halfDir, Abs(halfDir));
	for (size_t s = 0; s < shapeCount; ++s)
	{
		if (IntersectAABBAABBInclusive(rayBounds, shapeBounds[s]))
			return true;
	}
	return false;
}

// Test if ray intersects at least one of the aabbs
bool IRaycast::IntersectAny( const BatchedRaycast& ray, AABB* shapeBounds, size_t shapeCount )
{
	const Vector3f halfDir = (ray.to-ray.from) * 0.5;
	const AABB rayBounds(ray.from + halfDir, Abs(halfDir));
	for (size_t s = 0; s < shapeCount; ++s)
	{
		if (IntersectAABBAABBInclusive(rayBounds, shapeBounds[s]))
			return true;
	}
	return false;
}

// AABB of ray segments expanded with particle radius
MinMaxAABB IRaycast::ComputeBatchAABB( const dynamic_array<BatchedRaycast>& raycasts )
{
	MinMaxAABB aabb;
	for (size_t q = 0; q < raycasts.size(); ++q)
	{
		aabb.Encapsulate(raycasts[q].from);
		aabb.Encapsulate(raycasts[q].to);
	}	
	return aabb;
}

static IRaycast* gRaycaster = NULL;

IRaycast* GetRaycastInterface()
{
	return gRaycaster;
}

void SetRaycastInterface(IRaycast* theInterface)
{
	gRaycaster = theInterface;
}
