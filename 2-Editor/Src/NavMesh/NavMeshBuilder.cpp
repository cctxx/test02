#include "UnityPrefix.h"
#include "NavMeshBuilder.h"
#include "HeightMesh.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Recast.h"
#include "DetourCommon.h"
#include "DetourAlloc.h"
#include "DetourAssert.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "Runtime/NavMesh/NavMesh.h"
#include "Runtime/NavMesh/OffMeshLink.h"
#include "Editor/Src/ExtractTerrainMesh.h"
#include "Editor/Src/LicenseInfo.h"
#include "Runtime/Terrain/TerrainRenderer.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/NavMesh/NavMeshLayers.h"
#include "Runtime/NavMesh/HeightMeshQuery.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/Application.h"


using namespace	std;

static const char* kDefaultNavMeshAssetName = "NavMesh.asset";
static const bool NAVMESH_PROJECT_VERTICES = false;

// Store recast data about one tile. It doesn't own the data.
struct RecastTile
{
	RecastTile ()
		: m_heightField (NULL)
		, m_polyMesh (NULL)
		, m_detailMesh (NULL)
	{}

	void Free ()
	{
		rcFreeHeightField (m_heightField);
		m_heightField = NULL;

		rcFreePolyMesh (m_polyMesh);
		m_polyMesh = NULL;

		rcFreePolyMeshDetail (m_detailMesh);
		m_detailMesh = NULL;

		m_polygonFlags.clear ();
	}

	void FreeHeightField ()
	{
		rcFreeHeightField (m_heightField);
		m_heightField = NULL;
	}

	rcHeightfield* m_heightField;
	rcPolyMesh* m_polyMesh;
	rcPolyMeshDetail* m_detailMesh;
	std::vector<unsigned char> m_polygonFlags;
};

// Store recast data for all the tiles. It own the data.
class RecastTiles
{
public:

	RecastTiles ()
	: m_recastTiles (NULL)
	, m_tileCountX (0)
	, m_tileCountZ (0)
	, m_tileWidth (0)
	, m_tileHeight (0)
	{}

	~RecastTiles ()
	{
		if (m_recastTiles)
		{
			for (int i = 0; i < m_tileCountX*m_tileCountZ; ++i)
			{
				m_recastTiles[i].Free ();
			}
			delete[] m_recastTiles;
		}
	}

	void Init (int tileCountZ, int tileCountX, float tileWidth, float tileHeight, const float* orig)
	{
		m_tileCountZ = tileCountZ;
		m_tileCountX = tileCountX;
		m_tileWidth = tileWidth;
		m_tileHeight = tileHeight;
		rcVcopy (m_orig, orig);
		m_recastTiles = new RecastTile[tileCountZ*tileCountX];
		memset (m_recastTiles, 0, sizeof (RecastTile)*tileCountZ*tileCountX);
	}

	bool CalcTileLoc (float posX, float posZ, int* tx, int* tz) const
	{
		*tx = (int)floorf ((posX-m_orig[0]) / m_tileWidth);
		*tz = (int)floorf ((posZ-m_orig[2]) / m_tileHeight);

		return *tx >=0 && *tx < m_tileCountX && *tz >=0 && *tz < m_tileCountZ;
	}

	bool InRange (int ix, int iz) const
	{
		return (ix>=0 && iz>=0 && ix<m_tileCountX && iz<m_tileCountZ);
	}

	void ClearTile (int ix, int iz)
	{
		if (!InRange (ix, iz)) return;
		m_recastTiles[iz*m_tileCountX+ix].Free ();
	}

	void FreeHeightField (int ix, int iz)
	{
		if (!InRange (ix, iz)) return;
		m_recastTiles[iz*m_tileCountX+ix].FreeHeightField ();
	}

	void SetTile (int ix, int iz, const RecastTile& recastTile)
	{
		m_recastTiles[iz*m_tileCountX+ix] = recastTile;
	}

	RecastTile& GetTile (int ix, int iz) const
	{
		dtAssert (InRange (ix, iz));
		return m_recastTiles[iz*m_tileCountX+ix];
	}

	bool CheckHeightfieldCollision (const float x, const float ymin, const float ymax, const float z) const
	{
		int tx, tz;
		if (CalcTileLoc (x, z, &tx, &tz))
		{
			rcHeightfield* heightField = m_recastTiles[tz*m_tileCountX+tx].m_heightField;
			if (heightField)
			{
				return rcCheckHeightfieldCollision (x, ymin, ymax, z, *heightField);
			}
		}
		return false;
	}

private:
	RecastTile* m_recastTiles;
	int m_tileCountX, m_tileCountZ, m_tileWidth, m_tileHeight;
	float m_orig[3];
};

static void ValidateBuildSettings (NavMeshBuildSettings& settings)
{
	// Recast assumes that walkableClimb is less than walkableHeight when neighbour cell access is calculated,
	// force this constraint. A warning is shown in the UI when this constraint is broken.
	if (settings.agentClimb >= settings.agentHeight)
		settings.agentClimb = settings.agentHeight - FLT_EPSILON;
}

static void ConfigureConfig (const NavMeshBuildSettings& settings, const MinMaxAABB& bounds, rcConfig& config)
{
	memset (&config, 0, sizeof (config));
	config.cs = (2.0f * settings.agentRadius * 0.01f*settings.widthInaccuracy);
	config.ch = (settings.agentHeight * 0.01f*settings.heightInaccuracy);

	config.walkableSlopeAngle = dtMax (settings.agentSlope, 0.1f);
	config.walkableHeight = (int)floorf (settings.agentHeight / config.ch);	///@TODO: Changed from ceilf. Ask mikko if this is any good???
	config.walkableClimb = (int)ceilf (settings.agentClimb / config.ch);
	config.walkableRadius = (int)ceilf (settings.agentRadius / config.cs);
	config.maxEdgeLen = 0;
	config.maxSimplificationError = 1.3f;

	// Recast assumes that walkableClimb is less than walkableHeight when neighbour cell access is calculated.
	// Step height is clamped to agent height on validation, but it might get equal or larger than agent height because or rounding.
	if (config.walkableClimb >= config.walkableHeight)
		config.walkableClimb = config.walkableHeight - 1;

	config.minRegionArea = (int)(settings.minRegionArea / (config.cs*config.cs));

	rcVcopy (config.bmin, bounds.GetMin ().GetPtr ());
	rcVcopy (config.bmax, bounds.GetMax ().GetPtr ());

	int worldTileSize = (int) (dtMax (config.bmax[0] - config.bmin[0], config.bmax[2] - config.bmin[2]) / config.cs);
	config.tileSize =  (worldTileSize < 100) ? 100 : (worldTileSize>1000) ? 1000 : worldTileSize;
	config.mergeRegionArea = 400;
	config.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
	config.detailSampleDist = config.cs * 6.0f;
	config.detailSampleMaxError = config.ch * 1.0f;

	rcCalcGridSize (config.bmin, config.bmax, config.cs, &config.width, &config.height);
}

