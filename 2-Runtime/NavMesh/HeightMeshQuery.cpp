#include "UnityPrefix.h"
#include "HeightMeshQuery.h"
#include "Runtime/Interfaces/ITerrainManager.h"


HeightMeshQuery::HeightMeshQuery ()
: m_HeightMaps (NULL)
, m_VerticalRayOffset (0.0f)
{
}

HeightMeshQuery::~HeightMeshQuery ()
{
}

void HeightMeshQuery::Init (const HeightmapDataVector* heightMaps, float verticalRayOffset)
{
	m_HeightMaps = heightMaps;
	m_VerticalRayOffset = verticalRayOffset;
}

dtStatus HeightMeshQuery::getHeight (const dtMeshTile* tile, const dtPoly* poly, const float* pos, float* height) const
{
	Vector3f rayStart (pos[0], pos[1] + m_VerticalRayOffset, pos[2]);
	float geometryHeight, terrainHeight;
	bool bGeometryHit = GetGeometryHeight (tile, poly, rayStart, &geometryHeight);
	bool bTerrainHit = GetTerrainHeight (rayStart, &terrainHeight);

	if (bGeometryHit && bTerrainHit)
	{
		float geomDist = Abs (rayStart.y - geometryHeight);
		float terrainDist = Abs (rayStart.y - terrainHeight);
		if (geomDist < terrainDist)
			(*height) = geometryHeight;
		else
			(*height) = terrainHeight;

		return DT_SUCCESS;
	}
	else if (bGeometryHit)
	{
		(*height) = geometryHeight;
		return DT_SUCCESS;
	}
	else if (bTerrainHit)
	{
		(*height) = terrainHeight;
		return DT_SUCCESS;
	}
	else
	{
		(*height) = pos[1];
		return DT_FAILURE;
	}
}


// HeightMesh Intersection
// Find the height of the nearest triangle with the same 2D values.
bool HeightMeshQuery::GetGeometryHeight (const dtMeshTile* tile, const dtPoly* poly, const Vector3f& pos, float* height) const
{
	Assert (poly->getType () == DT_POLYTYPE_GROUND);
	Assert (height != NULL);
	*height = pos.y;

	bool hit = false;
	float bestHeight = std::numeric_limits<float>::infinity ();

	const unsigned int ip = (unsigned int)(poly - tile->polys);
	const dtPolyDetail* pd = &tile->detailMeshes[ip];
	for (int j = 0; j < pd->triCount; ++j)
	{
		const dtPolyDetailIndex* t = &tile->detailTris[(pd->triBase+j)*4];
		const float* v[3];
		for (int k = 0; k < 3; ++k)
		{
			if (t[k] < poly->vertCount)
				v[k] = &tile->verts[poly->verts[t[k]]*3];
			else
				v[k] = &tile->detailVerts[(pd->vertBase+(t[k]-poly->vertCount))*3];
		}
		float h;
		if (dtClosestHeightPointTriangle (pos.GetPtr (), v[0], v[1], v[2], h))
		{
			if (Abs (pos.y - h) < Abs (pos.y - bestHeight))
			{
				*height = h;
				bestHeight = h;
				hit = true;
			}
		}
	}

	return hit;
}

bool HeightMeshQuery::GetTerrainHeight (const Vector3f& rayStart, float* height) const
{
	Assert (height != NULL);
	*height = rayStart.y;

	if (!m_HeightMaps)
		return false;

	ITerrainManager* terrain = GetITerrainManager ();
	if (terrain == NULL)
		return false;

	bool hit = false;
	const float upperBound = rayStart.y;
	float lowerBound = -std::numeric_limits<float>::infinity ();


	for (int i = 0; i < m_HeightMaps->size (); ++i)
	{
		const HeightmapData& heightmapData = (*m_HeightMaps)[i];
		float terrainHeight;

		// TODO: this should be cleaned up. Abstracting the Terrain-manager away only to feed it
		// (serialized!) PPtr's to terrain data is a half-assed abstraction at best.
		Object* terrainData = InstanceIDToObjectThreadSafe (heightmapData.terrainData.GetInstanceID ());
		bool hitTerrain = terrain->GetInterpolatedHeight (terrainData, heightmapData.position, rayStart, terrainHeight);
		if (!hitTerrain)
			continue;

		if (terrainHeight < upperBound && terrainHeight > lowerBound)
		{
			(*height) = terrainHeight;
			lowerBound = terrainHeight;
			hit = true;
		}
	}
	return hit;
}
