#include "UnityPrefix.h"
#include "AABB.h"
#include "Runtime/Math/Quaternion.h"

const AABB AABB::zero = AABB(Vector3f::zero, Vector3f::zero);

void CalculateClosestPoint (const Vector3f& rkPoint, const AABB& rkBox, Vector3f& outPoint, float& outSqrDistance)
{
	// compute coordinates of point in box coordinate system
	Vector3f kClosest = rkPoint - rkBox.GetCenter();

	// project test point onto box
	float fSqrDistance = 0.0f;
	float fDelta;
	
	for (int i=0;i<3;i++)
	{
		if ( kClosest[i] < -rkBox.GetExtent (i) )
		{
			fDelta = kClosest[i] + rkBox.GetExtent (i);
			fSqrDistance += fDelta * fDelta;
			kClosest[i] = -rkBox.GetExtent (i);
		}
		else if ( kClosest[i] > rkBox.GetExtent(i) )
		{
			fDelta = kClosest[i] - rkBox.GetExtent (i);
			fSqrDistance += fDelta * fDelta;
			kClosest[i] = rkBox.GetExtent (i);
		}
	}
	
	// Inside
	if (fSqrDistance == 0.0F)
	{
		outPoint = rkPoint;
		outSqrDistance = 0.0F;
	}
	// Outside
	else
	{
		outPoint = kClosest + rkBox.GetCenter();
		outSqrDistance = fSqrDistance;
	}
}


// Sphere-AABB distance, Arvo's algorithm
float CalculateSqrDistance (const Vector3f& rkPoint, const AABB& rkBox)
{
	Vector3f closest = rkPoint - rkBox.GetCenter();
	float sqrDistance = 0.0f;
	
	for (int i = 0; i < 3; ++i)
	{
		float clos = closest[i];
		float ext = rkBox.GetExtent(i);
		if (clos < -ext)
		{
			float delta = clos + ext;
			sqrDistance += delta * delta;
			closest[i] = -ext;
		}
		else if (clos > ext)
		{
			float delta = clos - ext;
			sqrDistance += delta * delta;
			closest[i] = ext;
		}
	}

	return sqrDistance;
}

void AABB::GetVertices (Vector3f* outVertices) const
{
	outVertices[0] = m_Center + Vector3f (-m_Extent.x, -m_Extent.y, -m_Extent.z);
	outVertices[1] = m_Center + Vector3f (+m_Extent.x, -m_Extent.y, -m_Extent.z);
	outVertices[2] = m_Center + Vector3f (-m_Extent.x, +m_Extent.y, -m_Extent.z);
	outVertices[3] = m_Center + Vector3f (+m_Extent.x, +m_Extent.y, -m_Extent.z);

	outVertices[4] = m_Center + Vector3f (-m_Extent.x, -m_Extent.y, +m_Extent.z);
	outVertices[5] = m_Center + Vector3f (+m_Extent.x, -m_Extent.y, +m_Extent.z);
	outVertices[6] = m_Center + Vector3f (-m_Extent.x, +m_Extent.y, +m_Extent.z);
	outVertices[7] = m_Center + Vector3f (+m_Extent.x, +m_Extent.y, +m_Extent.z);
}

void MinMaxAABB::GetVertices( Vector3f outVertices[8] ) const
{
	//    7-----6
	//   /     /|
	//  3-----2 |
	//  | 4   | 5
	//  |     |/
	//  0-----1
	outVertices[0].Set( m_Min.x, m_Min.y, m_Min.z );
	outVertices[1].Set( m_Max.x, m_Min.y, m_Min.z );
	outVertices[2].Set( m_Max.x, m_Max.y, m_Min.z );
	outVertices[3].Set( m_Min.x, m_Max.y, m_Min.z );
	outVertices[4].Set( m_Min.x, m_Min.y, m_Max.z );
	outVertices[5].Set( m_Max.x, m_Min.y, m_Max.z );
	outVertices[6].Set( m_Max.x, m_Max.y, m_Max.z );
	outVertices[7].Set( m_Min.x, m_Max.y, m_Max.z );
}



bool AABB::IsInside (const Vector3f& inPoint) const
{
	if (inPoint[0] < m_Center[0] - m_Extent[0])
		return false;
	if (inPoint[0] > m_Center[0] + m_Extent[0])
		return false;

	if (inPoint[1] < m_Center[1] - m_Extent[1])
		return false;
	if (inPoint[1] > m_Center[1] + m_Extent[1])
		return false;

	if (inPoint[2] < m_Center[2] - m_Extent[2])
		return false;
	if (inPoint[2] > m_Center[2] + m_Extent[2])
		return false;
	
	return true;
}

