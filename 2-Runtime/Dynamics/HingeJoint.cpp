#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "Runtime/Utilities/StaticAssert.h"
#include "HingeJoint.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "PhysicsManager.h"

#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"

using namespace std;

namespace Unity
{

#define GET_JOINT() static_cast<NxRevoluteJoint*> (m_Joint)

/*
- We awake the hingejoint only once. (AwakeFromLoad)
  At this point we setup the axes. They are never changed afterwards
  -> The perfect solution remembers the old position/rotation of the rigid bodies.
      Then when changing axis/anchor is changed it generates axes that are rleative to the old position/rotation state!
*/

HingeJoint::HingeJoint (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	InitJointLimits (m_Limits);
	InitJointSpring (m_Spring);
	InitJointMotor (m_Motor);

	m_UseLimits = false;
	m_UseMotor = false;
	m_UseSpring = false;

	CompileTimeAssert(sizeof(JointMotor)  == sizeof(NxMotorDesc), "Unity JointMotor type has different size from physx one");
	CompileTimeAssert(sizeof(JointSpring) == sizeof(NxSpringDesc), "Unity JointSpring type has different size from physx one");
	CompileTimeAssert(sizeof(JointLimits) == sizeof(NxJointLimitPairDesc), "Unity JointLimits type has different size from physx one");
}

HingeJoint::~HingeJoint ()
{
}

inline NxSpringDesc ToNovodex (const JointSpring& spring)
{
	JointSpring desc = spring;
	desc.targetPosition = Deg2Rad (desc.targetPosition);
	return (const NxSpringDesc&)desc;
}

inline NxMotorDesc ToNovodex (const JointMotor& motor)
{
	JointMotor desc = motor;
	desc.targetVelocity = Deg2Rad (desc.targetVelocity);
	return (const NxMotorDesc&)desc;
}

#define kHingeJointLimitError "Joint limits for hinge joint out of bounds. Limits need to be between -360 and 360 degrees."
inline NxJointLimitPairDesc ToNovodex (const JointLimits& limits)
{
	NxJointLimitPairDesc l = (const NxJointLimitPairDesc&)limits;
	if(IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1))
	{
		// PhysX specs limit hinge joints to [-180,180] degress. But it seems 360 works fine
		// and is very useful in practice. Higher numbers produce broken results (case 492847)
		if (l.low.value < -360)
		{
			l.low.value = -360;
			ErrorString (kHingeJointLimitError);
		}
		if (l.low.value > 360)
		{
			l.low.value = 360;
			ErrorString (kHingeJointLimitError);
		}
		if (l.high.value < -360)
		{
			l.high.value = -360;
			ErrorString (kHingeJointLimitError);
		}
		if (l.high.value > 360)
		{
			l.high.value = 360;
			ErrorString (kHingeJointLimitError);
		}
	}
	l.low.value = Deg2Rad (l.low.value);
	l.high.value = Deg2Rad (l.high.value);

	/// DO SOME SPECIAL HANDLING WHEN LOW IS LARGER THAN HIGH!!!!!
	/// MAKE IT MORE INTUITIVE

	if (l.low.value > l.high.value)
		swap (l.low, l.high);

	return l;
}

void HingeJoint::SetUseMotor (bool enable)
{
	SetDirty ();
	m_UseMotor = enable;
	if (m_Joint)
	{
		UInt32 flags = GET_JOINT()->getFlags ();
		if (enable)
			flags |= NX_RJF_MOTOR_ENABLED;
		else
			flags &= ~NX_RJF_MOTOR_ENABLED;
		GET_JOINT()->setFlags (flags);
	}
}

bool HingeJoint::GetUseMotor () const
{
	return m_UseMotor;
}

void HingeJoint::SetUseLimits (bool enable)
{
	SetDirty ();
	m_UseLimits = enable;
	if (m_Joint)
	{
		UInt32 flags = GET_JOINT()->getFlags ();
		if (enable)
			flags |= NX_RJF_LIMIT_ENABLED;
		else
			flags &= ~NX_RJF_LIMIT_ENABLED;
		GET_JOINT()->setFlags (flags);
	}
}

