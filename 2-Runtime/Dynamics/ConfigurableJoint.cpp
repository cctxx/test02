#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "ConfigurableJoint.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "PhysicsManager.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Misc/BuildSettings.h"
#include "NxWrapperUtility.h"

#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"

using namespace std;

namespace Unity
{

#define GET_JOINT() static_cast<NxD6Joint*> (m_Joint)


/*
- We awake the hingejoint only once. (AwakeFromLoad)
  At this point we setup the axes. They are never changed afterwards
  -> The perfect solution remembers the old position/rotation of the rigid bodies.
      Then when changing axis/anchor is changed it generates axes that are rleative to the old position/rotation state!
*/

void ConfigurableJoint::InitializeClass ()
{
	#if UNITY_EDITOR
	// Only introduced during 2.0 alpha
	RegisterAllowNameConversion (ConfigurableJoint::GetClassStringStatic(), "m_ConfigureInWorldSpace", "m_ConfiguredInWorldSpace");
	#endif
}



ConfigurableJoint::ConfigurableJoint (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

ConfigurableJoint::~ConfigurableJoint ()
{
}

void ConfigurableJoint::CalculateGlobalHingeSpace (Vector3f& globalAnchor, Vector3f& globalAxis, Vector3f& globalNormal) const
{	
	const Transform& transform = GetComponent (Transform);

	Vector3f localAxis = m_Axis;
	if (SqrMagnitude (localAxis) < Vector3f::epsilon)
		localAxis = Vector3f (1.0F, 0.0F, 0.0F);
	Vector3f localNormal = m_SecondaryAxis;
	
	OrthoNormalize (&localAxis, &localNormal);	
	
	globalAnchor = transform.TransformPoint (m_Anchor);
	//Vector3f globalRigidbodyPos = transform.GetPosition ();
	
	if (m_ConfiguredInWorldSpace)
	{
		globalAxis = localAxis;
		globalNormal = localNormal;
	}
	else
	{
		globalAxis = transform.TransformDirection (localAxis);
		globalNormal = transform.TransformDirection (localNormal);
	}
//	Matrix3x3f m;
//	m.SetOrthoNormalBasisInverse(globalAxis, globalNormal, Cross (globalAxis, globalNormal));
//	m.SetOrthoNormalBasisInverse(Vector3f::xAxis, globalNormal, Cross (globalAxis, globalNormal));
//	m.SetOrthoNormalBasisInverse(globalNormal, Cross (globalAxis, globalNormal), globalAxis);
//	OrthoNormalize(m);
//	Quaternionf q;
//	MatrixToQuaternion(m, q);
//	m_ConfigurationSpace = q * Inverse(transform.GetRotation ());
}

void ConfigurableJoint::Reset ()
{
	Super::Reset();
	
	m_XMotion = NX_D6JOINT_MOTION_FREE;
	m_YMotion = NX_D6JOINT_MOTION_FREE;
	m_ZMotion = NX_D6JOINT_MOTION_FREE;

	m_AngularXMotion = NX_D6JOINT_MOTION_FREE;
	m_AngularYMotion = NX_D6JOINT_MOTION_FREE;
	m_AngularZMotion = NX_D6JOINT_MOTION_FREE;
	
	InitSoftJointLimit(m_LinearLimit);	
	InitSoftJointLimit(m_LowAngularXLimit);	
	InitSoftJointLimit(m_HighAngularXLimit);	
	InitSoftJointLimit(m_AngularYLimit);	
	InitSoftJointLimit(m_AngularZLimit);	
	
	InitJointDrive(m_XDrive);	
	InitJointDrive(m_YDrive);	
	InitJointDrive(m_ZDrive);	
	InitJointDrive(m_AngularXDrive);	
	InitJointDrive(m_AngularYZDrive);	
	InitJointDrive(m_SlerpDrive);	
	
	m_ProjectionMode = 0;
	m_ProjectionDistance = 0.1F;
	m_ProjectionAngle = 5.0F;
//	m_GearRatio =  1.0F;
	m_RotationDriveMode = 0;
//	m_UseGear = false;
	m_TargetAngularVelocity = m_TargetVelocity = m_TargetPosition =  Vector3f(0.0F, 0.0F, 0.0F);
	m_TargetRotation = Quaternionf::identity();
	m_SecondaryAxis = Vector3f::yAxis;
	m_ConfiguredInWorldSpace = false;
	m_SwapBodies = false;
}

void ConfigurableJoint::CheckConsistency ()
{
	Super::CheckConsistency();
}

void ConfigurableJoint::SetXMotion (int motion)
{
	m_XMotion = motion;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetYMotion (int motion)
{
	m_YMotion = motion;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetAngularXMotion (int motion)
{
	m_AngularXMotion = motion;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetAngularYMotion (int motion)
{
	m_AngularYMotion = motion;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetAngularZMotion (int motion)
{
	m_AngularZMotion = motion;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetZMotion (int motion)
{
	m_ZMotion = motion;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetLinearLimit (const SoftJointLimit& limit)
{
	m_LinearLimit = limit;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetAngularZLimit (const SoftJointLimit& limit)
{
	m_AngularZLimit = limit;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetAngularYLimit (const SoftJointLimit& limit)
{
	m_AngularYLimit = limit;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetLowAngularXLimit (const SoftJointLimit& limit)
{
	m_LowAngularXLimit = limit;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetHighAngularXLimit (const SoftJointLimit& limit)
{
	m_HighAngularXLimit = limit;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetXDrive (const JointDrive& drive)
{
	m_XDrive = drive;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetYDrive (const JointDrive& drive)
{
	m_YDrive = drive;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetZDrive (const JointDrive& drive)
{
	m_ZDrive = drive;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetAngularXDrive (const JointDrive& drive)
{
	m_AngularXDrive = drive;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetAngularYZDrive (const JointDrive& drive)
{
	m_AngularYZDrive = drive;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetSlerpDrive (const JointDrive& drive)
{
	m_SlerpDrive = drive;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetRotationDriveMode (int mode)
{
	m_RotationDriveMode = mode;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetProjectionMode (int mode)
{
	m_ProjectionMode = mode;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetProjectionDistance (float dist)
{
	m_ProjectionDistance = dist;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetProjectionAngle (float angle)
{
	m_ProjectionAngle = angle;
	ApplyKeepConfigurationSpace();
}

void ConfigurableJoint::SetConfiguredInWorldSpace (bool c)
{
	m_ConfiguredInWorldSpace = c;
	ApplyRebuildConfigurationSpace();
}

void ConfigurableJoint::SetSwapBodies (bool c)
{
	m_SwapBodies = c;
	ApplyRebuildConfigurationSpace();
}

void ConfigurableJoint::SetTargetPosition (const Vector3f& pos)
{
	m_TargetPosition = pos;
	if (m_Joint)
		GET_JOINT()->setDrivePosition(Vec3ToNx(pos));
}
	
void ConfigurableJoint::SetTargetRotation (const Quaternionf& rotation)
{
	m_TargetRotation = rotation;
	if (m_Joint)
		GET_JOINT()->setDriveOrientation((const NxQuat&)rotation);
}

void ConfigurableJoint::SetTargetVelocity (const Vector3f& vel)
{
	m_TargetVelocity = vel;
	if (m_Joint)
		GET_JOINT()->setDriveLinearVelocity(Vec3ToNx(vel));
}
	
void ConfigurableJoint::SetTargetAngularVelocity (const Vector3f& angular)
{
	m_TargetAngularVelocity = angular;
	if (m_Joint)
		GET_JOINT()->setDriveAngularVelocity(Vec3ToNx(angular));
}

/*
- When the rigid body which is attached with the hinge joint is activated after the joint is loaded
  It will not be connected with the joint!
*/

template<class TransferFunction>
void ConfigurableJoint::Transfer (TransferFunction& transfer)
{
	JointTransferPre (transfer);
	TRANSFER (m_SecondaryAxis);
	
	TRANSFER(m_XMotion);
	TRANSFER(m_YMotion);
	TRANSFER(m_ZMotion);
	TRANSFER(m_AngularXMotion);
	TRANSFER(m_AngularYMotion);
	TRANSFER(m_AngularZMotion);

	TRANSFER(m_LinearLimit);
	TRANSFER(m_LowAngularXLimit);
	TRANSFER(m_HighAngularXLimit);
	TRANSFER(m_AngularYLimit);
	TRANSFER(m_AngularZLimit);

	TRANSFER(m_TargetPosition);
	TRANSFER(m_TargetVelocity);

	TRANSFER(m_XDrive);
	TRANSFER(m_YDrive);
	TRANSFER(m_ZDrive);

	TRANSFER(m_TargetRotation);
	TRANSFER(m_TargetAngularVelocity);

	TRANSFER(m_RotationDriveMode);

	TRANSFER(m_AngularXDrive);
	TRANSFER(m_AngularYZDrive);
	TRANSFER(m_SlerpDrive);

	TRANSFER(m_ProjectionMode);
	TRANSFER(m_ProjectionDistance);
	TRANSFER(m_ProjectionAngle);

	TRANSFER(m_ConfiguredInWorldSpace);
	TRANSFER(m_SwapBodies);
	transfer.Align();

	JointTransferPost (transfer);
}

void ConfigurableJoint::ApplyKeepConfigurationSpace()
{
	SetDirty();
	if (m_Joint)
	{
		// Joint has NX_JS_UNBOUND state, when it's created, but physics aren't simulated yet.
		// For ex., if you create ConfigurableJoint from a script, right after creation it will have such state.
		if (m_Joint->getState () == NX_JS_SIMULATING || m_Joint->getState () == NX_JS_UNBOUND)
		{
			NxD6JointDesc desc;
			GET_JOINT()->saveToDesc (desc);
			SetupD6Desc(desc);
			GET_JOINT()->loadFromDesc (desc);
		}
	}
}

void ConfigurableJoint::ApplyRebuildConfigurationSpace()
{
	SetDirty();
	if (m_Joint)
	{
		GetDynamicsScene ().releaseJoint (*m_Joint);
		m_Joint = NULL;
	}
	Create();
}


void ConfigurableJoint::SetupD6Desc (NxD6JointDesc& desc)
{
	desc.xMotion = (NxD6JointMotion)m_XMotion;
	desc.yMotion = (NxD6JointMotion)m_YMotion;
	desc.zMotion = (NxD6JointMotion)m_ZMotion;

	desc.swing1Motion = (NxD6JointMotion)m_AngularYMotion;
	desc.swing2Motion = (NxD6JointMotion)m_AngularZMotion;
	desc.twistMotion = (NxD6JointMotion)m_AngularXMotion;
	
	ConvertSoftLimitLinear(m_LinearLimit, desc.linearLimit);
	ConvertSoftLimit(m_AngularYLimit, desc.swing1Limit);
	ConvertSoftLimit(m_AngularZLimit, desc.swing2Limit);
	ConvertSoftLimit(m_LowAngularXLimit, desc.twistLimit.low);
	ConvertSoftLimit(m_HighAngularXLimit, desc.twistLimit.high);
	
	if (desc.twistLimit.low.value > desc.twistLimit.high.value)
		swap (desc.twistLimit.low, desc.twistLimit.high);
	
	ConvertDrive(m_XDrive, desc.xDrive);
	ConvertDrive(m_YDrive, desc.yDrive);
	ConvertDrive(m_ZDrive, desc.zDrive);
	
	ConvertDrive(m_AngularXDrive, desc.twistDrive);
	ConvertDrive(m_AngularYZDrive, desc.swingDrive);
	ConvertDrive(m_SlerpDrive, desc.slerpDrive);

	desc.projectionMode = (NxJointProjectionMode)m_ProjectionMode;
	desc.projectionDistance = m_ProjectionDistance;
	desc.projectionAngle = Deg2Rad(m_ProjectionAngle);
//	desc.gearRatio = m_GearRatio;
	
	desc.flags = 0;
	if (m_RotationDriveMode)
		desc.flags |= NX_D6JOINT_SLERP_DRIVE;
	
//	if (m_UseGear)
//		desc.flags |= NX_D6JOINT_GEAR_ENABLED;
	
	desc.driveAngularVelocity = Vec3ToNx(m_TargetAngularVelocity);
	desc.driveOrientation = (NxQuat&)m_TargetRotation;
	desc.driveLinearVelocity = Vec3ToNx(m_TargetVelocity);
	desc.drivePosition = Vec3ToNx(m_TargetPosition);
}

void ConfigurableJoint::Create ()
{
	AssertIf (!IsActive ());

	NxD6JointDesc desc;
	
	if (m_Joint && m_Joint->getState () == NX_JS_SIMULATING)
		GET_JOINT()->saveToDesc (desc);
	
	SetupD6Desc (desc);
	FinalizeCreateD6 (desc);
}

void ConfigurableJoint::FinalizeCreateD6 (NxD6JointDesc& desc)
{
	bool swapBodies = m_SwapBodies || (m_ConfiguredInWorldSpace && !IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_4_a1));
	FinalizeCreateImpl (desc, swapBodies);
	if (swapBodies)
	{
		swap(desc.localNormal[0], desc.localNormal[1]);
		swap(desc.localAxis[0], desc.localAxis[1]);
		swap(desc.localAnchor[0], desc.localAnchor[1]);
	}
	
	if (GET_JOINT ())
	{
		GET_JOINT ()->loadFromDesc (desc);
		
		// When actors change, we can't use loadFromDesc on the same joint.
		// Thus recreate the joint
		if (m_Joint && m_Joint->getState () == NX_JS_BROKEN)
		{
			GetDynamicsScene ().releaseJoint (*m_Joint);
			m_Joint = GetDynamicsScene ().createJoint (desc);
		}
	}
	else
		m_Joint = GetDynamicsScene ().createJoint (desc);
}


void ConfigurableJoint::SetSecondaryAxis (const Vector3f& axis)
{
	SetDirty ();
	m_SecondaryAxis = axis;
	if (IsActive () && GET_JOINT())
	{
		NxD6JointDesc desc;
		AssertIf (m_Joint->getState () == NX_JS_BROKEN);
		GET_JOINT()->saveToDesc (desc);
		
		SetupAxes (desc, kChangeAxis);
		GET_JOINT()->loadFromDesc (desc);
		AssertIf (m_Joint->getState () == NX_JS_BROKEN);
	}
}

IMPLEMENT_AXIS_ANCHOR(ConfigurableJoint,NxD6JointDesc)

}

IMPLEMENT_CLASS_HAS_INIT (ConfigurableJoint)
IMPLEMENT_OBJECT_SERIALIZE (ConfigurableJoint)
#endif //ENABLE_PHSYICS
