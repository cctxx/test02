#ifndef EDITOR_HEIGHT_MESH_BUILDER
#define EDITOR_HEIGHT_MESH_BUILDER

#include "HMPoly.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "HeightMesh.h"

struct rcPolyMesh;
struct rcPolyMeshDetail;


class HeightMeshBuilder
{
public:
	// Structure to represent Height Mesh bake sources.
	struct HeightMeshBakeSource
	{
		~HeightMeshBakeSource (){}

		HMPoly poly;
		dynamic_array<bool>			triWalkableFlags;	// Flag a triangle as a walkable or not.
		dynamic_array<int>			triPolyHit;			// If intersecting with detailed mesh triangles, will be set the to polygon index.
		MinMaxAABB					aabb;				// AABB encapsulating the entire bake source.
	};
	typedef std::vector<HeightMeshBakeSource> HeightMeshBakeSources;

	HeightMeshBuilder ();
	~HeightMeshBuilder ();

	// Initialization. Following steps should be called in order.
	void Config (
		float voxelHeight,
		float voxelSampling);

	void AddBakeSource (
		const dynamic_array<Vector3f>& vertices,
		const dynamic_array<int>& triIndices,
		MinMaxAABB& aabb);

	void AddDetailMesh (const rcPolyMeshDetail& polyMeshDetail);

	// Simplify the geometry. Must be called when all bake sources have been added.
	void SimplifyBakeSources ();

	// Compute the height mesh. Config must have been called,
	// and bake sources and detail meshes added.
	void ComputeTile ();

	// Must be called when all the tiles had been computed.
	void Complete ();

	// Fill the detail mesh with the current tile height mesh data.
	bool FillDetailMesh (const rcPolyMesh& polyMesh, rcPolyMeshDetail& tempDetailMesh);

	// Sample the height at the input position
	bool GetMeshHeight (int tileID, const float* position, float maxSampleDistance, float* height) const;

private:
	void AddSourceToPolyDetail (const HeightMeshBakeSource& rSource, HMPoly& hdp);
	void BeginTile ();
	void MarkNonWalkableGeom ();

	HeightMeshBakeSources m_heightBakeSources;
	HeightMesh m_heightMesh;
	HMPolys* m_detailPolys;
	HMPolys* m_heightMeshPolys;

	// Configuration.
	float m_voxelizationError;

	// Debug.
	unsigned int m_bakeSourceTriangleCount;
	unsigned int m_bakeSourceVertexCount;
};

#endif // EDITOR_HEIGHT_MESH_BUILDER
