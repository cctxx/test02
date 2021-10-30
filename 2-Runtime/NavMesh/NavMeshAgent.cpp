#include "UnityPrefix.h"
#include "NavMeshAgent.h"

#include "DetourCommon.h"
#include "DetourCrowd.h"
#include "DetourCrowdTypes.h"
#include "NavMesh.h"
#include "NavMeshManager.h"
#include "NavMeshLayers.h"
#include "NavMeshPath.h"
#include "NavMeshSettings.h"
#include "OffMeshLink.h"
#include "Runtime/BaseClasses/MessageHandler.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/AABBUtility.h"
#include "Runtime/Utilities/ValidateArgs.h"
#include "Runtime/Misc/BuildSettings.h"

#define REQUIRE_INITIALIZED(FUNC, returnValue) \
if (!InCrowdSystem ()) \
{ \
	ErrorString (#FUNC " can only be called on an active agent that has been placed on a NavMesh."); \
	return returnValue; \
}

NavMeshAgent::NavMeshAgent (MemLabelId& label, ObjectCreationMode mode)
: Super (label, mode)
{
	m_ManagerHandle = -1;
	m_CachedPolyRef = -1;
	m_Destination = Vector3f::infinityVec;
	m_RequestedDestination = Vector3f::infinityVec;
	m_StopDistance = false;
	m_StopExplicit = false;
	m_Request = false;
	Reset ();
}

void NavMeshAgent::Reset ()
{
	Super::Reset ();
	m_Radius = 0.5F;
	m_Height = 2.0F;
	m_BaseOffset = 0.0F;
	m_Acceleration = 8.0f;
	m_AngularSpeed = 120.0F;
	m_Speed = 3.5f;
	m_AvoidancePriority = 50;
	m_ObstacleAvoidanceType = kHighQualityObstacleAvoidance;
	m_UpdatePosition = true;
	m_UpdateRotation = true;
	m_StopRotating = false;
	m_AutoTraverseOffMeshLink = true;
	m_AutoBraking = true;
	m_AutoRepath = true;
	m_WalkableMask = 0xFFFFFFFF;
	m_StoppingDistance = 0.0f;
	m_InstanceID = 0;
}

void NavMeshAgent::SmartReset ()
{
	Super::SmartReset ();
	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		Vector3f extents = aabb.GetCenter () + aabb.GetExtent ();
		SetRadius (max (extents.x, extents.z));
		SetHeight (2.0F*extents.y);
		SetBaseOffset (extents.y);
	}
	else
	{
		SetRadius (0.5F);
		SetHeight (2.0F);
		SetBaseOffset (0.0F);
	}
}

NavMeshAgent::~NavMeshAgent ()
{
}

template<class TransferFunc>
void NavMeshAgent::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);

	TRANSFER (m_Radius);
	TRANSFER (m_Speed);
	TRANSFER (m_Acceleration);
	transfer.Transfer (m_AvoidancePriority, "avoidancePriority");
	TRANSFER (m_AngularSpeed);
	TRANSFER (m_StoppingDistance);
	TRANSFER (m_AutoTraverseOffMeshLink);
	TRANSFER (m_AutoBraking);
	TRANSFER (m_AutoRepath);
	transfer.Align ();
	TRANSFER (m_Height);
	TRANSFER (m_BaseOffset);
	TRANSFER (m_WalkableMask);
	TRANSFER_ENUM(m_ObstacleAvoidanceType);
}

void NavMeshAgent::InitializeClass ()
{
	REGISTER_MESSAGE (NavMeshAgent, kTransformChanged, OnTransformChanged, int);
}

void NavMeshAgent::OnTransformChanged (int mask)
{
	if (!InCrowdSystem ())
		return;

	if (mask & Transform::kPositionChanged)
	{
		const Vector3f npos = GetGroundPositionFromTransform ();
		GetCrowdSystem ()->updateAgentPosition (m_AgentHandle, npos.GetPtr ());
	}
	if (mask & Transform::kRotationChanged)
		m_Angle = std::numeric_limits<float>::infinity ();
	if (mask & Transform::kScaleChanged)
		UpdateActiveAgentParameters ();
}

int NavMeshAgent::GetCurrentPolygonMask () const
{
	dtPolyRef polyRef;
	if (IsOnOffMeshLink ())
		polyRef = GetCrowdSystem ()->getAgentAnimation (m_AgentHandle)->polyRef;
	else
		polyRef = GetInternalAgent ()->corridor.getFirstPoly ();

	const dtNavMeshQuery* meshq = GetInternalNavMeshQuery ();
	return meshq->getPolygonFlags (polyRef);
}

const dtQueryFilter& NavMeshAgent::GetFilter () const
{
	Assert (InCrowdSystem ());
	const dtCrowd* crowd = GetCrowdSystem ();
	return *crowd->getAgentFilter (m_AgentHandle);
}

