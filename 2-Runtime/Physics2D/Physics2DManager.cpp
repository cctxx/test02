#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS
#include "Runtime/Physics2D/Physics2DManager.h"
#include "Runtime/Physics2D/CollisionListener2D.h"
#include "Runtime/Physics2D/Physics2DSettings.h"
#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"

#include "External/Box2D/Box2D/Box2D.h"
#include "Runtime/Core/Callbacks/PlayerLoopCallbacks.h"
#include "Runtime/Geometry/Ray.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/BaseClasses/MessageHandler.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Interfaces/IPhysics2D.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Math/Simd/math.h"
#include "Runtime/Profiler/ProfilerStats.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"


PROFILER_INFORMATION(gPhysics2DDynamicUpdateProfile, "Physics2D.DynamicUpdate", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DFixedUpdateProfile, "Physics2D.FixedUpdate", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DInterpolationsProfile, "Physics2D.Interpolation", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DSimProfile, "Physics2D.Simulate", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DUpdateTransformsProfile, "Physics2D.UpdateTransforms", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DCallbacksProfile, "Physics2D.Callbacks", kProfilerPhysics)
PROFILER_INFORMATION(gLinecast2DProfile, "Physics2D.Linecast", kProfilerPhysics)
PROFILER_INFORMATION(gLinecastAll2DProfile, "Physics2D.LinecastAll", kProfilerPhysics)
PROFILER_INFORMATION(gRaycast2DProfile, "Physics2D.Raycast", kProfilerPhysics)
PROFILER_INFORMATION(gRaycastAll2DProfile, "Physics2D.RaycastAll", kProfilerPhysics)
PROFILER_INFORMATION(gOverlapPoint2DProfile, "Physics2D.OverlapPoint", kProfilerPhysics)
PROFILER_INFORMATION(gOverlapPointAll2DProfile, "Physics2D.OverlapPointAll", kProfilerPhysics)
PROFILER_INFORMATION(gOverlapCircle2DProfile, "Physics2D.OverlapCircle", kProfilerPhysics)
PROFILER_INFORMATION(gOverlapCircleAll2DProfile, "Physics2D.OverlapCircleAll", kProfilerPhysics)
PROFILER_INFORMATION(gOverlapArea2DProfile, "Physics2D.OverlapArea", kProfilerPhysics)
PROFILER_INFORMATION(gOverlapAreaAll2DProfile, "Physics2D.OverlapAreaAll", kProfilerPhysics)
PROFILER_INFORMATION(gGetRayIntersection2DProfile, "Physics2D.GetRayIntersection", kProfilerPhysics)
PROFILER_INFORMATION(gGetRayIntersectionAll2DProfile, "Physics2D.GetRayIntersectionAll", kProfilerPhysics)


// --------------------------------------------------------------------------


inline bool CheckFixtureLayer(b2Fixture* fixture, int layerMask)
{
	DebugAssert(fixture);

	Collider2D* col = reinterpret_cast<Collider2D*>(fixture->GetUserData());
	if (!col)
		return false; // no collider

	GameObject* go = col->GetGameObjectPtr();
	if (!go)
		return false; // no GO

	int goMask = go->GetLayerMask();
	if ((goMask & layerMask) == 0)
		return false; // ignoring this layer

	return true; // all passed
}

// --------------------------------------------------------------------------


inline bool CheckColliderDepth(Collider2D* collider, const float minDepth, const float maxDepth)
{
	DebugAssert(collider);

	// Fetch depth of the collider.
	const float depth = collider->GetComponent(Transform).GetPosition ().z;

	// Check if in the depth range.
	return !(depth < minDepth || depth > maxDepth);
}


// --------------------------------------------------------------------------


inline void NormalizeDepthRange(float& minDepth, float& maxDepth)
{
	// Clamp any range bounds specified as +- infinity to real values.
	minDepth = (minDepth == -std::numeric_limits<float>::infinity ()) ? -std::numeric_limits<float>::max () : minDepth;
	maxDepth = (maxDepth == std::numeric_limits<float>::infinity ()) ? std::numeric_limits<float>::max () : maxDepth;
	
	if (minDepth < maxDepth)
		return;

	std::swap (minDepth, maxDepth);
}


// --------------------------------------------------------------------------


struct ContactFilter2D : public b2ContactFilter
{
	virtual bool ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB)
	{
		Collider2D* colliderA = reinterpret_cast<Collider2D*>(fixtureA->GetUserData ());
		Collider2D* colliderB = reinterpret_cast<Collider2D*>(fixtureB->GetUserData ());

		if (!colliderA->GetEnabled () || !colliderB->GetEnabled ())
			return false;

		// Fetch collider layers.
		const int layerA = colliderA->GetGameObject ().GetLayer();
		const int layerB = colliderB->GetGameObject ().GetLayer();

		// Decide on the contact based upon the layer collision mask.
		return GetPhysics2DSettings().GetLayerCollisionMask(layerA) & (1<<layerB);
	}
};


// --------------------------------------------------------------------------


struct ColliderHitsByDepthComparitor
{
	inline bool operator()(const Collider2D* a, const Collider2D* b)
	{
		return a->GetComponent(Transform).GetPosition ().z < b->GetComponent(Transform).GetPosition ().z;
	}

	inline int CompareDepth(const Collider2D* a, const Collider2D* b)
	{
		const float depthA = a->GetComponent(Transform).GetPosition ().z;
		const float depthB = b->GetComponent(Transform).GetPosition ().z;

		if (depthA < depthB) return -1;
		if (depthA > depthB) return 1;
		return 0;
	}
} m_CompareDepth;


