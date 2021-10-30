#ifndef SPRINGJOINT_H
#define SPRINGJOINT_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Vector3.h"
#include "JointDescriptions.h"
#include "Joint.h"
class Rigidbody;
class NxJointDesc;

namespace Unity
{


class SpringJoint : public Joint
{
	public:
	
	REGISTER_DERIVED_CLASS (SpringJoint, Joint)
	DECLARE_OBJECT_SERIALIZE (SpringJoint)
	
	SpringJoint (MemLabelId label, ObjectCreationMode mode);
	void Reset();
	
	float GetSpring () const { return m_Spring; } 
	void SetSpring (float spring);
	
	float GetDamper () { return m_Damper; }
	void SetDamper(float damper);
	
	float GetMinDistance() { return m_MinDistance; }
	void SetMinDistance(float distance);

	float GetMaxDistance() { return m_MaxDistance; }
	void SetMaxDistance(float distance);
	
	private:

	virtual void ApplySetupAxesToDesc (int option);
	virtual void Create ();
	
	float m_MinDistance; ///< range { 0, infinity }
	float m_MaxDistance; ///< range { 0, infinity }
	float m_Spring; ///< range { 0, infinity }
	float m_Damper; ///< range { 0, infinity }
};

}

#endif // SPRINGJOINT_H

