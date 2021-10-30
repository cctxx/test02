#ifndef DYNAMICSMATERIAL_H
#define DYNAMICSMATERIAL_H

#if ENABLE_PHYSICS
#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Math/Vector3.h"
#include "JointDescriptions.h"

class NxMaterial;

class PhysicMaterial : public NamedObject
{
	public:
	
	REGISTER_DERIVED_CLASS (PhysicMaterial, NamedObject)
	DECLARE_OBJECT_SERIALIZE (PhysicMaterial)
	
	PhysicMaterial (MemLabelId label, ObjectCreationMode mode);
	// ~PhysicMaterial (); declared-by-macro

	virtual bool MainThreadCleanup ();
	
	static void InitializeClass ();
	static void CleanupClass () {}

	float GetDynamicFriction () const;
	void SetDynamicFriction (float friction);

	float GetStaticFriction () const;
	void SetStaticFriction (float friction);
	
	void SetBounciness (float bounce);
	float GetBounciness () const;
	
	void SetFrictionDirection2 (Vector3f dir);
	Vector3f GetFrictionDirection2 () const;

	float GetStaticFriction2 () const;
	void SetStaticFriction2 (float fric);

	float GetDynamicFriction2 () const;
	void SetDynamicFriction2 (float fric);
	
	void SetSpring (const JointSpring& spring);
	JointSpring GetSpring () const;
	
	void SetUseSpring (bool use);
	bool GetUseSpring () const;
	
	int GetMaterialIndex () const { Assert(m_MaterialIndex > 0); return m_MaterialIndex; }
	
	virtual void Reset ();

	static PhysicMaterial& GetInstantiatedMaterial (PhysicMaterial* material, Object& owner);
	
	int GetFrictionCombine ();
	void SetFrictionCombine (int mode);

	int GetBounceCombine ();
	void SetBounceCombine (int mode);

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	
	private:
	
	void UpdateFrictionDirection2 ();
	Vector3f    m_FrictionDirection2;///< Friction 
	int m_FrictionCombine;///< enum { Average = 0, Minimum = 1, Multiply = 2, Maximum = 3 }
	int m_BounceCombine;///< enum { Average = 0, Minimum = 1, Multiply = 2, Maximum = 3 }
	float m_DynamicFriction;///< range { 0, 1 }
	float m_StaticFriction;///< range { 0, infinity }
	float m_Bounciness;///< range { 0, 1 }
	float m_DynamicFriction2;///< range { 0, 1 }
	float m_StaticFriction2;///< range { 0, infinity }

#if DOXYGEN
	Vector3f    frictionDirection2;///< Friction 
	int frictionCombine;///< enum { Average = 0, Minimum = 1, Multiply = 2, Maximum = 3 }
	int bounceCombine;///< enum { Average = 0, Minimum = 1, Multiply = 2, Maximum = 3 }
	float dynamicFriction;///< range { 0, 1 }
	float staticFriction;///< range { 0, infinity }
	float bounciness;///< range { 0, 1 }
	float dynamicFriction2;///< range { 0, 1 }
	float staticFriction2;///< range { 0, infinity }
#endif
	
	void CopyMaterialToDefault () const;
	bool IsDefaultMaterial ();
	void ChangedMaterial ();
			
	int m_MaterialIndex;
	NxMaterial* m_Material;
	PPtr<Object> m_Owner;
	
	friend class PhysicsManager;
};

#endif //ENABLE_PHYSICS
#endif
