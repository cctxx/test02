#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "RaycastCollider.h"
#include "Runtime/Graphics/Transform.h"
#include "RigidBody.h"
#include "PhysicsManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/AABBUtility.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"

#define GET_SHAPE() ((class NxCapsuleShape*)m_Shape)

/*
 - i am not sure about the getscaled extents calculation.
*/

const float RaycaseCollider_kMinSize = 0.00001F;

// Novodex bugs with thin raycast triggers. It likes the fat hairy ones!
// Remove this as soon as they have fixed this!!!
const float kMinTriggerSize = 0.05F;


using namespace std;

RaycastCollider::RaycastCollider(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

RaycastCollider::~RaycastCollider ()
{
}

void RaycastCollider::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	if (m_Shape)
	{
		// Apply changed values
		SetLength (m_Length);
		SetCenter (m_Center);
	}
	
	Super::AwakeFromLoad (awakeMode);
}	

void RaycastCollider::SmartReset ()
{
	Super::Reset ();
	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		SetLength (aabb.GetExtent ().y);
		SetCenter (aabb.GetCenter ());
	}
	else
	{
		SetLength (1.0F);
		SetCenter (Vector3f::zero);
	}
}

void RaycastCollider::Reset ()
{
	Super::Reset ();
	m_Length = 1.0F;
	m_Center = Vector3f::zero;
}

Vector3f RaycastCollider::GetGlobalCenter () const
{
	return GetComponent (Transform).TransformPoint (m_Center);
}

float RaycastCollider::GetGlobalLength () const
{
	Vector3f scale = GetComponent (Transform).GetWorldScaleLossy ();

	float absoluteHeight = max (Abs (m_Length * scale.y), RaycaseCollider_kMinSize);
	return absoluteHeight;
}

void RaycastCollider::Create (const Rigidbody* ignoreAttachRigidbody)
{
	if (m_Shape)
		Cleanup ();

	NxCapsuleShapeDesc shapeDesc;
	shapeDesc.radius = GetIsTrigger() ? kMinTriggerSize : RaycaseCollider_kMinSize;
	shapeDesc.height = GetGlobalLength ();
	shapeDesc.flags |= NX_SWEPT_SHAPE;
	
	FinalizeCreate (shapeDesc, true, ignoreAttachRigidbody);
}

void RaycastCollider::SetLength (float height)
{
	if (m_Length != height)
	{
		SetDirty ();
		m_Length = height;
	}
	
	if (GET_SHAPE ())
		GET_SHAPE ()->setHeight (GetGlobalLength ());
}

void RaycastCollider::SetCenter (const Vector3f& center)
{
	if (m_Center != center)
	{
		m_Center = center;
		SetDirty ();
	}
	
	if (GET_SHAPE ())
		TransformChanged (Transform::kRotationChanged | Transform::kPositionChanged);
}

void RaycastCollider::ScaleChanged ()
{
	PROFILE_MODIFY_STATIC_COLLIDER

	NxCapsuleShape* shape = GET_SHAPE ();
	shape->setHeight (GetGlobalLength ());
}

Matrix4x4f RaycastCollider::CalculateTransform () const
{
	Transform& transform = GetComponent (Transform);
	Vector3f p = transform.TransformPoint (m_Center - Vector3f (0.0F, m_Length * .5F, 0.0F));

	Quaternionf rotation = transform.GetRotation ();
	rotation *= AxisAngleToQuaternion (Vector3f::xAxis, Deg2Rad (180));

	Matrix4x4f matrix;
	matrix.SetTR (p, rotation);

	return matrix;
}

void RaycastCollider::FetchPoseFromTransform ()
{
	AssertIf (HasActorRigidbody ());
	
	Transform& transform = GetComponent (Transform);
	Vector3f p = transform.TransformPoint (m_Center - Vector3f (0.0F, m_Length * .5F, 0.0F));
	AssertFiniteParameter(p)
	m_Shape->getActor().setGlobalPosition ((const NxVec3&)p);
	
	Quaternionf rotation = transform.GetRotation ();
	rotation *= AxisAngleToQuaternion (Vector3f::xAxis, Deg2Rad (180));
	
	AssertFiniteParameter(rotation)
	
	m_Shape->getActor().setGlobalOrientationQuat ((const NxQuat&)rotation);
}

bool RaycastCollider::GetRelativeToParentPositionAndRotation (Transform& transform, Transform& anyParent, Matrix4x4f& matrix)
{
	Matrix4x4f childMatrix = CalculateTransform ();
	Matrix4x4f parentMatrix = anyParent.GetWorldToLocalMatrixNoScale ();
	MultiplyMatrices4x4 (&parentMatrix, &childMatrix, &matrix);
	ErrorFiniteParameterReturnFalse(matrix)
	return true;
}

void RaycastCollider::TransformChanged (int changeMask)
{
	if (m_Shape)
	{
		if (!m_Shape->getActor ().userData)
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
		}
		
		if (changeMask & Transform::kScaleChanged)
			ScaleChanged ();
	}
}

void RaycastCollider::InitializeClass ()
{
	REGISTER_MESSAGE (RaycastCollider, kTransformChanged, TransformChanged, int);
}

template<class TransferFunction>
void RaycastCollider::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	transfer.Align();
	TRANSFER_SIMPLE (m_Center);
	TRANSFER_SIMPLE (m_Length);
}

IMPLEMENT_CLASS_HAS_INIT (RaycastCollider)
IMPLEMENT_OBJECT_SERIALIZE (RaycastCollider)

#undef GET_SHAPE
#endif //ENABLE_PHYSICS
