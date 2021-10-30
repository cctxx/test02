#pragma once

#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Utilities/NonCopyable.h"
#include "Runtime/Scripting/ScriptingUtility.h"

class Rigidbody;
struct PhysicsStats;
class NxTriangleMesh;
class NxConvexMesh;
class NxStream;
class CapsuleCollider;
struct Collision;
namespace Unity { class SkinnedCloth; }
class Collider;
class MeshCollider;
class NxConvexMeshDesc;
class NxTriangleMeshDesc;
class MemoryStream;
class Mesh;
class NxHeightField;
class NxHeightFieldDesc;

class EXPORT_COREMODULE IPhysics : public NonCopyable
{
public:
	// Collision intersection
	struct RigidBodyState
	{
		Vector3f position;
		Quaternionf rotation;
		Vector3f velocity;
		Vector3f avelocity;
	};

	virtual void SetRigidBodyState( Rigidbody& rigidbody, const RigidBodyState& state ) = 0;
	virtual void GetRigidBodyState( const Rigidbody& rigidbody, RigidBodyState* result) = 0;
	virtual Vector3f GetRigidBodyVelocity( const Rigidbody& rigidbody) = 0;
	virtual Vector3f GetGravity() = 0;

	virtual void* CreateNxMeshFromNxStream(bool convex, const NxStream& stream) = 0;

	virtual void ReleaseNxTriangleMesh(NxTriangleMesh& mesh) = 0;
	virtual void ReleaseNxConvexMesh(NxConvexMesh& mesh) = 0;

	virtual void CapsuleColliderSetHeight(CapsuleCollider& collider, float height) = 0;

	virtual ScriptingObjectPtr ConvertContactToMono (Collision* input) = 0;
	virtual int GetColliderMaterialInstanceID(Collider& collider) = 0;
	virtual int GetColliderSharedMeshInstanceID(MeshCollider& collider) = 0;

	virtual bool CreateNxStreamFromUnityMesh (Mesh* mesh, bool convex, const Matrix4x4f& scalematrix, TransformType transformType, MemoryStream& stream ) = 0;
	virtual void* CreateNxMeshFromUnityMesh (Mesh* mesh, bool convex, const Matrix4x4f& scalematrix, TransformType transformType ) = 0;
	virtual MemoryStream* CreateNxStreamFromUnityMesh(Mesh& meshData, bool convex) = 0;

	virtual void DeleteMemoryStream(MemoryStream* memory) = 0;

	virtual void ReleaseHeightField(NxHeightField& heightField) = 0;
	virtual NxHeightField* CreateNxHeightField(NxHeightFieldDesc& desc) = 0;

#if ENABLE_CLOTH
	virtual void SetUpSkinnedBuffersOnSkinnedCloth (Unity::SkinnedCloth& cloth, void *vertices, void *normals, void *tangents, size_t bufferStride) = 0;
#endif

#if ENABLE_PROFILER
	virtual void GetProfilerStats(PhysicsStats& stats) = 0;
#endif
};

EXPORT_COREMODULE IPhysics* GetIPhysics();
EXPORT_COREMODULE void SetIPhysics(IPhysics* physics);