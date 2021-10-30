#include "UnityPrefix.h"
#include "ComputionalGeometry.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Math/Vector2.h"
#include "Plane.h"
#include <cmath>

float SignedTriangleArea2D (const Vector2f& a, const Vector2f& b, const Vector2f& c)
{
	float i01 = (b.x - a.x) * (b.y + a.y);
	float i12 = (c.x - b.x) * (c.y + b.y);
	float i20 = (a.x - c.x) * (a.y + c.y);
	
	return (i01 + i12 + i20) * 0.5F;
}

float SignedTriangleArea2D (const Vector3f* v)
{
	float i01 = (v[1].x - v[0].x) * (v[1].y + v[0].y);
	float i12 = (v[2].x - v[1].x) * (v[2].y + v[1].y);
	float i20 = (v[0].x - v[2].x) * (v[0].y + v[2].y);
	
	return (i01 + i12 + i20) * 0.5F;
}

float TriangleArea3D (const Vector3f& a, const Vector3f& b, const Vector3f& c)
{
	return Magnitude (CalcRawNormalFromTriangle (a, b, c)) * 0.5F;
}


float CalculateProjectedBoxArea2D (const Vector3f* v)
{
	float i01 = (v[1].x - v[0].x) * (v[1].y + v[0].y);
	float i13 = (v[3].x - v[1].x) * (v[3].y + v[1].y);
	float i20 = (v[0].x - v[2].x) * (v[0].y + v[2].y);
	float i37 = (v[7].x - v[3].x) * (v[7].y + v[3].y);
	float i62 = (v[2].x - v[6].x) * (v[2].y + v[6].y);
	float i23 = (v[3].x - v[2].x) * (v[3].y + v[2].y);
	float i45 = (v[5].x - v[4].x) * (v[5].y + v[4].y);
	float i57 = (v[7].x - v[5].x) * (v[7].y + v[5].y);
	float i76 = (v[6].x - v[7].x) * (v[6].y + v[7].y);
	float i64 = (v[4].x - v[6].x) * (v[4].y + v[6].y);
	float i15 = (v[5].x - v[1].x) * (v[5].y + v[1].y);
	float i40 = (v[0].x - v[4].x) * (v[0].y + v[4].y);
	
	float area 
		  = Abs (
				   i01
	  			 + i13
	  			 - i23
				 + i20);

	area += Abs (
				   i23
	  			 + i37
	  			 + i76
				 + i62);

	area += Abs (
				   i45
	  			 + i57
	  			 + i76
				 + i64);

	area += Abs (
				   i01
	  			 + i15
	  			 - i45
				 + i40);

	area += Abs (
				   i15
	  			 + i57
	  			 - i37
				 - i13);

	area += Abs (
				 - i40
	  			 - i64
	  			 + i62
				 + i20);
				 
	return area * 0.5F;
}

float CalculateBoxAreaRadialFrustum (
	Vector3f* v,
	float inFovy,
	float inNear,
	float inFar,
	float inScreenHeight)
{
	UInt32 i;
	double cotan, w, distance;
	Vector3f projectedTriangle[8];

	cotan = Deg2Rad (inFovy / 2.0F);
	cotan = cos (cotan) / sin (cotan);
	
	for (i=0;i<8;i++)
	{
		projectedTriangle[i] = v[i];
		distance = Magnitude (projectedTriangle[i]);

		w = (inFar + inNear) / (inFar - inNear) * distance 
			+ 2.0F * inNear * inFar / (inFar - inNear);
		w = 1.0F / w * cotan * inScreenHeight / 2.0F;
		
		projectedTriangle[i].x *= w;
		projectedTriangle[i].y *= w;
	}

	return CalculateProjectedBoxArea2D (projectedTriangle);	
}

//float a = (inFar + inNear) / (inFar - inNear);
//float b = 2.0F * inNear * inFar / (inFar - inNear);
//float c = cotan * inScreenHeight / 2.0F;

float CalculateBoxAreaRadialFrustum2 (Vector3f* v, float a, float b, float c)
{
	UInt32 i;
	float w, distance;
	Vector3f projectedTriangle[8];
	
	for (i=0;i<8;i++)
	{
		projectedTriangle[i] = v[i];
		distance = Magnitude (projectedTriangle[i]);

		w = a * distance + b;
		w = 1.0F / w * c;
		
		projectedTriangle[i].x *= w;
		projectedTriangle[i].y *= w;
	}

	return CalculateProjectedBoxArea2D (projectedTriangle);	
	
}

