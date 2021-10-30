#ifndef RUNTIME_HEIGHT_MESH_QUERY
#define RUNTIME_HEIGHT_MESH_QUERY

#include "External/Recast/Detour/Include/DetourNavMeshQuery.h"
#include "HeightmapData.h"

class dtNavMeshQuery;
class Vector3f;

// Query specialization for Height Placement of a HeightMesh.
class HeightMeshQuery : public dtHeightQuery
{
public:
	HeightMeshQuery ();
	virtual ~HeightMeshQuery ();

	void Init (const HeightmapDataVector* heightMaps, float verticalRayOffset);
	virtual dtStatus getHeight (const dtMeshTile* tile, const dtPoly* poly, const float* pos, float* height) const;
	bool GetTerrainHeight (const Vector3f& position, float* height) const;

private:
	bool GetGeometryHeight (const dtMeshTile* tile, const dtPoly* poly, const Vector3f& pos, float* height) const;

	const HeightmapDataVector* m_HeightMaps;
	float m_VerticalRayOffset;
};

#endif