// --------------------------------------------------------------------------


struct RayHitsByDepthComparitor
{
	inline bool operator()(const RaycastHit2D& a, const RaycastHit2D& b)
	{
		return a.collider->GetComponent(Transform).GetPosition ().z < b.collider->GetComponent(Transform).GetPosition ().z;
	}
};

struct RayHitsByInverseDepthComparitor
{
	inline bool operator()(const RaycastHit2D& a, const RaycastHit2D& b)
	{
		return a.collider->GetComponent(Transform).GetPosition ().z > b.collider->GetComponent(Transform).GetPosition ().z;
	}
};


// --------------------------------------------------------------------------


class Raycast2DQuery : public b2RayCastCallback
{
public:
	Raycast2DQuery(const Vector2f& pointA, const Vector2f& pointB, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<RaycastHit2D>& outHits)
		: m_PointA(pointA)
		, m_PointB(pointB)
		, m_LayerMask(layerMask)
		, m_MinDepth(minDepth)
		, m_MaxDepth(maxDepth)
		, m_Hits(outHits)
	{
		NormalizeDepthRange(m_MinDepth, m_MaxDepth);
	}

	int RunQuery ()
	{
		// Calculate if the ray has zero-length or not.
		const bool rayHasMagnitude = SqrMagnitude (m_PointB - m_PointA) > b2_epsilon * b2_epsilon;

		// Perform a discrete check at the start-point (Box2D does not discover these for a ray-cast).
		dynamic_array<Collider2D*> colliderHits(kMemTempAlloc);
		if (GetPhysics2DManager ().OverlapPointAll (m_PointA, m_LayerMask, m_MinDepth, m_MaxDepth, &colliderHits) > 0)
		{
			const Vector2f collisionNormal = rayHasMagnitude ? NormalizeFast (m_PointA-m_PointB) : Vector2f::zero;
			for (dynamic_array<Collider2D*>::iterator colliderItr = colliderHits.begin (); colliderItr != colliderHits.end (); ++colliderItr)
			{
				RaycastHit2D rayHit;
				rayHit.collider = *colliderItr;
				rayHit.point = m_PointA;
				rayHit.normal = collisionNormal;
				rayHit.fraction = 0.0f;
				m_Hits.push_back (rayHit);
			}
		}

		if (rayHasMagnitude)
		{
			// Perform the ray-cast.
			GetPhysics2DWorld ()->RayCast (this, b2Vec2(m_PointA.x, m_PointA.y), b2Vec2(m_PointB.x, m_PointB.y));

			// Sort the hits by fraction.
			std::sort (m_Hits.begin(), m_Hits.end(), RaycastHitsByFractionComparitor());
		}

		return m_Hits.size ();
	}

	virtual float32 ReportFixture (b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float32 fraction)
	{
		// Handle whether ray-casts are hitting triggers or not.
		if (fixture->IsSensor () && !GetPhysics2DSettings ().GetRaycastsHitTriggers ())
			return -1.0f;

		// Ignore if not in the selected fixture layer.
		if (!CheckFixtureLayer (fixture, m_LayerMask))
			return -1.0f; // ignore and continue

		// Ignore if not in the selected depth-range.
		Collider2D* collider = reinterpret_cast<Collider2D*>(fixture->GetUserData());
		if (!CheckColliderDepth (collider, m_MinDepth, m_MaxDepth))
			return -1.0f; // ignore and continue;

		RaycastHit2D hit;
		hit.point.Set (point.x, point.y);
		hit.normal.Set (normal.x, normal.y);
		hit.fraction = fraction;
		hit.collider = collider;

		// For chain colliders, Box2D will report all individual segments that are hit; also similar situation
		// when using composite convex colliders. Try searching for a hit with same Collider2D, and replace info
		// if closer hit.  Unfortunately, this is is O(n).
		for (size_t i = 0, n = m_Hits.size(); i != n; ++i)
		{
			if (m_Hits[i].collider == collider)
			{
				if (fraction < m_Hits[i].fraction)
					m_Hits[i] = hit;

				return 1.0f; // continue
			}
		}

		// Add new hit
		m_Hits.push_back (hit);

		return 1.0f; // continue
	}

private:
	struct RaycastHitsByFractionComparitor
	{
		inline bool operator()(const RaycastHit2D& a, const RaycastHit2D& b)
		{
			return a.fraction < b.fraction;
		}
	};

private:
	int m_LayerMask;
	float m_MinDepth;
	float m_MaxDepth;
	Vector2f m_PointA;
	Vector2f m_PointB;
	dynamic_array<RaycastHit2D>& m_Hits;
};


// --------------------------------------------------------------------------


class OverlapPointQuery2D : public b2QueryCallback
{
public:
	OverlapPointQuery2D(const Vector2f& point, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<Collider2D*>& outHits)
		: m_LayerMask(layerMask)
		, m_MinDepth(minDepth)
		, m_MaxDepth(maxDepth)
		, m_Hits(outHits)
	{
		NormalizeDepthRange(m_MinDepth, m_MaxDepth);

		m_Point.Set (point.x, point.y);
	}

	int RunQuery ()
	{
		// Reset results.
		m_Hits.clear();

		b2AABB aabb;
		aabb.lowerBound = aabb.upperBound = m_Point;
		GetPhysics2DWorld ()->QueryAABB (this, aabb);

		// Sort the hits by depth.
		std::sort (m_Hits.begin(), m_Hits.end(), ColliderHitsByDepthComparitor());

		return m_Hits.size ();
	}

