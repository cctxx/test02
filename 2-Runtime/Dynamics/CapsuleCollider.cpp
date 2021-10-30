#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "CapsuleCollider.h"
#include "Runtime/Graphics/Transform.h"
#include "RigidBody.h"
#include "PhysicsManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/AABBUtility.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "Runtime/Misc/BuildSettings.h"

#define GET_SHAPE() ((class NxCapsuleShape*)m_Shape)

/*
 - i am not sure about the getscaled extents calculation.
 
*/

using namespace std;

CapsuleCollider::CapsuleCollider (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

CapsuleCollider::~CapsuleCollider ()
{
}

void CapsuleCollider::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	if (m_Shape)
	{
		// Apply changed values
		SetRadius(m_Radius);
		SetHeight(m_Height);
		SetCenter(m_Center);
		SetDirection (m_Direction);
	}
	
	Super::AwakeFromLoad (awakeMode);
}	

void CapsuleCollider::SmartReset ()
{
	Super::SmartReset();
	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		Vector3f extents = aabb.GetExtent ();
		SetRadius (max (extents.x, extents.z));
		SetHeight (extents.y * 2.0F);
		SetCenter (aabb.GetCenter ());
	}
	else
	{
		SetRadius (0.5F);
		SetHeight (1.0F);
		SetCenter (Vector3f::zero);
	}
}

void CapsuleCollider::Reset ()
{
	Super::Reset ();
	m_Radius = 0.5F;
	m_Height = 1.0F;
	m_Center = Vector3f::zero;
	m_Direction = 1;
}


Vector2f CapsuleCollider::GetGlobalExtents () const
{
	const float kMinSize = 0.00001F;
	Vector3f scale = GetComponent (Transform).GetWorldScaleLossy ();

	float absoluteHeight = max (Abs (m_Height * scale.y), kMinSize);
	float absoluteRadius = max (Abs (scale.x), Abs (scale.z)) * m_Radius;
	
	float height = absoluteHeight - absoluteRadius * 2.0F;
	
	height = max (height, kMinSize);
	absoluteRadius = max (absoluteRadius, kMinSize);

	return Vector2f (absoluteRadius, height);
}

Vector3f CapsuleCollider::GetGlobalCenter () const
{
	return GetComponent (Transform).TransformPoint (m_Center);
}

AABB CapsuleCollider::GetBounds ()
{
	if (m_Shape)
	{
		// AABB reported by PhysX is inaccurate, as PhysX will just transform the local AABB.
		// For Capsules it's very easy to do better.
		
		Vector2f extents = GetGlobalExtents();
		Matrix4x4f m = CalculateTransform ();
		
		Vector3f center1 = m.MultiplyPoint3 (Vector3f(0, extents.y * 0.5, 0));
		Vector3f center2 = m.MultiplyPoint3 (Vector3f(0, -extents.y * 0.5, 0));

		// Make AABB of both global centers
		AABB aabb (center1, Vector3f::zero);
		aabb.Encapsulate (center2);
		
		// Expand by global radius
		aabb.m_Extent += Vector3f(extents.x, extents.x, extents.x);
		return aabb;
	}
	else 
		return Super::GetBounds ();
}

void CapsuleCollider::Create (const Rigidbody* ignoreRigidbody)
{
	if (m_Shape)
		Cleanup ();

	NxCapsuleShapeDesc shapeDesc;
	Vector2f extents = GetGlobalExtents ();
	shapeDesc.radius = extents.x;
	shapeDesc.height = extents.y;
	
	FinalizeCreate (shapeDesc, true, ignoreRigidbody);
}

void CapsuleCollider::SetRadius (float radius)
{
	if (m_Radius != radius)
	{
		SetDirty ();
		m_Radius = radius;
	}

	PROFILE_MODIFY_STATIC_COLLIDER

	if (GET_SHAPE ())
	{
		GET_SHAPE ()->setRadius (GetGlobalExtents ().x);
		RigidbodyMassDistributionChanged ();
		RefreshPhysicsInEditMode();
		UpdateCCDSkeleton ();
	}
}

void CapsuleCollider::SetHeight (float height)
{
	if (m_Height != height)
	{
		SetDirty ();
		m_Height = height;
	}
	
	PROFILE_MODIFY_STATIC_COLLIDER
	
	if (GET_SHAPE ())
	{
		GET_SHAPE ()->setHeight (GetGlobalExtents ().y);
		RigidbodyMassDistributionChanged ();
		RefreshPhysicsInEditMode();
		UpdateCCDSkeleton ();
	}
}

void CapsuleCollider::SetCenter (const Vector3f& center)
{
	if (m_Center != center)
	{
		m_Center = center;
		SetDirty ();
	}
	
	if (GET_SHAPE ())
	{
		TransformChanged (Transform::kRotationChanged | Transform::kPositionChanged | kForceUpdateMass);
		RefreshPhysicsInEditMode();
	}
}

void CapsuleCollider::ScaleChanged ()
{
	NxCapsuleShape* shape = GET_SHAPE ();
	Vector2f extents = GetGlobalExtents ();
	shape->setRadius (extents.x);
	shape->setHeight (extents.y);
	
	UpdateCCDSkeleton ();

	PROFILE_MODIFY_STATIC_COLLIDER
}

