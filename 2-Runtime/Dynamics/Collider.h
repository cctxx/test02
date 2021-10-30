#ifndef COLLIDER_H
#define COLLIDER_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Profiler/Profiler.h"

class Transform;
class Matrix4x4f;
class Vector3f;

class Rigidbody;
class NxShape;
class NxShapeDesc;
class NxCCDSkeleton;
class PhysicMaterial;
namespace Unity { class InteractiveCloth; }
struct RaycastHit;
class Ray;

#if ENABLE_PROFILER
#define PROFILE_MODIFY_STATIC_COLLIDER if (m_Shape && m_Shape->getActor ().userData == NULL) { PROFILER_AUTO(gStaticColliderModify, this); }
extern ProfilerInformation gStaticColliderModify;
extern ProfilerInformation gStaticColliderMove;
extern ProfilerInformation gStaticColliderCreate;
extern ProfilerInformation gDynamicColliderCreate;
#else 
#define PROFILE_MODIFY_STATIC_COLLIDER
#endif

class Collider : public Unity::Component
{
	public:
	REGISTER_DERIVED_ABSTRACT_CLASS (Collider, Component)
	DECLARE_OBJECT_SERIALIZE (Collider)
	
	Collider (MemLabelId label, ObjectCreationMode mode);
	// virtual ~Collider (); declared-in-macro
	
	virtual void Deactivate (DeactivateOperation operation);
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	/// Enable or disable updates of this behaviour
	virtual void SetEnabled (bool enab);
	bool GetEnabled () const { return m_Enabled; }
	
	virtual bool SupportsMaterial () const { return true; }
	PPtr<PhysicMaterial> GetMaterial ();
	void SetMaterial (PPtr<PhysicMaterial> material);

	virtual void SetIsTrigger (bool trigger);
	bool GetIsTrigger () { 	return m_IsTrigger; }

	Rigidbody* GetRigidbody ();

	static void InitializeClass ();
	static void CleanupClass () {}
	void SetupLayer ();

	virtual void SupportedMessagesDidChange (int supported);
	
	virtual AABB GetBounds ();
	
	void ClosestPointOnBounds (const Vector3f& position, Vector3f& outPosition, float& outSqrDistance);
	
	void SetupIgnoreLayer ();
	
	bool Raycast (const Ray& ray, float distance, RaycastHit& outHit);
	
	public:

	// SUBCLASSES OVERRIDE THIS
	virtual void Create (const Rigidbody* ignoreAttachRigidbody) = 0;
	virtual void ScaleChanged () = 0;
	virtual void Cleanup ();
	virtual void ReCreate();
	
	void CreateWithoutIgnoreAttach ();
	
	NxShape* CreateShapeIfNeeded();

	// Testing API
	NxShape* GetShape() { return m_Shape; }

	protected:
	
	enum { kForceUpdateMass = 1 << 31 };
	virtual bool GetRelativeToParentPositionAndRotation (Transform& transform, Transform& anyParent, Matrix4x4f& matrix);
	static bool GetRelativeToParentPositionAndRotationUtility (Transform& transform, Transform& anyParent, const Vector3f& localOffset, Matrix4x4f& matrix);

	bool HasActorRigidbody ();
	int GetMaterialIndex ();

	void RecreateCollider (const Rigidbody* ignoreRigidbody);

	virtual void TransformChanged (int changeMask);

	virtual void FetchPoseFromTransform ();
	void FetchPoseFromTransformUtility (const Vector3f& offset);
	
	void FinalizeCreate( NxShapeDesc& shape, bool dontSetMaterial, const Rigidbody* dontAttachToRigidbody );
	virtual NxCCDSkeleton* CreateCCDSkeleton(float scale) { return NULL; }
	NxCCDSkeleton* CreateCCDSkeleton();
	void UpdateCCDSkeleton();

	void RigidbodyMassDistributionChanged ();

	#if UNITY_EDITOR
	void RefreshPhysicsInEditMode();
	#else
	void RefreshPhysicsInEditMode() {}
	#endif

	/// Finds the rigid body we want this collider to attach to.
	/// If this returns null the collider will create a kinematic actor.
	Rigidbody* FindNewAttachedRigidbody (const Rigidbody* ignoreAttachRigidbody);
	
	PPtr<PhysicMaterial> m_Material;
	NxShape*               m_Shape;
	bool                   m_IsTrigger;
	bool					m_Enabled;
	
	friend class Rigidbody;
	friend class Unity::InteractiveCloth;
	friend class PhysicsManager;
};


	/*
	PhysicMaterial& GetInstantiatedMaterial ()
	{
		PhysicMaterial* material = m_Material;
		if (material)
		{
			if (material->m_Owner == PPtr<Object> (this))
				return material;
			
			
		}
		else
		{
			material = new PhysicMaterial ();
			material->Reset ();
			material->m_Owner = this;
			m_Material = material;
		}
	}*/
struct MonoContactPoint
{
	Vector3f point;
	Vector3f normal;
	ScriptingObjectPtr thisCollider;
	ScriptingObjectPtr otherCollider;
};

struct MonoCollision
{
	Vector3f relativeVelocity;
	ScriptingObjectPtr rigidbody;
	ScriptingObjectPtr collider;
	ScriptingArrayPtr contacts;
};


struct Collision;
ScriptingObjectPtr ConvertContactToMono (Collision* input);

#endif