	virtual bool ReportFixture (b2Fixture* fixture)
	{
		// Handle whether ray-casts are hitting triggers or not.
		if (fixture->IsSensor () && !GetPhysics2DSettings ().GetRaycastsHitTriggers ())
			return true;

		// Ignore if not in the selected fixture layer.
		if (!CheckFixtureLayer (fixture, m_LayerMask))
			return true; // ignore and continue

		// Ignore if not in the selected depth-range.
		Collider2D* collider = reinterpret_cast<Collider2D*>(fixture->GetUserData());
		if (!CheckColliderDepth (collider, m_MinDepth, m_MaxDepth))
			return true; // ignore and continue;

		// Ignore if we've already selected this collider and it has a higher depth.
		for (size_t i = 0; i != m_Hits.size (); ++i)
		{
			if (m_Hits[i] == collider)
			{
				if (m_CompareDepth.CompareDepth (m_Hits[i], collider) == 1)
					m_Hits[i] = collider;
				return true;
			}
		}

		// Test point against fixture.
		if (!fixture->TestPoint (m_Point))
			return true;

		// Add new hit
		m_Hits.push_back (collider);

		return true;
	}

private:
	b2Vec2 m_Point;
	int m_LayerMask;
	float m_MinDepth;
	float m_MaxDepth;
	dynamic_array<Collider2D*>& m_Hits;
};


// --------------------------------------------------------------------------


class OverlapCircleQuery2D : public b2QueryCallback
{
public:
	OverlapCircleQuery2D(const Vector2f& point, const float radius, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<Collider2D*>& outHits)
		: m_LayerMask(layerMask)
		, m_MinDepth(minDepth)
		, m_MaxDepth(maxDepth)
		, m_Hits(outHits)
	{
		NormalizeDepthRange(m_MinDepth, m_MaxDepth);

		// Extremely small radii indicates a point query.
		if (radius < 0.00001f)
		{
			m_Point.Set (point.x, point.y);
			m_AABB.lowerBound = m_AABB.upperBound = m_Point;
			m_PointQuery = true;
		}
		else
		{
			// Calculate circle shape and its AABB.
			m_CircleShape.m_p.Set (point.x, point.y);
			m_CircleShape.m_radius = radius;
			m_QueryTransform.SetIdentity ();
			m_CircleShape.ComputeAABB( &m_AABB, m_QueryTransform, 0 );
			m_PointQuery = false;
		}
	}


	int RunQuery ()
	{
		// Reset results.
		m_Hits.clear();

		GetPhysics2DWorld ()->QueryAABB (this, m_AABB);

		// Sort the hits by depth.
		std::sort (m_Hits.begin(), m_Hits.end(), ColliderHitsByDepthComparitor());

		return m_Hits.size ();
	}

	virtual bool ReportFixture (b2Fixture* fixture)
	{
		// Handle whether ray-casts are hitting triggers or not.
		if (fixture->IsSensor () && !GetPhysics2DSettings ().GetRaycastsHitTriggers ())
			return true;

		// Ignore if not in the selected fixture layer.
		if (!CheckFixtureLayer (fixture, m_LayerMask))
			return true; // ignore and continue

		// Ignore if not in the selected depth-range.
		Collider2D* collider = reinterpret_cast<Collider2D*>(fixture->GetUserData());
		if (!CheckColliderDepth (collider, m_MinDepth, m_MaxDepth))
			return true; // ignore and continue;

		// Ignore if we've already selected this collider and it has a higher depth.
		for (size_t i = 0; i != m_Hits.size (); ++i)
		{
			if (m_Hits[i] == collider)
			{
				if (m_CompareDepth.CompareDepth (m_Hits[i], collider) == 1)
					m_Hits[i] = collider;
				return true;
			}
		}

		if (m_PointQuery)
		{
			// Test point against fixture.
			if (!fixture->TestPoint (m_Point))
				return true;
		}
		else
		{
			// Test circle against fixture.
			if ( !b2TestOverlap( &m_CircleShape, 0, fixture->GetShape (), 0, m_QueryTransform, fixture->GetBody ()->GetTransform () ) )
				return true;
		}

		// Add new hit
		m_Hits.push_back (collider);

		return true;
	}

private:
	int m_LayerMask;
	float m_MinDepth;
	float m_MaxDepth;
	b2Vec2 m_Point;
	b2CircleShape m_CircleShape;
	b2AABB m_AABB;
	b2Transform m_QueryTransform;
	dynamic_array<Collider2D*>& m_Hits;
	bool m_PointQuery;
};


// --------------------------------------------------------------------------


class OverlapAreaQuery2D : public b2QueryCallback
{
public:
	OverlapAreaQuery2D(Vector2f pointA, Vector2f pointB, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<Collider2D*>& outHits)
		: m_LayerMask(layerMask)
		, m_MinDepth(minDepth)
		, m_MaxDepth(maxDepth)
		, m_Hits(outHits)
	{
		NormalizeDepthRange(m_MinDepth, m_MaxDepth);

		// Normalize the points.
		if (pointA.x > pointB.x)
			std::swap (pointA.x, pointB.x);
		if (pointA.y > pointB.y)
			std::swap (pointA.y, pointB.y);

		// Calculate the AABB.
		m_PolygonAABB.lowerBound.Set (pointA.x, pointA.y);
		m_PolygonAABB.upperBound.Set (pointB.x, pointB.y);

		// Calculate polygon shape.
		b2Vec2 verts[4];
		verts[0].Set( pointA.x, pointA.y );
		verts[1].Set( pointB.x, pointA.y );
		verts[2].Set( pointB.x, pointB.y );
		verts[3].Set( pointA.x, pointB.y );
		m_PolygonShape.Set( verts, 4 );

		m_QueryTransform.SetIdentity ();
	}

