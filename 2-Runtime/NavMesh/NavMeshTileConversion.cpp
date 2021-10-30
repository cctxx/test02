#include "UnityPrefix.h"
#include "NavMeshTileConversion.h"
#include "DynamicMesh.h"
#include "DetourNavMesh.h"
#include "DetourCommon.h"
#include "DetourAlloc.h"

// TODO:
// Create BVH for carved tile
// Preserve detail mesh for the carved polygons.

const float MAGIC_EDGE_DISTANCE = 1e-2f; // Same as used in detour navmesh builder.

static void RequirementsForDetailMeshUsingHeightMesh (int* detailVertCount, int* detailTriCount, const DynamicMesh& mesh, const dtMeshTile* sourceTile);
static void RequirementsForDetailMeshMixed (int* detailVertCount, int* detailTriCount, const DynamicMesh& mesh, const dtMeshTile* sourceTile);
static void WritePortalFlags (const float* verts, dtPoly* polys, const int polyCount, const dtMeshHeader* sourceHeader);
static void WriteDetailMeshUsingHeightMesh (dtPolyDetail* detail, float* dverts, dtPolyDetailIndex* dtris
		, const DynamicMesh& mesh, const dtMeshTile* sourceTile, const int detailTriCount, const int detailVertCount);
static void WriteDetailMeshMixed (dtPolyDetail* detail, float* dverts, dtPolyDetailIndex* dtris
		, const DynamicMesh& mesh, const dtMeshTile* sourceTile, const int detailTriCount, const int detailVertCount);
static void WriteOffMeshLinks (dtOffMeshConnection* offMeshCons, dtPoly* polys, float* verts, int polyCount, int vertCount, const dtMeshTile* sourceTile);
static int SimplePolygonTriangulation (dtPolyDetail* dtl, dtPolyDetailIndex* dtris, int detailTriBase, const int polygonVertexCount);



// Converts detour navmesh tile to dynamic mesh format
bool TileToDynamicMesh (const dtMeshTile* tile, DynamicMesh& mesh, const Vector3f& tileOffset)
{
	if (!tile || !tile->header)
	{
		return false;
	}

	const int vertCount = tile->header->vertCount;
	const int polyCount = tile->header->polyCount;
	mesh.Reserve (vertCount, polyCount);

	for (int iv = 0; iv < vertCount; ++iv)
	{
		const Vector3f srcVertex(&tile->verts[3*iv]);
		mesh.AddVertex (srcVertex - tileOffset);
	}

	for (int ip = 0; ip < polyCount; ++ip)
	{
		const dtPoly& srcPoly = tile->polys[ip];
		if (srcPoly.getType () == DT_POLYTYPE_GROUND)
			mesh.AddPolygon (srcPoly.verts, ip, srcPoly.vertCount);
	}

	return true;
}

