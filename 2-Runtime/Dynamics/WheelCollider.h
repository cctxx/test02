#ifndef WHEELCOLLIDER_H
#define WHEELCOLLIDER_H

#include "Collider.h"
#include "Runtime/Math/Vector3.h"
#include "JointDescriptions.h"

class NxMaterial;

// ----------------------------------------------------------------------

struct WheelFrictionCurve
{
	float extremumSlip; ///<Extremum Slip. range { 0.001, infinity }
	float extremumValue; ///<Extremum Value. range { 0.001, infinity }
	float asymptoteSlip; ///<Asymptote Slip. range { 0.001, infinity }
	float asymptoteValue; ///<Asymptote Value. range { 0.001, infinity }
	float stiffnessFactor; ///<Stiffness Factor. range { 0, infinity }

	bool operator != (const WheelFrictionCurve& c)const	
	{ 
		return extremumSlip != c.extremumSlip 
			|| extremumValue != c.extremumValue
			|| asymptoteSlip != c.asymptoteSlip
			|| asymptoteValue != c.asymptoteValue
			|| stiffnessFactor != c.stiffnessFactor
		; 
	}
	
	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (WheelFrictionCurve)
};


template<class TransferFunction>
void WheelFrictionCurve::Transfer (TransferFunction& transfer)
{
	TRANSFER_SIMPLE( extremumSlip );
	TRANSFER_SIMPLE( extremumValue );
	TRANSFER_SIMPLE( asymptoteSlip );
	TRANSFER_SIMPLE( asymptoteValue );
	TRANSFER_SIMPLE( stiffnessFactor );
}

// ----------------------------------------------------------------------

struct WheelHit
{
	Vector3f	point;
	Vector3f	normal;
	Vector3f	forwardDir;
	Vector3f	sidewaysDir;
	float		force;
	float		forwardSlip;
	float		sidewaysSlip;
	Collider*	collider;
};


// ----------------------------------------------------------------------


class WheelCollider : public Collider
{
public:	
	REGISTER_DERIVED_CLASS (WheelCollider, Collider)
	DECLARE_OBJECT_SERIALIZE (WheelCollider)
	
	WheelCollider(MemLabelId label, ObjectCreationMode mode);
	
	virtual void Reset();
	virtual void SmartReset();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	
	Vector3f GetCenter() const { return m_Center; }
	void SetCenter( const Vector3f& center );

	float GetRadius() const { return m_Radius; }
	void SetRadius( float f );
	
	float GetSuspensionDistance() const { return m_SuspensionDistance; }
	void SetSuspensionDistance( float f );

	void SetSuspensionSpring( const JointSpring& spring );
	const JointSpring& GetSuspensionSpring() const { return m_SuspensionSpring; }
	
	void SetForwardFriction( const WheelFrictionCurve& curve );
	const WheelFrictionCurve& GetForwardFriction() const { return m_ForwardFriction; }
	
	void SetSidewaysFriction( const WheelFrictionCurve& curve );
	const WheelFrictionCurve& GetSidewaysFriction() const { return m_SidewaysFriction; }
	
	float GetMass() const { return m_Mass; }
	void SetMass( float f );

	float GetMotorTorque() const;
	void SetMotorTorque( float f );
	
	float GetBrakeTorque() const;
	void SetBrakeTorque( float f );
	
	float GetSteerAngle() const;
	void SetSteerAngle( float f );

	bool IsGrounded() const;
	bool GetGroundHit( WheelHit& hit ) const;
	
	float GetRpm() const;

	void TransformChanged( int changeMask );

	static void InitializeClass();
	static void CleanupClass() { }

	Vector3f GetGlobalCenter() const;
	float GetGlobalRadius() const;
	float GetGlobalSuspensionDistance() const;
	Matrix4x4f CalculateTransform() const;

	virtual bool SupportsMaterial () const { return false; }

protected:
	virtual void FetchPoseFromTransform();
	virtual bool GetRelativeToParentPositionAndRotation( Transform& transform, Transform& anyParent, Matrix4x4f& matrix );

	virtual void Create(const Rigidbody* ignoreRigidbody);
	virtual void ScaleChanged();
	virtual void Cleanup();

private:
	Vector3f		m_Center;
	JointSpring		m_SuspensionSpring;
	WheelFrictionCurve	m_ForwardFriction;
	WheelFrictionCurve	m_SidewaysFriction;
	float			m_Radius; ///< range { 0, infinity }
	float			m_SuspensionDistance; ///< range { 0, infinity }
	float			m_Mass; ///< range { 0.0001, infinity }

	// the wheel material is shared by all instances
	static NxMaterial*	m_WheelMaterial;
};

#endif