	int RunQuery ()
	{
		// Reset results.
		m_Hits.clear();

		// Finish if AABB is invalid.
		if (!m_PolygonAABB.IsValid())
			return 0;

		GetPhysics2DWorld ()->QueryAABB (this, m_PolygonAABB);

		// Sort the hits by depth.
		std::sort (m_Hits.begin(), m_Hits.end(), ColliderHitsByDepthComparitor());

		return m_Hits.size ();
	}

	virtual bool ReportFixture (b2Fixture* fixture)
	{
		// Handle whether ray-casts are hitting triggers or not.
		if (fixture->IsSensor () && !GetPhysics2DSettings ().GetRaycastsHitTriggers ())
			return true;

		// Ignore if not in the selected fixture layer.
		if (!CheckFixtureLayer (fixture, m_LayerMask))
			return true; // ignore and continue

		// Ignore if not in the selected depth-range.
		Collider2D* collider = reinterpret_cast<Collider2D*>(fixture->GetUserData());
		if (!CheckColliderDepth (collider, m_MinDepth, m_MaxDepth))
			return true; // ignore and continue;

		// Ignore if we've already selected this collider and it has a higher depth.
		for (size_t i = 0; i != m_Hits.size (); ++i)
		{
			if (m_Hits[i] == collider)
			{
				if (m_CompareDepth.CompareDepth (m_Hits[i], collider) == 1)
					m_Hits[i] = collider;
				return true;
			}
		}

		// Test polygon against fixture.
		if ( !b2TestOverlap( &m_PolygonShape, 0, fixture->GetShape (), 0, m_QueryTransform, fixture->GetBody ()->GetTransform () ) )
			return true;

		// Add new hit
		m_Hits.push_back (collider);

		return true;
	}

private:
	int m_LayerMask;
	float m_MinDepth;
	float m_MaxDepth;
	b2PolygonShape m_PolygonShape;
	b2AABB m_PolygonAABB;
	b2Transform m_QueryTransform;
	dynamic_array<Collider2D*>& m_Hits;
};


// --------------------------------------------------------------------------


struct Physics2DState
{
	Physics2DState() :
		m_PhysicsManager(NULL),
		m_PhysicsWorld(NULL),
		m_PhysicsGroundBody(NULL) { }

	void Initialize();
	void Cleanup();
		
	Physics2DManager*	m_PhysicsManager;
	b2World*			m_PhysicsWorld;
	b2Body*				m_PhysicsGroundBody;
	CollisionListener2D	m_Collisions;
	ContactFilter2D		m_ContactFilter;
};

static Physics2DState g_Physics2DState;


// --------------------------------------------------------------------------

void Physics2DState::Initialize()
{
	Assert (m_PhysicsManager == NULL);
	Assert (m_PhysicsWorld == NULL);

	m_PhysicsManager = new Physics2DManager ();
	SetIPhysics2D (m_PhysicsManager);

	// Initialize the Box2D physics world.
	b2Vec2 gravity(0.0f, -9.81f);
	m_PhysicsWorld = new b2World (gravity);
	m_PhysicsWorld->SetContactListener (&m_Collisions);
	m_PhysicsWorld->SetContactFilter (&m_ContactFilter);

	// Initialize the static ground-body.
	b2BodyDef groundBodyDef;
	m_PhysicsGroundBody = GetPhysics2DWorld()->CreateBody (&groundBodyDef);

	// Register physics updates.
	REGISTER_PLAYERLOOP_CALL (Physics2DFixedUpdate, GetPhysics2DManager().FixedUpdate());
	REGISTER_PLAYERLOOP_CALL (Physics2DUpdate, GetPhysics2DManager().DynamicUpdate());
	REGISTER_PLAYERLOOP_CALL (Physics2DResetInterpolatedTransformPosition, GetPhysics2DManager().ResetInterpolations());	
}


void Physics2DState::Cleanup()
{
	delete m_PhysicsWorld;
	m_PhysicsWorld = NULL;

	delete m_PhysicsManager;
	m_PhysicsManager = NULL;
	SetIPhysics2D (NULL);
}


void InitializePhysics2DManager ()
{
	g_Physics2DState.Initialize ();
}


void CleanupPhysics2DManager ()
{
	g_Physics2DState.Cleanup ();
}


b2World* GetPhysics2DWorld ()
{
	Assert (g_Physics2DState.m_PhysicsWorld);
	return g_Physics2DState.m_PhysicsWorld;
}


b2Body* GetPhysicsGroundBody ()
{
	Assert (g_Physics2DState.m_PhysicsGroundBody);
	return g_Physics2DState.m_PhysicsGroundBody;
}


Physics2DManager& GetPhysics2DManager()
{
	Assert (g_Physics2DState.m_PhysicsManager);
	return *g_Physics2DState.m_PhysicsManager;
}


// --------------------------------------------------------------------------


Physics2DManager::Physics2DManager()
	: m_RigidbodyTransformMessageEnabled(true)
{
	Object::FindAllDerivedClasses (ClassID (Collider2D), &m_AllCollider2DTypes);
}


