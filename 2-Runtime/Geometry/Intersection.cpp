#include "UnityPrefix.h"
#include "Intersection.h"
#include "Ray.h"
#include "Plane.h"
#include "Sphere.h"
#include "AABB.h"
#include "Runtime/Utilities/LogAssert.h"
#include "TriTriIntersect.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Camera/CullingParameters.h"


using namespace std;

bool IntersectRayTriangle (const Ray& ray, const Vector3f& a, const Vector3f& b, const Vector3f& c)
{
	float t;
	return IntersectRayTriangle (ray, a, b, c, &t);
}

bool IntersectRayTriangle (const Ray& ray, const Vector3f& a, const Vector3f& b, const Vector3f& c, float* outT)
{
	const float kMinDet = 1e-6f;

	float  t, u, v;
	Vector3f edge1, edge2, tvec, pvec, qvec;
	float det, inv_det;

	/* find vectors for two edges sharing vert0 */
	edge1 = b - a;
	edge2 = c - a;

	/* begin calculating determinant - also used to calculate U parameter */
	pvec = Cross (ray.GetDirection (), edge2);

	/* if determinant is near zero, ray lies in plane of triangle */
	det = Dot (edge1, pvec);

	if (Abs (det) < kMinDet)
		return false;

	inv_det = 1.0F / det;

	/* calculate distance from vert0 to ray origin */
	tvec = ray.GetOrigin () - a;

	/* calculate U parameter and test bounds */
	u = Dot (tvec, pvec) * inv_det;
	if (u < 0.0F || u > 1.0F)
		return false;

	/* prepare to test V parameter */
	qvec = Cross (tvec, edge1);

	/* calculate V parameter and test bounds */
	v = Dot (ray.GetDirection (), qvec) * inv_det;
	if (v < 0.0F || u + v > 1.0F)
		return false;
	
	t = Dot (edge2, qvec) * inv_det;
	if (t < 0.0F)
		return false;
	*outT = t;
	
	return true;
}

bool IntersectRaySphere (const Ray& ray, const Sphere& inSphere)
{
	Vector3f dif = inSphere.GetCenter () - ray.GetOrigin ();
	float d = Dot (dif, ray.GetDirection ());
	float lSqr = Dot (dif, dif);
	float rSqr = Sqr (inSphere.GetRadius ());
	
	if (d < 0.0F && lSqr > rSqr)
		return false;
	
	float mSqr = lSqr - Sqr (d);
	
	if (mSqr > rSqr)
		return false;
	else
		return true;
}
/*
bool IntersectRaySphere (const Ray& ray, const Sphere& inSphere, float* t)
{
	AssertIf (t == NULL);
	
	Vector3f dif = inSphere.GetCenter () - ray.GetOrigin ();
	float d = Dot (dif, ray.GetDirection ());
	float lSqr = Dot (dif, dif);
	float rSqr = Sqr (inSphere.GetRadius ());
	
	if (d < 0.0F && lSqr > rSqr)
		return false;
	
	float mSqr = lSqr - Sqr (d);
	
	if (mSqr > rSqr)
		return false;

	float q = sqrt (rSqr - mSqr);
	
	// ray.origin is inside the ray so a negative intersection will be returned
	*t = d - q;
	
	return true;
}
*/
bool IntersectRaySphere (const Ray& ray, const Sphere& inSphere, float* t0, float* t1)
{
	AssertIf (t0 == NULL);
	AssertIf (t1 == NULL);
	
	Vector3f dif = inSphere.GetCenter () - ray.GetOrigin ();
	float d = Dot (dif, ray.GetDirection ());
	float lSqr = Dot (dif, dif);
	float rSqr = Sqr (inSphere.GetRadius ());
	
	if (d < 0.0F && lSqr > rSqr)
		return false;

	float mSqr = lSqr - Sqr (d);
		
	if (mSqr > rSqr)
		return false;
		
	float q = sqrt (rSqr - mSqr);
	
	*t0 = d - q;
	*t1 = d + q;
	
	return true;
}

