#include "UnityPrefix.h"
#include "NavMesh.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "DetourNavMeshQuery.h"
#include "NavMeshManager.h"
#include "NavMeshPath.h"
#include "NavMeshLayers.h"
#include "DetourAlloc.h"
#include "DetourSwapEndian.h"
#include "HeightMeshQuery.h"

/*
 TODO:

 1) Tile boundary coordinates depend on the global bounding box (worldMin + n*size) - make it independent of the global extents (eg. use origo).

 2) make remainingDistance return distance in all the time (not infinity).

 3) NavMesh baking bounding box and seed points to cull poly-count.

 4) Support non-cylinder avoidance obstacles (eg. box).

 5) Support cylinder shaped obstacle carving.

 6) Line-to-line offmeshlink type (in addition to the current point-to-point)

*/

NavMesh::NavMesh (MemLabelId& label, ObjectCreationMode mode)
: Super (label, mode)
, m_NavMesh (NULL)
, m_NavMeshQuery (NULL)
, m_HeightMeshQuery (NULL)
{
}

NavMesh::~NavMesh ()
{
	Cleanup ();
}

int NavMesh::CalculatePolygonPath (NavMeshPath* path, const Vector3f& sourcePosition, const Vector3f& targetPosition, const dtQueryFilter& filter)
{
	if (m_NavMeshQuery==NULL)
		return 0;

	float targetMappedPos[3];
	float sourceMappedPos[3];
	dtNavMeshQuery* query = m_NavMeshQuery;
	const float* ext = m_QueryExtents.GetPtr ();

	dtPolyRef targetPolyRef;
	query->findNearestPoly (targetPosition.GetPtr (), ext, &filter, &targetPolyRef, targetMappedPos);
	if (targetPolyRef == 0)
		return 0;

	dtPolyRef sourcePolyRef;
	query->findNearestPoly (sourcePosition.GetPtr (), ext, &filter, &sourcePolyRef, sourceMappedPos);
	if (sourcePolyRef == 0)
		return 0;

	int polygonCount = 0;

	// TODO: Cache an up-to-date filter in navmesh (or manager) to avoid this copy
	dtQueryFilter filter2 = filter;
	const NavMeshLayers& layers = GetNavMeshLayers ();
	for (int i = 0; i < NavMeshLayers::kLayerCount; ++i)
		filter2.setAreaCost (i, layers.GetLayerCost (i));

	dtStatus status = query->initSlicedFindPath (sourcePolyRef, targetPolyRef, sourceMappedPos, targetMappedPos, &filter2);
	if (!dtStatusFailed (status))
		status = query->updateSlicedFindPath (65535, NULL);
	if (!dtStatusFailed (status))
		status = query->finalizeSlicedFindPath (path->GetPolygonPath (), &polygonCount, NavMeshPath::kMaxPathPolygons);

	path->SetTimeStamp (m_NavMesh->getTimeStamp ());
	path->SetPolygonCount (polygonCount);
	path->SetSourcePosition (Vector3f (sourceMappedPos));
	path->SetTargetPosition (Vector3f (targetMappedPos));
	if (dtStatusFailed (status) || polygonCount == 0)
	{
		path->SetStatus (kPathInvalid);
		return 0;
	}

	if (dtStatusDetail (status, DT_PARTIAL_RESULT))
	{
		// when path is partial we project the target position
		// to the last polygon in the path.

		const dtPolyRef* polygonPath = path->GetPolygonPath ();
		const dtPolyRef lastPolyRef = polygonPath[polygonCount-1];
		Vector3f partialTargetPos;
		dtStatus status = query->closestPointOnPoly (lastPolyRef, targetMappedPos, partialTargetPos.GetPtr ());
		if (dtStatusFailed (status))
		{
			path->SetStatus (kPathInvalid);
			return 0;
		}

		path->SetStatus (kPathPartial);
		path->SetTargetPosition (partialTargetPos);
	}
	else
	{
		path->SetStatus (kPathComplete);
	}

	return polygonCount;
}

int NavMesh::CalculatePathCorners (Vector3f* corners, int maxCorners, const NavMeshPath& path)
{
	if (m_NavMeshQuery==NULL)
		return 0;

	const dtNavMeshQuery* query = m_NavMeshQuery;

	int cornerCount = 0;
	dtStatus result;
	Vector3f sourcePos = path.GetSourcePosition ();
	Vector3f targetPos = path.GetTargetPosition ();

	result = query->findStraightPath (sourcePos.GetPtr (), targetPos.GetPtr (),
									 path.GetPolygonPath (), path.GetPolygonCount (),
									 corners[0].GetPtr (), NULL, NULL, &cornerCount, maxCorners);
	if (result != DT_SUCCESS)
		return 0;
	return cornerCount;
}