const dtCrowdAgent* NavMeshAgent::GetInternalAgent () const
{
	Assert (InCrowdSystem ());
	return GetCrowdSystem ()->getAgent (m_AgentHandle);
}

bool NavMeshAgent::SetDestination (const Vector3f& targetPos)
{
	REQUIRE_INITIALIZED ("SetDestination", false);
	m_RequestedDestination = targetPos;
	m_Request = true;

	dtCrowd* crowd = GetCrowdSystem ();
	const dtQueryFilter& filter = GetFilter ();
	const dtNavMeshQuery* query = crowd->getNavMeshQuery ();
	const float* ext = crowd->getQueryExtents ();

	dtPolyRef newPoly;
	float destination[3];

	query->findNearestPoly (targetPos.GetPtr (), ext, &filter, &newPoly, destination);

	if (!newPoly)
		return false;

	if (m_CachedPolyRef == -1 || m_CachedPolyRef != newPoly || IsPathStale ())
	{
		if (crowd->requestMoveTarget (m_AgentHandle, newPoly, destination))
		{
			m_CachedPolyRef = newPoly;
			m_Destination = Vector3f (destination);
			if (m_StopExplicit)
			{
				m_StopExplicit = false;
				SetUpdatePosition (true);
			}
			return true;
		}
	}
	else
	{
		if (crowd->adjustMoveTarget (m_AgentHandle, m_CachedPolyRef, destination))
		{
			m_Destination = Vector3f (destination);
			if (m_StopExplicit)
			{
				m_StopExplicit = false;
				SetUpdatePosition (true);
			}
			return true;
		}
	}
	return false;
}

Vector3f NavMeshAgent::GetDestination () const
{
	if (HasPath () && !PathPending ())
		return GetEndPositionOfCurrentPath ();

	return m_Destination;
}

Vector3f NavMeshAgent::GetEndPositionOfCurrentPath () const
{
	Assert (InCrowdSystem ());

	const dtCrowdAgent* agent = GetInternalAgent ();
	Vector3f endPosition (agent->corridor.getTarget ());
	return endPosition;
}

void NavMeshAgent::SetInternalAgentPosition (const Vector3f& position)
{
	if (!InCrowdSystem ())
		return;

	const Transform& transform = GetComponent (Transform);
	Vector3f groundPosition = transform.TransformPointWithLocalOffset (position, Vector3f (0.0f, m_BaseOffset, 0.0f));
	GetCrowdSystem ()->moveAgent (m_AgentHandle, groundPosition.GetPtr ());
}

Vector3f NavMeshAgent::GetVelocity () const
{
	if (!InCrowdSystem ())
		return Vector3f (0,0,0);

	return Vector3f (GetInternalAgent ()->avel);
}

void NavMeshAgent::SetVelocity (const Vector3f& vel)
{
	if (!InCrowdSystem ())
		return;

	ABORT_INVALID_VECTOR3 (vel, velocity, navmeshagent);
	GetCrowdSystem ()->updateAgentVelocity (m_AgentHandle, vel.GetPtr ());
}

Vector3f NavMeshAgent::GetNextPosition () const
{
	const Transform& transform = GetComponent (Transform);
	if (!InCrowdSystem ())
		return transform.GetPosition ();

	const Vector3f position (GetInternalAgent ()->npos);
	return transform.TransformPointWithLocalOffset (position, Vector3f (0.0f, -m_BaseOffset, 0.0f));
}

Vector3f NavMeshAgent::GetNextCorner () const
{
	if (!InCrowdSystem ())
		return GetComponent (Transform).GetPosition ();

	const dtCrowdAgent* agent = GetInternalAgent ();
	Vector3f target;
	GetCrowdSystem ()->getSteerTarget (target.GetPtr (), agent);
	return target;
}

Vector3f NavMeshAgent::GetDesiredVelocity () const
{
	if (!InCrowdSystem ())
		return Vector3f::zero;

	const dtCrowdAgent* agent = GetInternalAgent ();
	Vector3f desiredVelocity (agent->nvel);
	return desiredVelocity;
}

bool NavMeshAgent::IsOnOffMeshLink () const
{
	if (!InCrowdSystem ())
		return false;

	const dtCrowdAgent* agent = GetInternalAgent ();
	return agent->state == DT_CROWDAGENT_STATE_OFFMESH;
}

