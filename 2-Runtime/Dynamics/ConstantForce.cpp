#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "ConstantForce.h"
#include "RigidBody.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

using namespace Unity;

IMPLEMENT_OBJECT_SERIALIZE (ConstantForce)
IMPLEMENT_CLASS (ConstantForce)

ConstantForce::ConstantForce (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_FixedUpdateNode (this)
{
}

ConstantForce::~ConstantForce ()
{
}

template<class TransferFunction>
void ConstantForce::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	TRANSFER (m_Force);
	TRANSFER (m_RelativeForce);
	TRANSFER (m_Torque);
	TRANSFER (m_RelativeTorque);
}

void ConstantForce::Reset ()
{
	Super::Reset ();
	m_Force = Vector3f::zero;
	m_RelativeForce = Vector3f::zero;
	m_Torque = Vector3f::zero;
	m_RelativeTorque = Vector3f::zero;
}

void ConstantForce::FixedUpdate() {
	Rigidbody* body = QueryComponent (Rigidbody);
	if (!body)
	{
		// It should be impossible to reach this case, since Unity will not allow
		// deleting components which are required by other components. But people
		// have somehow managed (case 506613), so better check for it.
		ErrorStringObject ("ConstantForce requires a Rigidbody component, but non is present.", this);
		return;
	}
	body->AddForce (m_Force);
	body->AddRelativeForce (m_RelativeForce);
	body->AddTorque (m_Torque);
	body->AddRelativeTorque (m_RelativeTorque);
}

void ConstantForce::AddToManager ()
{
	GetFixedBehaviourManager ().AddBehaviour (m_FixedUpdateNode, -1);
}

void ConstantForce::RemoveFromManager ()
{
	GetFixedBehaviourManager ().RemoveBehaviour (m_FixedUpdateNode);
}
#endif //ENABLE_PHSYICS