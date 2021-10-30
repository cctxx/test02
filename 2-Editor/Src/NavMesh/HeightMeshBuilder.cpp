#include "UnityPrefix.h"
#include "HeightMeshBuilder.h"
#include "HeightMesh.h"
#include "TriReduction.h"
#include "TriTessellation.h"
#include "Recast.h"
#include "RecastAlloc.h"

#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Geometry/Ray.h"
#include "Runtime/Geometry/Plane.h"

//#define HEIGHT_MESH_BUILDER_DEBUG

#ifdef HEIGHT_MESH_BUILDER_DEBUG
	#include "Runtime/Utilities/LogAssert.h"
	#define HEIGHT_MESH_BUILDER_LOG(x) LogString (x);
#else
	#define HEIGHT_MESH_BUILDER_LOG(x) {}
#endif


// HMPoly triangle vertex extraction
static inline void GetHMPolyTriangleVertices (const HMPoly& p, int triIndex, Vector3f vertices[3])
{
	vertices[0] = Vector3f (&p.vertices[3 * p.triIndices[4*triIndex + 0]]);
	vertices[1] = Vector3f (&p.vertices[3 * p.triIndices[4*triIndex + 1]]);
	vertices[2] = Vector3f (&p.vertices[3 * p.triIndices[4*triIndex + 2]]);
}

// Is any of the triangle vertices inside AABB
static inline bool InsideTriVertexAABB (const Vector3f triVertices[3], const MinMaxAABB& aabb)
{
	return aabb.IsInside (triVertices[0]) || aabb.IsInside (triVertices[1]) || aabb.IsInside (triVertices[2]);
}

// Approximate test if triangle edge segments penetrate the AABB.
// This does not catch cases where AABB is inside the triangle.
static inline bool IntersectTriEdgeAABB (const Vector3f triVerts[3], const AABB& aabb)
{
	Vector3f dir[3];
	dir[0] = NormalizeFast (triVerts[1] - triVerts[0]);
	dir[1] = NormalizeFast (triVerts[2] - triVerts[1]);
	dir[2] = NormalizeFast (triVerts[0] - triVerts[2]);

	for (int j = 0; j < 3; ++j)
	{
		// We want to intersect with a segment. Check for ray intersection
		// and validate that the intersection is between the two vertices.

		// Here, we make the assumption that since the AABB of the 2 triangles
		// intersect, the intersection is on the segment. We may end up with some
		// unwanted triangles, but not much.

		Ray ray (triVerts[j], dir[j]);
		if (IntersectRayAABB (ray, aabb))
			return true;
	}
	return false;
}

HeightMeshBuilder::HeightMeshBuilder ()
: m_detailPolys (NULL)
, m_heightMeshPolys (NULL)
, m_voxelizationError (0.0f)
, m_bakeSourceTriangleCount (0)
, m_bakeSourceVertexCount (0)
{
}

HeightMeshBuilder::~HeightMeshBuilder ()
{
}

void HeightMeshBuilder::Config (
	float voxelHeight,
	float voxelSampling)
{
	Assert (voxelHeight >= 0.0f);
	Assert (voxelSampling >= 0.0f);

	// Worst-case height error approximation correspond to the sampling
	// multiplied by the voxel height + error margin.
	m_voxelizationError = voxelHeight * (voxelSampling + 1.0f);
}

void HeightMeshBuilder::AddBakeSource (
	const dynamic_array<Vector3f>& vertices,
	const dynamic_array<int>& triIndices,
	MinMaxAABB& aabb)
{
	m_heightBakeSources.push_back (HeightMeshBakeSource ());
	HeightMeshBakeSource& bakeData = m_heightBakeSources.back ();

	bakeData.poly.ntris = triIndices.size () / 3;
	bakeData.poly.nverts = vertices.size ();


	dynamic_array<float>& verts = bakeData.poly.vertices;
	verts.resize_uninitialized (vertices.size () * 3);
	for (int i = 0; i < bakeData.poly.nverts; ++i)
	{
		verts[i*3+0] = vertices[i].x;
		verts[i*3+1] = vertices[i].y;
		verts[i*3+2] = vertices[i].z;
	}

	dynamic_array<unsigned short>& tris = bakeData.poly.triIndices;
	tris.resize_uninitialized (triIndices.size () * 4);
	for (int i = 0; i < bakeData.poly.ntris; ++i)
	{
		tris[i*4+0] = triIndices[i*3+0];
		tris[i*4+1] = triIndices[i*3+1];
		tris[i*4+2] = triIndices[i*3+2];
		tris[i*4+3] = -1;
	}

	bakeData.triPolyHit.resize_initialized (bakeData.poly.ntris, -1);
	bakeData.aabb = aabb;

	m_bakeSourceTriangleCount += bakeData.poly.ntris;
	m_bakeSourceVertexCount += bakeData.poly.nverts;
}