bool IntersectRayAABB (const Ray& ray, const AABB& inAABB)
{
	float t0, t1;
	return IntersectRayAABB (ray, inAABB, &t0, &t1);
}
/*
bool IntersectRayAABB (const Ray& ray, const AABB& inAABB, float* outT)
{
    float tmin = -Vector3f::infinity;
    float tmax = Vector3f::infinity;
	
	float t0, t1, f;
	
	Vector3f p = inAABB.GetCenter () - ray.GetOrigin ();
	Vector3f extent = inAABB.GetExtent ();
	long i;
	for (i=0;i<3;i++)
	{
		// ray and plane are paralell so no valid intersection can be found
		//if (Abs (ray.GetDirection ()[i]) > Vector3f::epsilon)
		{
			f = 1.0F / ray.GetDirection ()[i];
			t0 = (p[i] + extent[i]) * f;
			t1 = (p[i] - extent[i]) * f;
			// Ray leaves on Right, Top, Back Side
			if (t0 < t1)
			{
				if (t0 > tmin)
					tmin = t0;
				
				if (t1 < tmax)
					tmax = t1;
				
				if (tmin > tmax)
					return false;
				
				if (tmax < 0.0F)
					return false;
			}
			// Ray leaves on Left, Bottom, Front Side
			else
			{
				if (t1 > tmin)
					tmin = t1;
				
				if (t0 < tmax)
					tmax = t0;
				
				if (tmin > tmax)
					return false;
				
				if (tmax < 0.0F)
					return false;
			}
		}
	}
	
	if (tmin > 0.0F)
		*outT = tmin;
	// ray starts inside the aabb
	else
		*outT = 0.0F;
	
	AssertIf (*outT < 0.0F);
	return true;
}
*/

bool IntersectRayAABB (const Ray& ray, const AABB& inAABB, float* outT0)
{
	float t1;
	return IntersectRayAABB (ray, inAABB, outT0, &t1);
}

bool IntersectRayAABB (const Ray& ray, const AABB& inAABB, float* outT0, float* outT1)
{
	float tmin = -Vector3f::infinity;
	float tmax = Vector3f::infinity;
	
	float t0, t1, f;
	
	Vector3f p = inAABB.GetCenter () - ray.GetOrigin ();
	Vector3f extent = inAABB.GetExtent ();
	long i;
	for (i=0;i<3;i++)
	{
		// ray and plane are paralell so no valid intersection can be found
		{
			f = 1.0F / ray.GetDirection ()[i];
			t0 = (p[i] + extent[i]) * f;
			t1 = (p[i] - extent[i]) * f;
			// Ray leaves on Right, Top, Back Side
			if (t0 < t1)
			{
				if (t0 > tmin)
					tmin = t0;
				
				if (t1 < tmax)
					tmax = t1;
				
				if (tmin > tmax)
					return false;
				
				if (tmax < 0.0F)
					return false;
			}
			// Ray leaves on Left, Bottom, Front Side
			else
			{
				if (t1 > tmin)
					tmin = t1;
				
				if (t0 < tmax)
					tmax = t0;
				
				if (tmin > tmax)
					return false;
				
				if (tmax < 0.0F)
					return false;
			}
		}
	}
	
	*outT0 = tmin;
	*outT1 = tmax;
	
	return true;
}


bool IntersectSphereSphere (const Sphere& s0, const Sphere& s1)
{
	float sqrDist = SqrMagnitude (s0.GetCenter () - s1.GetCenter ());
	if (Sqr (s0.GetRadius () + s1.GetRadius ()) > sqrDist)
		return true;
	else
		return false;
}

bool IntersectSphereSphereInclusive (const Sphere& s0, const Sphere& s1)
{
	float sqrDist = SqrMagnitude (s0.GetCenter () - s1.GetCenter ());
	if (Sqr (s0.GetRadius () + s1.GetRadius ()) >= sqrDist)
		return true;
	else
		return false;
}

