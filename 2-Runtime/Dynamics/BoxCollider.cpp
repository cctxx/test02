#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "BoxCollider.h"
#include "Runtime/Graphics/Transform.h"
#include "RigidBody.h"
#include "PhysicsManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/AABBUtility.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "NxWrapperUtility.h"
#include "Runtime/Misc/BuildSettings.h"

#define GET_SHAPE() static_cast<NxBoxShape*> (m_Shape)
BoxCollider::BoxCollider (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

BoxCollider::~BoxCollider ()
{
}

void BoxCollider::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	if (m_Shape)
	{
		// Apply changed values
		SetSize (m_Size);
		SetCenter (m_Center);
	}
	
	Super::AwakeFromLoad (awakeMode);
}	

void BoxCollider::SmartReset ()
{
	Super::SmartReset();
	
	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		SetSize (aabb.GetExtent () * 2.0F);
		SetCenter (aabb.GetCenter ());
	}
	else
	{
		SetSize (Vector3f::one);
		SetCenter (Vector3f::zero);
	}
}

void BoxCollider::Reset ()
{
	Super::Reset ();
	m_Center = Vector3f::zero;
	m_Size = Vector3f::one;
}

Vector3f BoxCollider::GetGlobalExtents () const
{
	Vector3f extents = GetComponent (Transform).GetWorldScaleLossy ();
	extents.Scale (m_Size);
	extents *= 0.5F;
	extents = Abs (extents);
	return extents;
}

Vector3f BoxCollider::GetGlobalCenter () const
{
	return GetComponent (Transform).TransformPoint (m_Center);
}

void BoxCollider::Create (const Rigidbody* ignoreRigidbody)
{
	if (m_Shape)
		Cleanup ();

	NxBoxShapeDesc shapeDesc;
	(Vector3f&)shapeDesc.dimensions = GetGlobalExtents ();
	
	FinalizeCreate (shapeDesc, true, ignoreRigidbody);
}

void BoxCollider::SetSize (const Vector3f& size)
{
	if (size != m_Size)
	{
		SetDirty ();
		m_Size = size;
	}
	
	PROFILE_MODIFY_STATIC_COLLIDER
	
	if (GET_SHAPE ())
	{
		GET_SHAPE ()->setDimensions (Vec3ToNx(GetGlobalExtents ()));
		RigidbodyMassDistributionChanged ();
		RefreshPhysicsInEditMode();
		UpdateCCDSkeleton ();
	}
}

void BoxCollider::SetCenter (const Vector3f& pos)
{
	if (pos != m_Center)
	{
		SetDirty ();
		m_Center = pos;
	}

	if (GET_SHAPE ())
		TransformChanged (Transform::kRotationChanged | Transform::kPositionChanged | kForceUpdateMass);
}

void BoxCollider::ScaleChanged ()
{	
	PROFILE_MODIFY_STATIC_COLLIDER
	
	NxBoxShape* shape = GET_SHAPE ();
	shape->setDimensions (Vec3ToNx(GetGlobalExtents ()));
	
	UpdateCCDSkeleton ();
}

void BoxCollider::FetchPoseFromTransform ()
{
	FetchPoseFromTransformUtility (m_Center);
}

bool BoxCollider::GetRelativeToParentPositionAndRotation (Transform& transform, Transform& anyParent, Matrix4x4f& matrix)
{
	return GetRelativeToParentPositionAndRotationUtility (transform, anyParent, m_Center, matrix);
}

void BoxCollider::TransformChanged (int changeMask)
{
	Super::TransformChanged (changeMask);
	if (m_Shape)
	{
		if (m_Shape->getActor ().userData == NULL)
		{
			PROFILER_AUTO(gStaticColliderMove, this)
			FetchPoseFromTransformUtility (m_Center);
/*			
			AssertIf (m_Shape == NULL);
			AssertIf (HasActorRigidbody ());
			Transform& transform = GetComponent (Transform);
			Vector3f p = transform.TransformPoint (m_Center);
			m_Shape->getActor ().setGlobalPosition ((const NxVec3&)p);
			Matrix3x3f m = transform.GetWorldRotationAndScale ();
			//OrthoNormalize (m);
			m_Shape->getActor ().setGlobalOrientation ((NxMat33&)m);
//			m_Shape->getActor ().setGlobalOrientationQuat ((const NxQuat&)transform.GetRotation ());
*/
		}
		else
		{
			Rigidbody* body = (Rigidbody*)m_Shape->getActor ().userData;
			Matrix4x4f matrix;
			if (GetRelativeToParentPositionAndRotationUtility (GetComponent (Transform), body->GetComponent (Transform), m_Center, matrix))
			{
				NxMat34 shapeMatrix;
				shapeMatrix.setColumnMajor44 (matrix.GetPtr ());
				m_Shape->setLocalPose (shapeMatrix);
			}
			
			if (body->GetGameObjectPtr() != GetGameObjectPtr() || changeMask & (Transform::kScaleChanged | kForceUpdateMass))
				RigidbodyMassDistributionChanged ();
		}
		
		if (changeMask & Transform::kScaleChanged)
			ScaleChanged ();

		RefreshPhysicsInEditMode();
	}
}

NxCCDSkeleton* BoxCollider::CreateCCDSkeleton(float scale)
{
	Vector3f size = Vector3f::one * scale;
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
		size.Scale(GetGlobalExtents());
	else
		// Prior to 4.0 we incorrectly ignored object scale here. Keep this behaviour for backwards compatibility.
		size.Scale(m_Size * 0.5f);

	NxU32 triangles[3 * 12] = {
		0,1,3,
		0,3,2,
		3,7,6,
		3,6,2,
		1,5,7,
		1,7,3,
		4,6,7,
		4,7,5,
		1,0,4,
		5,1,4,
		4,0,2,
		4,2,6
	};

	NxVec3 points[8];
	// Static mesh
	points[0].set( size.x, -size.y, -size.z);
	points[1].set( size.x, -size.y,  size.z);
	points[2].set( size.x,  size.y, -size.z);
	points[3].set( size.x,  size.y,  size.z);

	points[4].set(-size.x, -size.y, -size.z);
	points[5].set(-size.x, -size.y,  size.z);
	points[6].set(-size.x,  size.y, -size.z);
	points[7].set(-size.x,  size.y,  size.z);

	if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
	{
		// This is wrong, as the m_Center transformation is already applied at the shape matrix.
		// Keep for backward compatibility.
		NxVec3 center(m_Center.x, m_Center.y, m_Center.z);
		for (int i=0;i<8;i++)
			points[i] += center;
	}
	
	NxSimpleTriangleMesh stm;
	stm.numVertices = 8;
	stm.numTriangles = 6*2;
	stm.pointStrideBytes = sizeof(NxVec3);
	stm.triangleStrideBytes = sizeof(NxU32)*3;

	stm.points = points;
	stm.triangles = triangles;
	return GetDynamicsSDK().createCCDSkeleton(stm);
}

template<class TransferFunction>
void BoxCollider::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	transfer.SetVersion (2);
	transfer.Align();
	if (transfer.IsCurrentVersion ())
	{
		TRANSFER_SIMPLE (m_Size);
	}
	else
	{
		transfer.Transfer (m_Size, "m_Extents");
		m_Size*=2.0F;
	}
	TRANSFER (m_Center);
}

IMPLEMENT_CLASS (BoxCollider)
IMPLEMENT_OBJECT_SERIALIZE (BoxCollider)

#undef GET_SHAPE
#endif //ENABLE_PHYSICS