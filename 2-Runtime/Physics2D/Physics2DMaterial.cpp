#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS
#include "Runtime/Physics2D/Physics2DMaterial.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

IMPLEMENT_CLASS (PhysicsMaterial2D)
IMPLEMENT_OBJECT_SERIALIZE (PhysicsMaterial2D)


// --------------------------------------------------------------------------


PhysicsMaterial2D::PhysicsMaterial2D (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
}


PhysicsMaterial2D::~PhysicsMaterial2D ()
{
}


void PhysicsMaterial2D::Reset ()
{
	Super::Reset();
	m_Friction = 0.4f;
	m_Bounciness = 0.0f;
}


void PhysicsMaterial2D::CheckConsistency ()
{
	Super::CheckConsistency ();

	m_Friction = clamp(m_Friction, 0.0f, 100000.0f);
	m_Bounciness = clamp(m_Bounciness, 0.0f, 1.0f);
}


template<class TransferFunction>
void PhysicsMaterial2D::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);

	transfer.Transfer (m_Friction, "friction", kSimpleEditorMask);
	transfer.Transfer (m_Bounciness, "bounciness", kSimpleEditorMask);
}


void PhysicsMaterial2D::SetFriction (float friction)
{
	m_Friction = clamp (friction, 0.0f, 100000.0f);
	SetDirty ();
}


void PhysicsMaterial2D::SetBounciness (float bounce)
{
	m_Bounciness = clamp (bounce, 0.0f, 1.0f);
	SetDirty ();
}

#endif //ENABLE_2D_PHYSICS