bool IntersectAABBAABB (const AABB& b0, const AABB& b1)
{
	const Vector3f dif = (b1.GetCenter () - b0.GetCenter ());
	
	return Abs (dif.x) < b0.GetExtent (0) + b1.GetExtent (0) 
		&& Abs (dif.y) < b0.GetExtent (1) + b1.GetExtent (1)
		&& Abs (dif.z) < b0.GetExtent (2) + b1.GetExtent (2);
}

bool IntersectAABBAABBInclusive (const AABB& b0, const AABB& b1)
{
	const Vector3f dif = (b1.GetCenter () - b0.GetCenter ());
	
	return Abs (dif.x) <= b0.GetExtent (0) + b1.GetExtent (0) 
		&& Abs (dif.y) <= b0.GetExtent (1) + b1.GetExtent (1)
		&& Abs (dif.z) <= b0.GetExtent (2) + b1.GetExtent (2);
}

bool IntersectAABBSphere (const AABB& aabb, const Sphere& s)
{
	return CalculateSqrDistance (s.GetCenter (), aabb) < Sqr (s.GetRadius ());
}

bool IntersectAABBSphereInclusive (const AABB& aabb, const Sphere& s)
{
	return CalculateSqrDistance (s.GetCenter (), aabb) <= Sqr (s.GetRadius ());
}

// possible optimization: Precalculate the Abs () of the plane. (3 fabs less per plane)
bool IntersectAABBFrustum (
	const AABB& a,
	const Plane* p, 
	UInt32 inClipMask)
{
	const Vector3f& m		= a.GetCenter ();// center of AABB
	const Vector3f& extent	= a.GetExtent ();// half-diagonal
	UInt32 mk		= 1;
	
	// loop while there are active planes..
	while (mk <= inClipMask)
	{
		// if clip plane is active...
		if (inClipMask & mk)
		{
			const Vector3f& normal = p->GetNormal ();
			float dist = p->GetDistanceToPoint (m);
			float radius = Dot (extent, Abs (normal));

			if (dist + radius < 0) return false; // behind clip plane
			//	if (dist - radius < 0) *outClipMask |= mk; // straddles clipplane
			// else in front of clip plane-> leave outClipMask bit off
			//			float m = (p->a () * b->v[p->nx].x) + (p->b * b->v[p->ny].y) + (p->c * b->v[p->nz].z);
			//			if (m > -p->d ()) return OUTSIDE;
			//			float r = Dot (m, normal) + p->d ();
			//			float n = (extent.x * Abs(normal.x)) + (extent.y * Abs(normal.y)) + (extent.z * Abs(normal.z));
			//			if (r + n < 0) return false;
		}
		mk += mk;
		p++; // next plane
	}
	return true; // AABB intersects frustum
}
// Optimize like this: http://www.cg.tuwien.ac.at/studentwork/CESCG/CESCG-2002/DSykoraJJelinek/

bool IntersectAABBFrustumFull (const AABB& a, const Plane p[6])
{
	return IntersectAABBPlaneBounds (a, p, 6);
}

bool IntersectAABBPlaneBounds (const AABB& a, const Plane* p, const int planeCount)
{
	const Vector3f& m		= a.GetCenter ();// center of AABB
	const Vector3f& extent	= a.GetExtent ();// half-diagonal

	for (int i = 0; i < planeCount; ++i, ++p)
	{
		const Vector3f& normal = p->GetNormal ();
		float dist = p->GetDistanceToPoint (m);
		float radius = Dot (extent, Abs (normal));
		if (dist + radius < 0) return false; // behind clip plane
	}
	return true; // AABB intersects space bounded by planes
}

