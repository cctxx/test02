#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_CLOTH || DOXYGEN

#include "DeformableMesh.h"

class Collider;

namespace Unity
{

struct ClothAttachment
{
	PPtr<Collider> m_Collider;
	bool m_TwoWayInteraction;
	bool m_Tearable;
	
	DECLARE_SERIALIZE (ClothAttachment)
	
	ClothAttachment() :
		m_TwoWayInteraction(false),
		m_Tearable(false)
	{  }

};

class InteractiveCloth : public Cloth
{
public:	
	REGISTER_DERIVED_CLASS (InteractiveCloth, Cloth)
	DECLARE_OBJECT_SERIALIZE (InteractiveCloth)

	InteractiveCloth (MemLabelId label, ObjectCreationMode mode);
		
	virtual void ProcessMeshForRenderer ();
	
	virtual void FixedUpdate ();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void Reset ();
	
	void AddForceAtPosition (const Vector3f& force, const Vector3f& position, float radius, int mode);
	void AttachToCollider (Collider *collider, bool tearable, bool twoWayInteraction);
	void DetachFromCollider (Collider *collider);

	PPtr<Mesh> GetMesh () { return m_Mesh; }
	void SetMesh (PPtr<Mesh> value);

	float GetFriction () { return m_Friction; }
	void SetFriction (float value);

	float GetDensity () { return m_Density; }
	void SetDensity (float value);

	float GetPressure () { return m_Pressure; }
	void SetPressure (float value);

	float GetCollisionResponse () { return m_CollisionResponse; }
	void SetCollisionResponse (float value);

	float GetTearFactor () { return m_TearFactor; }
	void SetTearFactor (float value);

	float GetAttachmentTearFactor () { return m_AttachmentTearFactor; }
	void SetAttachmentTearFactor (float value);

	float GetAttachmentResponse () { return m_AttachmentResponse; }
	void SetAttachmentResponse (float value);

	bool GetIsTeared () { return m_IsTeared; }

	virtual void AddToManager ();
	virtual void RemoveFromManager ();

protected:
	
	virtual void Create ();
	virtual void PauseSimulation ();
	virtual void ResumeSimulation ();

	void CheckTearing();

	// configuration
	float m_Friction; ///<Friction. range { 0, 1 }
	float m_Density; ///<Density (mass per area). range { 0.001, 10000 }
	float m_Pressure; ///<Air pressure inside a closed cloth mesh. 0 = disabled. 1 = same pressure as outside atmosphere. range { 0, 10000 }
	float m_CollisionResponse; ///<Force to apply back to colliding rigidbodies. 0 = disabled. range { 0, 10000 }
	float m_TearFactor; ///<How far vertices need to stretch until they tear. 0 = disabled. range { 0, 10000 }
	float m_AttachmentTearFactor; ///<How far vertices need to stretch until attachments tear off, if tearing is enabled for the attachment. range { 0.001, 10000 }
	float m_AttachmentResponse; ///<Force to apply back to attached rigidbodies, if two way interaction is enabled for the attachment. range { 0, 1 }
	std::vector<ClothAttachment> m_AttachedColliders;

	// state
	bool m_IsTeared;
	PPtr<Mesh> m_CachedMesh;
	BehaviourListNode m_FixedUpdateNode;
};

}

#endif // ENABLE_CLOTH
