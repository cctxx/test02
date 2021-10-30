#include "UnityPrefix.h"
#include "Cloth.h"

#if ENABLE_CLOTH

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "PhysicsManager.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "Collider.h"
#include "Runtime/BaseClasses/IsPlaying.h"

namespace Unity
{

InteractiveCloth::InteractiveCloth (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_FixedUpdateNode (this)
{
	// configuration
	m_Friction = 0.5f;
	m_Density = 1.0f;
	m_Pressure = 0.0f;
	m_CollisionResponse = 0.0f;
	m_TearFactor = 0.0f;
	m_AttachmentTearFactor = 0.5f;
	m_AttachmentResponse = 0.2f;
	
	// state
	m_IsTeared = false;
}

InteractiveCloth::~InteractiveCloth ()
{
}

void InteractiveCloth::Reset ()
{
	Super::Reset();

	// configuration
	m_Friction = 0.5f;
	m_Density = 1.0f;
	m_Pressure = 0.0f;
	m_CollisionResponse = 0.0f;
	m_TearFactor = 0.0f;
	m_AttachmentTearFactor = 0.5f;
	m_AttachmentResponse = 0.2f;
}

#if ENABLE_CLOTH

void InteractiveCloth::Create()
{
	Cleanup ();
	
	m_CachedMesh = m_Mesh;

	if (!m_Mesh.IsValid())
		return;
	
	m_IsTeared = false;

	int tearMemoryFactor = (m_TearFactor > 0.0f) ? 2 : 1;
	if (!SetupMeshData (true, false, tearMemoryFactor))
		return;

#if UNITY_EDITOR
	if (IsWorldPlaying())
	{
#endif
	NxClothDesc clothDesc;
	SetupClothDesc (clothDesc, true);
	clothDesc.density = m_Density;
	clothDesc.pressure = m_Pressure;
	clothDesc.friction = m_Friction;
	if (m_TearFactor > 0)
		clothDesc.tearFactor = m_TearFactor+1;

	clothDesc.collisionResponseCoefficient = m_CollisionResponse;
	clothDesc.attachmentTearFactor = m_AttachmentTearFactor+1;
	clothDesc.attachmentResponseCoefficient = m_AttachmentResponse;
	if (m_Pressure > 0)
		clothDesc.flags |= NX_CLF_PRESSURE;
	if (m_CollisionResponse > 0)
		clothDesc.flags |= NX_CLF_COLLISION_TWOWAY;
	if (m_TearFactor > 0)
		clothDesc.flags |= NX_CLF_TEARABLE;
	
	m_ClothScene = &GetDynamicsScene ();
	m_Cloth = m_ClothScene->createCloth(clothDesc);
	
	if (m_Cloth)
	{
		for(std::vector<ClothAttachment>::iterator i = m_AttachedColliders.begin(); i != m_AttachedColliders.end(); i++)
		{
			ClothAttachment& attach = *i;
			AttachToCollider(attach.m_Collider, attach.m_Tearable, attach.m_TwoWayInteraction);
		}
	}
	else
		GetDynamicsSDK().releaseClothMesh(*clothDesc.clothMesh);
#if UNITY_EDITOR
	}
#endif
}

void InteractiveCloth::AddForceAtPosition (const Vector3f& force, const Vector3f& position, float radius, int mode)
{
	AssertFiniteParameter(force)
	AssertFiniteParameter(position)
	AssertIf (m_Cloth == NULL);

	if (!m_IsSuspended)
		m_Cloth->wakeUp();
	m_Cloth->addDirectedForceAtPos ((const NxVec3&)position, (const NxVec3&)force, radius ,(NxForceMode)mode);
}

void InteractiveCloth::AttachToCollider (Collider *collider, bool tearable, bool twoWayInteraction)
{
	AssertIf (m_Cloth == NULL);
	NxShape* shape = collider ? collider->CreateShapeIfNeeded() : NULL;
	if (shape)
	{
		NxU32 attachmentFlags = 0;
		if (twoWayInteraction)
			attachmentFlags |= NX_CLOTH_ATTACHMENT_TWOWAY;
		if (tearable)
			attachmentFlags |= NX_CLOTH_ATTACHMENT_TEARABLE;
		m_Cloth->attachToShape(shape, attachmentFlags);
		m_NeedToWakeUp = true;
	}
}

void InteractiveCloth::DetachFromCollider (Collider *collider)
{
	AssertIf (m_Cloth == NULL);
	if (collider && collider->IsActive())
	{
		m_Cloth->detachFromShape(collider->m_Shape);
		m_NeedToWakeUp = true;
	}
}

void InteractiveCloth::CheckTearing() 
{
	if (m_NumVerticesFromPhysX > m_NumVertices)
	{
		m_IsTeared = true;
		m_Cloth->setFlags(m_Cloth->getFlags() & ~NX_CLF_PRESSURE);
	}
}

void InteractiveCloth::ProcessMeshForRenderer () 
{ 
	CheckTearing(); 
	Super::ProcessMeshForRenderer(); 
}

////@TODO: TEST IF THIS ACTUALLLY CAUSES THE CLOTH PIECE TO EVER FALL ASLEEP???	
void InteractiveCloth::FixedUpdate() 
{
	Super::FixedUpdate ();
	if (m_Cloth)
	{
		CheckTearing();
	}
}

void InteractiveCloth::AddToManager ()
{
	GetFixedBehaviourManager ().AddBehaviour (m_FixedUpdateNode, -1);
}

void InteractiveCloth::RemoveFromManager ()
{
	GetFixedBehaviourManager ().RemoveBehaviour (m_FixedUpdateNode);
}

void InteractiveCloth::PauseSimulation () {
	Super::PauseSimulation ();
	m_Cloth->setFlags(m_Cloth->getFlags() | NX_CLF_DISABLE_COLLISION | NX_CLF_STATIC);	
}

void InteractiveCloth::ResumeSimulation () {
	Super::ResumeSimulation ();
	m_Cloth->setFlags(m_Cloth->getFlags() & ~(NX_CLF_DISABLE_COLLISION | NX_CLF_STATIC));
}

#define ENFORCE_MINEQ(x) {if (value < x) { value = x; ErrorString("value must be greater than or equal to " #x);}}
#define ENFORCE_MIN(x) {if (value <= x) { value = x; ErrorString("value must be greater than " #x);}}
#define ENFORCE_MAXEQ(x) {if (value > x) { value = x; ErrorString("value must be smaller than or equal to " #x);}}
#define ENFORCE_MAX(x) {if (value >= x) { value = x; ErrorString("value must be smaller than " #x);}}

void InteractiveCloth::SetMesh (PPtr<Mesh> value)
{
	if (value != m_CachedMesh)
	{
		m_Mesh = value;
		Create();
		SetDirty();
	}
}

void InteractiveCloth::SetFriction (float value)
{
	ENFORCE_MINEQ(0);
	ENFORCE_MAXEQ(1);

	if (value != m_Friction)
	{
		m_NeedToWakeUp = true;
		m_Friction = value;
	}
	if (m_Cloth)
		m_Cloth->setFriction(value);
	SetDirty();
}

void InteractiveCloth::SetDensity (float value)
{
	ENFORCE_MIN(0);
	
	if (value != m_Density)
	{
		m_NeedToWakeUp = true;
		m_Density = value;
	}
	if (m_Cloth)
	{
		if (m_Density != m_Cloth->getDensity())
			Create();
	}
	SetDirty();
}

void InteractiveCloth::SetPressure (float value)
{
	ENFORCE_MINEQ(0);

	if (value != m_Pressure)
	{
		m_NeedToWakeUp = true;
		m_Pressure = value;
	}
	if (m_Cloth)
	{
		if (value > 0 && !m_IsTeared)
		{
			m_Cloth->setPressure(value);
			m_Cloth->setFlags(m_Cloth->getFlags() | NX_CLF_PRESSURE);			
		}
		else
			m_Cloth->setFlags(m_Cloth->getFlags() & ~(NX_CLF_PRESSURE));			
	}
	SetDirty();
}

void InteractiveCloth::SetCollisionResponse (float value)
{
	ENFORCE_MINEQ(0);

	if (value != m_CollisionResponse)
	{
		m_NeedToWakeUp = true;
		m_CollisionResponse = value;
	}
	if (m_Cloth)
	{
		if (value > 0)
		{
			m_Cloth->setCollisionResponseCoefficient(value);
			m_Cloth->setFlags(m_Cloth->getFlags() | NX_CLF_COLLISION_TWOWAY);			
		}
		else
			m_Cloth->setFlags(m_Cloth->getFlags() & ~(NX_CLF_COLLISION_TWOWAY));			
	}
	SetDirty();
}

void InteractiveCloth::SetTearFactor (float value)
{
	ENFORCE_MINEQ(0);

	if (value != m_TearFactor)
	{
		m_NeedToWakeUp = true;
		m_TearFactor = value;
	}
	if (m_Cloth)
	{
		if ((m_TearFactor>0) != ((m_Cloth->getFlags() & NX_CLF_TEARABLE) != 0))
			Create();
		else if (m_TearFactor>0)
			m_Cloth->setTearFactor(m_TearFactor + 1);
	}
	SetDirty();
}

void InteractiveCloth::SetAttachmentTearFactor (float value)
{
	ENFORCE_MINEQ(0);

	if (value != m_AttachmentTearFactor)
	{
		m_NeedToWakeUp = true;
		m_AttachmentTearFactor = value;
	}
	if (m_Cloth && m_AttachmentTearFactor>0)
		m_Cloth->setAttachmentTearFactor(m_AttachmentTearFactor + 1);
	SetDirty();
}

void InteractiveCloth::SetAttachmentResponse (float value)
{
	ENFORCE_MINEQ(0);
	ENFORCE_MAXEQ(1);
	
	if (value != m_AttachmentResponse)
	{
		m_NeedToWakeUp = true;
		m_AttachmentResponse = value;
	}
	if (m_Cloth)
		m_Cloth->setAttachmentResponseCoefficient(value);
	SetDirty();
}

#else //ENABLE_CLOTH

void InteractiveCloth::Create() {}
void InteractiveCloth::AddForceAtPosition (const Vector3f& force, const Vector3f& position, float radius, int mode) {}
void InteractiveCloth::AttachToCollider (Collider *collider, bool tearable, bool twoWayInteraction) {}
void InteractiveCloth::DetachFromCollider (Collider *collider) {}
void InteractiveCloth::CheckTearing() {}
void InteractiveCloth::ProcessMeshForRenderer () {}
void InteractiveCloth::FixedUpdate() {}
void InteractiveCloth::PauseSimulation () {}
void InteractiveCloth::ResumeSimulation () {}
void InteractiveCloth::SetMesh (PPtr<Mesh> value) {}
void InteractiveCloth::SetFriction (float value) {}
void InteractiveCloth::SetDensity (float value) {}
void InteractiveCloth::SetPressure (float value) {}
void InteractiveCloth::SetCollisionResponse (float value) {}
void InteractiveCloth::SetTearFactor (float value) {}
void InteractiveCloth::SetAttachmentTearFactor (float value) {}
void InteractiveCloth::SetAttachmentResponse (float value) {}
void InteractiveCloth::AddToManager () {}
void InteractiveCloth::RemoveFromManager (){}
#endif //ENABLE_CLOTH

void InteractiveCloth::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
#if ENABLE_CLOTH
	if (m_Cloth)
	{
		// Apply changed values
		SetMesh (m_Mesh);
		SetFriction (m_Friction);
		SetPressure (m_Pressure);
		SetCollisionResponse (m_CollisionResponse);
		SetAttachmentTearFactor (m_AttachmentTearFactor);
		SetAttachmentResponse (m_AttachmentResponse);
		SetTearFactor (m_TearFactor);
		SetDensity (m_Density);
	}
#endif
	
	Super::AwakeFromLoad (awakeMode);
}	
	
template<class TransferFunction>
void InteractiveCloth::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	TRANSFER (m_Mesh);
	TRANSFER (m_Friction);
	TRANSFER (m_Density);
	TRANSFER (m_Pressure);
	TRANSFER (m_CollisionResponse);
	TRANSFER (m_AttachmentTearFactor);
	TRANSFER (m_AttachmentResponse);
	TRANSFER (m_TearFactor);		
	TRANSFER (m_AttachedColliders);
}

template<class TransferFunction> inline
void ClothAttachment::Transfer (TransferFunction& transfer)
{
	TRANSFER(m_Collider);
	TRANSFER(m_TwoWayInteraction);
	TRANSFER(m_Tearable);
}

IMPLEMENT_CLASS (InteractiveCloth)
IMPLEMENT_OBJECT_SERIALIZE (InteractiveCloth)

}

void RegisterClass_InteractiveCloth () { Unity::RegisterClass_InteractiveCloth(); } 

#endif // ENABLE_CLOTH