// Create tile in the format understood by the detour runtime.
// Polygons are converted from the dynamic mesh 'mesh'.
// Settings and static offmeshlinks are carried over from 'sourceTile'.
unsigned char* DynamicMeshToTile (int* dataSize, const DynamicMesh& mesh, const dtMeshTile* sourceTile, const Vector3f& tileOffset)
{
	// Determine data size
	DebugAssert (sourceTile);
	const int vertCount = mesh.VertCount ();
	const int polyCount = mesh.PolyCount ();
	const dtMeshHeader* sourceHeader = sourceTile->header;

	int polyEdgeCount = 0;
	for (int ip = 0; ip < polyCount; ++ip)
	{
		polyEdgeCount += mesh.GetPoly (ip)->m_VertexCount;
	}

	const int offMeshConCount = sourceHeader->offMeshConCount;
	const int totVertCount = vertCount + 2 * offMeshConCount;
	const int totPolyCount = polyCount + offMeshConCount;
	const int totLinkCount = polyEdgeCount + 4*offMeshConCount; // TODO: reserve for links to external offmeshlink connections

	const bool hasHeightMesh = sourceTile->header->flags & DT_MESH_HEADER_USE_HEIGHT_MESH;

	int detailVertCount = 0;
	int detailTriCount = 0;
	if (hasHeightMesh)
	{
		RequirementsForDetailMeshUsingHeightMesh (&detailVertCount, &detailTriCount, mesh, sourceTile);
	} else
	{
		RequirementsForDetailMeshMixed (&detailVertCount, &detailTriCount, mesh, sourceTile);
	}

	const unsigned int headSize = dtAlign4 (sizeof (dtMeshHeader));
	const unsigned int vertSize = dtAlign4 (totVertCount * 3*sizeof (float));
	const unsigned int polySize = dtAlign4 (totPolyCount * sizeof (dtPoly));
	const unsigned int linkSize = dtAlign4 (totLinkCount * sizeof (dtLink));
	const unsigned int detailMeshesSize = dtAlign4 (polyCount * sizeof (dtPolyDetail));
	const unsigned int detailVertsSize = dtAlign4 (detailVertCount * 3*sizeof (float));
	const unsigned int detailTrisSize = dtAlign4 (detailTriCount * 4*sizeof (dtPolyDetailIndex));
	const unsigned int bvTreeSize = 0;
	const unsigned int offMeshConsSize = dtAlign4 (offMeshConCount * sizeof (dtOffMeshConnection));

	const int newSize = headSize + vertSize + polySize + linkSize
		+ detailTrisSize + detailVertsSize + detailMeshesSize + bvTreeSize + offMeshConsSize;

	unsigned char* newTile = dtAllocArray<unsigned char> (newSize);
	if (newTile == NULL)
	{
		*dataSize = 0;
		return NULL;
	}
	*dataSize = newSize;
	memset (newTile, 0, newSize);

	// Serialize in the detour recognized format
	int offset = 0;
	dtMeshHeader* header = (dtMeshHeader*)(newTile+offset); offset += headSize;
	float* verts = (float*)(newTile+offset); offset += vertSize;
	dtPoly* polys = (dtPoly*)(newTile+offset); offset += polySize;
	/*dtLink* links = (dtLink*)(newTile+offset);*/ offset += linkSize;
	dtPolyDetail* detail = (dtPolyDetail*)(newTile+offset); offset += detailMeshesSize;
	float* dverts = (float*)(newTile+offset); offset += detailVertsSize;
	dtPolyDetailIndex* dtris = (dtPolyDetailIndex*)(newTile+offset); offset += detailTrisSize;
	/*dtBVNode* bvtree = (dtBVNode*)(newTile+offset);*/ offset += bvTreeSize;
	dtOffMeshConnection* offMeshCons = (dtOffMeshConnection*)(newTile+offset); offset += offMeshConsSize;
	DebugAssert (offset == newSize);

	for (int iv = 0; iv < vertCount; ++iv)
	{
		const float* v = mesh.GetVertex (iv);
		verts[3*iv+0] = v[0] + tileOffset.x;
		verts[3*iv+1] = v[1] + tileOffset.y;
		verts[3*iv+2] = v[2] + tileOffset.z;
	}

	for (int ip = 0; ip < polyCount; ++ip)
	{
		const DynamicMesh::Poly* p = mesh.GetPoly (ip);
		const int sourcePolyIndex = *mesh.GetData (ip);
		const dtPoly& srcPoly = sourceTile->polys[sourcePolyIndex];

		dtPoly& poly = polys[ip];
		memcpy (poly.verts, p->m_VertexIDs, DT_VERTS_PER_POLYGON*sizeof (UInt16));
		memcpy (poly.neis, p->m_Neighbours, DT_VERTS_PER_POLYGON*sizeof (UInt16));
		unsigned char area = srcPoly.getArea ();
		poly.flags = 1<<area;
		poly.setArea (area);
		poly.setType (DT_POLYTYPE_GROUND);
		poly.vertCount = p->m_VertexCount;
	}

	// Set external portal flags
	WritePortalFlags (verts, polys, polyCount, sourceHeader);

	if (hasHeightMesh)
	{
		WriteDetailMeshUsingHeightMesh (detail, dverts, dtris, mesh, sourceTile, detailTriCount, detailVertCount);
	} else
	{
		WriteDetailMeshMixed (detail, dverts, dtris, mesh, sourceTile, detailTriCount, detailVertCount);
	}

	// Fill in offmeshlink data from source tile: vertices, polygons, connection data.
	WriteOffMeshLinks (offMeshCons, polys, verts, polyCount, vertCount, sourceTile);

	// Copy values from source
	memcpy (header, sourceHeader, sizeof (*header));

	// (re)set new tile values
	header->polyCount = totPolyCount;
	header->vertCount = totVertCount;
	header->maxLinkCount = totLinkCount;
	header->detailMeshCount = polyCount;
	header->detailVertCount = detailVertCount;
	header->detailTriCount = detailTriCount;
	header->bvNodeCount = 0;                   // Fixme: bv-tree
	header->offMeshConCount = offMeshConCount;
	header->offMeshBase = polyCount;           // points beyond regular polygons.

	return newTile;
}

