#include "UnityPrefix.h"
#include "NavMeshManager.h"

#include "DetourFeatures.h"
#include "DetourCrowd.h"
#include "DetourCrowdTypes.h"
#include "HeightMeshQuery.h"
#include "Runtime/Input/TimeManager.h"
#include "NavMeshAgent.h"
#include "NavMeshObstacle.h"
#include "NavMeshProfiler.h"
#include "OffMeshLink.h"
#include "NavMeshCarving.h"
#include "Runtime/Threads/JobScheduler.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Core/Callbacks/PlayerLoopCallbacks.h"
#include "Runtime/Misc/BuildSettings.h"


static const int MAX_ITERS_PER_UPDATE = 100;
const int NavMeshManager::kInitialAgentCount = 4;

PROFILER_INFORMATION (gCrowdManagerUpdate, "CrowdManager.Update", kProfilerAI)
PROFILER_INFORMATION (gNavMeshAgentsUpdateState, "CrowdManager.NavMeshAgentStates", kProfilerAI)
PROFILER_INFORMATION (gNavMeshAgentsUpdateTransform, "CrowdManager.NavMeshAgentTransforms", kProfilerAI)

NavMeshManager::NavMeshManager ()
{
	m_CarvingSystem = NULL;
	m_CrowdSystem = NULL;
	m_CrowdAgentDebugInfo = NULL;
	m_Profiler = UNITY_NEW (CrowdProfiler, kMemNavigation) ();
#if UNITY_EDITOR
	m_CrowdAgentDebugInfo =  UNITY_NEW (dtCrowdAgentDebugInfo, kMemNavigation);
	memset (m_CrowdAgentDebugInfo, 0, sizeof (*m_CrowdAgentDebugInfo));
	m_CrowdAgentDebugInfo->idx = -1;
#endif
}

NavMeshManager::~NavMeshManager ()
{
	if (m_CrowdAgentDebugInfo)
	{
		UNITY_DELETE (m_CrowdAgentDebugInfo, kMemNavigation);
	}
	UNITY_DELETE (m_CrowdSystem, kMemNavigation);
	UNITY_DELETE (m_Profiler, kMemNavigation);
	UNITY_DELETE (m_CarvingSystem, kMemNavigation);
}

template <typename T>
static inline int RegisterInArray (dynamic_array<T*>& array, T& element)
{
	int handle = array.size ();
	array.push_back (&element);
	return handle;
}

template <typename T>
static inline void UnregisterFromArray (dynamic_array<T*>& array, int handle)
{
	Assert (handle >= 0 && handle < array.size ());
	int last = array.size () - 1;
	if (handle != last)
	{
		T* swap = array[last];
		array[handle] = swap;
		swap->SetManagerHandle (handle);
	}
	array.pop_back ();
}

void NavMeshManager::RegisterAgent (NavMeshAgent& agent, int& handle)
{
	Assert (handle == -1);
	handle = RegisterInArray (m_Agents, agent);
}

void NavMeshManager::UnregisterAgent (int& handle)
{
	UnregisterFromArray (m_Agents, handle);
	handle = -1;
}

void NavMeshManager::RegisterObstacle (NavMeshObstacle& obstacle, int& handle)
{
	Assert (handle == -1);
	handle = RegisterInArray (m_Obstacles, obstacle);
}

void NavMeshManager::UnregisterObstacle (int& handle)
{
	UnregisterFromArray (m_Obstacles, handle);
	handle = -1;
}

void NavMeshManager::RegisterOffMeshLink (OffMeshLink& link, int& handle)
{
	Assert (handle == -1);
	handle = RegisterInArray (m_Links, link);
}

void NavMeshManager::UnregisterOffMeshLink (int& handle)
{
	UnregisterFromArray (m_Links, handle);
	handle = -1;
}

#if ENABLE_NAVMESH_CARVING
void NavMeshManager::UpdateCarving ()
{
	if (m_CarvingSystem && m_CarvingSystem->Carve ())
	{
		InvalidateDynamicLinks ();
		for (size_t i=0;i<m_Agents.size (); ++i)
		{
			m_Agents[i]->OnNavMeshChanged ();
		}
	}
}
#else
void NavMeshManager::UpdateCarving () {}
#endif


#if DT_DYNAMIC_OFFMESHLINK
PROFILER_INFORMATION (gCrowdManagerLinks, "CrowdManager.DynamicOffMeshLinks", kProfilerAI)

