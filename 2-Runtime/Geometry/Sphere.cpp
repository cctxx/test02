#include "UnityPrefix.h"
#include "Sphere.h"

Sphere MergeSpheres (const Sphere& inSphere0, const Sphere& inSphere1);
float MergeSpheresRadius (const Sphere& inSphere0, const Sphere& inSphere1);

void Sphere::Set (const Vector3f* inVertices, UInt32 inHowmany)
{
	m_Radius = 0.0F;
	m_Center = Vector3f::zero;
	UInt32 i;
	for (i=0;i<inHowmany;i++)
		m_Radius = std::max (m_Radius, SqrMagnitude (inVertices[i]));
	m_Radius = sqrt (m_Radius);
}


Sphere MergeSpheres (const Sphere& inSphere0, const Sphere& inSphere1)
{
	Vector3f kCDiff = inSphere1.GetCenter() - inSphere0.GetCenter();
	float fLSqr = SqrMagnitude (kCDiff);
	float fRDiff = inSphere1.GetRadius() - inSphere0.GetRadius();

	if (fRDiff*fRDiff >= fLSqr)
	   return fRDiff >= 0.0 ? inSphere1 : inSphere0;

	float fLength = sqrt (fLSqr);
	const float fTolerance = 1.0e-06f;
	Sphere kSphere;

	if (fLength > fTolerance)
	{
	   float fCoeff = (fLength + fRDiff) / (2.0 * fLength);
	   kSphere.GetCenter () = inSphere0.GetCenter () + fCoeff * kCDiff;
	}
	else
	{
	   kSphere.GetCenter () = inSphere0.GetCenter ();
	}

	kSphere.GetRadius () = 0.5F * (fLength + inSphere0.GetRadius () +  inSphere1.GetRadius ());

	return kSphere;
}

float MergeSpheresRadius (const Sphere& inSphere0, const Sphere& inSphere1)
{
	Vector3f kCDiff = inSphere1.GetCenter() - inSphere0.GetCenter();
	float fLSqr = SqrMagnitude (kCDiff);
	float fRDiff = inSphere1.GetRadius () - inSphere0.GetRadius();

	if (fRDiff*fRDiff >= fLSqr)
	   return fRDiff >= 0.0 ? inSphere1.GetRadius () : inSphere0.GetRadius ();

	float fLength = sqrt (fLSqr);
	return 0.5F * (fLength + inSphere0.GetRadius () +  inSphere1.GetRadius ());
}
