#ifndef HINGEJOINT_H
#define HINGEJOINT_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Vector3.h"
#include "JointDescriptions.h"
#include "Joint.h"
class Rigidbody;
class NxJointDesc;
class NxRevoluteJoint;

namespace Unity
{

class HingeJoint : public Joint
{
	public:
	
	REGISTER_DERIVED_CLASS (HingeJoint, Joint)
	DECLARE_OBJECT_SERIALIZE (HingeJoint)
	
	HingeJoint (MemLabelId label, ObjectCreationMode mode);

	JointMotor GetMotor () const;
	void SetMotor (const JointMotor& motor);

	JointLimits GetLimits () const;
	void SetLimits (const JointLimits& limits);

	JointSpring GetSpring () const;
	void SetSpring (const JointSpring& spring);

	void SetUseMotor (bool enable);
	bool GetUseMotor () const;

	void SetUseLimits (bool enable);
	bool GetUseLimits () const;

	void SetUseSpring (bool enable);
	bool GetUseSpring () const ;

	// The hinge angle's rate of change (angular velocity).
	float GetVelocity () const;

	// The hinge's angle
	float GetAngle () const;
	
	virtual void ApplySetupAxesToDesc (int option);

	private:

	virtual void Create ();

	void SetSpringNoEnable (const JointSpring& spring);
	void SetMotorNoEnable (const JointMotor& motor);
	void SetLimitsNoEnable (const JointLimits& limits);
	
	JointLimits      m_Limits;
	JointSpring      m_Spring;
	JointMotor       m_Motor;
	
	bool             m_UseLimits;
	bool             m_UseMotor;
	bool             m_UseSpring;
};

}

#endif
