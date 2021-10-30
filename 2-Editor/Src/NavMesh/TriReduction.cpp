/*
 *  Progressive Mesh type Polygon Reduction Algorithm
 *  Based on Game Developer Magazine article by Stan Melax (c) 1998
 *  "A Simple, Fast and Effective Polygon Reduction Algorithm"
 *  November 1998
 */
#include "UnityPrefix.h"
#include "TriReduction.h"
#include "HMPoly.h"

#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Allocator/MemoryMacros.h"

static const float TRI_RED_NO_COST = 0.001f;
HMVertex*	g_pVertMem	= NULL;
HMTriangle* g_pTriMem	= NULL;

template <typename T>
struct HMCache
{
	T const& operator[] (size_t index) const { return m_cache[index]; }
	T& operator[] (size_t index) { return m_cache[index]; }

	void push_back (const T& t)
	{
		if (m_size < m_cache.size ())
			m_cache[m_size] = t;
		else
			m_cache.push_back (t);

		m_size++;
	}

	// Reset the cache without clearing memory.
	void reset ()
	{
		m_size = 0;
	}

	size_t size () const { return m_size; }

	std::vector<T> m_cache;
	size_t m_size;
};

void	tri_reduction_compute_all_edge_collapse_cost (TRVerts&);
void	tri_compute_edge_cost_at_vertex (HMVertex*);
float	tri_reduction_edge_collapse_cost (HMVertex*, HMVertex*);
void	tri_reduction_collapse_edge (TRVerts&, TRTris&, HMVertex*, HMVertex*);
bool    tri_reduction_is_deforming (HMVertex*, HMVertex*);
bool	tri_reduction_is_flipping_normal (HMVertex*, HMVertex*);
void	tri_reduction_dispose_tri (TRTris&, HMTriangle*);
void	tri_reduction_dispose_vert (TRVerts&, HMVertex*);

void tri_reduction_collapse_no_cost (HMPoly& poly)
{
	// Convert the poly to a structure understandable by the reduction algorithm.
	TRVerts vertices;
	TRTris triangles;
	tri_reduction_convert (poly, vertices, triangles);
	tri_reduction_collapse_no_cost (vertices, triangles);
	tri_reduction_convert_back (vertices, triangles, poly);
	tri_reduction_clean_up (vertices, triangles);
}

void tri_reduction_collapse_no_cost (
	TRVerts& vertices,
	TRTris& triangles)
{
	// Here starts the collapsing. We don't bother finding the lowest cost, we just collapseCandidate
	// if the cost is near zero. What we want is to merge as much triangles in the same plane as possible.
	tri_reduction_compute_all_edge_collapse_cost (vertices);
 	float cost = 0.0f;
	while (cost < TRI_RED_NO_COST)
	{
		cost = 1.0f;
		for (int i = 0; i < vertices.size (); i++)
		{
			HMVertex* v = vertices[i];
			if (v == NULL) continue;

			if (v->collapseCost < TRI_RED_NO_COST)
			{
				cost = v->collapseCost;
				tri_reduction_collapse_edge (vertices, triangles, v, v->collapseCandidate);
			}
		}
	}
}

