#pragma once
#ifndef NAVMESH_TYPES_H_INCLUDED
#define NAVMESH_TYPES_H_INCLUDED

#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "DetourReference.h"
class NavMeshPath;

// Keep this enum in sync with the one defined in "NavMeshAgentBindings.txt"
enum ObstacleAvoidanceType
{
	kNoObstacleAvoidance = 0,
	kLowQualityObstacleAvoidance = 1,
	kMedQualityObstacleAvoidance = 2,
	kGoodQualityObstacleAvoidance = 3,
	kHighQualityObstacleAvoidance = 4
};

// Keep this struct in sync with the one defined in "NavMeshBindings.txt"
struct NavMeshHit
{
	Vector3f position;
	Vector3f normal;
	float    distance;
	int      mask;
	int      hit;
};

// Keep this struct in sync with the one defined in "NavMeshBindings.txt"
enum NavMeshPathStatus
{
	kPathComplete = 0,
	kPathPartial = 1,
	kPathInvalid = 2
};

// Keep this enum in sync with the one defined in "NavMeshBindings.txt"
enum OffMeshLinkType
{
	kLinkTypeManual = 0,
	kLinkTypeDropDown = 1,
	kLinkTypeJumpAcross = 2
};

// Keep this struct in sync with the one defined in "NavMeshBindings.txt"
struct OffMeshLinkData
{
	int m_Valid;
	int m_Activated;
	int m_InstanceID;
	OffMeshLinkType m_LinkType;
	Vector3f m_StartPos;
	Vector3f m_EndPos;
};

// Used in: NavMeshBindings.txt, NavMeshAgentBindings.txt, NavMeshPathBindings.txt
struct MonoNavMeshPath
{
	MonoNavMeshPath ()
	: native (NULL)
	, corners (SCRIPTING_NULL)
	{}

	NavMeshPath* native;
	ScriptingObjectPtr corners;
};

struct NavMeshCarveData
{
	Matrix4x4f transform;
	Vector3f size;
};

#endif