void CapsuleCollider::SetDirection (int dir)
{
	if (m_Direction != dir)
	{
		SetDirty ();
		m_Direction = dir;
	}

	TransformChanged (kForceUpdateMass);
}

Matrix4x4f CapsuleCollider::CalculateTransform () const
{
	Transform& transform = GetComponent (Transform);
	Vector3f p = transform.TransformPoint (m_Center);

	Quaternionf rotation = transform.GetRotation ();
	if (m_Direction == 2)
		rotation *= AxisAngleToQuaternion (Vector3f::xAxis, Deg2Rad (90));
	else if (m_Direction == 0)
		rotation *= AxisAngleToQuaternion (Vector3f::zAxis, Deg2Rad (90));
	else
		rotation *= AxisAngleToQuaternion (Vector3f::xAxis, Deg2Rad (180));

	Matrix4x4f matrix;
	matrix.SetTR (p, rotation);

	return matrix;
}

void CapsuleCollider::FetchPoseFromTransform ()
{
	AssertIf (HasActorRigidbody ());
	
	Transform& transform = GetComponent (Transform);
	Vector3f p = transform.TransformPoint (m_Center);
	m_Shape->getActor ().setGlobalPosition ((const NxVec3&)p);
	
	Quaternionf rotation = transform.GetRotation ();
	if (m_Direction == 2)
		rotation *= AxisAngleToQuaternion (Vector3f::xAxis, Deg2Rad (90));
	else if (m_Direction == 0)
		rotation *= AxisAngleToQuaternion (Vector3f::zAxis, Deg2Rad (90));
	else
		rotation *= AxisAngleToQuaternion (Vector3f::xAxis, Deg2Rad (180));
	
	m_Shape->getActor ().setGlobalOrientationQuat ((const NxQuat&)rotation);
}

bool CapsuleCollider::GetRelativeToParentPositionAndRotation (Transform& transform, Transform& anyParent, Matrix4x4f& matrix)
{
	Matrix4x4f childMatrix = CalculateTransform ();
	Matrix4x4f parentMatrix = anyParent.GetWorldToLocalMatrixNoScale ();
	MultiplyMatrices4x4 (&parentMatrix, &childMatrix, &matrix);
	ErrorFiniteParameterReturnFalse(matrix)
	return true;
}

void CapsuleCollider::TransformChanged (int changeMask)
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

			if (body->GetGameObjectPtr() != GetGameObjectPtr() || changeMask & (Transform::kScaleChanged | kForceUpdateMass))
				RigidbodyMassDistributionChanged ();
		}
		
		if (changeMask & Transform::kScaleChanged)
			ScaleChanged ();

		RefreshPhysicsInEditMode();
	}
}

NxCCDSkeleton* CapsuleCollider::CreateCCDSkeleton(float scale)
{
	// This is a very simple "approximation" of a capuse, of only 10 vertices.
	// Since CCD only kicks in when normal collisions fail, any is probably mostly 
	// interesting for small objects (as those tend to move faster), I believe that this
	// is a reasonable choice for common expected use. NVidia even recommends using 
	// single vertex meshes for very small objects.
	float radius;
	float height;
	
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
	{
		Vector2f extents = GetGlobalExtents ();
		radius = extents.x * scale;
		height = extents.y * scale;
	}
	else
	{
		// Prior to 4.0 we incorrectly ignored object scale here. Keep this behaviour for backwards compatibility.
		radius = m_Radius * scale;
		height = m_Height * 0.5f * scale;
	}

	NxU32 triangles[3 * 16] = {
		0,1,2,
		0,2,3,
		0,3,4,
		0,4,1,
		
		1,5,6,
		1,6,2,
		2,6,7,
		2,7,3,
		3,7,8,
		3,8,4,
		4,8,5,
		4,5,1,
		
		9,6,5,
		9,7,6,
		9,8,7,
		9,5,8,
	};

	Vector3f points[10];
	points[0].Set( 0, -radius - height, 0);
	points[1].Set( -radius, -height, 0);
	points[2].Set( 0, -height, -radius);
	points[3].Set( radius, -height, 0);
	points[4].Set( 0, -height, radius);
	points[5].Set( -radius, height, 0);
	points[6].Set( 0, height, -radius);
	points[7].Set( radius, height, 0);
	points[8].Set( 0, height, radius);
	points[9].Set( 0, radius + height, 0);

	NxSimpleTriangleMesh stm;
	stm.numVertices = 10;
	stm.numTriangles = 16;
	stm.pointStrideBytes = sizeof(Vector3f);
	stm.triangleStrideBytes = sizeof(NxU32)*3;

	stm.points = points;
	stm.triangles = triangles;
	return GetDynamicsSDK().createCCDSkeleton(stm);
}

template<class TransferFunction>
void CapsuleCollider::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	transfer.Align();
	TRANSFER_SIMPLE (m_Radius);
	TRANSFER_SIMPLE (m_Height);
	TRANSFER (m_Direction);
	TRANSFER (m_Center);
}

IMPLEMENT_CLASS (CapsuleCollider)
IMPLEMENT_OBJECT_SERIALIZE (CapsuleCollider)

#undef GET_SHAPE
#endif //ENABLE_PHYSICS