// Returns the shortest distance to planes if point is outside (positive float),
// and 0.0 if point is inside frustum planes 
float PointDistanceToFrustum (const Vector4f& point, const Plane* p, const int planeCount)
{
	float maxDistanceNegative = -std::numeric_limits<float>::infinity();
	for (int i = 0; i < planeCount; ++i, ++p)
	{
		float dist = p->GetDistanceToPoint (point);
		if ((dist<0.0f) && (dist > maxDistanceNegative))
			maxDistanceNegative = dist;
	}
	if (maxDistanceNegative != -std::numeric_limits<float>::infinity())
		return -maxDistanceNegative;
	else
		return 0.0f;
}

bool IntersectTriTri (const Vector3f& a0, const Vector3f& b0, const Vector3f& c0,
					  const Vector3f& a1, const Vector3f& b1, const Vector3f& c1,
					  Vector3f* intersectionLine0, Vector3f* intersectionLine1, bool* coplanar)
{
	int coplanarInt;
	bool ret;
	ret = tri_tri_intersect_with_isectline (const_cast<Vector3f&> (a0).GetPtr (), const_cast<Vector3f&> (b0).GetPtr (), const_cast<Vector3f&> (c0).GetPtr (),
											const_cast<Vector3f&> (a1).GetPtr (), const_cast<Vector3f&> (b1).GetPtr (), const_cast<Vector3f&> (c1).GetPtr (),
											&coplanarInt, intersectionLine0->GetPtr (), intersectionLine1->GetPtr ());
	*coplanar = coplanarInt;
	return ret;
}

bool IntersectRayPlane (const Ray& ray, const Plane& plane, float* enter)
{
	AssertIf (enter == NULL);
	float vdot = Dot (ray.GetDirection (), plane.GetNormal ());
	float ndot = -Dot (ray.GetOrigin (), plane.GetNormal ()) - plane.d ();

	// is line parallel to the plane? if so, even if the line is
	// at the plane it is not considered as intersection because
	// it would be impossible to determine the point of intersection
	if ( CompareApproximately (vdot, 0.0F) )
		return false;

	// the resulting intersection is behind the origin of the ray
	// if the result is negative ( enter < 0 )
	*enter = ndot / vdot;
	
	return *enter > 0.0F;
}

bool IntersectSegmentPlane( const Vector3f& p1, const Vector3f& p2, const Plane& plane, Vector3f* result )
{
	AssertIf( result == NULL );
	Vector3f vec = p2 - p1;
	float vdot = Dot( vec, plane.GetNormal() );

	// segment parallel to the plane
	if( CompareApproximately(vdot, 0.0f) )
		return false;

	float ndot = -Dot( p1, plane.GetNormal() ) - plane.d();
	float u = ndot / vdot;
	// intersection is out of segment
	if( u < 0.0f || u > 1.0f )
		return false;

	*result = p1 + vec * u;
	return true;
}

