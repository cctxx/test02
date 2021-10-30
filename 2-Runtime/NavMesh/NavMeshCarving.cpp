#include "UnityPrefix.h"
#include "NavMeshCarving.h"

#include "DetourNavMesh.h"
#include "NavMesh.h"
#include "NavMeshSettings.h"
#include "DetourCrowdTypes.h"
#include "NavMeshObstacle.h"
#include "NavMeshProfiler.h"
#include <float.h>

// Performance note:
// The performance of the current implementation will be sub-optimal
// in case of many tiles compared to update count.
// [ e.g. : tileCount >= 10*(newCarveData.size () + m_OldCarveBounds.size ()) ]
//
// Consider letting obstacles push themselves to lists of tiles which they cover.

PROFILER_INFORMATION (gCrowdManagerCarve, "CrowdManager.CarveNavmesh", kProfilerAI)


NavMeshCarving::NavMeshCarving ()
{
}

NavMeshCarving::~NavMeshCarving ()
{
}

#if ENABLE_NAVMESH_CARVING

#include "Runtime/Geometry/AABB.h"
#include "Runtime/Geometry/Intersection.h"
#include "NavMeshTileCarving.h"

static void CalculateCarveBounds (MinMaxAABB& carveBounds, const NavMeshCarveData& carveData)
{
	AABB localCarveBounds (Vector3f::zero, carveData.size);
	AABB worldCarveBounds;
	TransformAABBSlow (localCarveBounds, carveData.transform, worldCarveBounds);
	carveBounds = worldCarveBounds;
}

void NavMeshCarving::AddObstacle (NavMeshObstacle& obstacle, int& handle)
{
	Assert (handle == -1);
	handle = m_ObstacleInfo.size ();
	ObstacleCarveInfo& info = m_ObstacleInfo.push_back ();
	info.obstacle = &obstacle;
	memset (&info.carveData, 0, sizeof (info.carveData));
}

void NavMeshCarving::RemoveObstacle (int& handle)
{
	Assert (handle >= 0 && handle < m_ObstacleInfo.size ());
	const int last = m_ObstacleInfo.size () - 1;
	m_OldCarveBounds.push_back (m_ObstacleInfo[handle].carveBounds);
	if (handle != last)
	{
		m_ObstacleInfo[handle] = m_ObstacleInfo[last];
		m_ObstacleInfo[handle].obstacle->SetCarveHandle (handle);
	}
	handle = -1;
	m_ObstacleInfo.pop_back ();
}

bool NavMeshCarving::Carve ()
{
	PROFILER_AUTO (gCrowdManagerCarve, NULL)

	NavMesh* navmesh = GetNavMeshSettings ().GetNavMesh ();
	if (navmesh == NULL)
		return false;

	// Temporary copy of new data for faster culling
	dynamic_array<NavMeshCarveData> newCarveData (m_ObstacleInfo.size (), kMemTempAlloc);
	UpdateCarveData (newCarveData);

	if (newCarveData.empty () && m_OldCarveBounds.empty ())
		return false;

	return UpdateTiles (navmesh, newCarveData);
}

// For registered obstacles - collect info for those that need updating
void NavMeshCarving::UpdateCarveData (dynamic_array<NavMeshCarveData>& newCarveData)
{
	newCarveData.resize_uninitialized (0);

	const size_t obstacleCount = m_ObstacleInfo.size ();
	for (size_t i = 0; i < obstacleCount; ++i)
	{
		if (!m_ObstacleInfo[i].obstacle->NeedsRebuild ())
			continue;

		// Store previous carved data
		m_OldCarveBounds.push_back (m_ObstacleInfo[i].carveBounds);
		NavMeshCarveData& data = newCarveData.push_back ();
		m_ObstacleInfo[i].obstacle->WillRebuildNavmesh (data);
		m_ObstacleInfo[i].carveData = data;
		CalculateCarveBounds (m_ObstacleInfo[i].carveBounds, data);
	}
}

// Extend the tile bounds by the carving dimensions
// note that the asymmetry in the vertical direction.
static void CalculateExtendedTileBounds (MinMaxAABB& bounds, const dtMeshTile* tile)
{
	const Vector3f tileMin = Vector3f (tile->header->bmin);
	const Vector3f tileMax = Vector3f (tile->header->bmax);
	const float horizontalMargin = tile->header->walkableRadius;
	const float depthMargin = tile->header->walkableRadius;

	bounds.m_Min = Vector3f (tileMin.x - horizontalMargin, tileMin.y, tileMin.z - horizontalMargin);
	bounds.m_Max = Vector3f (tileMax.x + horizontalMargin, tileMax.y + depthMargin, tileMax.z + horizontalMargin);
}

