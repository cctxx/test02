#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/Testing/Testing.h"

class GameObjectFixture
{
protected:
	GameObject* NewGameObject()
	{
		return NEW_OBJECT_RESET_AND_AWAKE(GameObject);
	}

	Unity::Component* NewComponent() 
	{
		return NEW_OBJECT_RESET_AND_AWAKE(Unity::Component);
	}
};

SUITE (GameObjectTests)
{
	TEST_FIXTURE (GameObjectFixture, AddandRemoveComponentTest)
	{
		GameObject* go = NewGameObject();

		Unity::Component* component = NewComponent();
		go->AddComponentInternal(component);
		CHECK_EQUAL (go->GetComponentCount(), 1);

		// Delete by RemoveComponentFromGameObjectInternal().
		go->RemoveComponentFromGameObjectInternal(*component);
		CHECK_EQUAL (go->GetComponentCount(), 0);

		go->AddComponentInternal(component);
		CHECK_EQUAL (go->GetComponentCount(), 1);

		// Delete by RemoveComponentAtIndex().
		go->RemoveComponentAtIndex(0);
		CHECK_EQUAL (go->GetComponentCount(), 0);

		go->AddComponentInternal(component);
		go->AwakeFromLoad (kDefaultAwakeFromLoad);
		DestroyObjectHighLevel(go);
	}

	TEST_FIXTURE (GameObjectFixture, HideFlagTest)
	{
		GameObject* go = NewGameObject();

		Unity::Component* component = NewComponent();
		go->AddComponentInternal(component);
		CHECK_EQUAL (go->GetComponentCount(), 1);

		int hideFlag = 2;
		go->SetHideFlags(hideFlag);

		CHECK_EQUAL (go->GetHideFlags(), hideFlag);

		for(int i = 0; i < go->GetComponentCount() ; i++)
		{
			CHECK_EQUAL (go->GetComponentAtIndex(i).GetHideFlags(), hideFlag);
		}

		// Add another component, it should have the same hide flag as the game object.
		Unity::Component* component1 = NewComponent();
		go->AddComponentInternal(component1);
		CHECK_EQUAL (go->GetComponentCount(), 2);

		for(int i = 0; i < go->GetComponentCount() ; i++)
		{
			CHECK_EQUAL (go->GetComponentAtIndex(i).GetHideFlags(), hideFlag);
		}

		DestroyObjectHighLevel(go);
	}

	TEST_FIXTURE (GameObjectFixture, NameTest)
	{
		GameObject* go = NewGameObject();

		AddComponents(*go, "Transform", "MeshRenderer", "MeshFilter", NULL);

		const char* name = "Test";
		go->SetName(name);

		CHECK_EQUAL (go->GetName(), name);

		for(int i = 0; i < go->GetComponentCount() ; i++)
		{
			CHECK_EQUAL (go->GetComponentAtIndex(i).GetName(), name);
		}

		// Set invalid value.
		// We will not test NULL as it's rejected by UI.
		go->SetName("");

		CHECK_EQUAL (go->GetName(), "");

		for(int i = 0; i < go->GetComponentCount() ; i++)
		{
			CHECK_EQUAL (go->GetComponentAtIndex(i).GetName(), "");
		}

		DestroyObjectHighLevel(go);
	}

	TEST_FIXTURE (GameObjectFixture, QueryComponentTest)
	{
		GameObject* go = NewGameObject();

		AddComponents(*go, "Transform", "MeshRenderer", "MeshFilter", NULL);

		// Go for QueryComponentExactTypeImplementation().
		Transform* transform = go->QueryComponentT<Transform>(ClassID(Transform));
		CHECK (transform != NULL);

		// Go for QueryComponentImplementation().
		Unity::Component* component = go->QueryComponentT<Unity::Component>(ClassID(Component));
		CHECK (component != NULL);

		DestroyObjectHighLevel(go);
	}

	TEST_FIXTURE (GameObjectFixture, SwapComponentTest)
	{
		GameObject* go = NewGameObject();

		AddComponents(*go, "Transform", "MeshRenderer", "MeshFilter", NULL);

		Unity::Component* component = go->GetComponentPtrAtIndex(0);
		go->SwapComponents(0, 1);

		CHECK_EQUAL (go->GetComponentIndex(component), 1);

		DestroyObjectHighLevel(go);
	}
}