void NavMeshBuilder::RasterizeMesh (const MeshBakeDescription& desc, const rcConfig& config, rcContext& context, rcHeightfield& heightfield) const
{
	if (m_Config.bmin[0] > desc.bounds.m_Max[0]
	||  m_Config.bmin[2] > desc.bounds.m_Max[2]
	||  m_Config.bmax[0] < desc.bounds.m_Min[0]
	||  m_Config.bmax[2] < desc.bounds.m_Min[2])
		return;

	const TriangleData triData = CalculateTriangleData (desc);

	// ComputeNavMesh triangle area
	int triangleCount = triData.triangles.size () / 3;
	dynamic_array<unsigned char> triAreas;

	// Not walkable area has to be tagged specifically, since it will be filtered later
	if (desc.areaTag == NavMeshLayers::kNotWalkable)
	{
		triAreas.resize_initialized (triangleCount, RC_FORCE_UNWALKABLE_AREA);
	}
	else
	{
		triAreas.resize_initialized (triangleCount, 0);
		rcMarkWalkableTriangles (&context, config.walkableSlopeAngle, triData.vertices[0].GetPtr (), triData.vertices.size (), &triData.triangles[0], triangleCount, &triAreas[0]);

		// during voxelization we use RC_WALKABLE_AREA + type. After voxelization we transform it back into areaType bits.
		for (size_t i = 0; i < triAreas.size (); ++i)
		{
			if (triAreas[i] != RC_WALKABLE_AREA)
				continue;

			// Compact area type and auto generate flag.
			triAreas[i] = RC_WALKABLE_AREA + (2*desc.areaTag + desc.autoOML);
			dtAssert (triAreas[i] < RC_FORCE_UNWALKABLE_AREA);
		}
	}

	// Rasterize into the heightfield
	rcRasterizeTriangles (&context, triData.vertices[0].GetPtr (), triData.vertices.size (), &triData.triangles[0], &triAreas[0], triangleCount, heightfield, config.walkableClimb);
}

int NavMeshBuilder::EstimateTileCount (int tileCountX, int tileCountZ, float tileSize, const Vector3f& worldMin) const
{
	std::vector<bool> marked (tileCountX*tileCountZ, false);

	int count = 0;
	for (MeshBakeDescriptions::const_iterator i=m_BakeDescriptions.begin (), end = m_BakeDescriptions.end (); i != end ; ++i)
	{
		const MinMaxAABB& bounds = i->bounds;
		int minX = dtClamp ((int)floorf ((bounds.m_Min.x - worldMin.x) / tileSize), 0, tileCountX-1);
		int minZ = dtClamp ((int)floorf ((bounds.m_Min.z - worldMin.z) / tileSize), 0, tileCountX-1);
		int maxX = dtClamp ((int)floorf ((bounds.m_Max.x - worldMin.x) / tileSize), 0, tileCountZ-1);
		int maxZ = dtClamp ((int)floorf ((bounds.m_Max.z - worldMin.z) / tileSize), 0, tileCountZ-1);

		// Mark overlapped tiles
		for (int ix = minX; ix <= maxX; ++ix)
		{
			int k = ix*tileCountZ;
			for (int iz = minZ; iz <= maxZ; ++iz)
			{
				if (marked [k+iz])
					continue;
				marked[k+iz] = true;
				++count;
			}
		}
	}
	return count;
}

#define PROGRESS_BAR_AND_CANCEL(progress,status) { if (m_Thread.IsRunning () && m_Thread.IsQuitSignaled ()) return "Cancelled"; m_Progress = (progress); m_ProgressStatus = status;  }

std::string NavMeshBuilder::ComputeTiles ()
{
	Assert (m_Config.tileSize > 0);

	// Prepare the height mesh builder. Simplify geometry.
	if (m_buildHeightMesh)
	{
		PROGRESS_BAR_AND_CANCEL (0.0f, "Simplifying geometry.");
		m_HeightMeshBuilder.SimplifyBakeSources ();
	}

	Vector3f worldMin (m_Config.bmin);
	Vector3f worldMax (m_Config.bmax);
	const int cw = m_Config.width;
	const int ch = m_Config.height;

	// Patch config with tile settings
	m_Config.borderSize = m_Config.walkableRadius + 3;
	m_Config.width  = m_Config.tileSize + 2*m_Config.borderSize;
	m_Config.height = m_Config.tileSize + 2*m_Config.borderSize;
	const int ts = m_Config.tileSize + 2*m_Config.borderSize;

	const int tileSize = m_Config.tileSize;
	const int tileCountX = (cw + ts - 1) / tileSize;
	const int tileCountZ = (ch + ts - 1) / tileSize;

	dtNavMeshParams mparams;

	rcVcopy (mparams.orig, worldMin.GetPtr ());
	mparams.tileWidth  = tileSize*m_Config.cs;
	mparams.tileHeight = tileSize*m_Config.cs;
	mparams.maxTiles = 1<<10;
	mparams.maxPolys = 1<<12;
	m_dtNavMesh.init (&mparams, mparams.maxTiles);
	m_NavMeshQuery.initPools (&m_dtNavMesh, 0);

	RecastTiles recastTiles;
	recastTiles.Init (tileCountZ, tileCountX, mparams.tileWidth, mparams.tileHeight, worldMin.GetPtr ());

	m_Progress = 0.0f;
	const int totalTileCount = tileCountX*tileCountZ;

	if (totalTileCount == 0)
		return "";

	if (totalTileCount > mparams.maxTiles)
	{
		// Trigger a bounding box overlap test for all tiles
		// in order to obtain a better estimate the number of needed tiles.

		int estTileCount = EstimateTileCount (tileCountX, tileCountZ, tileSize*m_Config.cs, worldMin);
		if (estTileCount > mparams.maxTiles)
		{
			char buf[256];
			const char* errStr = "NavMesh estimated tile count: %i exceeds maximum limit of: %i";
			snprintf (buf, sizeof (buf), errStr, estTileCount, mparams.maxTiles);
			return buf;
		}
	}

	// Sweep a 3 element kernel such that every tile gets processed once by each of:
	// ComputeRecastTile, AppendOffMeshLinks, ClearTile.

	// Make inner loop the shortest
	// to free up tile-data earlier.
	if (tileCountZ >= tileCountX)
	{
		for (int iz = -1; iz <= tileCountZ; ++iz)
		{
			for (int ix = -1; ix <= tileCountX; ++ix)
			{
				if (recastTiles.InRange (ix+1, iz+1))
				{
					const float progress = (ix+1 + tileCountX*(iz+1)) * 1.0f/totalTileCount;
					PROGRESS_BAR_AND_CANCEL (progress, "Exporting Tiles");

					std::string error = ComputeRecastTile (ix+1, iz+1, tileSize, worldMin, worldMax, recastTiles);
					if (!error.empty ())
						return error;
					CreateDetourNavmesh (ix+1, iz+1, recastTiles);
				}

				AppendOffMeshLinks (ix, iz, recastTiles);
				recastTiles.ClearTile (ix-1, iz-1);
			}
		}
	}
	else
	{
		for (int ix = -1; ix <= tileCountX; ++ix)
		{
			for (int iz = -1; iz <= tileCountZ; ++iz)
			{
				if (recastTiles.InRange (ix+1, iz+1))
				{
					const float progress = (iz+1 + tileCountZ*(ix+1)) * 1.0f/totalTileCount;
					PROGRESS_BAR_AND_CANCEL (progress, "Exporting Tiles");

					std::string error = ComputeRecastTile (ix+1, iz+1, tileSize, worldMin, worldMax, recastTiles);
					if (!error.empty ())
						return error;
					CreateDetourNavmesh (ix+1, iz+1, recastTiles);
				}

				AppendOffMeshLinks (ix, iz, recastTiles);
				recastTiles.ClearTile (ix-1, iz-1);
			}
		}
	}
	ProjectVertices ();
	PROGRESS_BAR_AND_CANCEL (0.99F , "Storing tiles");

	// initialize mesh set header.
	dtNavMeshSetHeader setHeader;
	setHeader.magic = DT_NAVMESH_SET_MAGIC;
	setHeader.version = DT_NAVMESH_SET_VERSION;
	setHeader.numTiles = 0;
	memcpy (&setHeader.params, &mparams, sizeof (dtNavMeshParams));

	// count tiles and data size
	int outputSize = sizeof (setHeader);

	for (int i = 0; i < m_dtNavMesh.getMaxTiles (); ++i)
	{
		const dtMeshTile* tile = m_dtNavMesh.getTile (i);
		if (!tile || !tile->header || !tile->dataSize) continue;

		setHeader.numTiles++;
		outputSize += sizeof (dtNavMeshTileHeader) + tile->dataSize;
	}

	// alloc output data
	if (m_OutputData != NULL)
		dtFree (m_OutputData);

	m_OutputData = dtAllocArray<UInt8> (outputSize);
	if (m_OutputData == NULL)
		return "Out of memory when storing navmesh tile set";

	int offset = 0;
	// write
	memcpy (m_OutputData, &setHeader, sizeof (setHeader));
	offset += sizeof (setHeader);

	// Store tiles.
	unsigned int totalStoredTileCount = 0;
	unsigned int totalStoredPolyCount = 0;
	unsigned int totalStoredVertCount = 0;

	for (int i = 0; i < m_dtNavMesh.getMaxTiles (); ++i)
	{
		const dtMeshTile* tile = m_dtNavMesh.getTile (i);
		if (!tile || !tile->header || !tile->dataSize) continue;

		dtNavMeshTileHeader tileHeader;
		tileHeader.tileRef = m_dtNavMesh.getTileRef (tile);
		tileHeader.dataSize = tile->dataSize;

		memcpy (m_OutputData + offset, &tileHeader, sizeof (tileHeader));
		offset += sizeof (tileHeader);

		memcpy (m_OutputData + offset, tile->data, tile->dataSize);
		offset += tile->dataSize;

		totalStoredTileCount += 1;
		totalStoredPolyCount += tile->header->polyCount;
		totalStoredVertCount += tile->header->vertCount;
	}

	Assert (offset == outputSize);
	m_OutputSize = offset;

	if (m_buildHeightMesh)
	{
		m_HeightMeshBuilder.Complete ();
	}

	return "";
}
#undef PROGRESS_BAR_AND_CANCEL