// Find vertex and triangle count needed when 'heightmesh' is enabled
static void RequirementsForDetailMeshUsingHeightMesh (int* detailVertCount, int* detailTriCount, const DynamicMesh& mesh, const dtMeshTile* sourceTile)
{
	// This is so bad - but until we change the detail mesh representation to
	// something more sane - we'll have to write code like this.
	int vertCount = 0;
	int triCount = 0;

	// collect sizes needed for detail mesh
	// detail mesh count is same as polyCount.
	// they're 1-to-1 with the regular polygons
	const int polyCount = mesh.PolyCount ();
	for (int ip = 0; ip < polyCount; ++ip)
	{
		const int sourcePolyIndex = *mesh.GetData (ip);
		const dtPolyDetail& sourceDetail = sourceTile->detailMeshes[sourcePolyIndex];

		vertCount += sourceDetail.vertCount;
		triCount += sourceDetail.triCount;
	}
	*detailVertCount = vertCount;
	*detailTriCount = triCount;
}

// Find vertex and triangle count needed for regular detailmesh
static void RequirementsForDetailMeshMixed (int* detailVertCount, int* detailTriCount, const DynamicMesh& mesh, const dtMeshTile* sourceTile)
{
	int vertCount = 0;
	int triCount = 0;

	// Collect sizes needed for detail mesh
	const int polyCount = mesh.PolyCount ();
	for (int ip = 0; ip < polyCount; ++ip)
	{
		const DynamicMesh::Poly* p = mesh.GetPoly (ip);
		const int sourcePolyIndex = *mesh.GetData (ip);

		if (p->m_Status == DynamicMesh::kOriginalPolygon)
		{
			// When preserving polygon detail mesh just add the source counts
			const dtPolyDetail& sourceDetail = sourceTile->detailMeshes[sourcePolyIndex];
			vertCount += sourceDetail.vertCount;
			triCount += sourceDetail.triCount;
		}
		else
		{
			// Simple triangulation needs n-2 triangles but no extra detail vertices
			triCount += p->m_VertexCount - 2;
		}
	}
	*detailVertCount = vertCount;
	*detailTriCount = triCount;
}

// Set flags on polygon edges colinear to tile edges.
// Flagged edges are considered when dynamically stitching neighboring tiles.
static void WritePortalFlags (const float* verts, dtPoly* polys, const int polyCount, const dtMeshHeader* sourceHeader)
{
	const float* bmax = sourceHeader->bmax;
	const float* bmin = sourceHeader->bmin;
	for (int ip = 0; ip < polyCount; ++ip)
	{
		dtPoly& poly = polys[ip];
		for (int iv = 0; iv < poly.vertCount; ++iv)
		{
			// Skip already connected edges
			if (poly.neis[iv] != 0)
				continue;

			const float* vert = &verts[3 * poly.verts[iv]];
			const int ivn = (iv+1 == poly.vertCount) ? 0 : iv+1;
			const float* nextVert = &verts[3 * poly.verts[ivn]];

			unsigned short nei = 0;

			if (dtMax (dtAbs (vert[0] - bmax[0]), dtAbs (nextVert[0] - bmax[0])) < MAGIC_EDGE_DISTANCE)
				nei = DT_EXT_LINK | 0; // x+ portal
			else if (dtMax (dtAbs (vert[2] - bmax[2]), dtAbs (nextVert[2] - bmax[2])) < MAGIC_EDGE_DISTANCE)
				nei = DT_EXT_LINK | 2; // z+ portal
			else if (dtMax (dtAbs (vert[0] - bmin[0]), dtAbs (nextVert[0] - bmin[0])) < MAGIC_EDGE_DISTANCE)
				nei = DT_EXT_LINK | 4; // x- portal
			else if (dtMax (dtAbs (vert[2] - bmin[2]), dtAbs (nextVert[2] - bmin[2])) < MAGIC_EDGE_DISTANCE)
				nei = DT_EXT_LINK | 6; // z- portal

			poly.neis[iv] = nei;
		}
	}
}