// @TODO: Deprecate (de)activation of baked offmeshlinks.
// We really don't want to be holding an OffMeshLink instance ID here.
// Instead promote use of: NavMeshAgent.currentOffMeshLinkData.offMeshLink
void NavMeshAgent::ActivateCurrentOffMeshLink (bool activated)
{
	if (!IsOnOffMeshLink ())
		return;

	int instanceID = 0;
	if (!activated)
	{
		const dtPolyRef polyref = GetCrowdSystem ()->getAgentAnimation (m_AgentHandle)->polyRef;
		GetNavMeshManager ().GetInternalNavMesh ()->getOffMeshLinkInstanceIDByRef (polyref, &instanceID);
		m_InstanceID = instanceID;
	}
	else
	{
		instanceID = m_InstanceID;
		m_InstanceID = 0;
	}

	if (OffMeshLink* oml = dynamic_instanceID_cast<OffMeshLink*> (instanceID))
	{
		oml->SetActivated (activated);
	}
	else
	{
		const dtPolyRef polyref = GetCrowdSystem ()->getAgentAnimation (m_AgentHandle)->polyRef;
		GetNavMeshSettings ().SetOffMeshPolyAccess (polyref, activated);
	}
}


bool NavMeshAgent::GetCurrentOffMeshLinkData (OffMeshLinkData* data) const
{
	Assert (data);
	memset (data, 0, sizeof (OffMeshLinkData));
	if (!IsOnOffMeshLink ())
		return false;

	const dtCrowdAgentAnimation* agentAnimation = GetCrowdSystem ()->getAgentAnimation (m_AgentHandle);
	if (agentAnimation == NULL)
		return false;

	if (!SetOffMeshLinkDataFlags (data, agentAnimation->polyRef))
		return false;

	data->m_StartPos = Vector3f (agentAnimation->startPos);
	data->m_EndPos = Vector3f (agentAnimation->endPos);
	return true;
}

bool NavMeshAgent::GetNextOffMeshLinkData (OffMeshLinkData* data) const
{
	Assert (data);
	memset (data, 0, sizeof (OffMeshLinkData));
	if (!InCrowdSystem ())
		return false;

	const dtPathCorridor& corridor = GetInternalAgent ()->corridor;
	if (!corridor.isPathValid ())
		return false;

	const dtNavMesh* navmesh = GetNavMeshManager ().GetInternalNavMesh ();
	const dtPolyRef* pathPolygons = corridor.getPath ();
	const int pathCount = corridor.getPathCount ();
	int i = 1;
	for (; i < pathCount; ++i)
	{
		if (SetOffMeshLinkDataFlags (data, pathPolygons[i]))
			break;
	}

	// Note: We intentionally fail if pathCount == 1
	// in that case any offmeshlink is not 'next' - but current.
	if (i >= pathCount)
		return false;

	dtStatus status = navmesh->getOffMeshConnectionPolyEndPoints (pathPolygons[i-1], pathPolygons[i], data->m_StartPos.GetPtr (), data->m_EndPos.GetPtr ());

	// Having successfully called 'SetOffMeshLinkDataFlags' above should ensure valid endpoints here.
	DebugAssert (status == DT_SUCCESS);
	if (status != DT_SUCCESS)
	{
		memset (data, 0, sizeof (OffMeshLinkData));
		return false;
	}

	return true;
}

// Set OffMeshLink data or return false
// Returns false if polyRef is not an offmeshlink
bool NavMeshAgent::SetOffMeshLinkDataFlags (OffMeshLinkData* data, const dtPolyRef polyRef) const
{
	Assert (InCrowdSystem ());

	const dtNavMesh* navmesh = GetNavMeshManager ().GetInternalNavMesh ();

	int instanceID, linkType, activated;
	if (!navmesh->GetOffMeshLinkData (polyRef, &instanceID, &linkType, &activated))
		return false;

	data->m_Valid = 1;
	data->m_Activated = activated;
	data->m_LinkType = static_cast<OffMeshLinkType> (linkType);
	data->m_InstanceID = instanceID;

	return true;
}

void NavMeshAgent::SetUpdatePosition (bool inbool)
{
	if (m_UpdatePosition == inbool)
		return;

	m_UpdatePosition = inbool;
	if (inbool)
	{
		Vector3f npos = GetGroundPositionFromTransform ();
		GetCrowdSystem ()->updateAgentPosition (m_AgentHandle, npos.GetPtr ());
	}
}

void NavMeshAgent::SetUpdateRotation (bool inbool)
{
	m_UpdateRotation = inbool;
	if (inbool)
	{
		const Transform& transform = GetComponent (Transform);
		Vector3f euler = QuaternionToEuler (transform.GetRotation ());
		m_Angle = euler.y;
	}
}

void NavMeshAgent::SetAutoTraverseOffMeshLink (bool inbool)
{
	m_AutoTraverseOffMeshLink = inbool;
	UpdateActiveAgentParameters ();
	SetDirty ();
}