/*
bool IntersectSphereTriangle (const Sphere& s, const Vector3f& vert0, const Vector3f& vert1, const Vector3f& vert2)
{
	const Vector3f& center = s.GetCenter ();
	float radius = s.GetRadius ();
	float radius2 = radius * radius;
	// Early exit if one of the vertices is inside the sphere
	Vector3f kDiff = vert2 - center;
	float fC = SqrMagnitude (kDiff);
	if(fC <= radius2)	return true;

	kDiff = vert1 - center;
	fC = SqrMagnitude (kDiff);
	if(fC <= radius2)	return true;

	kDiff = vert0 - center;
	fC = SqrMagnitude (kDiff);
	if(fC <= radius2)	return true;

	// Else do the full distance test
	Vector3f TriEdge0	= vert1 - vert0;
	Vector3f TriEdge1	= vert2 - vert0;

	float fA00	= SqrMagnitude (TriEdge0);
	float fA01	= Dot (TriEdge0, TriEdge1);
	float fA11	= SqrMagnitude (TriEdge1);
	float fB0	= Dot (kDiff, TriEdge0);
	float fB1	= Dot (kDiff, TriEdge0);

	float fDet	= Abs(fA00*fA11 - fA01*fA01);
	float u		= fA01*fB1-fA11*fB0;
	float v		= fA01*fB0-fA00*fB1;
	float SqrDist;

	if(u + v <= fDet)
	{
		if(u < 0.0f)
		{
			if(v < 0.0f)  // region 4
			{
				if(fB0 < 0.0f)
				{
					if(-fB0>=fA00)			{                   SqrDist = fA00+2.0f*fB0+fC;	}
					else					{ u = -fB0/fA00;	SqrDist = fB0*u+fC;			}
				}
				else
				{
					if(fB1>=0.0f)			{                   SqrDist = fC;				}
					else if(-fB1>=fA11)		{                   SqrDist = fA11+2.0f*fB1+fC;	}
					else					{ v = -fB1/fA11;	SqrDist = fB1*v+fC;			}
				}
			}
			else  // region 3
			{
				if(fB1>=0.0f)				{                   SqrDist = fC;				}
				else if(-fB1>=fA11)			{                   SqrDist = fA11+2.0f*fB1+fC;	}
				else						{ v = -fB1/fA11;	SqrDist = fB1*v+fC;			}
			}
		}
		else if(v < 0.0f)  // region 5
		{
			if(fB0>=0.0f)					{                   SqrDist = fC;				}
			else if(-fB0>=fA00)				{                   SqrDist = fA00+2.0f*fB0+fC;	}
			else							{ u = -fB0/fA00;	SqrDist = fB0*u+fC;			}
		}
		else  // region 0
		{
			// minimum at interior point
			if(fDet==0.0f)
			{
				SqrDist = std::numeric_limits<float>::max ();
			}
			else
			{
				float fInvDet = 1.0f/fDet;
				u *= fInvDet;
				v *= fInvDet;
				SqrDist = u*(fA00*u+fA01*v+2.0f*fB0) + v*(fA01*u+fA11*v+2.0f*fB1)+fC;
			}
		}
	}
	else
	{
		float fTmp0, fTmp1, fNumer, fDenom;

		if(u < 0.0f)  // region 2
		{
			fTmp0 = fA01 + fB0;
			fTmp1 = fA11 + fB1;
			if(fTmp1 > fTmp0)
			{
				fNumer = fTmp1 - fTmp0;
				fDenom = fA00-2.0f*fA01+fA11;
				if(fNumer >= fDenom)
				{
//					u = 1.0f;
//					v = 0.0f;
					SqrDist = fA00+2.0f*fB0+fC;
				}
				else
				{
					u = fNumer/fDenom;
					v = 1.0f - u;
					SqrDist = u*(fA00*u+fA01*v+2.0f*fB0) + v*(fA01*u+fA11*v+2.0f*fB1)+fC;
				}
			}
			else
			{
//				u = 0.0f;
				if(fTmp1 <= 0.0f)		{                   SqrDist = fA11+2.0f*fB1+fC;	}
				else if(fB1 >= 0.0f)	{                   SqrDist = fC;				}
				else					{ v = -fB1/fA11;	SqrDist = fB1*v+fC;			}
			}
		}
		else if(v < 0.0f)  // region 6
		{
			fTmp0 = fA01 + fB1;
			fTmp1 = fA00 + fB0;
			if(fTmp1 > fTmp0)
			{
				fNumer = fTmp1 - fTmp0;
				fDenom = fA00-2.0f*fA01+fA11;
				if(fNumer >= fDenom)
				{
					SqrDist = fA11+2.0f*fB1+fC;
				}
				else
				{
					v = fNumer/fDenom;
					u = 1.0f - v;
					SqrDist = u*(fA00*u+fA01*v+2.0f*fB0) + v*(fA01*u+fA11*v+2.0f*fB1)+fC;
				}
			}
			else
			{
				if(fTmp1 <= 0.0f)		{                   SqrDist = fA00+2.0f*fB0+fC;	}
				else if(fB0 >= 0.0f)	{                   SqrDist = fC;				}
				else					{ u = -fB0/fA00;	SqrDist = fB0*u+fC;			}
			}
		}
		else  // region 1
		{
			fNumer = fA11 + fB1 - fA01 - fB0;
			if(fNumer <= 0.0f)
			{
				SqrDist = fA11+2.0f*fB1+fC;
			}
			else
			{
				fDenom = fA00-2.0f*fA01+fA11;
				if(fNumer >= fDenom)
				{
					SqrDist = fA00+2.0f*fB0+fC;
				}
				else
				{
					u = fNumer/fDenom;
					v = 1.0f - u;
					SqrDist = u*(fA00*u+fA01*v+2.0f*fB0) + v*(fA01*u+fA11*v+2.0f*fB1)+fC;
				}
			}
		}
	}

	return Abs (SqrDist) < radius2;
}*/

