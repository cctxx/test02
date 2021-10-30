#pragma once

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/NavMesh/NavMeshTypes.h"
#include "Runtime/Geometry/AABB.h"

class NavMeshObstacle;
class NavMesh;

class NavMeshCarving
{
	enum TileCarveStatus
	{
		kIgnore = 0,
		kRestore = 1,
		kCarve = 2
	};

	struct ObstacleCarveInfo
	{
		NavMeshCarveData carveData;
		MinMaxAABB carveBounds;
		NavMeshObstacle* obstacle;
	};

public:
	NavMeshCarving ();
	~NavMeshCarving ();


	void AddObstacle (NavMeshObstacle& obstacle, int& handle);
	void RemoveObstacle (int& handle);
	bool Carve ();

private:

	void UpdateCarveData (dynamic_array<NavMeshCarveData>& newCarveData);
	bool UpdateTiles (NavMesh* navmesh, const dynamic_array<NavMeshCarveData>& newCarveData);

	TileCarveStatus CollectCarveDataAndStatus (dynamic_array<Matrix4x4f>& transforms, dynamic_array<Vector3f>& sizes, dynamic_array<MinMaxAABB>& aabbs, const dynamic_array<NavMeshCarveData>& newCarveData, const MinMaxAABB& tileBounds) const;
	void CollectOverlappingCarveData (dynamic_array<Matrix4x4f>& transforms, dynamic_array<Vector3f>& sizes, dynamic_array<MinMaxAABB>& aabbs, const MinMaxAABB& bounds) const;

	dynamic_array<ObstacleCarveInfo> m_ObstacleInfo;
	dynamic_array<MinMaxAABB> m_OldCarveBounds;
};
