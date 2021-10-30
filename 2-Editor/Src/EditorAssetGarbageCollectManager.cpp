#include "UnityPrefix.h"
#include "Editor/Src/EditorAssetGarbageCollectManager.h"
#include "Runtime/Misc/PreloadManager.h"
#include "Runtime/Misc/GarbageCollectSharedAssets.h"
#include "Runtime/Profiler/CollectProfilerStats.h"

#define MEMORY_PRESSURE_COLLECT_THRESHOLD 50*1024*1024

void EditorAssetGarbageCollectManager::GarbageCollectIfHighMemoryUsage()
{
	if(!ShouldCollectDueToHighMemoryUsage())
		return;

	GetPreloadManager().LockPreloading();
	GarbageCollectSharedAssets(true);
	GetPreloadManager().UnlockPreloading();
}

bool EditorAssetGarbageCollectManager::ShouldCollectDueToHighMemoryUsage()
{
	return GetUsedHeapSize() > m_MemoryUsedAfterLastCollect + MEMORY_PRESSURE_COLLECT_THRESHOLD;
}

void EditorAssetGarbageCollectManager::SetPostCollectMemoryUsage()
{
	m_MemoryUsedAfterLastCollect = GetUsedHeapSize();
}

EditorAssetGarbageCollectManager* EditorAssetGarbageCollectManager::s_AssetGCManager = NULL;

void EditorAssetGarbageCollectManager::StaticInitialize()
{
	Assert(!s_AssetGCManager);
	s_AssetGCManager = UNITY_NEW(EditorAssetGarbageCollectManager,kMemEditorUtility);
}
void EditorAssetGarbageCollectManager::StaticDestroy()
{
	UNITY_DELETE(s_AssetGCManager,kMemEditorUtility);
}