// Convert an HMPoly to a HMVertex / HMTriangle representation allowing
// to use HMTriangle Reduction algorithm.
void tri_reduction_convert (
	HMPoly& poly,
	TRVerts& vertices,
	TRTris& triangles)
{
	vertices.reserve (poly.nverts);
	triangles.reserve (poly.ntris);

	// Allocate memory for all Vertices.
	AssertIf (g_pVertMem != NULL);
	g_pVertMem = (HMVertex*) UNITY_MALLOC (kMemNavigation, sizeof (HMVertex) * poly.nverts);
	HMVertex* pVertMem = g_pVertMem;
	for (int iVert = 0; iVert < poly.nverts; ++iVert)
	{
		vertices.push_back (new (pVertMem++) HMVertex (Vector3f (&poly.vertices[(iVert*3)]), iVert));
	}

	AssertIf (g_pTriMem != NULL);
	g_pTriMem = (HMTriangle*) UNITY_MALLOC (kMemNavigation, sizeof (HMTriangle) * poly.ntris);
	HMTriangle* pTriMem = g_pTriMem;
	for (int iTri = 0; iTri < poly.ntris; ++iTri)
	{
		int iv0 = poly.triIndices[iTri*4+0];
		int iv1 = poly.triIndices[iTri*4+1];
		int iv2 = poly.triIndices[iTri*4+2];

		triangles.push_back (new (pTriMem++) HMTriangle (
			vertices[iv0],
			vertices[iv1],
			vertices[iv2],
			iTri));
	}
}

// Find the optimal edge collapseCandidate for all vertices.
void tri_reduction_compute_all_edge_collapse_cost (TRVerts& vertices)
{
	for (int i = 0; i < vertices.size (); i++)
	{
		if (vertices[i] == NULL)
			continue;

		tri_compute_edge_cost_at_vertex (vertices[i]);
	}
}

// Find the optimal edge collapseCandidate for given vertex.
void tri_compute_edge_cost_at_vertex (HMVertex* v)
{
	// Compute the edge collapseCandidate cost for all edges that start
	// from vertex v. We cache the cost of the least cost edge at this vertex
	// as well as the value of the cost (in member variable collapseCost).
	v->collapseCost = 1000000.0f;
	v->collapseCandidate = NULL;

	if (v->neighbor.empty ())
	{
		// v doesn't have neighbors so it costs nothing to collapseCandidate
		v->collapseCandidate = NULL;
		v->collapseCost = -0.01f;
		return;
	}

	// Search all neighboring edges for "least cost" edge
	for (std::list<HMVertex*>::iterator it = v->neighbor.begin (); it != v->neighbor.end (); ++it)
	{
		HMVertex* neighbor = (*it);
		float cost = tri_reduction_edge_collapse_cost (v, neighbor);
		if (cost < v->collapseCost)
		{
			v->collapseCandidate = neighbor;  // candidate for edge collapseCandidate
			v->collapseCost  = cost;      // cost of the collapseCandidate

			// We have a winner; bail-out.
			if (cost < TRI_RED_NO_COST)
				break;
		}
	}
}

// If we collapseCandidate edge uv by moving u to v then how
// much different will the model change..
float tri_reduction_edge_collapse_cost (HMVertex* u, HMVertex* v)
{
	float edgelength = Magnitude (v->pos - u->pos);
	float curvature = 0.0f;

	// Find the "sides" triangles that are on the edge uv
	static HMCache<HMTriangle*> sideCache;
	sideCache.reset ();
	for (std::list<HMTriangle*>::iterator it = u->tris.begin (); it != u->tris.end (); ++it)
	{
		if ((*it)->HasVertex (v))
			sideCache.push_back (*it);
	}

	// Filter out reductions that will deform the polygon.
	if (tri_reduction_is_deforming (u, v))
		return 1.0f;

	// Filter out reductions that will cause normal flipping.
	if (tri_reduction_is_flipping_normal (u, v))
		return 1.0f;

	// Use the triangle facing most away from the sides to determine our curvature term
	for (std::list<HMTriangle*>::iterator it = u->tris.begin (); it != u->tris.end (); ++it)
	{
		float mincurv = 1.0f; // curve for face i and closer side to it
		HMTriangle* face = (*it);

		for (int i = 0; i < sideCache.size (); i++)
		{
			// Don't test against the same triangle.
			if (face == sideCache[i]) continue;

			// Use dot product of face normals.
			float dotprod = Dot (face->normal, sideCache[i]->normal);
			mincurv = std::min (mincurv, (1.0f-dotprod)/2.0f);
		}
		curvature = std::max (curvature, mincurv);
	}

	// The more coplanar the lower the curvature term.
	return edgelength * curvature;
}