bool IntersectSphereTriangle (const Sphere& s, const Vector3f& vert0, const Vector3f& vert1, const Vector3f& vert2)
{
	const Vector3f& center = s.GetCenter ();
	float radius = s.GetRadius ();
	float radius2 = radius * radius;
	Vector3f Diff;

	// Early exit if one of the vertices is inside the sphere
	float sqrDiff;
	Diff = vert1 - center;
	sqrDiff = SqrMagnitude (Diff);
	if(sqrDiff <= radius2)	return true;

	Diff = vert2 - center;
	sqrDiff = SqrMagnitude (Diff);
	if(sqrDiff <= radius2)	return true;

	Diff = vert0 - center;
	sqrDiff = SqrMagnitude (Diff);
	if(sqrDiff <= radius2)	return true;

	// Else do the full distance test
	Vector3f Edge0	= vert1 - vert0;
	Vector3f Edge1	= vert2 - vert0;
	
	float A00 = Dot (Edge0, Edge0);
	float A01 = Dot (Edge0, Edge1);
	float A11 = Dot (Edge1, Edge1);

	float B0 = Dot (Diff, Edge0);
	float B1 = Dot (Diff, Edge1);

	float C = Dot (Diff, Diff);

	float Det = Abs (A00 * A11 - A01 * A01);
	float u = A01 * B1 - A11 * B0;
	float v = A01 * B0 - A00 * B1;

	float DistSq;
	if (u + v <= Det)
	{
		if(u < 0.0F)
		{
			if(v < 0.0F)
			{
			  // region 4
				if(B0 < 0.0F)
				{
					if (-B0 >= A00)
						DistSq = A00 + 2.0F * B0 + C;
					else
					{
						u = -B0 / A00;
						DistSq = B0 * u + C;
					}
				}
				else{
					if(B1 >= 0.0F)
						DistSq = C;
					else if(-B1 >= A11)
						DistSq = A11 + 2.0F * B1 + C;
					else
					{
						v = -B1 / A11;
						DistSq = B1 * v + C;
					}
				}
			}
			else
			{  // region 3
				if(B1 >= 0.0F)
					DistSq = C;
				else if(-B1 >= A11)
					DistSq = A11 + 2.0F * B1 + C;
				else
				{
					v = -B1 / A11;
					DistSq = B1 * v + C;
				}
			}
		}
		else if(v < 0.0F)
		{  // region 5
			if (B0 >= 0.0F)
				DistSq = C;
			else if (-B0 >= A00)
				DistSq = A00 + 2.0F * B0 + C;
			else
			{
				u = -B0 / A00;
				DistSq = B0 * u + C;
			}
		}
		else
		{  // region 0
			// minimum at interior point
			if (Det == 0.0F)
				DistSq = std::numeric_limits<float>::max ();
			else
			{
				float InvDet = 1.0F / Det;
				u *= InvDet;
				v *= InvDet;
				DistSq = u * (A00 * u + A01 * v + 2.0F * B0) + v * (A01 * u + A11 * v + 2.0F * B1) + C;
			}
		}
	}
	else{
		double Tmp0, Tmp1, Numer, Denom;

		if(u < 0.0F)
		{  
			// region 2
			Tmp0 = A01 + B0;
			Tmp1 = A11 + B1;
			if (Tmp1 > Tmp0){
				Numer = Tmp1 - Tmp0;
				Denom = A00 - 2.0F * A01 + A11;
				if (Numer >= Denom)
					DistSq = A00 + 2.0F * B0 + C;
				else
				{
					u = Numer / Denom;
					v = 1.0 - u;
					DistSq = u * (A00 * u + A01 * v + 2.0F * B0) + v * (A01 * u + A11 * v + 2.0F * B1) + C;
				}
			}
			else
			{
				if(Tmp1 <= 0.0F)
					DistSq = A11 + 2.0F * B1 + C;
				else if(B1 >= 0.0)
					DistSq = C;
				else
				{
					v = -B1 / A11;
					DistSq = B1 * v + C;
				}
			}
		}
		else if(v < 0.0)
		{  // region 6
			Tmp0 = A01 + B1;
			Tmp1 = A00 + B0;
			if (Tmp1 > Tmp0)
			{
				Numer = Tmp1 - Tmp0;
				Denom = A00 - 2.0F * A01 + A11;
				if (Numer >= Denom)
					DistSq = A11 + 2.0 * B1 + C;
				else
				{
					v = Numer / Denom;
					u = 1.0F - v;
					DistSq =  u * (A00 * u + A01 * v + 2.0F * B0) + v * (A01 * u + A11 * v + 2.0F * B1) + C;
				}
			}
			else
			{
				if (Tmp1 <= 0.0F)
					DistSq = A00 + 2.0F * B0 + C;
				else if(B0 >= 0.0F)
					DistSq = C;
				else
				{
					u = -B0 / A00;
					DistSq = B0 * u + C;
				}
			}
		}
		else
		{
		  // region 1
			Numer = A11 + B1 - A01 - B0;
			if (Numer <= 0.0F)
				DistSq = A11 + 2.0F * B1 + C;
			else
			{
				Denom = A00 - 2.0F * A01 + A11;
				if (Numer >= Denom)
					DistSq = A00 + 2.0F * B0 + C;
				else
				{
					u = Numer / Denom;
					v = 1.0F - u;
					DistSq = u * (A00 * u + A01 * v + 2.0F * B0) + v * (A01 * u + A11 * v + 2.0F * B1) + C;
				}
			}
		}
	}

	return Abs (DistSq) <= radius2;
}

