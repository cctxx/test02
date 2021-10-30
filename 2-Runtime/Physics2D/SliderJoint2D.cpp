#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Physics2D/SliderJoint2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"
#include "Runtime/Physics2D/Physics2DManager.h"

#include "Runtime/Graphics/Transform.h"
#include "Runtime/Math/Simd/math.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "External/Box2D/Box2D/Box2D.h"


IMPLEMENT_CLASS (SliderJoint2D)
IMPLEMENT_OBJECT_SERIALIZE (SliderJoint2D)


// --------------------------------------------------------------------------


SliderJoint2D::SliderJoint2D (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_OldReferenceAngle (std::numeric_limits<float>::infinity ())
{
}


SliderJoint2D::~SliderJoint2D ()
{
}


template<class TransferFunction>
void SliderJoint2D::Transfer (TransferFunction& transfer)
{
	Super::Transfer(transfer);

	TRANSFER (m_Anchor);
	TRANSFER (m_ConnectedAnchor);
	TRANSFER (m_Angle);
	TRANSFER (m_UseMotor);
	transfer.Align ();
	TRANSFER (m_Motor);
	TRANSFER (m_UseLimits);
	transfer.Align ();
	TRANSFER (m_TranslationLimits);
}


void SliderJoint2D::CheckConsistency ()	
{
	Super::CheckConsistency ();

	m_Motor.CheckConsistency ();
	m_TranslationLimits.CheckConsistency ();

	m_Angle = clamp<float> (m_Angle, 0.0f,  359.9999f);
	
	if (!IsFinite(m_Anchor))
		m_Anchor = Vector2f::zero;

	if (!IsFinite(m_ConnectedAnchor))
		m_ConnectedAnchor = Vector2f::zero;	
}


void SliderJoint2D::Reset ()
{
	Super::Reset ();

	m_Angle = 0.0f;
	m_UseMotor = false;
	m_UseLimits = false;
	m_Motor.Initialize ();
	m_TranslationLimits.Initialize ();

	m_Anchor = Vector2f::zero;
	m_ConnectedAnchor = Vector2f::zero;
}


void SliderJoint2D::SetAnchor (const Vector2f& anchor)
{
	ABORT_INVALID_VECTOR2 (anchor, anchor, SliderJoint2D);

	m_Anchor = anchor;
	SetDirty();

	// Recreate the joint.
	if (m_Joint != NULL)
		ReCreate();
}


void SliderJoint2D::SetConnectedAnchor (const Vector2f& anchor)
{
	ABORT_INVALID_VECTOR2 (anchor, connectedAnchor, SliderJoint2D);

	m_ConnectedAnchor = anchor;
	SetDirty();

	// Recreate the joint.
	if (m_Joint != NULL)
		ReCreate();
}


void SliderJoint2D::SetAngle (float angle)
{
	ABORT_INVALID_FLOAT (angle, angle, DistanceJoint2D);

	m_Angle = clamp<float> (angle, 0.0f,  359.9999f);
	SetDirty();

	// Recreate the joint.
	if (m_Joint != NULL)
		ReCreate();
}


void SliderJoint2D::SetUseMotor (bool enable)
{
	m_UseMotor = enable;
	SetDirty();

	if (m_Joint != NULL)
		((b2PrismaticJoint*)m_Joint)->EnableMotor(m_UseMotor);
}


void SliderJoint2D::SetUseLimits (bool enable)
{
	m_UseLimits = enable;
	SetDirty();

	if (m_Joint != NULL)
		((b2PrismaticJoint*)m_Joint)->EnableLimit(m_UseLimits);
}


void SliderJoint2D::SetMotor (const JointMotor2D& motor)
{
	m_Motor = motor;
	m_Motor.CheckConsistency ();
	SetDirty();

	// Motor is automatically enabled if motor is set.
	SetUseMotor(true);

	if (m_Joint != NULL)
	{
		b2PrismaticJoint* joint = (b2PrismaticJoint*)m_Joint;
		joint->SetMotorSpeed (math::radians (m_Motor.m_MotorSpeed));
		joint->SetMaxMotorForce (m_Motor.m_MaximumMotorForce);
	}
}


void SliderJoint2D::SetLimits (const JointTranslationLimits2D& limits)
{
	m_TranslationLimits = limits;
	m_TranslationLimits.CheckConsistency ();
	SetDirty();

	// Limits ares automatically enabled if limits are set.
	SetUseLimits(true);

	if (m_Joint != NULL)
	{
		b2PrismaticJoint* joint = (b2PrismaticJoint*)m_Joint;
		joint->SetLimits (m_TranslationLimits.m_LowerTranslation, m_TranslationLimits.m_UpperTranslation);
	}
}


// --------------------------------------------------------------------------


void SliderJoint2D::Create ()
{
	Assert (m_Joint == NULL);

	if (!IsActive ())
		return;

	// Fetch transform scales.
	const Vector3f scale = GetComponent (Transform).GetWorldScaleLossy ();
	const Vector3f connectedScale = m_ConnectedRigidBody.IsNull () ? Vector3f::one : m_ConnectedRigidBody->GetComponent (Transform).GetWorldScaleLossy ();

	// Fetch bodies.
	b2Body* bodyA = FetchBodyA();
	b2Body* bodyB = FetchBodyB();

	// Configure the joint definition.
	b2PrismaticJointDef jointDef;
	jointDef.localAnchorA.Set (m_Anchor.x * scale.x, m_Anchor.y * scale.y);
	jointDef.localAnchorB.Set (m_ConnectedAnchor.x * connectedScale.x, m_ConnectedAnchor.y * connectedScale.y);
	jointDef.enableMotor = m_UseMotor;
	jointDef.enableLimit = m_UseLimits;
	jointDef.motorSpeed = math::radians (m_Motor.m_MotorSpeed);
	jointDef.maxMotorForce = m_Motor.m_MaximumMotorForce;
	jointDef.lowerTranslation = m_TranslationLimits.m_LowerTranslation;
	jointDef.upperTranslation = m_TranslationLimits.m_UpperTranslation;
	jointDef.referenceAngle = m_OldReferenceAngle == std::numeric_limits<float>::infinity () ? bodyB->GetAngle() - bodyA->GetAngle() : m_OldReferenceAngle;

	float angle = math::radians (m_Angle);
	jointDef.localAxisA = bodyA->GetLocalVector (b2Vec2 (math::sin (angle), -math::cos(angle)));

	// Create the joint.
	FinalizeCreateJoint (&jointDef);
}


void SliderJoint2D::ReCreate()
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
