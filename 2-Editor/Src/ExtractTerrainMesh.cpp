#include "UnityPrefix.h"
#include "ExtractTerrainMesh.h"
#include "Runtime/Terrain/TerrainData.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Utilities/dynamic_array.h"

using namespace std;


void ExtractStaticTerrains(StaticEditorFlags staticFlags, dynamic_array<TerrainBakeInfo>& outTerrainBakeInfo)
{
	// get terrain datas and positions from C#
	MonoArray* monoTerrainDatas = NULL;
	MonoArray* monoTerrainPositions = NULL;
	MonoArray* monoTerrainCastShadows = NULL;
	MonoArray* monoTerrainMaterials = NULL;
	MonoArray* monoTerrainLightmapSizes = NULL;
	MonoArray* monoTerrainLightmapIndices = NULL;
	MonoArray* monoTerrainSelection = NULL;
	unsigned castedFlags = staticFlags;
	
	void* params[] = {
		&castedFlags,
		&monoTerrainDatas,
		&monoTerrainPositions,
		&monoTerrainCastShadows,
		&monoTerrainMaterials,
		&monoTerrainLightmapSizes,
		&monoTerrainLightmapIndices,
		&monoTerrainSelection
	};
	CallStaticMonoMethod("TerrainEditorUtility", "Extract", params);
	
	
	vector<TerrainData*> terrainData;
	vector<Vector3f> terrainPositions;
	vector<int> castShadows;
	vector<Material*> materials;
	vector<int> lightmapSizes;
	vector<int> lightmapIndices;
	vector<bool> selection;
	
	MonoObjectArrayToVector(monoTerrainDatas, terrainData);
	MonoArrayToVector<Vector3f>(monoTerrainPositions, terrainPositions);
	MonoArrayToVector<int>(monoTerrainCastShadows, castShadows);
	MonoObjectArrayToVector(monoTerrainMaterials, materials);
	MonoArrayToVector<int>(monoTerrainLightmapSizes, lightmapSizes);
	MonoArrayToVector<int>(monoTerrainLightmapIndices, lightmapIndices);
	MonoArrayToVector<bool>(monoTerrainSelection, selection);
	
	Assert(terrainData.size() == terrainPositions.size() &&
		   terrainPositions.size() == castShadows.size() &&
		   castShadows.size() == materials.size() &&
		   castShadows.size() == lightmapSizes.size() && 
		   lightmapSizes.size() == lightmapIndices.size() &&
		   lightmapIndices.size() == selection.size());
	
	outTerrainBakeInfo.clear();
	for (int i=0;i<terrainData.size();i++)
	{
		TerrainBakeInfo info;
		info.terrainData = terrainData[i];
		info.position = terrainPositions[i];
		info.castShadows = castShadows[i];
		info.templateMaterial = materials[i];
		info.lightmapSize = lightmapSizes[i];
		info.lightmapIndex = lightmapIndices[i];
		info.selected = selection[i];
		info.bounds = info.terrainData->GetHeightmap().GetBounds();
		info.bounds.GetCenter() += info.position;
		outTerrainBakeInfo.push_back(info);
	}
}