void NavMesh::Triangulate (NavMesh::Triangulation& triangulation) const
{
	// Mapping between old and new vertex indices.
	typedef std::map<UInt16, SInt32> VertexMap;

	dynamic_array<SInt32>& layers = triangulation.layers;
	dynamic_array<SInt32>& indices = triangulation.indices;
	dynamic_array<Vector3f>& vertices = triangulation.vertices;

	indices.clear ();
	vertices.clear ();

	const size_t tileCount = m_NavMesh->tileCount ();
	for (size_t it = 0; it < tileCount; ++it)
	{
		const dtMeshTile* tile = m_NavMesh->getTile (it);
		if (tile == NULL || tile->header == NULL)
			continue;

		for (size_t ip = 0; ip < tile->header->polyCount; ++ip)
		{
			const dtPoly& p = tile->polys[ip];
			const size_t polyVertCount = p.vertCount;

			// Ignore irregular polygons.
			if (polyVertCount < 3)
				continue;

			VertexMap vertexMap;

			// Find or update new vertex index.
			for (size_t iv = 0; iv < polyVertCount; ++iv)
			{
				UInt16 vi = p.verts[iv];
				VertexMap::iterator found = vertexMap.find (vi);
				if (found != vertexMap.end ())
					continue;

				// Lookup the height for vertex
				dtPolyRef ref = m_NavMesh->getPolyRefBase (tile)|(dtPolyRef)ip;
				Vector3f vertex = Vector3f (&tile->verts[3*vi]);
				float ypos;
				m_NavMeshQuery->getPolyHeight (ref, vertex.GetPtr (), &ypos);
				vertex.y = ypos;

				vertexMap[vi] = vertices.size ();
				vertices.push_back (vertex);
			}

			// Add triangles for this polygon.
			const SInt32 v0 = vertexMap[p.verts[0]];
			SInt32 v1 = vertexMap[p.verts[1]];
			for (size_t iv = 2; iv < polyVertCount; ++iv)
			{
				const SInt32 v2 = vertexMap[p.verts[iv]];
				indices.push_back (v0);
				indices.push_back (v1);
				indices.push_back (v2);

				v1 = v2;
			}

			// Add the navmesh layer for each triangle
			for (size_t iv = 2; iv < polyVertCount; ++iv)
			{
				layers.push_back (p.getArea ());
			}
		}
	}
}

bool NavMesh::Raycast (NavMeshHit* hit, const Vector3f& sourcePosition, const Vector3f& targetPosition, const dtQueryFilter& filter)
{
	dtPolyRef mappedPolyRef;
	Vector3f mappedPosition;
	if (!MapPosition (&mappedPolyRef, &mappedPosition, sourcePosition, m_QueryExtents, filter))
	{
		InvalidateNavMeshHit (hit);
		return false;
	}

	float st;
	float hitNormal[3];
	unsigned int hitPolyFlags;
	float height;
	dtStatus result = m_NavMeshQuery->simpleRaycast (mappedPolyRef, mappedPosition.GetPtr (), targetPosition.GetPtr (), &filter, &st, hitNormal, &hitPolyFlags, NULL, &height);
	if (dtStatusFailed (result))
	{
		InvalidateNavMeshHit (hit);
		return false;
	}

	if (st<1.0f)
	{
		hit->mask = hitPolyFlags;
		hit->hit = true;
		hit->position = Lerp (mappedPosition, targetPosition, st);
	}
	else
	{
		hit->mask = 0;
		hit->hit = false;
		hit->position = targetPosition;
	}

	hit->position.y = height;
	hit->distance = Magnitude (hit->position - sourcePosition);
	hit->normal = Vector3f (hitNormal);

	return hit->hit;
}

bool NavMesh::SamplePosition (NavMeshHit* hit, const Vector3f& sourcePosition, const dtQueryFilter& filter, float maxDistance)
{
	dtPolyRef mappedPolyRef;
	Vector3f mappedPosition;
	const Vector3f extents = Vector3f (maxDistance, maxDistance, maxDistance);
	if (!MapPosition (&mappedPolyRef, &mappedPosition, sourcePosition, extents, filter))
	{
		InvalidateNavMeshHit (hit);
		return false;
	}

	const float distance = Magnitude (mappedPosition - sourcePosition);
	if (distance > maxDistance)
	{
		InvalidateNavMeshHit (hit);
		return false;
	}

	hit->mask = m_NavMeshQuery->getPolygonFlags (mappedPolyRef);
	hit->hit = true;
	hit->position = mappedPosition;
	hit->distance = distance;
	hit->normal = Vector3f::zero;
	return true;
}

bool NavMesh::DistanceToEdge (NavMeshHit* hit, const Vector3f& sourcePosition, const dtQueryFilter& filter)
{
	dtPolyRef mappedPolyRef;
	Vector3f mappedPosition;
	if (!MapPosition (&mappedPolyRef, &mappedPosition, sourcePosition, m_QueryExtents, filter))
	{
		InvalidateNavMeshHit (hit);
		return false;
	}

	const dtStatus status = m_NavMeshQuery->findEdge (mappedPolyRef, mappedPosition.GetPtr (), &filter, &hit->mask, hit->normal.GetPtr (), hit->position.GetPtr ());
	if (dtStatusFailed (status))
	{
		InvalidateNavMeshHit (hit);
		return false;
	}
	hit->distance = Magnitude (hit->position - sourcePosition);
	hit->hit = true;

	return true;
}

