#ifndef BOUNDINGVOLUMECONVERSION_H
#define BOUNDINGVOLUMECONVERSION_H

#include "AABB.h"
#include "Sphere.h"

inline void AABBToSphere (const AABB& a, Sphere& s)
{
	s.GetCenter () = a.GetCenter ();
	s.GetRadius () = Magnitude (a.GetExtent ());
}

inline void MinMaxAABBToSphere (const MinMaxAABB& a, Sphere& s)
{
	AABB aabb = a;
	AABBToSphere (aabb, s);
}

inline void SphereToAABB (const Sphere& s, AABB& a)
{
	a.GetCenter () = s.GetCenter ();
	a.GetExtent () = Vector3f (s.GetRadius (), s.GetRadius (), s.GetRadius ());
}

inline void SphereToMinMaxAABB (const Sphere& s, MinMaxAABB& minmax)
{
	AABB aabb;
	SphereToAABB (s, aabb);
	minmax = aabb;
}

#endif