// Collapse edge uv by moving vertex u unto v.
void tri_reduction_collapse_edge (
	TRVerts& vertices,
	TRTris& triangles,
	HMVertex* u,
	HMVertex* v)
{
	// Steps:
	// 1- Remove tris containing uv.
	// 2- Update tris that have u to have v, and then remove u.
	// 3- Recompute edge collapsing cost.

	if (!v)
	{
		// u is a vertex all by itself so just delete it
		tri_reduction_dispose_vert (vertices, u);
		return;
	}

	// Cache neighbors of u.
	static HMCache<HMVertex*> neighborCache;
	neighborCache.reset ();
	for (std::list<HMVertex*>::iterator it = u->neighbor.begin (); it != u->neighbor.end (); ++it)
		neighborCache.push_back (*it);

	// 1- Remove tris containing uv.
	for (std::list<HMTriangle*>::iterator it = u->tris.begin (); it != u->tris.end ();)
	{
		HMTriangle* t = (*it);
		if (t->HasVertex (v))
		{
			tri_reduction_dispose_tri (triangles, t);
			it = u->tris.begin ();
		}
		else
			++it;
	}

	// 2- Update remaining triangles to have v instead of u
	for (std::list<HMTriangle*>::iterator it = u->tris.begin (); it != u->tris.end ();)
	{
		HMTriangle* t = (*it);
		t->ReplaceVertex (u,v);
		it = u->tris.begin ();
	}
	tri_reduction_dispose_vert (vertices, u);

	// 3- Recompute the edge collapseCandidate costs for neighbors vertices
	for (int i = 0; i < neighborCache.size (); ++i)
	{
		tri_compute_edge_cost_at_vertex (neighborCache[i]);
	}
}

// If we collapseCandidate edge uv by moving u to v, will it alter the geometry.
bool tri_reduction_is_deforming (HMVertex* u, HMVertex* v)
{
	// In order to be non-deforming, a collapseCandidate from a vertex u to a vertex v
	// must not happen if u is part of an outline edge.
	// Following conditions must be fulfilled:
	//	1- u must be part of more than 1 triangle.
	//	2- All neighbor of u must be part of more than one triangle.
	//  3- Edges composed of u must be part of more than one triangle.
	if (u->tris.size () < 2)
		return true;

	for (std::list<HMVertex*>::iterator itNeighbor = u->neighbor.begin (); itNeighbor != u->neighbor.end (); ++itNeighbor)
	{
		HMVertex* v = (*itNeighbor);
		if (v->tris.size () < 2)
			return true;

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
			return true;
	}

	return false;
}

// If we collapseCandidate edge uv by moving u to v, will it invert some triangle normals.
bool tri_reduction_is_flipping_normal (HMVertex* u, HMVertex* v)
{
	// Find faces containing only u and see if replacing v by u will invert the normal.
	for (std::list<HMTriangle*>::iterator it = u->tris.begin (); it != u->tris.end (); ++it)
	{
		HMTriangle* t = (*it);
		if (t->HasVertex (v))
			continue;

		Vector3f vc[3];
		for (int i = 0; i < 3; ++i)
		{
			if (t->vertex[i] == u)
				vc[i] = v->pos;
			else
				vc[i] = t->vertex[i]->pos;
		}
		Vector3f nc = Cross ((vc[1]-vc[0]), (vc[2]-vc[1]));

		// If normal are pointing in opposite direction.
		float cosTheta = Dot (t->normal, nc);
		if (cosTheta < 0.0f)
			return true;
	}

	return false;
}