bool NavMesh::MapPosition (dtPolyRef* mappedPolyRef, Vector3f* mappedPosition, const Vector3f& position, const Vector3f& extents, const dtQueryFilter& filter) const
{
	if (!m_NavMeshQuery)
		return false;

	m_NavMeshQuery->findNearestPoly (position.GetPtr (), extents.GetPtr (), &filter, mappedPolyRef, mappedPosition->GetPtr ());
	const dtPolyRef ref = *mappedPolyRef;
	return ref != 0;
}

void NavMesh::Create ()
{
	Cleanup ();

	if (m_MeshData.empty ())
		return;

	Assert (m_NavMesh == NULL);
	m_NavMesh = dtAllocNavMesh ();
	if (!m_NavMesh)
		return CleanupWithError ();

	dtStatus status;

	// try loading tile data
	status = m_NavMesh->initTiles (GetMeshData (), GetMeshDataSize ());
	if (dtStatusFailed (status))
		return CleanupWithError ();
	if (m_NavMesh->tileCount () == 0)
		return Cleanup ();

	////@TODO: WTF IS THIS MAGIC NUMBER???
	Assert (m_NavMeshQuery == NULL);
	m_NavMeshQuery = dtAllocNavMeshQuery (m_NavMesh, 2048);
	if (m_NavMeshQuery == NULL)
		return CleanupWithError ();

	// @TODO: remove tile header redundancy and enforce identical tile parameters.
	const dtMeshTile* tile = m_NavMesh->getTile (0);
	if (tile && tile->header)
	{
		const float height = tile->header->walkableHeight;
		const float width = tile->header->walkableRadius;
		m_QueryExtents = Vector3f (width, height, width);

		// Set up HeightMeshQuery object if the baked data needs it
		const bool useHeightMeshQuery = (tile->header->flags & DT_MESH_HEADER_USE_HEIGHT_MESH) || !m_Heightmaps.empty ();
		if (useHeightMeshQuery)
		{
			if (!m_HeightMeshQuery)
			{
				m_HeightMeshQuery = UNITY_NEW (HeightMeshQuery, kMemNavigation) ();
				m_HeightMeshQuery->Init (&m_Heightmaps, 1.05f*tile->header->walkableClimb);
			}
			m_NavMeshQuery->setHeightQuery (m_HeightMeshQuery);
		}
		else
		{
			UNITY_DELETE (m_HeightMeshQuery, kMemNavigation);
			m_NavMeshQuery->setHeightQuery (NULL);
		}
	}
	else
	{
		m_QueryExtents = Vector3f (1,1,1);
	}
}

void NavMesh::CleanupWithError ()
{
	ErrorString ("Creating NavMesh failed");
	Cleanup ();
}

void NavMesh::Cleanup ()
{
	GetNavMeshManager ().CleanupMeshDependencies (m_NavMesh);
	if (m_NavMesh)
	{
		dtFreeNavMesh (m_NavMesh);
		m_NavMesh = NULL;
	}
	if (m_NavMeshQuery)
	{
		dtFreeNavMeshQuery (m_NavMeshQuery);
		m_NavMeshQuery = NULL;
	}
	if (m_HeightMeshQuery)
	{
		UNITY_DELETE (m_HeightMeshQuery, kMemNavigation);
	}
}


void NavMesh::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad (mode);
	Create ();
}

void NavMesh::SetData (const UInt8* data, unsigned size)
{
	m_MeshData.assign (data, data + size);
	Create ();
}


template<class TransferFunction> inline
void TransferMeshDataByteSwap (TransferFunction& transfer, dynamic_array<UInt8>& data)
{
	if (transfer.IsWriting ())
	{
		dynamic_array<UInt8> copy = data;
		if (!copy.empty ())
		{
			ErrorIf (!dtNavMeshSetSwapEndian (&copy[0], copy.size ()));
		}
		transfer.Transfer (copy, "m_MeshData");
		return;
	}

	transfer.Transfer (data, "m_MeshData");

	if (transfer.IsReading () && !data.empty ())
	{
		ErrorIf (!dtNavMeshSetSwapEndian (&data[0], data.size ()));
	}
}

template<class TransferFunction>
void NavMesh::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	if (!transfer.ConvertEndianess ())
		transfer.Transfer (m_MeshData, "m_MeshData");
	else
		TransferMeshDataByteSwap (transfer, m_MeshData);

	TRANSFER (m_Heightmaps);
}

IMPLEMENT_OBJECT_SERIALIZE (NavMesh)
IMPLEMENT_CLASS (NavMesh)


void InvalidateNavMeshHit (NavMeshHit* hit)
{
	const float maxDistance = std::numeric_limits<float>::infinity ();
	hit->mask = 0;
	hit->hit = false;
	hit->position = Vector3f::infinityVec;
	hit->distance = maxDistance;
	hit->normal = Vector3f::zero;
}
