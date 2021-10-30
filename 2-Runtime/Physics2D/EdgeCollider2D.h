#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Graphics/Polygon2D.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Utilities/dynamic_array.h"



// --------------------------------------------------------------------------


class EdgeCollider2D : public Collider2D
{
public:	
	REGISTER_DERIVED_CLASS (EdgeCollider2D, Collider2D)
	DECLARE_OBJECT_SERIALIZE (EdgeCollider2D)

	typedef dynamic_array<Vector2f> Points;

	EdgeCollider2D (MemLabelId label, ObjectCreationMode mode);
	// virtual ~EdgeCollider2D (); declared-by-macro

	virtual void Reset ();
	virtual void SmartReset ();

	bool SetPoints (const Vector2f* points, size_t count);
	const Points& GetPoints() const { return m_Points; }
	const Vector2f& GetPoint (int index) { Assert(index < m_Points.size ()); return m_Points[index]; }
	size_t GetPointCount() const { return m_Points.size (); }
	size_t GetEdgeCount() const { return m_Points.size () - 1; }

protected:
	virtual void Create (const Rigidbody2D* ignoreRigidbody = NULL);

private:
	int TransformPoints(const Matrix4x4f& relativeTransform, const Vector3f& scale, b2Vec2* outPoints);

private:
	Points m_Points;
};

#endif
