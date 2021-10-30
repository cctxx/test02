#ifndef _DYNAMICMESH_H_INCLUDED_
#define _DYNAMICMESH_H_INCLUDED_

#include "Runtime/Math/Vector3.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Utilities/dynamic_array.h"
#include <vector>

// TODO handle T-junctions (produced by merging / culling degenerate polys).
// TODO cleanup orphan vertices (remap indices)
// TODO optimize using bv-tree to collect source polygons for carving
// TODO re-create bv-tree for faster lookup (possibly by modifying source bv-tree).

class DynamicMesh
{
	struct Edge
	{
		UInt16 v1, v2, p1, p2, c1, c2;
	};
	typedef dynamic_array< Edge > EdgeList;
public:

	enum
	{
		NUM_VERTS = 6
	};
	enum
	{
		kOriginalPolygon = 0,
		kGeneratedPolygon = 1
	};
	struct Poly
	{
		UInt16 m_Neighbours[NUM_VERTS];
		UInt16 m_VertexIDs[NUM_VERTS];
		UInt32 m_VertexCount;
		UInt32 m_Status;
	};

	typedef int DataType;
	typedef dynamic_array< Plane > Hull;
	typedef std::vector< Hull > HullContainer;

	typedef dynamic_array< Vector3f > VertexContainer;
	typedef VertexContainer Polygon;
	typedef std::vector< Polygon > PolygonContainer;

	inline void Clear ();
	inline size_t PolyCount () const;
	inline size_t VertCount () const;
	inline const float* GetVertex (size_t i) const;
	inline const Poly* GetPoly (size_t i) const;
	inline const DataType* GetData (size_t i) const;

	void MergePolygons ();
	void FindNeighbors ();
	void AddPolygon (const Polygon& vertices, const DataType& data);
	bool ClipPolys (const HullContainer& carveHulls);

	void Reserve (const int vertexCount, const int polygonCount);
	void AddVertex (const Vector3f& v);
	void AddPolygon (const UInt16* vertexIDs, const DataType& data, size_t vertexCount);

private:
	size_t AddVertexChecked (const Vector3f& v);
	void AddPolygon (const Polygon& vertices, const DataType& data, const UInt32 status);
	Poly CreatePolygon (const Polygon& vertices, const UInt32 status);
	void RemovePolygon (size_t i);

	void Intersection (Polygon& inside, const Polygon& poly, const Hull& clipHull) const;
	void SplitPoly (Polygon& inside, const Polygon& poly, const Plane& plane) const;
	void FromPoly (Polygon& result, const Poly& poly) const;
	void BuildEdgeConnections (EdgeList& edges) const;
	int FindFurthest (const Vector3f& v1, const Vector3f& v2, const VertexContainer& vertices) const;
	void Subtract (PolygonContainer& result, const Polygon& outer, const Polygon& inner) const;
	void ConnectPolygons ();
	void RemoveDegeneratePolygons ();
	void MergePolygons (PolygonContainer& polys);
	bool MergePolygons (Polygon& merged, const Polygon& p1, const Polygon& p2) const;

	dynamic_array<Poly> m_Polygons;
	dynamic_array<Vector3f> m_Vertices;
	dynamic_array<DataType> m_Data;
};

inline void DynamicMesh::Clear ()
{
	m_Polygons.resize_uninitialized (0);
	m_Vertices.resize_uninitialized (0);
	m_Data.resize_uninitialized (0);
}

inline size_t DynamicMesh::PolyCount () const
{
	return m_Polygons.size ();
}

inline size_t DynamicMesh::VertCount () const
{
	return m_Vertices.size ();
}

inline const float* DynamicMesh::GetVertex (size_t i) const
{
	DebugAssert (i < VertCount ());
	return m_Vertices[i].GetPtr ();
}

inline const DynamicMesh::Poly* DynamicMesh::GetPoly (size_t i) const
{
	DebugAssert (i < PolyCount ());
	return &m_Polygons[i];
}

inline const DynamicMesh::DataType* DynamicMesh::GetData (size_t i) const
{
	DebugAssert (i < PolyCount ());
	return &m_Data[i];
}

#endif
