#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "SpringJoint.h"
#include "PhysicsManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"

using namespace std;

namespace Unity
{

#define GET_JOINT() static_cast<NxDistanceJoint*> (m_Joint)

/*
- We awake the hingejoint only once. (AwakeFromLoad)
  At this point we setup the axes. They are never changed afterwards
  -> The perfect solution remembers the old position/rotation of the rigid bodies.
      Then when changing axis/anchor is changed it generates axes that are rleative to the old position/rotation state!
*/

SpringJoint::SpringJoint (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

SpringJoint::~SpringJoint ()
{
}

void SpringJoint::Reset()
{
	Super::Reset();
	m_MinDistance = 0.0F;
	m_MaxDistance = 0.5F;
	m_Spring = 2;
	m_Damper = 0.2f;
}

void SpringJoint::Create ()
{
	AssertIf (!IsActive ());

	NxDistanceJointDesc desc;

	if (m_Joint && m_Joint->getState () == NX_JS_SIMULATING)
		GET_JOINT()->saveToDesc (desc);

	desc.spring.spring = m_Spring;
	desc.spring.damper = m_Damper;
	
	if (desc.minDistance < m_MaxDistance)
	{
		desc.minDistance = m_MinDistance;
		desc.maxDistance = m_MaxDistance;
	}
	else
	{
		desc.minDistance = m_MaxDistance;
		desc.maxDistance = m_MinDistance;
	}
		
	desc.flags |= NX_DJF_SPRING_ENABLED | NX_DJF_MAX_DISTANCE_ENABLED | NX_DJF_MIN_DISTANCE_ENABLED;
	
	FINALIZE_CREATE (desc, NxDistanceJointDesc);
}
void SpringJoint::SetSpring (float spring)
{
	m_Spring = spring;
	SetDirty();

	if (GET_JOINT() != NULL)
	{
		NxDistanceJointDesc desc;
		GET_JOINT()->saveToDesc (desc);
		desc.spring.spring = m_Spring;
		GET_JOINT()->loadFromDesc (desc);
	}
}

void SpringJoint::SetDamper(float damper)
{
	m_Damper = damper;
	SetDirty();
	
	if (GET_JOINT() != NULL)
	{
		NxDistanceJointDesc desc;
		GET_JOINT()->saveToDesc (desc);
		desc.spring.damper = m_Damper;
		GET_JOINT()->loadFromDesc (desc);
	}
}

void SpringJoint::SetMinDistance(float distance)
{
	m_MinDistance = distance;
	SetDirty();

	if (GET_JOINT() != NULL)
	{
		NxDistanceJointDesc desc;
		GET_JOINT()->saveToDesc (desc);
		if (desc.minDistance < m_MaxDistance)
		{
			desc.minDistance = m_MinDistance;
			desc.maxDistance = m_MaxDistance;
		}
		else
		{
			desc.minDistance = m_MaxDistance;
			desc.maxDistance = m_MinDistance;
		}
		GET_JOINT()->loadFromDesc (desc);
	}
}

void SpringJoint::SetMaxDistance(float distance)
{
	m_MaxDistance = distance;
	SetDirty();

	if (GET_JOINT() != NULL)
	{
		NxDistanceJointDesc desc;
		GET_JOINT()->saveToDesc (desc);
		if (desc.minDistance < m_MaxDistance)
		{
			desc.minDistance = m_MinDistance;
			desc.maxDistance = m_MaxDistance;
		}
		else
		{
			desc.minDistance = m_MaxDistance;
			desc.maxDistance = m_MinDistance;
		}
		GET_JOINT()->loadFromDesc (desc);
	}
}

IMPLEMENT_AXIS_ANCHOR(SpringJoint,NxDistanceJointDesc)

template<class TransferFunction>
void SpringJoint::Transfer (TransferFunction& transfer)
{
	JointTransferPreNoAxis(transfer);
	TRANSFER_SIMPLE (m_Spring);
	TRANSFER_SIMPLE (m_Damper);
	TRANSFER_SIMPLE (m_MinDistance);
	TRANSFER_SIMPLE (m_MaxDistance);
	JointTransferPost (transfer);
}

#undef GET_JOINT

}

IMPLEMENT_CLASS (SpringJoint)
IMPLEMENT_OBJECT_SERIALIZE (SpringJoint)

#endif //ENABLE_PHYSICS