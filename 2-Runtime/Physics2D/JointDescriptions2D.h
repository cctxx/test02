#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Physics2D/Physics2DManager.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Utilities/ValidateArgs.h"

// --------------------------------------------------------------------------


struct JointMotor2D
{
	float m_MotorSpeed;			///< The target motor speed in degrees/second.  range { -1000000, 1000000 }
	float m_MaximumMotorForce;	///< The maximum force the motor can use to achieve the desired motor speed.  range { 0.0, 1000000 }

	void Initialize ()
	{
		m_MotorSpeed = 0.0f;
		m_MaximumMotorForce = 10000.0f;
	}

	void CheckConsistency ()
	{
		m_MotorSpeed = clamp<float> (m_MotorSpeed, -PHYSICS_2D_LARGE_RANGE_CLAMP, PHYSICS_2D_LARGE_RANGE_CLAMP);
		m_MaximumMotorForce = clamp<float> (m_MaximumMotorForce, 0, PHYSICS_2D_LARGE_RANGE_CLAMP);
	}

	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (JointMotor2D)
};

template<class TransferFunction>
void JointMotor2D::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_MotorSpeed);
	TRANSFER (m_MaximumMotorForce);
}


// --------------------------------------------------------------------------


struct JointAngleLimits2D
{
	float m_LowerAngle;	///< The lower angle (in degrees) limit to constrain the joint to.  range { -359.9999, 359.9999 }
	float m_UpperAngle;	///< The upper angle (in degrees) limit to constrain the joint to.  range { -359.9999, 359.9999 }

	void Initialize ()
	{
		m_LowerAngle = 0.0f;
		m_UpperAngle = 359.0f;
	}

	void CheckConsistency ()
	{
		m_LowerAngle = clamp<float> (m_LowerAngle, -359.9999f, 359.9999f);
		m_UpperAngle = clamp<float> (m_UpperAngle, -359.9999f, 359.9999f);
	}

	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (JointAngleLimit2D)
};

template<class TransferFunction>
void JointAngleLimits2D::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_LowerAngle);
	TRANSFER (m_UpperAngle);
}


// --------------------------------------------------------------------------


struct JointTranslationLimits2D
{
	float m_LowerTranslation;	///< The lower translation limit to constrain the joint to.  range { -1000000, 1000000 }
	float m_UpperTranslation;	///< The upper translation limit to constrain the joint to.  range { -1000000, 1000000 }

	void Initialize ()
	{
		m_LowerTranslation = 0.0f;
		m_UpperTranslation = 0.0f;
	}

	void CheckConsistency ()
	{
		m_LowerTranslation = clamp<float> (m_LowerTranslation, -PHYSICS_2D_LARGE_RANGE_CLAMP, PHYSICS_2D_LARGE_RANGE_CLAMP);
		m_UpperTranslation = clamp<float> (m_UpperTranslation, -PHYSICS_2D_LARGE_RANGE_CLAMP, PHYSICS_2D_LARGE_RANGE_CLAMP);
	}

	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (JointTranslationLimits2D)
};

template<class TransferFunction>
void JointTranslationLimits2D::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_LowerTranslation);
	TRANSFER (m_UpperTranslation);
}

#endif