void HeightMeshBuilder::SimplifyBakeSources ()
{
	// Cull-out non-walkable surfaces.
	MarkNonWalkableGeom ();

	for (HeightMeshBakeSources::iterator it = m_heightBakeSources.begin ();it != m_heightBakeSources.end ();it++)
	{
		HMPoly& hp = (*it).poly;

		// Convert the poly to a structure understandable by the reduction algorithms.
		TRVerts vertices;
		TRTris triangles;
		tri_reduction_convert (hp, vertices, triangles);

		// 1. Remove non-walkable geometry.
		tri_reduction_remove_non_walkable (vertices, triangles, (*it).triWalkableFlags);

		// A possible optimization may be to sanitize the triangle/vertices list here to
		// remove unused vertices and NULL triangles...

		// 2. See if it is a planar surface. If yes, re-tessellate.
		bool isPlanar = tri_tessellation_planar_surface_retessallation (vertices, triangles, hp);

		if (!isPlanar)
		{
			// 3. Try to collapse the geometry.
			tri_reduction_collapse_no_cost (vertices, triangles);
			tri_reduction_convert_back (vertices, triangles, hp);
		}

		// Clean-up.
		tri_reduction_clean_up (vertices, triangles);

		(*it).triPolyHit.resize_initialized (hp.ntris, -1);
	}
}

void HeightMeshBuilder::AddDetailMesh (const rcPolyMeshDetail& polyMeshDetail)
{
	// Start a new tile.
	BeginTile ();

	// Update the
	int detailTriangleCount = 0;
	m_detailPolys->reserve (polyMeshDetail.nmeshes);

	for (int iPoly = 0; iPoly < polyMeshDetail.nmeshes; ++iPoly)
	{
		// Detail Mesh
		// int[0] = base vertex
		// int[1] = vertices count
		// int[2] = base triangle
		// int[3] = triIndices count
		// NOTE: Maybe we could avoid a copy by using directly the PolyMeshDetail...
		// However, I'm not sure the saved copy worth the ease of manipulating the data.
		const unsigned int* m = &polyMeshDetail.meshes[iPoly*4];
		const unsigned int bverts = m[0];
		const unsigned int nverts = m[1];
		const unsigned int btris = m[2];
		const int ntris = (int)m[3];
		const float* verts = &polyMeshDetail.verts[bverts*3];
		const rcPolyMeshDetailIndex* tris = &polyMeshDetail.tris[btris*4];

		m_detailPolys->push_back (HMPoly ());
		HMPoly& dp = m_detailPolys->back ();
		dp.polyIdx = iPoly;
		dp.ntris = ntris;
		dp.nverts = nverts;
		dp.vertices.assign (verts, verts + nverts*3);	// 3 floats per vertices.

		// 4 indices per triangle (last is junk). We convert the indices to int, because to
		// define a poly, it will take more than 255 geometry triangles.
		dp.triIndices.resize_uninitialized (ntris*4);
		for (int i = 0; i < ntris*4; ++i)
		{
			dp.triIndices[i] = *(tris+i);
		}

		detailTriangleCount += ntris;
	}

	HEIGHT_MESH_BUILDER_LOG (Format ("Detailed triangle count: %d", detailTriangleCount));
}

void HeightMeshBuilder::BeginTile ()
{
	HeightMesh::HMTile& currTile = m_heightMesh.AddTile ();

	// Update pointers.
	m_detailPolys = &currTile.detailPolys;
	m_heightMeshPolys = &currTile.heightMeshPolys;
}

bool HeightMeshBuilder::GetMeshHeight (int tileID, const float* position, float maxSampleDistance, float* height) const
{
	const HeightMesh::HMTile& tile = m_heightMesh.GetTile (tileID);

	float tmin = maxSampleDistance;
	Vector3f dir = -Vector3f::yAxis;
	Ray ray (Vector3f (position), dir);
	*height = position[1];

	for (HMPolys::const_iterator it = tile.heightMeshPolys.begin (); it != tile.heightMeshPolys.end (); ++it)
	{
		const HMPoly& hp = *it;
		for (int j=0; j<hp.ntris; ++j)
		{
			float t;
			Vector3f verts[3];
			GetHMPolyTriangleVertices (hp, j, verts);
			if (IntersectRayTriangle (ray, verts[0], verts[1], verts[2], &t) && t<tmin)
				tmin = t;
		}
	}

	// Return original height if no hit within maxSampleDistance
	if (tmin == maxSampleDistance)
		return false;

	// Return closest hit
	*height = position[1] + tmin*dir[1];
	return true;
}