void NavMeshAgent::SetAutoBraking (bool inbool)
{
	m_AutoBraking = inbool;
	UpdateActiveAgentParameters ();
	SetDirty ();
}

void NavMeshAgent::SetAutoRepath (bool inbool)
{
	m_AutoRepath = inbool;
	SetDirty ();
}

void NavMeshAgent::UpdateActiveAgentParameters ()
{
	CheckConsistency ();
	if (InCrowdSystem ())
	{
		dtCrowdAgentParams params;
		FillAgentParams (params);
		GetCrowdSystem ()->updateAgentParameters (m_AgentHandle, &params);
	}
}

void NavMeshAgent::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad (mode);
	UpdateActiveAgentParameters ();
}

void NavMeshAgent::CheckConsistency ()
{
	Super::CheckConsistency ();
	m_AvoidancePriority = clamp (m_AvoidancePriority, 0, 99);
	m_Speed = clamp (m_Speed, 0.0f, 1e15f); // squaring m_Speed keeps it below inf.
	m_StoppingDistance = max (0.0f, m_StoppingDistance);
	m_Acceleration = max (0.0f, m_Acceleration);
	m_AngularSpeed = max (0.0f, m_AngularSpeed);
	m_Height = EnsurePositive (m_Height);
	m_Radius = EnsurePositive (m_Radius);
}

