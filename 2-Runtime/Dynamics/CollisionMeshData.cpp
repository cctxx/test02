#include "UnityPrefix.h"
#include "CollisionMeshData.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Profiler/Profiler.h"
//#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
//#include "Runtime/Dynamics/PhysicsManager.h"
#include "Runtime/Dynamics/nxmemorystream.h"
//#include "External/PhysX/builds/SDKs/Cooking/include/NxCooking.h"
#include "Runtime/Interfaces/IPhysics.h"
//#include "Runtime/Dynamics/ExtractDataFromMesh.h"

CollisionMeshData::CollisionMeshData ()
{
	m_NxConvexMesh = NULL;
	m_NxTriangleMesh = NULL;
	m_SharedPhysicsMeshDirty = false;
}

CollisionMeshData::~CollisionMeshData ()
{
	Cleanup ();
}

void CollisionMeshData::Cleanup ()
{
	if (m_NxTriangleMesh)
	{
		GetIPhysics()->ReleaseNxTriangleMesh(*reinterpret_cast<NxTriangleMesh*> (m_NxTriangleMesh));
		m_NxTriangleMesh = NULL;
	}
	
	if (m_NxConvexMesh)
	{
		GetIPhysics()->ReleaseNxConvexMesh(*reinterpret_cast<NxConvexMesh*> (m_NxConvexMesh));
		m_NxConvexMesh = NULL;
	}
}

void CollisionMeshData::VertexDataHasChanged ()
{
	if (m_NxConvexMesh != NULL || m_NxTriangleMesh != NULL)
		m_SharedPhysicsMeshDirty = true;
}

void* CollisionMeshData::GetSharedNxMesh (Mesh& meshData)
{
#if ENABLE_PHYSICS
	if (m_NxTriangleMesh != NULL)
		return m_NxTriangleMesh;
	Matrix4x4f identity;  identity.SetIdentity();
	m_NxTriangleMesh = GetIPhysics()->CreateNxMeshFromUnityMesh(&meshData, false, identity, kNoScaleTransform);
#endif
	return m_NxTriangleMesh;
}


void* CollisionMeshData::GetSharedNxConvexMesh (Mesh& meshData)
{
#if ENABLE_PHYSICS
	if (m_NxConvexMesh != NULL)
		return m_NxConvexMesh;
	Matrix4x4f identity;  identity.SetIdentity();
	m_NxConvexMesh = GetIPhysics()->CreateNxMeshFromUnityMesh(&meshData, true, identity, kNoScaleTransform);
#endif
	return m_NxConvexMesh;
}

void CollisionMeshData::AwakeFromLoadThreaded(Mesh& meshData)
{
#if !USE_PREBAKED_COLLISIONMESH && ENABLE_PHYSICS
	int meshUsageFlags = meshData.GetMeshUsageFlags();
	IPhysics* physics = GetIPhysics();

	if (meshUsageFlags & kRequiresSharedTriangleCollisionMesh)
		m_NxTriangleMesh = physics->CreateNxStreamFromUnityMesh(meshData, false);

	if (meshUsageFlags & kRequiresSharedConvexCollisionMesh)
		m_NxConvexMesh = physics->CreateNxStreamFromUnityMesh(meshData, true);
#endif	
}

void CollisionMeshData::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	// Mark the shared physics mesh dirty when we have a mesh representation already and we are modifying the mesh (as opposed to just loading it from disk)
	if ((m_NxConvexMesh != NULL || m_NxTriangleMesh != NULL) && (awakeMode & kDidLoadFromDisk) == 0 )
		m_SharedPhysicsMeshDirty = true;

	IPhysics* physics = GetIPhysics();

#if USE_PREBAKED_COLLISIONMESH
	if (!m_BakedConvexCollisionMesh.empty())
	{
		m_NxConvexMesh = physics->CreateNxMeshFromByteStream(TRUE,m_BakedConvexCollisionMesh);
		m_BakedConvexCollisionMesh.clear();
	}
	if (!m_BakedTriangleCollisionMesh.empty())
	{
		m_NxTriangleMesh = physics->CreateNxMeshFromByteStream(FALSE,m_BakedTriangleCollisionMesh);
		m_BakedTriangleCollisionMesh.clear();
	}
#else
	
	if (awakeMode & kDidLoadThreaded)
	{
		if (m_NxTriangleMesh)
		{
			MemoryStream* stream = reinterpret_cast<MemoryStream*>(m_NxTriangleMesh);
			m_NxTriangleMesh = physics->CreateNxMeshFromNxStream (false, *stream);
			physics->DeleteMemoryStream(stream);
		}
		
		if (m_NxConvexMesh)
		{
			MemoryStream* stream = reinterpret_cast<MemoryStream*>(m_NxConvexMesh);
			m_NxConvexMesh = physics->CreateNxMeshFromNxStream (true, *stream);
			physics->DeleteMemoryStream(stream);
		}
	}	
#endif
}
