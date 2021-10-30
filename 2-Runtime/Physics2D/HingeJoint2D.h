#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Math/Vector2.h"
#include "Joint2D.h"

class Vector2f;


// --------------------------------------------------------------------------


class HingeJoint2D : public Joint2D
{
public:
	
	REGISTER_DERIVED_CLASS (HingeJoint2D, Joint2D)
	DECLARE_OBJECT_SERIALIZE (HingeJoint2D)

	HingeJoint2D (MemLabelId label, ObjectCreationMode mode);
	// virtual ~HingeJoint2D (); declared-by-macro
	
	virtual void CheckConsistency();
	virtual void Reset ();

	Vector2f GetAnchor () const { return m_Anchor; }
	virtual void SetAnchor (const Vector2f& anchor);

	Vector2f GetConnectedAnchor () const { return m_ConnectedAnchor; }
	virtual void SetConnectedAnchor (const Vector2f& anchor);

	bool GetUseMotor () const { return m_UseMotor; }
	void SetUseMotor (bool enable);

	bool GetUseLimits () const { return m_UseLimits; }
	void SetUseLimits (bool enable);

	JointMotor2D GetMotor () const { return m_Motor; }
	void SetMotor (const JointMotor2D& motor);

	JointAngleLimits2D GetLimits () const { return m_AngleLimits; }
	void SetLimits (const JointAngleLimits2D& limits);

protected:
	virtual void Create ();
	virtual void ReCreate();
	
protected:
	Vector2f			m_Anchor;			///< The local-space anchor from the base rigid-body.
	Vector2f			m_ConnectedAnchor;	///< The local-space anchor from the connected rigid-body.
	JointMotor2D		m_Motor;			///< The joint motor.
	JointAngleLimits2D	m_AngleLimits;		///< The joint angle limits.
	bool				m_UseMotor;			///< Whether to use the joint motor or not.
	bool				m_UseLimits;		///< Whether to use the angle limits or not.

private:
	float				m_OldReferenceAngle;
};

#endif
