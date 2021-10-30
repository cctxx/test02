#include "UnityPrefix.h"
#include "NavMeshObstacle.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/AABBUtility.h"
#include "DetourCrowd.h"
#include "Runtime/Utilities/ValidateArgs.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "NavMeshTypes.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "NavMeshManager.h"
#include "NavMeshCarving.h"

NavMeshObstacle::NavMeshObstacle (MemLabelId& label, ObjectCreationMode mode)
: Super (label, mode)
{
	m_Velocity = Vector3f::zero;
	m_ManagerHandle = -1;
#if ENABLE_NAVMESH_CARVING
	m_CarveHandle = -1;
	m_Status = kForceRebuild;
#endif
	Reset ();
}

NavMeshObstacle::~NavMeshObstacle ()
{
}

template<class TransferFunc>
void NavMeshObstacle::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	TRANSFER (m_Radius);
	TRANSFER (m_Height);
#if ENABLE_NAVMESH_CARVING
	TRANSFER (m_MoveThreshold);
	TRANSFER (m_Carve);
#endif
}

void NavMeshObstacle::CheckConsistency ()
{
	m_Radius = EnsurePositive (m_Radius);
	m_Height = EnsurePositive (m_Height);
#if ENABLE_NAVMESH_CARVING
	m_MoveThreshold = max (0.0f, m_MoveThreshold);
#endif
}

UInt32 NavMeshObstacle::CalculateSupportedMessages ()
{
	return kSupportsVelocityChanged;
}

void NavMeshObstacle::Reset ()
{
	Super::Reset ();

	m_Radius =  0.5f;
	m_Height =  2.0f;
#if ENABLE_NAVMESH_CARVING
	m_MoveThreshold = 0.0f;
	m_Carve = false;
#endif
}

void NavMeshObstacle::SmartReset ()
{
	Super::SmartReset ();
	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		Vector3f extents = aabb.GetCenter () + aabb.GetExtent ();

		SetRadius (max (extents.x, extents.z));
		SetHeight (2.0F*extents.y);
	}
	else
	{
		SetRadius (0.5F);
		SetHeight (2.0F);
	}
}

void NavMeshObstacle::AddToManager ()
{
	GetNavMeshManager ().RegisterObstacle (*this, m_ManagerHandle);
	AddToCrowdSystem ();

#if ENABLE_NAVMESH_CARVING
	AddOrRemoveObstacle ();
#endif
}

void NavMeshObstacle::RemoveFromManager ()
{
	RemoveFromCrowdSystem ();
	GetNavMeshManager ().UnregisterObstacle (m_ManagerHandle);

#if ENABLE_NAVMESH_CARVING
	if (m_CarveHandle != -1)
	{
		if (NavMeshCarving* carving = GetNavMeshManager ().GetCarvingSystem ())
		{
			carving->RemoveObstacle (m_CarveHandle);
		}
	}
#endif
}

void NavMeshObstacle::RemoveFromCrowdSystem ()
{
	if (!InCrowdSystem ())
		return;
	GetCrowdSystem ()->RemoveObstacle (m_ObstacleHandle);
}

void NavMeshObstacle::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad (mode);

#if ENABLE_NAVMESH_CARVING
	if (m_ManagerHandle != -1)
		AddOrRemoveObstacle ();
#endif

	dtCrowd* crowd = GetCrowdSystem ();
	if (crowd == NULL || !InCrowdSystem ())
		return;

	const Vector3f position = GetPosition ();
	const Vector3f dimensions = GetScaledDimensions ();
	crowd->SetObstaclePosition (m_ObstacleHandle, position.GetPtr ());
	crowd->SetObstacleDimensions (m_ObstacleHandle, dimensions.GetPtr ());
}

void NavMeshObstacle::OnNavMeshChanged ()
{
	if (!InCrowdSystem ())
	{
		AddToCrowdSystem ();
	}

#if ENABLE_NAVMESH_CARVING
	m_Status |= kForceRebuild;
#endif
}

void NavMeshObstacle::OnNavMeshCleanup ()
{
	RemoveFromCrowdSystem ();
}

void NavMeshObstacle::AddToCrowdSystem ()
{
	if (!IsWorldPlaying ())
		return;

	if (!GetNavMeshManager ().GetInternalNavMeshQuery ())
		return;

	Assert (!InCrowdSystem ());
	dtCrowd* crowd = GetCrowdSystem ();
	if (crowd == NULL)
		return;

	if (!crowd->AddObstacle (m_ObstacleHandle))
		return;

	const Vector3f position = GetPosition ();
	const Vector3f dimensions = GetScaledDimensions ();
	crowd->SetObstaclePosition (m_ObstacleHandle, position.GetPtr ());
	crowd->SetObstacleDimensions (m_ObstacleHandle, dimensions.GetPtr ());
}

void NavMeshObstacle::InitializeClass ()
{
	REGISTER_MESSAGE (NavMeshObstacle, kTransformChanged, OnTransformChanged, int);
	REGISTER_MESSAGE_PTR (NavMeshObstacle, kDidVelocityChange, OnVelocityChanged, Vector3f);
}

