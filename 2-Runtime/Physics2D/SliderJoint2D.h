#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Math/Vector2.h"
#include "Runtime/Physics2D/Joint2D.h"

class Vector2f;


// --------------------------------------------------------------------------


class SliderJoint2D : public Joint2D
{
public:

	REGISTER_DERIVED_CLASS (SliderJoint2D, Joint2D)
	DECLARE_OBJECT_SERIALIZE (SliderJoint2D)

	SliderJoint2D (MemLabelId label, ObjectCreationMode mode);
	// virtual ~SliderJoint2D (); declared-by-macro

	virtual void CheckConsistency();
	virtual void Reset ();

	Vector2f GetAnchor () const { return m_Anchor; }
	virtual void SetAnchor (const Vector2f& anchor);

	Vector2f GetConnectedAnchor () const { return m_ConnectedAnchor; }
	virtual void SetConnectedAnchor (const Vector2f& anchor);

	float GetAngle () const { return m_Angle; }
	void SetAngle (float angle);

	bool GetUseMotor () const { return m_UseMotor; }
	void SetUseMotor (bool enable);

	bool GetUseLimits () const { return m_UseLimits; }
	void SetUseLimits (bool enable);

	JointMotor2D GetMotor () const { return m_Motor; }
	void SetMotor (const JointMotor2D& motor);

	JointTranslationLimits2D GetLimits () const { return m_TranslationLimits; }
	void SetLimits (const JointTranslationLimits2D& limits);

protected:
	virtual void Create ();
	virtual void ReCreate();

protected:
	Vector2f					m_Anchor;				///< The local-space anchor from the base rigid-body.
	Vector2f					m_ConnectedAnchor;		///< The local-space anchor from the connected rigid-body.
	float						m_Angle;				///< The translation angle that the joint slides along.  range { 0.0, 359.9999 }
	JointMotor2D				m_Motor;				///< The joint motor.
	JointTranslationLimits2D	m_TranslationLimits;	///< The joint angle limits.
	bool						m_UseMotor;				///< Whether to use the joint motor or not.
	bool						m_UseLimits;			///< Whether to use the angle limits or not.

private:
	float						m_OldReferenceAngle;
};

#endif
