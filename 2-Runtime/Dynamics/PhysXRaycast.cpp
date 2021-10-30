#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "PhysXRaycast.h"

#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Dynamics/Collider.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include "Runtime/Dynamics/RigidBody.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Geometry/Plane.h"
#include <limits>

// a small value to expand the shape AABBs with in order to make sure there are not boundary issues when broadphase culling the raycasts
static const float kAABBEpsilon	= 0.00001f;
static const float kEpsilon		= 0.000001f;

void PhysXRaycast::InitializeClass ()
{
	SetRaycastInterface (new PhysXRaycast ());
}

void PhysXRaycast::CleanupClass ()
{
	PhysXRaycast* raycast = reinterpret_cast<PhysXRaycast*> (GetRaycastInterface ());
	delete raycast;
	SetRaycastInterface (NULL);
}

static inline int GetAttachedRigidbodyOrColliderInstanceID (Collider& collider)
{
	Rigidbody* body = collider.GetRigidbody ();
	if (body)
		return body->GetInstanceID();
	else
		return collider.GetInstanceID();
}

bool PhysXRaycast::Raycast (const Ray& ray, float distance, int mask, HitInfo& output)
{
	RaycastHit hit;
	bool result = GetPhysicsManager().Raycast(ray,distance,hit,mask);
    if (!result)
        return false;
    
	output.intersection = hit.point;
	output.normal = hit.normal;
	output.colliderInstanceID = hit.collider->GetInstanceID();
	output.rigidBodyOrColliderInstanceID = GetAttachedRigidbodyOrColliderInstanceID (*hit.collider);

	return true;
}


// Cast a ray against a list of shapes shapes
bool PhysXRaycast::Intersect(const Ray& ray, float maxDist, NxShape** shapes, AABB* shapeBounds, size_t shapeCount, HitInfo& hit) const
{
	Vector3f halfDir = ray.GetDirection() * maxDist * 0.5;
	AABB rayBounds(ray.GetOrigin() + halfDir, Abs(halfDir));
	rayBounds.Expand(kAABBEpsilon);
	NxRay physicsRay ((const NxVec3&)ray.GetOrigin(), (const NxVec3&)ray.GetDirection());

	NxRaycastHit nxhit; 
	NxU32 flags = NX_RAYCAST_IMPACT | NX_RAYCAST_NORMAL;

	Vector3f collisionPos;
	Vector3f collisionNormal;
	float collisionSqrDist = std::numeric_limits<float>::infinity ();

	for (size_t s = 0; s < shapeCount; ++s)
	{
		if (!IntersectAABBAABBInclusive(rayBounds, shapeBounds[s]))
			continue;

		NxShape* shape = shapes[s];
		if (!shape) continue;

#if ENABLE_MULTITHREADED_CODE
		// We shouldn't need to do this. But it turns out that sometimes raycast will return a duff intersection normal when
		// called from multiple threads intersection the same object at the same time, this causes graphical functional test
		// 267 to fail for instance. Jesper has contacted NVidia about this.
		SimpleLock::AutoLock lock(m_Lock);
#endif

		if (shape->raycast (physicsRay, maxDist, flags, nxhit, false))
		{
			Vector3f intersectionPoint = (const Vector3f&)nxhit.worldImpact;
			float sqrDistance = SqrMagnitude(ray.GetOrigin() - intersectionPoint);

			if (sqrDistance < collisionSqrDist)
			{
				Collider* collider = (Collider*)shape->userData;
				if (!collider || collider->GetIsTrigger ())
					continue;
				hit.colliderInstanceID = collider->GetInstanceID ();
				hit.rigidBodyOrColliderInstanceID = GetAttachedRigidbodyOrColliderInstanceID (*collider);
				collisionSqrDist = sqrDistance;
				hit.intersection = (const Vector3f&)nxhit.worldImpact;
				hit.normal = (const Vector3f&)nxhit.worldNormal;												
			}
		}
	}

	return collisionSqrDist < std::numeric_limits<float>::infinity ();
}

