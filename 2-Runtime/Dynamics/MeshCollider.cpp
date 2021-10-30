#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "MeshCollider.h"
#include "Runtime/Graphics/Transform.h"
#include "RigidBody.h"
#include "PhysicsManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/AABBUtility.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "nxmemorystream.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "External/PhysX/builds/SDKs/Cooking/include/NxCooking.h"
#include "Runtime/Misc/BuildSettings.h"

#define GET_CONVEX_SHAPE() ((class NxConvexShape*)m_Shape)
#define GET_MESH_SHAPE() ((class NxTriangleMeshShape*)m_Shape)

#if UNITY_EDITOR
const NxConvexMesh* MeshCollider::GetConvexMesh() const
{
	if( !m_Shape )
		return NULL;
	if( !m_Shape->isConvexMesh() )
		return NULL;
	const NxConvexMesh& mesh = GET_CONVEX_SHAPE ()->getConvexMesh ();
	return &mesh;
}
const NxTriangleMesh* MeshCollider::GetTriangleMesh() const
{
	if( !m_Shape )
		return NULL;
	if( m_Shape->isConvexMesh() )
		return NULL;
	const NxTriangleMesh& mesh = GET_MESH_SHAPE ()->getTriangleMesh ();
	return &mesh;
}
#endif

MeshCollider::MeshCollider (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_MeshNode(this)
{
	m_Shared = false;
}

MeshCollider::~MeshCollider ()
{
}

void MeshCollider::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	if (m_Shape)
	{
		// Apply changed values
		if (m_Convex != (m_Shape->isConvexMesh() != NULL))
			SetConvex(m_Convex);
			
		if (m_Shape)
		{
			bool smooth;
			if (m_Shape->isConvexMesh())
			{
				NxConvexShapeDesc desc;
				GET_CONVEX_SHAPE ()->saveToDesc(desc);
				smooth = desc.meshFlags & NX_MESH_SMOOTH_SPHERE_COLLISIONS;
			}
			else
			{
				NxTriangleMeshShapeDesc desc;
				GET_MESH_SHAPE ()->saveToDesc(desc);
				smooth = desc.meshFlags & NX_MESH_SMOOTH_SPHERE_COLLISIONS;
			}
			if (smooth != m_SmoothSphereCollisions)
				SetSmoothSphereCollisions(m_SmoothSphereCollisions);
			SetSharedMesh(m_Mesh);
		}
	}
	
	Super::AwakeFromLoad (awakeMode);
}	

void MeshCollider::Cleanup ()
{
	m_MeshNode.RemoveFromList();
	if (m_Shape)
	{
		if (m_Shape->isConvexMesh())
		{
			NxConvexMesh& mesh = GET_CONVEX_SHAPE ()->getConvexMesh ();
			Super::Cleanup ();
			if (!m_Shared)
				GetDynamicsSDK ().releaseConvexMesh (mesh);
		}
		else
		{
			NxTriangleMesh& mesh = GET_MESH_SHAPE ()->getTriangleMesh ();
			Super::Cleanup ();
			if (!m_Shared)
				GetDynamicsSDK ().releaseTriangleMesh (mesh);
		}
	}
}

void MeshCollider::DidDeleteMesh ()
{
	Cleanup ();
}

void MeshCollider::CreateShape( void* nxmesh, const Rigidbody* ignoreRigidbody )
{
	if ( !nxmesh )
		return;
	
	if (m_Convex)
	{
		NxConvexShapeDesc shapeDesc;
		shapeDesc.meshData = (NxConvexMesh*)nxmesh;
		
		if (m_SmoothSphereCollisions)
			shapeDesc.meshFlags |= NX_MESH_SMOOTH_SPHERE_COLLISIONS;
		
		FinalizeCreate (shapeDesc, true, ignoreRigidbody);
	}
	else
	{
		NxTriangleMeshShapeDesc shapeDesc;
		
		if (m_SmoothSphereCollisions)
			shapeDesc.meshFlags |= NX_MESH_SMOOTH_SPHERE_COLLISIONS;
		
		shapeDesc.meshData = (NxTriangleMesh*)nxmesh;
		
		FinalizeCreate (shapeDesc, true, ignoreRigidbody);
	}
}

//@TODO: Updating physics mesh when reimporting no longer works!

