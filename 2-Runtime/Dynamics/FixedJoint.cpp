#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "FixedJoint.h"
#include "PhysicsManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
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

FixedJoint::FixedJoint (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

FixedJoint::~FixedJoint ()
{
}

void FixedJoint::Create ()
{
	AssertIf (!IsActive ());

	NxD6JointDesc desc;
	desc.zMotion = desc.yMotion = desc.xMotion = NX_D6JOINT_MOTION_LOCKED;
	desc.swing1Motion = desc.swing2Motion = desc.twistMotion = NX_D6JOINT_MOTION_LOCKED;
	
	if (m_Joint && m_Joint->getState () == NX_JS_SIMULATING)
		GET_JOINT()->saveToDesc (desc);

	FINALIZE_CREATE (desc, NxFixedJointDesc);
}

IMPLEMENT_AXIS_ANCHOR(FixedJoint,NxD6JointDesc)

template<class TransferFunction>
void FixedJoint::Transfer (TransferFunction& transfer)
{
	Super::Super::Transfer (transfer);
	TRANSFER_SIMPLE (m_ConnectedBody);
	JointTransferPost (transfer);
}

}

IMPLEMENT_CLASS (FixedJoint)
IMPLEMENT_OBJECT_SERIALIZE (FixedJoint)

#undef GET_JOINT
#endif //ENABLE_PHYSICS