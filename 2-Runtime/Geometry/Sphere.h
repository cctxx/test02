#ifndef SPHERE_H
#define SPHERE_H

#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Math/Vector3.h"
#include <algorithm>
#include "Runtime/Modules/ExportModules.h"

class Sphere
{
	Vector3f	m_Center;
	float		m_Radius;
	
	public:
	
	DECLARE_SERIALIZE (Sphere)
	
	Sphere () {}
	Sphere (const Vector3f& p0, float r)				{Set (p0, r);}
	
	void Set (const Vector3f& p0)						{m_Center = p0;	m_Radius = 0;}
	void Set (const Vector3f& p0, float r)				{m_Center = p0;	m_Radius = r;}
	
	void Set (const Vector3f& p0, const Vector3f& p1);
	
	void Set (const Vector3f* inVertices, UInt32 inHowmany);
	
	Vector3f& GetCenter () {return m_Center;}
	const Vector3f& GetCenter ()const  {return m_Center;}
	
	float& GetRadius () {return m_Radius;}
	const float& GetRadius ()const {return m_Radius;}
	
	bool IsInside (const Sphere& inSphere)const;
};

float EXPORT_COREMODULE CalculateSqrDistance (const Vector3f& p, const Sphere& s);
bool Intersect (const Sphere& inSphere0, const Sphere& inSphere1);


inline void Sphere::Set (const Vector3f& p0, const Vector3f& p1)
{
	Vector3f dhalf = (p1 - p0) * 0.5;

	m_Center = dhalf + p0;
	m_Radius = Magnitude (dhalf);
}

inline bool Sphere::IsInside (const Sphere& inSphere)const
{
	float sqrDist = SqrMagnitude (GetCenter () - inSphere.GetCenter ());
	if (Sqr (GetRadius ()) > sqrDist + Sqr (inSphere.GetRadius ()))
		return true;
	else
		return false;
}

inline bool Intersect (const Sphere& inSphere0, const Sphere& inSphere1)
{
	float sqrDist = SqrMagnitude (inSphere0.GetCenter () - inSphere1.GetCenter ());
	if (Sqr (inSphere0.GetRadius () + inSphere1.GetRadius ()) > sqrDist)
		return true;
	else
		return false;
}

inline float CalculateSqrDistance (const Vector3f& p, const Sphere& s)
{
	return std::max (0.0F, SqrMagnitude (p - s.GetCenter ()) - Sqr (s.GetRadius ()));
}

template<class TransferFunction> inline
void Sphere::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_Center);
	TRANSFER (m_Radius);
}

#endif
