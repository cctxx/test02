/*
 * Ear based re-tessellation based on Kong's algorithm.
 * This is not the best nor complete algorithm, but it should suits our needs given
 * that:
 *  - We will tessellate only simple polygons, with no holes.
 *  - The number of vertices forming the polygon should be pretty small.
 */
#include "UnityPrefix.h"
#include "TriReduction.h"
#include "TriTessellation.h"

#include "Runtime/Math/Quaternion.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Utilities/dynamic_array.h"

struct TTEdge
{
	TTEdge ();
	TTEdge (HMVertex* v0, HMVertex* v1) { v[0] = v0; v[1] = v1; }
	void Invert () { HMVertex* pTemp = v[0]; v[0] = v[1]; v[1] = pTemp; }

	HMVertex* v[2];
};
typedef dynamic_array<TTEdge> TTEdges;

struct TTLocalVertex
{
	TTLocalVertex () {}
	TTLocalVertex (HMVertex* v0, Vector2f& pos) : v (v0), localPos (pos) {}

	HMVertex* v;
	Vector2f localPos;
};
typedef std::list<TTLocalVertex> TTLocalVertices;

struct TTTri
{
	TTTri () {}
	TTTri (HMVertex* v0, HMVertex* v1, HMVertex* v2) { v[0] = v0; v[1] = v1; v[2] = v2; }
	HMVertex* v[3];
};
typedef std::vector<TTTri> TTTris;

bool tri_tessellation_is_planar (TRTris&);
void tri_tessellation_find_contour (TRVerts&, TTEdges&);
bool tri_tessellation_connect_contour (TTEdges&, TRVerts&);
bool tri_tessellation_simplify_contour (TRVerts&, TRVerts&);
Quaternionf tri_tessellation_transform_local (TRVerts& , Vector3f , std::list<TTLocalVertex>&);
void tri_tessellation_order_clockwise (TTLocalVertices& vertices);
bool tri_tessellation_kong (TTLocalVertices&, TTTris&);
void tri_tessellation_find_concave_verts (TTLocalVertices&, TTLocalVertices&);
bool tri_tessellation_is_ear (Vector2f&, Vector2f&, Vector2f&, TTLocalVertices&);
bool tri_tessellation_is_convex (Vector2f&,	Vector2f&, Vector2f&);
bool tri_tessellation_is_inside (Vector2f&, Vector2f&, Vector2f&, Vector2f&);
void tri_tessellation_convert_to_poly (TRVerts&, TTTris&, HMPoly&);

// Re-tessellate if we have a planar surface.
bool tri_tessellation_planar_surface_retessallation (
	TRVerts& vertices,
	TRTris& triangles,
	HMPoly& out_poly)
{
	if (!tri_tessellation_is_planar (triangles))
		return false;

	// Find contour.
	TTEdges contour;
	tri_tessellation_find_contour (vertices, contour);

	// Connect vertices.
	TRVerts contourVerts;
	if (!tri_tessellation_connect_contour (contour, contourVerts))
		return false;

	// Simplify the contour.
	TRVerts simpleContourVerts;
	tri_tessellation_simplify_contour (contourVerts, simpleContourVerts);

	// Put the plane in a x,z local system.
	std::list<TTLocalVertex> localVerts;
	tri_tessellation_transform_local (simpleContourVerts, triangles[0]->normal, localVerts);

	// Tessellate.
	TTTris newTris;
	if (!tri_tessellation_kong (localVerts, newTris))
		return false;

	// Convert tris.
	tri_tessellation_convert_to_poly (vertices, newTris, out_poly);

	return true;
}

// Returns true if the surface is planar (all triangles have the same normal)
bool tri_tessellation_is_planar (TRTris& triangles)
{
	HMTriangle* pTriA = *(triangles.begin ());
	if (pTriA == NULL)
		return false;

	float delta = 0.9999f;	// cos (0.1 degrees)
	for (TRTris::iterator it = triangles.begin (); it != triangles.end (); ++it)
	{
		HMTriangle* pTriB = (*it);
		if (pTriB == NULL)
			return false;

		float cosTheta = Dot (pTriA->normal, pTriB->normal);

		if (cosTheta < delta)
		{
			// We don't have a planar surface.
			return false;
		}
	}

	return true;
}

