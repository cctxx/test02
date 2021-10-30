#include "UnityPrefix.h"
#include "NavMeshTileCarving.h"
#include "NavMeshTileConversion.h"
#include "DynamicMesh.h"
#include "DetourNavMesh.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Geometry/AABB.h"
#include "DetourCommon.h"
#include "DetourAlloc.h"

// TODO:
// Sort carving objects spatially - so carving order does not depend on carve index

static inline Vector3f TileMidpoint (const dtMeshHeader* tileHeader);
static bool CalculateHull (DynamicMesh::Hull& carveHull, const Matrix4x4f& transform, const Vector3f& size, const MinMaxAABB& aabb, const Vector3f& tileOffset, const float carveWidth, const float carveDepth);
static Vector3f CalculateCarveOffsetScale (const Vector3f& axis);

// Replaces a single tile in the detour navmesh with a carved tile.
void CarveNavMeshTile (const dtMeshTile* tile, dtNavMesh* navmesh, size_t count, const Matrix4x4f* transforms, const Vector3f* sizes, const MinMaxAABB* aabbs)
{
	if (count == 0 || tile == NULL || tile->header == NULL)
	{
		return;
	}

	const Vector3f tileOffset = TileMidpoint (tile->header);
	const float carveWidth = tile->header->walkableRadius;
	const float carveDepth = tile->header->walkableHeight;

	DynamicMesh::HullContainer carveHulls;
	for (size_t i = 0; i < count; ++i)
	{
		DynamicMesh::Hull carveHull;
		if (CalculateHull (carveHull, transforms[i], sizes[i], aabbs[i], tileOffset, carveWidth, carveDepth))
		{
			carveHulls.push_back (carveHull);
		}
	}

	DynamicMesh dynamicMesh;
	if (!TileToDynamicMesh (tile, dynamicMesh, tileOffset))
	{
		return;
	}
	if (!dynamicMesh.ClipPolys (carveHulls))
	{
		return;
	}

	dynamicMesh.FindNeighbors ();

	int newTileSize = 0;
	unsigned char* newTile = DynamicMeshToTile (&newTileSize, dynamicMesh, tile, tileOffset);

	dtPolyRef tileRef = navmesh->getTileRef (tile);
	navmesh->removeTile (tileRef, 0, 0);
	dtStatus status = navmesh->addTile (newTile, newTileSize, DT_TILE_FREE_DATA, tileRef, 0);
	if (dtStatusFailed (status))
	{
		dtFree (newTile);
	}
}

static inline Vector3f TileMidpoint (const dtMeshHeader* tileHeader)
{
	if (!tileHeader)
	{
		return Vector3f::zero;
	}
	return 0.5f * (Vector3f (tileHeader->bmin) + Vector3f (tileHeader->bmax));
}

static inline bool AreColinear (const Vector3f& v, const Vector3f& u, const float cosAngleAccept)
{
	DebugAssert (IsNormalized (v));
	DebugAssert (IsNormalized (u));
	return Abs (Dot (v, u)) > cosAngleAccept;
}