void MeshCollider::Create (const Rigidbody* ignoreRigidbody)
{
	if (m_Shape)
		Cleanup ();

	Mesh* mesh = m_Mesh;
	m_CachedMesh = mesh;
	if (mesh == NULL)
		return;
	// do not create anything if have no vertices or less-than-one triangle
	if (mesh->GetVertexCount() == 0 || mesh->GetPrimitiveCount() == 0)
		return;

	/*
	NxCookingParams params;
	params.hintCollisionSpeed = true;
	NxSetCookingParams (params);
	*/

	Matrix4x4f scalematrix;

	TransformType type = GetComponent (Transform).CalculateTransformMatrixScaleDelta (scalematrix);

	void* nxmesh = NULL;
	if (IsNoScaleTransform(type) && !mesh->IsSharedPhysicsMeshDirty()) 
	{
		m_Shared = true;
		
		if (m_Convex)
		{
			nxmesh = mesh->GetSharedNxConvexMesh ();
		}
		else
		{
			nxmesh = mesh->GetSharedNxMesh ();
		}
	}
	// For scaled meshes, create instance
	else
	{
		m_Shared = false;
		nxmesh = CreateNxMeshFromUnityMesh(mesh, m_Convex, scalematrix, type);
	}
	
	if (nxmesh == NULL)	
		return;
	
	mesh->AddObjectUser( m_MeshNode );
		
	CreateShape( nxmesh, ignoreRigidbody );
}

void MeshCollider::ReCreate()
{
	if( !m_Shape )
		return;
	// Re-creating the full mesh collider is expensive, so we only re-create the shape and not the mesh.
	if ( m_Shape->isConvexMesh() )
	{
		NxConvexMesh& mesh = GET_CONVEX_SHAPE ()->getConvexMesh ();
		Super::Cleanup();
		CreateShape( &mesh, NULL );
	}
	else
	{
		NxTriangleMesh& mesh = GET_MESH_SHAPE ()->getTriangleMesh ();
		Super::Cleanup();
		CreateShape( &mesh, NULL );
	}
}

void MeshCollider::ScaleChanged ()
{
	Create (NULL);
}

void MeshCollider::TransformChanged (int changeMask)
{	
	Super::TransformChanged (changeMask);
	if (m_Shape)
	{
		if (m_Shape->getActor ().userData == NULL)
		{
			PROFILER_AUTO(gStaticColliderMove, this)
			FetchPoseFromTransform ();
		}
		else
		{
			Rigidbody* body = (Rigidbody*)m_Shape->getActor ().userData;
			Matrix4x4f matrix;
			if (GetRelativeToParentPositionAndRotation (GetComponent (Transform), body->GetComponent (Transform), matrix))
			{
				NxMat34 shapeMatrix;
				shapeMatrix.setColumnMajor44 (matrix.GetPtr ());
				m_Shape->setLocalPose (shapeMatrix);
			}

			if (body->GetGameObjectPtr() != GetGameObjectPtr() || changeMask & Transform::kScaleChanged)
				RigidbodyMassDistributionChanged ();
		}
				
		if (changeMask & Transform::kScaleChanged)
			ScaleChanged ();

		RefreshPhysicsInEditMode();
	}
	else if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1) && IsActive() && GetEnabled())
		Create (NULL);		
}

void MeshCollider::InitializeClass ()
{
	REGISTER_MESSAGE_VOID(MeshCollider, kDidDeleteMesh, DidDeleteMesh);
}

void MeshCollider::SetSharedMesh (const PPtr<Mesh> m)
{
	if (m_CachedMesh != m)
	{
		SetDirty ();
		m_Mesh = m;
		if (IsActive())
			Create(NULL);
	}
}

PPtr<Mesh> MeshCollider::GetSharedMesh ()
{
	return m_Mesh;
}

void MeshCollider::Reset ()
{
	Super::Reset ();
	if (GetGameObjectPtr ())
	{
		MeshFilter* filter = QueryComponent (MeshFilter);
		if (filter && m_Mesh.GetInstanceID () == 0)
		{
			PPtr<Mesh> newMesh = filter->GetSharedMesh ();
			if (newMesh != m_Mesh)
			{
				m_Mesh = newMesh;
				if (IsActive())
					Create(NULL);
			}
		}
	}
	m_SmoothSphereCollisions = false;
	m_Convex = false;
}

template<class TransferFunction>
void MeshCollider::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	transfer.SetVersion (2);
	
	TRANSFER (m_SmoothSphereCollisions);
	transfer.Transfer (m_Convex, "m_Convex");
	transfer.Align();
	TRANSFER (m_Mesh);
}

void MeshCollider::SetConvex (bool convex)
{
	m_Convex = convex;
	if (m_Shape)
		Create(NULL);
	SetDirty();
	RefreshPhysicsInEditMode();
}

void MeshCollider::SetSmoothSphereCollisions (bool smooth)
{
	m_SmoothSphereCollisions = smooth;
	if (m_Shape)
		Create(NULL);
	SetDirty();
	RefreshPhysicsInEditMode();
}

IMPLEMENT_CLASS_HAS_INIT (MeshCollider)
IMPLEMENT_OBJECT_SERIALIZE (MeshCollider)
#endif //ENABLE_PHYSICS
