#ifndef JOINTDESCRIPTIONS_H
#define JOINTDESCRIPTIONS_H

#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"

struct JointMotor
{
	/*
	The relative velocity the motor is trying to achieve. The motor will only be able
	to reach this velocity if the maxForce is sufficiently large. If the joint is 
	spinning faster than this velocity, the motor will actually try to brake. If you set this
	to infinity then the motor will keep speeding up, unless there is some sort of resistance
	on the attached bodies. The sign of this variable determines the rotation direction,
	with positive values going the same way as positive joint angles.
	Default is infinity.
	*/
	
	float targetVelocity;///< target velocity of motor
	
	/*
	The maximum force (torque in this case) the motor can exert. Zero disables the motor.
	Default is 0, should be >= 0. Setting this to a very large value if velTarget is also 
	very large may not be a good idea.
	*/
	float force;	///< maximum motor force / torque range { 0, infinity }

	/*
	If true, motor will not brake when it spins faster than velTarget
	default: false.
	*/
	int freeSpin;///

	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (JointMotor)
};

template<class TransferFunction>
void JointMotor::Transfer (TransferFunction& transfer)
{
	TRANSFER_SIMPLE (targetVelocity);
	TRANSFER (force);
	transfer.Transfer (freeSpin, "freeSpin", kEditorDisplaysCheckBoxMask);
}

struct JointDrive
{
	int    mode; ///< The mode of what this dirves enum { Disabled = 0, position = 1, velocity = 2, position and velocity = 3 } 
	float positionSpring;///< The spring used to reach the target range { 0, infinity }
	float positionDamper;///< The damping used to reach the target range { 0, infinity }
	float maximumForce;///< The maximum force the drive can exert to reach the target velocity. range {0, infinity}

	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (JointDrive)
};

template<class TransferFunction>
void JointDrive::Transfer (TransferFunction& transfer)
{
	transfer.SetVersion (2);
	
	TRANSFER_SIMPLE (mode);
	TRANSFER_SIMPLE (positionSpring);
	TRANSFER_SIMPLE (positionDamper);
	TRANSFER_SIMPLE (maximumForce);
	if (transfer.IsOldVersion(1) && transfer.IsReading())
	{
		// This case used to be ignored in old versions of PhysX, but no longer is.
		// If it is zero, set it to a working value instead.
		if (mode == NX_D6JOINT_DRIVE_POSITION)
			maximumForce = NX_MAX_F32;
	}
}
struct SoftJointLimit
{
	float limit;
	float bounciness;///< When the joint hits the limit. This will determine how bouncy it will be. { 0, 1 }
	float spring;///< If greater than zero, the limit is soft. The spring will pull the joint back. { 0, infinity }
	float damper;///< If spring is greater than zero, the limit is soft. This is the damping of spring. { 0, infinity }
	
	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (SoftJointLimit)
};

template<class TransferFunction>
void SoftJointLimit::Transfer (TransferFunction& transfer)
{
	TRANSFER_SIMPLE (limit);
	TRANSFER_SIMPLE (bounciness);
	TRANSFER_SIMPLE (spring);
	TRANSFER_SIMPLE (damper);
}




// Describes a joint spring. The spring is implicitly integrated, so even high spring and damper
// coefficients should be robust.
struct JointSpring
{
	float spring;		//!< spring coefficient range { 0, infinity }
	float damper;		//!< damper coefficient range { 0, infinity }
	float targetPosition;	//!< target value (angle/position) of spring where the spring force is zero.

	bool operator != (const JointSpring& s)const	
	{ 
		return spring != s.spring 
			|| damper != s.damper
			|| targetPosition != s.targetPosition
		; 
	}

	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (JointSpring)
};

template<class TransferFunction>
void JointSpring::Transfer (TransferFunction& transfer)
{
	TRANSFER_SIMPLE (spring);
	TRANSFER_SIMPLE (damper);
	TRANSFER_SIMPLE (targetPosition);
}

struct JointLimits
{
	float min;
	float minBounce;///< range {0, 1}
	float minHardness;// unsupported
	
	float max;
	float maxBounce;///< range {0, 1}
	float maxHardness;// unsupported
	JointLimits () { minHardness = maxHardness = 0.0F; }
	
	DECLARE_SERIALIZE_NO_PPTR (JointLimits)
};

template<class TransferFunction>
void JointLimits::Transfer (TransferFunction& transfer)
{
	TRANSFER_SIMPLE (min);
	TRANSFER_SIMPLE (max);
	TRANSFER_SIMPLE (minBounce);
	TRANSFER_SIMPLE (maxBounce);
}

inline void InitJointLimits (JointLimits& limits)
{
	limits.min = 0.0F;
	limits.max = 0.0F;
	limits.minBounce = 0.0F;
	limits.maxBounce = 0.0F;
	limits.minHardness = 0.0F;
	limits.maxHardness = 0.0F;
}

inline void InitJointSpring (JointSpring& spring)
{
	spring.spring = 0.0F;
	spring.damper = 0.0F;
	spring.targetPosition = 0.0F;
}

inline void InitJointMotor (JointMotor& motor)
{
	motor.targetVelocity = 0.0F;
	motor.force = 0.0F;
	motor.freeSpin = 0;
}

inline void InitJointDrive (JointDrive& motor)
{
	motor.mode = 0;
	motor.positionSpring = 0.0F;
	motor.positionDamper = 0.0F;
	motor.maximumForce = NX_MAX_F32;
}

inline void InitSoftJointLimit (SoftJointLimit& motor)
{
	motor.limit = 0.0F;
	motor.bounciness = 0.0F;
	motor.spring = 0.0F;
	motor.damper = 0.0F;
}

inline void ConvertDrive (JointDrive& drive, NxJointDriveDesc& out)
{
	out.spring = drive.positionSpring;
	out.damping = drive.positionDamper;
	out.forceLimit = drive.maximumForce;
	out.driveType = drive.mode;
}

inline void ConvertDrive (JointDrive& drive, NxJointDriveDesc& out, int mask)
{
	out.spring = drive.positionSpring;
	out.damping = drive.positionDamper;
	out.forceLimit = drive.maximumForce;
	out.driveType = mask;
}

inline void ConvertSoftLimit (SoftJointLimit& limit, NxJointLimitSoftDesc& out)
{
	out.value = Deg2Rad (limit.limit);
	out.restitution = limit.bounciness;
	out.spring = limit.spring;
	out.damping = limit.damper;
}

inline void ConvertSoftLimitLinear (SoftJointLimit& limit, NxJointLimitSoftDesc& out)
{
	out.value = limit.limit;
	out.restitution = limit.bounciness;
	out.spring = limit.spring;
	out.damping = limit.damper;
}

#endif