struct BuildTileTempMemory
{
	BuildTileTempMemory () : chf (0), cset (0) {};
	~BuildTileTempMemory ()
	{
		if (cset)rcFreeContourSet (cset);
		if (chf)rcFreeCompactHeightfield (chf);
	}

	rcCompactHeightfield* chf;
	rcContourSet* cset;
};

#define PROGRESS_BAR_AND_CANCEL_CONDITION(condition, progress,status) { if (m_Thread.IsRunning () && m_Thread.IsQuitSignaled ()) return "Cancelled"; if (condition){m_Progress = (progress); m_ProgressStatus = status;}  }

std::string NavMeshBuilder::BuildTileMesh (const Vector3f& bMin, const Vector3f& bMax, RecastTile& recastTile, bool updateProgress)
{
	BuildTileTempMemory tempMemory;

	// Patch config for this tile
	rcVcopy (m_Config.bmin, bMin.GetPtr ());
	rcVcopy (m_Config.bmax, bMax.GetPtr ());
	const float borderPatch = m_Config.borderSize*m_Config.cs;
	m_Config.bmin[0] -= borderPatch;
	m_Config.bmin[2] -= borderPatch;
	m_Config.bmax[0] += borderPatch;
	m_Config.bmax[2] += borderPatch;

	rcContext context;

	//
	//	Step 1. create height field
	//
	recastTile.m_polyMesh = rcAllocPolyMesh ();
	if (!recastTile.m_polyMesh)
		return "buildNavigation: Out of memory 'poly mesh'.";

	recastTile.m_detailMesh = rcAllocPolyMeshDetail ();
	if (!recastTile.m_detailMesh)
		return "buildNavigation: Out of memory 'mesh detail'.";

	recastTile.m_heightField = rcAllocHeightfield ();
	if (!recastTile.m_heightField)
		return "buildNavigation: Out of memory 'height field'.";

	if (!rcCreateHeightfield (&context, *recastTile.m_heightField, m_Config.width, m_Config.height, m_Config.bmin, m_Config.bmax, m_Config.cs, m_Config.ch))
		return "buildNavigation: Could not create solid heightfield.";

	//
	// Step 2. Rasterize all geometry
	//
	size_t meshCount = m_BakeDescriptions.size ();
	size_t c = 0;
	for (MeshBakeDescriptions::iterator i=m_BakeDescriptions.begin (), end = m_BakeDescriptions.end (); i != end; ++i)
	{
		PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.125F * (c / meshCount), "Rasterizing Geometry");
		RasterizeMesh (*i, m_Config, context, *recastTile.m_heightField);
		++c;
	}

	//
	// Step 3. Filter walkables surfaces.
	//
	PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.3F, "Filtering Navmesh");

	// Once all geoemtry is rasterized, we do initial pass of filtering to
	// remove unwanted overhangs caused by the conservative rasterization
	// as well as filter spans where the character cannot possibly stand.
	rcFilterForceUnwalkableArea (&context, *recastTile.m_heightField);
	rcFilterLowHangingWalkableObstacles (&context, m_Config.walkableClimb, *recastTile.m_heightField);
	rcFilterLedgeSpans (&context, m_Config.walkableHeight, m_Config.walkableClimb, *recastTile.m_heightField);
	rcFilterWalkableLowHeightSpans (&context, m_Config.walkableHeight, *recastTile.m_heightField);

	// Skip if tile is empty.
	if (!rcGetHeightFieldSpanCount (&context, *recastTile.m_heightField))
	{
		return "";
	}

	//
	// Step 4. Partition walkable surface to simple regions.
	//
	PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.4F, "Eroding Navmesh");

	// Compact the heightfield so that it is faster to handle from now on.
	// This will result more cache coherent data as well as the neighbours
	// between walkable cells will be calculated.
	tempMemory.chf = rcAllocCompactHeightfield ();
	if (!tempMemory.chf)
		return "buildNavigation: Out of memory 'chf'.";

	if (!rcBuildCompactHeightfield (&context, m_Config.walkableHeight, m_Config.walkableClimb, *recastTile.m_heightField, *tempMemory.chf))
		return "buildNavigation: Could not build compact data.";

	// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea (&context, m_Config.walkableRadius, *tempMemory.chf))
		return "buildNavigation: Could not erode.";

	// @TODO: Solve this.
	if (1)
	{
		// Fast methods - which works well for overlapping areas.
		// has a tendency to generate elongated polygons.

		PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.5F, "Building regions");
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildRegionsMonotone (&context, *tempMemory.chf, m_Config.borderSize, m_Config.minRegionArea, m_Config.mergeRegionArea))
			return "buildNavigation: Could not build regions.";
	}
	else
	{
		// Produce better tesselation for simple areas (non-overlapping).

		PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.5F, "Building distance fields");
		// Prepare for region partitioning, by calculating distance field along the walkable surface.
		if (!rcBuildDistanceField (&context, *tempMemory.chf))
			return "buildNavigation: Could not build distance field.";

		PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.55F, "Building regions");
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildRegions (&context, *tempMemory.chf, m_Config.borderSize, m_Config.minRegionArea, m_Config.mergeRegionArea))
			return "buildNavigation: Could not build regions.";
	}

	//
	// Step 5. Trace and simplify region contours.
	//
	PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.6F, "Tracing countours");

	// Create contours.
	tempMemory.cset = rcAllocContourSet ();
	if (!tempMemory.cset)
		return "buildNavigation: Out of memory 'cset'.";

	if (!rcBuildContours (&context, *tempMemory.chf, m_Config.maxSimplificationError, m_Config.maxEdgeLen, *tempMemory.cset))
		return "buildNavigation: Could not create contours.";

	// Skip if tile is empty.
	if (!tempMemory.cset->nconts)
	{
		return "";
	}

	//
	// Step 6. Build polygons mesh from contours.
	//
	PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.7F, "Building polymesh");
	// Build polygon navmesh from the contours.
	if (!rcBuildPolyMesh (&context, *tempMemory.cset, m_Config.maxVertsPerPoly, *recastTile.m_polyMesh))
		return "buildNavigation: Could not triangulate contours.";

	rcFreeContourSet (tempMemory.cset);
	tempMemory.cset = 0;

	//
	// Step 7. Set polygon flags.
	//
	PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.8F, "Updating polygon area flags");

	// Update poly flags from areas.
	for (int i = 0; i < recastTile.m_polyMesh->npolys; ++i)
	{
		unsigned int area = recastTile.m_polyMesh->areas[i] - RC_WALKABLE_AREA;

		// Extract area type and auto generate flag.
		unsigned char autoGenerateOffMeshLinks = area & 0x1;
		area = area / 2;
		dtAssert (area < 32);

		// store flags in separate buffer
		recastTile.m_polygonFlags.push_back (autoGenerateOffMeshLinks);

		// store area in flags as walkable mask
		recastTile.m_polyMesh->flags[i] = 1 << area;
		recastTile.m_polyMesh->areas[i] = area;
	}

	//
	// Step 8. Create detail mesh which allows to access approximate height on each polygon.
	//
	PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.9F, "Building detail mesh");

	if (!rcBuildPolyMeshDetail (&context, *recastTile.m_polyMesh, *tempMemory.chf, m_Config.detailSampleDist, m_Config.detailSampleMaxError, *recastTile.m_detailMesh))
		return "buildNavigation: Could not build detail mesh.";

	rcFreeCompactHeightfield (tempMemory.chf);
	tempMemory.chf = 0;

	// Step 9. Compute HeightMesh.
	if (m_buildHeightMesh)
	{
		PROGRESS_BAR_AND_CANCEL_CONDITION (updateProgress, 0.95F, "Building HeightMesh");
		ComputeHeightMeshTile (*recastTile.m_detailMesh);

		if (m_Settings.accuratePlacement)
		{
			// Replace the detail mesh
			rcFreePolyMeshDetail (recastTile.m_detailMesh);
			recastTile.m_detailMesh = rcAllocPolyMeshDetail ();
			if (!recastTile.m_detailMesh)
				return "buildNavigation: Out of memory 'mesh detail'.";

			m_HeightMeshBuilder.FillDetailMesh (*recastTile.m_polyMesh, *recastTile.m_detailMesh);
		}
	}

	return "";
}
#undef PROGRESS_BAR_AND_CANCEL_CONDITION

