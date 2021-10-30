#pragma once
#include <list>

#include "Runtime/Modules/ExportModules.h"
#include "Runtime/Camera/SceneNode.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Utilities/dynamic_array.h"

class Light;
class Object;
class TerrainInstance;
class Vector3f;
class NxHeightField;
class Heightmap;

typedef UNITY_LIST(kMemRenderer, TerrainInstance*) TerrainList;

class ITerrainManager
{
public:
#if ENABLE_PHYSICS
	virtual NxHeightField* Heightmap_GetNxHeightField(Heightmap& heightmap) = 0;
#endif
	virtual int Heightmap_GetMaterialIndex(Heightmap& heightmap) = 0;
	virtual Vector3f Heightmap_GetSize(Heightmap& heightmap) = 0;

	/// Extracts the height on the heightmap from a TerrainData
	/// Returns true if the heightmap was sampled and the position is inside the terrain extents.
	virtual bool GetInterpolatedHeight (const Object* terrainData, const Vector3f& terrainPosition, const Vector3f& position, float& outputHeight) = 0;

	/// Render all terrains
	virtual void CullAllTerrains (int cullingMask) = 0;
	/// Set the lightmap index on all terrains
	virtual void SetLightmapIndexOnAllTerrains (int lightmapIndex) = 0;
	virtual void AddTerrainAndSetActive (TerrainInstance* terrain) = 0;
	virtual void RemoveTerrain (TerrainInstance* terrain) = 0;
	virtual TerrainInstance* GetActiveTerrain () const = 0;
	virtual const TerrainList& GetActiveTerrains () const = 0;
	virtual void UnloadTerrainsFromGfxDevice () = 0;
	virtual void ReloadTerrainsToGfxDevice () = 0;

	virtual void CollectTreeRenderers(dynamic_array<SceneNode>& sceneNodes, dynamic_array<AABB>& boundingBoxes) const = 0;
};

EXPORT_COREMODULE ITerrainManager*  GetITerrainManager ();
EXPORT_COREMODULE void       SetITerrainManager (ITerrainManager* manager);