// Find all edges defining the mesh contour.
void tri_tessellation_find_contour (
	TRVerts& vertices,
	TTEdges& out_contour)
{
	dynamic_array<bool> markedTris;
	markedTris.resize_initialized (vertices.size (), false);

	// Find edges that are part of the contour.
	for (TRVerts::iterator it = vertices.begin (); it != vertices.end (); ++it)
	{
		HMVertex* u = (*it);

		for (std::list<HMVertex*>::iterator itNeighbor = u->neighbor.begin (); itNeighbor != u->neighbor.end (); ++itNeighbor)
		{
			HMVertex* v = (*itNeighbor);

			// Edge already processed?
			if (markedTris[v->idx] == true)
				continue;

			HMTriangle* tuv = NULL;
			bool innerEdge = false;
			for (std::list<HMTriangle*>::iterator itTris = u->tris.begin (); itTris != u->tris.end (); ++itTris)
			{

				HMTriangle* t = (*itTris);
				if (t->HasVertex (v))
				{
					if (tuv == NULL)
						tuv = t;
					else
					{
						innerEdge = true;
						break;
					}
				}
			}

			if (!innerEdge)
			{
				out_contour.push_back (TTEdge (u, v));
				markedTris[u->idx] = true;
			}
		}
	}
}

// Connect all edges of polygon.
bool tri_tessellation_connect_contour (
	TTEdges& contour,
	TRVerts& out_verts)
{
	if (contour.size () < 1)
		return false;

	// Basic bubble-sort algorithm. Room for improvement.
	// Start with first edge vertices[0] as first point.
	out_verts.push_back (contour[0].v[0]);
	for (int i = 0; i < contour.size (); i++)
	{
		TTEdge& rCurr = contour[i];
		for (int j = i + 1; j < contour.size (); j++)
		{
			TTEdge& rNeighbor = contour[j];
			HMVertex* v = NULL;
			if (rCurr.v[1] == rNeighbor.v[0])
			{
				v = rNeighbor.v[0];
			}
			else if (rCurr.v[1] == rNeighbor.v[1])
			{
				rNeighbor.Invert ();		// Invert, because we always search for match using v[1].
				v = rNeighbor.v[0];
			}

			if (v != NULL)
			{
				out_verts.push_back (v);

				// Switch element j with element i+1.
				TTEdge temp (contour[i+1]);
				contour[i+1] = rNeighbor;
				contour[j] = temp;

				break;
			}
		}
	}

	if (out_verts.size () != contour.size ())
	{
		LogString ("Error occurs while connecting the polygon contour.");
		return false;
	}

	return true;
}

inline bool tri_tessellation_merge_contour_vertices (
	HMVertex* prev, HMVertex* curr, HMVertex* next,
	TRVerts& out_simple)
{
	// Define some constants.
	const float COS_ZERO =  1.0f;

	Vector3f v1 = curr->pos - prev->pos;
	v1 = NormalizeSafe (v1);
	Vector3f v2 = next->pos - curr->pos;
	v2 = NormalizeSafe (v2);

	// Take the angle between the three verts.
	float cosTheta = Dot (v1, v2);
	if (!CompareApproximately (cosTheta, COS_ZERO) && !CompareApproximately (cosTheta, -COS_ZERO))
	{
		out_simple.push_back (curr);
		return true;
	}

	return false;
}

// Simplify a contour.
bool tri_tessellation_simplify_contour (TRVerts& contour, TRVerts& out_simple)
{
	if (contour.size () <= 3)
	{
		out_simple.assign (contour.begin (), contour.end ());
		return false;
	}

	// Push first vertex.
	out_simple.clear ();
	out_simple.push_back (contour[0]);

	// Find edges that are connected to each other and parallel and merge them.
	bool simplified = false;
	TRVerts::iterator itNext = contour.begin ();
	TRVerts::iterator itPrev = itNext++;
	TRVerts::iterator itCurr = itNext++;
	for (; itNext != contour.end (); ++itPrev, ++itCurr, ++itNext)
	{
		simplified &= tri_tessellation_merge_contour_vertices (*itPrev, *itCurr, *itNext, out_simple);
	}

	// Merge last vertex?
	simplified &= tri_tessellation_merge_contour_vertices (*itPrev, *itCurr, *contour.begin (), out_simple);

	return simplified;
}