SUITE (ComponentTests)
{
	TEST (GameObjectTest)
	{
		GameObject& go = CreateGameObject ("TestGameObject", "Transform", "MeshRenderer", NULL);

		Unity::Component& component = go.GetComponentAtIndex(0);

		CHECK(component.GetGameObjectPtr() == &go);
	}

	TEST_FIXTURE (GameObjectFixture, ActiveTest)
	{
		GameObject* go = NewGameObject();
		go->Activate();

		Unity::Component* component = NewComponent();
		CHECK (!component->IsActive());
		
		go->AddComponentInternal(component);
		CHECK (component->IsActive());
	}

	TEST_FIXTURE (GameObjectFixture, NameTest)
	{
		GameObject* go = NewGameObject();

		Unity::Component* component = NewComponent();
		CHECK_EQUAL (component->GetName(), component->GetClassName());

		go->AddComponentInternal(component);
		CHECK_EQUAL (component->GetName(), go->GetName());

		const char* name = "TestComponent";
		component->SetName(name);

		CHECK_EQUAL(go->GetName(), name);
	}

	TEST_FIXTURE (GameObjectFixture, CheckConsistencyTest)
	{
		GameObject* go = NewGameObject();

		Unity::Component* component = NewComponent();
		component->SetGameObjectInternal(go);

		CHECK_EQUAL(go->GetComponentCount(), 0);

		EXPECT (Error, "GameObject does not reference component");
		component->CheckConsistency();

		CHECK_EQUAL(go->GetComponentCount(), 1);
	}

	TEST (GameObjectMessagesCheckTest)
	{
#if !UNITY_RELEASE
		{
			GameObject& go = CreateGameObject ("test", "Transform", NULL);
			CHECK_EQUAL(go.GetSupportedMessages(), 0);

			//Add a Tree component
			Unity::Component *cmp1 = AddComponentUnchecked (go, 193, NULL, NULL);
			CHECK_EQUAL(go.GetSupportedMessages(), (int) kHasOnWillRenderObject);

			go.Deactivate();
			CHECK_EQUAL(go.GetSupportedMessages(), (int) kHasOnWillRenderObject);

			//Add a NavMeshObstablce component
			Unity::Component *cmp2 = AddComponentUnchecked (go, 208, NULL, NULL);
			CHECK_EQUAL(go.GetSupportedMessages(), (kHasOnWillRenderObject | kSupportsVelocityChanged | kSupportsTransformChanged));
			go.Activate();
			CHECK_EQUAL(go.GetSupportedMessages(), (kHasOnWillRenderObject | kSupportsVelocityChanged | kSupportsTransformChanged));
			DestroyObjectHighLevel(cmp1);
			CHECK_EQUAL(go.GetSupportedMessages(), (kSupportsVelocityChanged | kSupportsTransformChanged));
			go.Deactivate();
			DestroyObjectHighLevel(cmp2);

			CHECK_EQUAL(go.GetSupportedMessages(),  0);
			DestroyObjectHighLevel(&go);
		}
#endif
	}

	TEST (AwakeFromLoadCheckTest)
	{
		// tests to check if checks of AwakeFromLoad behavior are working
		// uncomment them when in doubt
	#if !UNITY_RELEASE
		// 1. simple object creation
		/*
		{
			GameObject* obj = NEW_OBJECT(GameObject);
			obj->CheckCorrectAwakeUsage();
			obj->CheckCorrectAwakeUsage();
		}
		*/

		// 2. enforce awake after reset
		/*
		{
			GameObject* obj = NEW_OBJECT(GameObject);
			obj->AwakeFromLoad(kDefaultAwakeFromLoad);
			obj->CheckCorrectAwakeUsage();
			obj->CheckCorrectAwakeUsage();
			obj->Reset();
			obj->CheckCorrectAwakeUsage();
			obj->CheckCorrectAwakeUsage();
		}
		*/
	
		// 3. check hacks are working
		{
			GameObject* obj = NEW_OBJECT(GameObject);
			obj->Reset();
			obj->HackSetAwakeWasCalled();
			obj->CheckCorrectAwakeUsage();
			obj->CheckCorrectAwakeUsage();
			DestroyObjectHighLevel(obj);
		}
	
		{
			GameObject* obj = NEW_OBJECT(GameObject);
			obj->Reset();
			obj->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
			obj->CheckCorrectAwakeUsage();
			DestroyObjectHighLevel(obj);
		}
#endif
	}
}

#endif