inline static bool AreaIsMarked (int a, int id)
{
	return a == id;
}

inline static void MarkArea (int& a, int ip)
{
	a = ip;
}

void HeightMeshBuilder::ComputeTile ()
{
	//	For each triangle in detailMesh
	//		Compute source AABB
	//	    For each triangle in geometryMesh
	//			Compute source AABB
	//			If detail AABB intersects source AABB
	//				If AABB intersects with tri
	//					Mark triangle as writable
	//
	// Note: This method collects to many detail triangles e.g.
	// lying under sloped polygons - due to conservative acceptance:
	// if any detail poly vertex is inside source poly AABB the detail poly is marked.

	m_heightMeshPolys->reserve (m_detailPolys->size ());
	for (int iPoly = 0; iPoly < m_detailPolys->size (); ++iPoly)
	{
		HMPoly& dp = (*m_detailPolys)[iPoly];
		m_heightMeshPolys->push_back (HMPoly ());
		HMPoly& hp = m_heightMeshPolys->back ();
		hp.polyIdx = dp.polyIdx;
		hp.ntris = 0;
		hp.nverts = 0;

		// Intersect all sources to every triangles of the poly.
		for (HeightMeshBakeSources::iterator it = m_heightBakeSources.begin (); it != m_heightBakeSources.end (); ++it)
		{
			HeightMeshBakeSource& rSource = *it;
			for (int iTri = 0; iTri < dp.ntris; ++iTri)
			{
				Vector3f detailVerts[3];
				GetHMPolyTriangleVertices (dp, iTri, detailVerts);

				// Compute inflated detail triangle bounding box.
				MinMaxAABB detailAABBExt (detailVerts[0], detailVerts[0]);
				detailAABBExt.Encapsulate (detailVerts[1]);
				detailAABBExt.Encapsulate (detailVerts[2]);
				detailAABBExt.Expand (Vector3f (0.0f, m_voxelizationError, 0.0f));

				// Skip if bounds are not intersecting.
				if (!IntersectAABBAABB (detailAABBExt, rSource.aabb))
					continue;

				// Check which triangles in the source are involved.
				for (int i = 0; i < rSource.poly.ntris; ++i)
				{
					// Skip triangles that are already marked.
					if (AreaIsMarked (rSource.triPolyHit[i], dp.polyIdx))
						continue;

					// The current source triangle.
					Vector3f sourceVerts[3];
					GetHMPolyTriangleVertices (rSource.poly, i, sourceVerts);

					// Compute source triangle bounding box.
					MinMaxAABB sourceAABB (sourceVerts[0], sourceVerts[0]);
					sourceAABB.Encapsulate (sourceVerts[1]);
					sourceAABB.Encapsulate (sourceVerts[2]);

					// Bounds must intersect.
					if (!IntersectAABBAABB (detailAABBExt, sourceAABB))
						continue;

					// Check if any of the source triangle vertices are inside the detail mesh triangle bounds.
					if (InsideTriVertexAABB (sourceVerts, detailAABBExt))
					{
						MarkArea (rSource.triPolyHit[i], iPoly);
						continue;
					}

					// Other way around.
					// Compute inflated source triangle bounding box.
					MinMaxAABB sourceAABBExt = sourceAABB;
					sourceAABBExt.Expand (Vector3f (0.0f, m_voxelizationError, 0.0f));

					if (InsideTriVertexAABB (detailVerts, sourceAABBExt))
					{
						MarkArea (rSource.triPolyHit[i], iPoly);
						continue;
					}

					// If not, check if triangles intersect the other triangle AABB
					if (IntersectTriEdgeAABB (sourceVerts, detailAABBExt) || IntersectTriEdgeAABB (detailVerts, sourceAABBExt))
					{
						MarkArea (rSource.triPolyHit[i], iPoly);
						continue;
					}
				}
			}

			// Build Height HMPoly.
			AddSourceToPolyDetail (rSource, hp);
		}
	}
	m_heightMesh.CompleteTile ();
}

