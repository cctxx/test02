#ifndef _EDITOR_ASSET_GC_MANAGER_
#define _EDITOR_ASSET_GC_MANAGER_

class EditorAssetGarbageCollectManager
{
public:
	EditorAssetGarbageCollectManager():m_MemoryUsedAfterLastCollect(0){}
	~EditorAssetGarbageCollectManager(){}

	bool ShouldCollectDueToHighMemoryUsage();
	void GarbageCollectIfHighMemoryUsage();
	void SetPostCollectMemoryUsage();

	static void StaticInitialize();
	static void StaticDestroy();
	static EditorAssetGarbageCollectManager* Get() {Assert(s_AssetGCManager); return s_AssetGCManager;}

private:
	size_t m_MemoryUsedAfterLastCollect;

	static EditorAssetGarbageCollectManager* s_AssetGCManager;
};

#endif