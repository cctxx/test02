#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Testing/Testing.h"

class GameObjectFixture
{
protected:
	GameObject* NewGameObject()
	{
		return NEW_OBJECT_RESET_AND_AWAKE(GameObject);
	}
};

SUITE (GameObjectUtilityTests)
{
	TEST (CreateGameObjectTest)
	{
		const char* name = "TestGameObject";
		GameObject& go = CreateGameObject (name, "Transform", "MeshRenderer", NULL);

		CHECK_EQUAL (go.GetName(), name);
		CHECK_EQUAL (go.GetComponentCount(), 2);
		CHECK (go.IsActive());

		DestroyObjectHighLevel(&go);
	}

	TEST (CreateGameObjectWithFlagsTest)
	{
		int flags = 2;
		GameObject& go = CreateGameObjectWithHideFlags ("TestGameObject", true, flags, NULL);

		CHECK (go.IsActive());
		CHECK_EQUAL (go.GetHideFlags(), flags);

		DestroyObjectHighLevel(&go);

		GameObject& go1 = CreateGameObjectWithHideFlags ("TestGameObject", false, flags, NULL);

		CHECK (!go1.IsActive());
		CHECK_EQUAL (go1.GetHideFlags(), flags);

		DestroyObjectHighLevel(&go1);
	}

	TEST_FIXTURE (GameObjectFixture, AddComponentsTest)
	{
		GameObject* go = NewGameObject();

		AddComponent(*go, "Transform", NULL);
		CHECK_EQUAL (go->GetComponentCount(), 1);

		AddComponent(*go, ClassID(MeshRenderer), NULL, NULL);
		CHECK_EQUAL (go->GetComponentCount(), 2);

		// Transform and MeshRenderer don't support multiple inclusion.
		EXPECT (Error, "Can't add component 'Transform'");
		EXPECT (Error, "Can't add component 'MeshRenderer'");
		AddComponents(*go, "Transform", "MeshRenderer", "Skybox", NULL);
		CHECK_EQUAL (go->GetComponentCount(), 3);

#if ENABLE_SPRITES
		// SpriteRenderer can't be added to a GO with a MeshRenderer - conflicting classes.
		EXPECT (Error, "Can't add component 'SpriteRenderer'");
		AddComponents(*go, "SpriteRenderer", NULL);
		CHECK_EQUAL (go->GetComponentCount(), 3);
#endif

		// Skybox supports multiple inclusion.
		AddComponent(*go, ClassID(Skybox), NULL);
		CHECK_EQUAL (go->GetComponentCount(), 4);

		DestroyObjectHighLevel(go);
	}

	TEST_FIXTURE (GameObjectFixture, CanAddorRemoveComponentTest)
	{
		GameObject* go = NewGameObject();

		AddComponents(*go, "Transform", "MeshFilter", "Skybox", NULL);

		CHECK (!CanAddComponent(*go, ClassID(Transform)));
		CHECK (CanAddComponent(*go, ClassID(Skybox)));

		// Refer to the InitComponentRequirements() function for details.
		CHECK ( !CanRemoveComponent(go->GetComponentT<Transform>(ClassID(Transform)), NULL) );
		DestroyObjectHighLevel(go);
	}

	TEST_FIXTURE (GameObjectFixture, FindWithTagTest)
	{
		GameObject* go = NewGameObject();
		int tag = 2;

		CHECK (FindGameObjectWithTag(tag) == NULL);

		go->SetTag(tag);
		CHECK (FindGameObjectWithTag(tag) == NULL);

		go->Activate();
		CHECK (FindGameObjectWithTag(tag) != NULL);

		GameObject* go1 = NewGameObject();
		go1->Activate();
		go1->SetTag(tag);

		std::vector<Unity::GameObject*> gos;
		FindGameObjectsWithTag(tag, gos);
		CHECK_EQUAL(gos.size(), 2);

		DestroyObjectHighLevel(go);
	}
}

#endif
