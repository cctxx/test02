#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Math/Vector2.h"


// --------------------------------------------------------------------------


class BoxCollider2D : public Collider2D
{
public:	
	REGISTER_DERIVED_CLASS (BoxCollider2D, Collider2D)
	DECLARE_OBJECT_SERIALIZE (BoxCollider2D)
	
	BoxCollider2D (MemLabelId label, ObjectCreationMode mode);
	
	virtual void CheckConsistency ();
	virtual void Reset ();
	virtual void SmartReset ();

	void SetSize (const Vector2f& size);
	const Vector2f& GetSize () const { return m_Size; }

	void SetCenter (const Vector2f& center);
	const Vector2f& GetCenter() const { return m_Center; }

protected:
	virtual void Create (const Rigidbody2D* ignoreRigidbody = NULL);
	
private:	
	Vector2f m_Size;	///< The size of the box.
	Vector2f m_Center;	///< The offset of the box.
};

#endif
