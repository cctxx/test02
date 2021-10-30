#pragma once
#ifndef RUNTIME_NAVMESH_AGENT
#define RUNTIME_NAVMESH_AGENT

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Graphics/Transform.h"
#include "NavMeshTypes.h"
#include "Runtime/Math/Vector2.h"

class NavMeshPath;
struct OffMeshLinkData;
struct NavMeshHit;

class dtNavMeshQuery;
class dtCrowd;
class dtQueryFilter;
struct dtCrowdAgentParams;
struct dtCrowdAgent;


class NavMeshAgent : public Behaviour
{
public:
	REGISTER_DERIVED_CLASS (NavMeshAgent, Behaviour)
	DECLARE_OBJECT_SERIALIZE (NavMeshAgent)

	NavMeshAgent (MemLabelId& label, ObjectCreationMode mode);
	// ~NavMeshAgent (); declared by a macro

	static void InitializeClass ();
	static void CleanupClass () {} // Avoid double free of Behavior

	virtual void AwakeFromLoad (AwakeFromLoadMode mode);
	virtual void CheckConsistency ();

	inline bool InCrowdSystem () const;
	inline void SetManagerHandle (int handle);
	bool SetDestination (const Vector3f& position);
	Vector3f GetDestination () const;
	void SetInternalAgentPosition (const Vector3f& position);

	int CalculatePolygonPath (const Vector3f& targetPosition, NavMeshPath* path);
	bool SetPath (const NavMeshPath* path);
	void CopyPath (NavMeshPath* path) const;
	void ResetPath ();
	bool PathPending () const;
	bool HasPath () const;
	bool DistanceToEdge (NavMeshHit* hit) const;
	bool Raycast (const Vector3f& position, NavMeshHit* hitPos);
	float GetRemainingDistance () const;

	// Consider moving these to the 'NavMeshPath' class
	bool SamplePathPosition (int collisionMask, float maxDistance, NavMeshHit* hit);
	Vector3f GetEndPositionOfCurrentPath () const;
	bool IsPathValid () const;
	bool IsPathPartial () const;
	bool IsPathStale () const;
	NavMeshPathStatus GetPathStatus () const;
	inline void ResetCachedPolyRef ();

	UInt32 GetWalkableMask () const			{ return m_WalkableMask; }
	void SetWalkableMask (UInt32 mask);

	void SetLayerCost (unsigned int layer, float cost);
	float GetLayerCost (unsigned int layer) const;

	inline ObstacleAvoidanceType GetObstacleAvoidanceType () const;
	void SetObstacleAvoidanceType (int type);

	void SetAvoidancePriority (int value);
	inline int GetAvoidancePriority () const;

	inline float GetSpeed () const;
	void  SetSpeed (float value);

	inline float GetAngularSpeed () const;
	void  SetAngularSpeed (float value);

	inline float GetAcceleration () const;
	void  SetAcceleration (float value);

	inline float GetStoppingDistance () const;
	void SetStoppingDistance (float value);

	Vector3f GetVelocity () const;
	void SetVelocity (const Vector3f& vel);
	Vector3f GetNextPosition () const;

	Vector3f GetNextCorner () const;
	Vector3f GetDesiredVelocity () const;

	bool IsOnOffMeshLink () const;
	void ActivateCurrentOffMeshLink (bool activated);
	bool GetCurrentOffMeshLinkData (OffMeshLinkData* data) const;
	bool GetNextOffMeshLinkData (OffMeshLinkData* data) const;
	bool SetOffMeshLinkDataFlags (OffMeshLinkData* data, const dtPolyRef polyRef) const;

	inline bool GetUpdatePosition () const;
	void SetUpdatePosition (bool inbool);
	inline bool GetUpdateRotation () const;
	void SetUpdateRotation (bool inbool);
	inline bool GetAutoTraverseOffMeshLink () const;
	void SetAutoTraverseOffMeshLink (bool inbool);
	inline bool GetAutoBraking () const;
	void SetAutoBraking (bool inbool);
	inline bool GetAutoRepath () const;
	void SetAutoRepath (bool inbool);

	inline float GetRadius () const;
	inline float GetHeight () const;
	inline float GetBaseOffset () const;

	float CalculateScaledRadius () const;
	float CalculateScaledHeight () const;

	void SetRadius (float radius);
	void SetHeight (float height);
	void SetBaseOffset (float baseOffset);

	bool Warp (const Vector3f& newPosition);
	void Move (const Vector3f& motion);
	void Stop (bool stopUpdates);
	void Resume ();
	void CompleteOffMeshLink ();

	void UpdateState ();
	void UpdateTransform (float deltaTime);

	inline Vector3f GetGroundPositionFromTransform () const;

	void OnNavMeshChanged ();
	inline void OnNavMeshCleanup ();

protected:
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	virtual void Reset ();
	virtual void SmartReset ();

	void OnTransformChanged (int mask);

private:

	void AddToCrowdSystem ();
	void RemoveFromCrowdSystem ();
	void ReinstateInCrowdSystem ();
	void StopOrResume (float remainingDistance);
	void RepathIfStuck (float remainingDistance);
	void UpdateRotation (Transform& transform, float deltaTime);

	void FillAgentParams (dtCrowdAgentParams& params);
	void UpdateActiveAgentParameters ();
	Vector2f CalculateDimensionScales () const;

	inline float CalculateScaledBaseOffset (const Transform& transform) const;
	inline void SetTransformFromGroundPosition (const Vector3f& groundPosition);

	int GetCurrentPolygonMask () const;
	const dtQueryFilter& GetFilter () const;
	const dtCrowdAgent* GetInternalAgent () const;
	static const dtNavMeshQuery* GetInternalNavMeshQuery ();
	static dtCrowd* GetCrowdSystem ();
	static inline float EnsurePositive (float value);

	Vector3f		m_Destination;
	Vector3f		m_RequestedDestination;
	float           m_Radius;
	float           m_Height;
	float           m_BaseOffset;
	float           m_Speed;
	float           m_AngularSpeed;
	float           m_Angle;
	float           m_Acceleration;
	float           m_StoppingDistance;

	// Internal Crowd system agent index
	dtCrowdHandle   m_AgentHandle;
	int             m_InstanceID;
	// Persistent manager handle
	int             m_ManagerHandle;

	dtPolyRef       m_CachedPolyRef;

	ObstacleAvoidanceType	m_ObstacleAvoidanceType; ///< enum { None = 0, Low Quality, Medium Quality, Good Quality, High Quality }
	UInt32          m_WalkableMask;
	int				m_AvoidancePriority;
	bool			m_AutoTraverseOffMeshLink;
	bool			m_AutoBraking;
	bool			m_AutoRepath;

	bool			m_UpdatePosition : 1;
	bool			m_UpdateRotation : 1;
	bool			m_StopDistance : 1;
	bool			m_StopExplicit : 1;
	bool			m_StopRotating : 1;
	bool			m_Request : 1;

	friend void DrawNavMeshAgent (const NavMeshAgent& agent);
};

inline float NavMeshAgent::EnsurePositive (float value)
{
	return std::max (0.00001F, value);
}

inline Vector3f NavMeshAgent::GetGroundPositionFromTransform () const
{
	const Transform& transform = GetComponent (Transform);
	return transform.TransformPoint (Vector3f (0, -m_BaseOffset, 0));
}

inline void NavMeshAgent::SetTransformFromGroundPosition (const Vector3f& groundPosition)
{
	Transform& transform = GetComponent (Transform);
	transform.SetPositionWithLocalOffset (groundPosition, Vector3f (0.0f, -m_BaseOffset, 0.0f));
}

inline float NavMeshAgent::CalculateScaledBaseOffset (const Transform& transform) const
{
	return m_BaseOffset * Abs (transform.GetWorldScaleLossy ().y);
}

inline bool NavMeshAgent::InCrowdSystem () const
{
	return m_AgentHandle.IsValid ();
}

inline void NavMeshAgent::SetManagerHandle (int handle)
{
	m_ManagerHandle = handle;
}

inline void NavMeshAgent::ResetCachedPolyRef ()
{
	m_CachedPolyRef = -1;
}

inline ObstacleAvoidanceType NavMeshAgent::GetObstacleAvoidanceType () const
{
	return m_ObstacleAvoidanceType;
}

inline int NavMeshAgent::GetAvoidancePriority () const
{
	return m_AvoidancePriority;
}

inline float NavMeshAgent::GetSpeed () const
{
	return m_Speed;
}

inline float NavMeshAgent::GetAngularSpeed () const
{
	return m_AngularSpeed;
}

inline float NavMeshAgent::GetAcceleration () const
{
	return m_Acceleration;
}

inline float NavMeshAgent::GetStoppingDistance () const
{
	return m_StoppingDistance;
}

inline bool NavMeshAgent::GetUpdatePosition () const
{
	return m_UpdatePosition;
}

inline bool NavMeshAgent::GetUpdateRotation () const
{
	return m_UpdateRotation;
}

inline bool NavMeshAgent::GetAutoTraverseOffMeshLink () const
{
	return m_AutoTraverseOffMeshLink;
}

inline bool NavMeshAgent::GetAutoBraking () const
{
	return m_AutoBraking;
}

inline bool NavMeshAgent::GetAutoRepath () const
{
	return m_AutoRepath;
}

inline float NavMeshAgent::GetRadius () const
{
	return m_Radius;
}

inline float NavMeshAgent::GetHeight () const
{
	return m_Height;
}

inline float NavMeshAgent::GetBaseOffset () const
{
	return m_BaseOffset;
}

inline void NavMeshAgent::OnNavMeshCleanup ()
{
	RemoveFromCrowdSystem ();
}

#endif
