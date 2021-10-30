#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Math/Vector2.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Collider2D.h"
#include "Runtime/Graphics/Polygon2D.h"

class Vector3f;


// --------------------------------------------------------------------------


class PolygonColliderBase2D : public Collider2D
{
public:	
	REGISTER_DERIVED_ABSTRACT_CLASS (PolygonColliderBase2D, Collider2D)

	PolygonColliderBase2D (MemLabelId label, ObjectCreationMode mode);

	virtual const Polygon2D& GetPoly() const = 0;

protected:
	virtual void Create (const Rigidbody2D* ignoreRigidbody = NULL);

	b2Shape* ExtractConvexShapes(dynamic_array<b2Shape*>& shapes, const Matrix4x4f& relativeTransform);
	static int TransformPoints(const Polygon2D::TPath& path, const Matrix4x4f& relativeTransform, const Vector3f& scale, b2Vec2* outPoints);
	static bool ValidatePolygonShape(const b2Vec2* const points, const int pointCount);
};

#endif
