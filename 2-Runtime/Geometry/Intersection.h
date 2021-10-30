#ifndef INTERSECTION_H
#define INTERSECTION_H

#include "Runtime/Math/Simd/math.h"

class Ray;
class OptimizedRay;
class Sphere;
class AABB;
class Plane;

class Vector3f;
class Vector4f;

// Intersects a Ray with a triangle.
bool IntersectRayTriangle (const Ray& ray, const Vector3f& a, const Vector3f& b, const Vector3f& c);
// t is to be non-Null and returns the first intersection point of the ray (ray.o + t * ray.dir)
bool IntersectRayTriangle (const Ray& ray, const Vector3f& a, const Vector3f& b, const Vector3f& c, float* t);

// Intersects a ray with a volume.
// Returns true if the ray stats inside the volume or in front of the volume
bool IntersectRaySphere (const Ray& ray, const Sphere& inSphere);
bool IntersectRayAABB (const Ray& ray, const AABB& inAABB);

// Intersects a ray with a volume.
// Returns true if the ray stats inside the volume or in front of the volume
// t0 is the first, t1 the second intersection. Both have to be non-NULL.
// (t1 is always positive, t0 is negative if the ray starts inside the volume)
bool IntersectRayAABB (const Ray& ray, const AABB& inAABB, float* t0, float* t1);
bool IntersectRayAABB (const Ray& ray, const AABB& inAABB, float* t0);
bool IntersectRaySphere (const Ray& ray, const Sphere& inSphere, float* t0, float* t1);

// Do these volumes intersect each other?
bool IntersectSphereSphere (const Sphere& s0, const Sphere& s1);
bool IntersectAABBAABB (const AABB& s0, const AABB& s1);
bool IntersectAABBSphere (const AABB& s0, const Sphere& s1);

// Do these volumes intersect or touch each other?
bool IntersectSphereSphereInclusive (const Sphere& s0, const Sphere& s1);
bool EXPORT_COREMODULE IntersectAABBAABBInclusive (const AABB& s0, const AABB& s1);
bool IntersectAABBSphereInclusive (const AABB& s0, const Sphere& s1);

// Tests if the aabb is inside any of the planes enabled by inClipMask
// The bitmask tells which planes have to be tested. (For 6 planes the bitmask is 63)
bool IntersectAABBFrustum (const AABB& a, const Plane* p, UInt32 inClipMask);
bool IntersectAABBFrustumFull (const AABB& a, const Plane p[6]);
bool IntersectAABBPlaneBounds (const AABB& a, const Plane* p, const int planeCount);

float PointDistanceToFrustum (const Vector4f& point, const Plane* p, const int planeCount);

bool IntersectTriTri (const Vector3f& a0, const Vector3f& b0, const Vector3f& c0,
					  const Vector3f& a1, const Vector3f& b1, const Vector3f& c1,
					  Vector3f* intersectionLine0, Vector3f* intersectionLine1, bool* coplanar);
					  
// Intersects a ray with a plane (The ray can hit the plane from front and behind)
// On return enter is the rays parameter where the intersection occurred.
bool IntersectRayPlane (const Ray& ray, const Plane& plane, float* enter);


// Intersects a line segment with a plane (can hit the plane from front and behind)
// Fill result point if intersected.
bool IntersectSegmentPlane( const Vector3f& p1, const Vector3f& p2, const Plane& plane, Vector3f* result );


// Returns true if the triangle touches or is inside the triangle (a, b, c)
bool IntersectSphereTriangle (const Sphere& s, const Vector3f& a, const Vector3f& b, const Vector3f& c);

/// Returns true if the bounding box is inside the planes or intersects any of the planes.
bool TestPlanesAABB(const Plane* planes, const int planeCount, const AABB& bounds);

/// Projects point on a line.
template <typename T>
T ProjectPointLine(const T& point, const T& lineStart, const T& lineEnd)
{
	T relativePoint = point - lineStart;
	T lineDirection = lineEnd - lineStart;
	float length = Magnitude(lineDirection);
	T normalizedLineDirection = lineDirection;
	if (length > T::epsilon)
		normalizedLineDirection /= length;

	float dot = Dot(normalizedLineDirection, relativePoint);
	dot = clamp(dot, 0.0F, length);

	return lineStart + normalizedLineDirection * dot;
}

/// Returns the distance to a line from a point.
template <typename T>
float DistancePointLine(const T& point, const T& lineStart, const T& lineEnd)
{
	return Magnitude(ProjectPointLine<T>(point, lineStart, lineEnd) - point);
}

#endif
