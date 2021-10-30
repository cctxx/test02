#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"
#include "DynamicMesh.h"

SUITE (DynamicMeshTests)
{
	struct DynamicMeshTestFixture
	{
		DynamicMeshTestFixture ()
		{
			data = 0;
			data2 = 2;

			goodPolygon.push_back (Vector3f (0,0,0));
			goodPolygon.push_back (Vector3f (0,0,1));
			goodPolygon.push_back (Vector3f (1,0,1));

			goodPolygonNeighbor.push_back (Vector3f (0,0,0));
			goodPolygonNeighbor.push_back (Vector3f (1,0,1));
			goodPolygonNeighbor.push_back (Vector3f (1,0,0));

			upsideDownPolygon.push_back (Vector3f (0,0,0));
			upsideDownPolygon.push_back (Vector3f (1,0,1));
			upsideDownPolygon.push_back (Vector3f (0,0,1));

			degeneratePolygon.push_back (Vector3f (0,0,0));
			degeneratePolygon.push_back (Vector3f (1,0,0));
			degeneratePolygon.push_back (Vector3f (2,0,0));

			degeneratePolygon2.push_back (Vector3f (0,0,0));
			degeneratePolygon2.push_back (Vector3f (1,0,0));
		}

		DynamicMesh mesh;
		unsigned char data;
		unsigned char data2;

		DynamicMesh::Polygon goodPolygon;
		DynamicMesh::Polygon goodPolygonNeighbor;
		DynamicMesh::Polygon degeneratePolygon;
		DynamicMesh::Polygon degeneratePolygon2;
		DynamicMesh::Polygon upsideDownPolygon;
	};

	// Create a container for DynamicMesh to use for carving.
	// Hull is a simple half-space defined by world-space position and normal.
	DynamicMesh::HullContainer HullsFromNormalAndPosition (const Vector3f& normal, const Vector3f& position)
	{
		Plane plane;
		plane.SetNormalAndPosition (normal, position);

		DynamicMesh::Hull hull;
		hull.push_back (plane);

		DynamicMesh::HullContainer hulls;
		hulls.push_back (hull);

		return hulls;
	}

	TEST_FIXTURE (DynamicMeshTestFixture, Construction)
	{
		CHECK (mesh.PolyCount () == 0);
		CHECK (mesh.VertCount () == 0);
	}

	TEST_FIXTURE (DynamicMeshTestFixture, AddPolygon)
	{
		mesh.AddPolygon (goodPolygon, data);

		CHECK (mesh.PolyCount () == 1);
		CHECK (mesh.VertCount () == 3);
	}

	TEST_FIXTURE (DynamicMeshTestFixture, AddPolygon_IgnoreDegeneratePolygon)
	{
		mesh.AddPolygon (degeneratePolygon, data);
		mesh.AddPolygon (degeneratePolygon2, data);

		CHECK (mesh.PolyCount () == 0);
		CHECK (mesh.VertCount () == 0);
	}

	TEST_FIXTURE (DynamicMeshTestFixture, AddPolygon_IgnoreUpsideDown)
	{
		mesh.AddPolygon (upsideDownPolygon, data);

		CHECK (mesh.PolyCount () == 0);
		CHECK (mesh.VertCount () == 0);
	}

	TEST_FIXTURE (DynamicMeshTestFixture, AddPolygon_SameTwice)
	{
		mesh.AddPolygon (goodPolygon, data);
		mesh.AddPolygon (goodPolygon, data);

		CHECK (mesh.PolyCount () == 2);
		CHECK (mesh.VertCount () == 3);
	}

	TEST_FIXTURE (DynamicMeshTestFixture, MergePolygonsWithSameData)
	{
		mesh.AddPolygon (goodPolygon, data);
		mesh.AddPolygon (goodPolygonNeighbor, data);
		mesh.MergePolygons ();

		CHECK (mesh.PolyCount () == 1);
		CHECK (mesh.VertCount () == 4);
	}

	TEST_FIXTURE (DynamicMeshTestFixture, DontMergePolygonsWithDifferentData)
	{
		mesh.AddPolygon (goodPolygon, data);
		mesh.AddPolygon (goodPolygonNeighbor, data2);
		mesh.MergePolygons ();

		CHECK (mesh.PolyCount () == 2);
		CHECK (mesh.VertCount () == 4);
	}

	// Verify mesh contains exactly one triangle. Return the area weighted normal.
	// ie. vector in direction of the triangle normal - with a magnitude equal to the triangle area.
	Vector3f CheckSingleTriangleGetAreaNormal (DynamicMesh& mesh)
	{
		// Verify a single polygon is left
		CHECK (mesh.PolyCount () == 1);

		// .. and it's a triangle
		const DynamicMesh::Poly* poly = mesh.GetPoly (0);
		CHECK (poly->m_VertexCount == 3);

		const Vector3f v0 = Vector3f (mesh.GetVertex (poly->m_VertexIDs[0]));
		const Vector3f v1 = Vector3f (mesh.GetVertex (poly->m_VertexIDs[1]));
		const Vector3f v2 = Vector3f (mesh.GetVertex (poly->m_VertexIDs[2]));
		const Vector3f triangleAreaNormal = 0.5f*Cross (v1 - v0, v2 - v0);
		return triangleAreaNormal;
	}

	TEST_FIXTURE (DynamicMeshTestFixture, ClipTriangleWithPlane_Result_ClippedTriangle)
	{
		// Cut everything z > 0.5f
		DynamicMesh::HullContainer carveHulls = HullsFromNormalAndPosition (-Vector3f::zAxis, Vector3f (0.0f, 0.0f, 0.5f));

		mesh.AddPolygon (goodPolygon, data);
		mesh.ClipPolys (carveHulls);

		// Expect a triangle in the horizontal plane with area 1/8
		const Vector3f expectedAreaNormal = Vector3f (0.0f, 0.125f, 0.0f);
		const Vector3f triangleAreaNormal = CheckSingleTriangleGetAreaNormal (mesh);

		CHECK (CompareApproximately (expectedAreaNormal, triangleAreaNormal));
	}

	TEST_FIXTURE (DynamicMeshTestFixture, ClipTriangleWithPlane_Result_OriginalTriangle)
	{
		// Cut everything z > 1.0f
		DynamicMesh::HullContainer carveHulls = HullsFromNormalAndPosition (-Vector3f::zAxis, Vector3f (0.0f, 0.0f, 1.0f));

		mesh.AddPolygon (goodPolygon, data);

		mesh.ClipPolys (carveHulls);

		// Expect a triangle in the horizontal plane with area 1/2
		const Vector3f expectedAreaNormal = Vector3f (0.0f, 0.5f, 0.0f);
		const Vector3f triangleAreaNormal = CheckSingleTriangleGetAreaNormal (mesh);
		CHECK (CompareApproximately (expectedAreaNormal, triangleAreaNormal));
	}

	TEST_FIXTURE (DynamicMeshTestFixture, ClipTriangleWithPlane_Result_NoTriangle)
	{
		// Cut everything z > 0
		DynamicMesh::HullContainer carveHulls = HullsFromNormalAndPosition (-Vector3f::zAxis, Vector3f (0.0f, 0.0f, 0.0f));

		mesh.AddPolygon (goodPolygon, data);
		mesh.ClipPolys (carveHulls);

		// Verify that the polygon is removed
		CHECK (mesh.PolyCount () == 0);
	}
	
	TEST_FIXTURE (DynamicMeshTestFixture, SplitTriangleIntoTwoPolygons)
	{
		// Split polygon into two
		Vector3f planePos(0.0f, 0.0f, 0.5f);
		Plane planeLeft, planeRight;
		planeLeft.SetNormalAndPosition (-Vector3f::zAxis, planePos);
		planeRight.SetNormalAndPosition (Vector3f::zAxis, planePos);
		
		DynamicMesh::Hull hull;
		hull.push_back (planeLeft);
		hull.push_back (planeRight);
		
		DynamicMesh::HullContainer carveHulls;
		carveHulls.push_back (hull);
		
		mesh.AddPolygon (goodPolygon, data);
		mesh.ClipPolys (carveHulls);
		
		// Verify that the polygon is cut in half
		CHECK (mesh.PolyCount () == 2);
	}

	static bool HasNeighbor (const DynamicMesh::Poly* poly, int neighborId)
	{
		for (int i = 0; i < poly->m_VertexCount; i++)
			if (poly->m_Neighbours[i] == neighborId)
				return true;
		return false;
	}

	TEST_FIXTURE (DynamicMeshTestFixture, CheckMeshConnectivity)
	{
		mesh.AddPolygon (goodPolygon, data);
		mesh.AddPolygon (goodPolygonNeighbor, data2);
		mesh.MergePolygons ();
		mesh.FindNeighbors ();

		CHECK (mesh.PolyCount () == 2);
		CHECK (mesh.VertCount () == 4);

		// Check that polygon A is connected to polygon B.
		const DynamicMesh::Poly* pa = mesh.GetPoly (0);
		CHECK (HasNeighbor (pa, 2)); // One based neighbor indices.

		// Check that polygon B is connected to polygon A.
		const DynamicMesh::Poly* pb = mesh.GetPoly (1);
		CHECK (HasNeighbor (pb, 1));
	}


	static DynamicMesh::Hull HullFromPolygon (const dynamic_array<Vector3f>& points)
	{
		DynamicMesh::Hull hull;
		for (int i = 0, j = points.size()-1; i < points.size(); j = i++)
		{
			Vector3f edgeDir = points[i] - points[j];
			Vector3f edgeNormal = Normalize (Vector3f (-edgeDir.z, 0.0f, edgeDir.x));
			Plane plane;
			plane.SetNormalAndPosition (edgeNormal, points[j]);
			hull.push_back (plane);
		}
		return hull;
	}

	TEST_FIXTURE (DynamicMeshTestFixture, CutTriangleWithRectangle)
	{
		// Cut triangle with rectangle		
		//
		//  o------o
		//  |  x../..x
		//  |  :/    :
		//  | /:     :
		//  o  x.....x
		//
		dynamic_array<Vector3f> points;
		points.push_back (Vector3f (0.25f, 0, 0));
		points.push_back (Vector3f (0.25f, 0, 0.75f));
		points.push_back (Vector3f (1.0f, 0, 0.75f));
		points.push_back (Vector3f (1.0f, 0, 0));
		DynamicMesh::Hull hull = HullFromPolygon (points);

		DynamicMesh::HullContainer carveHulls;
		carveHulls.push_back (hull);

		mesh.AddPolygon (goodPolygon, data);
		mesh.ClipPolys (carveHulls);
		
		// Verify that there are more polygons after clipping, because result is non-convex.
		CHECK (mesh.PolyCount () > 1);
		// Verify that there are 6 vertices.
		CHECK (mesh.VertCount() == 6);
	}
	
	
}

#endif