void Physics2DManager::FixedUpdate()
{
	PROFILER_AUTO(gPhysics2DFixedUpdateProfile, NULL)

		// Gather interpolation info.
	{
		PROFILER_AUTO(gPhysics2DInterpolationsProfile, NULL)

		// Store interpolated position
		for (InterpolatedBodiesIterator i=m_InterpolatedBodies.begin();i!=m_InterpolatedBodies.end();++i)
		{
			Rigidbody2D* rigidBody = i->body;
			i->disabled = 0;

			if ( rigidBody->GetBody() == NULL )
				continue;

			if (rigidBody->GetInterpolation() == kInterpolate2D)
			{
				i->position = rigidBody->GetBodyPosition();
				i->rotation = rigidBody->GetBodyRotation();
			}
		}
	}

	// simulate
	{
		PROFILER_AUTO(gPhysics2DSimProfile, NULL)

		const Physics2DSettings& settings = GetPhysics2DSettings();
		g_Physics2DState.m_PhysicsWorld->Step (GetTimeManager().GetFixedDeltaTime(), settings.GetVelocityIterations(), settings.GetPositionIterations());
	}

	// update data back from simulation
	{
		PROFILER_AUTO(gPhysics2DUpdateTransformsProfile, NULL)

		// Disable rigid body, collider & joint transform changed message handler.
		// Turns off setting the pose while we are fetching the state.
		SetTransformMessageEnabled (false);

		// Update position / rotation of all rigid bodies
		b2Body* body = GetPhysics2DWorld()->GetBodyList();
		while (body != NULL)
		{
			Rigidbody2D* rb = (Rigidbody2D*)body->GetUserData();
			if (rb != NULL && body->GetType() != b2_staticBody && body->IsAwake())
			{
				GameObject& go = rb->GetGameObject();
				Transform& transform = go.GetComponent (Transform);

				// Calculate new position.
				const b2Vec2& pos2 = body->GetPosition();
				Vector3f pos3 = transform.GetPosition();
				pos3.x = pos2.x;
				pos3.y = pos2.y;

				// Calculate new rotation.
				Vector3f localEuler = QuaternionToEuler (transform.GetLocalRotation ());
				localEuler.z = body->GetAngle ();

				// Update position and rotation.
				transform.SetPositionAndRotation (pos3, EulerToQuaternion(localEuler));
			}

			body = body->GetNext();
		}

		// Enable transform change notifications back.
		SetTransformMessageEnabled (true);
	}

	// do script callbacks
	{
		PROFILER_AUTO(gPhysics2DCallbacksProfile, NULL)

		const bool oldDisableDestruction = GetDisableImmediateDestruction();
		SetDisableImmediateDestruction(true);

		// report collisions and triggers
		g_Physics2DState.m_Collisions.ReportCollisions();

		SetDisableImmediateDestruction(oldDisableDestruction);
	}
}


void Physics2DManager::DynamicUpdate()
{
	PROFILER_AUTO(gPhysics2DDynamicUpdateProfile, NULL);

	{
		PROFILER_AUTO(gPhysics2DInterpolationsProfile, NULL)

		// Disable rigid body / collider transform changed message handler.
		// Turns off setting the pose while we are fetching the state.
		SetTransformMessageEnabled (false);

		// Also disable rigidbody (2D) transform changed message, otherwise interpolation will affect physics results.
		// This is not done in SetTransformMessageEnabled, as we need the rigidbody (2D) messages in the physics fixed update
		// so kinematic child rigid-bodies (2D) are moved with their parents.
		GameObject::GetMessageHandler ().SetMessageEnabled (ClassID(Rigidbody2D), kTransformChanged.messageID, false);

		// Interpolation time is [0...1] between the two steps
		// Extrapolation time the delta time since the last fixed step
		const float dynamicTime = GetTimeManager().GetCurTime();
		const float step = GetTimeManager().GetFixedDeltaTime();
		const float fixedTime = GetTimeManager().GetFixedTime();

		// Update interpolated position
		for (InterpolatedBodiesIterator i=m_InterpolatedBodies.begin ();i!=m_InterpolatedBodies.end ();++i)
		{
			Rigidbody2D* rigidBody = i->body;
			const RigidbodyInterpolation2D interpolation = rigidBody->GetInterpolation ();

			// Ignore if disabled, no interpolation is specified or the body is sleeping.
			if (i->disabled || interpolation == kNoInterpolation2D || rigidBody->IsSleeping ())
				continue;

			// Interpolate between this physics and last physics frame.
			if (interpolation == kInterpolate2D)
			{
				const float interpolationTime = clamp01 ((dynamicTime - fixedTime) / step);

				// Interpolate position.
				const Vector3f position = Lerp (i->position, rigidBody->GetBodyPosition(), interpolationTime);

				// Interpolate rotation.
				const Quaternionf rotation = Slerp (i->rotation, rigidBody->GetBodyRotation (), interpolationTime);

				// Update position/rotation.
				rigidBody->GetComponent(Transform).SetPositionAndRotationSafe (position, rotation);
				continue;
			}

			// Extrapolate current position using velocity.
			else if (interpolation == kExtrapolate2D)
			{
				const float extrapolationTime = dynamicTime - fixedTime;

				// Interpolate position.
				const Vector2f linearVelocity2D = rigidBody->GetVelocity();
				const Vector3f linearVelocity3D(linearVelocity2D.x * extrapolationTime, linearVelocity2D.y * extrapolationTime, 0.0f);
				const Vector3f position = rigidBody->GetBodyPosition () + linearVelocity3D;

				// Interpolate rotation.
				const float angularVelocity2D = rigidBody->GetAngularVelocity ();				
				const Quaternionf rotation = CompareApproximately (angularVelocity2D, 0.0f) ? AngularVelocityToQuaternion(Vector3f(0.0f, 0.0f, angularVelocity2D), extrapolationTime) * rigidBody->GetBodyRotation() : rigidBody->GetBodyRotation();

				// Update position/rotation.
				rigidBody->GetComponent(Transform).SetPositionAndRotationSafe (position, rotation);
				continue;
			}
		}

		// Re-enable rigidbody (2D) transform changed messages.
		GameObject::GetMessageHandler ().SetMessageEnabled (ClassID(Rigidbody2D), kTransformChanged.messageID, true);

		// Enable transform change notifications back.
		SetTransformMessageEnabled (true);
	}
}