bool NavMeshCarving::UpdateTiles (NavMesh* navmesh, const dynamic_array<NavMeshCarveData>& newCarveData)
{
	dtNavMesh* detourNavMesh = navmesh->GetInternalNavMesh ();
	const size_t tileCount = detourNavMesh->tileCount ();
	const size_t obstacleCount = m_ObstacleInfo.size ();

	dynamic_array<Vector3f> sizes (obstacleCount, kMemTempAlloc);
	dynamic_array<Matrix4x4f> transforms (obstacleCount, kMemTempAlloc);
	dynamic_array<MinMaxAABB> aabbs (obstacleCount, kMemTempAlloc);

	int updatedTileCount = 0;
	for (size_t i = 0; i < tileCount; ++i)
	{
		const dtMeshTile* tile = detourNavMesh->getTile (i);
		if (!tile || !tile->header)
			continue;

		MinMaxAABB tileBounds;
		CalculateExtendedTileBounds (tileBounds, tile);

		const TileCarveStatus status = CollectCarveDataAndStatus (transforms, sizes, aabbs, newCarveData, tileBounds);
		DebugAssert (transforms.size () == aabbs.size ());
		DebugAssert (transforms.size () == sizes.size ());
		if (status == kIgnore)
			continue;

		// Reinitialize tile since we have either 'kRestore' or 'kCarve' at this point
		updatedTileCount++;
		detourNavMesh->restoreTile (navmesh->GetMeshData (), navmesh->GetMeshDataSize (), i);

		if (status == kCarve)
		{
			CarveNavMeshTile (tile, detourNavMesh, transforms.size (), transforms.begin (), sizes.begin (), aabbs.begin ());
		}
	}

	m_OldCarveBounds.resize_uninitialized (0);

	return updatedTileCount > 0;
}

// Does any of the bounds in 'arrayOfBounds' overlap with 'bounds'
static bool AnyOverlaps (const dynamic_array<MinMaxAABB>& arrayOfBounds, const MinMaxAABB& bounds)
{
	const size_t count = arrayOfBounds.size ();
	for (size_t i = 0; i < count; ++i)
	{
		if (IntersectAABBAABB (arrayOfBounds[i], bounds))
			return true;
	}
	return false;
}

NavMeshCarving::TileCarveStatus NavMeshCarving::CollectCarveDataAndStatus (dynamic_array<Matrix4x4f>& transforms, dynamic_array<Vector3f>& sizes, dynamic_array<MinMaxAABB>& aabbs, const dynamic_array<NavMeshCarveData>& newCarveData, const MinMaxAABB& tileBounds) const
{
	CollectOverlappingCarveData (transforms, sizes, aabbs, tileBounds);
	if (!transforms.empty ())
		return kCarve;

	if (AnyOverlaps (m_OldCarveBounds, tileBounds))
		return kRestore;

	return kIgnore;
}

void NavMeshCarving::CollectOverlappingCarveData (dynamic_array<Matrix4x4f>& transforms, dynamic_array<Vector3f>& sizes, dynamic_array<MinMaxAABB>& aabbs, const MinMaxAABB& bounds) const
{
	aabbs.resize_uninitialized (0);
	sizes.resize_uninitialized (0);
	transforms.resize_uninitialized (0);

	const size_t count = m_ObstacleInfo.size ();
	for (size_t i = 0; i < count; ++i)
	{
		if (IntersectAABBAABB (m_ObstacleInfo[i].carveBounds, bounds))
		{
			aabbs.push_back (m_ObstacleInfo[i].carveBounds);
			sizes.push_back (m_ObstacleInfo[i].carveData.size);
			transforms.push_back (m_ObstacleInfo[i].carveData.transform);
		}
	}
}

#else
bool NavMeshCarving::Carve () {return false;}
void NavMeshCarving::AddObstacle (NavMeshObstacle& obstacle, int& handle) {}
void NavMeshCarving::RemoveObstacle (int& handle) {}

#endif // ENABLE_NAVMESH_CARVING
