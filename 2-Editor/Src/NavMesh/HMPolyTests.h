#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"

SUITE (HMPolyTests)
{
	TEST (StdList_AddUnique)
	{
		HMVertex v;
		std::list<HMVertex*> vList;
		add_unique (vList, &v);
		add_unique (vList, &v);
		CHECK (vList.size () == 1);
	}

	TEST (StdList_Contains)
	{
		HMVertex v0;
		HMVertex v1 (Vector3f (0.f, 0.f, 0.f), 0);
		std::list<HMVertex*> vList;
		add_unique (vList, &v0);
		CHECK (contains (vList, &v0));
		CHECK (!contains (vList, &v1));
	}

	TEST (StdList_Remove)
	{
		HMVertex v0 (Vector3f (0.f, 0.f, 0.f), 0);
		HMVertex v1 (Vector3f (1.f, 1.f, 1.f), 1);
		std::list<HMVertex*> vList;
		vList.push_back (&v0);
		vList.push_back (&v1);
		CHECK (vList.size () == 2);
		CHECK (contains (vList, &v0));
		CHECK (contains (vList, &v1));

		remove (vList, &v0);
		CHECK (vList.size () == 1);
		CHECK (!contains (vList, &v0));
		CHECK (contains (vList, &v1));
	}

	TEST (Vertex_Neighbors)
	{
		HMVertex v0 (Vector3f (0.f, 0.f, 0.f), 0);
		HMVertex v1 (Vector3f (1.f, 1.f, 1.f), 1);
		HMVertex v2 (Vector3f (2.f, 2.f, 2.f), 2);
		HMTriangle t (&v0, &v1, &v2, 0);

		CHECK (contains (v0.neighbor, &v1));
		CHECK (contains (v0.neighbor, &v2));
		CHECK (contains (v1.neighbor, &v0));
		CHECK (contains (v1.neighbor, &v2));
		CHECK (contains (v2.neighbor, &v0));
		CHECK (contains (v2.neighbor, &v1));

		CHECK (!contains (v0.neighbor, &v0));
		CHECK (!contains (v1.neighbor, &v1));
		CHECK (!contains (v2.neighbor, &v2));
	}

	TEST (Vertex_Face)
	{
		HMVertex v0 (Vector3f (0.f, 0.f, 0.f), 0);
		HMVertex v1 (Vector3f (1.f, 1.f, 1.f), 1);
		HMVertex v2 (Vector3f (2.f, 2.f, 2.f), 2);
		HMTriangle t (&v0, &v1, &v2, 0);

		CHECK (v0.tris.size () == 1 && contains (v0.tris, &t));
		CHECK (v1.tris.size () == 1 && contains (v0.tris, &t));
		CHECK (v2.tris.size () == 1 && contains (v0.tris, &t));
	}

	TEST (Triangle_HasVertex)
	{
		HMVertex v0 (Vector3f (0.f, 0.f, 0.f), 0);
		HMVertex v1 (Vector3f (1.f, 1.f, 1.f), 1);
		HMVertex v2 (Vector3f (2.f, 2.f, 2.f), 2);
		HMTriangle t (&v0, &v1, &v2, 0);
		CHECK (t.HasVertex (&v0));
		CHECK (t.HasVertex (&v1));
		CHECK (t.HasVertex (&v2));
	}

	TEST (Triangle_Normal)
	{
		HMVertex v0 (Vector3f (0.f, 0.f, 0.f), 0);
		HMVertex v1 (Vector3f (0.f, 0.f, 1.f), 1);
		HMVertex v2 (Vector3f (1.f, 0.f, 0.f), 2);
		Vector3f n1 (0.f, 1.f, 0.f);
		Vector3f n2 (0.f, -1.f, 0.f);
		HMTriangle t (&v0, &v1, &v2, 0);
		CHECK (CompareApproximately (t.normal, n1) || CompareApproximately (t.normal, n2));
	}

	TEST (Triangle_ReplaceVertex)
	{
		HMVertex v0 (Vector3f (0.f, 0.f, 0.f), 0);
		HMVertex v1 (Vector3f (1.f, 1.f, 1.f), 1);
		HMVertex v2 (Vector3f (2.f, 2.f, 2.f), 2);
		HMVertex v3 (Vector3f (3, 3, 3), 3);
		HMTriangle t (&v0, &v1, &v2, 0);
		CHECK (t.HasVertex (&v0));

		t.ReplaceVertex (&v0, &v3);
		CHECK (!t.HasVertex (&v0));
		CHECK (t.HasVertex (&v3));
		CHECK (!contains (v1.neighbor, &v0));
		CHECK (!contains (v2.neighbor, &v0));
		CHECK (!contains (v3.neighbor, &v0));
		CHECK (contains (v1.neighbor, &v3));
		CHECK (contains (v2.neighbor, &v3));
		CHECK (contains (v3.neighbor, &v1));
		CHECK (contains (v3.neighbor, &v2));
		CHECK (!contains (v0.tris, &t));
		CHECK (contains (v3.tris, &t));
	}
}

#endif
