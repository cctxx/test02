#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"

void BuildSquareFiveVertices (
	TRVerts& verts,
	TRTris& tris)
{
	// Create a square with 4 triangles, sharing the same vertex in the middle.
	//	 v0	______ v1                ______
	//     |\    /|                 |\ 0  /|
	//     |  \/  |				    |  \/  |
	//     |  /\  |				    | 1/\2 |
	//     | /v2\ |				    | /  \ |
	//     |/____\|				    |/_3__\|
	//   v3        v4
	verts.push_back (new HMVertex (Vector3f (0, 0, 0), 0));
	verts.push_back (new HMVertex (Vector3f (2, 0, 0), 1));
	verts.push_back (new HMVertex (Vector3f (1, 0, 1), 2));
	verts.push_back (new HMVertex (Vector3f (0, 0, 2), 3));
	verts.push_back (new HMVertex (Vector3f (2, 0, 2), 4));

	tris.push_back (new HMTriangle (verts[0], verts[1], verts[2], 0));
	tris.push_back (new HMTriangle (verts[0], verts[2], verts[3], 1));
	tris.push_back (new HMTriangle (verts[1], verts[4], verts[2], 2));
	tris.push_back (new HMTriangle (verts[2], verts[4], verts[3], 3));
}


void BuildSquareFourVertices (
	TRVerts& verts,
	TRTris& tris)
{
	// Create a square with 4 triangles, sharing the same vertex in the middle.
	//	 v0	_____ v1                  _____
	//     |    /|                   |    /|
	//     |   / |				     | 0 / |
	//     |  /  |				     |  /  |
	//     | /   |				     | / 1 |
	//     |/____|				     |/____|
	//   v2       v3
	verts.push_back (new HMVertex (Vector3f (0, 0, 0), 0));
	verts.push_back (new HMVertex (Vector3f (2, 0, 0), 1));
	verts.push_back (new HMVertex (Vector3f (0, 0, 2), 2));
	verts.push_back (new HMVertex (Vector3f (2, 0, 2), 3));

	tris.push_back (new HMTriangle (verts[0], verts[1], verts[2], 0));
	tris.push_back (new HMTriangle (verts[1], verts[3], verts[2], 1));
}


void BuildTriFourVertices (
	TRVerts& verts,
	TRTris& tris)
{
	// Create a two triangles sharing a same vertex.
	//              v0
	//	           / \
	//            / | \
	//   	     /  |  \
	//   	    / 0 | 1 \
	//          ---------
	//   	   v1   v2    v3
	verts.push_back (new HMVertex (Vector3f (0, 0, 0), 0));
	verts.push_back (new HMVertex (Vector3f (2, 0, 0), 1));
	verts.push_back (new HMVertex (Vector3f (0, 0, 2), 2));
	verts.push_back (new HMVertex (Vector3f (2, 0, 2), 3));

	tris.push_back (new HMTriangle (verts[0], verts[1], verts[2], 0));
	tris.push_back (new HMTriangle (verts[1], verts[3], verts[2], 1));
}


void TriReductionTestCleanUp (
	TRVerts& verts,
	TRTris& tris)
{
	for (int i = 0; i < tris.size (); ++i)
	{
		if (tris[i] != NULL)
			tris[i]->Dispose ();
	}
	for (int i = 0; i < verts.size (); ++i)
	{
		if (verts[i] != NULL)
			verts[i]->Dispose ();
	}
}


