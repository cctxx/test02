#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Geometry/Sphere.h"
#include "Runtime/Geometry/Ray.h"


SUITE (IntersectionTests)
{


// AABB Ray (ray inside aabb)
TEST (RayInsideAABB)
{
	AABB aabb;
	Ray ray;
	float t0, t1;
	bool result;

	aabb.GetCenter () = Vector3f (5.0F, 10.0F, 20.0F);
	aabb.GetExtent () = Vector3f (5.0F, 10.0F, 20.0F);
	
	ray.SetOrigin (aabb.GetCenter ());
	ray.SetDirection (Vector3f::zAxis);
		
	CHECK (IntersectRayAABB (ray, aabb));
	
	result = IntersectRayAABB (ray, aabb, &t0, &t1);
	CHECK (result);
	CHECK_CLOSE (t0, -20.0F, 0.000001F);
	CHECK_CLOSE (t1, 20.0F, 0.000001F);

	ray.SetDirection (-Vector3f::zAxis);
	result = IntersectRayAABB (ray, aabb, &t0, &t1);
	CHECK (result);
	CHECK_CLOSE (t0, -20.0F, 0.000001F);
	CHECK_CLOSE (t1, 20.0F, 0.000001F);
}


// AABB Ray (ray doesn't hit)
TEST (RayOutsideAABB)
{
	AABB aabb;
	Ray ray;
	float t0, t1;

	aabb.GetCenter () = Vector3f (5.0F, 10.0F, 20.0F);
	aabb.GetExtent () = Vector3f (5.0F, 10.0F, 20.0F);
	
	ray.SetOrigin (aabb.GetCenter () + Vector3f (5.0F, 10.0F, 20.01F));
	ray.SetDirection (Vector3f::zAxis);

	CHECK (!IntersectRayAABB (ray, aabb));
	CHECK (!IntersectRayAABB (ray, aabb, &t0, &t1));
}


// AABB Ray (ray hits frontal)
TEST (RayHitsAABBFrontal)
{
	AABB aabb;
	Ray ray;
	float t0, t1;
	bool result;

	aabb.GetCenter () = Vector3f (5.0F, 10.0F, 20.0F);
	aabb.GetExtent () = Vector3f (5.0F, 10.0F, 20.0F);
	
	ray.SetOrigin (Vector3f (5.0F, 10.0F, 60.0F));
	ray.SetDirection (-Vector3f::zAxis);
		
	CHECK (IntersectRayAABB (ray, aabb));
	
	result = IntersectRayAABB (ray, aabb, &t0, &t1);
	CHECK (result);
	CHECK_CLOSE (t0, 20.0F, 0.000001F);
	CHECK_CLOSE (t1, 60.0F, 0.000001F);
}


// AABB Ray (ray hits backward)
TEST (RayHitsAABBBackward)
{
	AABB aabb;
	Ray ray;
	float t0, t1;

	aabb.GetCenter () = Vector3f (5.0F, 10.0F, 20.0F);
	aabb.GetExtent () = Vector3f (5.0F, 10.0F, 20.0F);
	
	ray.SetOrigin (Vector3f (5.0F, 10.0F, 60.0F));
	ray.SetDirection (Vector3f (0.0F, 0.0F, 1.0F));
		
	CHECK (!IntersectRayAABB (ray, aabb));
	CHECK (!IntersectRayAABB (ray, aabb, &t0, &t1));
}


// Sphere Ray (ray inside) (sphere.origin in front of ray.origin)
TEST (SphereToRay1)
{
	Sphere sphere;
	Ray ray;
	float t0, t1;
	bool result;

	sphere.GetCenter () = Vector3f (5.0F, 10.0F, 20.0F);
	sphere.GetRadius () = 10.0F;
	
	ray.SetOrigin (Vector3f (5.0F, 10.0F, 25.0F));
	ray.SetDirection (Vector3f (0.0F, 0.0F, 1.0F));
	
	CHECK (IntersectRaySphere (ray, sphere));
	
	result = IntersectRaySphere (ray, sphere, &t0, &t1);
	CHECK (result);
	CHECK_CLOSE (t0, -15.0F, 0.000001F);
	CHECK_CLOSE (t1, 5.0F, 0.000001F);
}


// Sphere Ray (ray inside) (sphere.origin in front of ray.origin)
TEST (SphereRay2)
{
	Sphere sphere;
	Ray ray;
	float t0, t1;
	bool result;

	sphere.GetCenter () = Vector3f (5.0F, 10.0F, 20.0F);
	sphere.GetRadius () = 10.0F;
	
	ray.SetOrigin (Vector3f (5.0F, 10.0F, 25.0F));
	ray.SetDirection (Vector3f (0.0F, 0.0F, -1.0F));
	
	CHECK (IntersectRaySphere (ray, sphere));
	
	result = IntersectRaySphere (ray, sphere, &t0, &t1);
	CHECK (result);
	CHECK_CLOSE (t0, -5.0F, 0.000001F);
	CHECK_CLOSE (t1, 15.0F, 0.000001F);
}


// Sphere Ray (hits sphere frontal)
TEST (RayHitsSphereFrontal)
{
	Sphere sphere;
	Ray ray;
	float t0, t1;
	bool result;

	sphere.GetCenter () = Vector3f (5.0F, 10.0F, 20.0F);
	sphere.GetRadius () = 10.0F;
	
	ray.SetOrigin (Vector3f (5.0F, 10.0F, 0.0F));
	ray.SetDirection (Vector3f (0.0F, 0.0F, 1.0F));
	
	CHECK (IntersectRaySphere (ray, sphere));
	
	result = IntersectRaySphere (ray, sphere, &t0, &t1);
	CHECK (result);
	CHECK_CLOSE (t0, 10.0F, 0.000001F);
	CHECK_CLOSE (t1, 30.0F, 0.000001F);
}


// Sphere Ray (hits sphere backwards)
TEST (RayHitsSphereBackwards)
{
	Sphere sphere;
	Ray ray;
	float t0, t1;
	bool result;

	sphere.GetCenter () = Vector3f (5.0F, 10.0F, 20.0F);
	sphere.GetRadius () = 10.0F;
	
	ray.SetOrigin (Vector3f (5.0F, 10.0F, 40.0F));
	ray.SetDirection (Vector3f (0.0F, 0.0F, 1.0F));
	
	CHECK (!IntersectRaySphere (ray, sphere));
	
	result = IntersectRaySphere (ray, sphere, &t0, &t1);
	CHECK (!result);
	ErrorIf (result != false);
}


// Sphere Ray (misses sphere)
TEST (RayMissesSphere)
{
	Sphere sphere;
	Ray ray;
	float t0, t1;

	sphere.GetCenter () = Vector3f (5.0F, 10.0F, 20.0F);
	sphere.GetRadius () = 10.0F;
	
	ray.SetOrigin (Vector3f (5.0F, 10.0F, 30.01F));
	ray.SetDirection (Vector3f (0.0F, 1.0F, 0.0F));
	
	CHECK (!IntersectRaySphere (ray, sphere));
	CHECK (!IntersectRaySphere (ray, sphere, &t0, &t1));
}
	

// Triangle Triangle (Not intersecting)
TEST (TriangleTriangleNotIntersecting)
{
	Vector3f
		a1 (0, 0, 0),
		a2 (1, 1, 0),
		a3 (2, 0, 0),
		b1 (0, 0, 1),
		b2 (1, 1, 1),
		b3 (2, 0, 1);
	Vector3f r1, r2;
	bool coplanar;

	CHECK (!IntersectTriTri (a1, a2, a3, b1, b2, b3, &r1, &r2, &coplanar));
}


// Triangle Triangle (intersecting)
TEST (TriangleTriangleIntersecting)
{
	Vector3f
		a1 (0, 2, 5),
		a2 (2, 2, 0),
		a3 (0, 2, 0),
		b1 (0, 0, 0),
		b2 (0, 5, 0),
		b3 (0, 5, 3);
	Vector3f r1, r2;
	bool coplanar;

	CHECK (IntersectTriTri (a1, a2, a3, b1, b2, b3, &r1, &r2, &coplanar));
	CHECK (CompareApproximately (r1, Vector3f (0, 2, 0)));
	CHECK (CompareApproximately (r2, Vector3f (0, 2, 1.2f)));
	CHECK_EQUAL (false, coplanar);
}


// Triangle Triangle (coplanaer)
TEST (TriangleTriangleCoplanar)
{
	Vector3f
		a1 (0, 8, 0),
		a2 (0, 4, 0),
		a3 (5, 4, 0),
		b1 (0, 5, 0),
		b2 (5, 0, 0),
		b3 (0, 0, 0);
	Vector3f r1, r2;
	bool coplanar;

	CHECK (IntersectTriTri (a1, a2, a3, b1, b2, b3, &r1, &r2, &coplanar));
	CHECK (coplanar);
}	


TEST (Misc)
{
	CHECK (IntersectSphereTriangle (
		Sphere (Vector3f (0.3F, 0.3F, 0.1F), .2F),
		Vector3f (0.0F, 0.0F, 0.0F),
		Vector3f (0.0F, 1.0F, 0.0F),
		Vector3f (1.0F, 0.0F, 0.0F)));
	CHECK (IntersectSphereTriangle (
		Sphere (Vector3f (0.3F, 0.3F, 0.0F), .2F),
		Vector3f (0.0F, 0.0F, 0.0F),
		Vector3f (0.0F, 1.0F, 0.0F),
		Vector3f (1.0F, 0.0F, 0.0F)));
	CHECK (!CompareApproximately (0.01f, 0.0));
}


}


#endif
