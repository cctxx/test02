#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Misc/GOCreation.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Dynamics/CapsuleCollider.h"

SUITE (GameObjectCreationTests)
{
	TEST (CreateSphereTest)
	{
		GameObject* go = CreatePrimitive(kPrimitiveSphere);

		CHECK_EQUAL(go->GetComponentCount(), 4);
		CHECK_EQUAL(go->GetName(), "Sphere");
		CHECK(!go->GetComponent(MeshFilter).GetSharedMesh().IsNull());
		CHECK_EQUAL(go->GetComponent(Renderer).GetMaterialCount(), 1);
	}

	TEST (CreateCubeTest)
	{
		GameObject* go = CreatePrimitive(kPrimitiveCube);

#if ENABLE_PHYSICS 
		int count = 4;
#else 
		int count = 3;
#endif

		CHECK_EQUAL(go->GetComponentCount(), count);
		CHECK_EQUAL(go->GetName(), "Cube");
		CHECK(!go->GetComponent(MeshFilter).GetSharedMesh().IsNull());
		CHECK_EQUAL(go->GetComponent(Renderer).GetMaterialCount(), 1);
	}

	TEST (CreateCylinderTest)
	{
		GameObject* go = CreatePrimitive(kPrimitiveCylinder);

		CHECK_EQUAL(go->GetComponentCount(), 4);
		CHECK_EQUAL(go->GetName(), "Cylinder");
		CHECK(!go->GetComponent(MeshFilter).GetSharedMesh().IsNull());
		CHECK_EQUAL(go->GetComponent(Renderer).GetMaterialCount(), 1);

#if ENABLE_PHYSICS 
		CHECK_EQUAL(go->GetComponent (CapsuleCollider).GetHeight(), 2.0f);
#endif
	}
}

#endif