#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS
#include "Runtime/Physics2D/Physics2DSettings.h"
#include "Runtime/Physics2D/Physics2DManager.h"

#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

#include "External/Box2D/Box2D/Box2D.h"


// --------------------------------------------------------------------------


Physics2DSettings::Physics2DSettings (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_VelocityIterations = 8;
	m_PositionIterations = 3;
	m_Gravity = Vector2f (0, -9.81f);
	m_RaycastsHitTriggers = true;
	m_LayerCollisionMatrix.resize_initialized (kNumLayers, 0xffffffff);
}


Physics2DSettings::~Physics2DSettings ()
{
}


void Physics2DSettings::InitializeClass ()
{
	InitializePhysics2DManager ();
}


void Physics2DSettings::CleanupClass ()
{
	CleanupPhysics2DManager ();
}


void Physics2DSettings::Reset ()
{
	Super::Reset();
	m_VelocityIterations = 8;
	m_PositionIterations = 3;
	m_Gravity = Vector2f (0, -9.81F);
	m_LayerCollisionMatrix.resize_initialized (kNumLayers, 0xffffffff);
}


void Physics2DSettings::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);	
	SetGravity (m_Gravity);
}


void Physics2DSettings::CheckConsistency ()
{
	Super::CheckConsistency ();

	m_VelocityIterations = std::max (1, m_VelocityIterations);
	m_PositionIterations = std::max (1, m_PositionIterations);
}


template<class TransferFunction>
void Physics2DSettings::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	TRANSFER (m_Gravity);
	TRANSFER (m_DefaultMaterial);
	TRANSFER (m_VelocityIterations);
	TRANSFER (m_PositionIterations);
	TRANSFER (m_RaycastsHitTriggers);
	transfer.Align ();
	transfer.Transfer (m_LayerCollisionMatrix, "m_LayerCollisionMatrix", kHideInEditorMask);
}


void Physics2DSettings::SetGravity (const Vector2f& value)
{
	m_Gravity = value;
	SetDirty ();

	GetPhysics2DWorld()->SetGravity(b2Vec2(m_Gravity.x, m_Gravity.y));

	if (m_Gravity == Vector2f::zero)
		return;

	// Wake all dynamic bodies that have non-zero gravity-scale.
	for (b2Body* body = GetPhysics2DWorld()->GetBodyList (); body != NULL; body = body->GetNext ())
	{
		if (body->GetType () == b2_dynamicBody && body->GetGravityScale () != 0.0f)
			body->SetAwake (true);
	}
}


void Physics2DSettings::SetVelocityIterations (const int velocityIterations)
{
	if ( velocityIterations == m_VelocityIterations )
		return;

	m_VelocityIterations = std::max (1,velocityIterations);
	SetDirty ();
}


void Physics2DSettings::SetPositionIterations (const int positionIterations)
{
	if ( positionIterations == m_PositionIterations )
		return;

	m_PositionIterations = std::max (1,positionIterations);
	SetDirty ();
}


void Physics2DSettings::IgnoreCollision(int layer1, int layer2, bool ignore)
{
	if (layer1 >= kNumLayers || layer2 >= kNumLayers)
	{
		ErrorString(Format("layer numbers must be between 0 and %d", kNumLayers));
		return;
	}

	Assert (kNumLayers <= m_LayerCollisionMatrix.size());
	Assert (kNumLayers <= sizeof(m_LayerCollisionMatrix[0])*8);

	if (ignore)
	{
		m_LayerCollisionMatrix[layer1] &= ~(1<<layer2);
		m_LayerCollisionMatrix[layer2] &= ~(1<<layer1);
	}
	else
	{
		m_LayerCollisionMatrix[layer1] |= 1<<layer2;
		m_LayerCollisionMatrix[layer2] |= 1<<layer1;
	}
	SetDirty();
}


bool Physics2DSettings::GetIgnoreCollision(int layer1, int layer2) const
{
	if (layer1 >= kNumLayers || layer2 >= kNumLayers)
	{
		ErrorString(Format("layer numbers must be between 0 and %d", kNumLayers));
		return false;
	}

	bool collides = m_LayerCollisionMatrix[layer1] & (1<<layer2);
	return !collides;
}


GET_MANAGER (Physics2DSettings)
GET_MANAGER_PTR (Physics2DSettings)
IMPLEMENT_CLASS_HAS_INIT (Physics2DSettings)
IMPLEMENT_OBJECT_SERIALIZE (Physics2DSettings)

#endif // #if ENABLE_2D_PHYSICS
