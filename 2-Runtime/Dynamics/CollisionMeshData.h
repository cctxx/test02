#ifndef COLLISION_MESH_DATA
#define COLLISION_MESH_DATA

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Editor/Interfaces/IPhysicsEditor.h"

class Mesh;

enum
{
	kMeshMustKeepVertexAndIndexData = 1 << 0,
	kRequiresSharedConvexCollisionMesh = 1 << 1, // Mesh can be shared and should be precomputed on standalone platforms
	kRequiresSharedTriangleCollisionMesh = 1 << 2, // Mesh can be shared and should be precomputed on standalone platforms
	kRequiresScaledCollisionMesh = 1 << 3 // Mesh is used as mesh collider but is scaled
};

class EXPORT_COREMODULE CollisionMeshData
{
	void*			      m_NxConvexMesh;
	void*			      m_NxTriangleMesh;
	bool                  m_SharedPhysicsMeshDirty;
	
public:
		
#if USE_PREBAKED_COLLISIONMESH
	dynamic_array<UInt8>  m_BakedTriangleCollisionMesh;
	dynamic_array<UInt8>  m_BakedConvexCollisionMesh;
#endif

	CollisionMeshData ();
	~CollisionMeshData ();
	
	void Cleanup ();
	
	template<class TransferFunction>
	void Transfer (TransferFunction& transfer, Mesh& mesh);
	
	void VertexDataHasChanged ();
	
	void AwakeFromLoad (AwakeFromLoadMode awake);
	void AwakeFromLoadThreaded(Mesh& meshData);
	
	void* GetSharedNxMesh (Mesh& mesh);	
	void* GetSharedNxConvexMesh (Mesh& mesh);
	
	bool IsSharedPhysicsMeshDirty () { return m_SharedPhysicsMeshDirty; }
	
};

EXPORT_COREMODULE void* CreateNxMeshFromUnityMesh (Mesh* mesh, bool convex, const Matrix4x4f& scalematrix, TransformType transformType );

template<class TransferFunction>
inline void CollisionMeshData::Transfer (TransferFunction& transfer, Mesh& mesh)
{
#if UNITY_EDITOR
	// When building player we precalcuate mesh usage based on who uses the different MeshColliders in different scenes.
 	if (transfer.IsWritingGameReleaseData())
	{
		int buildMeshUsageFlags = transfer.GetBuildUsage().meshUsageFlags;
		
		// Bake physX meshes
		if (transfer.GetFlags() & kGenerateBakedPhysixMeshes)
		{
			IPhysicsEditor* physicsEditor = GetIPhysicsEditor();
			Assert(physicsEditor != NULL) ;

			dynamic_array<UInt8> bakedConvex;
			if (buildMeshUsageFlags & kRequiresSharedConvexCollisionMesh)
				physicsEditor->BakeMesh (&mesh, true, ShouldSerializeForBigEndian(transfer), bakedConvex);
			transfer.Transfer (bakedConvex, "m_BakedConvexCollisionMesh", kHideInEditorMask);
			
			dynamic_array<UInt8> bakedConcave;
			if (buildMeshUsageFlags & kRequiresSharedTriangleCollisionMesh)
				physicsEditor->BakeMesh (&mesh, false, ShouldSerializeForBigEndian(transfer), bakedConcave);
			transfer.Transfer (bakedConcave, "m_BakedTriangleCollisionMesh", kHideInEditorMask);
		}
	}
#endif
#if USE_PREBAKED_COLLISIONMESH
	transfer.Transfer (m_BakedConvexCollisionMesh, "m_BakedConvexCollisionMesh", kHideInEditorMask);
	transfer.Transfer (m_BakedTriangleCollisionMesh, "m_BakedTriangleCollisionMesh", kHideInEditorMask);
#endif
}

#endif