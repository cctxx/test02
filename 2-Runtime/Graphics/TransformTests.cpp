#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
#include "Transform.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Testing/TestFixtures.h"
#include "Runtime/Testing/Testing.h"
#include "Editor/Src/Prefabs/Prefab.h"


class Prefab;

SUITE (TransformTests)
{
	class TransformFixture : TestFixtureBase
	{
	protected:
		Transform* MakeTransform ()
		{
			Transform* tf = NewTestObject<Transform> ();
			GameObject* go = NewTestObject<GameObject> ();
			go->AddComponentInternal (tf);
			return tf;
		}

		Prefab* MakePrefabParent ()
		{
			Prefab* prefab = NewTestObject<Prefab> ();
			prefab->m_IsPrefabParent = true;
			return prefab;
		}
	};

	TEST_FIXTURE (TransformFixture, Has_Null_Parrent_By_Default)
	{
		Transform* transform = MakeTransform ();
		CHECK_EQUAL ((Transform*)0, transform->GetParent ());
	}

	TEST_FIXTURE (TransformFixture, SetParent_Returns_True_If_New_Parent_Is_Null)
	{
		Transform* transform = MakeTransform ();
		CHECK_EQUAL (true, transform->SetParent (0));
	}

	TEST_FIXTURE (TransformFixture, SetParent_Returns_False_When_GameObject_Is_Being_Destroyed)
	{
		Transform* transform = MakeTransform ();
		Transform* parent = MakeTransform ();
		transform->GetGameObject().WillDestroyGameObject ();
		CHECK_EQUAL (false, transform->SetParent (parent));
	}

	TEST_FIXTURE (TransformFixture, SetParent_Returns_False_When_GameObject_Of_New_Parent_Is_Being_Destroyed)
	{
		Transform* transform = MakeTransform ();
		Transform* parent = MakeTransform ();
		parent->GetGameObject().WillDestroyGameObject ();
		CHECK_EQUAL (false, transform->SetParent (parent));
	}

	TEST_FIXTURE (TransformFixture, SetParent_Returns_False_If_New_Parent_Is_A_Child)
	{
		Transform* parent = MakeTransform ();
		Transform* child = MakeTransform ();
		child->SetParent (parent);
		CHECK_EQUAL (false, parent->SetParent (child));
	}

	#if UNITY_EDITOR
	TEST_FIXTURE (TransformFixture, SetParent_With_Disable_Parenting_From_Prefab_Returns_False_If_Tranform_Resides_In_Prefab_EditorOnly)
	{
		Transform* transform = MakeTransform ();
		transform->m_Prefab = PPtr<Prefab> (MakePrefabParent ());

		EXPECT (Error, "Setting the parent of a transform which resides in a prefab is disabled");
		CHECK_EQUAL (false, transform->SetParent (MakeTransform(), Transform::kWorldPositionStays));
	}
	#endif

	TEST (ChildPosition_Test)
	{
		GameObject& go = CreateGameObject ("parent", "Transform", NULL);
		go.GetComponent (Transform).SetLocalScale (Vector3f (1.0F, 1.0F, 0.1F));

		GameObject& child = CreateGameObject ("child", "Transform", NULL);

		child.GetComponent (Transform).SetParent (&go.GetComponent (Transform), Transform::kLocalPositionStays);

		child.GetComponent (Transform).SetLocalPosition (Vector3f (0.0F, 0.0F, 10.0F));
		child.GetComponent (Transform).SetLocalScale (Vector3f (1.0F, 1.0F, 1.0F));
		child.GetComponent (Transform).SetLocalRotation (Quaternionf::identity ());

		CHECK ( CompareApproximately (child.GetComponent (Transform).GetPosition (), Vector3f (0.0F,0.0F,1.0F)) );

		DestroyObjectHighLevel (&go);
	}
}

#endif // ENABLE_UNIT_TESTS
