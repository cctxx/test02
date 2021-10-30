#ifndef CONFIGURABLEJOINT_H
#define CONFIGURABLEJOINT_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "JointDescriptions.h"
#include "Joint.h"
class Rigidbody;
class NxJointDesc;
class NxD6JointDesc;
class NxRevoluteJoint;

namespace Unity
{

class ConfigurableJoint : public Joint
{
	public:
	
	REGISTER_DERIVED_CLASS (ConfigurableJoint, Joint)
	DECLARE_OBJECT_SERIALIZE (ConfigurableJoint)

	ConfigurableJoint (MemLabelId label, ObjectCreationMode mode);
	
	virtual void Reset();

	int GetXMotion() { return m_XMotion; }
	void SetXMotion (int motion);

	int GetYMotion() { return m_YMotion; }
	void SetYMotion (int motion);
	
	int GetZMotion() { return m_ZMotion; }
	void SetZMotion (int motion);
	
	int GetAngularXMotion() { return m_AngularXMotion; }
	void SetAngularXMotion (int motion);

	int GetAngularYMotion() { return m_AngularYMotion; }
	void SetAngularYMotion (int motion);
	
	int GetAngularZMotion() { return m_AngularZMotion; }
	void SetAngularZMotion (int motion);
	
	SoftJointLimit GetLinearLimit () { return m_LinearLimit; }
	void SetLinearLimit (const SoftJointLimit& limit);

	
	SoftJointLimit GetLowAngularXLimit () { return m_LowAngularXLimit; }
	void SetLowAngularXLimit (const SoftJointLimit& limit);
	
	SoftJointLimit GetHighAngularXLimit () { return m_HighAngularXLimit; }
	void SetHighAngularXLimit (const SoftJointLimit& limit);

	SoftJointLimit GetAngularYLimit () { return m_AngularYLimit; }
	void SetAngularYLimit (const SoftJointLimit& limit);

	SoftJointLimit GetAngularZLimit () { return m_AngularZLimit; }
	void SetAngularZLimit (const SoftJointLimit& limit);
	
	JointDrive GetXDrive () { return m_XDrive; }
	void SetXDrive (const JointDrive& drive);
	
	JointDrive GetYDrive () { return m_YDrive; }
	void SetYDrive (const JointDrive& drive);
	
	JointDrive GetZDrive () { return m_ZDrive; }
	void SetZDrive (const JointDrive& drive);

	JointDrive GetAngularXDrive () { return m_AngularXDrive; }
	void SetAngularXDrive (const JointDrive& drive);
	
	JointDrive GetAngularYZDrive () { return m_AngularYZDrive; }
	void SetAngularYZDrive (const JointDrive& drive);
	
	JointDrive GetSlerpDrive () { return m_SlerpDrive; }
	void SetSlerpDrive (const JointDrive& drive);
	
	int GetRotationDriveMode () { return m_RotationDriveMode; }
	void SetRotationDriveMode (int drive);
	
//	void SetUseGear (bool gear);
//	bool GetUseGear () { return m_UseGear; }
	
	int GetProjectionMode () { return m_ProjectionMode; }
	void SetProjectionMode (int mode);

	float GetProjectionDistance () { return m_ProjectionDistance; }
	void SetProjectionDistance (float dist);

	float GetProjectionAngle () { return m_ProjectionAngle; }
	void SetProjectionAngle (float dist);

//	float GetGearRatio () { return m_GearRatio; }
//	void SetGearRatio (float ratio);
	
	Vector3f GetTargetPosition () { return m_TargetPosition; }
	void SetTargetPosition (const Vector3f& pos);
	
	Quaternionf GetTargetRotation () { return m_TargetRotation; }
	void SetTargetRotation (const Quaternionf& rotation);

	Vector3f GetTargetVelocity () { return m_TargetVelocity; }
	void SetTargetVelocity (const Vector3f& vel);
	
	Vector3f GetTargetAngularVelocity () { return m_TargetAngularVelocity; }
	void SetTargetAngularVelocity (const Vector3f& angular);

	void SetConfiguredInWorldSpace (bool c);
	bool GetConfiguredInWorldSpace () { return m_ConfiguredInWorldSpace; }

	void SetSwapBodies (bool c);
	bool GetSwapBodies () { return m_SwapBodies; }
	
	////// THIS IS NOT GOOD!!!!!!!!!!!
	void CalculateGlobalHingeSpace (Vector3f& globalAnchor, Vector3f& globalAxis, Vector3f& globalNormal) const;

	void CheckConsistency ();

	virtual void ApplySetupAxesToDesc (int option);

	virtual void SetSecondaryAxis (const Vector3f& axis);
	Vector3f GetSecondaryAxis () { return m_SecondaryAxis; }
	
	static void InitializeClass ();
	static void CleanupClass() { }
	
	private:
	void SetupD6Desc (NxD6JointDesc& desc);
	void FinalizeCreateD6 (NxD6JointDesc& desc);
	void ApplyKeepConfigurationSpace();
	void ApplyRebuildConfigurationSpace();

	virtual void Create ();
	void SetupDriveType ();

	int m_XMotion; ///< enum { Locked=0, Limited=1, Free=2 }
	int m_YMotion; ///< enum { Locked=0, Limited=1, Free=2 }
	int m_ZMotion; ///< enum { Locked=0, Limited=1, Free=2 }

	int m_AngularXMotion;  ///< enum { Locked=0, Limited=1, Free=2 }
	int m_AngularYMotion;  ///< enum { Locked=0, Limited=1, Free=2 }
	int m_AngularZMotion;  ///< enum { Locked=0, Limited=1, Free=2 }
	
	SoftJointLimit m_LinearLimit;
	SoftJointLimit m_LowAngularXLimit;
	SoftJointLimit m_HighAngularXLimit;
	SoftJointLimit m_AngularYLimit;
	SoftJointLimit m_AngularZLimit;
	
	JointDrive        m_XDrive;
	JointDrive        m_YDrive;
	JointDrive        m_ZDrive;
	
	JointDrive        m_AngularYZDrive;
	JointDrive        m_AngularXDrive;
	JointDrive        m_SlerpDrive;
	
	int m_ProjectionMode;///<	enum { None, Position and Rotation = 1, Position Only = 2 }

	float m_ProjectionDistance;
	float m_ProjectionAngle;

	int   m_RotationDriveMode; ///<	enum { X & YZ = 0, Slerp = 1 }
	bool m_ConfiguredInWorldSpace;
	bool m_SwapBodies;

	Vector3f       m_TargetPosition;
	Quaternionf   m_TargetRotation;

	Vector3f       m_TargetVelocity;
	Vector3f       m_TargetAngularVelocity;
	
	////// THIS IS NOT GOOD!!!!!!!!!!!
	Vector3f       m_SecondaryAxis;
};

}
#endif
