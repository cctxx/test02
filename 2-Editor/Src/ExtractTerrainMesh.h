#pragma once

#include "Runtime/Math/Vector3.h"
#include "Runtime/Geometry/AABB.h"
#include "Editor/Src/Utility/StaticEditorFlags.h"
#include "Runtime/Utilities/dynamic_array.h"

class TerrainData;
namespace Unity { class Material; }

struct TerrainBakeInfo
{
	TerrainBakeInfo()
		: terrainData(0)
		, templateMaterial(0)
		, position(0,0,0)
		, bounds()
		, castShadows(false)
		, lightmapSize(1)
		, lightmapIndex(0)
		, selected(false)
	{
	}
	
	TerrainData* terrainData;
	Unity::Material* templateMaterial;
	Vector3f     position;
	AABB         bounds;
	bool         castShadows;
	int          lightmapSize;
	int          lightmapIndex;
	bool         selected;
};

void ExtractStaticTerrains(StaticEditorFlags staticFlags, dynamic_array<TerrainBakeInfo>& outTerrainBakeInfo);