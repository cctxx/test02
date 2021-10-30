#include "UnityPrefix.h"
#include "TangentSpace.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/Utility.h"

#include <cmath>


//------------------------------------------------------------------------------

static const double kEpsilon = 1e-10;

double
TangentInfo::Vector3d::Dot(Vector3d v1, Vector3d v2)
{
	return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

double
TangentInfo::Vector3d::Magnitude(Vector3d v)
{
	return std::sqrt(Dot(v,v));
}


TangentInfo::Vector3d
TangentInfo::Vector3d::Normalize(Vector3d v, double mag)
{
	Vector3d ret = {0,0,0};

	if( mag > kEpsilon )
	{
		ret.x = v.x/mag;
		ret.y = v.y/mag;
		ret.z = v.z/mag;
	}

	return ret;
}


TangentInfo::Vector3d
TangentInfo::Vector3d::Normalize(Vector3d v)
{
	double mag = Magnitude(v);
	return Normalize(v,mag);
}


//------------------------------------------------------------------------------

void ComputeTriangleTangentBasis (const Vector3f* vertices, const Vector2f* uvs, const UInt32* indices, TangentInfo out[3])
{
	// Using Eric Lengyel's approach with a few modifications
	// From Mathematics for 3D Game Programming and Computer Graphics
	// want to be able to transform a vector in Object Space to Tangent Space
	// such that the x-axis corresponds to the 's' direction and the
	// y-axis corresponds to the 't' direction, and the z-axis corresponds
	// to <0,0,1>, straight up out of the texture map
	const Vector3f triVertex[]	= { vertices[indices[0]], vertices[indices[1]], vertices[indices[2]] };
	const Vector2f triUV[]		= { uvs[0], uvs[1], uvs[2] };
	
	double p[] = { triVertex[1].x - triVertex[0].x, triVertex[1].y - triVertex[0].y, triVertex[1].z - triVertex[0].z }; 
	double q[] = { triVertex[2].x - triVertex[0].x, triVertex[2].y - triVertex[0].y, triVertex[2].z - triVertex[0].z }; 
	
	double s[] = { triUV[1].x - triUV[0].x, triUV[2].x - triUV[0].x };
	double t[] = { triUV[1].y - triUV[0].y, triUV[2].y - triUV[0].y };

	// we need to solve the equation
	// P = s1*T + t1*B
	// Q = s2*T + t2*B
	// for T and B
	
	// this is a linear system with six unknowns and six equations, for TxTyTz BxByBz
	// [px,py,pz] = [s1,t1] * [Tx,Ty,Tz]
	//  qx,qy,qz     s2,t2     Bx,By,Bz
	
	// multiplying both sides by the inverse of the s,t matrix gives
	// [Tx,Ty,Tz] = 1/(s1t2-s2t1) *  [t2,-t1] * [px,py,pz]
	//  Bx,By,Bz                      -s2,s1	 qx,qy,qz  
	
	TangentInfo faceInfo;
	
	faceInfo.tangent.x  = faceInfo.tangent.y  = faceInfo.tangent.z  = 0.0;
	faceInfo.binormal.x = faceInfo.binormal.y = faceInfo.binormal.z = 0.0;

	double div      = s[0]*t[1] - s[1]*t[0];
	double areaMult = std::abs(div);

	if( areaMult >= 1e-8 )
	{
		double r = 1.0 / div;
		
		s[0] *= r;	t[0] *= r;
		s[1] *= r;	t[1] *= r;

		
		faceInfo.tangent.x  = (t[1] * p[0] - t[0] * q[0]);
		faceInfo.tangent.y  = (t[1] * p[1] - t[0] * q[1]);
		faceInfo.tangent.z  = (t[1] * p[2] - t[0] * q[2]);
		
		faceInfo.binormal.x  = (s[0] * q[0] - s[1] * p[0]);
		faceInfo.binormal.y  = (s[0] * q[1] - s[1] * p[1]);
		faceInfo.binormal.z  = (s[0] * q[2] - s[1] * p[2]);
		
		// weight by area
		
		faceInfo.tangent    = TangentInfo::Vector3d::Normalize(faceInfo.tangent);
		faceInfo.tangent.x *= areaMult;
		faceInfo.tangent.y *= areaMult;
		faceInfo.tangent.z *= areaMult;
		
		faceInfo.binormal    = TangentInfo::Vector3d::Normalize(faceInfo.binormal);
		faceInfo.binormal.x *= areaMult;
		faceInfo.binormal.y *= areaMult;
		faceInfo.binormal.z *= areaMult;
	}

    for( unsigned v = 0 ; v < 3 ; ++v )
    {
        static const unsigned kNextIndex[][2] = { {2,1}, {0,2}, {1,0} };

		TangentInfo::Vector3d edge1 = 
		{
			triVertex[ kNextIndex[v][0] ].x - triVertex[v].x,
			triVertex[ kNextIndex[v][0] ].y - triVertex[v].y,
			triVertex[ kNextIndex[v][0] ].z - triVertex[v].z
		};
		/*
		edge1.x = triVertex[ kNextIndex[v][0] ].x - triVertex[v].x;
		edge1.y = triVertex[ kNextIndex[v][0] ].y - triVertex[v].y;
		edge1.z = triVertex[ kNextIndex[v][0] ].z - triVertex[v].z;
		*/

        TangentInfo::Vector3d edge2 = 
        {
			triVertex[ kNextIndex[v][1] ].x - triVertex[v].x,
			triVertex[ kNextIndex[v][1] ].y - triVertex[v].y,
			triVertex[ kNextIndex[v][1] ].z - triVertex[v].z
        };
        /*
        edge2.x = triVertex[ kNextIndex[v][1] ].x - triVertex[v].x;
		edge2.y = triVertex[ kNextIndex[v][1] ].y - triVertex[v].y;
		edge2.z = triVertex[ kNextIndex[v][1] ].z - triVertex[v].z;
		*/
		
		// weight by angle

		double angle = TangentInfo::Vector3d::Dot(TangentInfo::Vector3d::Normalize(edge1), TangentInfo::Vector3d::Normalize(edge2));
		double w     = std::acos( clamp(angle, -1.0, 1.0) );

		out[v].tangent.x  = w * faceInfo.tangent.x;
		out[v].tangent.y  = w * faceInfo.tangent.y;
		out[v].tangent.z  = w * faceInfo.tangent.z;
		
		out[v].binormal.x = w * faceInfo.binormal.x;
		out[v].binormal.y = w * faceInfo.binormal.y;
		out[v].binormal.z = w * faceInfo.binormal.z;
    }

}
