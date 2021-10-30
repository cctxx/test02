#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Physics2D/SpringJoint2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"

#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/ValidateArgs.h"

#include "External/Box2D/Box2D/Box2D.h"

IMPLEMENT_CLASS (SpringJoint2D)
IMPLEMENT_OBJECT_SERIALIZE (SpringJoint2D)


// --------------------------------------------------------------------------


SpringJoint2D::SpringJoint2D (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}


SpringJoint2D::~SpringJoint2D ()
{
}


template<class TransferFunction>
void SpringJoint2D::Transfer (TransferFunction& transfer)
{
	Super::Transfer(transfer);

	TRANSFER (m_Anchor);
	TRANSFER (m_ConnectedAnchor);
	TRANSFER (m_Distance);
	TRANSFER (m_DampingRatio);
	TRANSFER (m_Frequency);
}


void SpringJoint2D::CheckConsistency ()	
{
	Super::CheckConsistency ();

	m_Distance = clamp<float> (m_Distance, b2_linearSlop, PHYSICS_2D_LARGE_RANGE_CLAMP);
	m_Frequency = clamp<float> (m_Frequency, 0.0f, PHYSICS_2D_LARGE_RANGE_CLAMP);
	m_DampingRatio = clamp(m_DampingRatio, 0.0f, 1.0f);

	if (!IsFinite(m_Anchor))
		m_Anchor = Vector2f::zero;

	if (!IsFinite(m_ConnectedAnchor))
		m_ConnectedAnchor = Vector2f::zero;
}


void SpringJoint2D::Reset ()
{
	Super::Reset ();

	m_Distance = 1.0f;
	m_DampingRatio = 0.0f;
	m_Frequency = 10.0f;
	m_Anchor = Vector2f::zero;
	m_ConnectedAnchor = Vector2f::zero;
}


void SpringJoint2D::SetAnchor (const Vector2f& anchor)
{
	ABORT_INVALID_VECTOR2 (anchor, anchor, SpringJoint2D);

	m_Anchor = anchor;
	SetDirty();

	// Recreate the joint.
	if (m_Joint != NULL)
		ReCreate();
}


void SpringJoint2D::SetConnectedAnchor (const Vector2f& anchor)
{
	ABORT_INVALID_VECTOR2 (anchor, connectedAnchor, SpringJoint2D);

	m_ConnectedAnchor = anchor;
	SetDirty();

	// Recreate the joint.
	if (m_Joint != NULL)
		ReCreate();
}


void SpringJoint2D::SetDistance (float distance)
{
	ABORT_INVALID_FLOAT (distance, distance, SpringJoint2D);

	m_Distance = clamp<float> (distance, b2_linearSlop, PHYSICS_2D_LARGE_RANGE_CLAMP);
	SetDirty();

	if (m_Joint != NULL)
		((b2DistanceJoint*)m_Joint)->SetLength (m_Distance);
}


void SpringJoint2D::SetDampingRatio (float ratio)
{
	ABORT_INVALID_FLOAT (ratio, dampingRatio, SpringJoint2D);

	m_DampingRatio = clamp(ratio, 0.0f, 1.0f);
	SetDirty();

	if (m_Joint != NULL)
		((b2DistanceJoint*)m_Joint)->SetDampingRatio (m_DampingRatio);
}


void SpringJoint2D::SetFrequency (float frequency)
{
	ABORT_INVALID_FLOAT (frequency, frequency, SpringJoint2D);

	m_Frequency = clamp<float> (frequency, 0.0f, PHYSICS_2D_LARGE_RANGE_CLAMP);
	SetDirty();

	if (m_Joint != NULL)
		((b2DistanceJoint*)m_Joint)->SetFrequency (m_Frequency);
}

// --------------------------------------------------------------------------


void SpringJoint2D::Create ()
{
	Assert (m_Joint == NULL);

	if (!IsActive ())
		return;

	// Fetch transform scales.
	const Vector3f scale = GetComponent (Transform).GetWorldScaleLossy ();
	const Vector3f connectedScale = m_ConnectedRigidBody.IsNull () ? Vector3f::one : m_ConnectedRigidBody->GetComponent (Transform).GetWorldScaleLossy ();

	// Configure the joint definition.
	b2DistanceJointDef jointDef;
	jointDef.dampingRatio = m_DampingRatio;
	jointDef.frequencyHz = m_Frequency;
	jointDef.length = m_Distance;
	jointDef.localAnchorA.Set (m_Anchor.x * scale.x, m_Anchor.y * scale.y);
	jointDef.localAnchorB.Set (m_ConnectedAnchor.x * connectedScale.x, m_ConnectedAnchor.y * connectedScale.y);
		
	// Create the joint.
	FinalizeCreateJoint (&jointDef);
}


#endif //ENABLE_2D_PHYSICS