// Transform the vertices of a plane in a local space coordinate system in 2D.
Quaternionf tri_tessellation_transform_local (
	TRVerts& vertices,
	Vector3f planeNormal,
	std::list<TTLocalVertex>& out_localVerts)
{
	Vector3f yaxis (0, -1, 0);
	Quaternionf q = Quaternionf::identity ();
	if (!CompareApproximately (planeNormal, yaxis))
	{
		q = FromToQuaternion (planeNormal, yaxis);

		for (int i = 0; i < vertices.size (); ++i)
		{
			Vector3f worldPos = vertices[i]->pos;
			out_localVerts.push_back (TTLocalVertex ());
			TTLocalVertex& localV = out_localVerts.back ();
			Vector3f localPos = RotateVectorByQuat (q, worldPos);

			localV.localPos.Set (localPos.x, localPos.z);
			localV.v = vertices[i];
		}
	}
	else
	{
		for (int i = 0; i < vertices.size (); ++i)
		{
			out_localVerts.push_back (TTLocalVertex ());
			TTLocalVertex& localV = out_localVerts.back ();
			localV.localPos.Set (vertices[i]->pos.x, vertices[i]->pos.z);
			localV.v = vertices[i];
		}
	}

	return Inverse (q);
}

void tri_tessellation_order_clockwise (TTLocalVertices& vertices)
{
	TTLocalVertices::const_iterator vit = vertices.begin ();
	Vector2f v0 = vit->localPos;
	Vector2f v1 = (++vit)->localPos;
	Vector2f v2 = (++vit)->localPos;

	// Reverse winding if not clockwise
	if (!tri_tessellation_is_convex (v0, v1, v2))
		vertices.reverse ();
}

// Main tessellation loop.
bool tri_tessellation_kong (
	TTLocalVertices& vertices,	// The vertices in that list will be consumed by the Kong algo.
	TTTris& out_tris)
{
	if (vertices.size () <= 3) return false;

	tri_tessellation_order_clockwise (vertices);

	TTLocalVertices concaveVerts;
	tri_tessellation_find_concave_verts (vertices, concaveVerts);

	Vector2f prev, curr, next;
	while (vertices.size () > 3)
	{
		TTLocalVertices::iterator itNext = vertices.begin ();
		TTLocalVertices::iterator itPrev = itNext++;
		TTLocalVertices::iterator itCurr = itNext++;
		curr = (*itPrev).localPos;
		next = (*itCurr).localPos;
		for (; itNext != vertices.end (); ++itPrev, ++itCurr, ++itNext)
		{
			// Set-up the polygon to test.
			prev = curr;
			curr = next;
			next = (*itNext).localPos;

			if (tri_tessellation_is_ear (prev, curr, next, concaveVerts))
			{
				// Add the triangle.
				out_tris.push_back (TTTri ((*itPrev).v, (*itCurr).v, (*itNext).v));

				// Remove the ear.
				vertices.erase (itCurr);
				break;
			}
		}

		// Found no ear!
		if (itNext == vertices.end ())
			break;
	}

	// Something went wrong...
	if (vertices.size () != 3)
	{
		AssertIf ("Kong tessellation failed.");
		return false;
	}

	// Add last triangle.
	TTLocalVertices::iterator itNext = vertices.begin ();
	TTLocalVertices::iterator itPrev = itNext++;
	TTLocalVertices::iterator itCurr = itNext++;
	out_tris.push_back (TTTri ((*itPrev).v, (*itCurr).v, (*itNext).v));

	return true;
}

// Find all the concave vertices of a polygon.
// The polygon must be simple and be stored as a list of P1, P2, Pn vertices, ordered clockwise.
void tri_tessellation_find_concave_verts (
	TTLocalVertices& vertices,
	TTLocalVertices& out_concaves)
{
	if (vertices.size () <= 3)	return;

	Vector2f prev, curr, next;
	TTLocalVertices::iterator nextIt = vertices.begin ();
	TTLocalVertices::iterator currIt = vertices.begin (); ++currIt;
	curr = (*(nextIt++)).localPos;
	next = (*(nextIt++)).localPos;
	for (; nextIt != vertices.end (); ++nextIt, ++currIt)
	{
		// Set-up the polygon to test.
		prev = curr;
		curr = next;
		next = (*nextIt).localPos;

		if (!tri_tessellation_is_convex (prev, curr, next))
			out_concaves.push_back (*(currIt));
	}

	// Last test: last 2 vertices and first one.
	prev = curr;
	curr = next;
	next = (*(vertices.begin ())).localPos;
	if (!tri_tessellation_is_convex (prev, curr, next))
		out_concaves.push_back (*vertices.begin ());
}

