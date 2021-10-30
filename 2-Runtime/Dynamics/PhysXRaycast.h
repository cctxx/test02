#ifndef PHYSXRAYCAST_H
#define PHYSXRAYCAST_H

#include "UnityPrefix.h"
#if ENABLE_MULTITHREADED_CODE
#include "Runtime/Threads/SimpleLock.h"
#endif
#include "Runtime/Interfaces/IRaycast.h"
#include "Runtime/Geometry/AABB.h"

class Plane;
class NxShape;

/// PhysX thread safety:
/// Unfortunately scene query + concurrent writes to the SDK are not thread safe. However, concurrent reads such as scene queries from multiple threads should be safe.
/// But simulate calls count as writes, so is not OK at the same time.
class PhysXRaycast : public IRaycast
{
public:	
	static void InitializeClass ();
	static void CleanupClass ();

	// virtual interface implementation	
	virtual size_t BatchIntersect( const dynamic_array<BatchedRaycast>& raycasts, dynamic_array<BatchedRaycastResult>& results, const UInt32 filter, bool staticOnly ) const;
	virtual bool Raycast (const Ray& ray, float distance, int mask, HitInfo& hit);

	PhysXRaycast() {}
	~PhysXRaycast() {}

private:
	// helper functions for working with the NxShapes
	bool Intersect( const Ray& ray, float maxDist, NxShape** shapes, AABB* shapeBounds, size_t shapeCount, HitInfo& hit ) const;
	size_t GetShapes( const AABB& bounds, int maxResult, NxShape** outShapes, UInt32 groupMask = 0xffffffff, bool staticOnly = false ) const;
	void GetAABB( AABB& bounds, const NxShape& shape ) const;
	size_t BroadPhaseCulling(dynamic_array<NxShape*>& nxShapes, dynamic_array<AABB>& aabbs, const MinMaxAABB& particleSystemAABB, const int filter, bool staticOnly) const;

#if ENABLE_MULTITHREADED_CODE
	mutable SimpleLock m_Lock;
#endif
};

#endif
