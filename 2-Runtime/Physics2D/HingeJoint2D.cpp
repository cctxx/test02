#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Physics2D/HingeJoint2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"

#include "Runtime/Graphics/Transform.h"
#include "Runtime/Math/Simd/math.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/ValidateArgs.h"

#include "External/Box2D/Box2D/Box2D.h"


IMPLEMENT_CLASS (HingeJoint2D)
IMPLEMENT_OBJECT_SERIALIZE (HingeJoint2D)


// --------------------------------------------------------------------------


HingeJoint2D::HingeJoint2D (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_OldReferenceAngle (std::numeric_limits<float>::infinity ())
{
}


HingeJoint2D::~HingeJoint2D ()
{
}


template<class TransferFunction>
void HingeJoint2D::Transfer (TransferFunction& transfer)
{
	Super::Transfer(transfer);

	TRANSFER (m_Anchor);
	TRANSFER (m_ConnectedAnchor);
	TRANSFER (m_UseMotor);
	transfer.Align ();
	TRANSFER (m_Motor);
	TRANSFER (m_UseLimits);
	transfer.Align ();
	TRANSFER (m_AngleLimits);
}


void HingeJoint2D::CheckConsistency ()	
{
	Super::CheckConsistency ();

	m_Motor.CheckConsistency ();
	m_AngleLimits.CheckConsistency ();

	if (!IsFinite(m_Anchor))
		m_Anchor = Vector2f::zero;

	if (!IsFinite(m_ConnectedAnchor))
		m_ConnectedAnchor = Vector2f::zero;
}


void HingeJoint2D::Reset ()
{
	Super::Reset ();

	m_UseMotor = false;
	m_UseLimits = false;
	m_Motor.Initialize ();
	m_AngleLimits.Initialize ();

	m_Anchor = Vector2f::zero;
	m_ConnectedAnchor = Vector2f::zero;
}


void HingeJoint2D::SetAnchor (const Vector2f& anchor)
{
	ABORT_INVALID_VECTOR2 (anchor, anchor, HingeJoint2D);

	m_Anchor = anchor;
	SetDirty();

	// Recreate the joint.
	if (m_Joint != NULL)
		ReCreate();
}


void HingeJoint2D::SetConnectedAnchor (const Vector2f& anchor)
{
	ABORT_INVALID_VECTOR2 (anchor, connectedAnchor, HingeJoint2D);

	m_ConnectedAnchor = anchor;
	SetDirty();

	// Recreate the joint.
	if (m_Joint != NULL)
		ReCreate();
}


void HingeJoint2D::SetUseMotor (bool enable)
{
	m_UseMotor = enable;
	SetDirty();

	if (m_Joint != NULL)
		((b2RevoluteJoint*)m_Joint)->EnableMotor(m_UseMotor);
}


void HingeJoint2D::SetUseLimits (bool enable)
{
	m_UseLimits = enable;
	SetDirty();

	if (m_Joint != NULL)
		((b2RevoluteJoint*)m_Joint)->EnableLimit(m_UseLimits);
}


void HingeJoint2D::SetMotor (const JointMotor2D& motor)
{
	m_Motor = motor;
	m_Motor.CheckConsistency ();
	SetDirty();

	// Motor is automatically enabled if motor is set.
	SetUseMotor(true);

	if (m_Joint != NULL)
	{
		b2RevoluteJoint* joint = (b2RevoluteJoint*)m_Joint;
		joint->SetMotorSpeed (math::radians (m_Motor.m_MotorSpeed));
		joint->SetMaxMotorTorque (m_Motor.m_MaximumMotorForce);		
	}
}


void HingeJoint2D::SetLimits (const JointAngleLimits2D& limits)
{
	m_AngleLimits = limits;
	m_AngleLimits.CheckConsistency ();
	SetDirty();

	// Limits ares automatically enabled if limits are set.
	SetUseLimits(true);

	if (m_Joint != NULL)
	{
		b2RevoluteJoint* joint = (b2RevoluteJoint*)m_Joint;
		joint->SetLimits(math::radians (m_AngleLimits.m_LowerAngle), math::radians (m_AngleLimits.m_UpperAngle));
	}
}


// --------------------------------------------------------------------------


void HingeJoint2D::Create ()
{
	Assert (m_Joint == NULL);

	if (!IsActive ())
		return;

	// Fetch transform scales.
	const Vector3f scale = GetComponent (Transform).GetWorldScaleLossy ();
	const Vector3f connectedScale = m_ConnectedRigidBody.IsNull () ? Vector3f::one : m_ConnectedRigidBody->GetComponent (Transform).GetWorldScaleLossy ();

	// Configure the joint definition.
	b2RevoluteJointDef jointDef;
	jointDef.localAnchorA.Set (m_Anchor.x * scale.x, m_Anchor.y * scale.y);
	jointDef.localAnchorB.Set (m_ConnectedAnchor.x * connectedScale.x, m_ConnectedAnchor.y * connectedScale.y);
	jointDef.enableMotor = m_UseMotor;
	jointDef.enableLimit = m_UseLimits;
	jointDef.motorSpeed = math::radians (m_Motor.m_MotorSpeed);
	jointDef.maxMotorTorque = m_Motor.m_MaximumMotorForce;
	jointDef.lowerAngle = math::radians (m_AngleLimits.m_LowerAngle);
	jointDef.upperAngle = math::radians (m_AngleLimits.m_UpperAngle);
	if (jointDef.lowerAngle > jointDef.upperAngle)
		std::swap(jointDef.lowerAngle, jointDef.upperAngle);
	jointDef.referenceAngle = m_OldReferenceAngle == std::numeric_limits<float>::infinity () ? FetchBodyB()->GetAngle() - FetchBodyA()->GetAngle() : m_OldReferenceAngle;
		
	// Create the joint.
	FinalizeCreateJoint (&jointDef);
}


void HingeJoint2D::ReCreate()
{
	// Do we have an existing joint and we're still active/enabled?
	if (m_Joint != NULL && IsActive () && GetEnabled ())
	{
		// Yes, so keep reference angle.
		m_OldReferenceAngle = ((b2RevoluteJoint*)m_Joint)->GetReferenceAngle ();
	}
	else
	{
		// No, so reset reference angle.
		m_OldReferenceAngle = std::numeric_limits<float>::infinity ();
	}

	Super::ReCreate ();
}

#endif //ENABLE_2D_PHYSICS
