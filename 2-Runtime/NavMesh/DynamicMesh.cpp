#include "UnityPrefix.h"
#include "./DynamicMesh.h"
#include <math.h>
#include <float.h>
#include "Runtime/Math/FloatConversion.h"
#include "DetourCommon.h"

const float MAGIC_QUANTIZE = 1e-2f;
const bool MERGE_POLYGONS = true;

// Compiling to conditional move this is typically faster than modulus operator % in the general case
static inline size_t NextIndex (size_t index, size_t modulus)
{
	DebugAssert (index < modulus);
	const size_t next = index + 1;
	return (next == modulus) ? 0 : next;
}

// Compiling to conditional move this is typically faster than modulus operator % in the general case
static inline size_t PrevIndex (size_t index, size_t modulus)
{
	DebugAssert (index < modulus);
	return (index == 0) ? modulus-1 : index - 1;
}

// TODO : round only fraction part of the floats
// in order to avoid integer overflow for large values of 'v'
static inline Vector3f QuantizeVertex (const Vector3f& v)
{
	if (MAGIC_QUANTIZE <= 0)
	{
		return v;
	}
	else
	{
		Vector3f qv = 1.0f / MAGIC_QUANTIZE * v;
		qv = Vector3f (RoundfToInt (qv.x), RoundfToInt (qv.y), RoundfToInt (qv.z)) * MAGIC_QUANTIZE;
		return qv;
	}
}

static inline bool IsQuantized (const Vector3f& v)
{
	Vector3f qv = QuantizeVertex (v);
	return qv == v;
}

static inline bool IsStrictlyConvex (const dynamic_array< Vector3f >& vertices)
{
	const size_t vertexCount = vertices.size ();
	for (size_t i = 0; i < vertexCount; ++i)
	{
		const float* v0 = vertices[PrevIndex (i, vertexCount)].GetPtr ();
		const float* v1 = vertices[i].GetPtr ();
		const float* v2 = vertices[NextIndex (i, vertexCount)].GetPtr ();
		const float triArea = dtTriArea2D (v0, v1, v2);
		if (triArea <= 0) return false;
	}
	return true;
}

static inline bool PolygonDegenerate (size_t vertexCount, const UInt16* indices, const Vector3f* vertices)
{
	for (size_t i = 0; i < vertexCount; ++i)
	{
		DebugAssert (IsQuantized (vertices[indices[i]]));
	}
	if (vertexCount < 3)
	{
		return true;
	}
	float area = 0.0f;
	float maxSideSq = 0.0f;
	for (size_t i = 2; i < vertexCount; ++i)
	{
		const float* v0 = vertices[indices[0]].GetPtr ();
		const float* v1 = vertices[indices[i-1]].GetPtr ();
		const float* v2 = vertices[indices[i]].GetPtr ();
		const float triArea = dtTriArea2D (v0, v1, v2);

		area += triArea;
		maxSideSq = std::max (dtVdistSqr (v0, v1), maxSideSq);
		maxSideSq = std::max (dtVdistSqr (v0, v2), maxSideSq);
	}
	if (area <= 0)
	{
		return true;
	}
	const float safety = 1e-2f * MAGIC_QUANTIZE;
	return area * area <= safety * safety * maxSideSq;
}

