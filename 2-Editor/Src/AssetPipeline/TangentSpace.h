#ifndef TANGENT_SPACE_H
#define TANGENT_SPACE_H


class Vector2f;
class Vector3f;


struct
TangentInfo
{
	struct Vector3d
	{
		double	x, y, z;

		static double	Dot(Vector3d v1, Vector3d v2);
		static double	Magnitude(Vector3d v);
		static Vector3d	Normalize(Vector3d v, double mag);
		static Vector3d	Normalize(Vector3d v);
	};

	Vector3d	tangent;
	Vector3d	binormal;
};


void ComputeTriangleTangentBasis (const Vector3f* vertices, const Vector2f* uvs, const UInt32* indices, TangentInfo out[3]);


#endif
