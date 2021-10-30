#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Math/Vector2.h"
#include "Runtime/Physics2D/Joint2D.h"

class Vector2f;


// --------------------------------------------------------------------------


class SpringJoint2D : public Joint2D
{
public:
	
	REGISTER_DERIVED_CLASS (SpringJoint2D, Joint2D)
	DECLARE_OBJECT_SERIALIZE (SpringJoint2D)

	SpringJoint2D (MemLabelId label, ObjectCreationMode mode);
	// virtual ~SpringJoint2D (); declared-by-macro
	
	virtual void CheckConsistency();
	virtual void Reset ();

	Vector2f GetAnchor () const { return m_Anchor; }
	virtual void SetAnchor (const Vector2f& anchor);

	Vector2f GetConnectedAnchor () const { return m_ConnectedAnchor; }
	virtual void SetConnectedAnchor (const Vector2f& anchor);

	void SetDistance (float distance);
	float GetDistance () const { return m_Distance; }

	void SetDampingRatio (float ratio);
	float GetDampingRatio () const { return m_DampingRatio; }

	void SetFrequency (float frequency);
	float GetFrequency () const { return m_Frequency; }

protected:
	virtual void Create ();
	void AutoCalculateDistance ();
	
protected:
	float		m_Distance;			///< The distance which the joint should attempt to maintain between attached bodies.  range { 0.005, 1000000 }
	float		m_DampingRatio;		///< The damping ratio for the oscillation whilst trying to achieve the specified distance.  0 means no damping.  1 means critical damping.  range { 0.0, 1.0 }
	float		m_Frequency;		///< The frequency in Hertz for the oscillation whilst trying to achieve the specified distance.  range { 0.0, 1000000 }
	Vector2f	m_Anchor;			///< The local-space anchor from the base rigid-body.
	Vector2f	m_ConnectedAnchor;	///< The local-space anchor from the connected rigid-body.
};

#endif