float CalculateTriangleAreaRadialFrustum (
	Vector3f* v,
	float inFovy,
	float inNear,
	float inFar,
	float inScreenHeight)
{
	UInt32 i;
	double cotan, w, distance;
	Vector3f projectedTriangle[3];

	cotan = Deg2Rad (inFovy / 2.0F);
	cotan = cos (cotan) / sin (cotan);
	
	for (i=0;i<3;i++)
	{
		projectedTriangle[i] = v[i];
		distance = Magnitude (projectedTriangle[i]);

		w = (inFar + inNear) / (inFar - inNear) * distance 
			+ 2.0F * inNear * inFar / (inFar - inNear);
		w = 1.0F / w;
		
		projectedTriangle[i].x *= w * cotan * inScreenHeight / 2.0F;
		projectedTriangle[i].y *= w * cotan * inScreenHeight / 2.0F;
	}

	return Abs (SignedTriangleArea2D (projectedTriangle));
}
//float a = (inFar + inNear) / (inFar - inNear);
//float b = 2.0F * inNear * inFar / (inFar - inNear);
//float c = cotan * inScreenHeight / 2.0F;

float CalculateTriangleAreaRadialFrustum2 (Vector3f* v, float a, float b, float c)
{
	UInt32 i;
	float w, distance;
	Vector3f projectedTriangle[3];
	
	for (i=0;i<3;i++)
	{
		projectedTriangle[i] = v[i];
		distance = Magnitude (projectedTriangle[i]);

		w = a * distance + b;
		w = 1.0F / w * c;
		
		projectedTriangle[i].x *= w;
		projectedTriangle[i].y *= w;
	}

	return Abs (SignedTriangleArea2D (projectedTriangle));
}

float CalculateTriangleAreaRotationless (Vector3f* v, float inFovy, float inScreenWidth, float inScreenHeight, Vector3f& viewPoint)
{
	Vector3f normal = Normalize (Cross (v[0] - v[1], v[1] - v[2]));
	float objectSpaceArea = Magnitude (Cross (v[1] - v[0], v[2] - v[0])) * 0.5F;
	Vector3f centroid = (v[0] + v[1] + v[2]) / 3.0F;
	Vector3f difference = viewPoint - centroid;
	float distance = Magnitude (difference);
	// We want 1.0 when it faces the tri directly -> 0 degrees
	// We want 0.0 when the tri is invisible -> 90 degrees
	// It doesn't matter if the tri faces away from the viewer or not.
	float angle = Abs (Dot (normal, (difference / distance)));
	
	float screenSpaceArea = 
		objectSpaceArea
	  * sin (Deg2Rad (inFovy * 0.5F))
	  * inScreenWidth
	  * inScreenHeight
	  * sin (Deg2Rad (inFovy * 0.5F))
	  * angle
	  / distance;
	  
	return screenSpaceArea;
}

int ClipPolygonAgainstPlane(int vertexCount, const Vector3f *vertex, const Plane& plane, char *location, Vector3f *result)
{
	const float kBoundaryEpsilon		= 1.0e-3F;

	enum
	{
		kPolygonInterior		= 1,		//## The point lies in the interior of the polygon.
		kPolygonBoundary		= 0,		//## The point lies on or very near the boundary of the polygon.
		kPolygonExterior		= -1		//## The point lies outside the polygon.
	};

	int positive = 0;
	int negative = 0;
	
	for (int a = 0; a < vertexCount; a++)
	{
		float d = plane.GetDistanceToPoint(vertex[a]);
		if (d > kBoundaryEpsilon)
		{
			location[a] = kPolygonInterior;
			positive++;
		}
		else
		{
			if (d < -kBoundaryEpsilon)
			{
				location[a] = kPolygonExterior;
				negative++;
			}
			else
			{
				location[a] = kPolygonBoundary;
			}
		}
	}
	
	if (negative == 0)
	{
		for (int a = 0; a < vertexCount; a++)
			result[a] = vertex[a];
		return vertexCount;
	}
	else if (positive == 0)
	{
		return 0;
	}
	
	int count = 0;
	int previous = vertexCount - 1;
	for (int index = 0; index < vertexCount; index++)
	{
		int loc = location[index];
		if (loc == kPolygonExterior)
		{
			if (location[previous] == kPolygonInterior)
			{
				const Vector3f& v1 = vertex[previous];
				const Vector3f& v2 = vertex[index];
				Vector3f dv = v2 - v1;
				
				float t = plane.GetDistanceToPoint(v2) / plane.GetDistanceToPoint(dv);
				result[count++] = v2 - dv * t;
			}
		}
		else
		{
			const Vector3f& v1 = vertex[index];
			if ((loc == kPolygonInterior) && (location[previous] == kPolygonExterior))
			{
				const Vector3f& v2 = vertex[previous];
				Vector3f dv = v2 - v1;
				
				float t = plane.GetDistanceToPoint(v2) / plane.GetDistanceToPoint(dv);
				result[count++] = v2 - dv * t;
			}
			
			result[count++] = v1;
		}
		
		previous = index;
	}
	
	return count;
}
