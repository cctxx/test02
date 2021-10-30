#ifndef RAY2D_H
#define RAY2D_H

#include "Runtime/Math/Vector2.h"


// --------------------------------------------------------------------------


class Ray2D
{
#if UNITY_FLASH //flash needs to be able to set these fields
public:
#endif
	Vector2f	m_Origin;
	Vector2f	m_Direction; // Direction is always normalized
	
public:
	Ray2D () {}
	Ray2D (const Vector2f& origin, const Vector2f& direction) { m_Origin = origin; SetDirection (direction); }
		
	const Vector2f& GetOrigin ()const { return m_Origin; }
	void SetOrigin (const Vector2f& origin)	{ m_Origin = origin; }

	const Vector2f& GetDirection () const { return m_Direction; }
	void SetDirection (const Vector2f& direction) { AssertIf (!IsNormalized (direction)); m_Direction = direction; }
	void SetApproxDirection (const Vector2f& direction) { m_Direction = NormalizeFast (direction); }

	Vector2f GetPoint (const float scale) const { return m_Origin + (scale * m_Direction); }
};

#endif
