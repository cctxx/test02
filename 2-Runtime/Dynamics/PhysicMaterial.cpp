#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "PhysicMaterial.h"
#include "PhysicsManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/Misc/ResourceManager.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Misc/BuildSettings.h"

enum MonoBounceCombineMode {
	kMonoAverage = 0,
	kMonoMultiply = 1,
	kMonoMinimum = 2,
	kMonoMaximum = 3,
};

static int MonoCombineModeToNxCombineMode(int monoMode)
{
	if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1)) {
		return monoMode; 
	}

	switch (monoMode) {
		case kMonoAverage: return NX_CM_AVERAGE;
		case kMonoMultiply: return NX_CM_MULTIPLY;
		case kMonoMinimum: return NX_CM_MIN;
		case kMonoMaximum: return NX_CM_MAX;
	}

	return 0;
}

static int NxCombineModeToMonoCombineMode(int nxMode)
{
	if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1)) {
		return nxMode;
	}

	switch (nxMode) {
		case NX_CM_AVERAGE: return kMonoAverage;
		case NX_CM_MULTIPLY: return kMonoMultiply;
		case NX_CM_MIN: return kMonoMinimum;
		case NX_CM_MAX: return kMonoMaximum;
	}

	return 0;
}

PhysicMaterial::PhysicMaterial (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Material = NULL;
	m_MaterialIndex = -1;
}

PhysicMaterial::~PhysicMaterial ()
{
	MainThreadCleanup ();
}

bool PhysicMaterial::MainThreadCleanup ()
{
	if (m_Material)
	{
		GetDynamicsScene ().releaseMaterial (*m_Material);
		m_Material = NULL;
	}
	
	return true;
}


void PhysicMaterial::InitializeClass ()
{
	RegisterAllowNameConversion(PhysicMaterial::GetClassStringStatic(), "bouncyness", "bounciness");
}
void PhysicMaterial::Reset ()
{
	Super::Reset();
	m_FrictionCombine = 0;
	m_BounceCombine = 0;
	m_DynamicFriction = 0.4f;
	m_StaticFriction = 0.4f;
	m_Bounciness = 0;
	m_DynamicFriction2 = 0;
	m_StaticFriction2 = 0;
	m_FrictionDirection2 = Vector3f::zero;
}

bool PhysicMaterial::IsDefaultMaterial ()
{
	return GetPhysicsManager ().m_CachedDefaultMaterial == this;
}

template<class TransferFunction>
void PhysicMaterial::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);

	transfer.Transfer (m_DynamicFriction, "dynamicFriction", kSimpleEditorMask);
	transfer.Transfer (m_StaticFriction, "staticFriction", kSimpleEditorMask);
	transfer.Transfer (m_Bounciness, "bounciness", kSimpleEditorMask);
	
	transfer.Transfer (m_FrictionCombine, "frictionCombine");
	transfer.Transfer (m_BounceCombine, "bounceCombine");
	
	transfer.Transfer (m_FrictionDirection2, "frictionDirection2");
	transfer.Transfer (m_DynamicFriction2, "dynamicFriction2");
	transfer.Transfer (m_StaticFriction2, "staticFriction2");
}

void PhysicMaterial::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	NxMaterialDesc representation;
	representation.dynamicFriction = m_DynamicFriction;
	representation.staticFriction = m_StaticFriction;
	representation.restitution = clamp<float>(m_Bounciness, 0.0f, 1.0f);
	representation.frictionCombineMode = (NxCombineMode)m_FrictionCombine;
	representation.restitutionCombineMode = (NxCombineMode)m_BounceCombine;
	representation.dynamicFrictionV = m_DynamicFriction2;
	representation.staticFrictionV = m_StaticFriction2;

	if (m_Material == NULL)
	{
		m_Material = GetDynamicsScene ().createMaterial (representation);
		m_MaterialIndex = m_Material->getMaterialIndex ();
	}
	else
		m_Material->loadFromDesc (representation);
	
	UpdateFrictionDirection2 ();
	
	if (IsDefaultMaterial ())
		CopyMaterialToDefault ();
}

float PhysicMaterial::GetDynamicFriction () const
{
	return m_DynamicFriction;
}

void PhysicMaterial::SetDynamicFriction (float friction)
{
    if (friction < 0)
    {
        ErrorString( Format("Physics material %s cannot have dynamicFriction = %f", GetName(), friction) );
        friction = 0.0f;
    }
    
	m_DynamicFriction = friction;
	m_Material->setDynamicFriction (friction);
	SetDirty ();
}

float PhysicMaterial::GetStaticFriction () const
{
	return m_StaticFriction;
}

void PhysicMaterial::SetStaticFriction (float friction)
{
    if (friction < 0)
    {
        ErrorString( Format("Physics material %s cannot have staticFriction = %f", GetName(), friction) );
        friction = 0.0f;
    }
    
	m_StaticFriction = friction;
	m_Material->setStaticFriction (friction);
	SetDirty ();
}

float PhysicMaterial::GetBounciness () const
{
	return m_Bounciness;
}

Vector3f PhysicMaterial::GetFrictionDirection2 () const
{
	return m_FrictionDirection2;
}