// Populate the tile with detail mesh. For the case where 'heightmesh' is enabled.
static void WriteDetailMeshUsingHeightMesh (dtPolyDetail* detail, float* dverts, dtPolyDetailIndex* dtris
		, const DynamicMesh& mesh, const dtMeshTile* sourceTile
		, const int detailTriCount, const int detailVertCount)
{
	int detailVertBase = 0;
	int detailTriBase = 0;

	const int polyCount = mesh.PolyCount ();
	for (int ip = 0; ip < polyCount; ++ip)
	{
		const int sourcePolyIndex = *mesh.GetData (ip);
		const dtPolyDetail& sourceDetail = sourceTile->detailMeshes[sourcePolyIndex];

		detail[ip].vertBase = detailVertBase;
		detail[ip].triBase = detailTriBase;
		detail[ip].triCount = sourceDetail.triCount;
		detail[ip].vertCount = sourceDetail.vertCount;

		// Detail vertex indices are corrected by the poly vertex count
		// adjust for this peculiarity.
		// NOTE: It causes detail meshes to by unusable by multiple polygons !
		const int oldPolyVertexCount = sourceTile->polys[sourcePolyIndex].vertCount;
		const int newPolyVertexCount = mesh.GetPoly (ip)->m_VertexCount;
		const int vertexDelta = newPolyVertexCount - oldPolyVertexCount;

		for (int iv = 0; iv < sourceDetail.vertCount; ++iv)
		{
			dverts[3*(detailVertBase + iv) + 0] = sourceTile->detailVerts[3*(sourceDetail.vertBase + iv) + 0];
			dverts[3*(detailVertBase + iv) + 1] = sourceTile->detailVerts[3*(sourceDetail.vertBase + iv) + 1];
			dverts[3*(detailVertBase + iv) + 2] = sourceTile->detailVerts[3*(sourceDetail.vertBase + iv) + 2];
		}

		for (int it = 0; it < sourceDetail.triCount; ++it)
		{
			dtris[4*(detailTriBase + it) + 0] = sourceTile->detailTris[4*(sourceDetail.triBase + it) + 0] + vertexDelta;
			dtris[4*(detailTriBase + it) + 1] = sourceTile->detailTris[4*(sourceDetail.triBase + it) + 1] + vertexDelta;
			dtris[4*(detailTriBase + it) + 2] = sourceTile->detailTris[4*(sourceDetail.triBase + it) + 2] + vertexDelta;
			dtris[4*(detailTriBase + it) + 3] = 0;
		}

		detailVertBase += sourceDetail.vertCount;
		detailTriBase += sourceDetail.triCount;
	}
	DebugAssert (detailVertBase == detailVertCount);
	DebugAssert (detailTriBase == detailTriCount);
}