void AABB::Encapsulate (const Vector3f& inPoint) {
	MinMaxAABB temp = *this;
	temp.Encapsulate (inPoint);
	FromMinMaxAABB (temp);
}

bool MinMaxAABB::IsInside (const Vector3f& inPoint) const
{
	if (inPoint[0] < m_Min[0])
		return false;
	if (inPoint[0] > m_Max[0])
		return false;

	if (inPoint[1] < m_Min[1])
		return false;
	if (inPoint[1] > m_Max[1])
		return false;

	if (inPoint[2] < m_Min[2])
		return false;
	if (inPoint[2] > m_Max[2])
		return false;
	
	return true;
}

MinMaxAABB AddAABB (const MinMaxAABB& lhs, const MinMaxAABB& rhs)
{
	MinMaxAABB minMax;
	if (lhs.IsValid())
		minMax = lhs;
	
	if (rhs.IsValid())
	{
		minMax.Encapsulate (rhs.GetMax ());
		minMax.Encapsulate (rhs.GetMin ());
	}

	return minMax;
}

inline Vector3f RotateExtents (const Vector3f& extents, const Matrix3x3f& rotation)
{
	Vector3f newExtents;
	for (int i=0;i<3;i++)
		newExtents[i] = Abs (rotation.Get (i, 0) * extents.x) + Abs (rotation.Get (i, 1) * extents.y) + Abs (rotation.Get (i, 2) * extents.z);
	return newExtents;	
}

inline Vector3f RotateExtents (const Vector3f& extents, const Matrix4x4f& rotation)
{
	Vector3f newExtents;
	for (int i=0;i<3;i++)
		newExtents[i] = Abs (rotation.Get (i, 0) * extents.x) + Abs (rotation.Get (i, 1) * extents.y) + Abs (rotation.Get (i, 2) * extents.z);
	return newExtents;	
}

void TransformAABB (const AABB& aabb, const Vector3f& position, const Quaternionf& rotation, AABB& result)
{
	Matrix3x3f m;
	QuaternionToMatrix (rotation, m);
	
	Vector3f extents = RotateExtents (aabb.GetExtent (), m);
	Vector3f center = m.MultiplyPoint3 (aabb.GetCenter ());
	center += position;
	result.SetCenterAndExtent( center, extents );
}

void TransformAABB (const AABB& aabb, const Matrix4x4f& transform, AABB& result)
{
	Vector3f extents = RotateExtents (aabb.GetExtent (), transform);
	Vector3f center = transform.MultiplyPoint3 (aabb.GetCenter ());
	result.SetCenterAndExtent( center, extents );
}

void TransformAABBSlow (const AABB& aabb, const Matrix4x4f& transform, AABB& result)
{
	MinMaxAABB transformed;
	transformed.Init ();
	
	Vector3f v[8];
	aabb.GetVertices (v);
	for (int i=0;i<8;i++)
		transformed.Encapsulate (transform.MultiplyPoint3 (v[i]));

	result = transformed;
}


void InverseTransformAABB (const AABB& aabb, const Vector3f& position, const Quaternionf& rotation, AABB& result)
{
	Matrix3x3f m;
	QuaternionToMatrix (Inverse (rotation), m);

	Vector3f extents = RotateExtents (aabb.GetExtent (), m);
	Vector3f center = aabb.GetCenter () - position;
	center = m.MultiplyPoint3 (center);
	
	result.SetCenterAndExtent( center, extents );
}

bool IsContainedInAABB (const AABB& inside, const AABB& bigBounds)
{
	bool outside = false;
	outside |= inside.m_Center[0] - inside.m_Extent[0] < bigBounds.m_Center[0] - bigBounds.m_Extent[0];
	outside |= inside.m_Center[0] + inside.m_Extent[0] > bigBounds.m_Center[0] + bigBounds.m_Extent[0];

	outside |= inside.m_Center[1] - inside.m_Extent[1] < bigBounds.m_Center[1] - bigBounds.m_Extent[1];
	outside |= inside.m_Center[1] + inside.m_Extent[1] > bigBounds.m_Center[1] + bigBounds.m_Extent[1];

	outside |= inside.m_Center[2] - inside.m_Extent[2] < bigBounds.m_Center[2] - bigBounds.m_Extent[2];
	outside |= inside.m_Center[2] + inside.m_Extent[2] > bigBounds.m_Center[2] + bigBounds.m_Extent[2];

	return !outside;
}
