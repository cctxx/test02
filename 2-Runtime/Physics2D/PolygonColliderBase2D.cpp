#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS

#include "Runtime/Physics2D/PolygonColliderBase2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"
#include "Runtime/Physics2D/Physics2DManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Graphics/SpriteFrame.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Profiler/Profiler.h"

#include "External/Box2D/Box2D/Box2D.h"
#include "External/libtess2/libtess2/tesselator.h"

PROFILER_INFORMATION(gPhysics2DProfilePolygonColliderBaseCreate, "Physics2D.PolygonColliderCreate", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfilePolygonColliderBaseDecomposition, "Physics2D.PolygonColliderDecomposition", kProfilerPhysics)

IMPLEMENT_CLASS (PolygonColliderBase2D)


// --------------------------------------------------------------------------


PolygonColliderBase2D::PolygonColliderBase2D (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
}


PolygonColliderBase2D::~PolygonColliderBase2D ()
{
}


// --------------------------------------------------------------------------


void PolygonColliderBase2D::Create (const Rigidbody2D* ignoreRigidbody)
{
	PROFILER_AUTO(gPhysics2DProfilePolygonColliderBaseCreate, NULL);

	// Ensure we're cleaned-up.
	Cleanup ();

	// Ignore if not active.
	if (!IsActive())
		return;

	const Polygon2D& poly = GetPoly();
	if (poly.IsEmpty())
		return;

	const int pathCount = poly.GetPathCount();
	if (pathCount == 0)
		return;

	// Calculate collider transformation.
	Matrix4x4f relativeTransform;
	b2Body* body;
	CalculateColliderTransformation (ignoreRigidbody, &body, relativeTransform);

	{
		PROFILER_AUTO(gPhysics2DProfilePolygonColliderBaseDecomposition, NULL);

		// Extract the convex shapes from the path(s).
		dynamic_array<b2Shape*> shapePtr;
		b2Shape* shapeMem = ExtractConvexShapes(shapePtr, relativeTransform);

		// Finish if no shapes generated.
		if (shapeMem == NULL)
			return;

		b2FixtureDef def;
		FinalizeCreate(def, body, &shapePtr);	

		FREE_TEMP_MANUAL(shapeMem);
	}
}


b2Shape* PolygonColliderBase2D::ExtractConvexShapes(dynamic_array<b2Shape*>& shapes, const Matrix4x4f& relativeTransform )
{
	// Calculate the maximum number of vertices for the shape type.
	const int kMaxPolygonVerts = b2_maxPolygonVertices;
	const int kVertexSize = 2;
	const Vector3f scale = GetComponent(Transform).GetWorldScaleLossy();
	
	// Tessellation
	TESStesselator* tess = tessNewTess (NULL);

	// Add all paths as a tessellation contour.
	const Polygon2D& poly = GetPoly();
	const int pathCount = poly.GetPathCount();
	int addedContours = 0;
	for (int pathIndex = 0; pathIndex < pathCount; ++pathIndex)
	{
		// Fetch the path.
		const Polygon2D::TPath& path = poly.GetPath(pathIndex);

		// Ignore illegal path.
		if (path.size() < 3)
			continue;

		// Validate the path.		
		b2Vec2* points;
		ALLOC_TEMP(points, b2Vec2, path.size());
		const int validPointCount = TransformPoints (path, relativeTransform, scale, points);

		// Add path contour.
		tessAddContour (tess, kVertexSize, points, sizeof(b2Vec2), validPointCount);
		addedContours++;
	}

	// Finish if no contours added.
	if (addedContours == 0)
		return NULL;

	// Perform the tessellation.
	const int tessError = tessTesselate(tess, TESS_WINDING_ODD, TESS_POLYGONS, kMaxPolygonVerts, kVertexSize, NULL);
	AssertBreak(tessError == 1);

	// Allocate the shape array.
	const int elemCount = tessGetElementCount (tess);

	// Finish if no elements.
	if (elemCount == 0)
		return NULL;

	shapes.resize_uninitialized(elemCount);
	b2PolygonShape* polygons = ALLOC_TEMP_MANUAL(b2PolygonShape, elemCount);

	// Extract the tessellation results into the shape array.
	const TESSindex* elements = tessGetElements(tess);
	const TESSreal* real = tessGetVertices(tess);
	b2Vec2* buffer;
	ALLOC_TEMP(buffer, b2Vec2, kMaxPolygonVerts);
	int totalElementCount=0;
	for (int elementIndex = 0; elementIndex < elemCount; ++elementIndex)
	{
		const int* indices = &elements[elementIndex * kMaxPolygonVerts];
				
		// Extract vertices
		int bufSize = 0;
		for (int i = 0; i < kMaxPolygonVerts && indices[i] != TESS_UNDEF; ++i)
		{
			const float& x = real[indices[i]*kVertexSize];
			const float& y = real[indices[i]*kVertexSize + 1];

			b2Vec2 newPoint(x, y);
			if (bufSize > 0 && b2DistanceSquared(buffer[bufSize-1], newPoint) <= b2_linearSlop * b2_linearSlop)
				continue;
			
			buffer[bufSize] = newPoint;
			++bufSize;
		}

		// Ignore small paths.
		if (bufSize < 3)
			continue;
    
		// Fill shape
		if (ValidatePolygonShape (buffer, bufSize))
		{
			b2PolygonShape& shape = polygons[totalElementCount];
			new (&shape) b2PolygonShape();
			shape.Set(buffer, bufSize);
			shapes[totalElementCount++] = &shape;
		}
	}

	tessDeleteTess(tess);

	// Finish if nothing generated.
	if (totalElementCount == 0)
	{
		if (polygons)
			FREE_TEMP_MANUAL(polygons);
		return NULL;
	}

	shapes.resize_initialized(totalElementCount);

	return polygons;
}


