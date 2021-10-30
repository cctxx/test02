#pragma once

#include "Runtime/Geometry/AABB.h"
#include "Runtime/NavMesh/NavMesh.h"
#include "Runtime/NavMesh/HeightmapData.h"
#include "Editor/Src/SceneBackgroundTask.h"
#include "Editor/Src/NavMesh/HeightMeshBuilder.h"
#include "Runtime/NavMesh/NavMeshSettings.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Recast.h"
#include "DetourNavMeshBuilder.h"
#include "Runtime/Math/Vector3.h"
#include "DetourNavMeshQuery.h"

class MeshRenderer;
class NavMesh;
class Mesh;
struct TerrainBakeInfo;
class Matrix4x4f;
class TerrainData;
class HeightMeshQuery;

class NavMeshBuilder : SceneBackgroundTask
{
	struct AutoLinkPoints
	{
		Vector3f start;
		Vector3f end;
	};
	struct EdgeSegment
	{
		Vector3f start;
		Vector3f end;
		Vector3f normal;
	};
	struct TriangleData
	{
		dynamic_array<Vector3f> vertices;
		dynamic_array<int> triangles;
	};
	struct MeshBakeDescription
	{
		enum { kInvalid = 0, kGeometry, kTerrain, kTree };

		Matrix4x4f transform;
		MinMaxAABB bounds;
		const Mesh* mesh;

		UInt8 areaTag;
		UInt8 autoOML;
		UInt8 reverseWinding;
		UInt8 sourceType;
	};
	typedef std::list<MeshBakeDescription> MeshBakeDescriptions;
	MeshBakeDescriptions m_BakeDescriptions;

	NavMeshBuildSettings           m_Settings;
	rcConfig                       m_Config;
	dtNavMesh                      m_dtNavMesh;
	dtNavMeshQuery                 m_NavMeshQuery;

	std::string                    m_AsyncNavMeshErrorMsg;

	const char*                    m_ProgressStatus;
	UInt8*                         m_OutputData;
	int                            m_OutputSize;
	float                          m_Progress;
	Thread                         m_Thread;
	bool						   m_buildHeightMesh;

	// HeightMesh
	HeightMeshBuilder						m_HeightMeshBuilder;
	HeightmapDataVector		m_Heightmaps;

	// Auto-generated link data
	dynamic_array <AutoLinkPoints> m_AutoLinkPoints;
	dynamic_array <EdgeSegment> m_AutoLinkEdgeSegments;

	// Off-Mesh connections.
	dynamic_array <dtOffMeshCreateParams> m_OffMeshParams;

	void AddOffMeshConnection (const Vector3f& start, const Vector3f& end, const float radius,
		bool bidirectional, unsigned char area, OffMeshLinkType linkType);

	virtual float       GetProgress ()             { return m_Progress; }
	virtual std::string GetStatusText ()           { const char* tempStatus = m_ProgressStatus; return tempStatus; }
	virtual std::string GetDialogTitle ()    { return "Building NavMesh"; }
	virtual void        BackgroundStatusChanged ();
	virtual bool        IsFinished ();
	virtual bool        Complete ();
	virtual SceneBackgroundTaskType GetTaskType () { return kNavMeshBuildingTask; }

	static void* AsyncCompute (void* data);

	std::string ComputeTiles ();

	std::string ComputeRecastTile (int ix, int iz, const int tileSize, const Vector3f& worldMin, const Vector3f& worldMax, class RecastTiles& recastTiles);
	void CreateDetourNavmesh (int ix, int iz, RecastTiles& recastTiles, bool addOffMeshLinks=false);
	bool KeepHeightField () const;
	void AppendOffMeshLinks (int ix, int iz, RecastTiles& recastTiles);

	std::string BuildTileMesh (const Vector3f& bMin, const Vector3f& bMax, struct RecastTile& recastTile, bool updateProgress=false);

	void PrepareHeightMesh ();
	void ComputeHeightMeshTile (const rcPolyMeshDetail& polyMeshDetail);

	void PlaceDropDownLinks (int ix, int iz, const class RecastTiles& tiles);
	void FindValidDropDowns (const class RecastTiles& tiles);
	bool DropDownBlocked (Vector3f startPos, Vector3f endPos, float cs, const class RecastTiles& tiles);

	void PlaceJumpAcrossLinks (int ix, int iz, const class RecastTiles& tiles);
	void FindValidJumpAcrossLinks (const class RecastTiles& tiles);
	bool JumpAcrossBlocked (Vector3f startPos, Vector3f endPos, float cs, const class RecastTiles& tiles);

	void GetMeshEdges (int ix, int iz, const class RecastTiles& recastTiles);
	float VerticalNavMeshTest (Vector3f testFrom, float testHeight);
	void GetSubsampledLocations (Vector3f segmentStart, Vector3f segmentEnd, float subsampleDistance, dynamic_array<Vector3f>& locations);

	void GetTerrainNavMeshLayerAndFlag (const TerrainData& terrainData, UInt32& layer, unsigned char& flags);
	void AddTrees (TerrainData& terrainData, const Vector3f& terrainPosition, MinMaxAABB& bounds);

	void ProjectVertices ();
	void SetVertexHeight (int tileID, const HeightMeshQuery& hmq, float sampleDistance, float* vertex);

	void AddMeshBakeDescription (const Mesh* mesh, UInt8 areaTag, UInt8 autoOML, const Matrix4x4f transform, bool reverseWinding, UInt8 sourceType);
	MinMaxAABB CalculateMeshBounds (const Mesh& mesh, const Matrix4x4f& transform) const;
	NavMeshBuilder::TriangleData CalculateTriangleData (const MeshBakeDescription& desc) const;
	void RasterizeMesh (const MeshBakeDescription& desc, const rcConfig& config, rcContext& context, rcHeightfield& heightfield) const;
	int EstimateTileCount (int tileCountX, int tileCountZ, float tileSize, const Vector3f& worldMin) const;

public:

	NavMeshBuilder ();
	~NavMeshBuilder ();

	void Prepare (const NavMeshBuildSettings& settings, MeshRenderer** renderers, int rendererSize, TerrainBakeInfo* terrains, int terrainsSize);
	void ClearStaticOffMeshLinkPolyRefs ();

	static void BuildNavMesh (const NavMeshBuildSettings& settings);
	static void BuildNavMeshAsync (const NavMeshBuildSettings& settings);
	static void ClearAllNavMeshes ();

	static bool IsBuildingNavMeshAsync () { return IsTaskRunning (kNavMeshBuildingTask); }
	static void Cancel ()                 { CleanupSingletonBackgroundTask (kNavMeshBuildingTask); }
};