void Physics2DManager::ResetInterpolations ()
{
	PROFILER_AUTO(gPhysics2DInterpolationsProfile, NULL)

	for (InterpolatedBodiesIterator i=m_InterpolatedBodies.begin ();i!=m_InterpolatedBodies.end ();++i)
	{
		Rigidbody2D* rigidBody = i->body;
		if (rigidBody->GetBody() == NULL || rigidBody->IsSleeping ())
			continue;

		Transform& transform = rigidBody->GetComponent(Transform);

		Vector3f pos = rigidBody->GetBodyPosition();
		Quaternionf rot = rigidBody->GetBodyRotation();
		transform.SetPositionAndRotationSafeWithoutNotification (pos, rot);
	}
}


int Physics2DManager::Linecast (const Vector2f& pointA, const Vector2f& pointB, const int layerMask, const float minDepth, const float maxDepth, RaycastHit2D* outHits, const int outHitsSize)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gLinecast2DProfile, NULL)

	// Finish if no available hits capacity.
	if (outHitsSize == 0)
		return 0;

	dynamic_array<RaycastHit2D> raycastHits(kMemTempAlloc);
	Raycast2DQuery query (pointA, pointB, layerMask, minDepth, maxDepth, raycastHits);	
	const int resultCount = query.RunQuery ();

	// Transfer the first n-results.
	const int allowedResultCount = std::min (resultCount, outHitsSize);
	for (int index = 0; index < allowedResultCount; ++index)
		*(outHits++) = raycastHits[index];

	return allowedResultCount;
}


int Physics2DManager::LinecastAll (const Vector2f& pointA, const Vector2f& pointB, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<RaycastHit2D>* outHits)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gLinecastAll2DProfile, NULL)

	Raycast2DQuery query (pointA, pointB, layerMask, minDepth, maxDepth, *outHits);
	return query.RunQuery ();
}


int Physics2DManager::Raycast (const Vector2f& origin, const Vector2f& direction, const float distance, const int layerMask, const float minDepth, const float maxDepth, RaycastHit2D* outHits, const int outHitsSize)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gRaycast2DProfile, NULL)

	// Finish if no available hits capacity.
	if (outHitsSize == 0)
		return 0;

	// Calculate destination point.
	const bool isInfiniteDistance = distance == std::numeric_limits<float>::infinity();
	Vector2f normalizedDirection = NormalizeFast (direction);
	const Vector2f pointB = origin + (normalizedDirection * (isInfiniteDistance ? PHYSICS_2D_RAYCAST_DISTANCE : distance));

	dynamic_array<RaycastHit2D> raycastHits(kMemTempAlloc);
	Raycast2DQuery query (origin, pointB, layerMask, minDepth, maxDepth, raycastHits);
	const int resultCount = query.RunQuery ();

	// Transfer the first n-results.
	const int allowedResultCount = std::min (resultCount, outHitsSize);
	for (int index = 0; index < allowedResultCount; ++index)
	{
		RaycastHit2D& hit = raycastHits[index];
		if (isInfiniteDistance)
			hit.fraction *= PHYSICS_2D_RAYCAST_DISTANCE;

		*(outHits++) = hit;
	}

	return allowedResultCount;
}


int Physics2DManager::RaycastAll (const Vector2f& origin, const Vector2f& direction, const float distance, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<RaycastHit2D>* outHits)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gRaycastAll2DProfile, NULL)

	// Calculate points.
	const bool isInfiniteDistance = distance == std::numeric_limits<float>::infinity();
	Vector2f normalizedDirection = NormalizeFast (direction);
	const Vector2f pointB = origin + (normalizedDirection * (isInfiniteDistance ? PHYSICS_2D_RAYCAST_DISTANCE : distance));

	Raycast2DQuery query (origin, pointB, layerMask, minDepth, maxDepth, *outHits);
	const int resultCount = query.RunQuery ();

	// Finish if not infinite distance.
	if (!isInfiniteDistance || resultCount == 0)
		return resultCount;

	// Change fraction to distance.
	for (dynamic_array<RaycastHit2D>::iterator hitItr = outHits->begin(); hitItr != outHits->end(); ++hitItr)
		hitItr->fraction *= PHYSICS_2D_RAYCAST_DISTANCE;

	return resultCount;
}


