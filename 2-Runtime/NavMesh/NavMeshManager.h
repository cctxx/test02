#pragma once

#include "Runtime/BaseClasses/GameManager.h"

class CrowdProfiler;
class HeightMeshQuery;
class NavMeshAgent;
class NavMeshCarving;
class NavMeshObstacle;
class OffMeshLink;
class dtCrowd;
class dtNavMesh;
class dtNavMeshQuery;
struct dtCrowdAgentDebugInfo;
struct dtMeshHeader;


class NavMeshManager
{
public:

	NavMeshManager ();
	~NavMeshManager ();

	// NavMeshModule Interface
	virtual void Update ();

	void Initialize (const dtNavMesh* navMesh, const HeightMeshQuery* heightMeshQuery);
	void CleanupMeshDependencies (const dtNavMesh* mesh);
	void CleanupMeshDependencies ();

	inline dtCrowd* GetCrowdSystem ();
	inline NavMeshCarving* GetCarvingSystem ();
	const dtNavMeshQuery* GetInternalNavMeshQuery () const;
	const dtNavMesh* GetInternalNavMesh () const;

	void RegisterAgent (NavMeshAgent& agent, int& handle);
	void UnregisterAgent (int& handle);

	void RegisterObstacle (NavMeshObstacle& obstacle, int& handle);
	void UnregisterObstacle (int& handle);

	void RegisterOffMeshLink (OffMeshLink& link, int& handle);
	void UnregisterOffMeshLink (int& handle);

	void UpdateAllNavMeshAgentCosts (int layerIndex, float layerCost);

#if UNITY_EDITOR
	inline dtCrowdAgentDebugInfo* GetInternalDebugInfo ();
#endif

private:
	const dtMeshHeader* GetNavMeshHeader (const dtNavMesh* navmesh);
	bool InitializeCrowdSystem (const dtNavMesh* navmesh, const HeightMeshQuery* heightMeshQuery, const dtMeshHeader* header);
	void InitializeObstacleSamplingQuality ();
	void InitializeCarvingSystem ();

	void NotifyNavMeshChanged ();
	void NotifyNavMeshCleanup ();
	void UpdateCrowdSystem (float deltaTime);
	void UpdateCarving ();
	void UpdateDynamicLinks ();
	void InvalidateDynamicLinks ();

	dynamic_array<NavMeshAgent*> m_Agents;
	dynamic_array<NavMeshObstacle*> m_Obstacles;
	dynamic_array<OffMeshLink*> m_Links;

	NavMeshCarving* m_CarvingSystem;

	dtCrowd* m_CrowdSystem;
	dtCrowdAgentDebugInfo* m_CrowdAgentDebugInfo;
	CrowdProfiler* m_Profiler;
	static const int kInitialAgentCount;
};

inline dtCrowd* NavMeshManager::GetCrowdSystem ()
{
	return m_CrowdSystem;
}

inline NavMeshCarving* NavMeshManager::GetCarvingSystem ()
{
	return m_CarvingSystem;
}

#if UNITY_EDITOR
inline dtCrowdAgentDebugInfo* NavMeshManager::GetInternalDebugInfo ()
{
	return m_CrowdAgentDebugInfo;
}
#endif // UNITY_EDITOR

NavMeshManager& GetNavMeshManager ();
void InitializeNavMeshManager ();
void CleanupNavMeshManager ();