bool NavMeshBuilder::KeepHeightField () const
{
	return (m_Settings.ledgeDropHeight > m_Settings.agentClimb) || (m_Settings.maxJumpAcrossDistance > 2.0f*m_Settings.agentRadius);
}

void NavMeshBuilder::AppendOffMeshLinks (int ix, int iz, RecastTiles& recastTiles)
{
	if (!recastTiles.InRange (ix, iz))
		return;

	const RecastTile& recastTile = recastTiles.GetTile (ix, iz);
	if (!recastTile.m_polyMesh || !recastTile.m_detailMesh)
		return;

	if (m_Settings.ledgeDropHeight > m_Settings.agentClimb)
	{
		dtAssert (recastTile.m_heightField);
		PlaceDropDownLinks (ix, iz, recastTiles);
	}
	if (m_Settings.maxJumpAcrossDistance > 2.0f*m_Settings.agentRadius)
	{
		dtAssert (recastTile.m_heightField);
		PlaceJumpAcrossLinks (ix, iz, recastTiles);
	}

	CreateDetourNavmesh (ix, iz, recastTiles, true);
}


void NavMeshBuilder::CreateDetourNavmesh (int ix, int iz, RecastTiles& recastTiles, bool addOffMeshLinks)
{
	const RecastTile& recastTile = recastTiles.GetTile (ix, iz);

	dtNavMeshCreateParams params;
	memset (&params, 0, sizeof (params));

	// fill params
	params.verts = recastTile.m_polyMesh->verts;
	params.vertCount = recastTile.m_polyMesh->nverts;
	params.polys = recastTile.m_polyMesh->polys;
	params.polyAreas = recastTile.m_polyMesh->areas;
	params.polyFlags = recastTile.m_polyMesh->flags;
	params.polyCount = recastTile.m_polyMesh->npolys;
	params.nvp = recastTile.m_polyMesh->nvp;
	params.detailMeshes = recastTile.m_detailMesh->meshes;
	params.detailVerts = recastTile.m_detailMesh->verts;
	params.detailVertsCount = recastTile.m_detailMesh->nverts;
	params.detailTris = recastTile.m_detailMesh->tris;
	params.detailTriCount = recastTile.m_detailMesh->ntris;
	params.offMeshParams = m_OffMeshParams.begin ();
	params.offMeshConCount = addOffMeshLinks ? m_OffMeshParams.size () : 0;

	params.flags = m_Settings.accuratePlacement ? DT_MESH_HEADER_USE_HEIGHT_MESH : 0;
	params.walkableHeight = m_Settings.agentHeight;
	params.walkableRadius = m_Settings.agentRadius;
	params.walkableClimb = m_Settings.agentClimb;
	params.tileX = ix;
	params.tileY = iz;
	params.tileLayer = 0;
	rcVcopy (params.bmin, recastTile.m_polyMesh->bmin);
	rcVcopy (params.bmax, recastTile.m_polyMesh->bmax);
	params.cs = m_Config.cs;
	params.ch = m_Config.ch;
	params.buildBvTree = true;

	unsigned char* tileData = 0;
	int tileDataSize = 0;

	if (dtCreateNavMeshTileData (&params, &tileData, &tileDataSize))
	{
		m_dtNavMesh.removeTile (m_dtNavMesh.getTileRefAt (ix,iz,0),0,0);
		dtStatus status = m_dtNavMesh.addTile (tileData, tileDataSize, DT_TILE_FREE_DATA, 0, 0);
		if (dtStatusFailed (status))
		{
			dtFree (tileData);
		}
	}
	else
	{
		recastTiles.ClearTile (ix, iz);
	}

	// If heightfield is not needed for postprocessing - free it right away.
	if (!KeepHeightField ())
		recastTiles.FreeHeightField (ix, iz);
}

static void ExtractMeshRenderers (dynamic_array<MeshRenderer*>& outputStaticRenderers)
{
	vector<MeshRenderer*> renderers;
	Object::FindObjectsOfType (&renderers);

	for (size_t i = 0; i < renderers.size (); ++i)
	{
		MeshRenderer& renderer = *renderers[i];
		if (!renderer.IsActive ())
			continue;
		if (!renderer.GetEnabled ())
			continue;
		if (!renderer.GetGameObject ().AreStaticEditorFlagsSet (kNavigationStatic))
			continue;

		Mesh* mesh = renderer.GetSharedMesh ();
		if (mesh == NULL)
			continue;

		outputStaticRenderers.push_back (&renderer);
	}
}

NavMeshBuilder::NavMeshBuilder ()
: m_OutputData (NULL)
, m_OutputSize (0)
, m_ProgressStatus ("")
, m_buildHeightMesh (false)
, m_Progress (0.0f)
{
}

NavMeshBuilder::~NavMeshBuilder ()
{
	m_Thread.WaitForExit ();

	if (m_OutputData != NULL)
		dtFree (m_OutputData);
}

void NavMeshBuilder::AddOffMeshConnection (const Vector3f& start, const Vector3f& end, const float radius, bool bidirectional,
										  unsigned char area, OffMeshLinkType linkType)
{
	dtAssert (area < 32);

	dtOffMeshCreateParams& oparams = m_OffMeshParams.push_back ();
	rcVcopy (&oparams.verts[0], start.GetPtr ());
	rcVcopy (&oparams.verts[3], end.GetPtr ());
	oparams.radius = radius;
	oparams.userID = 0;
	oparams.linkType = (unsigned short)linkType;
	oparams.area = area;
	oparams.linkDirection = bidirectional ? DT_LINK_DIRECTION_TWO_WAY : DT_LINK_DIRECTION_ONE_WAY;
}