int Physics2DManager::GetRayIntersection(const Vector3f& origin, const Vector3f& direction, const float distance, const int layerMask, RaycastHit2D* outHits, const int outHitsSize)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gGetRayIntersection2DProfile, NULL)

	// Finish if no available hits capacity.
	if (outHitsSize == 0)
		return 0;

	dynamic_array<RaycastHit2D> raycastHits(kMemTempAlloc);
	const int resultCount = GetRayIntersectionAll (origin, direction, distance, layerMask, &raycastHits);

	// Transfer the first n-results.
	const int allowedResultCount = std::min (resultCount, outHitsSize);
	for (int index = 0; index < allowedResultCount; ++index)
		*(outHits++) = raycastHits[index];

	return allowedResultCount;
}


int Physics2DManager::GetRayIntersectionAll(const Vector3f& origin, const Vector3f& direction, const float distance, const int layerMask, dynamic_array<RaycastHit2D>* outHits)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gGetRayIntersectionAll2DProfile, NULL)

	// Clear hits.
	outHits->clear ();

	// Set ray.
	Ray ray;
	ray.SetOrigin (origin);
	ray.SetApproxDirection (direction);

	// Calculate destination point.
	const bool isInfiniteDistance = distance == std::numeric_limits<float>::infinity();
	const float rayDistanceScale = isInfiniteDistance ? 1.0f : 1.0f / distance;
	const Vector3f destination = ray.GetPoint (isInfiniteDistance ? PHYSICS_2D_RAYCAST_DISTANCE : distance);

	// If the ray is parallel to the X/Y plane then no intersections are possible.
	if (CompareApproximately (origin.z, destination.z))
		return 0;

	// Calculate 2D start/end points.
	const Vector2f pointA = Vector2f(origin.x, origin.y);
	const Vector2f pointB = Vector2f(destination.x, destination.y);

	// Find all the colliders that hit somewhere along the ray.
	// NOTE:- These will all be unique colliders (not duplicates).
	dynamic_array<RaycastHit2D> potentialHits(kMemTempAlloc);
	if (LinecastAll (pointA, pointB, layerMask, origin.z, destination.z, &potentialHits) == 0)
		return 0;

	// Sort the hits by depth.
	const bool isForwardDepth = origin.z < direction.z;
	if (isForwardDepth)
		std::sort (potentialHits.begin(), potentialHits.end(), RayHitsByDepthComparitor());
	else
		std::sort (potentialHits.begin(), potentialHits.end(), RayHitsByInverseDepthComparitor());

	// Calculate the plane normal.
	const Vector3f planeNormal(0.0f, 0.0f, isForwardDepth ? 1.0f : -1.0f);

	// Iterate all hits and check collider intersections.
	for (dynamic_array<RaycastHit2D>::iterator hitItr = potentialHits.begin (); hitItr != potentialHits.end (); ++hitItr)
	{
		// Fetch the hit.
		RaycastHit2D& hit = *hitItr;

		// Fetch the collider depth.
		const Collider2D* collider = hit.collider;
		const float depth = collider->GetGameObject ().GetComponent (Transform).GetPosition ().z;

		// Configure a collider plane.
		Plane colliderPlane;
		colliderPlane.SetNormalAndPosition (planeNormal, Vector3f(0.0f, 0.0f, depth));

		// Test ray for intersection position.
		float intersectionDistance;
		if (!IntersectRayPlane (ray, colliderPlane, &intersectionDistance))
			continue;

		// Test if the intersection point overlaps the collider.
		const Vector3f intersectionPoint3 = ray.GetPoint (intersectionDistance);
		const Vector2f intersectionPoint2 = Vector2f(intersectionPoint3.x, intersectionPoint3.y);
		if (!collider->OverlapPoint (intersectionPoint2))
			continue;

		// Update the hit with the 3D intersection details.
		// NOTE: We leave the normal in 2D space as we can't use the plane normal.
		hit.point = intersectionPoint2;
		hit.fraction = intersectionDistance * rayDistanceScale;

		// Add hit result.
		outHits->push_back (hit);
	}

	return outHits->size ();
}


int Physics2DManager::OverlapPoint (const Vector2f& point, const int layerMask, const float minDepth, const float maxDepth, Collider2D** outHits, const int outHitsSize)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gOverlapPoint2DProfile, NULL)

	dynamic_array<Collider2D*> colliderHits(kMemTempAlloc);
	OverlapPointQuery2D query (point, layerMask, minDepth, maxDepth, colliderHits);
	const int resultCount = query.RunQuery ();

	// Transfer the first n-results.
	const int allowedResultCount = std::min (resultCount, outHitsSize);
	for (int index = 0; index < allowedResultCount; ++index)
		*(outHits++) = colliderHits[index];

	return allowedResultCount;
}


int Physics2DManager::OverlapPointAll (const Vector2f& point, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<Collider2D*>* outHits)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gOverlapPointAll2DProfile, NULL)

	OverlapPointQuery2D query (point, layerMask, minDepth, maxDepth, *outHits);
	return query.RunQuery ();
}


int Physics2DManager::OverlapCircle (const Vector2f& point, const float radius, const int layerMask, const float minDepth, const float maxDepth, Collider2D** outHits, const int outHitsSize)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gOverlapCircle2DProfile, NULL)

	dynamic_array<Collider2D*> colliderHits(kMemTempAlloc);
	OverlapCircleQuery2D query (point, radius, layerMask, minDepth, maxDepth, colliderHits);
	const int resultCount = query.RunQuery ();

	// Transfer the first n-results.
	const int allowedResultCount = std::min (resultCount, outHitsSize);
	for (int index = 0; index < allowedResultCount; ++index)
		*(outHits++) = colliderHits[index];

	return allowedResultCount;
}


