#include "UnityPrefix.h"

#if ENABLE_PHYSICS
#include "Runtime/Interfaces/IPhysics.h"
#include "PhysicsManager.h"
#include "RigidBody.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "External/PhysX/builds/SDKs/Cooking/include/NxCooking.h"
#include "Runtime/Dynamics/SkinnedCloth.h"
#include "Runtime/Dynamics/CapsuleCollider.h"
#include "Runtime/Dynamics/Collider.h"
#include "Runtime/Dynamics/MeshCollider.h"
#include "Runtime/Math/Random/rand.h"
#include "Runtime/Dynamics/NxMeshCreation.h"
#include "Runtime/Dynamics/nxmemorystream.h"

//only implementation.
class PhysicsModule : public IPhysics
{
public:	
	virtual void SetRigidBodyState( Rigidbody& rigidbody, const RigidBodyState& state )
	{
		rigidbody.SetPosition(state.position);
		rigidbody.SetRotation(state.rotation);
		rigidbody.SetVelocity(state.velocity);
		rigidbody.SetAngularVelocity(state.avelocity);	
	}

	virtual void GetRigidBodyState( const Rigidbody& rigidbody, RigidBodyState* result)
	{
		result->position = rigidbody.GetPosition();
		result->rotation = rigidbody.GetRotation();
		result->velocity = rigidbody.GetVelocity();
		result->avelocity = rigidbody.GetAngularVelocity();	
	}

	virtual Vector3f GetRigidBodyVelocity( const Rigidbody& rigidbody)
	{
		return rigidbody.GetVelocity();
	}

#if ENABLE_PROFILER
	virtual void GetProfilerStats(PhysicsStats& stats)
	{
		GetPhysicsManager().GetPerformanceStats(stats);		
	}
#endif

	virtual Vector3f GetGravity()
	{
		return GetPhysicsManager().GetGravity();
	}

	virtual void* CreateNxMeshFromNxStream(bool convex, const NxStream& stream)
	{
		return convex ? (void*)GetDynamicsSDK ().createConvexMesh(stream) : (void*)GetDynamicsSDK ().createTriangleMesh(stream);
	}

	virtual void ReleaseNxTriangleMesh(NxTriangleMesh& mesh)
	{
		GetDynamicsSDK ().releaseTriangleMesh (mesh);
	}

	virtual void ReleaseNxConvexMesh(NxConvexMesh& mesh)
	{
		GetDynamicsSDK ().releaseConvexMesh (mesh);
	}

#if ENABLE_CLOTH
	virtual void SetUpSkinnedBuffersOnSkinnedCloth (SkinnedCloth& cloth, void *vertices, void *normals, void *tangents, size_t bufferStride)
	{
		cloth.SetUpSkinnedBuffers(vertices,normals,tangents,bufferStride);
	}
#endif

	virtual void CapsuleColliderSetHeight(CapsuleCollider& collider, float height)
	{
		collider.SetHeight(height);
	}

	virtual ScriptingObjectPtr ConvertContactToMono (Collision* input)
	{
		return ::ConvertContactToMono(input);
	}

	virtual int GetColliderMaterialInstanceID(Collider& collider)
	{
		return collider.GetMaterial().GetInstanceID();
	}

	virtual int GetColliderSharedMeshInstanceID(MeshCollider& collider)
	{
		return collider.GetSharedMesh().GetInstanceID();
	}

	virtual bool CreateNxStreamFromUnityMesh (Mesh* mesh, bool convex, const Matrix4x4f& scalematrix, TransformType transformType, MemoryStream& stream )
	{
		return ::CreateNxStreamFromUnityMesh (mesh, convex, scalematrix, transformType, stream );
	}

	virtual void* CreateNxMeshFromUnityMesh (Mesh* mesh, bool convex, const Matrix4x4f& scalematrix, TransformType transformType )
	{
		return ::CreateNxMeshFromUnityMesh (mesh, convex, scalematrix, transformType );
	}

	virtual MemoryStream* CreateNxStreamFromUnityMesh(Mesh& meshData, bool convex)
	{
		return ::CreateNxStreamFromUnityMesh(meshData,convex);
	}

	virtual void DeleteMemoryStream(MemoryStream* stream)
	{
		delete stream;
	}

	virtual void ReleaseHeightField(NxHeightField& heightField)
	{
		GetDynamicsSDK().releaseHeightField(heightField);
	}

	virtual NxHeightField* CreateNxHeightField(NxHeightFieldDesc& desc)
	{
		return GetDynamicsSDK().createHeightField(desc);		
	}

};

void InitializePhysicsModule ()
{
	SetIPhysics(UNITY_NEW_AS_ROOT(PhysicsModule, kMemPhysics, "PhysicsInterface", ""));
}

void CleanupPhysicsModule ()
{
	PhysicsModule* module = reinterpret_cast<PhysicsModule*> (GetIPhysics ());
	UNITY_DELETE(module, kMemPhysics);
	SetIPhysics (NULL);
}

#endif