#ifndef IRAYCAST_H
#define IRAYCAST_H

#include "Runtime/Math/Vector3.h"
#include "Runtime/Geometry/Ray.h"
#include "Runtime/Utilities/NonCopyable.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Modules/ExportModules.h"

class AABB;
class MinMaxAABB;
class Plane;
class Collider;
struct RaycastHit;

// Collision intersection
struct HitInfo
{
	Vector3f	intersection;
	Vector3f	normal;
	int			colliderInstanceID;				// The instanceID of the intersected Collider component.
	int			rigidBodyOrColliderInstanceID;	// The instanceID of the RigidBody to which the Collider is attached. If there is none then the ID is the same as colliderInstanceID.
};

// Batch intersection result
struct BatchedRaycastResult
{
	BatchedRaycastResult(unsigned int i, const HitInfo& h) : index(i), hitInfo(h) {}
	unsigned int index;	// the index of the result into BatchedRaycast array
	HitInfo hitInfo;	// the intersection
};

// Batch intersection request
struct BatchedRaycast
{
	BatchedRaycast(unsigned int i, const Vector3f& f, const Vector3f& t) : index(i), from(f), to(t) {}
	unsigned int index;	// user provided index
	Vector3f from;		// ray origin
	Vector3f to;		// ray endpoint
};

/// Abstract ray tracing class
class EXPORT_COREMODULE IRaycast : public NonCopyable
{
public:
	/// ======================================	
	/// Useful intersection methods

	/// Returns true if any shape is intersected along the ray within maxDist
	static bool IntersectAny( const Ray& ray, float maxDist, AABB* shapeBounds, size_t shapeCount );

	///	Version of above taking a pair of points
	static bool IntersectAny( const BatchedRaycast& ray, AABB* shapeBounds, size_t shapeCount );

	/// ======================================
	/// Virtual interface

	/// Intersects all the rays and returns any intersections in results, the return value specifies how many intersections were found.
	/// The index entry within BatchedRaycast is user provided while the index in BatchedRaycastResult specifies the BatchedRaycast that it is a result.
	size_t virtual BatchIntersect( const dynamic_array<BatchedRaycast>& raycasts, dynamic_array<BatchedRaycastResult>& results, const UInt32 filter, bool staticOnly ) const = 0;	
	
	bool virtual Raycast (const Ray& ray, float distance, int mask, HitInfo& hit) = 0;

protected:
	/// Helper function for computing AABB collection of rays
	static MinMaxAABB ComputeBatchAABB( const dynamic_array<BatchedRaycast>& raycasts);
};

/// Singleton access
EXPORT_COREMODULE IRaycast* GetRaycastInterface();
EXPORT_COREMODULE void SetRaycastInterface(IRaycast* theInterface); // argh, using interface raises an C2332

#endif