int Physics2DManager::OverlapCircleAll (const Vector2f& point, const float radius, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<Collider2D*>* outHits)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gOverlapCircleAll2DProfile, NULL)

	OverlapCircleQuery2D query (point, radius, layerMask, minDepth, maxDepth, *outHits);
	return query.RunQuery ();
}


int Physics2DManager::OverlapArea (const Vector2f& pointA, const Vector2f& pointB, const int layerMask, const float minDepth, const float maxDepth, Collider2D** outHits, const int outHitsSize)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gOverlapArea2DProfile, NULL)

	dynamic_array<Collider2D*> colliderHits(kMemTempAlloc);
	OverlapAreaQuery2D query (pointA, pointB, layerMask, minDepth, maxDepth, colliderHits);
	const int resultCount = query.RunQuery ();

	// Transfer the first n-results.
	const int allowedResultCount = std::min (resultCount, outHitsSize);
	for (int index = 0; index < allowedResultCount; ++index)
		*(outHits++) = colliderHits[index];

	return allowedResultCount;
}


int Physics2DManager::OverlapAreaAll (const Vector2f& pointA, const Vector2f& pointB, const int layerMask, const float minDepth, const float maxDepth, dynamic_array<Collider2D*>* outHits)
{
	Assert(g_Physics2DState.m_PhysicsWorld);
	Assert(outHits);
	PROFILER_AUTO(gOverlapAreaAll2DProfile, NULL)

	OverlapAreaQuery2D query (pointA, pointB, layerMask, minDepth, maxDepth, *outHits);
	return query.RunQuery ();	
}

void Physics2DManager::InvalidateColliderCollisions (Collider2D* collider)
{
	g_Physics2DState.m_Collisions.InvalidateColliderCollisions (collider);
}


void Physics2DManager::DestroyColliderCollisions (Collider2D* collider)
{
	g_Physics2DState.m_Collisions.DestroyColliderCollisions (collider);
}


#if ENABLE_PROFILER
void Physics2DManager::GetProfilerStats (Physics2DStats& stats)
{
	// Fetch the physics world.
	b2World* world = g_Physics2DState.m_PhysicsWorld;

	// Cannot populate stats without a world.
	if (world == NULL)
		return;

	// Calculate body metrics.
	int dynamicBodyCount = 0;
	int kinematicBodyCount = 0;
	int activeBodyCount = 0;
	int sleepingBodyCount = 0;
	int discreteBodyCount = 0;
	int continuousBodyCount = 0;
	int activeColliderShapesCount = 0;
	int sleepingColliderShapesCount = 0;

	for (b2Body* body = world->GetBodyList (); body; body = body->GetNext ())
	{
		// Check body type.
		const b2BodyType bodyType = body->GetType ();
		if (bodyType == b2_staticBody)
			continue;
		else if (bodyType == b2_dynamicBody)
			dynamicBodyCount++;
		else if (bodyType == b2_kinematicBody)
			kinematicBodyCount++;

		// Check sleep state.
		if (body->IsAwake ())
		{
			activeBodyCount++;
			activeColliderShapesCount += body->GetFixtureCount ();
		}
		else
		{
			sleepingBodyCount++;
			sleepingColliderShapesCount += body->GetFixtureCount ();
		}

		// Check CCD state.
		if (body->IsBullet ())
			continuousBodyCount++;
		else
			discreteBodyCount++;
	}

	// Populate profile counts.
	stats.m_TotalBodyCount = world->GetBodyCount () - 1; // Ignore the hidden static ground-body.
	stats.m_ActiveBodyCount = activeBodyCount;
	stats.m_SleepingBodyCount = sleepingBodyCount;
	stats.m_DynamicBodyCount = dynamicBodyCount;
	stats.m_KinematicBodyCount = kinematicBodyCount;
	stats.m_DiscreteBodyCount = discreteBodyCount;
	stats.m_ContinuousBodyCount = continuousBodyCount;
	stats.m_ActiveColliderShapesCount = activeColliderShapesCount;
	stats.m_SleepingColliderShapesCount = sleepingColliderShapesCount;
	stats.m_JointCount = world->GetJointCount ();
	stats.m_ContactCount = world->GetContactCount ();	

	// Populate profile times.
	const b2Profile& timeProfile = world->GetProfile ();
	const float millisecondUpscale = 1000000.0f;
	stats.m_StepTime = (int)(timeProfile.step * millisecondUpscale);
	stats.m_CollideTime = (int)(timeProfile.collide * millisecondUpscale);
	stats.m_SolveTime = (int)(timeProfile.solve * millisecondUpscale);
	stats.m_SolveInitialization = (int)(timeProfile.solveInit * millisecondUpscale);
	stats.m_SolveVelocity = (int)(timeProfile.solveVelocity * millisecondUpscale);
	stats.m_SolvePosition = (int)(timeProfile.solvePosition * millisecondUpscale);
	stats.m_SolveBroadphase = (int)(timeProfile.broadphase * millisecondUpscale);
	stats.m_SolveTimeOfImpact = (int)(timeProfile.solveTOI * millisecondUpscale);
}

#endif

// --------------------------------------------------------------------------


void Physics2DManager::SetTransformMessageEnabled (const bool enable)
{
	for (size_t i = 0, n = m_AllCollider2DTypes.size(); i < n; ++i)
		GameObject::GetMessageHandler ().SetMessageEnabled (m_AllCollider2DTypes[i], kTransformChanged.messageID, enable);

	m_RigidbodyTransformMessageEnabled = enable;
}

#endif // #if ENABLE_2D_PHYSICS
