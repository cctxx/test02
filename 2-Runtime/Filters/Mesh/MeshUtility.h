#ifndef MESHUTILITY_H
#define MESHUTILITY_H

#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Utilities/StrideIterator.h"
#include "Runtime/Utilities/dynamic_array.h"

struct Tangent;

// Calculate normals for the mesh, given vertex array and triangle list (3 indices per triangle).
void CalculateNormals( StrideIterator<Vector3f> verts, const UInt32* indices, int vertexCount, int triangleCount, StrideIterator<Vector3f> outNormals );

float CalculateSurfaceArea (const Matrix4x4f& objectToWorld, const Mesh::TemporaryIndexContainer& triangles, dynamic_array<Vector3f>& vertices);

// Use this to generate a normal from an tangent basis quickly
inline Vector3f NormalFromQuatTangentBasis (const Quaternionf& lhs)
{
	float x = lhs.x * 2.0F;
	float y = lhs.y * 2.0F;
	float z = lhs.z * 2.0F;
	float xx = lhs.x * x;
	float yy = lhs.y * y;
	float xz = lhs.x * z;
	float yz = lhs.y * z;
	float wx = lhs.w * x;
	float wy = lhs.w * y;

	Vector3f res;
	res.x = xz - wy;
	res.y = yz + wx;
	res.z = 1.0f - xx - yy;
	AssertIf (!CompareApproximately (res, RotateVectorByQuat(Inverse (lhs), Vector3f::zAxis)));
	return res;
}

//bool HasDegenerateTriangles (const Vector3f* verts, const MeshData &meshData, float degenerateArea = 0.0001);


#endif
