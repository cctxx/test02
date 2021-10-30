#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Math/Vector2.h"


// --------------------------------------------------------------------------


class CircleCollider2D : public Collider2D
{
public:	
	REGISTER_DERIVED_CLASS (CircleCollider2D, Collider2D)
	DECLARE_OBJECT_SERIALIZE (CircleCollider2D)
	
	CircleCollider2D (MemLabelId label, ObjectCreationMode mode);
	// virtual ~CircleCollider2D (); declared-by-macro
	
	virtual void CheckConsistency ();
	virtual void Reset ();
	virtual void SmartReset ();
	
	void SetRadius (float radius);
	float GetRadius () const { return m_Radius; }

	void SetCenter (const Vector2f& center);
	const Vector2f& GetCenter() const { return m_Center; }

protected:
	virtual void Create (const Rigidbody2D* ignoreRigidbody = NULL);

private:	
	float m_Radius;		///< The radius of the circle. range { 0.0001, 1000000 }
	Vector2f m_Center;	///< The offset of the circle.
};

#endif
