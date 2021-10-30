#pragma once

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Interfaces/ITerrainManager.h"
#include <list>

class TerrainData;
class HeightMap;

class TerrainManager : public ITerrainManager
{
public:
	TerrainManager();

	virtual void CullAllTerrains (int cullingMask);
	virtual void SetLightmapIndexOnAllTerrains (int lightmapIndex);
	virtual void AddTerrainAndSetActive (TerrainInstance* terrain);
	virtual void RemoveTerrain (TerrainInstance* terrain);
	TerrainInstance* GetActiveTerrain() const { return m_ActiveTerrain; }
	const TerrainList& GetActiveTerrains() const { return m_ActiveTerrains; }
	void UnloadTerrainsFromGfxDevice ();
	void ReloadTerrainsToGfxDevice ();

	// TODO Move these to heightmap, does not really belong here
#if ENABLE_PHYSICS	
	virtual NxHeightField* Heightmap_GetNxHeightField(Heightmap& heightmap);
#endif
	virtual int Heightmap_GetMaterialIndex(Heightmap& heightmap);
	virtual Vector3f Heightmap_GetSize(Heightmap& heightmap);
	
	// TODO this should move to TerrainData
	/// Extracts the height on the heightmap from a TerrainData
	virtual bool GetInterpolatedHeight (const Object* terrainData, const Vector3f& terrainPosition, const Vector3f& position, float& outputHeight);

	virtual void CollectTreeRenderers(dynamic_array<SceneNode>& sceneNodes, dynamic_array<AABB>& boundingBoxes) const;
	
	static void InitializeClass ();
	static void CleanupClass ();
	PPtr<GameObject> CreateTerrainGameObject (const TerrainData& assignTerrain);

private:

	TerrainList m_TempCulledTerrains;
	TerrainList m_ActiveTerrains;
	TerrainInstance* m_ActiveTerrain;
};
