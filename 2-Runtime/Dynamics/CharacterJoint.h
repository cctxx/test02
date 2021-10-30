#ifndef CHARACTERJOINT_H
#define CHARACTERJOINT_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "JointDescriptions.h"
#include "Joint.h"
class Rigidbody;
class NxJointDesc;
class NxRevoluteJoint;

namespace Unity
{

class CharacterJoint : public Joint
{
	public:
	
	REGISTER_DERIVED_CLASS (CharacterJoint, Joint)
	DECLARE_OBJECT_SERIALIZE (CharacterJoint)

	CharacterJoint (MemLabelId label, ObjectCreationMode mode);
	
	JointDrive GetRotationDrive ();
	void SetRotationDrive (const JointDrive& drive);

	void SetTargetRotation (const Quaternionf& rot); 
	Quaternionf GetTargetRotation (); 

	void SetTargetAngularVelocity (const Vector3f& angular); 
	Vector3f GetTargetAngularVelocity ();

	virtual void ApplySetupAxesToDesc (int option);

	virtual void SetSwingAxis (const Vector3f& axis);
	Vector3f GetSwingAxis () { return m_SwingAxis; }
	
	void SetLowTwistLimit (const SoftJointLimit& limit);
	SoftJointLimit GetLowTwistLimit () { return m_LowTwistLimit; }

	void SetHighTwistLimit (const SoftJointLimit& limit);
	SoftJointLimit GetHighTwistLimit () { return m_HighTwistLimit; }

	void SetSwing1Limit (const SoftJointLimit& limit);
	SoftJointLimit GetSwing1Limit () { return m_Swing1Limit; }

	void SetSwing2Limit (const SoftJointLimit& limit);
	SoftJointLimit GetSwing2Limit () { return m_Swing2Limit; }
	
	virtual void Reset();

	void UpdateTargetRotation ();
	virtual void CheckConsistency ();
	
	////// THIS IS NOT GOOD!!!!!!!!!!!
	void CalculateGlobalHingeSpace (Vector3f& globalAnchor, Vector3f& globalAxis, Vector3f& globalNormal) const;

	private:

	virtual void Create ();
	void SetupDriveType ();

	bool           m_UseTargetRotation;
	Quaternionf    m_TargetRotation;
	Vector3f       m_TargetAngularVelocity;
	
	mutable Quaternionf    m_ConfigurationSpace;
	
	////// THIS IS NOT GOOD!!!!!!!!!!!
	Vector3f       m_SwingAxis;
	
	JointDrive     m_RotationDrive;
	
	SoftJointLimit m_LowTwistLimit;
	SoftJointLimit m_HighTwistLimit;
	SoftJointLimit m_Swing1Limit;
	SoftJointLimit m_Swing2Limit;
};

}

#endif
