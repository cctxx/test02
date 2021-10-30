#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Physics2D/DistanceJoint2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"

#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/ValidateArgs.h"

#include "External/Box2D/Box2D/Box2D.h"


IMPLEMENT_CLASS (DistanceJoint2D)
IMPLEMENT_OBJECT_SERIALIZE (DistanceJoint2D)


// --------------------------------------------------------------------------


DistanceJoint2D::DistanceJoint2D (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}


DistanceJoint2D::~DistanceJoint2D ()
{
}


template<class TransferFunction>
void DistanceJoint2D::Transfer (TransferFunction& transfer)
{
	Super::Transfer(transfer);

	TRANSFER (m_Anchor);
	TRANSFER (m_ConnectedAnchor);
	TRANSFER (m_Distance);
}


void DistanceJoint2D::CheckConsistency ()	
{
	Super::CheckConsistency ();

	m_Distance = clamp<float> (m_Distance, b2_linearSlop, PHYSICS_2D_LARGE_RANGE_CLAMP);

	if (!IsFinite(m_Anchor))
		m_Anchor = Vector2f::zero;

	if (!IsFinite(m_ConnectedAnchor))
		m_ConnectedAnchor = Vector2f::zero;
}


void DistanceJoint2D::Reset ()
{
	Super::Reset ();

	m_Distance = 1.0f;
	m_Anchor = Vector2f::zero;
	m_ConnectedAnchor = Vector2f::zero;
}


void DistanceJoint2D::SetAnchor (const Vector2f& anchor)
{
	ABORT_INVALID_VECTOR2 (anchor, anchor, DistanceJoint2D);

	m_Anchor = anchor;
	SetDirty();

	// Recreate the joint.
	if (m_Joint != NULL)
		ReCreate();
}


void DistanceJoint2D::SetConnectedAnchor (const Vector2f& anchor)
{
	ABORT_INVALID_VECTOR2 (anchor, connectedAnchor, DistanceJoint2D);

	m_ConnectedAnchor = anchor;
	SetDirty();

	// Recreate the joint.
	if (m_Joint != NULL)
		ReCreate();
}


void DistanceJoint2D::SetDistance (float distance)
{
	ABORT_INVALID_FLOAT (distance, distance, DistanceJoint2D);

	m_Distance = clamp<float> (distance, b2_linearSlop, PHYSICS_2D_LARGE_RANGE_CLAMP);
	SetDirty();

	if (m_Joint != NULL)
		((b2RopeJoint*)m_Joint)->SetMaxLength (m_Distance);
}


// --------------------------------------------------------------------------


void DistanceJoint2D::Create ()
{
	Assert (m_Joint == NULL);

	if (!IsActive ())
		return;

	// Fetch transform scales.
	const Vector3f scale = GetComponent (Transform).GetWorldScaleLossy ();
	const Vector3f connectedScale = m_ConnectedRigidBody.IsNull () ? Vector3f::one : m_ConnectedRigidBody->GetComponent (Transform).GetWorldScaleLossy ();

	// Configure the joint definition.
	b2RopeJointDef jointDef;
	jointDef.maxLength = m_Distance;
	jointDef.localAnchorA.Set (m_Anchor.x * scale.x, m_Anchor.y * scale.y);
	jointDef.localAnchorB.Set (m_ConnectedAnchor.x * connectedScale.x, m_ConnectedAnchor.y * connectedScale.y);
		
	// Create the joint.
	FinalizeCreateJoint (&jointDef);
}


void DistanceJoint2D::AutoCalculateDistance ()
{
	// Reset to default.
	m_Distance = 1.0f;

	if (m_ConnectedRigidBody.IsNull ())
		return;

	// Find the appropriate rigid body A.
	Rigidbody2D* rigidBodyA = QueryComponent(Rigidbody2D);
	Assert (rigidBodyA != NULL);

	// Find the appropriate rigid body B.
	Rigidbody2D* rigidBodyB = m_ConnectedRigidBody;

	Transform* transformA = QueryComponent (Transform);
	Vector3f pointA = transformA->TransformPoint(Vector3f(m_Anchor.x, m_Anchor.y, 0.0f));

	if (rigidBodyB == NULL)
	{
		m_Distance = Magnitude(Vector2f(pointA.x, pointA.y));
		return;
	}

	Transform* transformB = rigidBodyB->GetGameObjectPtr ()->QueryComponent (Transform);
	Vector3f pointB = transformB->TransformPoint(Vector3f(m_ConnectedAnchor.x, m_ConnectedAnchor.y, 0.0f));

	m_Distance = Magnitude(Vector2f(pointA.x-pointB.x, pointA.y-pointB.y));
}

#endif //ENABLE_2D_PHYSICS