void HeightMeshBuilder::AddSourceToPolyDetail (const HeightMeshBakeSource& rSource, HMPoly& poly)
{
	// 1. Mark used vertices.
	// 2. Create the old tris indices array conversion.
	// 3. Convert tris vertex indices and copy them to the HMPoly.
	// 4. Copy vertices to the HMPoly
	// 5. Set ntris, nverts.

	const int NOT_MARK = -2;
	const int MARK = -1;

	// 1. Mark used vertices.
	dynamic_array<int> markedVerts;
	int ntris = 0;
	int nverts = 0;
	markedVerts.resize_initialized (rSource.poly.nverts, NOT_MARK);
	for (int iHit = 0; iHit < rSource.triPolyHit.size (); ++iHit)
	{
		// Skip triangles that are not intersecting with a detailed polygon.
		if (!AreaIsMarked (rSource.triPolyHit[iHit], poly.polyIdx))
			continue;

		int iTri = iHit * 4;
		for (int i = iTri; i < iTri+3; ++i)
			markedVerts[ rSource.poly.triIndices[i] ] = MARK;

		++ntris;
	}

	// 2. Create conversion array.
	int newIdx = poly.nverts;
	for (int iVert = 0; iVert < markedVerts.size (); ++iVert)
	{
		if (markedVerts[iVert] == MARK)
		{
			markedVerts[iVert] = newIdx++;
			++nverts;
		}
	}

	// 3. Convert tris vertex indices and copy them to the HMPoly.
	// Remember we have 4 indices per triangle... Fourth is junk.
	int nextTriangleIndex = poly.triIndices.size ();
	const int totalTriangleCount = nextTriangleIndex + ntris*4;
	poly.triIndices.resize_uninitialized (totalTriangleCount);

	for (int iHit = 0; iHit < rSource.triPolyHit.size (); ++iHit)
	{
		if (!AreaIsMarked (rSource.triPolyHit[iHit], poly.polyIdx))
			continue;

		int iSrcTri = iHit * 4;
		poly.triIndices[nextTriangleIndex++] = markedVerts[rSource.poly.triIndices[iSrcTri+0]];
		poly.triIndices[nextTriangleIndex++] = markedVerts[rSource.poly.triIndices[iSrcTri+1]];
		poly.triIndices[nextTriangleIndex++] = markedVerts[rSource.poly.triIndices[iSrcTri+2]];
		poly.triIndices[nextTriangleIndex++] = -1; // junk
	}
	Assert (nextTriangleIndex == totalTriangleCount);

	// 4. Copy vertices to the HMPoly
	int nextVertexIndex = poly.vertices.size ();
	const int totalVertexCount = nextVertexIndex + nverts*3;
	poly.vertices.resize_uninitialized (totalVertexCount);
	for (int iVert = 0; iVert < markedVerts.size (); ++iVert)
	{
		if (markedVerts[iVert] != NOT_MARK)
		{
			poly.vertices[nextVertexIndex++] = rSource.poly.vertices[iVert*3 + 0];
			poly.vertices[nextVertexIndex++] = rSource.poly.vertices[iVert*3 + 1];
			poly.vertices[nextVertexIndex++] = rSource.poly.vertices[iVert*3 + 2];
		}
	}
	Assert (nextVertexIndex == totalVertexCount);

	// 5. Set ntris, nverts.
	poly.ntris += ntris;
	poly.nverts += nverts;
}

void HeightMeshBuilder::MarkNonWalkableGeom ()
{
	const float walkableSlopeAngleDeg = 89.9f;
	const float walkableThr = cosf (Deg2Rad (walkableSlopeAngleDeg));
	for (HeightMeshBakeSources::iterator it = m_heightBakeSources.begin (); it != m_heightBakeSources.end (); ++it)
	{
		HeightMeshBakeSource& rSource = *it;

		int triangleCount = rSource.poly.ntris;
		rSource.triWalkableFlags.resize_initialized (triangleCount, false);

		bool* areas = &rSource.triWalkableFlags[0];

		for (int iTri = 0; iTri < rSource.poly.ntris; ++iTri)
		{
			Vector3f verts[3];
			GetHMPolyTriangleVertices (rSource.poly, iTri, verts);
			Vector3f n = Cross (verts[1] - verts[0], verts[2] - verts[1]);
			n = NormalizeSafe (n);

			// Check if the face is walkable.
			float costheta = Dot (Vector3f (0, 1, 0), n);
			if (costheta > walkableThr)
				areas[iTri] = true;
		}
	}
}