bool TestPlanesAABB(const Plane* planes, const int planeCount, const AABB& bounds)
{
	UInt32 planeMask = 0;
	if (planeCount == 6)
		planeMask = 63;
	else
	{
		for (int i = 0; i < planeCount; ++i)
			planeMask |= 1 << i;
	}

	return IntersectAABBFrustum (bounds, planes, planeMask);
}

// --------------------------------------------------------------------------

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Profiler/TimeHelper.h"

void GenerateUnitFrustumPlanes (Plane* p)
{
	p[0].SetABCD(-1.0, 0.0, 0.0,-1.0);
	p[1].SetABCD( 1.0, 0.0, 0.0, 1.0);
	p[2].SetABCD( 0.0,-1.0, 0.0,-1.0);
	p[3].SetABCD( 0.0, 1.0, 0.0, 1.0);
	p[4].SetABCD( 0.0, 0.0,-1.0,-1.0);
	p[5].SetABCD( 0.0, 0.0, 1.0, 1.0);
}

float PointDistanceToFrustumRef (const Vector3f& point, const Plane* p, const int planeCount)
{
	DebugAssert(planeCount <= 6);
	
	float maxDistanceNegative = -std::numeric_limits<float>::infinity();
	float distances[6];

	// Point distances to frustum planes
	for (int i=0; i<planeCount; i++)
		distances[i] = p[i].GetDistanceToPoint (point);

	// Replace positive distances with negative infinity. This simplifies the shortest negative distance search to maximum operators
	for (int i=0; i<planeCount; i++)
		distances[i] = (distances[i] > 0.0f) ? -std::numeric_limits<float>::infinity() : distances[i];
		
	// Find the shortest negative distance from the distance values
	for (int i=0; i<planeCount; i++)
		maxDistanceNegative = (distances[i] > maxDistanceNegative) ? distances[i] : maxDistanceNegative;

	// If maxNegativeDistance is negative infinity, all distance were positive and the point is inside the frustum. In that case, return 0.0, otherwise return the shortest distance (abs value)
	return (maxDistanceNegative == -std::numeric_limits<float>::infinity()) ? 0.0f : -maxDistanceNegative;
}