float PhysicMaterial::GetStaticFriction2 () const
{
	return m_StaticFriction2;
}

float PhysicMaterial::GetDynamicFriction2 () const
{
	return m_DynamicFriction2;
}

int PhysicMaterial::GetFrictionCombine ()
{
	return NxCombineModeToMonoCombineMode(m_FrictionCombine);
}

int PhysicMaterial::GetBounceCombine ()
{
	return NxCombineModeToMonoCombineMode(m_BounceCombine);
}

void PhysicMaterial::ChangedMaterial ()
{
	if (IsDefaultMaterial ())
		CopyMaterialToDefault ();
	SetDirty ();
}

void PhysicMaterial::SetBounciness (float bounce)
{
    if (bounce < 0 || bounce > 1)
    {
        ErrorString( Format("Physics material %s cannot have bounciness = %f", GetName(), bounce) );
        bounce = clamp(bounce, 0.0f, 1.0f);
    }
    
	m_Bounciness = bounce;
	m_Material->setRestitution (bounce);
	ChangedMaterial ();
}

void PhysicMaterial::UpdateFrictionDirection2 ()
{
	Vector3f dir = m_FrictionDirection2;
	float magnitude = Magnitude (dir);
	if (magnitude < 0.1F)
	{
		m_Material->setDirOfAnisotropy (NxVec3 (0,0,0));
		m_Material->setFlags (m_Material->getFlags () & (~NX_MF_ANISOTROPIC));
	}
	else
	{
		dir /= magnitude;
		m_Material->setDirOfAnisotropy ((NxVec3&)dir);
		m_Material->setFlags (m_Material->getFlags () | NX_MF_ANISOTROPIC);
	}

}

void PhysicMaterial::SetFrictionDirection2 (Vector3f dir)
{
	m_FrictionDirection2 = dir;
	UpdateFrictionDirection2 ();
	ChangedMaterial ();
}

void PhysicMaterial::SetDynamicFriction2 (float fric)
{
    if (fric < 0)
    {
        ErrorString( Format("Physics material %s cannot have dynamicFriction2 = %f", GetName(), fric) );
        fric = 0.0f;
    }
    
	m_DynamicFriction2 = fric;
	m_Material->setDynamicFrictionV (fric);
	ChangedMaterial ();
}

void PhysicMaterial::SetStaticFriction2 (float fric)
{
    if (fric < 0)
    {
        ErrorString( Format("Physics material %s cannot have staticFriction2 = %f", GetName(), fric) );
        fric = 0.0f;
    }
    
	m_StaticFriction2 = fric;
	m_Material->setStaticFrictionV (fric);
	ChangedMaterial ();
}

void PhysicMaterial::SetFrictionCombine (int mode)
{
	m_FrictionCombine = MonoCombineModeToNxCombineMode(mode);
	m_Material->setFrictionCombineMode ((NxCombineMode)m_FrictionCombine);
	ChangedMaterial ();
}

void PhysicMaterial::SetBounceCombine (int mode)
{
	m_BounceCombine = MonoCombineModeToNxCombineMode(mode);
	m_Material->setRestitutionCombineMode ((NxCombineMode)m_BounceCombine);
	ChangedMaterial ();
}

void PhysicMaterial::CopyMaterialToDefault () const
{
	NxMaterialDesc desc;
	m_Material->saveToDesc (desc);
	GetDynamicsScene ().getMaterialFromIndex (0)->loadFromDesc (desc);
}

PhysicMaterial& PhysicMaterial::GetInstantiatedMaterial (PhysicMaterial* material, Object& owner)
{
	if (material)
	{
		if (material->m_Owner == PPtr<Object> (&owner))
			return const_cast<PhysicMaterial&> (*material);
		else
		{
			PhysicMaterial* instance = NEW_OBJECT(PhysicMaterial);
			instance->Reset ();

			instance->SetNameCpp (Append (material->GetName (), " (Instance)"));
			instance->m_FrictionCombine = material->m_FrictionCombine;
			instance->m_BounceCombine = material->m_BounceCombine;
			instance->m_DynamicFriction = material->m_DynamicFriction;
			instance->m_StaticFriction = material->m_StaticFriction;
			instance->m_Bounciness = material->m_Bounciness;
			instance->m_DynamicFriction2 = material->m_DynamicFriction2;
			instance->m_StaticFriction2 = material->m_StaticFriction2;
			instance->m_FrictionDirection2 = material->m_FrictionDirection2;
			instance->m_Owner = &owner;

			instance->AwakeFromLoad (kDefaultAwakeFromLoad);
			return *instance;
		}
	}
	else
	{
		PhysicMaterial* instance = NEW_OBJECT (PhysicMaterial);
		instance->Reset ();

		instance->SetName ("Default (Instance)");
		instance->m_Owner = &owner;

		instance->AwakeFromLoad (kDefaultAwakeFromLoad);
		return *instance;
	}
}

IMPLEMENT_CLASS_HAS_INIT (PhysicMaterial)
IMPLEMENT_OBJECT_SERIALIZE (PhysicMaterial)
#endif //ENABLE_PHYSICS
