#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "SphereCollider.h"
#include "Runtime/Graphics/Transform.h"
#include "RigidBody.h"
#include "PhysicsManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/AABBUtility.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "Runtime/Misc/BuildSettings.h"

#define GET_SHAPE() ((class NxSphereShape*)m_Shape)

using namespace std;

SphereCollider::SphereCollider (MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{
	#if UNITY_EDITOR
	fixupSphereColliderBackwardsCompatibility = false;
	#endif
}
	
SphereCollider::~SphereCollider ()
{
}

void SphereCollider::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	if (m_Shape)
	{
		// Apply changed values
		SetRadius (m_Radius);
		SetCenter (m_Center);
	}
	
	Super::AwakeFromLoad (awakeMode);
	
	#if UNITY_EDITOR
	if (fixupSphereColliderBackwardsCompatibility)
	{
		float oldRadius = m_Radius;
		m_Radius = 1.0F;
		if (GetScaledRadius () > 0.001)
		{
			SetRadius (oldRadius / GetScaledRadius ());
		}
		fixupSphereColliderBackwardsCompatibility = false;
	}
	#endif
}	

void SphereCollider::SmartReset ()
{
	Super::Reset ();
	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		Vector3f dist = aabb.GetExtent ();
		float longestAxis = max (dist.x, dist.y);
		longestAxis = max (longestAxis, dist.z);
		SetRadius (longestAxis);
		SetCenter (aabb.GetCenter ());
	}
	else
	{
		SetRadius (0.5F);
		SetCenter (Vector3f::zero);
	}
}

void SphereCollider::Reset ()
{
	Super::Reset ();
	m_Radius = 0.5F;
	m_Center = Vector3f::zero;
}

float SphereCollider::GetScaledRadius () const
{
	Vector3f scale = GetComponent (Transform).GetWorldScaleLossy ();
	float absoluteRadius = max (max (Abs (scale.x), Abs (scale.y)), Abs (scale.z)) * m_Radius;
	absoluteRadius = Abs (absoluteRadius);
	absoluteRadius = max (absoluteRadius, 0.00001F);
	return absoluteRadius;
}

void SphereCollider::SetRadius (float radius)
{
	if (m_Radius != radius)
	{
		SetDirty ();
		m_Radius = radius;
	}
	
	PROFILE_MODIFY_STATIC_COLLIDER

	if (GET_SHAPE ())
	{
		GET_SHAPE ()->setRadius (GetScaledRadius ());
		RigidbodyMassDistributionChanged ();
		RefreshPhysicsInEditMode();
		UpdateCCDSkeleton ();
	}
}


void SphereCollider::SetCenter (const Vector3f& center)
{
	if (m_Center != center)
	{
		m_Center = center;
		SetDirty ();
	}
	
	if (GET_SHAPE ())
		TransformChanged (Transform::kRotationChanged | Transform::kPositionChanged | kForceUpdateMass);
}

Vector3f SphereCollider::GetGlobalCenter () const
{
	return GetComponent (Transform).TransformPoint (m_Center);
}

void SphereCollider::Create (const Rigidbody* ignoreRigidbody)
{
	if (m_Shape)
		Cleanup ();

	NxSphereShapeDesc shapeDesc;
	shapeDesc.radius = GetScaledRadius ();
	
	FinalizeCreate (shapeDesc, true, ignoreRigidbody);
}

void SphereCollider::FetchPoseFromTransform ()
{
	FetchPoseFromTransformUtility (m_Center);
}

bool SphereCollider::GetRelativeToParentPositionAndRotation (Transform& transform, Transform& anyParent, Matrix4x4f& matrix)
{
	return GetRelativeToParentPositionAndRotationUtility (transform, anyParent, m_Center, matrix);
}

void SphereCollider::ScaleChanged ()
{	
	GET_SHAPE ()->setRadius (GetScaledRadius ());
	UpdateCCDSkeleton ();
}

void SphereCollider::TransformChanged (int changeMask)
{
	Super::TransformChanged (changeMask);
	if (m_Shape)
	{
		if (!m_Shape->getActor ().userData)
		{
			PROFILER_AUTO(gStaticColliderMove, this)
			FetchPoseFromTransformUtility (m_Center);
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

AABB SphereCollider::GetBounds ()
{
	if (m_Shape)
	{
		// AABB reported by PhysX is inaccurate, as PhysX will just transform the local AABB.
		// For Spheres it's very easy to do better.
		float globalRadius = GetScaledRadius();
		return AABB(GetGlobalCenter(), Vector3f(globalRadius, globalRadius, globalRadius));
	}
	else 
		return Super::GetBounds ();
}

NxCCDSkeleton* SphereCollider::CreateCCDSkeleton(float scale)
{
	// This is a very simple "approximation" of a sphere, of only 6 vertices.
	// Since CCD only kicks in when normal collisions fail, any is probably mostly 
	// interesting for small objects (as those tend to move faster), I believe that this
	// is a reasonable choice for common expected use. NVidia even recommends using 
	// single vertex meshes for very small objects.
	float radius = scale;
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
		radius *= GetScaledRadius();
	else
		// Prior to 4.0 we incorrectly ignored object scale here. Keep this behaviour for backwards compatibility.
		radius *= m_Radius;
	
	NxU32 triangles[3 * 8] = {
		0,1,2,
		0,2,3,
		0,3,4,
		0,4,1,
		5,2,1,
		5,3,2,
		5,4,3,
		5,1,4,
	};

	NxVec3 points[6];

	// Static mesh
	points[0].set( 0, -radius, 0);
	points[1].set( -radius, 0, 0);
	points[2].set( 0, 0, -radius);
	points[3].set( radius, 0, 0);
	points[4].set( 0, 0, radius);
	points[5].set( 0, radius, 0);

	if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
	{
		// This is wrong, as the m_Center transformation is already applied at the shape matrix.
		// Keep for backward compatibility.
		NxVec3 center(m_Center.x, m_Center.y, m_Center.z);
		for (int i=0;i<6;i++)
			points[i] += center;
	}
	
	NxSimpleTriangleMesh stm;
	stm.numVertices = 6;
	stm.numTriangles = 8;
	stm.pointStrideBytes = sizeof(NxVec3);
	stm.triangleStrideBytes = sizeof(NxU32)*3;

	stm.points = points;
	stm.triangles = triangles;
	return GetDynamicsSDK().createCCDSkeleton(stm);
}

template<class TransferFunction>
void SphereCollider::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	transfer.SetVersion (2);
	
	transfer.Align();

	#if UNITY_EDITOR
	fixupSphereColliderBackwardsCompatibility = transfer.IsOldVersion (1);
	#endif

	TRANSFER_SIMPLE (m_Radius);
	TRANSFER (m_Center);
}

IMPLEMENT_CLASS (SphereCollider)
IMPLEMENT_OBJECT_SERIALIZE (SphereCollider)

#undef GET_SHAPE
#endif //ENABLE_PHYSICS