#include "Runtime/NavMesh/NavMesh.h"
#include "Runtime/NavMesh/NavMeshSettings.h"
void NavMeshManager::InvalidateDynamicLinks ()
{
	NavMesh* navmesh = GetNavMeshSettings ().GetNavMesh ();
	dtNavMesh* detourNavMesh = navmesh->GetInternalNavMesh ();
	detourNavMesh->ClearDynamicOffMeshLinks ();
	for (size_t i = 0; i < m_Links.size (); ++i)
	{
		m_Links[i]->OnNavMeshChanged ();
	}
}

void NavMeshManager::UpdateDynamicLinks ()
{
	PROFILER_AUTO (gCrowdManagerLinks, NULL)
	if (IsWorldPlaying ())
	{
		for (size_t i = 0; i < m_Links.size (); ++i)
			m_Links[i]->UpdateMovedPositions ();
	}
	else
	{
		for (size_t i = 0; i < m_Links.size (); ++i)
			m_Links[i]->UpdatePositions ();
	}
}
#else
void NavMeshManager::InvalidateDynamicLinks () {}
void NavMeshManager::UpdateDynamicLinks () {}
#endif

void NavMeshManager::UpdateCrowdSystem (float deltaTime)
{
	m_CrowdSystem->update (deltaTime, m_Profiler);
}

void NavMeshManager::Update ()
{
	if (GetInternalNavMeshQuery () == NULL)
		return;

	UpdateCarving ();
	UpdateDynamicLinks ();

	const float deltaTime = GetDeltaTime ();
	if (deltaTime == 0.0f)
		return;

	PROFILER_BEGIN (gCrowdManagerUpdate, NULL)

	UpdateCrowdSystem (deltaTime);

	PROFILER_BEGIN (gNavMeshAgentsUpdateState, NULL)
	for (size_t i = 0; i < m_Agents.size (); ++i)
	{
		m_Agents[i]->UpdateState ();
	}
	PROFILER_END

	PROFILER_BEGIN (gNavMeshAgentsUpdateTransform, NULL)
	for (size_t i = 0; i < m_Agents.size (); ++i)
	{
		m_Agents[i]->UpdateTransform (deltaTime);
	}
	PROFILER_END

	PROFILER_END
}

// Cleanup references to 'mesh'.
// Skips cleanup if the 'mesh' is not the currently loaded internal navmesh.
// This is needed because when loading scenes we'll temporarily have two instances of NavMesh class.
// (i.e. ctor of new NavMesh object is call before dtor of old NavMesh object).
void NavMeshManager::CleanupMeshDependencies (const dtNavMesh* mesh)
{
	if (mesh == GetInternalNavMesh ())
	{
		CleanupMeshDependencies ();
	}
}

// Unconditionally cleanup the navmesh dependencies
void NavMeshManager::CleanupMeshDependencies ()
{
	NotifyNavMeshCleanup ();

	if (m_CrowdSystem)
	{
		m_CrowdSystem->purge ();
		UNITY_DELETE (m_CrowdSystem, kMemNavigation);
	}
}

const dtMeshHeader* NavMeshManager::GetNavMeshHeader (const dtNavMesh* navmesh)
{
	if (navmesh == NULL || navmesh->tileCount () == 0)
	{
		return NULL;
	}
	return navmesh->getTile (0)->header;
}

void NavMeshManager::Initialize (const dtNavMesh* navMesh, const HeightMeshQuery* heightMeshQuery)
{
	InitializeCarvingSystem ();

	const dtMeshHeader* header = GetNavMeshHeader (navMesh);
	if (!header)
	{
		CleanupMeshDependencies ();
		return;
	}

	if (!InitializeCrowdSystem (navMesh, heightMeshQuery, header))
	{
		CleanupMeshDependencies ();
		return;
	}

	InitializeObstacleSamplingQuality ();

	NotifyNavMeshChanged ();
}

void NavMeshManager::InitializeCarvingSystem ()
{
#if ENABLE_NAVMESH_CARVING
	// Carving requires advanced version. Otherwise leave the null pointer for 'm_CarvingSystem'.
	if (!m_CarvingSystem && GetBuildSettings ().hasAdvancedVersion)
	{
		m_CarvingSystem = UNITY_NEW (NavMeshCarving, kMemNavigation);
	}
#endif
}

