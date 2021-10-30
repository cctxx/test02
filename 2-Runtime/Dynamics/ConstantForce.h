#ifndef CONSTANTFORCE_H
#define CONSTANTFORCE_H

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Math/Vector3.h"

class ConstantForce : public Behaviour
{
 public:
	REGISTER_DERIVED_CLASS (ConstantForce, Behaviour)
	DECLARE_OBJECT_SERIALIZE (ConstantForce)

	ConstantForce (MemLabelId label, ObjectCreationMode mode);
	virtual void Reset ();

	virtual void FixedUpdate ();
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	
	Vector3f	m_Force;				///< Force applied globally
	Vector3f	m_RelativeForce;		///< Force applied locally
	Vector3f	m_Torque;			///< Torque applied globally
	Vector3f	m_RelativeTorque;	///< Torque applied locally
	
	
	BehaviourListNode m_FixedUpdateNode;
};


#endif
