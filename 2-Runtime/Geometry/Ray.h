#ifndef RAY_H
#define RAY_H

#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Math/Vector3.h"

class Ray
{
#if UNITY_FLASH //flash needs to be able to set these fields
public:
#endif
	Vector3f	m_Origin;
	Vector3f	m_Direction; // Direction is always normalized
	
public:
	Ray () {}
	Ray (const Vector3f& orig, const Vector3f& dir) { m_Origin = orig; SetDirection (dir); }
		
	const Vector3f& GetDirection ()const		{ return m_Direction; }
	// Direction has to be normalized
	void SetDirection (const Vector3f& dir)	{ AssertIf (!IsNormalized (dir)); m_Direction = dir; }
	void SetApproxDirection (const Vector3f& dir)	{ m_Direction = NormalizeFast (dir); }
	void SetOrigin (const Vector3f& origin)	{ m_Origin = origin; }

	const Vector3f& GetOrigin ()const		{ return m_Origin; }
	Vector3f GetPoint (float t) const			{ return m_Origin + t * m_Direction; }

	float SqrDistToPoint (const Vector3f &v) const;
};


#endif