bool NavMeshManager::InitializeCrowdSystem (const dtNavMesh* navmesh, const HeightMeshQuery* heightMeshQuery, const dtMeshHeader* header)
{
	Assert (navmesh);

	// Lazily create crowd manager
	if (m_CrowdSystem == NULL)
	{
		m_CrowdSystem = UNITY_NEW (dtCrowd, kMemNavigation) ();
		if (m_CrowdSystem == NULL)
		{
			return false;
		}
		m_CrowdSystem->init (kInitialAgentCount, MAX_ITERS_PER_UPDATE); /////@TODO: WRONG
	}

	const float queryRange = 10.0f*header->walkableRadius; ///@TODO: UN-HACK
	if (!m_CrowdSystem->setNavMesh (navmesh, queryRange))
	{
		return false;
	}

	m_CrowdSystem->setHeightMeshQuery (heightMeshQuery);

	return true;
}

void NavMeshManager::InitializeObstacleSamplingQuality ()
{
	// Setup local avoidance params to different qualities.
	dtObstacleAvoidanceParams params;
	// Use mostly default settings, copy from dtCrowd.
	memcpy (&params, m_CrowdSystem->getObstacleAvoidanceParams (0), sizeof (dtObstacleAvoidanceParams));

	// Low (11)
	params.adaptiveDivs = 5;
	params.adaptiveRings = 2;
	params.adaptiveDepth = 1;
	m_CrowdSystem->setObstacleAvoidanceParams (kLowQualityObstacleAvoidance, &params);

	// Medium (22)
	params.adaptiveDivs = 5;
	params.adaptiveRings = 2;
	params.adaptiveDepth = 2;
	m_CrowdSystem->setObstacleAvoidanceParams (kMedQualityObstacleAvoidance, &params);

	// Good (45)
	params.adaptiveDivs = 7;
	params.adaptiveRings = 2;
	params.adaptiveDepth = 3;
	m_CrowdSystem->setObstacleAvoidanceParams (kGoodQualityObstacleAvoidance, &params);

	// High (66)
	params.adaptiveDivs = 7;
	params.adaptiveRings = 3;
	params.adaptiveDepth = 3;
	m_CrowdSystem->setObstacleAvoidanceParams (kHighQualityObstacleAvoidance, &params);
}

void NavMeshManager::NotifyNavMeshChanged ()
{
	for (size_t i = 0; i < m_Agents.size (); ++i)
		m_Agents[i]->OnNavMeshChanged ();

	for (size_t i = 0; i < m_Obstacles.size (); ++i)
		m_Obstacles[i]->OnNavMeshChanged ();

	for (size_t i = 0; i < m_Links.size (); ++i)
		m_Links[i]->OnNavMeshChanged ();
}

void NavMeshManager::NotifyNavMeshCleanup ()
{
	for (size_t i = 0; i < m_Agents.size (); ++i)
		m_Agents[i]->OnNavMeshCleanup ();

	for (size_t i = 0; i < m_Obstacles.size (); ++i)
		m_Obstacles[i]->OnNavMeshCleanup ();

	for (size_t i = 0; i < m_Links.size (); ++i)
		m_Links[i]->OnNavMeshCleanup ();
}

void NavMeshManager::UpdateAllNavMeshAgentCosts (int layerIndex, float layerCost)
{
	if (m_CrowdSystem != NULL)
	{
		m_CrowdSystem->UpdateFilterCost (layerIndex, layerCost);
	}
}

const dtNavMeshQuery* NavMeshManager::GetInternalNavMeshQuery () const
{
	if (m_CrowdSystem == NULL)
		return NULL;
	return m_CrowdSystem->getNavMeshQuery ();
}

const dtNavMesh* NavMeshManager::GetInternalNavMesh () const
{
	if (m_CrowdSystem == NULL || m_CrowdSystem->getNavMeshQuery () == NULL)
		return NULL;
	return m_CrowdSystem->getNavMeshQuery ()->getAttachedNavMesh ();
}

static NavMeshManager* gManager = NULL;

void InitializeNavMeshManager ()
{
	Assert (gManager == NULL);
	gManager = UNITY_NEW (NavMeshManager, kMemNavigation) ();

	REGISTER_PLAYERLOOP_CALL(NavMeshUpdate, GetNavMeshManager ().Update ());
}

void CleanupNavMeshManager ()
{
	UNITY_DELETE (gManager, kMemNavigation);
	gManager = NULL;
}

NavMeshManager& GetNavMeshManager ()
{
	return *gManager;
}
