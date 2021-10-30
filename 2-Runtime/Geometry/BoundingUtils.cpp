#include "UnityPrefix.h"
#include "BoundingUtils.h"
#include "Plane.h"
#include "AABB.h"
#include "Intersection.h"

// --------------------------------------------------------------------------

void GetFrustumPoints( const Matrix4x4f& clipToWorld, Vector3f* frustum )
{
	clipToWorld.PerspectiveMultiplyPoint3( Vector3f(-1,-1,-1), frustum[0] );
	clipToWorld.PerspectiveMultiplyPoint3( Vector3f( 1,-1,-1), frustum[1] );
	clipToWorld.PerspectiveMultiplyPoint3( Vector3f( 1, 1,-1), frustum[2] );
	clipToWorld.PerspectiveMultiplyPoint3( Vector3f(-1, 1,-1), frustum[3] );
	clipToWorld.PerspectiveMultiplyPoint3( Vector3f(-1,-1, 1), frustum[4] );
	clipToWorld.PerspectiveMultiplyPoint3( Vector3f( 1,-1, 1), frustum[5] );
	clipToWorld.PerspectiveMultiplyPoint3( Vector3f( 1, 1, 1), frustum[6] );
	clipToWorld.PerspectiveMultiplyPoint3( Vector3f(-1, 1, 1), frustum[7] );
}

void GetFrustumPortion( const Vector3f* frustum, float nearSplit, float farSplit, Vector3f* outPortion )
{
	outPortion[0] = Lerp( frustum[0], frustum[0+4], nearSplit );
	outPortion[1] = Lerp( frustum[1], frustum[1+4], nearSplit );
	outPortion[2] = Lerp( frustum[2], frustum[2+4], nearSplit );
	outPortion[3] = Lerp( frustum[3], frustum[3+4], nearSplit );
	outPortion[4] = Lerp( frustum[0], frustum[0+4], farSplit );
	outPortion[5] = Lerp( frustum[1], frustum[1+4], farSplit );
	outPortion[6] = Lerp( frustum[2], frustum[2+4], farSplit );
	outPortion[7] = Lerp( frustum[3], frustum[3+4], farSplit );
}

static bool ClipTest( const float p, const float q, float& u1, float& u2 )
{
	// Return value is 'true' if line segment intersects the current test
	// plane.  Otherwise 'false' is returned in which case the line segment
	// is entirely clipped.
	const float EPS = 1.0e-10f;
	if( p < -EPS ) {
		float r = q/p;
		if( r > u2 )
			return false;
		else {
			if( r > u1 )
				u1 = r;
			return true;
		}
	}
	else if( p > EPS )
	{
		float r = q/p;
		if( r < u1 )
			return false;
		else {
			if( r < u2 )
				u2 = r;
			return true;
		}
	}
	else
	{
		return q >= 0.0f;
	}
}

static bool IntersectLineAABB( Vector3f& v, const Vector3f& p, const Vector3f& dir, const MinMaxAABB& b )
{
	float t1 = 0.0f;
	float t2 = 1.0e30f;
	bool intersect = false;
	if (ClipTest(-dir.z,p.z-b.GetMin().z,t1,t2) && ClipTest(dir.z,b.GetMax().z-p.z,t1,t2) &&
		ClipTest(-dir.y,p.y-b.GetMin().y,t1,t2) && ClipTest(dir.y,b.GetMax().y-p.y,t1,t2) &&
		ClipTest(-dir.x,p.x-b.GetMin().x,t1,t2) && ClipTest(dir.x,b.GetMax().x-p.x,t1,t2))
	{
		if( 0 <= t1 ) {
			v = p + dir * t1;
			intersect = true;
		}
		if( 0 <= t2 ) {
			v = p + dir * t2;
			intersect = true;
		}
	}
	return intersect;
}