MinMaxAABB NavMeshBuilder::CalculateMeshBounds (const Mesh& mesh, const Matrix4x4f& transform) const
{
	MinMaxAABB boundingBox;
	for (StrideIterator<Vector3f> it = mesh.GetVertexBegin (), end = mesh.GetVertexEnd (); it != end; ++it)
	{
		Vector3f transformed = transform.MultiplyPoint3 (*it);
		boundingBox.Encapsulate (transformed);
	}
	return boundingBox;
}

void NavMeshBuilder::AddMeshBakeDescription (const Mesh* mesh, UInt8 areaTag, UInt8 autoOML, const Matrix4x4f transform, bool reverseWinding, UInt8 sourceType)
{
	NavMeshBuilder::MeshBakeDescription desc;

	desc.transform = transform;
	desc.bounds   = CalculateMeshBounds (*mesh, transform);
	desc.mesh     = mesh;

	desc.areaTag  = areaTag;
	desc.autoOML  = autoOML;
	desc.reverseWinding = reverseWinding;
	desc.sourceType     = sourceType;

	m_BakeDescriptions.push_back (desc);
}

NavMeshBuilder::TriangleData NavMeshBuilder::CalculateTriangleData (const MeshBakeDescription& desc) const
{
	NavMeshBuilder::TriangleData triangleData;
	triangleData.vertices.resize_uninitialized (desc.mesh->GetVertexCount ());

	// Transform all vertices
	dynamic_array<Vector3f>::iterator outIt = triangleData.vertices.begin ();
	for (StrideIterator<Vector3f> it = desc.mesh->GetVertexBegin (), end = desc.mesh->GetVertexEnd (); it != end; ++it, ++outIt)
		*outIt = desc.transform.MultiplyPoint3 (*it);

	// Copy and potentially swap all triangle indices
	Mesh::TemporaryIndexContainer indices;
	desc.mesh->GetTriangles (indices);
	if (desc.reverseWinding)
	{
		const UInt32 triangleCount = indices.size ()/3;
		for (UInt32 i = 0; i < triangleCount; ++i)
		{
			UInt32 i0 = indices[3*i + 0];
			UInt32 i1 = indices[3*i + 1];
			indices[3*i + 0] = i1;
			indices[3*i + 1] = i0;
		}
	}
	const int* firstIndex = reinterpret_cast<const int*> (&indices[0]);
	triangleData.triangles.assign (firstIndex, firstIndex + indices.size ());
	return triangleData;
}

void NavMeshBuilder::GetTerrainNavMeshLayerAndFlag (const TerrainData& terrainData, UInt32& layer, unsigned char& flags)
{
	layer = 0;
	flags = 0;

	// Pick highest NavMeshLayer value
	vector<GameObject*> gos;
	Object::FindObjectsOfType (&gos);
	for (size_t i = 0; i < gos.size (); ++i)
	{
		GameObject* go = gos[i];
		if (terrainData.HasUser (go))
		{
			layer = std::max (go->GetNavMeshLayer (), layer);
			flags = flags || go->AreStaticEditorFlagsSet (kOffMeshLinkGeneration);
		}
	}
}

void NavMeshBuilder::Prepare (const NavMeshBuildSettings& settings, MeshRenderer** renderers, int renderersSize, TerrainBakeInfo* terrains, int terrainsSize)
{
	if (renderersSize == 0 && terrainsSize == 0)
		return;

	MinMaxAABB bounds;
	bounds.Init ();

	m_Progress = 0.0F;
	for (int i = 0; i < renderersSize; ++i)
	{
		if (renderers[i] == NULL)
			continue;

		MeshRenderer& renderer = *renderers[i];
		const Mesh* mesh = renderer.GetSharedMesh ();
		if (mesh == NULL)
			continue;

		bool reverseWinding = (renderer.GetTransformInfo ().transformType & kOddNegativeScaleTransform);
		unsigned char autoOML = renderer.GetGameObject ().AreStaticEditorFlagsSet (kOffMeshLinkGeneration);

		AddMeshBakeDescription (mesh, renderer.GetGameObject ().GetNavMeshLayer (), autoOML, renderer.GetLocalToWorldMatrix (), reverseWinding, MeshBakeDescription::kGeometry);
		bounds.Encapsulate (m_BakeDescriptions.back ().bounds);
	}

	for (int i = 0; i < terrainsSize; ++i)
	{
		if (terrains[i].terrainData == NULL)
			continue;

		TerrainData& terrainData = *terrains[i].terrainData;
		Vector3f terrainPosition = terrains[i].position;

		Matrix4x4f mtx;
		mtx.SetTranslate (terrainPosition);

		// Get meshes and base material for current terrain
		TerrainRenderer tr (0, &terrainData, terrainPosition, -1);
		std::vector<Mesh*> meshes = tr.GetMeshPatches ();

		if (meshes.empty ())
			continue;

		// Keep reference to TerrainData for further accurate placement height look-ups.
		m_Heightmaps.push_back (HeightmapData ());
		HeightmapData& hd = m_Heightmaps.back ();
		hd.terrainData = PPtr<TerrainData> (&terrainData);
		hd.position = terrainPosition;

		UInt32 terrainNavMeshLayer;
		unsigned char autoOML;
		GetTerrainNavMeshLayerAndFlag (terrainData, terrainNavMeshLayer, autoOML);

		// add all terrain patches as bake data
		for (size_t j=0; j<meshes.size (); ++j)
		{
			const Mesh* mesh = meshes[j];
			if (mesh == NULL)
				continue;

			AddMeshBakeDescription (mesh, terrainNavMeshLayer, autoOML, mtx, false, MeshBakeDescription::kTerrain);
			bounds.Encapsulate (m_BakeDescriptions.back ().bounds);
		}

		AddTrees (terrainData, terrainPosition, bounds);
	}

	m_Settings = settings;
	ValidateBuildSettings (m_Settings);
	ConfigureConfig (m_Settings, bounds, m_Config);

	m_buildHeightMesh = m_Settings.accuratePlacement || NAVMESH_PROJECT_VERTICES;
	if (m_buildHeightMesh)
	{
		PrepareHeightMesh ();
	}
}

void NavMeshBuilder::AddTrees (TerrainData& terrainData, const Vector3f& terrainPosition, MinMaxAABB& bounds)
{
	TreeDatabase& treeDatabase = terrainData.GetTreeDatabase ();
	std::vector<TreeInstance>& treeInstances = treeDatabase.GetInstances ();
	Vector3f terrainSize = terrainData.GetHeightmap ().GetSize ();

	for (std::vector<TreeInstance>::iterator it=treeInstances.begin ();it!=treeInstances.end (); ++it)
	{
		const TreeInstance& treeInstance = *it;

		Vector3f position = Scale (treeInstance.position, terrainSize) + terrainPosition;
		const TreeDatabase::Prototype& prototype = treeDatabase.GetPrototypes ()[treeInstance.index];
		const Mesh* mesh = prototype.mesh;
		if (mesh == NULL)
			continue;

		// Create the instance
		Matrix4x4f treeTransform;
		treeTransform.SetTRS (position, Quaternionf::identity (), Vector3f (treeInstance.widthScale, treeInstance.heightScale, treeInstance.widthScale));

		AddMeshBakeDescription (mesh, NavMeshLayers::kNotWalkable, 0, treeTransform, true, MeshBakeDescription::kTree);
		bounds.Encapsulate (m_BakeDescriptions.back ().bounds);
	}
}