// Convert a polygon in HMVertex / HMTriangle notation to an HMPoly.
void tri_reduction_convert_back (
	TRVerts& vertices,
	TRTris& triangles,
	HMPoly& poly)
{
	poly.ResetTris ();

	// 1. Create conversion array.
	static const int NOT_MARK = -1;
	dynamic_array<int> markedVerts;
	int ntris = 0;
	int nverts = 0;
	markedVerts.resize_initialized (vertices.size (), NOT_MARK);
	for (int iVert = 0; iVert < vertices.size (); ++iVert)
	{
		HMVertex* v = vertices[iVert];
		if (v == NULL) continue;

		markedVerts[iVert] = nverts++;
	}

	// 2. Convert tris vertex indices and copy them to the HMPoly.
	// Remember we have 4 indices per triangle... Fourth is junk.
	const int TRI_IDX_PADDING = -1;
	int nextTriIdx = 0;
	int totalNbTriIndices = triangles.size ()*4;	// We allocate worst case memory. It will probably be too much...
	poly.triIndices.resize_initialized (totalNbTriIndices, NOT_MARK);

	for (int iTri = 0; iTri < triangles.size (); ++iTri)
	{
		HMTriangle* t = triangles[iTri];
		if (t == NULL) continue;

		// markedVerts[oldIdx] = newIdx
		poly.triIndices[nextTriIdx++] = markedVerts[ t->vertex[0]->idx ];
		poly.triIndices[nextTriIdx++] = markedVerts[ t->vertex[1]->idx ];
		poly.triIndices[nextTriIdx++] = markedVerts[ t->vertex[2]->idx ];
		poly.triIndices[nextTriIdx++] = TRI_IDX_PADDING;
		ntris++;
	}

	// 3. Copy vertices to the HMPoly
	const float UNINIT_VERT = std::numeric_limits<float>::infinity ();
	int nextVertIdx = 0;
	int totalNbVertices = nverts*3;
	poly.vertices.resize_initialized (totalNbVertices, UNINIT_VERT);		// 3 floats per vertices.
	for (int iVert = 0; iVert < markedVerts.size (); ++iVert)
	{
		if (markedVerts[iVert] > NOT_MARK)
		{
			const HMVertex* v = vertices[iVert];
			poly.vertices[nextVertIdx++] = v->pos.x;
			poly.vertices[nextVertIdx++] = v->pos.y;
			poly.vertices[nextVertIdx++] = v->pos.z;
		}
	}

	AssertIf (nextVertIdx != totalNbVertices);

	// 4. Set ntris, nverts.
	poly.ntris += ntris;
	poly.nverts += nverts;
}

void tri_reduction_dispose_tri (TRTris& triangles, HMTriangle* t)
{
	t->Dispose ();
	triangles[t->idx] = NULL;
	t->~HMTriangle ();
}

void tri_reduction_dispose_vert (TRVerts& vertices, HMVertex* v)
{
	v->Dispose ();
	vertices[v->idx] = NULL;
	v->~HMVertex ();
}

void tri_reduction_clean_up (TRVerts& vertices, TRTris& triangles)
{
	for (int i = 0; i < triangles.size (); ++i)
	{
		if (triangles[i] != NULL)
			triangles[i]->~HMTriangle ();
	}

	for (int i = 0; i < vertices.size (); ++i)
	{
		if (vertices[i] != NULL)
			vertices[i]->~HMVertex ();
	}

	triangles.clear ();
	vertices.clear ();

	UNITY_FREE (kMemNavigation, g_pTriMem); g_pTriMem = NULL;
	UNITY_FREE (kMemNavigation, g_pVertMem); g_pVertMem = NULL;
}

void tri_reduction_remove_non_walkable (
	TRVerts& vertices,
	TRTris& triangles,
	dynamic_array<bool>& triWalkableFlags)
{
	AssertIf (triangles.size () != triWalkableFlags.size ());

	for (int i = 0; i < triWalkableFlags.size (); ++i)
	{
		if (!triWalkableFlags[i])
		{
			tri_reduction_dispose_tri (triangles, triangles[i]);
		}
	}
}

#include "TriReductionTest.h"

