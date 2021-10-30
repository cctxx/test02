#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Interfaces/IPhysics2D.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/LinkedList.h"

class Rigidbody2D;
class Collider2D;
class b2World;
class b2Body;
class Ray;
struct Physics2DStats;

#define PHYSICS_2D_RAYCAST_DISTANCE		(1e+5f)
#define PHYSICS_2D_LARGE_RANGE_CLAMP	(1e+6f)
#define PHYSICS_2D_SMALL_RANGE_CLAMP	(0.0001f)

// --------------------------------------------------------------------------


struct Rigidbody2DInterpolationInfo : public ListElement
{
	Vector3f		position;
	Quaternionf		rotation;
	Rigidbody2D*	body;
	int				disabled;
};


// --------------------------------------------------------------------------


struct RaycastHit2D
{
	Vector2f point;
	Vector2f normal;
	float fraction;
	Collider2D* collider;
};

// --------------------------------------------------------------------------


class Physics2DManager : public IPhysics2D
{
private:
	typedef List<Rigidbody2DInterpolationInfo>	InterpolatedBodiesList;
	typedef InterpolatedBodiesList::iterator	InterpolatedBodiesIterator;

public:
	Physics2DManager();
	// ~Physics2DManager() // declared-by-macro

	// IPhysics2D interface
	virtual void FixedUpdate ();
	virtual void DynamicUpdate ();
	virtual void ResetInterpolations ();

	// 2D line-casts.
	int Linecast (const Vector2f& pointA, const Vector2f& pointB, const int layerMask, const float minDepth, const float maxDepth, RaycastHit2D* outHits, const int outHitsSize);
	int LinecastAll (const Vector2f& pointA, const Vector2f& pointB, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<RaycastHit2D>* outHits);

	// 2D ray-casts.
	int Raycast (const Vector2f& origin, const Vector2f& direction, const float distance, const int layerMask, const float minDepth, const float maxDepth, RaycastHit2D* outHits, const int outHitsSize);
	int RaycastAll (const Vector2f& origin, const Vector2f& direction, const float distance, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<RaycastHit2D>* outHits);

	// 3D ray-intersections.
	int GetRayIntersection(const Vector3f& origin, const Vector3f& direction, const float distance, const int layerMask, RaycastHit2D* outHits, const int outHitsSize);
	int GetRayIntersectionAll(const Vector3f& origin, const Vector3f& direction, const float distance, const int layerMask, dynamic_array<RaycastHit2D>* outHits);

	// 2D geometry overlaps.
	int OverlapPoint (const Vector2f& point, const int layerMask, const float minDepth, const float maxDepth, Collider2D** outHits, const int outHitsSize);
	int OverlapPointAll (const Vector2f& point, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<Collider2D*>* outHits);
	int OverlapCircle (const Vector2f& point, const float radius, const int layerMask, const float minDepth, const float maxDepth, Collider2D** outHits, const int outHitsSize);
	int OverlapCircleAll (const Vector2f& point, const float radius, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<Collider2D*>* outHits);
	int OverlapArea (const Vector2f& pointA, const Vector2f& pointB, const int layerMask, const float minDepth, const float maxDepth, Collider2D** outHits, const int outHitsSize);
	int OverlapAreaAll (const Vector2f& pointA, const Vector2f& pointB, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<Collider2D*>* outHits);

	void InvalidateColliderCollisions (Collider2D* collider);
	void DestroyColliderCollisions (Collider2D* collider);

	inline bool IsTransformMessageEnabled() const { return m_RigidbodyTransformMessageEnabled; }
	inline List<Rigidbody2DInterpolationInfo>& GetInterpolatedBodies() { return m_InterpolatedBodies; }

#if ENABLE_PROFILER
	virtual void GetProfilerStats (Physics2DStats& stats);
#endif

private:
	void SetTransformMessageEnabled (const bool enable);

private:
	std::vector<SInt32>		m_AllCollider2DTypes;
	bool					m_RigidbodyTransformMessageEnabled;
	InterpolatedBodiesList	m_InterpolatedBodies;
};


// --------------------------------------------------------------------------


void InitializePhysics2DManager ();
void CleanupPhysics2DManager ();
b2World* GetPhysics2DWorld ();
b2Body* GetPhysicsGroundBody ();
Physics2DManager& GetPhysics2DManager ();

#endif
