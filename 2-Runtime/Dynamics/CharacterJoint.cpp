#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "CharacterJoint.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "PhysicsManager.h"
#include "Runtime/Utilities/Utility.h"

#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"

using namespace std;

namespace Unity
{

#define GET_JOINT() static_cast<NxD6Joint*> (m_Joint)

inline void FixupNovodexLimitBug (NxD6JointDesc& desc)
{
	desc.twistMotion = desc.swing2Motion = desc.swing1Motion = NX_D6JOINT_MOTION_LIMITED;
}


/*
- We awake the hingejoint only once. (AwakeFromLoad)
  At this point we setup the axes. They are never changed afterwards
  -> The perfect solution remembers the old position/rotation of the rigid bodies.
      Then when changing axis/anchor is changed it generates axes that are rleative to the old position/rotation state!
*/

CharacterJoint::CharacterJoint (MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{
	m_TargetRotation = Quaternionf::identity();
//	m_TargetAngularVelocity = Vector3f::zero;
	m_UseTargetRotation = false;
	m_SwingAxis = Vector3f::yAxis;
}

CharacterJoint::~CharacterJoint ()
{
}

void CharacterJoint::CalculateGlobalHingeSpace (Vector3f& globalAnchor, Vector3f& globalAxis, Vector3f& globalNormal) const
{
	const Transform& transform = GetComponent (Transform);

	Vector3f localAxis = m_Axis;
	if (SqrMagnitude (localAxis) < Vector3f::epsilon)
		localAxis = Vector3f (1.0F, 0.0F, 0.0F);
	Vector3f localNormal = m_SwingAxis;

	OrthoNormalize (&localAxis, &localNormal);

	globalAnchor = transform.TransformPoint (m_Anchor);
//	Vector3f globalRigidbodyPos = transform.GetPosition ();
	globalAxis = transform.TransformDirection (localAxis);
	
	globalNormal = transform.TransformDirection (localNormal);
	
	Matrix3x3f m;
	m.SetOrthoNormalBasisInverse(globalAxis, globalNormal, Cross (globalAxis, globalNormal));
//	m.SetOrthoNormalBasisInverse(Vector3f::xAxis, globalNormal, Cross (globalAxis, globalNormal));
//	m.SetOrthoNormalBasisInverse(globalNormal, Cross (globalAxis, globalNormal), globalAxis);
//	OrthoNormalize(m);
	Quaternionf q;
	MatrixToQuaternion(m, q);
	m_ConfigurationSpace = q * Inverse(transform.GetRotation ());
}

void CharacterJoint::Reset ()
{
	Super::Reset();
	
	m_RotationDrive.maximumForce = 20;
	m_RotationDrive.positionSpring = 50;
	m_RotationDrive.positionDamper = 5;
	
	InitSoftJointLimit (m_LowTwistLimit);
	InitSoftJointLimit (m_HighTwistLimit);
	InitSoftJointLimit (m_Swing1Limit);
	InitSoftJointLimit (m_Swing2Limit);
	
	m_LowTwistLimit.limit = -20;
	m_HighTwistLimit.limit = 70;

	m_Swing1Limit.limit = 40;
	m_Swing2Limit.limit = 0;
}

void CharacterJoint::CheckConsistency ()
{
	Super::CheckConsistency();
	m_LowTwistLimit.limit = clamp<float> (m_LowTwistLimit.limit, -180, 180);
	m_HighTwistLimit.limit = clamp<float> (m_HighTwistLimit.limit, -180, 180);
	m_Swing1Limit.limit = clamp<float> (m_Swing1Limit.limit, 0, 180);
	m_Swing2Limit.limit = clamp<float> (m_Swing2Limit.limit, 0, 180);
}

void CharacterJoint::UpdateTargetRotation ()
{
	NxD6Joint* joint = GET_JOINT ();
	if (joint)
	{
		
		Quaternionf temp = m_ConfigurationSpace * Inverse(m_TargetRotation) * Inverse(m_ConfigurationSpace);
		NxQuat targetRotation = (const NxQuat&)temp;
//		NxActor *a0, *a1;
//		joint->getActors(&a0, &a1);
//		if (a1)
//			targetRotation = a1->getGlobalOrientationQuat () * targetRotation;
//NxQuat id;
//id.id();
		joint->setDriveOrientation(targetRotation);
	}
}

Quaternionf CharacterJoint::GetTargetRotation ()
{
	return m_TargetRotation;
}

void CharacterJoint::SetTargetRotation (const Quaternionf& rot)
{
	SetDirty ();
	if (m_Joint)
	{
		NxActor* a0, *a1;
		m_Joint->getActors(&a0, &a1);
/*		if (a1)
		{
			Quaternionf q = (const Quaternionf&)a1->getGlobalOrientationQuat();
			m_TargetRotation = NormalizeSafe(Inverse(q) * rot);
		}
		else*/
			m_TargetRotation = NormalizeSafe (rot);	
	}

	UpdateTargetRotation();
}

Vector3f CharacterJoint::GetTargetAngularVelocity ()
{
	return m_TargetAngularVelocity;
}

void CharacterJoint::SetTargetAngularVelocity (const Vector3f& rot)
{
	SetDirty ();
	m_TargetAngularVelocity = rot;
	if (m_Joint)
		GET_JOINT ()->setDriveAngularVelocity ((const NxVec3&)m_TargetAngularVelocity);
}

JointDrive CharacterJoint::GetRotationDrive ()
{
	return m_RotationDrive;
}

void CharacterJoint::SetRotationDrive (const JointDrive& drive)
{
	SetDirty ();
	m_RotationDrive = drive;
	if (GET_JOINT())
	{
		NxD6JointDesc desc;
		GET_JOINT()->saveToDesc (desc);
		FixupNovodexLimitBug(desc);
	
		int flag = 0;
		if (m_UseTargetRotation)
			flag |= NX_D6JOINT_DRIVE_POSITION;

		ConvertDrive (m_RotationDrive, desc.slerpDrive, flag);
		ConvertDrive (m_RotationDrive, desc.twistDrive, flag);
		ConvertDrive (m_RotationDrive, desc.swingDrive, flag);
		GET_JOINT()->loadFromDesc (desc);
	}
}

void CharacterJoint::SetLowTwistLimit (const SoftJointLimit& limit)
{
	SetDirty ();
	m_LowTwistLimit = limit;
	if (GET_JOINT())
	{
		NxD6JointDesc desc;
		GET_JOINT()->saveToDesc (desc);
		FixupNovodexLimitBug(desc);

		ConvertSoftLimit (m_LowTwistLimit, desc.twistLimit.low);
		ConvertSoftLimit (m_HighTwistLimit, desc.twistLimit.high);
		if (desc.twistLimit.low.value > desc.twistLimit.high.value)
			swap (desc.twistLimit.low, desc.twistLimit.high);
		GET_JOINT()->loadFromDesc (desc);
	}
}

void CharacterJoint::SetHighTwistLimit (const SoftJointLimit& limit)
{
	SetDirty ();
	m_HighTwistLimit = limit;
	if (GET_JOINT())
	{
		NxD6JointDesc desc;
		GET_JOINT()->saveToDesc (desc);
		FixupNovodexLimitBug(desc);


		ConvertSoftLimit (m_LowTwistLimit, desc.twistLimit.low);
		ConvertSoftLimit (m_HighTwistLimit, desc.twistLimit.high);
		if (desc.twistLimit.low.value > desc.twistLimit.high.value)
			swap (desc.twistLimit.low, desc.twistLimit.high);
		GET_JOINT()->loadFromDesc (desc);
	}
}

void CharacterJoint::SetSwing1Limit (const SoftJointLimit& limit)
{
	SetDirty ();
	m_Swing1Limit = limit;
	if (GET_JOINT())
	{
		NxD6JointDesc desc;
		GET_JOINT()->saveToDesc (desc);
		FixupNovodexLimitBug(desc);

		ConvertSoftLimit (m_Swing1Limit, desc.swing1Limit);
		GET_JOINT()->loadFromDesc (desc);
	}
}

void CharacterJoint::SetSwing2Limit (const SoftJointLimit& limit)
{
	SetDirty ();
	m_Swing2Limit = limit;
	if (GET_JOINT())
	{
		NxD6JointDesc desc;
		GET_JOINT()->saveToDesc (desc);
		FixupNovodexLimitBug(desc);
		
		ConvertSoftLimit (m_Swing2Limit, desc.swing2Limit);
		GET_JOINT()->loadFromDesc (desc);
	}
}

/*
- When the rigid body which is attached with the hinge joint is activated after the joint is loaded
  It will not be connected with the joint!
*/

template<class TransferFunction>
void CharacterJoint::Transfer (TransferFunction& transfer)
{
	JointTransferPre (transfer);
	TRANSFER (m_SwingAxis);
//	TRANSFER (m_UseTargetRotation);
//	TRANSFER (m_RotationDrive);
	TRANSFER (m_LowTwistLimit);
	TRANSFER (m_HighTwistLimit);
	TRANSFER (m_Swing1Limit);
	TRANSFER (m_Swing2Limit);
	
	JointTransferPost (transfer);
}

void CharacterJoint::Create ()
{
	AssertIf (!IsActive ());

	NxD6JointDesc desc;
	
	if (m_Joint && m_Joint->getState () == NX_JS_SIMULATING)
		GET_JOINT()->saveToDesc (desc);

	desc.xMotion = NX_D6JOINT_MOTION_LOCKED;
	desc.yMotion = NX_D6JOINT_MOTION_LOCKED;
	desc.zMotion = NX_D6JOINT_MOTION_LOCKED;
	
	ConvertSoftLimit (m_LowTwistLimit, desc.twistLimit.low);
	ConvertSoftLimit (m_HighTwistLimit, desc.twistLimit.high);
	
	if (desc.twistLimit.low.value > desc.twistLimit.high.value)
		swap (desc.twistLimit.low, desc.twistLimit.high);
	
	ConvertSoftLimit (m_Swing1Limit, desc.swing1Limit);
	ConvertSoftLimit (m_Swing2Limit, desc.swing2Limit);
	
	desc.swing1Motion = NX_D6JOINT_MOTION_LIMITED;
	desc.swing2Motion = NX_D6JOINT_MOTION_LIMITED;
	desc.twistMotion = NX_D6JOINT_MOTION_LIMITED;
	
//	desc.driveOrientation = (const NxQuat&)m_TargetRotation;
	desc.driveAngularVelocity = (const NxVec3&)m_TargetAngularVelocity;
	
	desc.flags = NX_D6JOINT_SLERP_DRIVE;
	
	int flag = 0;
	if (m_UseTargetRotation)
		flag |= NX_D6JOINT_DRIVE_POSITION;
	
	ConvertDrive (m_RotationDrive, desc.slerpDrive, flag);
	ConvertDrive (m_RotationDrive, desc.twistDrive, flag);
	ConvertDrive (m_RotationDrive, desc.swingDrive, flag);

	FINALIZE_CREATE (desc, NxD6Joint);
	
	///////// DO WE WANT THIS?????????
	SetTargetRotation (GetComponent(Transform).GetRotation());
}

void CharacterJoint::SetSwingAxis (const Vector3f& axis)\
{
	SetDirty ();
	m_SwingAxis = axis;
	ApplySetupAxesToDesc (kChangeAxis);
}

void CharacterJoint::ApplySetupAxesToDesc (int option)
{
	if (IsActive () && m_Joint)
	{
		NxD6JointDesc desc;
		AssertIf (m_Joint->getState () == NX_JS_BROKEN);
		GET_JOINT()->saveToDesc (desc);
		FixupNovodexLimitBug(desc);
		
		SetupAxes (desc, option);
		GET_JOINT()->loadFromDesc (desc);
		AssertIf (m_Joint->getState () == NX_JS_BROKEN);
	}
}

}


IMPLEMENT_CLASS (CharacterJoint)
IMPLEMENT_OBJECT_SERIALIZE (CharacterJoint)
#endif //ENABLE_PHYSICS
