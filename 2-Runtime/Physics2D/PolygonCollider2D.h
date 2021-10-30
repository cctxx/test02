#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Math/Vector2.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Physics2D/PolygonColliderBase2D.h"


// --------------------------------------------------------------------------


class PolygonCollider2D : public PolygonColliderBase2D
{
public:	
	REGISTER_DERIVED_CLASS (PolygonCollider2D, PolygonColliderBase2D)
	DECLARE_OBJECT_SERIALIZE (PolygonCollider2D)
	
	PolygonCollider2D (MemLabelId label, ObjectCreationMode mode);
	
	virtual void Reset ();
	virtual void SmartReset ();

	virtual const Polygon2D& GetPoly() const { return m_Poly; }
	Polygon2D& GetPoly() { return m_Poly; }
	void RefreshPoly();

	void CreatePrimitive (int sides, Vector2f scale = Vector2f(1.0f, 1.0f), Vector2f offset = Vector2f::zero);
	static void CreateNgon (const int sides, const Vector2f scale, const Vector2f offset, Polygon2D& polygon2D);

private:
	Polygon2D m_Poly;
};

#endif