static inline float PolygonDegenerate (const dynamic_array< Vector3f >& vertices)
{
	if (vertices.size () < 3)
	{
		return true;
	}
	float area = 0.0f;
	float maxSideSq = 0.0f;
	for (size_t i = 2; i < vertices.size (); ++i)
	{
		const float* v0 = vertices[0].GetPtr ();
		const float* v1 = vertices[i-1].GetPtr ();
		const float* v2 = vertices[i].GetPtr ();
		const float triArea = dtTriArea2D (v0, v1, v2);

		area += triArea;
		maxSideSq = std::max (dtVdistSqr (v0, v1), maxSideSq);
		maxSideSq = std::max (dtVdistSqr (v0, v2), maxSideSq);
	}
	if (area <= 0)
	{
		return true;
	}
	const float safety = 1e-2f * MAGIC_QUANTIZE;
	return area * area <= safety * safety * maxSideSq;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


size_t DynamicMesh::AddVertexChecked (const Vector3f& v)
{
	const Vector3f qv = QuantizeVertex (v);
	size_t vertexCount = m_Vertices.size ();
	for (size_t iv = 0; iv < vertexCount; ++iv)
	{
		const Vector3f& ov = m_Vertices[iv];
		if (qv == ov)
			return iv;
	}

	m_Vertices.push_back (qv);
	return vertexCount;
}

DynamicMesh::Poly DynamicMesh::CreatePolygon (const Polygon& vertices, const UInt32 status)
{
	size_t vertexCount = vertices.size ();
	DebugAssert (vertexCount <= NUM_VERTS);
	DebugAssert (vertexCount > 2);

	Poly newPoly = {{0}, {0}, 0, 0};
	newPoly.m_VertexCount = vertexCount;
	newPoly.m_Status = status;
	for (size_t i = 0; i < vertexCount; ++i)
	{
		size_t vi = AddVertexChecked (vertices[i]);
		DebugAssert (vi < 0xffff); //< vertex overflow
		newPoly.m_VertexIDs[i] = (UInt16)vi;
	}
	return newPoly;
}

void DynamicMesh::RemovePolygon (size_t i)
{
	DebugAssert (i < m_Polygons.size ());
	DebugAssert (m_Data.size () == m_Polygons.size ());

	m_Polygons.erase (m_Polygons.begin () + i);
	m_Data.erase (m_Data.begin () + i);
}

// Clip the convex polygon 'poly' by the half-space defined by 'plane'
void DynamicMesh::SplitPoly (Polygon& inside, const Polygon& poly, const Plane& plane) const
{
	inside.resize_uninitialized (0);
	const size_t vertexCount = poly.size ();

	dynamic_array< float > dist (vertexCount, kMemTempAlloc);
	for (size_t iv = 0; iv < vertexCount; ++iv)
	{
		const Vector3f& v = poly[iv];
		dist[iv] = plane.GetDistanceToPoint (v);
	}

	Vector3f prevVert = poly[vertexCount-1];
	float prevDist = dist[vertexCount-1];

	for (size_t iv = 0; iv < vertexCount; ++iv)
	{
		const Vector3f& currVert = poly[iv];
		const float currDist = dist[iv];

		if (currDist < 0 && prevDist > 0)
		{
			const float absDist = -currDist;
			const float w = absDist / (absDist + prevDist);
			const Vector3f newVert = Lerp (currVert, prevVert, w);
			inside.push_back (newVert);
			//			inside.push_back (QuantizeVertex (newVert));
		}
		else if (currDist > 0 && prevDist < 0)
		{
			const float absDist = -prevDist;
			const float w = absDist / (absDist + currDist);
			const Vector3f newVert = Lerp (prevVert, currVert, w);
			inside.push_back (newVert);
			//			inside.push_back (QuantizeVertex (newVert));
		}

		if (currDist <= 0)
		{
			inside.push_back (currVert);
		}

		prevVert = currVert;
		prevDist = currDist;
	}
}

// Return the intersection of 'poly' and 'carveHull'
// Assuming convex shapes.
void DynamicMesh::Intersection (Polygon& inside, const Polygon& poly, const Hull& carveHull) const
{
	DebugAssert (inside.empty ());
	const size_t planeCount = carveHull.size ();
	inside.reserve (planeCount + NUM_VERTS);
	inside = poly;

	Polygon insidePoly;
	insidePoly.reserve (planeCount + NUM_VERTS);

	for (size_t ic = 0; ic < planeCount; ++ic)
	{
		const Plane& plane = carveHull[ic];
		SplitPoly (insidePoly, inside, plane);
		if (insidePoly.empty ())
		{
			inside.resize_uninitialized (0);
			break;
		}
		inside = insidePoly;
	}
}

void DynamicMesh::FromPoly (Polygon& result, const Poly& poly) const
{
	DebugAssert (poly.m_VertexCount > 2);
	DebugAssert (poly.m_VertexCount <= NUM_VERTS);

	const UInt32 vertexCount = poly.m_VertexCount;
	result.resize_uninitialized (vertexCount);

	for (size_t i = 0; i < vertexCount; ++i)
	{
		result[i] = Vector3f (GetVertex (poly.m_VertexIDs[i]));
	}
}

void DynamicMesh::BuildEdgeConnections (EdgeList& edges) const
{
	DebugAssert (edges.empty ());
	const size_t polyCount = m_Polygons.size ();
	for (size_t ip = 0; ip < polyCount; ++ip)
	{
		const Poly& poly = m_Polygons[ip];
		if (PolygonDegenerate (poly.m_VertexCount, poly.m_VertexIDs, &m_Vertices[0]))
			continue;

		size_t vertexCount = poly.m_VertexCount;

		for (size_t ivp = vertexCount-1, iv = 0; iv < vertexCount; ivp = iv++)
		{
			UInt16 vp = poly.m_VertexIDs[ivp];
			UInt16 v = poly.m_VertexIDs[iv];

			DebugAssert (v != vp);

			// Find edge by ordered vertex indices
			UInt16 vmin = (v<vp) ? v : vp;
			UInt16 vmax = (v>vp) ? v : vp;

			size_t ie = 0;
			size_t edgeCount = edges.size ();
			for (; ie < edgeCount; ++ie)
			{
				Edge& edge = edges[ie];
				if (edge.v1 != vmin || edges[ie].v2 != vmax)
					continue;

				// If already connected skip it.
				// A polygon edge cannot connect more than two polygons.
				// Ideally this should not happen.
				if (edge.c2 != 0xffff)
					break;

				// Found an existing unconnected edge
				edge.p2 = ip;
				edge.c2 = ivp;
				break;
			}
			// Edge not found - insert
			if (ie == edgeCount)
			{
				Edge edge =
				{ vmin, vmax, ip, 0xffff, ivp, 0xffff };
				edges.push_back (edge);
			}
		}
	}
}

// Locate furthest vertex in positive half-plane or -1 in none found.
int DynamicMesh::FindFurthest (const Vector3f& v1, const Vector3f& v2, const VertexContainer& vertices) const
{
	int bestIndex = -1;
	float bestDist = 0;

	for (size_t iv = 0; iv < vertices.size (); ++iv)
	{
		const float* v = vertices[iv].GetPtr ();
		float dist = dtTriArea2D(v1.GetPtr (), v2.GetPtr (), v);
		if (dist > bestDist)
		{
			bestDist = dist;
			bestIndex = iv;
		}
	}
	return bestIndex;
}

void DynamicMesh::Subtract (PolygonContainer& result, const Polygon& outer, const Polygon& inner) const
{
	const size_t innerVertexCount = inner.size ();
	const size_t outerVertexCount = outer.size ();
	result.clear ();
	Polygon tri (3, kMemTempAlloc);

	if (innerVertexCount == 1)
	{
		DebugAssert (outerVertexCount > 0);
		for (size_t ov = 0; ov < outerVertexCount; ++ov)
		{
			const size_t ovn = NextIndex (ov, outerVertexCount);
			tri[0] = outer[ov];
			tri[1] = outer[ovn];
			tri[2] = inner[0];
			if (PolygonDegenerate (tri))
			{
				continue;
			}
			tri[2] = QuantizeVertex (inner[0]);
			if (PolygonDegenerate (tri))
			{
				continue;
			}
			result.push_back (tri);
		}
		return;
	}

	dynamic_array< int > ol (innerVertexCount, -1, kMemTempAlloc);
	dynamic_array< int > oh (innerVertexCount, -1, kMemTempAlloc);
	dynamic_array< bool > used (outerVertexCount, false, kMemTempAlloc);

	for (size_t ivp = innerVertexCount-1, iv = 0; iv < innerVertexCount; ivp = iv++)
	{
		int bestOuter = FindFurthest (inner[iv], inner[ivp], outer);

		if (bestOuter == -1)
		{
			continue;
		}
		ol[iv] = bestOuter;
		oh[ivp] = bestOuter;

		tri[0] = inner[iv];
		tri[1] = inner[ivp];
		tri[2] = outer[bestOuter];

		if (PolygonDegenerate (tri))
		{
			continue;
		}
		tri[0] = QuantizeVertex (inner[iv]);
		tri[1] = QuantizeVertex (inner[ivp]);
		if (PolygonDegenerate (tri))
		{
			continue;
		}
		result.push_back (tri);
	}

	for (size_t iv = 0; iv < innerVertexCount; ++iv)
	{
		if (ol[iv] != -1)
		{
			size_t ov = ol[iv];
			size_t iter = 0;
			while (ov != (size_t)oh[iv])
			{
				const size_t ovn = NextIndex (ov, outerVertexCount);
				if (!used[ov])
				{
					tri[0] = outer[ov];
					tri[1] = outer[ovn];
					tri[2] = inner[iv];
					if (PolygonDegenerate (tri))
					{
						break;
					}
					tri[2] = QuantizeVertex (inner[iv]);
					if (PolygonDegenerate (tri))
					{
						break;
					}
					result.push_back (tri);
					used[ov] = true;
				}
				ov = ovn;
				if (++iter == outerVertexCount)
				{
					break;
				}
			}
		}

		if (oh[iv] != -1)
		{
			size_t ov = oh[iv];
			size_t iter = 0;
			while (ov != (size_t)ol[iv])
			{
				const size_t ovp = PrevIndex (ov, outerVertexCount);
				if (!used[ovp])
				{
					tri[0] = outer[ovp];
					tri[1] = outer[ov];
					tri[2] = inner[iv];
					if (PolygonDegenerate (tri))
					{
						break;
					}
					tri[2] = QuantizeVertex (inner[iv]);
					if (PolygonDegenerate (tri))
					{
						break;
					}
					result.push_back (tri);
					used[ovp] = true;
				}
				ov = ovp;
				if (++iter == outerVertexCount)
				{
					break;
				}
			}
		}
	}
}

bool DynamicMesh::MergePolygons (Polygon& merged, const Polygon& p1, const Polygon& p2) const
{
	if (!MERGE_POLYGONS)
		return false;
	const size_t count1 = p1.size ();
	const size_t count2 = p2.size ();

	if (count1 < 3) return false;
	if (count2 < 3) return false;
	if ((count1 + count2 - 2) > NUM_VERTS)
		return false;

	for (size_t iv = 0; iv < count1; ++iv)
	{
		const size_t ivn = NextIndex (iv, count1);
		const Vector3f& v1 = p1[iv];
		const Vector3f& v2 = p1[ivn];
		for (size_t jv = 0; jv < count2; ++jv)
		{
			const size_t jvn = NextIndex (jv, count2);
			const Vector3f& w1 = p2[jv];
			const Vector3f& w2 = p2[jvn];
			if ((v1 == w2) && (v2 == w1))
			{
				// Found shared edge

				// Test convexity
				const Vector3f& wn = p2[NextIndex (jvn, count2)];
				const Vector3f& vp = p1[PrevIndex (iv, count1)];
				if (dtTriArea2D (vp.GetPtr (), v1.GetPtr (), wn.GetPtr ()) <= 0)
				{
					return false;
				}

				// Test convexity
				const Vector3f& wp = p2[PrevIndex (jv, count2)];
				const Vector3f& vn = p1[NextIndex (ivn, count1)];
				if (dtTriArea2D (v2.GetPtr (), vn.GetPtr (), wp.GetPtr ()) <= 0)
				{
					return false;
				}

				// Merge two polygon parts
				for (size_t k = ivn ; k != iv ; k = NextIndex (k, count1))
				{
					merged.push_back (p1[k]);
				}
				for (size_t k = jvn ; k != jv ; k = NextIndex (k, count2))
				{
					merged.push_back (p2[k]);
				}
				DebugAssert (merged.size () == count1 + count2 - 2);
				return IsStrictlyConvex (merged);
			}
		}
	}
	return false;
}

void DynamicMesh::MergePolygons ()
{
	// Merge list of convex non-overlapping polygons assuming identical data.
	for (size_t ip = 0; ip < m_Polygons.size (); ++ip)
	{
		Polygon poly;
		FromPoly (poly, m_Polygons[ip]);
		for (size_t jp = m_Polygons.size () - 1; jp > ip; --jp)
		{
			bool dataConforms = (m_Data[ip] == m_Data[jp]);
			if (!dataConforms)
				continue;

			Polygon merged;
			Polygon poly2;
			FromPoly (poly2, m_Polygons[jp]);
			if (MergePolygons (merged, poly, poly2))
			{
				poly = merged;
				m_Polygons.erase (m_Polygons.begin () + jp);
			}
			if (poly.size () == NUM_VERTS) break;
		}
		m_Polygons[ip] = CreatePolygon (poly, kGeneratedPolygon);
	}
}

void DynamicMesh::MergePolygons (PolygonContainer& polys)
{
	// Merge list of convex non-overlapping polygons assuming identical data.
	for (size_t ip = 0; ip < polys.size (); ++ip)
	{
		Polygon poly = polys[ip];
		for (size_t jp = polys.size ()-1; jp>ip; --jp)
		{
			Polygon merged;
			if (MergePolygons (merged, poly, polys[jp]))
			{
				poly = merged;
				polys.erase (polys.begin () + jp);
			}
		}
		polys[ip] = poly;
	}
}

void DynamicMesh::ConnectPolygons ()
{
	EdgeList edges;
	BuildEdgeConnections (edges);

	size_t edgeCount = edges.size ();
	for (size_t ie = 0; ie < edgeCount; ++ie)
	{
		const Edge& edge = edges[ie];
		if (edge.c2 == 0xffff)
			continue;
		m_Polygons[edge.p1].m_Neighbours[edge.c1] = edge.p2+1;
		m_Polygons[edge.p2].m_Neighbours[edge.c2] = edge.p1+1;
	}
}

void DynamicMesh::RemoveDegeneratePolygons ()
{
	size_t count = m_Polygons.size ();
	for (size_t ip = 0; ip < count; ++ip)
	{
		if (PolygonDegenerate (m_Polygons[ip].m_VertexCount, m_Polygons[ip].m_VertexIDs, &m_Vertices[0]))
		{
			RemovePolygon (ip);
			--count;
			--ip;
		}
	}
}

void DynamicMesh::FindNeighbors ()
{
	RemoveDegeneratePolygons ();
	ConnectPolygons ();
}

void DynamicMesh::AddPolygon (const Polygon& vertices, const DataType& data)
{
	AddPolygon (vertices, data, kOriginalPolygon);
}

void DynamicMesh::AddPolygon (const Polygon& vertices, const DataType& data, const UInt32 status)
{
	// Delaying neighbor connections.
	DebugAssert (m_Polygons.size () < 0xffff); //< poly overflow
	DebugAssert (vertices.size () <= NUM_VERTS);
	DebugAssert (m_Data.size () == m_Polygons.size ());

	if (PolygonDegenerate (vertices))
	{
		return;
	}

	DebugAssert (IsStrictlyConvex (vertices));

	Poly newPoly = CreatePolygon (vertices, status);

	// TODO: avoid leaking vertices when not accepting the poly
	if (PolygonDegenerate (newPoly.m_VertexCount, newPoly.m_VertexIDs, &m_Vertices[0]))
	{
		return;
	}
	m_Polygons.push_back (newPoly);
	m_Data.push_back (data);
}

bool DynamicMesh::ClipPolys (const HullContainer& carveHulls)
{
	size_t hullCount = carveHulls.size ();
	PolygonContainer outsidePolygons;
	bool clipped = false;

	for (size_t ih = 0; ih < hullCount; ++ih)
	{
		Hull carveHull = carveHulls[ih];

		size_t count = m_Polygons.size ();
		for (size_t ip = 0; ip < count; ++ip)
		{
			Polygon currentPoly;
			FromPoly (currentPoly, m_Polygons[ip]);

			Polygon inside;
			Intersection (inside, currentPoly, carveHull);
			if (inside.empty ())
				continue;

			clipped = true;

			Subtract (outsidePolygons, currentPoly, inside);
			MergePolygons (outsidePolygons);

			DataType currentData = m_Data[ip];
			RemovePolygon (ip);
			--count;
			--ip;

			for (size_t io = 0; io < outsidePolygons.size (); ++io)
			{
				AddPolygon (outsidePolygons[io], currentData, kGeneratedPolygon);
			}
		}
	}
	return clipped;
}

void DynamicMesh::Reserve (const int vertexCount, const int polygonCount)
{
	m_Polygons.reserve (polygonCount);
	m_Data.reserve (polygonCount);
	m_Vertices.reserve (vertexCount);
}

void DynamicMesh::AddVertex (const Vector3f& v)
{
	const Vector3f qv = QuantizeVertex (v);
	m_Vertices.push_back (qv);
}

void DynamicMesh::AddPolygon (const UInt16* vertexIDs, const DataType& data, size_t vertexCount)
{
	// TODO : figure out why this needs to be zero'ed
	Poly poly = {{0}, {0}, 0, 0};

	poly.m_Status = kOriginalPolygon;
	poly.m_VertexCount = vertexCount;
	for (size_t iv = 0; iv < vertexCount; ++iv)
	{
		poly.m_VertexIDs[iv] = vertexIDs[iv];
	}
	m_Polygons.push_back (poly);
	m_Data.push_back (data);
}