void TestComparePerformancePointDistanceToFrustum ();

SUITE (IntersectionTests)
{
	TEST(PointDistanceToFrustum)
	{
		Plane unitFrustumPlanes[6];
		GenerateUnitFrustumPlanes(unitFrustumPlanes);

		Rand r (1);	// fixed seed produces fixed random series

		for (int i=0; i<1000; i++)
		{
			// Random coordinates (2x unit frustum volume)
			float x = (r.GetFloat() - 0.5f) * 3.0f;
			float y = (r.GetFloat() - 0.5f) * 3.0f;
			float z = (r.GetFloat() - 0.5f) * 3.0f;
			Vector3f point3f(x,y,z);  
			Vector4f point4f(x,y,z,0.0f);

			float distanceRef = PointDistanceToFrustumRef(point3f, unitFrustumPlanes, 6);
			float distance = PointDistanceToFrustum(point4f, unitFrustumPlanes, 6);
		
			if (distanceRef > 0.0F)
			{
				CHECK_EQUAL(distance, distanceRef);
			}
			else
			{
				CHECK(distance <= 0.0F);
			}	
		}
	}
} // SUITE

static void TestPerformancePointDistanceToFrustum ()
{
	Plane unitFrustumPlanes[6];
	GenerateUnitFrustumPlanes(unitFrustumPlanes);
	
	Rand r (1);	// fixed seed produces fixed random series
	float x = (r.GetFloat() - 0.5f) * 2.0f;
	float y = (r.GetFloat() - 0.5f) * 2.0f;
	float z = (r.GetFloat() - 0.5f) * 2.0f;
	Vector4f pointFloat4(x,y,z,0.0f);
	float distanceSum = 0.0f;
	
	ABSOLUTE_TIME time = START_TIME;
	
	for (int i=0; i<1000000; i++)
	{
		distanceSum += PointDistanceToFrustum(pointFloat4, unitFrustumPlanes, 6);
	}
	
	printf_console("\n\nTime Impl: %f ms %f\n\n", GetElapsedTimeInSeconds (time) * 1000.0F, distanceSum);
}

static void TestPerformancePointDistanceToFrustumRef ()
{
	Plane unitFrustumPlanes[6];
	GenerateUnitFrustumPlanes(unitFrustumPlanes);
	
	Rand r (1);	// fixed seed produces fixed random series
	float x = (r.GetFloat() - 0.5f) * 2.0f;
	float y = (r.GetFloat() - 0.5f) * 2.0f;
	float z = (r.GetFloat() - 0.5f) * 2.0f;
	Vector3f point3f(x,y,z);
	float distanceSum = 0.0f;
	
	ABSOLUTE_TIME time = START_TIME;
	
	for (int i=0; i<1000000; i++)
	{
		distanceSum += PointDistanceToFrustumRef(point3f, unitFrustumPlanes, 6);
	}
	
	printf_console("\n\nTime REF: %f (%f)ms\n\n", GetElapsedTimeInSeconds (time) * 1000.0F, distanceSum);
}

void TestComparePerformancePointDistanceToFrustum ()
{
	TestPerformancePointDistanceToFrustum ();
	TestPerformancePointDistanceToFrustumRef ();
}

#endif // ENABLE_UNIT_TESTS