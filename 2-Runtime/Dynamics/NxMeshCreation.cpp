#include "UnityPrefix.h"

#if ENABLE_PHYSICS
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Dynamics/ExtractDataFromMesh.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include "Runtime/Dynamics/nxmemorystream.h"
#include "External/PhysX/builds/SDKs/Cooking/include/NxCooking.h"
#include "Runtime/Interfaces/IPhysics.h"
#include "Runtime/Profiler/Profiler.h"

PROFILER_INFORMATION(gBakeCollisionMesh, "Mesh.Bake PhysX CollisionData", kProfilerPhysics)
PROFILER_INFORMATION(gBakeCollisionScaledMesh, "Mesh.Bake Scaled Mesh PhysX CollisionData", kProfilerPhysics)

bool CreateNxStreamFromUnityMesh (Mesh* mesh, bool convex, const Matrix4x4f& scalematrix, TransformType transformType, MemoryStream& stream )
{
	dynamic_array<Vector3f> vertices;
	dynamic_array<Vector3f> normals;
	dynamic_array<UInt16> triangles;
	dynamic_array<UInt16> remap;
	if (!ExtractDataFromMesh(*mesh, vertices, triangles, remap))
		return false;

	int vertexCount = vertices.size();
	int	inStride = sizeof(Vector3f);
	if (!IsNoScaleTransform(transformType))
		TransformPoints3x3 (scalematrix, &vertices[0], sizeof(Vector3f), &vertices[0], sizeof(Vector3f), vertexCount);
	
	// PhysX crashes when using only 1 triangle
	// So just duplicate the triangle...
	if (triangles.size() == 3)
	{
		triangles.push_back(triangles[0]);
		triangles.push_back(triangles[1]);
		triangles.push_back(triangles[2]);
	}
	
	if (convex)
	{
		NxConvexMeshDesc desc;
		desc.flags = NX_CF_COMPUTE_CONVEX;
		
		desc.numVertices = vertexCount;
		desc.points = &vertices[0];
		desc.pointStrideBytes = inStride;
				
		return NxCookConvexMesh (desc, stream);
	}
	else
	{
		NxTriangleMeshDesc desc;
		
		desc.numVertices = vertexCount;
		desc.points = &vertices[0];
		desc.pointStrideBytes = inStride;
		
		desc.numTriangles = triangles.size () / 3;
		desc.triangles = &triangles[0];
		desc.triangleStrideBytes = sizeof (triangles[0]) * 3;
		desc.flags = NX_MF_16_BIT_INDICES;

		if (transformType & kOddNegativeScaleTransform)
			desc.flags |= NX_MF_FLIPNORMALS;
		
		return NxCookTriangleMesh (desc, stream);
	}
}

MemoryStream* CreateNxStreamFromUnityMesh(Mesh& meshData, bool convex)
{
	PROFILER_AUTO_THREAD_SAFE(gBakeCollisionMesh, &meshData)
	Matrix4x4f identity; identity.SetIdentity();
	MemoryStream* stream = new MemoryStream (NULL, 0);
	CreateNxStreamFromUnityMesh(&meshData, convex, identity, kNoScaleTransform, *stream);
	return stream;
}

void* CreateNxMeshFromUnityMesh (Mesh* mesh, bool convex, const Matrix4x4f& scalematrix, TransformType transformType )
{
#if ENABLE_PROFILER
	if (IsNoScaleTransform(transformType))
	{	
		PROFILER_BEGIN(gBakeCollisionMesh, mesh)
	}
	else
	{
		PROFILER_BEGIN(gBakeCollisionScaledMesh, mesh)
	}
#endif
	
	MemoryStream stream (NULL, 0);
	
	if (!CreateNxStreamFromUnityMesh(mesh, convex, scalematrix, transformType, stream))
	{
		PROFILER_END
		return NULL;
	}
	
	if (convex)
	{
		NxConvexMesh* nxmesh = GetDynamicsSDK().createConvexMesh (stream);
		PROFILER_END
		return nxmesh;
	}		
	else
	{
		NxTriangleMesh* nxmesh = GetDynamicsSDK().createTriangleMesh (stream);
		PROFILER_END
		return nxmesh;
	}
}

#endif