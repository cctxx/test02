#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Math/Vector2.h"
#include "Runtime/Physics2D/Joint2D.h"

class Vector2f;


// --------------------------------------------------------------------------


class DistanceJoint2D : public Joint2D
{
public:
	
	REGISTER_DERIVED_CLASS (DistanceJoint2D, Joint2D)
	DECLARE_OBJECT_SERIALIZE (DistanceJoint2D)

	DistanceJoint2D (MemLabelId label, ObjectCreationMode mode);
	// virtual ~DistanceJoint2D (); declared-by-macro
	
	virtual void CheckConsistency();
	virtual void Reset ();

	Vector2f GetAnchor () const { return m_Anchor; }
	virtual void SetAnchor (const Vector2f& anchor);

	Vector2f GetConnectedAnchor () const { return m_ConnectedAnchor; }
	virtual void SetConnectedAnchor (const Vector2f& anchor);

	void SetDistance (float distance);
	float GetDistance () const { return m_Distance; }

protected:
	virtual void Create ();
	void AutoCalculateDistance ();
	
protected:
	float		m_Distance;			///< The maximum distance which the joint should attempt to maintain between attached bodies.  range { 0.005, 1000000 }
	Vector2f	m_Anchor;			///< The local-space anchor from the base rigid-body.
	Vector2f	m_ConnectedAnchor;	///< The local-space anchor from the connected rigid-body.
};

#endif
