#include "UnityPrefix.h"
#include "AssetImporterPrefabQueue.h"
#include "Editor/Src/Prefabs/Prefab.h"


static AssetImportPrefabQueue* gSingleton = NULL;

void AssetImportPrefabQueue::InitializeClass ()
{
	Assert(gSingleton == NULL);
	gSingleton = new AssetImportPrefabQueue ();
}

void AssetImportPrefabQueue::CleanupClass ()
{
	Assert(gSingleton != NULL);
	delete gSingleton;
	gSingleton = NULL;
}


AssetImportPrefabQueue& AssetImportPrefabQueue::Get ()
{
	Assert(gSingleton != NULL);
	return *gSingleton;
}


void AssetImportPrefabQueue::QueuePrefab (PPtr<Prefab> prefab)
{
	// Check if there are any instances in the scene at all
	std::vector<Prefab*> prefabInstances;
	CalculateAllLoadedPrefabChildren(prefab, prefabInstances);
	
	if (!prefabInstances.empty())
		m_Prefabs.push_back(prefab);
}

void AssetImportPrefabQueue::ApplyMergePrefabQueue ()
{
	for (int i=0;i<m_Prefabs.size();i++)
		MergeAllPrefabInstances(m_Prefabs[i]);
	m_Prefabs.clear();
}