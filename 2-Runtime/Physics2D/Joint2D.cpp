#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS || DOXYGEN
#include "Runtime/Physics2D/Joint2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"
#include "Runtime/Physics2D/Physics2DManager.h"

#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/LogAssert.h"
#include "External/Box2D/Box2D/Box2D.h"


IMPLEMENT_CLASS (Joint2D)
IMPLEMENT_OBJECT_SERIALIZE (Joint2D)
INSTANTIATE_TEMPLATE_TRANSFER (Joint2D)

// --------------------------------------------------------------------------


Joint2D::Joint2D (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_Joint(NULL)
{
}


Joint2D::~Joint2D ()
{
	Cleanup ();
}


template<class TransferFunction>
void Joint2D::Transfer (TransferFunction& transfer) 
{
	Super::Transfer(transfer);

	TRANSFER (m_CollideConnected);
	transfer.Align();
	TRANSFER (m_ConnectedRigidBody);
}


void Joint2D::Reset ()
{
	Super::Reset ();

	Cleanup ();

	m_ConnectedRigidBody = NULL;
	m_CollideConnected = false;
}	
	

void Joint2D::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	// Recreate joint if appropriate.
	// Most of Box2D joint properties are immutable once created
	// thus causing us to regenerate the joint if a property changes in the editor.
	if ((awakeMode == kDefaultAwakeFromLoad))
		ReCreate ();
}


void Joint2D::Deactivate (DeactivateOperation operation)
{
	Cleanup ();
	Super::Deactivate (operation);
}


void Joint2D::AddToManager ()
{
	// Create the joint.
	ReCreate ();
}


void Joint2D::RemoveFromManager ()
{
	// Destroy the joint.
	Cleanup ();
}


void Joint2D::RecreateJoint (const Rigidbody2D* ignoreRigidbody)
{
	if (IsActive () && GetEnabled())
		ReCreate ();
	else
		Cleanup ();
}


void Joint2D::SetConnectedBody (PPtr<Rigidbody2D> rigidBody)
{
	m_ConnectedRigidBody = rigidBody;
	SetDirty ();

	ReCreate ();
}


void Joint2D::SetCollideConnected (bool collide)
{
	m_CollideConnected = collide;
	SetDirty ();

	ReCreate ();
}


// --------------------------------------------------------------------------


void Joint2D::ReCreate()
{
	Cleanup();

	if (IsActive () && GetEnabled ())
		Create();
}


void Joint2D::Cleanup ()
{
	// Finish if no joint to clean-up.
	if (!m_Joint)
		return;

	// Destroy the joint.
	GetPhysics2DWorld ()->DestroyJoint (m_Joint);
	m_Joint = NULL;
}


b2Body* Joint2D::FetchBodyA () const
{
	// Find the rigid-body A.
	Rigidbody2D* rigidBodyA = QueryComponent(Rigidbody2D);
	Assert (rigidBodyA != NULL);

	// Ensure the rigid-body (body) is available.
	if ( rigidBodyA )
		rigidBodyA->Create();

	// Fetch the body A.
	return rigidBodyA->GetBody();
}


b2Body* Joint2D::FetchBodyB () const
{
	// Find the appropriate rigid body B.
	Rigidbody2D* rigidBodyB = m_ConnectedRigidBody;

	// Ensure the rigid-body (body) is available.
	if ( rigidBodyB )
		rigidBodyB->Create();

	// Fetch the appropriate body B.
	return rigidBodyB != NULL ? rigidBodyB->GetBody() : GetPhysicsGroundBody();
}


void Joint2D::FinalizeCreateJoint (b2JointDef* jointDef)
{
	Assert (jointDef != NULL);

	if (!IsActive ())
		return;

	// Fetch the appropriate body A.
	b2Body* bodyA = FetchBodyA();

	// Fetch the appropriate body B.
	b2Body* bodyB = FetchBodyB();

	// Finish if the same body is being used.
	if ( bodyA == bodyB )
	{
		WarningStringObject(Format("Cannot create 2D joint on '%s' as it connects to itself.\n",	
			 GetGameObjectPtr()->GetName()), this);
		return;
	}

	// Populate the basic joint definition information.
	jointDef->bodyA = bodyA;
	jointDef->bodyB = bodyB;
	jointDef->collideConnected = m_CollideConnected;
	jointDef->userData = this;

	// Create the joint.
	m_Joint = GetPhysics2DWorld ()->CreateJoint (jointDef);
}


#endif //ENABLE_2D_PHYSICS