// Should probably be a param
enum { kMaxParticleCollisionShapes = 256 };

// Determines which shapes overlap the particle system and caches the bounding boxes of those shapes
size_t PhysXRaycast::BroadPhaseCulling(dynamic_array<NxShape*>& nxShapes, dynamic_array<AABB>& aabbs, const MinMaxAABB& particleSystemAABB, const int filter, bool staticOnly) const
{
	size_t shapeCount = GetShapes(particleSystemAABB, kMaxParticleCollisionShapes, nxShapes.data(), filter, staticOnly);
	if (shapeCount == 0)
		return 0;

	// Cache bounds for all shapes
	for (int i = 0; i < shapeCount; ++i)
	{
		GetAABB( aabbs[i], *nxShapes[i] );
	}
	return shapeCount;
}

size_t PhysXRaycast::BatchIntersect( const dynamic_array<BatchedRaycast>& raycasts, dynamic_array<BatchedRaycastResult>& results, const UInt32 filter, bool staticOnly ) const
{
	// Get new aabb for particle system
	MinMaxAABB aabb = ComputeBatchAABB(raycasts);
	aabb.Expand(kAABBEpsilon);
	
	// Get all shapes within bounds (aabb)
	dynamic_array<NxShape*> nxShapes(kMaxParticleCollisionShapes, kMemTempAlloc);
	dynamic_array<AABB> aabbs(kMaxParticleCollisionShapes, kMemTempAlloc);
	
	size_t shapeCount = BroadPhaseCulling( nxShapes, aabbs, aabb, filter, staticOnly );

	if (shapeCount == 0)
		return 0;
	
	// trace rays against the shapes
	size_t numIntersections = 0;
	for (size_t r = 0; r < raycasts.size(); r++)
	{
		const BatchedRaycast pointsRay = raycasts[r];

		// attempt to cull based on ray extent
		if ( !IntersectAny( pointsRay, aabbs.data(), shapeCount ) ) continue;

		// build actual ray - this is relatively expensive due to Magnitude that has a sqrt and a dot product
		// TODO: possibly do 4 rays at a time in SIMD
		const Vector3f displacement = pointsRay.to-pointsRay.from;
		float distance = Magnitude(displacement);
		if (distance <= kEpsilon)
			continue;
		const Vector3f direction = displacement / distance;
		const Ray ray(pointsRay.from, direction);

		// intersect the shapes with the ray
		HitInfo hit;
		const bool isHit = Intersect(ray, distance, nxShapes.data(), aabbs.data(), shapeCount, hit);
		if ( isHit )
		{
			results[numIntersections] = BatchedRaycastResult(r,hit);
			numIntersections++;
		}
	}
	
	return numIntersections;
}

// Perform PhysX broad phase culling
size_t PhysXRaycast::GetShapes( const AABB& bounds, int maxResult, NxShape** outShapes, UInt32 groupMask, bool staticOnly ) const
{
	Vector3f min = bounds.GetMin();
	Vector3f max = bounds.GetMax();

	NxBounds3 physicsBounds;
	physicsBounds.set((const NxVec3&)min, (const NxVec3&)max);

	return GetDynamicsScene().overlapAABBShapes (physicsBounds, ( staticOnly ? NX_STATIC_SHAPES : NX_ALL_SHAPES ), maxResult, outShapes, NULL, groupMask, NULL, false);
}

void PhysXRaycast::GetAABB( AABB& bounds, const NxShape& shape ) const
{
	NxBounds3 physicsBounds;
	shape.getWorldBounds(physicsBounds);
	NxVec3 center, extents;
	physicsBounds.getCenter(center);
	physicsBounds.getExtents(extents);
	bounds = AABB((const Vector3f&)center, (const Vector3f&)extents);
	bounds.Expand(kAABBEpsilon);
}

#endif //ENABLE_PHYSICS