void NavMeshAgent::UpdateState ()
{
	if (!InCrowdSystem ())
		return;

	const float remainingDistance = GetRemainingDistance ();
	StopOrResume (remainingDistance);

	if (m_AutoRepath && m_Request)
		RepathIfStuck (remainingDistance);

	// Previously we were resetting the path when agentes reached the stopping distance.
	// Now this is no longer done, the path is always kept. We can kill this code on the next backwards breaking release.
	if (!IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
	{
		if (m_StopDistance && !PathPending ())
			ResetPath ();
	}
}

void NavMeshAgent::StopOrResume (float remainingDistance)
{
	m_StopDistance = remainingDistance < m_StoppingDistance && HasPath ();
	if (!m_StopExplicit)
	{
		m_StopRotating = false;
	}

	if (m_StopExplicit || m_StopDistance)
		GetCrowdSystem ()->stopAgent (m_AgentHandle);
	else
		GetCrowdSystem ()->resumeAgent (m_AgentHandle);
}

void NavMeshAgent::RepathIfStuck (float remainingDistance)
{
	bool isValid = IsPathValid ();
	bool isOffMesh = IsOnOffMeshLink ();
	bool isPartial = IsPathPartial ();
	bool isStale = IsPathStale ();

	// m_Radius must be scaled by transform scale - so use the (already scaled) internal radius!
	const float agentRadius = GetInternalAgent ()->params.radius;
	if (!isValid || (!isOffMesh && isPartial && isStale && remainingDistance <= agentRadius))
	{
		m_CachedPolyRef = -1;
		SetDestination (m_RequestedDestination);
	}
}

static inline void SetTransformMessageEnabled (bool enable)
{
	GameObject::GetMessageHandler ().SetMessageEnabled (ClassID (NavMeshAgent), kTransformChanged.messageID, enable);
}

void NavMeshAgent::UpdateTransform (float deltaTime)
{
	if (!InCrowdSystem ())
		return;

	Transform& transform = GetComponent (Transform);

	// Avoid dirtying the transform twice (when updating position and rotation)
	// Instead call "WithoutNotification" variants of the SetPosition / SetRotation
	// and signal change once at the end using 'changeMessageMask'
	int changeMessageMask = 0;

	if (m_UpdatePosition)
	{
		const Vector3f groundPosition = Vector3f (GetInternalAgent ()->npos);
		const Vector3f localOffset = Vector3f (0.0f, -m_BaseOffset, 0.0f);
		const Vector3f newPosition = transform.TransformPointWithLocalOffset (groundPosition, localOffset);
		transform.SetPositionWithoutNotification (newPosition);
		changeMessageMask |= Transform::kPositionChanged;
	}

	if (m_UpdateRotation && !m_StopRotating)
	{
		UpdateRotation (transform, deltaTime);
		changeMessageMask |= Transform::kRotationChanged;
	}

	if (changeMessageMask)
	{
		// Update the transform hierarchy w/o triggering our own callback
		SetTransformMessageEnabled (false);
		transform.SendTransformChanged (changeMessageMask);
		SetTransformMessageEnabled (true);
	}
}

void NavMeshAgent::UpdateRotation (Transform& transform, float deltaTime)
{
	if (m_Angle == std::numeric_limits<float>::infinity ())
	{
		Vector3f euler = QuaternionToEuler (transform.GetRotation ());
		m_Angle = euler.y;
	}

	const dtCrowdAgent* agent = GetInternalAgent ();
	float sqrLen = Sqr (agent->vel[0]) + Sqr (agent->vel[2]);
	if (sqrLen > 0.001F)
	{
		float angle = atan2 (agent->vel[0], agent->vel[2]);
		const float deltaAngle = DeltaAngleRad (m_Angle, angle);
		const float maxAngleSpeed = std::min (Abs (deltaAngle)*(1.0f+sqrtf (sqrLen)), Deg2Rad (m_AngularSpeed));
		m_Angle += std::min (Abs (deltaAngle), maxAngleSpeed*deltaTime) * Sign (deltaAngle);
	}
	Quaternionf rotation = AxisAngleToQuaternion (Vector3f::yAxis, m_Angle);
	transform.SetRotationWithoutNotification (rotation);
}

void NavMeshAgent::FillAgentParams (dtCrowdAgentParams& params)
{
	Vector2f scale = CalculateDimensionScales ();
	params.radius = max (0.00001F, m_Radius*scale.x);
	params.height = max (0.00001F, m_Height*scale.y);
	params.maxAcceleration = m_Acceleration;
	params.maxSpeed = m_Speed;
	params.priority = 99 - m_AvoidancePriority;
	params.obstacleAvoidanceType = m_ObstacleAvoidanceType;
	params.includeFlags = GetWalkableMask ();

	params.updateFlags = 0;
	if (m_ObstacleAvoidanceType != kNoObstacleAvoidance)
		params.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE;
	if (m_AutoTraverseOffMeshLink)
		params.updateFlags |= DT_CROWD_AUTO_TRAVERSE_OFFMESHLINK;
	if (m_AutoBraking)
		params.updateFlags |= DT_CROWD_AUTO_BRAKING;
}

void NavMeshAgent::OnNavMeshChanged ()
{
	if (InCrowdSystem ())
	{
		ReinstateInCrowdSystem ();
	}
	else
	{
		AddToCrowdSystem ();
	}
}

void NavMeshAgent::AddToCrowdSystem ()
{
	if (!IsWorldPlaying ())
		return;

	if (GetInternalNavMeshQuery () == NULL)
	{
		WarningString ("Failed to create agent because there is no valid NavMesh");
		return;
	}
	dtCrowd* crowd = GetCrowdSystem ();
	Assert (crowd != NULL);

	dtCrowdAgentParams params;
	FillAgentParams (params);

	Assert (!InCrowdSystem ());
	Assert (m_CachedPolyRef == -1);
	Vector3f currentPosition = GetGroundPositionFromTransform ();
	if (!crowd->addAgent (m_AgentHandle, currentPosition.GetPtr (), &params))
	{
		WarningStringObject ("Failed to create agent because it is not close enough to the NavMesh", this);
		return;
	}

	const dtCrowdAgent* agent = GetInternalAgent ();

	m_CachedPolyRef = agent->corridor.getFirstPoly ();
	m_Destination = Vector3f (agent->corridor.getPos ());
	m_RequestedDestination = m_Destination;
	m_Angle = std::numeric_limits<float>::infinity ();

	// Collect global layer costs and apply to created agent
	float layerCosts[NavMeshLayers::kLayerCount];
	NavMeshLayers& layers = GetNavMeshLayers ();
	for (int il = 0; il < NavMeshLayers::kLayerCount; ++il)
	{
		layerCosts[il] = layers.GetLayerCost (il);
	}
	crowd->initializeAgentFilter (m_AgentHandle, layerCosts, NavMeshLayers::kLayerCount);
}

void NavMeshAgent::ReinstateInCrowdSystem ()
{
	if (!InCrowdSystem ())
		return;

	dtCrowd* crowd = GetCrowdSystem ();
	Assert (crowd);

	ResetCachedPolyRef ();
	if (!crowd->remapAgentPathStart (m_AgentHandle))
	{
		RemoveFromCrowdSystem ();
	}
	else if (m_AutoRepath && m_Request)
	{
		SetDestination (m_RequestedDestination);
	}
}

void NavMeshAgent::RemoveFromCrowdSystem ()
{
	if (!InCrowdSystem ())
		return;

#if UNITY_EDITOR
	dtCrowdAgentDebugInfo* debugInfo = GetNavMeshManager ().GetInternalDebugInfo ();
	if (debugInfo && debugInfo->idx == m_AgentHandle.GetIndex ())
	{
		debugInfo->idx = -1;
	}
#endif

	GetCrowdSystem ()->removeAgent (m_AgentHandle);
	m_CachedPolyRef = -1;
}

void NavMeshAgent::AddToManager ()
{
	GetNavMeshManager ().RegisterAgent (*this, m_ManagerHandle);
	AddToCrowdSystem ();
}

void NavMeshAgent::RemoveFromManager ()
{
	RemoveFromCrowdSystem ();
	GetNavMeshManager ().UnregisterAgent (m_ManagerHandle);
}

float NavMeshAgent::GetRemainingDistance () const
{
	REQUIRE_INITIALIZED ("GetRemainingDistance", std::numeric_limits<float>::infinity ());
	return GetCrowdSystem ()->CalculateRemainingPath (m_AgentHandle);
}

int NavMeshAgent::CalculatePolygonPath (const Vector3f& targetPosition, NavMeshPath* path)
{
	REQUIRE_INITIALIZED ("CalculatePolygonPath", 0)
	const dtCrowdAgent* agent = GetInternalAgent ();
	Vector3f sourcePosition (agent->corridor.getPos ());

	const dtQueryFilter& filter = GetFilter ();
	return GetNavMeshSettings ().GetNavMesh ()->CalculatePolygonPath (path, sourcePosition, targetPosition, filter);
}

bool NavMeshAgent::SetPath (const NavMeshPath* path)
{
	REQUIRE_INITIALIZED ("SetPath", false)

	ResetPath ();

	const NavMeshPathStatus status = path->GetStatus ();
	const int polyCount = path->GetPolygonCount ();
	if (status == kPathInvalid || polyCount == 0)
		return false;

	const Vector3f targetPos = path->GetTargetPosition ();
	const Vector3f sourcePos = path->GetSourcePosition ();
	const dtPolyRef* polyPath = path->GetPolygonPath ();
	GetCrowdSystem ()->setAgentPath (m_AgentHandle, sourcePos.GetPtr (), targetPos.GetPtr (), polyPath, polyCount, status == kPathPartial);
	const dtCrowdAgent* agent = GetInternalAgent ();

	dtPolyRef lastPoly = agent->corridor.getLastPoly ();
	if (lastPoly != polyPath[polyCount - 1])
		return false;

	m_CachedPolyRef = lastPoly;
	return true;
}

void NavMeshAgent::CopyPath (NavMeshPath* path) const
{
	Assert (path);
	if (!m_AgentHandle.IsValid ())
	{
		path->SetPolygonCount (0);
		path->SetStatus (kPathInvalid);
		return;
	}

	const dtCrowdAgent* agent = GetInternalAgent ();

	unsigned int pathCount = agent->corridor.getPathCount ();
	memcpy (path->GetPolygonPath (), agent->corridor.getPath (), sizeof (dtPolyRef)*pathCount);
	path->SetPolygonCount (pathCount);
	path->SetTargetPosition (Vector3f (agent->corridor.getTarget ()));
	path->SetSourcePosition (Vector3f (agent->corridor.getPos ()));
	path->SetStatus (GetPathStatus ());
}

void NavMeshAgent::ResetPath ()
{
	REQUIRE_INITIALIZED ("ResetPath",)

	GetCrowdSystem ()->resetAgentPath (m_AgentHandle);
	m_StopDistance = false;
	m_StopExplicit = false;
	m_Request = false;
	m_CachedPolyRef = -1;
}

bool NavMeshAgent::PathPending () const
{
	return InCrowdSystem () && GetInternalAgent ()->pendingRequest;
}

bool NavMeshAgent::HasPath () const
{
	if (!InCrowdSystem () || m_CachedPolyRef == -1)
		return false;

	const dtCrowdAgent* agent = GetInternalAgent ();
	if (agent->state == DT_CROWDAGENT_STATE_WALKING)
	{
		return agent->ncorners > 0;
	}
	else if (agent->state == DT_CROWDAGENT_STATE_OFFMESH || agent->state == DT_CROWDAGENT_STATE_WAITING_OFFMESH)
	{
		return true;
	}

	return false;
}

bool NavMeshAgent::DistanceToEdge (NavMeshHit* hit) const
{
	Assert (hit);
	REQUIRE_INITIALIZED ("DistanceToEdge", false);
	const dtCrowdAgent* agent = GetInternalAgent ();
	Vector3f sourcePosition (agent->corridor.getPos ());

	const dtQueryFilter& filter = GetFilter ();
	return GetNavMeshSettings ().GetNavMesh ()->DistanceToEdge (hit, sourcePosition, filter);
}

bool NavMeshAgent::Raycast (const Vector3f& targetPosition, NavMeshHit* hit)
{
	Assert (hit);
	REQUIRE_INITIALIZED ("Raycast", false)
	const dtCrowdAgent* agent = GetInternalAgent ();
	Vector3f sourcePosition (agent->corridor.getPos ());

	const dtQueryFilter& filter = GetFilter ();
	return GetNavMeshSettings ().GetNavMesh ()->Raycast (hit, sourcePosition, targetPosition, filter);
}

// TODO: handle case where: maxDistance > remainingDistance (non-inf).
bool NavMeshAgent::SamplePathPosition (int passableMask, float maxDistance, NavMeshHit* hit)
{
	Assert (hit);
	REQUIRE_INITIALIZED ("SamplePathPosition", false);

	const dtCrowdAgent* agent = GetInternalAgent ();
	const dtNavMeshQuery* meshq = GetInternalNavMeshQuery ();
	Vector3f sourcePosition (agent->corridor.getPos ());
	Vector3f agentPosition (agent->npos);

	// Copy and modify agent filter with provided mask
	dtQueryFilter filter = GetFilter ();
	filter.setIncludeFlags (passableMask & filter.getIncludeFlags ());

	if (agent->ncorners == 0 || maxDistance <= 0.0f)
	{
		hit->position = agentPosition;
		hit->normal = Vector3f::zero;
		hit->distance = 0.0f;
		hit->mask = GetCurrentPolygonMask ();
		hit->hit = false;
		return false;
	}

	if (!meshq->passRef (agent->corridor.getFirstPoly (), &filter))
	{
		hit->position = sourcePosition;
		hit->normal = Vector3f::zero;
		hit->distance = 0.0f;
		hit->mask = GetCurrentPolygonMask ();
		hit->hit = true;
		return true;
	}

	float totalDistance = 0.0f;
	for (int i = 0; i < agent->ncorners; ++i)
	{
		const Vector3f targetPosition (&agent->cornerVerts[3*i]);
		if (GetNavMeshSettings ().GetNavMesh ()->Raycast (hit, sourcePosition, targetPosition, filter))
		{
			const float hitDistance = totalDistance + hit->distance;
			if (hitDistance <= maxDistance)
			{
				// position, normal, mask, hit set by Raycast ()
				hit->distance = hitDistance; // set to distance along path
				return true;
			}
		}
		float segLen = Magnitude (targetPosition-sourcePosition);
		if (totalDistance+segLen > maxDistance)
		{
			// TODO: set proper height in "hit->position[1]"
			hit->position = Lerp (sourcePosition, targetPosition, (maxDistance-totalDistance)/segLen);
			hit->normal = Vector3f::zero;
			hit->distance = maxDistance;
			hit->mask = meshq->getPolygonFlags (agent->cornerPolys[i]);
			hit->hit = false;
			return false;
		}

		totalDistance += segLen;
		sourcePosition = targetPosition;
	}

	// Found end of straight path in "agent->cornerVerts"
	// - this is not necessarily the end of the path.
	return false;
}

bool NavMeshAgent::Warp (const Vector3f& newPosition)
{
	RemoveFromCrowdSystem ();
	Transform& transform = GetComponent (Transform);
	transform.SetPosition (newPosition);
	AddToCrowdSystem ();
	return InCrowdSystem ();
}

void NavMeshAgent::Move (const Vector3f& motion)
{
	REQUIRE_INITIALIZED ("Move",)

	const dtCrowdAgent* agent = GetInternalAgent ();
	Vector3f targetPos = motion + Vector3f (agent->npos);
	GetCrowdSystem ()->moveAgent (m_AgentHandle, targetPos.GetPtr ());

	if (m_UpdatePosition)
	{
		SetTransformFromGroundPosition (Vector3f (agent->npos));
	}
}

void NavMeshAgent::Stop (bool stopUpdates)
{
	REQUIRE_INITIALIZED ("Stop",)

	m_StopExplicit = true;
	if (stopUpdates)
	{
		SetVelocity (Vector3f::zero);
		SetUpdatePosition (false);
		m_StopRotating = true;
	}
}

void NavMeshAgent::Resume ()
{
	REQUIRE_INITIALIZED ("Resume",)

	m_StopExplicit = false;
	SetUpdatePosition (true);
}

void NavMeshAgent::CompleteOffMeshLink ()
{
	REQUIRE_INITIALIZED ("CompleteOffMeshLink",)

	GetCrowdSystem ()->completeOffMeshLink (m_AgentHandle);
}

NavMeshPathStatus NavMeshAgent::GetPathStatus () const
{
	if (!InCrowdSystem () || !GetInternalAgent ()->corridor.isPathValid ())
		return kPathInvalid;

	if (GetInternalAgent ()->corridor.isPathPartial ())
		return kPathPartial;

	return kPathComplete;
}

bool NavMeshAgent::IsPathValid () const
{
	if (!InCrowdSystem ())
		return false;

	return GetInternalAgent ()->corridor.isPathValid ();
}

bool NavMeshAgent::IsPathPartial () const
{
	if (!InCrowdSystem ())
		return false;

	return GetInternalAgent ()->corridor.isPathPartial ();
}

bool NavMeshAgent::IsPathStale () const
{
	if (!InCrowdSystem ())
		return false;

	return GetInternalAgent ()->corridor.isPathStale ();
}

void NavMeshAgent::SetLayerCost (unsigned int layer, float cost)
{
	REQUIRE_INITIALIZED ("SetLayerCost",)

	if (layer >= NavMeshLayers::kLayerCount)
	{
		ErrorString ("Index out of bounds");
		return;
	}

#if UNITY_EDITOR
	if (cost < 1.0f)
	{
		WarningStringObject (NavMeshLayers::s_WarningCostLessThanOne, this);
	}
#endif

	GetCrowdSystem ()->updateAgentFilterCost (m_AgentHandle, layer, cost);
}

float NavMeshAgent::GetLayerCost (unsigned int layer) const
{
	REQUIRE_INITIALIZED ("GetLayerCost", 0.0F)

	if (layer >= NavMeshLayers::kLayerCount)
	{
		ErrorString ("Index out of bounds");
		return 0.0F;
	}

	const dtQueryFilter& filter = GetFilter ();
	return filter.getAreaCost (layer);
}

void NavMeshAgent::SetHeight (float height)
{
	ABORT_INVALID_FLOAT (height, height, navmeshagent);
	m_Height = EnsurePositive (height);
	UpdateActiveAgentParameters ();
	SetDirty ();
}

void NavMeshAgent::SetBaseOffset (float baseOffset)
{
	ABORT_INVALID_FLOAT (baseOffset, baseOffset, navmeshagent);
	m_BaseOffset = baseOffset;
	SetDirty ();
	if (!InCrowdSystem ())
		return;

	if (m_UpdatePosition)
	{
		const dtCrowdAgent* agent = GetInternalAgent ();
		SetTransformFromGroundPosition (Vector3f (agent->npos));
	}
}

Vector2f NavMeshAgent::CalculateDimensionScales () const
{
	Vector3f absScale = Abs (GetComponent (Transform).GetWorldScaleLossy ());
	float scaleWidth = max (absScale.x, absScale.z);
	float scaleHeight = absScale.y;
	return Vector2f (scaleWidth, scaleHeight);
}

float NavMeshAgent::CalculateScaledRadius () const
{
	const Vector2f scale = CalculateDimensionScales ();
	return m_Radius*scale.x;
}

float NavMeshAgent::CalculateScaledHeight () const
{
	const Vector2f scale = CalculateDimensionScales ();
	return m_Height*scale.y;
}

void NavMeshAgent::SetRadius (float radius)
{
	ABORT_INVALID_FLOAT (radius, radius, navmeshagent);
	m_Radius = EnsurePositive (radius);
	UpdateActiveAgentParameters ();
	SetDirty ();
}

void NavMeshAgent::SetSpeed (float value)
{
	ABORT_INVALID_FLOAT (value, speed, navmeshagent);
	m_Speed = clamp (value, 0.0f, 1e15f); // squaring m_Speed keeps it below inf.
	UpdateActiveAgentParameters ();
	SetDirty ();
}

void NavMeshAgent::SetAvoidancePriority (int value)
{
	m_AvoidancePriority = value;
	UpdateActiveAgentParameters ();
	SetDirty ();
}

void NavMeshAgent::SetAngularSpeed (float value)
{
	ABORT_INVALID_FLOAT (value, angularSpeed, navmeshagent);
	m_AngularSpeed = value;
	UpdateActiveAgentParameters ();
	SetDirty ();
}

void NavMeshAgent::SetAcceleration (float value)
{
	ABORT_INVALID_FLOAT (value, acceleration, navmeshagent);
	m_Acceleration = value;
	UpdateActiveAgentParameters ();
	SetDirty ();
}

void NavMeshAgent::SetStoppingDistance (float value)
{
	ABORT_INVALID_FLOAT (value, stoppingDistance, navmeshagent);
	m_StoppingDistance = value;
	SetDirty ();
}

void NavMeshAgent::SetWalkableMask (UInt32 mask)
{
	if (m_WalkableMask == mask)
		return;

	m_WalkableMask = mask;

	UpdateActiveAgentParameters ();
	SetDirty ();
}

void NavMeshAgent::SetObstacleAvoidanceType (int type)
{
	m_ObstacleAvoidanceType = (ObstacleAvoidanceType) type;
	UpdateActiveAgentParameters ();
	SetDirty ();
}

const dtNavMeshQuery* NavMeshAgent::GetInternalNavMeshQuery ()
{
	return GetNavMeshManager ().GetInternalNavMeshQuery ();
}

dtCrowd* NavMeshAgent::GetCrowdSystem ()
{
	return GetNavMeshManager ().GetCrowdSystem ();
}

IMPLEMENT_CLASS_HAS_INIT (NavMeshAgent)
IMPLEMENT_OBJECT_SERIALIZE (NavMeshAgent)