void HeightMeshBuilder::Complete ()
{
#ifdef HEIGHT_MESH_BUILDER_DEBUG

	unsigned int heightMeshTriangleCount = 0;
	unsigned int heightMeshVertexCount = 0;
	int heightMeshSize = 0;
	for (HMPolys::iterator it = m_heightMeshPolys->begin (); it != m_heightMeshPolys->end (); ++it)
	{
		HMPoly& p = *it;
		heightMeshTriangleCount += p.ntris;
		heightMeshVertexCount += p.nverts;
		heightMeshSize +=
			sizeof (float) * p.vertices.size () +
			sizeof (int) * p.triIndices.size ();
	}
	heightMeshSize /= 1024.0;
	HEIGHT_MESH_BUILDER_LOG (Format ("HeightMesh size: %d Kb", heightMeshSize));
	HEIGHT_MESH_BUILDER_LOG (Format ("Geometry Tris: %d, HeightMesh Tris: %d", m_bakeSourceTriangleCount, heightMeshTriangleCount));
	HEIGHT_MESH_BUILDER_LOG (Format ("Geometry Verts: %d, HeightMesh Verts: %d", m_bakeSourceVertexCount, heightMeshVertexCount));

#endif
}

bool HeightMeshBuilder::FillDetailMesh (const rcPolyMesh& polyMesh, rcPolyMeshDetail& tempDetailMesh)
{
	// Calculate vertex count for each poly in nav mesh.
	dynamic_array<int> polyVertCount (polyMesh.npolys, kMemTempAlloc);

	for (int i=0; i <polyMesh.npolys; ++i)
	{
		const unsigned short* p = &polyMesh.polys[i*polyMesh.nvp*2];
		int nv = 0;
		for (int j = 0; j < polyMesh.nvp; ++j)
		{
			if (p[j] == RC_MESH_NULL_IDX) break;
			++nv;
		}
		polyVertCount[i] = nv;
	}

	tempDetailMesh.nmeshes = m_heightMeshPolys->size ();

	tempDetailMesh.meshes = (unsigned int*)rcAlloc (sizeof (unsigned int)*tempDetailMesh.nmeshes*4, RC_ALLOC_PERM);
	if (!tempDetailMesh.meshes)
	{
		return false;
	}

	tempDetailMesh.nverts = 0;
	unsigned int* meshHeader = tempDetailMesh.meshes;
	int polyIndex=0;
	for (HMPolys::iterator it = m_heightMeshPolys->begin (); it != m_heightMeshPolys->end (); ++it, ++polyIndex)
	{
		HMPoly& hp = (*it);
		meshHeader[0] = tempDetailMesh.nverts; // base verts
		int numVerts = hp.nverts + polyVertCount[polyIndex];
		meshHeader[1] = numVerts; // num verts
		meshHeader += 4;
		tempDetailMesh.nverts += numVerts;
	}

	tempDetailMesh.verts = (float*)rcAlloc (sizeof (float)*tempDetailMesh.nverts*3, RC_ALLOC_PERM);
	if (!tempDetailMesh.verts)
	{
		return false;
	}

	float* verts = tempDetailMesh.verts;
	polyIndex=0;
	for (HMPolys::iterator it = m_heightMeshPolys->begin (); it != m_heightMeshPolys->end (); ++it, ++polyIndex)
	{
		HMPoly& hp = (*it);
		// Leave first ones uninitialized as they are not used.
		memcpy (verts+polyVertCount[polyIndex]*3, hp.vertices.begin (), hp.vertices.size () * sizeof (float));
		verts += hp.vertices.size ()+polyVertCount[polyIndex]*3;
	}

	tempDetailMesh.ntris = 0;
	meshHeader = tempDetailMesh.meshes;

	for (HMPolys::iterator it = m_heightMeshPolys->begin (); it != m_heightMeshPolys->end (); ++it)
	{
		HMPoly& hp = (*it);
		meshHeader[2] = tempDetailMesh.ntris; // base triangle
		meshHeader[3] = hp.ntris; // num tris
		meshHeader += 4;
		tempDetailMesh.ntris += hp.ntris;
	}

	tempDetailMesh.tris = (rcPolyMeshDetailIndex*)rcAlloc (sizeof (rcPolyMeshDetailIndex)*tempDetailMesh.ntris*4, RC_ALLOC_PERM);
	if (!tempDetailMesh.tris)
	{
		return false;
	}

	rcPolyMeshDetailIndex* triangles = tempDetailMesh.tris;
	polyIndex = 0;
	for (HMPolys::iterator it = m_heightMeshPolys->begin (); it != m_heightMeshPolys->end (); ++it, ++polyIndex)
	{
		HMPoly& hp = (*it);

		for (int j=0; j<hp.ntris; ++j)
		{
			triangles[0] = polyVertCount[polyIndex] + hp.triIndices[j*4+0];
			triangles[1] = polyVertCount[polyIndex] + hp.triIndices[j*4+1];
			triangles[2] = polyVertCount[polyIndex] + hp.triIndices[j*4+2];
			triangles += 4;
		}
	}

	return true;
}