void NavMeshBuilder::BuildNavMesh (const NavMeshBuildSettings& settings)
{
	if (!GetApplication ().EnsureSceneHasBeenSaved ("navigation"))
		return;

	dynamic_array<MeshRenderer*> renderers;
	ExtractMeshRenderers (renderers);

	dynamic_array<TerrainBakeInfo> terrains;
	ExtractStaticTerrains (kNavigationStatic, terrains);

	if (renderers.empty () && terrains.empty ())
	{
		ClearAllNavMeshes ();
		return;
	}

	NavMeshBuilder builder;
	builder.Prepare (settings, renderers.begin (), renderers.size (), terrains.begin (), terrains.size ());
	const std::string result = builder.ComputeTiles ();

	if (!result.empty ())
	{
		ErrorString (result);
		return;
	}

	builder.Complete ();
}

void* NavMeshBuilder::AsyncCompute (void* data)
{
	NavMeshBuilder* builder = reinterpret_cast<NavMeshBuilder*> (data);
	builder->m_AsyncNavMeshErrorMsg = builder->ComputeTiles ();
	return NULL;
}

void NavMeshBuilder::BuildNavMeshAsync (const NavMeshBuildSettings& settings)
{
	if (!GetApplication ().EnsureSceneHasBeenSaved ("navigation"))
		return;

	dynamic_array<MeshRenderer*> renderers;
	ExtractMeshRenderers (renderers);

	dynamic_array<TerrainBakeInfo> terrains;
	ExtractStaticTerrains (kNavigationStatic, terrains);

	if (renderers.empty () && terrains.empty ())
	{
		ClearAllNavMeshes ();
		return;
	}

	NavMeshBuilder* builder = new NavMeshBuilder ();
	builder->Prepare (settings, renderers.begin (), renderers.size (), terrains.begin (), terrains.size ());
	builder->m_Thread.Run (AsyncCompute, builder);

	SceneBackgroundTask::CreateTask (*builder);
}


void NavMeshBuilder::ClearAllNavMeshes ()
{
	Object* navMeshObject = GetNavMeshSettings ().GetNavMesh ();
	const std::string assetPath = GetAssetPathFromObject (navMeshObject);

	// Unload navmesh data
	GetNavMeshSettings ().SetNavMesh (NULL);

	// Delete object if not an asset.
	if (assetPath.empty ())
		DestroySingleObject (navMeshObject);

	// Delete the default file at the default location.
	const std::string folderPath = GetSceneBakedAssetsPath ();
	if (folderPath.empty ())
		return;

	const std::string defaultAssetPath = AppendPathName (folderPath, kDefaultNavMeshAssetName);
	if (IsFileCreated (defaultAssetPath))
		AssetInterface::Get ().DeleteAsset (defaultAssetPath);
}

void NavMeshBuilder::BackgroundStatusChanged ()
{
	CallStaticMonoMethod ("NavMeshEditorWindow", "BackgroundTaskStatusChanged");
}

bool NavMeshBuilder::IsFinished ()
{
	return !m_Thread.IsRunning ();
}

bool NavMeshBuilder::Complete ()
{
	bool success = false;

	std::string assetDirectory = GetSceneBakedAssetsPath ();

	// Handle error during NavMesh baking
	if (!m_AsyncNavMeshErrorMsg.empty ())
	{
		ErrorString (m_AsyncNavMeshErrorMsg);
	}
	else if (m_OutputData == NULL || m_OutputSize == 0)
	{
		ErrorString ("Internal NavMesh generation failure.");
	}
	else if (assetDirectory.empty ())
	{
		ErrorString ("No asset directory.");
	}
	else if (!CreateDirectory (assetDirectory))
	{
		ErrorString (Format ("Could not create asset directory %s.", assetDirectory.c_str ()));
	}
	else
	{
		NavMesh* oldNavMesh = GetNavMeshSettings ().GetNavMesh ();

		// Create the NavMesh object now to avoid extra copy of the HeightMesh.
		NavMesh* navmesh = NEW_OBJECT (NavMesh);
		navmesh->Reset ();
		navmesh->AwakeFromLoad (kInstantiateOrCreateFromCodeAwakeFromLoad);
		navmesh->SetData (m_OutputData, m_OutputSize);

		// Setup heightmesh
		if (m_buildHeightMesh)
			navmesh->SetHeightmaps (m_Heightmaps);

		GetNavMeshSettings ().SetNavMesh (navmesh);
		GetNavMeshSettings ().AwakeFromLoad (kInstantiateOrCreateFromCodeAwakeFromLoad);

		DestroySingleObject (oldNavMesh);
		AssetInterface::Get ().CreateSerializedAsset (*navmesh, AppendPathName (assetDirectory, kDefaultNavMeshAssetName), AssetInterface::kDeleteExistingAssets | AssetInterface::kWriteAndImport);

		ClearStaticOffMeshLinkPolyRefs ();

		success = true;
	}

	return success;
}

// Reset the polyref on OffMeshLinks set by previous bakes
void NavMeshBuilder::ClearStaticOffMeshLinkPolyRefs ()
{
    dynamic_array<OffMeshLink*> offMeshLinks;
    Object::FindObjectsOfType (&offMeshLinks);

    for (size_t i = 0; i < offMeshLinks.size (); ++i)
    {
    	// Avoid changing prefabs - it would mess up changing the instances in the scene.
        if (offMeshLinks[i]->IsPersistent ())
        	continue;

        if (offMeshLinks[i]->ClearStaticPolyRef ())
			offMeshLinks[i]->UpdatePositions ();
	}
}

void NavMeshBuilder::PrepareHeightMesh ()
{
	// Configure the Height Mesh.
	// Retrieve original sample dist.
	m_HeightMeshBuilder.Config (m_Config.ch, m_Config.detailSampleDist / m_Config.cs); // WEIRD NEED TO CHECK

	for (MeshBakeDescriptions::iterator i = m_BakeDescriptions.begin (); i != m_BakeDescriptions.end (); ++i)
	{
		MeshBakeDescription& desc = *i;
		if (desc.sourceType == MeshBakeDescription::kGeometry && desc.areaTag != NavMeshLayers::kNotWalkable)
		{
			const TriangleData triData = CalculateTriangleData (desc);
			m_HeightMeshBuilder.AddBakeSource (triData.vertices, triData.triangles, desc.bounds);
		}
	}
}


void NavMeshBuilder::ComputeHeightMeshTile (const rcPolyMeshDetail& polyMeshDetail)
{
	m_HeightMeshBuilder.AddDetailMesh (polyMeshDetail);
	m_HeightMeshBuilder.ComputeTile ();
}


void NavMeshBuilder::PlaceDropDownLinks (int ix, int iz, const class RecastTiles& tiles)
{
	m_AutoLinkPoints.resize_uninitialized (0);
	if (!LicenseInfo::Flag (lf_pro_version))
		return;

	GetMeshEdges (ix, iz, tiles);
	FindValidDropDowns (tiles);

	m_OffMeshParams.reserve (m_OffMeshParams.size () + m_AutoLinkPoints.size ());
	for (size_t i = 0; i < m_AutoLinkPoints.size (); ++i)
	{
		const Vector3f& spos = m_AutoLinkPoints[i].start;
		const Vector3f& epos = m_AutoLinkPoints[i].end;
		AddOffMeshConnection (spos, epos, m_Settings.agentRadius, false,
							 NavMeshLayers::kJumpLayer, kLinkTypeDropDown);
	}
}

