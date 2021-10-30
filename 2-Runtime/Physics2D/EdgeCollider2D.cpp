#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS

#include "Runtime/Physics2D/EdgeCollider2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"
#include "Runtime/Physics2D/Physics2DManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Graphics/SpriteFrame.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Filters/AABBUtility.h"
#include "Runtime/Filters/Mesh/SpriteRenderer.h"

#include "External/Box2D/Box2D/Box2D.h"
#include "External/libtess2/libtess2/tesselator.h"

PROFILER_INFORMATION(gPhysics2DProfileEdgeColliderCreate, "Physics2D.EdgeColliderCreate", kProfilerPhysics)

IMPLEMENT_CLASS (EdgeCollider2D)
IMPLEMENT_OBJECT_SERIALIZE (EdgeCollider2D)

#define EDGE_COLLIDER_2D_MIN_DISTANCE	(b2_linearSlop * b2_linearSlop * 2.01f)

// --------------------------------------------------------------------------


EdgeCollider2D::EdgeCollider2D (MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{
}


EdgeCollider2D::~EdgeCollider2D ()
{
}


template<class TransferFunction>
void EdgeCollider2D::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	TRANSFER (m_Points);
}


void EdgeCollider2D::Reset ()
{
	Super::Reset ();

	m_Points.clear ();
	m_Points.push_back (Vector2f (-0.5f, 0.0f));
	m_Points.push_back (Vector2f (0.5f, 0.0f));
}


void EdgeCollider2D::SmartReset ()
{
	Super::SmartReset ();

	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		if (aabb.GetExtent ().x < EDGE_COLLIDER_2D_MIN_DISTANCE)
		{
			m_Points.clear ();
			m_Points.push_back (Vector2f (-0.5f, 0.0f));
			m_Points.push_back (Vector2f (0.5f, 0.0f));
			return;
		}

		const Vector3f min = aabb.GetMin();
		const Vector3f max = aabb.GetMax();
		const float y = (min.y + max.y) * 0.5f;
		
		Vector2f points[2] = { Vector2f(min.x, y), Vector2f(max.x, y) };
		SetPoints (points, 2);
	}
}


bool EdgeCollider2D::SetPoints (const Vector2f* points, size_t count)
{
	// Fail if point count is invalid.
	if (count < 2)
		return false;

	m_Points.clear ();
	while (count-- > 0)
	{
		m_Points.push_back (*points++);
	}

	// Create the points.
	Create();

	return true;
}


// --------------------------------------------------------------------------


void EdgeCollider2D::Create (const Rigidbody2D* ignoreRigidbody)
{
	PROFILER_AUTO(gPhysics2DProfileEdgeColliderCreate, NULL);

	// Ensure we're cleaned-up.
	Cleanup ();

	// Ignore if not active.
	if (!IsActive() || m_Points.size () < 2)
		return;

	// Calculate collider transformation.
	Matrix4x4f relativeTransform;
	b2Body* body;
	CalculateColliderTransformation (ignoreRigidbody, &body, relativeTransform);

	// Fetch the collider scale.
	const Vector3f scale = GetComponent(Transform).GetWorldScaleLossy();

	// Transform the chain.
	b2Vec2* transformedPoints;
	ALLOC_TEMP(transformedPoints, b2Vec2, m_Points.size () + 1);	// We add an extra one in-case we need to extend.
	const int validPointCount = TransformPoints (relativeTransform, scale, transformedPoints);
	
	// Invalid chain if a single edge doesn't exit.
	if (validPointCount < 2)
		return;

	// Check vertex distances.
	for (int i = 1; i < validPointCount; ++i)
		if (b2DistanceSquared (transformedPoints[i-1], transformedPoints[i]) < EDGE_COLLIDER_2D_MIN_DISTANCE)
			return;

	// Generate the chain shape.
	b2ChainShape chainShape;
	chainShape.CreateChain (transformedPoints, validPointCount);

	// Create the chain fixture.
	dynamic_array<b2Shape*> chainShapes(kMemTempAlloc);
	chainShapes.push_back (&chainShape);
	b2FixtureDef def;
	FinalizeCreate(def, body, &chainShapes);	
}


// --------------------------------------------------------------------------


int EdgeCollider2D::TransformPoints(const Matrix4x4f& relativeTransform, const Vector3f& scale, b2Vec2* outPoints)
{
	const size_t inPointCount = m_Points.size ();
	const Vector2f* edgePoint = m_Points.data ();
	int outCount = 0;
	for (size_t i = 0; i <inPointCount; ++i, ++edgePoint)
	{
		// Calculate transform points.
		const Vector3f vertex3D = relativeTransform.MultiplyPoint3 (Vector3f(edgePoint->x * scale.x, edgePoint->y * scale.y, 0.0f));
		const b2Vec2 vertex2D(vertex3D.x, vertex3D.y);		

		// Skip point if they end up being too close. Box2d fires asserts if distance between neighbors is less than b2_linearSlop.
		if (outCount > 0 && b2DistanceSquared(*(outPoints-1), vertex2D) <= EDGE_COLLIDER_2D_MIN_DISTANCE)
			continue;

		*outPoints++ = vertex2D;
		++outCount;
	}

	return outCount;
}


#endif // #if ENABLE_2D_PHYSICS