// Returns true if the 3 given points are considered an ear of a polygon.
// An ear is defined as a set of consecutive vertices that is considered convex and
// no other point of the polygon is inside the triangle formed by those 3 vertices.
// The concave verts list is an optimization; it prevent checking all points all the time
// as concave points don't change in the tessellation process.
bool tri_tessellation_is_ear (
	Vector2f& v0,
	Vector2f& v1,
	Vector2f& v2,
	TTLocalVertices& concaveVerts)
{
	if (!(tri_tessellation_is_convex (v0, v1, v2)))
		return false;

	// If no concave vertex is inside the triangle, we have an ear.
	for (TTLocalVertices::iterator it = concaveVerts.begin (); it != concaveVerts.end (); ++it)
	{
		if (tri_tessellation_is_inside (v0, v1, v2, (*it).localPos))
			return false;
	}

	return true;
}

// Returns true if a given polygon is convex.
// This assume that the polygon is constructed clockwise.
bool tri_tessellation_is_convex (
	Vector2f& v0,
	Vector2f& v1,
	Vector2f& v2)
{
	Vector2f v (v1.x - v0.x, v1.y - v0.y);
	int res = v2.x * v.y - v2.y * v.x + v.x * v0.y - v.y * v0.x;
	return (res <= 0);
}

// Barycentric method to determine if a point is inside a triangle.
bool tri_tessellation_is_inside (
	Vector2f& a,
	Vector2f& b,
	Vector2f& c,
	Vector2f& p)
{
	Vector2f v0 (b.x - a.x, b.y - a.y);
	Vector2f v1 (c.x - a.x, c.y - a.y);
	Vector2f v2 (p.x - a.x, p.y - a.y);

	float det =  v0.x * v1.y - v1.x * v0.y;
	float u   = (v2.x * v1.y - v1.x * v2.y) / det;
	float v   = (v0.x * v2.y - v2.x * v0.y) / det;

	return (u > 0) && (v > 0) && (u + v) < 1;
}

void tri_tessellation_convert_to_poly (TRVerts& vertices, TTTris& tris, HMPoly& out_poly)
{
	out_poly.ResetTris ();

	// 1. Create conversion array.
	static const int NOT_MARK = -2;
	static const int MARK = -1;
	dynamic_array<int> markedVerts;
	int ntris = 0;
	int nverts = 0;
	markedVerts.resize_initialized (vertices.size (), NOT_MARK);
	for (int iTri = 0; iTri < tris.size (); ++iTri)
	{
		TTTri& t = tris[iTri];
		for (int i = 0; i < 3; ++i)
		{
			int iVert = t.v[i]->idx;
			markedVerts[iVert] = MARK;
		}
	}
	for (int iVert = 0; iVert < vertices.size (); ++iVert)
	{
		if (markedVerts[iVert] == MARK)
			markedVerts[iVert] = nverts++;
	}

	// 2. Convert tris vertex indices and copy them to the HMPoly.
	// Remember we have 4 indices per triangle... Fourth is junk.
	const int TRI_IDX_PADDING = -1;
	int nextTriIdx = 0;
	int totalNbTriIndices = tris.size ()*4;	// We allocate worst case memory. It will probably be too much...
	out_poly.triIndices.resize_initialized (totalNbTriIndices, NOT_MARK);

	for (int iTri = 0; iTri < tris.size (); ++iTri)
	{
		TTTri& t = tris[iTri];

		// markedVerts[oldIdx] = newIdx
		out_poly.triIndices[nextTriIdx++] = markedVerts[ t.v[0]->idx ];
		out_poly.triIndices[nextTriIdx++] = markedVerts[ t.v[1]->idx ];
		out_poly.triIndices[nextTriIdx++] = markedVerts[ t.v[2]->idx ];
		out_poly.triIndices[nextTriIdx++] = TRI_IDX_PADDING;
		ntris++;
	}

	// 3. Copy vertices to the HMPoly
	const float UNINIT_VERT = std::numeric_limits<float>::infinity ();
	int nextVertIdx = 0;
	int totalNbVertices = nverts*3;
	out_poly.vertices.resize_initialized (totalNbVertices, UNINIT_VERT);		// 3 floats per vertices.
	for (int iVert = 0; iVert < markedVerts.size (); ++iVert)
	{
		if (markedVerts[iVert] > NOT_MARK)
		{
			const HMVertex* v = vertices[iVert];
			out_poly.vertices[nextVertIdx++] = v->pos.x;
			out_poly.vertices[nextVertIdx++] = v->pos.y;
			out_poly.vertices[nextVertIdx++] = v->pos.z;
		}
	}

	AssertIf (nextVertIdx != totalNbVertices);

	// 4. Set ntris, nverts.
	out_poly.ntris += ntris;
	out_poly.nverts += nverts;
}

#include "TriTessellationTest.h"