void NavMeshBuilder::FindValidDropDowns (const class RecastTiles& tiles)
{
	float agentRadius = m_Config.walkableRadius * m_Config.cs;
	float stepSize = agentRadius * 2;
	float dropDownOffset = m_Settings.agentRadius * 2 + m_Config.cs * 4; // double agent radius + voxel error
	float minDropHeight = m_Config.walkableClimb * m_Config.ch;
	dynamic_array<Vector3f> samplePositions;
	dynamic_array<Vector3f> startPositions;

	for (size_t i = 0; i < m_AutoLinkEdgeSegments.size (); ++i)
	{
		const EdgeSegment& segment = m_AutoLinkEdgeSegments[i];
		Vector3f offsetSegmentStart = segment.start + segment.normal * dropDownOffset;
		Vector3f offsetSegmentEnd = segment.end + segment.normal * dropDownOffset;

		GetSubsampledLocations (segment.start, segment.end, stepSize, startPositions);
		GetSubsampledLocations (offsetSegmentStart, offsetSegmentEnd, stepSize, samplePositions);

		for (size_t j = 0; j < samplePositions.size (); ++j)
		{
			float nearestDistance = VerticalNavMeshTest (samplePositions[j], m_Settings.ledgeDropHeight);

			if (nearestDistance < m_Settings.ledgeDropHeight && nearestDistance > minDropHeight)
			{
				Vector3f endPosition (samplePositions[j]);
				endPosition.y -= nearestDistance;
				if (!DropDownBlocked (startPositions[j], endPosition, m_Config.cs, tiles))
				{
					AutoLinkPoints& points = m_AutoLinkPoints.push_back ();
					points.start = startPositions[j];
					points.end = endPosition;
				}
			}
		}
	}
}

bool NavMeshBuilder::DropDownBlocked (Vector3f startPos, Vector3f endPos, float cs, const class RecastTiles& tiles)
{
	Vector3f xzStep, perp1Step, perp2Step, centerScanner, perp1Scanner, perp2Scanner;
	float height = m_Config.walkableHeight * m_Config.ch;
	float linkXZlength, scannedLen, perpScannedLen, radius, stepSize, verticalOffset;
	bool overedgeflag = false;
	float scanHeight;

	verticalOffset = m_Config.walkableRadius * cs * tan (m_Config.walkableSlopeAngle / 180.0f*RC_PI) + cs;
	// max reachable within radius of agent on a slope + one voxel
	startPos.y += verticalOffset;
	xzStep = endPos - startPos;
	xzStep.y = 0;
	linkXZlength = Magnitude (xzStep);
	xzStep = Normalize (xzStep);

	stepSize = cs / 2.0f;
	xzStep *= stepSize;
	perp1Step.x = xzStep.z;
	perp1Step.y = 0;
	perp1Step.z = -xzStep.x;
	perp2Step.x = -xzStep.z;
	perp2Step.y = 0;
	perp2Step.z = xzStep.x;

	radius = m_Config.walkableRadius * cs;

	scannedLen = 0;
	centerScanner = startPos;
	centerScanner.y += verticalOffset;
	scanHeight = height - verticalOffset;
	while (scannedLen < linkXZlength)
	{
		if (tiles.CheckHeightfieldCollision (centerScanner.x, centerScanner.y, centerScanner.y + scanHeight, centerScanner.z))
		{
			return true;
		}
		perpScannedLen = stepSize;
		perp1Scanner = centerScanner + perp1Step;
		perp2Scanner = centerScanner + perp2Step;
		while (perpScannedLen < radius)
		{
			if (tiles.CheckHeightfieldCollision (perp1Scanner.x, perp1Scanner.y, perp1Scanner.y + scanHeight, perp1Scanner.z))
			{
				return true;
			}
			if (tiles.CheckHeightfieldCollision (perp2Scanner.x, perp2Scanner.y, perp2Scanner.y + scanHeight, perp2Scanner.z))
			{
				return true;
			}
			perp1Scanner += perp1Step;
			perp2Scanner += perp2Step;
			perpScannedLen += stepSize;
		}

		scannedLen += stepSize;
		centerScanner += xzStep;

		if (!overedgeflag && linkXZlength - scannedLen < radius)
		{
			overedgeflag = true;
			centerScanner.y = endPos.y + verticalOffset;
			perp1Scanner.y = endPos.y + verticalOffset;
			perp2Scanner.y = endPos.y + verticalOffset;
			scanHeight += startPos.y - endPos.y;
		}
	}
	return false;
}

void NavMeshBuilder::PlaceJumpAcrossLinks (int ix, int iz, const class RecastTiles& tiles)
{
	m_AutoLinkPoints.resize_uninitialized (0);
	if (!LicenseInfo::Flag (lf_pro_version))
		return;

	GetMeshEdges (ix, iz, tiles);
	FindValidJumpAcrossLinks (tiles);

	m_OffMeshParams.reserve (m_OffMeshParams.size () + m_AutoLinkPoints.size ());
	for (size_t i = 0; i < m_AutoLinkPoints.size (); ++i)
	{
		const Vector3f& spos = m_AutoLinkPoints[i].start;
		const Vector3f& epos = m_AutoLinkPoints[i].end;
		AddOffMeshConnection (spos, epos, m_Settings.agentRadius, false,
							 NavMeshLayers::kJumpLayer, kLinkTypeJumpAcross);
	}
}

void NavMeshBuilder::FindValidJumpAcrossLinks (const class RecastTiles& tiles)
{
	float agentRadius = m_Config.walkableRadius * m_Config.cs;
	float unevenLinkMargin = m_Config.ch * 2;
	float testDepth = unevenLinkMargin * 2;
	float stepSize = agentRadius * 2;
	dynamic_array<Vector3f> samplePositions;
	dynamic_array<Vector3f> startPositions;

	for (size_t i = 0; i < m_AutoLinkEdgeSegments.size (); ++i)
	{
		const EdgeSegment& segment = m_AutoLinkEdgeSegments[i];
		Vector3f offsetSegmentStart = segment.start + segment.normal * stepSize;
		offsetSegmentStart.y += unevenLinkMargin;
		Vector3f offsetSegmentEnd = segment.end + segment.normal * stepSize;
		offsetSegmentEnd.y += unevenLinkMargin;

		GetSubsampledLocations (segment.start, segment.end, stepSize, startPositions);
		GetSubsampledLocations (offsetSegmentStart, offsetSegmentEnd, stepSize, samplePositions);
		Vector3f sampleStep = segment.normal * stepSize;
		for (size_t j=0; j<samplePositions.size (); ++j)
		{
			float sampleProgress = 0;
			Vector3f samplePoint = samplePositions[j];

			while (sampleProgress < m_Settings.maxJumpAcrossDistance)
			{
				float nearestDistance = VerticalNavMeshTest (samplePoint, testDepth);

				if (nearestDistance <= testDepth)
				{
					Vector3f endPosition (samplePoint);
					endPosition.y -= nearestDistance;
					if (!JumpAcrossBlocked (startPositions[j], endPosition, m_Config.cs, tiles))
					{
						AutoLinkPoints& points = m_AutoLinkPoints.push_back ();
						points.start = startPositions[j];
						points.end = endPosition;
					}
					break;
				}
				sampleProgress += stepSize;
				samplePoint += sampleStep;
			}
		}
	}
}