// Compute the set of planes defining an extruded bounding box.
// Bounding box is represented by transform and size.
// Extrusion is based on 'carveWidth' horizontally and 'carveDepth' vertically down.
// Everyting is translated relative to 'tileOffset'.
static bool CalculateHull (DynamicMesh::Hull& carveHull, const Matrix4x4f& transform, const Vector3f& size, const MinMaxAABB& aabb, const Vector3f& tileOffset, const float carveWidth, const float carveDepth)
{
	carveHull.resize_uninitialized (12);

	const float cosAngleConsiderAxisAligned = Cos (Deg2Rad (10.0f)); // Consider colinear if within 10 degrees
	bool isAlmostAxisAlignedX = false;
	bool isAlmostAxisAlignedY = false;
	bool isAlmostAxisAlignedZ = false;

	// First add the six planes from the OBB
	const Vector3f carveLocalOffset = Vector3f (carveWidth, carveDepth, carveWidth);
	const Vector3f position = transform.GetPosition () - tileOffset;
	Vector3f axis, offset, planeOffset;

	axis = transform.GetAxisX ();
	if (CompareApproximately (axis, Vector3f::zero, 0.0001f))
	{
		return false;
	}
	offset = size.x * axis;
	axis = Normalize (axis);
	planeOffset = Scale (CalculateCarveOffsetScale (-axis), carveLocalOffset) - offset;
	carveHull[0].SetNormalAndPosition (-axis, position + planeOffset);
	planeOffset = Scale (CalculateCarveOffsetScale (axis), carveLocalOffset) + offset;
	carveHull[1].SetNormalAndPosition (axis, position + planeOffset);
	isAlmostAxisAlignedX = isAlmostAxisAlignedX || AreColinear (axis, Vector3f::xAxis, cosAngleConsiderAxisAligned);
	isAlmostAxisAlignedY = isAlmostAxisAlignedY || AreColinear (axis, Vector3f::yAxis, cosAngleConsiderAxisAligned);
	isAlmostAxisAlignedZ = isAlmostAxisAlignedZ || AreColinear (axis, Vector3f::zAxis, cosAngleConsiderAxisAligned);

	axis = transform.GetAxisY ();
	if (CompareApproximately (axis, Vector3f::zero, 0.0001f))
	{
		return false;
	}
	offset = size.y * axis;
	axis = Normalize (axis);
	planeOffset = Scale (CalculateCarveOffsetScale (-axis), carveLocalOffset) - offset;
	carveHull[2].SetNormalAndPosition (-axis, position + planeOffset);
	planeOffset = Scale (CalculateCarveOffsetScale (axis), carveLocalOffset) + offset;
	carveHull[3].SetNormalAndPosition (axis, position + planeOffset);
	isAlmostAxisAlignedX = isAlmostAxisAlignedX || AreColinear (axis, Vector3f::xAxis, cosAngleConsiderAxisAligned);
	isAlmostAxisAlignedY = isAlmostAxisAlignedY || AreColinear (axis, Vector3f::yAxis, cosAngleConsiderAxisAligned);
	isAlmostAxisAlignedZ = isAlmostAxisAlignedZ || AreColinear (axis, Vector3f::zAxis, cosAngleConsiderAxisAligned);

	axis = transform.GetAxisZ ();
	if (CompareApproximately (axis, Vector3f::zero, 0.0001f))
	{
		return false;
	}
	offset = size.z * axis;
	axis = Normalize (axis);
	planeOffset = Scale (CalculateCarveOffsetScale (-axis), carveLocalOffset) - offset;
	carveHull[4].SetNormalAndPosition (-axis, position + planeOffset);
	planeOffset = Scale (CalculateCarveOffsetScale (axis), carveLocalOffset) + offset;
	carveHull[5].SetNormalAndPosition (axis, position + planeOffset);
	isAlmostAxisAlignedX = isAlmostAxisAlignedX || AreColinear (axis, Vector3f::xAxis, cosAngleConsiderAxisAligned);
	isAlmostAxisAlignedY = isAlmostAxisAlignedY || AreColinear (axis, Vector3f::yAxis, cosAngleConsiderAxisAligned);
	isAlmostAxisAlignedZ = isAlmostAxisAlignedZ || AreColinear (axis, Vector3f::zAxis, cosAngleConsiderAxisAligned);

	int planeCount = 6;

	// The add the six planes from the containing AABB
	const Vector3f min = aabb.m_Min - tileOffset;
	const Vector3f max = aabb.m_Max - tileOffset;
	if (!isAlmostAxisAlignedX)
	{
		carveHull[planeCount++].SetNormalAndPosition (-Vector3f::xAxis, min - carveWidth*Vector3f::xAxis);
		carveHull[planeCount++].SetNormalAndPosition (Vector3f::xAxis, max + carveWidth*Vector3f::xAxis);
	}
	if (!isAlmostAxisAlignedY)
	{
		carveHull[planeCount++].SetNormalAndPosition (-Vector3f::yAxis, min - carveDepth*Vector3f::yAxis);
		carveHull[planeCount++].SetNormalAndPosition (Vector3f::yAxis, max);
	}
	if (!isAlmostAxisAlignedZ)
	{
		carveHull[planeCount++].SetNormalAndPosition (-Vector3f::zAxis, min - carveWidth*Vector3f::zAxis);
		carveHull[planeCount++].SetNormalAndPosition (Vector3f::zAxis, max + carveWidth*Vector3f::zAxis);
	}

	carveHull.resize_uninitialized (planeCount);
	return true;
}

// Returns the general plane offset direction given the plane axis
static Vector3f CalculateCarveOffsetScale (const Vector3f& axis)
{
	Vector3f res;
	res.x = Sign (axis.x);
	res.y = std::min (0.0f, Sign (axis.y));
	res.z = Sign (axis.z);
	return res;
}
