#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS
#include "Runtime/Physics2D/BoxCollider2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"
#include "Runtime/Physics2D/Physics2DManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/AABBUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Utilities/ValidateArgs.h"

#include "External/Box2D/Box2D/Box2D.h"

PROFILER_INFORMATION(gPhysics2DProfileBoxColliderCreate, "Physics2D.BoxColliderCreate", kProfilerPhysics)

IMPLEMENT_CLASS (BoxCollider2D)
IMPLEMENT_OBJECT_SERIALIZE (BoxCollider2D)


// --------------------------------------------------------------------------


BoxCollider2D::BoxCollider2D (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
}


BoxCollider2D::~BoxCollider2D ()
{
}


template<class TransferFunction>
void BoxCollider2D::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);

	TRANSFER (m_Size);
	TRANSFER (m_Center);
}


void BoxCollider2D::CheckConsistency ()
{
	Super::CheckConsistency ();

	if (IsFinite (m_Size))
	{
		m_Size.x = std::max (PHYSICS_2D_SMALL_RANGE_CLAMP, m_Size.x);
		m_Size.y = std::max (PHYSICS_2D_SMALL_RANGE_CLAMP, m_Size.y);
	}
	else
	{
		m_Size.Set (1.0f, 1.0f);
	}

	if (!IsFinite (m_Center))
		m_Center = Vector2f::zero;
}


void BoxCollider2D::Reset ()
{
	Super::Reset ();

	m_Size.Set (1.0f, 1.0f);
	m_Center = Vector2f::zero;
}


void BoxCollider2D::SmartReset ()
{
	Super::SmartReset ();

	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		m_Size.x = aabb.GetExtent().x * 2.0f;
		m_Size.y = aabb.GetExtent().y * 2.0f;
		m_Center.x = aabb.GetCenter().x;
		m_Center.y = aabb.GetCenter().y;
		return;
	}

	m_Size.Set (1.0f, 1.0f);
	m_Center = Vector2f::zero;
}


void BoxCollider2D::SetSize (const Vector2f& size)
{
	ABORT_INVALID_VECTOR2 (size, size, BoxCollider2D);

	// Finish if no change.
	if (m_Size == size)
		return;

	m_Size.x = std::max (PHYSICS_2D_SMALL_RANGE_CLAMP, size.x);
	m_Size.y = std::max (PHYSICS_2D_SMALL_RANGE_CLAMP, size.y);

	SetDirty ();

	Create();
}


void BoxCollider2D::SetCenter (const Vector2f& center)
{
	ABORT_INVALID_VECTOR2 (center, center, BoxCollider2D);

	// Finish if no change.
	if (m_Center == center)
		return;

	m_Center = center;
	SetDirty ();

	Create();
}


// --------------------------------------------------------------------------


void BoxCollider2D::Create (const Rigidbody2D* ignoreRigidbody)
{
	PROFILER_AUTO(gPhysics2DProfileBoxColliderCreate, NULL);

	// Ensure we're cleaned-up.
	Cleanup ();

	// Ignore if not active.
	if (!IsActive())
		return;

	// Calculate collider transformation.
	Matrix4x4f relativeTransform;
	b2Body* body;
	CalculateColliderTransformation (ignoreRigidbody, &body, relativeTransform);

	// Calculate collider center.
	const Vector3f scale = GetComponent(Transform).GetWorldScaleLossy();
	Vector3f center = relativeTransform.MultiplyPoint3 (Vector3f(m_Center.x * scale.x, m_Center.y * scale.y, 0.0f));

	// Calculate rotation.
	Quaternionf quat;
	MatrixToQuaternion(relativeTransform, quat);
	const Vector3f euler = QuaternionToEuler(quat);

	// Calculate scaled size.
	Vector3f size = GetComponent (Transform).GetWorldScaleLossy ();
	size.x = max(Abs(size.x * m_Size.x), PHYSICS_2D_SMALL_RANGE_CLAMP);
	size.y = max(Abs(size.y * m_Size.y), PHYSICS_2D_SMALL_RANGE_CLAMP);

	// Create the shape.
	b2PolygonShape shape;
	shape.SetAsBox (size.x*0.5f, size.y*0.5f, b2Vec2(center.x, center.y), euler.z);
	b2FixtureDef def;
	def.shape = &shape;

	// Finalize the creation.
	FinalizeCreate (def, body);
}

#endif // #if ENABLE_2D_PHYSICS