SUITE (TriReductionTests)
{
	TEST (TriReduction_EdgeCollapseCost)
	{
		TRVerts verts;
		TRTris tris;
		BuildSquareFiveVertices (verts, tris);

		// Edge (0,2) is not collapsible because 0 is part of an outline edge.
		float c02 = tri_reduction_edge_collapse_cost (verts[0], verts[2]);
		CHECK (c02 > TRI_RED_NO_COST);

		// Edge (2,0) should be collapsible with minimal cost.
		float c20 = tri_reduction_edge_collapse_cost (verts[2], verts[0]);
		CHECK (c20 < TRI_RED_NO_COST);


		TriReductionTestCleanUp (verts, tris);
	}


	TEST (TriReduction_CollapseEdgeCost2)
	{
		TRVerts verts;
		TRTris tris;
		BuildSquareFourVertices (verts, tris);

		// Edge (0,1) should not be collapsible.
		float c01 = tri_reduction_edge_collapse_cost (verts[0], verts[2]);
		CHECK (c01 > 0.0f);

		// Edge (1,2) should not be collapsible. Collapsing this edge will not
		// result in creating triangles.
		float c12 = tri_reduction_edge_collapse_cost (verts[2], verts[0]);
		CHECK (c12 > 0.0f);


		TriReductionTestCleanUp (verts, tris);
	}


	TEST (TriReduction_CollapseEdgeCost3)
	{
		TRVerts verts;
		TRTris tris;
		BuildTriFourVertices (verts, tris);

		// Edge (0,1) should not be collapsible.
		float c01 = tri_reduction_edge_collapse_cost (verts[0], verts[1]);
		CHECK (c01 > 0.0f);

		// Edge (1,2) should not be collapsible. Collapsing this edge will not
		// result in creating triangles.
		float c12 = tri_reduction_edge_collapse_cost (verts[1], verts[2]);
		CHECK (c12 > 0.0f);


		TriReductionTestCleanUp (verts, tris);
	}


	TEST (TriReduction_CollapseEdge)
	{
		TRVerts verts;
		TRTris tris;
		BuildSquareFiveVertices (verts, tris);

		// Edge (2,0) should be collapsible with minimal cost.
		float c20 = tri_reduction_edge_collapse_cost (verts[2], verts[0]);
		CHECK (c20 < TRI_RED_NO_COST);

		tri_reduction_collapse_edge (verts, tris, verts[2], verts[0]);
		CHECK (
			(tris[0] == NULL || !tris[0]->HasVertex (verts[0]) || !tris[0]->HasVertex (verts[2])) &&
			(tris[1] == NULL || !tris[1]->HasVertex (verts[0]) || !tris[1]->HasVertex (verts[2])) &&
			(tris[2] == NULL || !tris[2]->HasVertex (verts[0]) || !tris[2]->HasVertex (verts[2])) &&
			(tris[3] == NULL || !tris[3]->HasVertex (verts[0]) || !tris[3]->HasVertex (verts[2])));


		TriReductionTestCleanUp (verts, tris);
	}


	TEST (TriReduction_ComputeAllEdgeCollapseCost)
	{
		TRVerts verts;
		TRTris tris;
		BuildSquareFiveVertices (verts, tris);

		// Compute all costs. The only collapsible edge should be center one (v2).
		tri_reduction_compute_all_edge_collapse_cost (verts);

		for (int i = 0; i < verts.size (); ++i)
		{
			if (i == 2)
				CHECK (verts[i]->collapseCost < TRI_RED_NO_COST);
			else
				CHECK (verts[i]->collapseCost > TRI_RED_NO_COST);
		}
	}


	TEST (TriReduction_IsFippingNormal)
	{
		// Create a triangle for which collapsing v2->v3 will cause a normal inversion.
		//		v0
		//       \  \
		//        \   \
		//		  v1___v2
		//        |   /
		//        | /
		//        v3
		TRVerts verts;
		TRTris tris;

		verts.push_back (new HMVertex (Vector3f (0, 0, 0), 0));
		verts.push_back (new HMVertex (Vector3f (2, 0, 2), 1));
		verts.push_back (new HMVertex (Vector3f (5, 0, 2), 2));
		verts.push_back (new HMVertex (Vector3f (2, 0, 5), 3));

		tris.push_back (new HMTriangle (verts[0], verts[2], verts[1], 0));
		tris.push_back (new HMTriangle (verts[1], verts[2], verts[3], 1));

		bool bInvertNormal = tri_reduction_is_flipping_normal (verts[2], verts[3]);
		CHECK (bInvertNormal);
	}


	TEST (TriReduction_Convert)
	{
		// Create a square with 4 triangles, sharing the same vertex in the middle.
		//	 v0	______ v1                ______
		//     |\    /|                 |\ 0  /|
		//     |  \/  |				    |  \/  |
		//     |  /\  |				    | 1/\2 |
		//     | /v2\ |				    | /  \ |
		//     |/____\|				    |/_3__\|
		//   v3        v4
		HMPoly poly;
		poly.vertices.push_back (0.0f); poly.vertices.push_back (0.0f); poly.vertices.push_back (0.0f);
		poly.vertices.push_back (2.0f); poly.vertices.push_back (0.0f); poly.vertices.push_back (0.0f);
		poly.vertices.push_back (1.0f); poly.vertices.push_back (0.0f); poly.vertices.push_back (1.0f);
		poly.vertices.push_back (0.0f); poly.vertices.push_back (0.0f); poly.vertices.push_back (2.0f);
		poly.vertices.push_back (2.0f); poly.vertices.push_back (0.0f); poly.vertices.push_back (2.0f);

		poly.triIndices.push_back (0); poly.triIndices.push_back (1); poly.triIndices.push_back (2); poly.triIndices.push_back (-1);
		poly.triIndices.push_back (1); poly.triIndices.push_back (3); poly.triIndices.push_back (2); poly.triIndices.push_back (-1);

		poly.nverts = 5;
		poly.ntris = 2;

		TRVerts verts;
		TRTris tris;
		tri_reduction_convert (poly, verts, tris);

		tri_reduction_convert_back (verts, tris, poly);
		tri_reduction_clean_up (verts, tris);
	}
}


#endif