inline bool ClipPolysByPlane( UInt8 numPoints, const Vector3f* __restrict input, const Plane& A, UInt8* __restrict pNumOutPoints, Vector3f* __restrict output, UInt8* __restrict pNumIntermPoints, Vector3f* __restrict interm )
{
	int i;
	if( numPoints < 3 )
	{
		*pNumOutPoints = 0;
		return false;
	}

	UInt8 numOutPoints = 0;
	UInt8& numIntermPoints = *pNumIntermPoints;

	Vector3f temp;

	bool * outside = (bool*)alloca(numPoints*sizeof(bool));
	for( i = 0; i < numPoints; ++i )
		outside[i] = A.GetDistanceToPoint( input[i] ) < 0.0f;

	for( i = 0; i < numPoints; ++i )
	{
		int idNext = (i+1) % numPoints;

		// both outside -> save none
		if(outside[i] && outside[idNext])
			continue;
		
		// outside-inside -> save intersection and i+1
		if(outside[i])
		{
			if( IntersectSegmentPlane( input[i], input[idNext], A, &temp ) )
			{
				output[numOutPoints++] = temp;
				interm[numIntermPoints++] = temp;
			}

			output[numOutPoints++] = input[idNext];
			continue;
		}

		// inside-outside -> save intersection
		if(outside[idNext]) 
		{
			if( IntersectSegmentPlane( input[i], input[idNext], A, &temp ) )
			{
				output[numOutPoints++] = temp;
				interm[numIntermPoints++] = temp;
			}
		
			continue;
		}

		output[numOutPoints++] = input[idNext];
	}
	*pNumOutPoints = numOutPoints;
	return numOutPoints ? true : false;
}

void CalcHullBounds(const Vector3f* __restrict hullPoints, const UInt8* __restrict hullCounts, UInt8 hullFaces, const Plane& nearPlane, const Matrix4x4f& cameraWorldToClip, MinMaxAABB& aabb)
{
	Vector3f outputData[128];
	Vector3f intermData[128];

	UInt8 outputDataCounts[64];
	UInt8 intermDataCounts[64];

	const Vector3f* __restrict inputPoints = hullPoints;
	const UInt8* __restrict inputCounts = hullCounts;

	Vector3f* __restrict outputPoints = outputData;
	UInt8* __restrict outputCounts = outputDataCounts;
	intermDataCounts[0] = 0;
	for( UInt8 i = 0; i != hullFaces; ++i )
	{
		const UInt8 inputCount = *inputCounts++;
		ClipPolysByPlane( inputCount, inputPoints, nearPlane, outputCounts, outputPoints, intermDataCounts, intermData);
		inputPoints += inputCount;
		outputPoints += *outputCounts;
		outputCounts++;
	}
	
	// Project hull's points onto the screen and compute bounding rectangle of them.
	Vector3f projectedPoint;
	outputPoints = outputData;
	outputCounts = outputDataCounts;

	for( int f = 0; f < hullFaces; ++f )
	{
		const UInt8 numPoints = *outputCounts++;
		for( int i = 0; i < numPoints; ++i )
		{
			cameraWorldToClip.PerspectiveMultiplyPoint3( outputPoints[i], projectedPoint );
			aabb.Encapsulate( projectedPoint );
		}
		outputPoints += numPoints;
	}

	if( aabb.m_Min.x < -1.0f ) aabb.m_Min.x = -1.0f;
	if( aabb.m_Min.y < -1.0f ) aabb.m_Min.y = -1.0f;
	if( aabb.m_Max.x >  1.0f ) aabb.m_Max.x =  1.0f;
	if( aabb.m_Max.y >  1.0f ) aabb.m_Max.y =  1.0f;

}


// Returns our "focused region of interest": frustum, clipped by scene bounds,
// extruded towards light and clipped by scene bounds again.
//
// Frustum is 
// { -1, -1, -1 }
// {  1, -1, -1 }
// {  1,  1, -1 }
// { -1,  1, -1 }
// { -1, -1,  1 }
// {  1, -1,  1 }
// {  1,  1,  1 }
// { -1,  1,  1 }