bool HingeJoint::GetUseLimits () const
{
	return m_UseLimits;
}

void HingeJoint::SetUseSpring (bool enable)
{
	SetDirty ();
	m_UseSpring = enable;
	if (m_Joint)
	{
		UInt32 flags = GET_JOINT()->getFlags ();
		if (enable)
			flags |= NX_RJF_SPRING_ENABLED;
		else
			flags &= ~NX_RJF_SPRING_ENABLED;
		GET_JOINT()->setFlags (flags);
	}
}

bool HingeJoint::GetUseSpring () const
{
	return m_UseSpring;
}

JointMotor HingeJoint::GetMotor () const
{
	return m_Motor;
}

void HingeJoint::SetMotor (const JointMotor& motor)
{
	SetDirty ();
	m_Motor = motor;
	if (m_Joint)
	{
		GET_JOINT()->setMotor (ToNovodex (motor));
	}
}

JointSpring HingeJoint::GetSpring () const
{
	return m_Spring;
}

void HingeJoint::SetSpring (const JointSpring& spring)
{
	SetDirty ();
	m_Spring = spring;
	if (m_Joint)
		GET_JOINT()->setSpring (ToNovodex (spring));
}

JointLimits HingeJoint::GetLimits () const
{
	return m_Limits;
}

void HingeJoint::SetLimits (const JointLimits& limits)
{
	SetDirty ();
	m_Limits = limits;
	if (m_Joint)
	{
		GET_JOINT()->setLimits (ToNovodex (limits));

		// Novodex seems to have a bug that it will not update HingeJoints when only the limits are changed.
		// Also call setMotor to force it to update.
		GET_JOINT()->setMotor (ToNovodex (m_Motor));
	}
}

float HingeJoint::GetVelocity () const
{
	if (m_Joint)
		return Rad2Deg (GET_JOINT()->getVelocity ());
	else
		return 0.0F;
}

float HingeJoint::GetAngle () const
{
	if (m_Joint)
		return Rad2Deg (GET_JOINT()->getAngle ());
	else
		return 0.0F;
}

/*
- When the rigid body which is attached with the hinge joint is activated after the joint is loaded
  It will not be connected with the joint!
*/

template<class TransferFunction>
void HingeJoint::Transfer (TransferFunction& transfer)
{
	JointTransferPre (transfer);

	TRANSFER (m_UseSpring);
	transfer.Align();
	TRANSFER (m_Spring);

	TRANSFER (m_UseMotor);
	transfer.Align();
	TRANSFER (m_Motor);

	TRANSFER (m_UseLimits);
	transfer.Align();
	TRANSFER (m_Limits);

	JointTransferPost (transfer);
}

void HingeJoint::Create ()
{
	AssertIf (!IsActive ());

	NxRevoluteJointDesc desc;

	if (m_Joint && m_Joint->getState () == NX_JS_SIMULATING)
		GET_JOINT()->saveToDesc (desc);

	desc.motor = ToNovodex (m_Motor);
	desc.limit = ToNovodex (m_Limits);
	desc.spring = ToNovodex (m_Spring);

	desc.flags = 0;
	if (m_UseMotor)
		desc.flags |= NX_RJF_MOTOR_ENABLED;
	if (m_UseLimits)
		desc.flags |= NX_RJF_LIMIT_ENABLED;
	if (m_UseSpring)
		desc.flags |= NX_RJF_SPRING_ENABLED;

	FINALIZE_CREATE (desc, NxRevoluteJoint);
}

IMPLEMENT_AXIS_ANCHOR(HingeJoint,NxRevoluteJointDesc)

}

IMPLEMENT_CLASS (HingeJoint)
IMPLEMENT_OBJECT_SERIALIZE (HingeJoint)

#undef GET_JOINT
#endif //ENABLE_PHSYICS
