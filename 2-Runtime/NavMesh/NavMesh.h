#pragma once
#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Math/Vector3.h"
#include "NavMeshTypes.h"
#include "HeightmapData.h"


class dtNavMesh;
class dtNavMeshQuery;
class dtQueryFilter;
class NavMeshPath;
class HeightMeshQuery;

class NavMesh : public NamedObject
{
public:

	struct Triangulation
	{
		dynamic_array<SInt32> layers;
		dynamic_array<SInt32> indices;
		dynamic_array<Vector3f> vertices;
	};

	REGISTER_DERIVED_CLASS (NavMesh, NamedObject);
	DECLARE_OBJECT_SERIALIZE (NavMesh);

	NavMesh (MemLabelId& label, ObjectCreationMode mode);
	void Create ();

	bool Raycast (NavMeshHit* hit, const Vector3f& sourcePosition, const Vector3f& targetPosition, const dtQueryFilter& filter);
	bool DistanceToEdge (NavMeshHit* hit, const Vector3f& sourcePosition, const dtQueryFilter& filter);
	bool SamplePosition (NavMeshHit* hit, const Vector3f& sourcePosition, const dtQueryFilter& filter, float maxDistance);

	int CalculatePolygonPath (NavMeshPath* path, const Vector3f& sourcePosition, const Vector3f& targetPosition, const dtQueryFilter& filter);
	int CalculatePathCorners (Vector3f* corners, int maxCorners, const NavMeshPath& path);

	void Triangulate (NavMesh::Triangulation& triangulation) const;

	virtual void AwakeFromLoad (AwakeFromLoadMode mode);

	void SetData (const UInt8* data, unsigned size);

	inline const UInt8* GetMeshData () const;
	inline size_t GetMeshDataSize () const;
	inline const HeightmapDataVector& GetHeightmaps () const;
	inline void SetHeightmaps (HeightmapDataVector& heightmaps);
	inline const dtNavMesh* GetInternalNavMesh () const;
	inline dtNavMesh*      GetInternalNavMesh ();
	inline dtNavMeshQuery* GetInternalNavMeshQuery ();
	inline const HeightMeshQuery* GetHeightMeshQuery () const;

private:
	bool MapPosition (dtPolyRef* mappedPolyRef, Vector3f* mappedPosition, const Vector3f& position, const Vector3f& extents, const dtQueryFilter& filter) const;

	void Cleanup ();
	void CleanupWithError ();

	Vector3f m_QueryExtents;
	dynamic_array<UInt8> m_MeshData;
	HeightmapDataVector m_Heightmaps;

	dtNavMesh* m_NavMesh;
	dtNavMeshQuery* m_NavMeshQuery;
	HeightMeshQuery* m_HeightMeshQuery;
};

inline const UInt8* NavMesh::GetMeshData () const
{
	return m_MeshData.begin ();
}

inline size_t NavMesh::GetMeshDataSize () const
{
	return m_MeshData.size ();
}

inline const HeightmapDataVector& NavMesh::GetHeightmaps () const
{
	return m_Heightmaps;
}

inline void NavMesh::SetHeightmaps (HeightmapDataVector& heightmaps)
{
	m_Heightmaps = heightmaps;
}


inline const dtNavMesh* NavMesh::GetInternalNavMesh () const
{
	return m_NavMesh;
}

inline dtNavMesh* NavMesh::GetInternalNavMesh ()
{
	return m_NavMesh;
}

inline dtNavMeshQuery* NavMesh::GetInternalNavMeshQuery ()
{
	return m_NavMeshQuery;
}

inline const HeightMeshQuery* NavMesh::GetHeightMeshQuery () const
{
	return m_HeightMeshQuery;
}

void InvalidateNavMeshHit (NavMeshHit* hit);
