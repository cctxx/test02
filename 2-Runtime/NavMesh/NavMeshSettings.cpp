#include "UnityPrefix.h"
#include "NavMeshSettings.h"
#include "NavMeshManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "OffMeshLink.h"
#include "NavMeshLayers.h"
#include "NavMesh.h"
#include "HeightmapData.h"
#include "DetourNavMesh.h"

void NavMeshSettings::InitializeClass ()
{
	InitializeNavMeshManager ();
}

void NavMeshSettings::CleanupClass ()
{
	CleanupNavMeshManager ();
}


NavMeshSettings::NavMeshSettings (MemLabelId& label, ObjectCreationMode mode)
	: Super (label, mode)
{
}

NavMeshSettings::~NavMeshSettings ()
{
	GetNavMeshManager ().CleanupMeshDependencies ();
}

void NavMeshSettings::Reset ()
{
	Super::Reset ();

	#if UNITY_EDITOR
	m_BuildSettings = NavMeshBuildSettings ();
	#endif
}

void NavMeshSettings::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad (mode);

	// Initialize NavMesh
	const dtNavMesh* internalNavMesh = NULL;
	const HeightMeshQuery* heightMeshQuery = NULL;
	if (m_NavMesh)
	{
		// Calling m_NavMesh->Create () here to ensure state of navmesh is restored.
		// Were are already copying the data so memory usage is not affected.
		m_NavMesh->Create ();
		internalNavMesh = m_NavMesh->GetInternalNavMesh ();
		heightMeshQuery = m_NavMesh->GetHeightMeshQuery ();
	}
	else
	{
		GetNavMeshManager ().CleanupMeshDependencies ();
	}
	GetNavMeshManager ().Initialize (internalNavMesh, heightMeshQuery);
}

template<class T>
void NavMeshSettings::Transfer (T& transfer)
{
	Super::Transfer (transfer);

	TRANSFER_EDITOR_ONLY (m_BuildSettings);
	TRANSFER (m_NavMesh);
}

bool NavMeshSettings::SetOffMeshPolyInstanceID (dtPolyRef ref, int instanceID)
{
	if (dtNavMesh* navmesh = GetInternalNavMesh ())
		return navmesh->setOffMeshPolyInstanceID (ref, instanceID) == DT_SUCCESS;
	return false;
}

void NavMeshSettings::SetOffMeshPolyCostOverride (dtPolyRef ref, float costOverride)
{
	if (dtNavMesh* navmesh = GetInternalNavMesh ())
		navmesh->setOffMeshPolyCostOverride (ref, costOverride);
}

void NavMeshSettings::SetOffMeshPolyAccess (dtPolyRef ref, bool access)
{
	if (dtNavMesh* navmesh = GetInternalNavMesh ())
		navmesh->setOffMeshPolyAccess (ref, access);
}

dtNavMesh* NavMeshSettings::GetInternalNavMesh ()
{
	NavMesh* navmesh = GetNavMesh ();
	if (navmesh == NULL)
		return NULL;
	return navmesh->GetInternalNavMesh ();
}

IMPLEMENT_OBJECT_SERIALIZE (NavMeshSettings)
IMPLEMENT_CLASS_HAS_INIT (NavMeshSettings)
GET_MANAGER (NavMeshSettings)