bool NavMeshBuilder::JumpAcrossBlocked (Vector3f startPos, Vector3f endPos, float cs, const class RecastTiles& tiles)
{
	Vector3f xzStep, perp1Step, perp2Step, centerScanner, perp1Scanner, perp2Scanner;
	float height = m_Config.walkableHeight * m_Config.ch;
	float linkXZlength, scannedLen, perpScannedLen, radius, stepSize, verticalOffset;
	float scanHeight;
	float heightDiff = abs (startPos.y - endPos.y);

	verticalOffset = m_Config.walkableRadius * cs * tan (m_Config.walkableSlopeAngle / 180.0f*RC_PI) + cs + heightDiff;
	// max reachable within radius of agent on a slope + one voxel
	startPos.y += verticalOffset;
	xzStep = endPos - startPos;
	xzStep.y = 0;
	linkXZlength = Magnitude (xzStep);
	xzStep = Normalize (xzStep);

	stepSize = cs / 2.0f;
	xzStep *= stepSize;
	perp1Step.x = xzStep.z;
	perp1Step.y = 0;
	perp1Step.z = -xzStep.x;
	perp2Step.x = -xzStep.z;
	perp2Step.y = 0;
	perp2Step.z = xzStep.x;

	radius = m_Config.walkableRadius * cs;

	scannedLen = 0;
	centerScanner = startPos;
	centerScanner.y += verticalOffset;
	scanHeight = height - verticalOffset;
	while (scannedLen < linkXZlength)
	{
		if (tiles.CheckHeightfieldCollision (centerScanner.x, centerScanner.y, centerScanner.y + scanHeight, centerScanner.z))
		{
			return true;
		}
		perpScannedLen = stepSize;
		perp1Scanner = centerScanner + perp1Step;
		perp2Scanner = centerScanner + perp2Step;
		while (perpScannedLen < radius)
		{
			if (tiles.CheckHeightfieldCollision (perp1Scanner.x, perp1Scanner.y, perp1Scanner.y + scanHeight, perp1Scanner.z))
			{
				return true;
			}
			if (tiles.CheckHeightfieldCollision (perp2Scanner.x, perp2Scanner.y, perp2Scanner.y + scanHeight, perp2Scanner.z))
			{
				return true;
			}
			perp1Scanner += perp1Step;
			perp2Scanner += perp2Step;
			perpScannedLen += stepSize;
		}

		scannedLen += stepSize;
		centerScanner += xzStep;
	}
	return false;
}

void NavMeshBuilder::GetMeshEdges (int ix, int iz, const class RecastTiles& recastTiles)
{
	static const int MAX_SEGS_PER_POLY = DT_VERTS_PER_POLYGON*3;
	m_AutoLinkEdgeSegments.clear ();
	dtQueryFilter filter;

	const dtMeshTile* tile = m_dtNavMesh.getTileAt (ix, iz, 0);
	if (!tile)
		return;

	dtTileRef tileRef = m_dtNavMesh.getTileRef (tile);
	float segs[MAX_SEGS_PER_POLY*6];

	const RecastTile& recastTile = recastTiles.GetTile (ix, iz);

	for (int poly=0; poly<tile->header->polyCount; ++poly)
	{
		if (recastTile.m_polygonFlags[poly] == 0)
			continue;

		dtPolyRef polyRef = tileRef | poly;
		int nsegs = 0;

		m_NavMeshQuery.getPolyWallSegments (polyRef, &filter, segs, NULL, &nsegs, MAX_SEGS_PER_POLY);

		for (int k = 0; k < nsegs; ++k)
		{
			const float* s = &segs[k*6];

			Vector3f start (s);
			Vector3f end (s+3);

			Vector3f normal = end-start;
			normal.y = 0;
			normal = Normalize (normal);
			normal.Set (-normal.z, 0, normal.x);

			EdgeSegment& segment = m_AutoLinkEdgeSegments.push_back ();
			segment.start = start;
			segment.end = end;
			segment.normal = normal;
		}
	}
}

float NavMeshBuilder::VerticalNavMeshTest (Vector3f testFrom, float testHeight)
{
	dtQueryFilter filter;
	float extentSize = testHeight * 0.5f;
	Vector3f extents (0, extentSize, 0);
	Vector3f midPoint = testFrom;

	midPoint.y -= extentSize;
	dtPolyRef polys[128];
	int polyCount = 0;
	if (dtStatusFailed (m_NavMeshQuery.queryPolygons (midPoint.GetPtr (), extents.GetPtr (), &filter, polys, &polyCount, 128)))
		return FLT_MAX;

	// Find higher poly but not higher then max height.
	float startHeight = testFrom.y;
	float nearestDistance = FLT_MAX;
	for (int polyIdx = 0; polyIdx < polyCount; ++polyIdx)
	{
		float height;
		if (dtStatusSucceed (m_NavMeshQuery.getPolyHeight (polys[polyIdx], testFrom.GetPtr (), &height)))
		{
			float distance = startHeight - height;
			if (distance > 0 && distance < nearestDistance)
			{
				nearestDistance = distance;
			}
		}
	}

	return nearestDistance;
}

void NavMeshBuilder::GetSubsampledLocations (Vector3f segmentStart, Vector3f segmentEnd, float subsampleDistance, dynamic_array<Vector3f>& locations)
{
	locations.resize_initialized (0);
	float segmentLength = Magnitude (segmentStart - segmentEnd);
	Vector3f segmentMidPoint = (segmentStart + segmentEnd) / 2;
	Vector3f normal = segmentEnd - segmentStart;
	normal.y = 0;
	normal = Normalize (normal);
	normal.Set (-normal.z, 0, normal.x);

	Vector3f samplePoint = segmentMidPoint + normal * subsampleDistance;
	locations.push_back (segmentMidPoint);
	if (segmentLength > subsampleDistance * 2) // construct subsample locations
	{
		Vector3f sampleStep = Normalize (segmentStart - segmentEnd) * subsampleDistance;
		Vector3f pos1 = segmentMidPoint;
		Vector3f pos2 = segmentMidPoint;
		float subSampleProgress = subsampleDistance;
		while (subSampleProgress < segmentLength / 2)
		{
			pos1 += sampleStep;
			pos2 -= sampleStep;
			locations.push_back (pos1);
			locations.push_back (pos2);
			subSampleProgress += subsampleDistance;
		}
	}
}

void NavMeshBuilder::ProjectVertices ()
{
	if (!NAVMESH_PROJECT_VERTICES)
		return;

	HeightMeshQuery hmq;
	hmq.Init (&m_Heightmaps, 0.0f);
	const float sampleDistance = 0.8f*m_Settings.agentHeight;

	for (int i = 0; i < m_dtNavMesh.getMaxTiles (); ++i)
	{
		const dtMeshTile* tile = m_dtNavMesh.getTile (i);
		if (!tile || !tile->header || !tile->dataSize)
			continue;

		for (int index=0; index<tile->header->vertCount; ++index)
		{
			float* vertex = &tile->verts[3*index];
			SetVertexHeight (i, hmq, sampleDistance, vertex);
		}

		for (int index=0; index<tile->header->detailVertCount; ++index)
		{
			float* vertex = &tile->detailVerts[3*index];
			SetVertexHeight (i, hmq, sampleDistance, vertex);
		}
	}
}

void NavMeshBuilder::SetVertexHeight (int tileID, const HeightMeshQuery& hmq, float sampleDistance, float* vertex)
{
	float terrainHeight, meshHeight;
	bool isOnTerrain = hmq.GetTerrainHeight (Vector3f (vertex), &terrainHeight);
	bool isOnMesh = m_HeightMeshBuilder.GetMeshHeight (tileID, vertex, sampleDistance, &meshHeight);

	if (isOnTerrain && isOnMesh)
	{
		float bestHeight = Abs (terrainHeight-vertex[1]) < Abs (meshHeight-vertex[1]) ? terrainHeight : meshHeight;
		vertex[1] = bestHeight;
	}
	else if (isOnTerrain)
	{
		vertex[1] = terrainHeight;
	}
	else if (isOnMesh)
	{
		vertex[1] = meshHeight;
	}
}

std::string NavMeshBuilder::ComputeRecastTile (int ix, int iz, const int tileSize, const Vector3f& worldMin, const Vector3f& worldMax, RecastTiles& recastTiles)
{
	Vector3f bMin = worldMin;
	bMin.x += ix*tileSize*m_Config.cs;
	bMin.z += iz*tileSize*m_Config.cs;

	Vector3f bMax;
	bMax.x = bMin.x + tileSize*m_Config.cs;
	bMax.y = worldMax.y;
	bMax.z = bMin.z + tileSize*m_Config.cs;

	RecastTile recastTile;
	std::string error = BuildTileMesh (bMin, bMax, recastTile);
	if (error.empty ())
	{
		recastTiles.SetTile (ix, iz, recastTile);
	}
	else
	{
		recastTile.Free ();
	}

	return error;
}


