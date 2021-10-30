#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemRenderer.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Testing/TestFixtures.h"


SUITE (ParticleSystemRendererTests)
{
	typedef ObjectTestFixture<ParticleSystemRenderer> Fixture;

	TEST_FIXTURE (Fixture, DeletingMeshClearsOutCachedMeshPointers)
	{
		// Arrange.
		PPtr<Mesh> mesh (NEW_OBJECT_RESET_AND_AWAKE (Mesh));
		m_ObjectUnderTest->SetMesh (mesh);

		// Act.
		DestroySingleObject (mesh);

		// Assert.
		CHECK (m_ObjectUnderTest->GetData().cachedMesh[0] == NULL);
	}
}

#endif