void NavMeshObstacle::OnTransformChanged (int mask)
{
#if ENABLE_NAVMESH_CARVING
	m_Status |= kHasMoved;
	if (mask & Transform::kRotationChanged)
	{
		m_Status = kForceRebuild;
	}
#endif
	if (!InCrowdSystem ())
		return;

	if (mask & Transform::kPositionChanged)
	{
		const Vector3f position = GetPosition ();
		GetCrowdSystem ()->SetObstaclePosition (m_ObstacleHandle, position.GetPtr ());
	}

	if (mask & Transform::kScaleChanged)
	{
		const Vector3f dimensions = GetScaledDimensions ();
		GetCrowdSystem ()->SetObstacleDimensions (m_ObstacleHandle, dimensions.GetPtr ());
	}
}

#if ENABLE_NAVMESH_CARVING

void NavMeshObstacle::AddOrRemoveObstacle ()
{
	NavMeshCarving* carving = GetNavMeshManager ().GetCarvingSystem ();
	if (!carving)
	{
		return;
	}

	if (m_Carve && m_CarveHandle == -1)
	{
		carving->AddObstacle (*this, m_CarveHandle);
		RemoveFromCrowdSystem ();
	}
	else if (!m_Carve && m_CarveHandle != -1)
	{
		carving->RemoveObstacle (m_CarveHandle);
		AddToCrowdSystem ();
	}

	m_Status |= kForceRebuild;
}

void NavMeshObstacle::WillRebuildNavmesh (NavMeshCarveData& carveData)
{
	const Vector3f position = GetPosition ();
	CalculateTransformAndSize (carveData.transform, carveData.size);

	m_LastCarvedPosition = position;
	m_Status = kClean;
}

bool NavMeshObstacle::NeedsRebuild () const
{
	if (m_Status == kClean)
		return false;

	if (m_Status & kForceRebuild)
		return true;

	if (m_Status & kHasMoved)
	{
		const Vector3f position = GetComponent (Transform).GetPosition ();
		const float sqrDistance = SqrMagnitude (m_LastCarvedPosition - position);
		if (sqrDistance > m_MoveThreshold * m_MoveThreshold)
			return true;
	}

	return false;
}

void NavMeshObstacle::SetCarving (bool carve)
{
	if (m_Carve == carve)
		return;

	m_Carve = carve;
	AddOrRemoveObstacle ();
	SetDirty ();
}

void NavMeshObstacle::SetMoveThreshold (float moveThreshold)
{
	ABORT_INVALID_FLOAT (moveThreshold, moveThreshold, navmeshobstacle);
	m_MoveThreshold = moveThreshold;
	SetDirty ();
}

#endif

void NavMeshObstacle::OnVelocityChanged (Vector3f* value)
{
	SetVelocity (*value);
}

void NavMeshObstacle::SetVelocity (const Vector3f& value)
{
	ABORT_INVALID_VECTOR3 (value, velocity, navmeshobstacle);
	m_Velocity = value;
	if (InCrowdSystem ())
	{
		GetCrowdSystem ()->SetObstacleVelocity (m_ObstacleHandle, m_Velocity.GetPtr ());
	}
}

void NavMeshObstacle::SetRadius (float value)
{
	ABORT_INVALID_FLOAT (value, radius, navmeshobstacle);
	m_Radius = EnsurePositive (value);
	SetDirty ();
	const Vector3f dimensions = GetScaledDimensions ();
	if (InCrowdSystem ())
	{
		GetCrowdSystem ()->SetObstacleDimensions (m_ObstacleHandle, dimensions.GetPtr ());
	}
}

void NavMeshObstacle::SetHeight (float value)
{
	ABORT_INVALID_FLOAT (value, height, navmeshobstacle);
	m_Height = EnsurePositive (value);
	SetDirty ();
	const Vector3f dimensions = GetScaledDimensions ();
	if (InCrowdSystem ())
	{
		GetCrowdSystem ()->SetObstacleDimensions (m_ObstacleHandle, dimensions.GetPtr ());
	}
}

Vector3f NavMeshObstacle::GetScaledDimensions () const
{
	Vector3f absScale = Abs (GetComponent (Transform).GetWorldScaleLossy ());
	float scaledRadius = m_Radius * max (absScale.x, absScale.z);
	float scaledHeight = m_Height * absScale.y;
	return Vector3f (scaledRadius, scaledHeight, scaledRadius);
}

void NavMeshObstacle::CalculateTransformAndSize (Matrix4x4f& trans, Vector3f& size)
{
	// TODO cache result on obstacle.
	const Transform& transform = GetComponent (Transform);
	trans = transform.GetLocalToWorldMatrix ();

	AABB aabb;
	if (CalculateLocalAABB (GetGameObject (), &aabb))
	{
		size = aabb.GetExtent ();
	}
	else
	{
		size = Vector3f (m_Radius, m_Height, m_Radius);
	}
}

dtCrowd* NavMeshObstacle::GetCrowdSystem ()
{
	return GetNavMeshManager ().GetCrowdSystem ();
}

IMPLEMENT_CLASS_HAS_INIT (NavMeshObstacle)
IMPLEMENT_OBJECT_SERIALIZE (NavMeshObstacle)
