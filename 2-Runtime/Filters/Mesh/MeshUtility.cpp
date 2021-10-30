#include "UnityPrefix.h"
#include "MeshUtility.h"
#include "Runtime/Geometry/Plane.h"
#include "Mesh.h"

using namespace std;

void CalculateNormals (StrideIterator<Vector3f> verts, const UInt32* indices, int vertexCount, int triangleCount, StrideIterator<Vector3f> outNormals)
{
	std::fill_n (outNormals, vertexCount, Vector3f(0,0,0));
	
	// Add normals from faces
	int idx = 0;
	for( int i = 0; i < triangleCount; ++i )
	{
		UInt32 index0 = indices[idx+0];
		UInt32 index1 = indices[idx+1];
		UInt32 index2 = indices[idx+2];
		Vector3f faceNormal = CalcRawNormalFromTriangle( verts[index0], verts[index1], verts[index2] );
		outNormals[index0] += faceNormal;
		outNormals[index1] += faceNormal;
		outNormals[index2] += faceNormal;
		idx += 3;
	}

	// Normalize
	for (StrideIterator<Vector3f> end = outNormals + vertexCount; outNormals != end; ++outNormals )
	{
		*outNormals = NormalizeFast (*outNormals);
	}
}


float CalculateSurfaceArea (
	const Matrix4x4f& objectToWorld,
	const Mesh::TemporaryIndexContainer& triangles,
	dynamic_array<Vector3f>& vertices)
{
	// transform the vertices to world space,
	// do it in place since they are a copy
	for (int i = 0; i < vertices.size (); i++)
		vertices[i] = objectToWorld.MultiplyPoint3 (vertices[i]);

	// calculate the area
	float cachedSurfaceArea = 0;
	for (int i = 0; i < triangles.size () / 3; i++)
	{	
		DebugAssert (triangles[3 * i] < vertices.size ());
		DebugAssert (triangles[3 * i + 1] < vertices.size ());
		DebugAssert (triangles[3 * i + 2] < vertices.size ());
		Vector3f a = vertices[triangles[3 * i]];
		Vector3f b = vertices[triangles[3 * i + 1]];
		Vector3f c = vertices[triangles[3 * i + 2]];
		cachedSurfaceArea += Magnitude (Cross (b - a, c - a)) * 0.5f;
	}

	return cachedSurfaceArea;
}
