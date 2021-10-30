#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

#include "Runtime/Filters/Mesh/LodMesh.h"
#include "TerrainData.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Utilities/MemoryPool.h"

class DetailPatchRender
{
public:
	Mesh *mesh;
	bool isCulledVisible;
	bool isMeshNull;
	bool inited;
	int lastUsed;
	int x;
	int y;
	
	DetailPatchRender() { inited = false; mesh = NULL; }	
	~DetailPatchRender() { DestroySingleObject (mesh); }
};

class DetailRenderer 
{
public: 
	Material *m_Materials[kDetailRenderModeCount];
	
	DetailRenderer (PPtr<TerrainData> terrain, Vector3f position, int lightmapIndex);
	void Render (Camera *camera, float viewDistance, int layer, float detailDensity);
	void Cleanup ();
	void ReloadAllDetails();
	void ReloadDirtyDetails();

	int GetLightmapIndex() { return m_LightmapIndex; }
	void SetLightmapIndex(int value) { m_LightmapIndex = value; }

private:
	typedef std::map<UInt32,DetailPatchRender, std::less<UInt32> ,memory_pool<std::pair<const UInt32, DetailPatchRender> > > DetailList;

	PPtr<TerrainData> m_Database;
	Vector3f m_TerrainSize;
	UInt8 m_LightmapIndex;

	DetailList m_Patches[kDetailRenderModeCount];
	Vector3f m_Position;
	int m_RenderCount;
	float m_LastTime;

	DetailPatchRender& GrabCachedPatch (int x, int y, int lightmapIndex, DetailRenderMode mode, float density);
};

#endif // ENABLE_TERRAIN
