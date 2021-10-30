#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/BaseClasses/NamedObject.h"


// --------------------------------------------------------------------------


class PhysicsMaterial2D : public NamedObject
{
public:	
	REGISTER_DERIVED_CLASS (PhysicsMaterial2D, NamedObject)
	DECLARE_OBJECT_SERIALIZE (PhysicsMaterial2D)
	
	PhysicsMaterial2D (MemLabelId label, ObjectCreationMode mode);
	// ~PhysicsMaterial2D (); declared-by-macro

	virtual void Reset ();
	virtual void CheckConsistency ();
	
	float GetFriction () const { return m_Friction; }
	void SetFriction (float friction);

	float GetBounciness () const { return m_Bounciness; }
	void SetBounciness (float bounce);

private:	
	float m_Friction;	///< Friction. Range { 0.0, 100000.0 }
	float m_Bounciness; ///< Bounciness. Range { 0.0, 1.0 }
	
	PPtr<Object> m_Owner;
};

#endif //ENABLE_2D_PHYSICS