void CalculateFocusedLightHull( const Vector3f* frustum, const Vector3f& lightDir, const MinMaxAABB& sceneAABB, std::vector<Vector3f>& points )
{
	int i;
	Vector3f tempPoints[3][256];
	UInt8 tempCounts[3][128];

	Plane planes[6];
	planes[0].SetABCD(  0,  1,  0, -sceneAABB.GetMin().y );
	planes[1].SetABCD(  0, -1,  0,  sceneAABB.GetMax().y );
	planes[2].SetABCD(  1,  0,  0, -sceneAABB.GetMin().x );
	planes[3].SetABCD( -1,  0,  0,  sceneAABB.GetMax().x );
	planes[4].SetABCD(  0,  0,  1, -sceneAABB.GetMin().z );
	planes[5].SetABCD(  0,  0, -1,  sceneAABB.GetMax().z );
	
	Vector3f* __restrict pData[2] = { (Vector3f*)&tempPoints[0][0], (Vector3f*)&tempPoints[1][0] };
	UInt8* __restrict pDataCounts[2] = { (UInt8*)&tempCounts[0][0], (UInt8*)&tempCounts[1][0] };
	
	Vector3f* v = *pData;
	UInt8* c = *pDataCounts;

	UInt32 numFaces = 6;

	c[0] = c[1] = c[2] = c[3] = c[4] = c[5] = 4;
	v[ 0] = frustum[0]; v[ 1] = frustum[1]; v[ 2] = frustum[2]; v[ 3] = frustum[3];
	v[ 4] = frustum[7]; v[ 5] = frustum[6]; v[ 6] = frustum[5]; v[ 7] = frustum[4];
	v[ 8] = frustum[0]; v[ 9] = frustum[3]; v[10] = frustum[7]; v[11] = frustum[4];
	v[12] = frustum[1]; v[13] = frustum[5]; v[14] = frustum[6]; v[15] = frustum[2];
	v[16] = frustum[4]; v[17] = frustum[5]; v[18] = frustum[1]; v[19] = frustum[0];
	v[20] = frustum[6]; v[21] = frustum[7]; v[22] = frustum[3]; v[23] = frustum[2];

	
	UInt32 numTotalPoints;
	UInt32 vIn = 0;


	Vector3f* intermPoints = &tempPoints[2][0];
	UInt8* intermCounts = &tempCounts[2][0];

	for( int p = 0; p < 6; ++p )
	{
		const Vector3f* __restrict inputPoints=pData[vIn];
		Vector3f* __restrict outputPoints = pData[1-vIn];

		UInt8* __restrict inputCounts=pDataCounts[vIn];
		UInt8* __restrict outputCounts = pDataCounts[1-vIn];

		numTotalPoints = 0;
		*intermCounts = 0;

		UInt32 faceCount = numFaces;
		for( i = 0; i < faceCount; ++i ) 
		{
			const UInt8 numInputPoints = *inputCounts;
			if(ClipPolysByPlane(numInputPoints, inputPoints, planes[p], outputCounts, outputPoints, intermCounts, intermPoints)) 
			{
				const UInt8 outputCount = *outputCounts++;
				numTotalPoints+=outputCount;
				outputPoints += outputCount;
			}
			else
			{
				if(0 == (--numFaces))
					break;
			}

			inputCounts++;
			inputPoints += numInputPoints;
		}
		vIn = 1-vIn;	// anyone for ping-pong ?
		
		// add an extra face built from all the intersection points so it catches all the edges.
		const UInt8 numIntermPoints = *intermCounts;
		if(numIntermPoints && (p<5))
		{
			numFaces++;
			*outputCounts = numIntermPoints;
			memcpy(outputPoints, intermPoints, numIntermPoints * sizeof(Vector3f));
		}
	}

	if(numFaces)
	{ // output the clipped points

		Vector3f pt;
		Vector3f ld = -lightDir;
		points.reserve(numTotalPoints << 1); // worst case scenario
		Vector3f* __restrict inputPoints=pData[vIn];
		UInt8* __restrict inputCounts=pDataCounts[vIn];

		for( i = 0; i < numFaces; ++i )
		{
			const UInt8 numPoints = *inputCounts++;
			for(UInt8 k=0;k!=numPoints;k++)
			{
				const Vector3f& v = inputPoints[k];
				points.push_back(v);
				if( IntersectLineAABB( pt, v, ld, sceneAABB ) )
					points.push_back( pt );
			}
			inputPoints += numPoints;
		}
	}
}

void CalculateBoundingSphereFromFrustumPoints(const Vector3f points[8], Vector3f& outCenter, float& outRadius)
{
	Vector3f spherePoints[4];
	spherePoints[0] = points[0];
	spherePoints[1] = points[3];
	spherePoints[2] = points[5];
	spherePoints[3] = points[7];

	// Is bounding sphere at the far or near plane?
	for( int plane = 1; plane >= 0; --plane )
	{
		Vector3f pointA = spherePoints[plane*2];
		Vector3f pointB = spherePoints[plane*2 + 1];
		Vector3f center = (pointA + pointB) * 0.5f;
		float radius2 = SqrMagnitude(pointA - center);
		Vector3f pointC = spherePoints[(1-plane)*2];
		Vector3f pointD = spherePoints[(1-plane)*2 + 1];

		// Check if all points are inside sphere
		if( SqrMagnitude(pointC - center) <= radius2 &&
			SqrMagnitude(pointD - center) <= radius2 )
		{
			outCenter = center;
			outRadius = sqrt(radius2);
			return;
		}
	}

	// Sphere touches all four frustum points
	CalculateSphereFrom4Points(spherePoints, outCenter, outRadius);
}