int PolygonColliderBase2D::TransformPoints(const Polygon2D::TPath& path, const Matrix4x4f& relativeTransform, const Vector3f& scale, b2Vec2* outPoints)
{
	int outCount = 0;
	for (size_t i = 0; i < path.size(); ++i)
	{
		// Calculate 3D vertex.
		const Vector3f vertex3D = relativeTransform.MultiplyPoint3 (Vector3f(path[i].x * scale.x, path[i].y * scale.y, 0.0f));

		// If any vertex are infinite or are a very large scale then abort transformation.
		// We abort here rather than ignore the vertex otherwise we may end-up with large-scale collider geometry warping if
		// only a few points are infinite or out-of-bounds.  This less likely an issue with the small-scale.
		if (!IsFinite (vertex3D) || SqrMagnitude(vertex3D ) > (PHYSICS_2D_LARGE_RANGE_CLAMP*PHYSICS_2D_LARGE_RANGE_CLAMP))
			return 0;

		// Fetch 2D vertex.
		b2Vec2 vertex2D(vertex3D.x, vertex3D.y);		

		// Skip point if they end up being too close. Box2d fires asserts if distance between neighbors is less than b2_linearSlop.
		if (outCount > 0 && b2DistanceSquared(*(outPoints-1), vertex2D) <= b2_linearSlop * b2_linearSlop)
			continue;

		*outPoints++ = vertex2D;
		++outCount;
	}

	return outCount;
}


bool PolygonColliderBase2D::ValidatePolygonShape(const b2Vec2* const points, const int pointCount)
{
	// Invalid polygon if the vertex count isn't in range.
	if (pointCount < 3 || pointCount > b2_maxPolygonVertices)
		return false;

	// Validate the polygon using the exact same code that Box2D uses.  This at least
	// ensures that we don't trigger any runtime asserts in Box2D.

	// Copy vertices into local buffer
	b2Vec2 ps[b2_maxPolygonVertices];
	for (int32 i = 0; i < pointCount; ++i)
		ps[i] = points[i];

	// Create the convex hull using the Gift wrapping algorithm
	// http://en.wikipedia.org/wiki/Gift_wrapping_algorithm

	// Find the right most point on the hull
	int32 i0 = 0;
	float32 x0 = ps[0].x;
	for (int32 i = 1; i < pointCount; ++i)
	{
		float32 x = ps[i].x;
		if (x > x0 || (x == x0 && ps[i].y < ps[i0].y))
		{
			i0 = i;
			x0 = x;
		}
	}

	int32 hull[b2_maxPolygonVertices];
	int32 validPointCount = 0;
	int32 ih = i0;

	for (;;)
	{
		hull[validPointCount] = ih;

		int32 ie = 0;
		for (int32 j = 1; j < pointCount; ++j)
		{
			if (ie == ih)
			{
				ie = j;
				continue;
			}

			b2Vec2 r = ps[ie] - ps[hull[validPointCount]];
			b2Vec2 v = ps[j] - ps[hull[validPointCount]];
			float32 c = b2Cross(r, v);
			if (c < 0.0f)
			{
				ie = j;
			}

			// Collinearity check
			if (c == 0.0f && v.LengthSquared() > r.LengthSquared())
			{
				ie = j;
			}
		}

		++validPointCount;
		ih = ie;

		if (ie == i0)
		{
			break;
		}
	}

	// Finish if invalid point count.
	if (validPointCount < 3)
		return false;


	// The following code is directly from Box2D.
	// Unfortunately Box2D simply asserts if the area inside the polygon is below a specific threshold when it
	// is calculating the centroid so using Box2Ds code and invalidating the polygon rather than throwing an assert is required.

	// Copy vertices.
	b2Vec2 vertices[b2_maxPolygonVertices];
	for (int32 i = 0; i < validPointCount; ++i)
	{
		vertices[i] = ps[hull[i]];
	}

	b2Vec2 c; c.Set(0.0f, 0.0f);
	float32 area = 0.0f;

	// pRef is the reference point for forming triangles.
	// It's location doesn't change the result (except for rounding error).
	b2Vec2 pRef(0.0f, 0.0f);

	const float32 inv3 = 1.0f / 3.0f;

	for (int32 i = 0; i < validPointCount; ++i)
	{
		// Triangle vertices.
		b2Vec2 p1 = pRef;
		b2Vec2 p2 = vertices[i];
		b2Vec2 p3 = i + 1 < validPointCount ? vertices[i+1] : vertices[0];

		b2Vec2 e1 = p2 - p1;
		b2Vec2 e2 = p3 - p1;

		float32 D = b2Cross(e1, e2);

		float32 triangleArea = 0.5f * D;
		area += triangleArea;

		// Area weighted centroid
		c += triangleArea * inv3 * (p1 + p2 + p3);
	}

	// Check for valid area.
	return IsFinite (area) && area > b2_epsilon;
}

#endif // #if ENABLE_2D_PHYSICS