// Mix preserved detail mesh for untouched polygons with simple triangulation for generated polygons
static void WriteDetailMeshMixed (dtPolyDetail* detail, float* dverts, dtPolyDetailIndex* dtris
		, const DynamicMesh& mesh, const dtMeshTile* sourceTile
		, const int detailTriCount, const int detailVertCount)
{
	int detailVertBase = 0;
	int detailTriBase = 0;

	const int polyCount = mesh.PolyCount ();
	for (int ip = 0; ip < polyCount; ++ip)
	{
		dtPolyDetail& dtl = detail[ip];
		const DynamicMesh::Poly* p = mesh.GetPoly (ip);

		if (p->m_Status == DynamicMesh::kOriginalPolygon)
		{
			// Fill in the original detail mesh for this polygon
			const int sourcePolyIndex = *mesh.GetData (ip);
			const dtPolyDetail& sourceDetail = sourceTile->detailMeshes[sourcePolyIndex];
			dtl.vertBase = detailVertBase;
			dtl.vertCount = sourceDetail.vertCount;
			dtl.triBase = detailTriBase;
			dtl.triCount = sourceDetail.triCount;

			// copy source detail vertices and triangles
			memcpy (&dverts[3*detailVertBase], &sourceTile->detailVerts[3*sourceDetail.vertBase], 3*sizeof (float)*sourceDetail.vertCount);
			memcpy (&dtris[4*detailTriBase], &sourceTile->detailTris[4*sourceDetail.triBase], 4*sizeof (dtPolyDetailIndex)*sourceDetail.triCount);

			detailVertBase += sourceDetail.vertCount;
			detailTriBase += sourceDetail.triCount;
		}
		else
		{
			detailTriBase = SimplePolygonTriangulation (&dtl, dtris, detailTriBase, p->m_VertexCount);
		}
	}
	DebugAssert (detailTriBase == detailTriCount);
	DebugAssert (detailVertBase == detailVertCount);
}

// Populate the tile with static offmesh links from the source tile.
static void WriteOffMeshLinks (dtOffMeshConnection* offMeshCons, dtPoly* polys, float* verts, int polyCount, int vertCount, const dtMeshTile* sourceTile)
{
	const dtMeshHeader* sourceHeader = sourceTile->header;
	const int offMeshConCount = sourceHeader->offMeshConCount;
	if (offMeshConCount)
	{
		memcpy (&polys[polyCount], &sourceTile->polys[sourceHeader->offMeshBase], offMeshConCount * sizeof (dtPoly));
		memcpy (offMeshCons, sourceTile->offMeshCons, offMeshConCount * sizeof (dtOffMeshConnection));

		// Vertex base for offmeshlinks is not stored in tile header
		// Here we assume that offmeshlink vertices are stored as the section of vertices
		// and that each offmeshlink stores two vertices.
		const int sourceOffMeshVertBase = sourceHeader->vertCount - 2*sourceHeader->offMeshConCount;

		memcpy (&verts[3*vertCount], &sourceTile->verts[3*sourceOffMeshVertBase], 2*3*sizeof (float));

		// Fixup internal index references
		for (int i = 0; i < offMeshConCount; ++i)
		{
			// Vertex indices
			dtPoly* poly = & polys[polyCount + i];
			poly->verts[0] = (unsigned short)(vertCount + 2*i+0);
			poly->verts[1] = (unsigned short)(vertCount + 2*i+1);

			// Polygon index
			dtOffMeshConnection* con = &offMeshCons[i];
			con->poly = (unsigned short) (polyCount + i);
		}
	}
}

static int SimplePolygonTriangulation (dtPolyDetail* dtl, dtPolyDetailIndex* dtris, int detailTriBase, const int polygonVertexCount)
{
	dtl->vertBase = 0;
	dtl->vertCount = 0;
	dtl->triBase = (unsigned int)detailTriBase;
	dtl->triCount = (dtPolyDetailIndex)(polygonVertexCount-2);

	// Triangulate polygon (local indices).
	for (int j = 2; j < polygonVertexCount; ++j)
	{
		dtPolyDetailIndex* t = &dtris[4*detailTriBase];
		t[0] = 0;
		t[1] = (dtPolyDetailIndex)(j-1);
		t[2] = (dtPolyDetailIndex)j;
		// Bit for each edge that belongs to poly boundary.
		t[3] = (1<<2);
		if (j == 2) t[3] |= (1<<0);
		if (j == polygonVertexCount-1) t[3] |= (1<<4);
		detailTriBase++;
	}
	return detailTriBase;
}
