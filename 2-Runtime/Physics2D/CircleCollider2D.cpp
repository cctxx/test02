#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS
#include "Runtime/Physics2D/CircleCollider2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"
#include "Runtime/Physics2D/Physics2DManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/AABBUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Utilities/ValidateArgs.h"

#include "External/Box2D/Box2D/Box2D.h"

PROFILER_INFORMATION(gPhysics2DProfileCircleColliderCreate, "Physics2D.CircleColliderCreate", kProfilerPhysics)


IMPLEMENT_CLASS (CircleCollider2D)
IMPLEMENT_OBJECT_SERIALIZE (CircleCollider2D)


// --------------------------------------------------------------------------


CircleCollider2D::CircleCollider2D (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
}


CircleCollider2D::~CircleCollider2D ()
{
}


template<class TransferFunction>
void CircleCollider2D::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);

	TRANSFER (m_Radius);
	TRANSFER (m_Center);
}


void CircleCollider2D::CheckConsistency ()
{
	Super::CheckConsistency ();

	m_Radius = clamp<float> (m_Radius, PHYSICS_2D_SMALL_RANGE_CLAMP, PHYSICS_2D_LARGE_RANGE_CLAMP);

	if (!IsFinite (m_Center))
		m_Center = Vector2f::zero;
}


void CircleCollider2D::Reset ()
{
	Super::Reset ();

	m_Radius = 0.5f;
	m_Center = Vector2f::zero;
}


void CircleCollider2D::SmartReset ()
{
	Super::SmartReset ();

	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		Vector3f dist = aabb.GetExtent ();
		m_Radius = clamp<float> (std::max(dist.x, dist.y), PHYSICS_2D_SMALL_RANGE_CLAMP, PHYSICS_2D_LARGE_RANGE_CLAMP);		
		m_Center.x = aabb.GetCenter().x;
		m_Center.y = aabb.GetCenter().y;
		return;
	}

	m_Radius = 0.5f;
	m_Center = Vector2f::zero;
}


void CircleCollider2D::SetRadius (float radius)
{
	ABORT_INVALID_FLOAT (radius, radius, CircleCollider2D);

	// Finish if no change.
	if (m_Radius == radius)
		return;

	m_Radius = clamp<float> (radius, PHYSICS_2D_SMALL_RANGE_CLAMP, PHYSICS_2D_LARGE_RANGE_CLAMP);

	if (GetShape() == NULL)
		return;

	SetDirty ();
	Create();
}


void CircleCollider2D::SetCenter (const Vector2f& center)
{
	ABORT_INVALID_VECTOR2 (center, center, CircleCollider2D);

	// Finish if no change.
	if (m_Center == center)
		return;

	m_Center = center;

	SetDirty ();
	Create();
}


// --------------------------------------------------------------------------


void CircleCollider2D::Create (const Rigidbody2D* ignoreRigidbody)
{
	PROFILER_AUTO(gPhysics2DProfileCircleColliderCreate, NULL);

	// Ensure we're cleaned-up.
	Cleanup ();

	// Ignore if not active.
	if (!IsActive())
		return;

	// Calculate collider transformation.
	Matrix4x4f relativeTransform;
	b2Body* body;
	CalculateColliderTransformation (ignoreRigidbody, &body, relativeTransform);

	// Fetch scale.
	const Vector3f scale = GetComponent(Transform).GetWorldScaleLossy();

	// Calculate collider center.
	Vector3f center = relativeTransform.MultiplyPoint3(Vector3f(m_Center.x * scale.x, m_Center.y * scale.y, 0.0f));

	// Calculate scaled radius.
	const float scaledRadius = clamp<float> (max( PHYSICS_2D_SMALL_RANGE_CLAMP, max (Abs (scale.x), Abs (scale.y)) * m_Radius), PHYSICS_2D_SMALL_RANGE_CLAMP, PHYSICS_2D_LARGE_RANGE_CLAMP);

	// Create the shape.
	b2CircleShape shape;
	shape.m_p.Set(center.x, center.y);
	shape.m_radius = scaledRadius;
	b2FixtureDef def;
	def.shape = &shape;

	// Finalize the creation.
	FinalizeCreate (def, body);
}

#endif // #if ENABLE_2D_PHYSICS