void CalculateSphereFrom4Points(const Vector3f points[4], Vector3f& outCenter, float& outRadius)
{
	Matrix4x4f mat;

	for( int i = 0; i < 4; ++i )
	{
		mat.Get(i, 0) = points[i].x;
		mat.Get(i, 1) = points[i].y;
		mat.Get(i, 2) = points[i].z;
		mat.Get(i, 3) = 1;
	}
	float m11 = mat.GetDeterminant();

	for( int i = 0; i < 4; ++i )
	{
		mat.Get(i, 0) = points[i].x*points[i].x + points[i].y*points[i].y + points[i].z*points[i].z;
		mat.Get(i, 1) = points[i].y;
		mat.Get(i, 2) = points[i].z;
		mat.Get(i, 3) = 1;
	}
	float m12 = mat.GetDeterminant();

	for( int i = 0; i < 4; ++i )
	{
		mat.Get(i, 0) = points[i].x;
		mat.Get(i, 1) = points[i].x*points[i].x + points[i].y*points[i].y + points[i].z*points[i].z;
		mat.Get(i, 2) = points[i].z;
		mat.Get(i, 3) = 1;
	}
	float m13 = mat.GetDeterminant();

	for( int i = 0; i < 4; ++i )
	{
		mat.Get(i, 0) = points[i].x;
		mat.Get(i, 1) = points[i].y;
		mat.Get(i, 2) = points[i].x*points[i].x + points[i].y*points[i].y + points[i].z*points[i].z;
		mat.Get(i, 3) = 1;
	}
	float m14 = mat.GetDeterminant();

	for( int i = 0; i < 4; ++i )
	{
		mat.Get(i, 0) = points[i].x*points[i].x + points[i].y*points[i].y + points[i].z*points[i].z;
		mat.Get(i, 1) = points[i].x;
		mat.Get(i, 2) = points[i].y;
		mat.Get(i, 3) = points[i].z;
	}
	float m15 = mat.GetDeterminant();

	Vector3f c;
	c.x = 0.5 * m12 / m11;
	c.y = 0.5 * m13 / m11;
	c.z = 0.5 * m14 / m11;
	outRadius = sqrt(c.x*c.x + c.y*c.y + c.z*c.z - m15/m11);
	outCenter = c;
}

void CalculateSpotLightBounds (const float range, const float cotanHalfSpotAngle, const Matrix4x4f& lightMatrix, SpotLightBounds& outBounds)
{
	float sideLength = range / cotanHalfSpotAngle;
	outBounds.points[0] = lightMatrix.GetPosition();
	outBounds.points[1] = lightMatrix.MultiplyPoint3( Vector3f(-sideLength,-sideLength, range) );
	outBounds.points[2] = lightMatrix.MultiplyPoint3( Vector3f( sideLength,-sideLength, range) );
	outBounds.points[3] = lightMatrix.MultiplyPoint3( Vector3f( sideLength, sideLength, range) );
	outBounds.points[4] = lightMatrix.MultiplyPoint3( Vector3f(-sideLength, sideLength, range) );
}

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Math/Random/rand.h"

SUITE (BoundingUtilsTest)
{
TEST(BoundingUtilsTest_CalculateSphereFrom4Points)
{
	Rand rnd(123);
	for( int i = 0; i < 10; ++i )
	{
		Vector3f points[4];
		for( int j = 0; j < 4; ++j )
		{
			points[j].x = rnd.GetSignedFloat()*100.f;
			points[j].y = rnd.GetSignedFloat()*100.f;
			points[j].z = rnd.GetSignedFloat()*100.f;
		}
		Vector3f center;
		float radius;
		CalculateSphereFrom4Points(points, center, radius);
		for( int j = 0; j < 4; ++j )
		{
			float dist = Magnitude(points[j] - center);
			float ratio = dist / radius;
			// Radius may be large compared to input range, avoid comparing absolute distances
			bool correct = (ratio > 0.999f) && (ratio < 1.001f);
			CHECK(correct);
		}
	}
}
}

#endif
