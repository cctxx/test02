#ifndef PLANE_H
#define PLANE_H

#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Serialize/SerializeUtility.h"

// Calculates the unnormalized normal from 3 points 
Vector3f CalcRawNormalFromTriangle (const Vector3f& a, const Vector3f& b, const Vector3f& c);

enum{
	kPlaneFrustumLeft,
	kPlaneFrustumRight,
	kPlaneFrustumBottom,
	kPlaneFrustumTop,
	kPlaneFrustumNear,
	kPlaneFrustumFar,
	kPlaneFrustumNum,
};

class Plane
{
public:

	Vector3f normal;
	float distance;

	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (Plane)

	const float& a ()const  			{ return normal.x; }
	const float& b ()const				{ return normal.y; }
	const float& c ()const				{ return normal.z; }

	const float& d () const { return distance; }
	float& d () { return distance; }

	const Vector3f& GetNormal ()const{ return normal; }
		
	Plane () { }
	Plane (float a, float b, float c, float d) { normal.x = a; normal.y = b; normal.z = c; distance = d; }
	
	Plane& operator *= (float scale);
	bool operator == (const Plane& p)const		{ return normal == p.normal && distance == p.distance; }
	bool operator != (const Plane& p)const		{ return normal != p.normal || distance != p.distance; }
	
	void SetInvalid () { normal = Vector3f::zero; distance = 0.0F; }

	// Just sets the coefficients. Does NOT normalize!
	void SetABCD (const float a, const float b, const float c, const float d);

	void Set3Points (const Vector3f& a, const Vector3f& b, const Vector3f& c);
	void Set3PointsUnnormalized (const Vector3f& a, const Vector3f& b, const Vector3f& c);
	
	void SetNormalAndPosition (const Vector3f& inNormal, const Vector3f& inPoint);
	
	float GetDistanceToPoint (const Vector3f& inPt) const;
	float GetDistanceToPoint (const Vector4f& inPt) const;
	bool GetSide (const Vector3f& inPt) const;
	bool SameSide (const Vector3f& inPt0, const Vector3f& inPt1);

	void NormalizeRobust ();
	void NormalizeUnsafe ();
};

template<class TransferFunction>
inline void Plane::Transfer (TransferFunction& transfer)
{
	TRANSFER (normal);
	TRANSFER (distance);
}

inline float Plane::GetDistanceToPoint (const Vector3f& inPt)const
{
	DebugAssert (IsNormalized (normal));
	return Dot (normal, inPt) + distance;
}

// inPt w component is ignored from distance computations
inline float Plane::GetDistanceToPoint (const Vector4f& inPt)const
{
	DebugAssert (IsNormalized (normal));
	//Dot3 (normal, inPt) + distance;
	return normal.x * inPt.x + normal.y * inPt.y + normal.z * inPt.z + distance;
}

// Returns true if we are on the front side (same as: GetDistanceToPoint () > 0.0)
inline bool Plane::GetSide (const Vector3f& inPt) const
{
	return Dot (normal, inPt) + distance > 0.0F;
}


// Calculates the normal from 3 points unnormalized
inline Vector3f CalcRawNormalFromTriangle (const Vector3f& a, const Vector3f& b, const Vector3f& c)
{
	return Cross (b - a, c - a);
}

inline Vector3f RobustNormalFromTriangle (const Vector3f& v0, const Vector3f& v1, const Vector3f& v2)
{
	Vector3f normal = CalcRawNormalFromTriangle (v0, v1, v2);
	return NormalizeRobust (normal);
}

inline void Plane::SetABCD (float a, float b, float c, float d)
{
	normal.Set(a, b, c);
	distance = d;
}

inline void Plane::Set3Points (const Vector3f& a, const Vector3f& b, const Vector3f& c)
{
	normal = CalcRawNormalFromTriangle (a, b, c);
	normal = ::Normalize (normal);
	distance = -Dot (normal, a);
	AssertIf (!IsNormalized (normal));
}

inline void Plane::Set3PointsUnnormalized (const Vector3f& a, const Vector3f& b, const Vector3f& c)
{
	normal = CalcRawNormalFromTriangle (a, b, c);
	distance = -Dot (normal, a);
}

inline void Plane::SetNormalAndPosition (const Vector3f& inNormal, const Vector3f& inPoint)
{
	normal = inNormal;
	AssertIf (!IsNormalized (normal, 0.001f));
	distance = -Dot (inNormal, inPoint);
}

inline bool Plane::SameSide (const Vector3f& inPt0, const Vector3f& inPt1)
{
	float d0 = GetDistanceToPoint(inPt0);
	float d1 = GetDistanceToPoint(inPt1);
	if (d0 > 0.0f && d1 > 0.0f)
		return true;
	else if (d0 <= 0.0f && d1 <= 0.0f)
		return true;
	else
		return false;
}

inline Plane& Plane::operator *= (float scale)
{
	normal *= scale;
	distance *= scale;
	return *this;
}

inline void Plane::NormalizeUnsafe ()
{
	float invMag = 1.0f/Magnitude(normal);
	normal *= invMag;
	distance *= invMag;
}

// It uses NormalizeRobust(), so it handles zero and extremely small vectors,
// but can be slow. Another option would be to use plain normalize, but
// always remember to check for division by zero with zero vectors.
inline void Plane::NormalizeRobust ()
{
	float invMag;
	normal = ::NormalizeRobust(normal, invMag);
	distance *= invMag;
}


#endif
