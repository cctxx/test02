#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"

void TriTessellationBuildSquareFiveVertices (
	TRVerts& verts,
	TRTris& tris,
	bool planar)
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

	int y = planar ? 0 : 1;
	verts.push_back (new HMVertex (Vector3f (1, y, 1), 2));

	verts.push_back (new HMVertex (Vector3f (0, 0, 2), 3));
	verts.push_back (new HMVertex (Vector3f (2, 0, 2), 4));

	tris.push_back (new HMTriangle (verts[0], verts[1], verts[2], 0));
	tris.push_back (new HMTriangle (verts[0], verts[2], verts[3], 1));
	tris.push_back (new HMTriangle (verts[1], verts[4], verts[2], 2));
	tris.push_back (new HMTriangle (verts[2], verts[4], verts[3], 3));
}


void TriTessellationBuildSquareFourVertices (
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


void TriTessellationBuildSquareFourVertices3D (
	TRVerts& verts,
	TRTris& tris)
{
	// Create a square with 4 triangles, sharing the same vertex in the middle.
	//	 v0	_____ v1                  _____            Side view
	//     |    /|                   |    /|              / v1, v3
	//     |   / |				     | 0 / |	        /
	//     |  /  |				     |  /  |	      /
	//     | /   |				     | / 1 |	    v0, v2
	//     |/____|				     |/____|
	//   v2       v3
	verts.push_back (new HMVertex (Vector3f (0, 0, 0), 0));
	verts.push_back (new HMVertex (Vector3f (2, 2, 0), 1));
	verts.push_back (new HMVertex (Vector3f (0, 0, 2), 2));
	verts.push_back (new HMVertex (Vector3f (2, 2, 2), 3));

	tris.push_back (new HMTriangle (verts[0], verts[1], verts[2], 0));
	tris.push_back (new HMTriangle (verts[1], verts[3], verts[2], 1));
}


void TriTessellationTestCleanUp (
	TRVerts& verts,
	TRTris& tris)
{
	for (int i = 0; i < tris.size (); ++i)
	{
		if (tris[i] != NULL)
			delete tris[i];
	}
	for (int i = 0; i < verts.size (); ++i)
	{
		if (verts[i] != NULL)
			delete verts[i];
	}

	tris.clear ();
	verts.clear ();
}

bool TriTessellationCheckContainsVertex (TTTri& tri, HMVertex* v0, HMVertex* v1, HMVertex* v2)
{
	return
		(tri.v[0] == v0 || tri.v[1] == v0 || tri.v[2] == v0) &&
		(tri.v[0] == v1 || tri.v[1] == v1 || tri.v[2] == v1) &&
		(tri.v[0] == v2 || tri.v[1] == v2 || tri.v[2] == v2);
}

#define CHECK_CONTOUR(edge, v0, v1)							\
	((edge.v[0]->idx == v0 && edge.v[1]->idx == v1) ||	\
	  (edge.v[0]->idx == v1 && edge.v[1]->idx == v0))


SUITE (TriTessellationTests)
{
	TEST (TriTessellation_IsPlanar)
	{
		TRVerts verts;
		TRTris tris;
		TriTessellationBuildSquareFiveVertices (verts, tris, true);

		bool bPlanar = tri_tessellation_is_planar (tris);
		CHECK (bPlanar);

		TriTessellationTestCleanUp (verts, tris);

		// Change vertex v2 so the surface is no longer planar.
		TriTessellationBuildSquareFiveVertices (verts, tris, false);

		bPlanar = tri_tessellation_is_planar (tris);
		CHECK (!bPlanar);

		TriTessellationTestCleanUp (verts, tris);
	}


	TEST (TriTessellation_IsPlanar3D)
	{
		TRVerts verts;
		TRTris tris;
		TriTessellationBuildSquareFourVertices3D (verts, tris);

		bool bPlanar = tri_tessellation_is_planar (tris);
		CHECK (bPlanar);

		TriTessellationTestCleanUp (verts, tris);
	}


	TEST (TriTessellation_FindContour)
	{
		TRVerts verts;
		TRTris tris;
		TriTessellationBuildSquareFiveVertices (verts, tris, true);

		TTEdges contour;
		tri_tessellation_find_contour (verts, contour);
		CHECK (verts.size () == 5);
		CHECK (contour.size () == 4);
		for (int i = 0; i < contour.size (); ++i)
		{
			bool success = CHECK_CONTOUR (contour[i], 0, 1);
			success     |= CHECK_CONTOUR (contour[i], 1, 4);
			success     |= CHECK_CONTOUR (contour[i], 4, 3);
			success     |= CHECK_CONTOUR (contour[i], 3, 0);
			CHECK (success);
		}
		TriTessellationTestCleanUp (verts, tris);

		contour.clear ();
		TriTessellationBuildSquareFourVertices (verts, tris);
		tri_tessellation_find_contour (verts, contour);
		CHECK (verts.size () == contour.size ());
		for (int i = 0; i < contour.size (); ++i)
		{
			bool success = CHECK_CONTOUR (contour[i], 0, 1);
			success     |= CHECK_CONTOUR (contour[i], 1, 3);
			success     |= CHECK_CONTOUR (contour[i], 3, 2);
			success     |= CHECK_CONTOUR (contour[i], 2, 0);
			CHECK (success);
		}
		TriTessellationTestCleanUp (verts, tris);
	}


	TEST (TriTessellation_FindContour3D)
	{
		TRVerts verts;
		TRTris tris;
		TriTessellationBuildSquareFourVertices3D (verts, tris);

		TTEdges contour;
		tri_tessellation_find_contour (verts, contour);
		CHECK (verts.size () == 4);
		CHECK (contour.size () == 4);
		for (int i = 0; i < contour.size (); ++i)
		{
			bool success = CHECK_CONTOUR (contour[i], 0, 1);
			success     |= CHECK_CONTOUR (contour[i], 1, 3);
			success     |= CHECK_CONTOUR (contour[i], 3, 2);
			success     |= CHECK_CONTOUR (contour[i], 2, 0);
			CHECK (success);
		}
		TriTessellationTestCleanUp (verts, tris);
	}


	TEST (TriTessellation_ConnectContour)
	{
		// Create a square contour.
		//       e0
		//	 v0	_____ v1
		//     |     |
		//     |     |
		//  e2 |     | e3
		//     |     |
		//     | ____|
		//   v3       v2
		//       e1
		TRVerts verts;
		verts.push_back (new HMVertex (Vector3f (0, 0, 0), 0));
		verts.push_back (new HMVertex (Vector3f (1, 0, 0), 1));
		verts.push_back (new HMVertex (Vector3f (1, 0, 1), 2));
		verts.push_back (new HMVertex (Vector3f (0, 0, 1), 3));

		TTEdges contour;
		contour.push_back (TTEdge (verts[0], verts[1]));
		contour.push_back (TTEdge (verts[1], verts[2]));
		contour.push_back (TTEdge (verts[2], verts[3]));
		contour.push_back (TTEdge (verts[3], verts[0]));

		TRVerts connVerts;
		CHECK (tri_tessellation_connect_contour (contour, connVerts));

		CHECK (connVerts.size () == 4);
		for (int i = 0; i < connVerts.size (); ++i)
		{
			CHECK (connVerts[i]->idx == i);
		}


		// Invert second edge.
		contour.clear ();
		connVerts.clear ();
		contour.push_back (TTEdge (verts[0], verts[1]));
		contour.push_back (TTEdge (verts[2], verts[1]));
		contour.push_back (TTEdge (verts[2], verts[3]));
		contour.push_back (TTEdge (verts[3], verts[0]));

		CHECK (tri_tessellation_connect_contour (contour, connVerts));

		CHECK (connVerts.size () == 4);
		for (int i = 0; i < connVerts.size (); ++i)
		{
			CHECK (connVerts[i]->idx == i);
		}


		// Mix edges.
		contour.clear ();
		connVerts.clear ();
		contour.push_back (TTEdge (verts[0], verts[1]));
		contour.push_back (TTEdge (verts[2], verts[3]));
		contour.push_back (TTEdge (verts[0], verts[3]));
		contour.push_back (TTEdge (verts[1], verts[2]));

		CHECK (tri_tessellation_connect_contour (contour, connVerts));

		CHECK (connVerts.size () == 4);
		for (int i = 0; i < connVerts.size (); ++i)
		{
			CHECK (connVerts[i]->idx == i);
		}

		for (int i = 0; i < verts.size (); ++i)
		{
			if (verts[i] != NULL)
				delete verts[i];
		}
	}


	TEST (TriTessellation_ConnectContour3D)
	{
		TRVerts verts;
		TRTris tris;
		TriTessellationBuildSquareFourVertices3D (verts, tris);

		TTEdges contour;
		TRVerts connVerts;
		std::list<TTLocalVertex> localVerts;
		tri_tessellation_find_contour (verts, contour);
		tri_tessellation_connect_contour (contour, connVerts);

		CHECK (connVerts.size () == 4);
		int expected[4] = {0, 1, 3, 2};
		for (int i = 0; i < connVerts.size (); ++i)
		{
			CHECK (connVerts[i]->idx == expected[i]);
		}
	}


	TEST (TriTessellation_TransformLocal)
	{
		TRVerts verts;
		TRTris tris;
		TriTessellationBuildSquareFourVertices3D (verts, tris);

		TTEdges contour;
		TRVerts connVerts;
		TTLocalVertices localVerts;
		tri_tessellation_find_contour (verts, contour);
		tri_tessellation_connect_contour (contour, connVerts);
		Quaternionf localToWorld = tri_tessellation_transform_local (connVerts, tris[0]->normal, localVerts);

		// Y coordinate should have been flatten.
		int i = 0;
		for (std::list<TTLocalVertex>::iterator it = localVerts.begin (); it != localVerts.end (); ++it)
		{
			TTLocalVertex& lv = (*it);
			Vector3f v (lv.localPos.x, 0, lv.localPos.y);
			v = RotateVectorByQuat (localToWorld, v);
			CHECK (CompareApproximately (v, connVerts[i]->pos));
			++i;
		}

		TriTessellationTestCleanUp (verts, tris);
	}


	TEST (TriTessellation_IsConvex)
	{
		// Convex edges.
		//       e0
		//	 v0	_____ v1
		//			|
		//			| e1
		//			|
		//			  v2
		Vector2f v0 = Vector2f (0, 0);
		Vector2f v1 = Vector2f (1, 0);
		Vector2f v2 = Vector2f (1, 1);

		CHECK (tri_tessellation_is_convex (v0, v1, v2));


		// Concave edges.
		//       e0
		//	 v0	_____
		//		|
		//	 e1	|
		//		|
		//	 v1 -----  v2
		v1 = Vector2f (0, 1);
		CHECK (!tri_tessellation_is_convex (v0, v1, v2));
	}


	TEST (TriTessellation_IsInside)
	{
		// Triangle. We will test if p0, p1 and p2 is inside.
		//    v0 _____ v1
		//       |   /
		//    p0 |p1/
		//       | /
		//    v2 |/
		//       p2
		Vector2f v0 (2, 2);
		Vector2f v1 (6, 2);
		Vector2f v2 (2, 6);
		Vector2f p0 (1, 2);
		Vector2f p1 (3, 3);
		Vector2f p2 (2, 5);

		CHECK (!tri_tessellation_is_inside (v0, v1, v2, p0));
		CHECK (tri_tessellation_is_inside (v0, v1, v2, p1));
		CHECK (!tri_tessellation_is_inside (v0, v1, v2, p2));

		CHECK (!tri_tessellation_is_inside (v0, v2, v1, p0));
		CHECK (tri_tessellation_is_inside (v0, v2, v1, p1));
		CHECK (!tri_tessellation_is_inside (v0, v2, v1, p2));
	}


	TEST (TriTessellation_FindConcaveVerts)
	{
		// Polygon with a concave point. See if the good point is found.
		//    v0______ v1
		//     |       |
		//     |   v3  |
		//     |  /  \ |
		//     | /    \|
		//     v4      v2
		Vector2f v[5];
		v[0] = Vector2f (0, 0);
		v[1] = Vector2f (4, 0);
		v[2] = Vector2f (4, 4);
		v[3] = Vector2f (2, 2);
		v[4] = Vector2f (0, 4);
		TTLocalVertices localVerts;
		for (int i = 0; i < 5; ++i)
			localVerts.push_back (TTLocalVertex (NULL, v[i]));

		TTLocalVertices concaveVerts;
		tri_tessellation_find_concave_verts (localVerts, concaveVerts);

		CHECK (concaveVerts.size () == 1);
		Vector2f u = (*(concaveVerts.begin ())).localPos;
		CHECK (CompareApproximately (v[3], u));


		// Test with a concave point as the last set of segments tested.
		for (int i = 4; i < 4+5; ++i)
			localVerts.push_back (TTLocalVertex (NULL, v[i%5]));

		CHECK (concaveVerts.size () == 1);
		u = (*(concaveVerts.begin ())).localPos;
		CHECK (CompareApproximately (v[3], u));
	}

	TEST (TriTessellation_Kong1)
	{
		// Simple square. Tessellate!
		//    v0______ v1
		//     |       |
		//     |       |
		//     |       |
		//     |_______|
		//     v3      v2
		Vector2f v[4];
		v[0] = Vector2f (0, 0);
		v[1] = Vector2f (4, 0);
		v[2] = Vector2f (4, 4);
		v[3] = Vector2f (0, 2);
		TTLocalVertices localVerts;
		for (int i = 0; i < 4; ++i)
			localVerts.push_back (TTLocalVertex ((HMVertex*)i, v[i]));

		TTTris tris;
		tri_tessellation_kong (localVerts, tris);
		CHECK (tris.size () == 2);
		CHECK (TriTessellationCheckContainsVertex (tris[0], (HMVertex*)0, (HMVertex*)1, (HMVertex*)2));
		CHECK (TriTessellationCheckContainsVertex (tris[1], (HMVertex*)2, (HMVertex*)3, (HMVertex*)0));
	}

	TEST (TriTessellation_Kong2)
	{
		// Polygon with a concave point. Tessellate!
		//    v0______ v1
		//     |       |
		//     |   v3  |
		//     |  /  \ |
		//     | /    \|
		//     v4      v2
		Vector2f v[5];
		v[0] = Vector2f (0, 0);
		v[1] = Vector2f (4, 0);
		v[2] = Vector2f (4, 4);
		v[3] = Vector2f (2, 2);
		v[4] = Vector2f (0, 4);
		TTLocalVertices localVerts;
		for (int i = 0; i < 5; ++i)
			localVerts.push_back (TTLocalVertex ((HMVertex*)i, v[i]));

		TTTris tris;
		tri_tessellation_kong (localVerts, tris);
		CHECK (tris.size () == 3);
		CHECK (TriTessellationCheckContainsVertex (tris[0], (HMVertex*)0, (HMVertex*)1, (HMVertex*)2));
		CHECK (TriTessellationCheckContainsVertex (tris[1], (HMVertex*)2, (HMVertex*)3, (HMVertex*)0));
		CHECK (TriTessellationCheckContainsVertex (tris[2], (HMVertex*)3, (HMVertex*)4, (HMVertex*)0));
	}


	TEST (TriTessellation_PlanarSurfaceRetessallation)
	{
		TRVerts verts;
		TRTris tris;
		TriTessellationBuildSquareFiveVertices (verts, tris, true);

		HMPoly poly;
		bool bSuccess = tri_tessellation_planar_surface_retessallation (verts, tris, poly);
		CHECK (bSuccess);
		CHECK (poly.nverts == 4);
		CHECK (poly.ntris  == 2);

		TriTessellationTestCleanUp (verts, tris);
	}
}


#endif

