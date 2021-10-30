#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"
#include "Editor/Src/ComponentUtility.h"

#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/Skybox.h"
#include "Runtime/Dynamics/BoxCollider.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Editor/Src/CommandImplementation.h"

static std::vector<Object*> CreateVectorWithOne (Object* o)
{
	std::vector<Object*> v;
	v.push_back (o);
	return v;
}

static std::vector<Object*> CreateVectorWithTwo (Object* o1, Object* o2)
{
	std::vector<Object*> v;
	v.push_back (o1);
	v.push_back (o2);
	return v;
}

SUITE (ComponentUtilityTests)
{
	TEST (ComponentMoveTest)
	{
		GameObject& go = CreateGameObject ("go", "Transform", "Camera", "MeshRenderer", NULL);
		Unity::Component* transform = go.QueryComponent(Transform);
		Unity::Component* camera = go.QueryComponent(Camera);
		Unity::Component* renderer = go.QueryComponent(Renderer);

		std::vector<Object*> vtransform = CreateVectorWithOne(transform);
		std::vector<Object*> vcamera = CreateVectorWithOne(camera);
		std::vector<Object*> vrenderer = CreateVectorWithOne(renderer);

		// assert expected order
		CHECK_EQUAL (go.GetComponentPtrAtIndex(0), transform);
		CHECK_EQUAL (go.GetComponentPtrAtIndex(1), camera);
		CHECK_EQUAL (go.GetComponentPtrAtIndex(2), renderer);

		CHECK (!MoveComponentUp(vtransform,true)); // can't move Transform
		CHECK (!MoveComponentUp(vcamera,true)); // can't move since first after transform
		CHECK (MoveComponentUp(vrenderer,true));

		CHECK (!MoveComponentDown(vtransform,true)); // can't move Transform
		CHECK (MoveComponentDown(vcamera,true));
		CHECK (!MoveComponentDown(vrenderer,true)); // can't move since at bottom

		// assert validation calls did not change order
		CHECK_EQUAL (go.GetComponentPtrAtIndex(0), transform);
		CHECK_EQUAL (go.GetComponentPtrAtIndex(1), camera);
		CHECK_EQUAL (go.GetComponentPtrAtIndex(2), renderer);

		// Transform should not move
		MoveComponentUp (vtransform, false);
		CHECK_EQUAL (go.GetComponentPtrAtIndex(0), transform);
		MoveComponentUp (vtransform, false);
		CHECK_EQUAL (go.GetComponentPtrAtIndex(0), transform);

		// Move camera down, then up again
		MoveComponentDown (vcamera, false);
		CHECK_EQUAL (go.GetComponentPtrAtIndex(1), renderer); // renderer got into it's place
		CHECK_EQUAL (go.GetComponentPtrAtIndex(2), camera); // camera moved down
		MoveComponentUp (vcamera, false);
		CHECK_EQUAL (go.GetComponentPtrAtIndex(1), camera);
		CHECK_EQUAL (go.GetComponentPtrAtIndex(2), renderer);

		DestroyObjectHighLevel (&go);
	}

	TEST (ComponentMoveMultiSelectTest)
	{
		GameObject& go1 = CreateGameObject ("go1", "Transform", "Camera", "MeshRenderer", "Skybox", NULL);
		GameObject& go2 = CreateGameObject ("go2", "Transform", "MeshRenderer", "Camera", "Skybox", NULL);
		Unity::Component* transform1 = go1.QueryComponent(Transform);
		Unity::Component* camera1 = go1.QueryComponent(Camera);
		Unity::Component* renderer1 = go1.QueryComponent(Renderer);
		Unity::Component* skybox1 = go1.QueryComponent(Skybox);
		Unity::Component* transform2 = go2.QueryComponent(Transform);
		Unity::Component* camera2 = go2.QueryComponent(Camera);
		Unity::Component* renderer2 = go2.QueryComponent(Renderer);
		Unity::Component* skybox2 = go2.QueryComponent(Skybox);

		std::vector<Object*> vtransform = CreateVectorWithTwo(transform1,transform2);
		std::vector<Object*> vcamera = CreateVectorWithTwo(camera1,camera2);
		std::vector<Object*> vrenderer = CreateVectorWithTwo(renderer1,renderer2);
		std::vector<Object*> vskybox = CreateVectorWithTwo(skybox1,skybox2);

		// assert expected order
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(0), transform1);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(1), camera1);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(2), renderer1);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(3), skybox1);
		
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(0), transform2);
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(1), renderer2);
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(2), camera2);
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(3), skybox2);

		CHECK (!MoveComponentUp(vtransform,true)); // can't move Transforms
		CHECK (MoveComponentUp(vcamera,true)); // can't move first camera (right below transform), but can move 2nd one
		CHECK (MoveComponentUp(vrenderer,true)); // can move 1st; can't move 2nd
		CHECK (MoveComponentUp(vskybox,true)); // can move both

		CHECK (!MoveComponentDown(vtransform,true)); // can't move Transform
		CHECK (MoveComponentDown(vcamera,true)); // can move both
		CHECK (!MoveComponentDown(vskybox,true)); // can't move either since at bottom

		// assert validation calls did not change order
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(0), transform1);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(1), camera1);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(2), renderer1);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(3), skybox1);

		CHECK_EQUAL (go2.GetComponentPtrAtIndex(0), transform2);
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(1), renderer2);
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(2), camera2);
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(3), skybox2);

		// Transform should not move
		MoveComponentUp (vtransform, false);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(0), transform1);
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(0), transform2);
		MoveComponentUp (vtransform, false);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(0), transform1);
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(0), transform2);

		// Move camera down, then up again
		MoveComponentDown (vcamera, false);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(1), renderer1); // go1: renderer got into it's place
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(2), camera1); // go1: camera moved down
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(2), skybox2); // go2: skybox got into it's place
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(3), camera2); // go2: camera moved down
		MoveComponentUp (vcamera, false);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(1), camera1);
		CHECK_EQUAL (go1.GetComponentPtrAtIndex(2), renderer1);
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(2), camera2);
		CHECK_EQUAL (go2.GetComponentPtrAtIndex(3), skybox2);

		DestroyObjectHighLevel (&go1);
		DestroyObjectHighLevel (&go2);
	}
}




// The below tests depends on the scene. We won't port to unit test for now.
static void TestComponentCopyPaste()
{
	GameObject& go1 = CreateGameObject ("go1", "Transform", "BoxCollider", "MeshFilter", NULL);
	GameObject& go2 = CreateGameObject ("go2", "Transform", "SphereCollider", NULL);
	Transform* transform1 = go1.QueryComponent(Transform);
	transform1->SetLocalPosition (Vector3f(1,2,3));
	Transform* transform2 = go2.QueryComponent(Transform);
	transform2->SetParent(transform1);
	transform2->SetLocalPosition (Vector3f(4,5,6));

	BoxCollider* collider1 = go1.QueryComponent(BoxCollider);
	MeshFilter* filter1 = go1.QueryComponent(MeshFilter);

	std::vector<Object*> vtransform1 = CreateVectorWithOne(transform1);
	std::vector<Object*> vtransform2 = CreateVectorWithOne(transform2);
	std::vector<Object*> vcollider1 = CreateVectorWithOne(collider1);
	std::vector<Object*> vfilter1 = CreateVectorWithOne(filter1);

	Assert (!CopyComponent(CreateVectorWithOne(&go1),true)); // can't copy; not a component
	Assert (CopyComponent(vtransform1,true));
	Assert (CopyComponent(vtransform2,true));

	Assert (!PasteComponentAsNew(vtransform1,true)); // nothing to paste
	Assert (!PasteComponentValues(vtransform1,true)); // nothing to paste

	// copy transform1
	CopyComponent(vtransform1, false);
	Assert (!PasteComponentAsNew(vtransform1,true)); // can't paste new transform
	Assert (!PasteComponentAsNew(vtransform2,true)); // can't paste new transform
	Assert (PasteComponentValues(vtransform1,true));
	Assert (PasteComponentValues(vtransform2,true));

	// paste into transform2
	PasteComponentValues (vtransform2,false);
	Assert (Vector3f(1,2,3) == transform2->GetLocalPosition()); // get position from copied one
	Assert (transform1 == transform2->GetParent()); // does not change the parent of transform2
	Assert (0 == transform2->GetChildrenCount()); // does not change anything with children of transform2

	// copy collider1
	CopyComponent(vcollider1, false);
	Assert (PasteComponentAsNew(vtransform2,true)); // can paste new collider (with dialog)

	// copy & paste mesh filter
	CopyComponent(vfilter1, false);
	Assert (go2.QueryComponent(MeshFilter) == NULL);
	Assert (PasteComponentAsNew(vtransform2,false));
	Assert (go2.QueryComponent(MeshFilter) != NULL);

	DestroyObjectHighLevel (&go1); // go2 will be destroyed since it's a child

	ClearComponentInPasteboard (); // clean up anything component related in clipboard
}

static void TestComponentCopyPasteMultiSelect()
{
	GameObject& go1 = CreateGameObject ("go1", "Transform", NULL);
	GameObject& go1c = CreateGameObject ("go1c", "Transform", NULL);
	GameObject& go2 = CreateGameObject ("go2", "Transform", NULL);
	Transform* transform1 = go1.QueryComponent(Transform);
	transform1->SetLocalPosition (Vector3f(1,2,3));
	Transform* transform1c = go1c.QueryComponent(Transform);
	transform1c->SetParent(transform1);
	transform1c->SetLocalPosition (Vector3f(4,5,6));
	Transform* transform2 = go2.QueryComponent(Transform);
	transform2->SetLocalPosition (Vector3f(7,8,9));

	std::vector<Object*> vtransform1 = CreateVectorWithOne(transform1);
	std::vector<Object*> vtransform1c = CreateVectorWithOne(transform1c);
	std::vector<Object*> vtransform1c2 = CreateVectorWithTwo(transform1c,transform2);

	Assert (!CopyComponent(CreateVectorWithOne(&go1),true)); // can't copy; not a component
	Assert (!CopyComponent(CreateVectorWithTwo(transform1,transform1c),true)); // can't copy; multiple selected

	// copy transform1
	CopyComponent(vtransform1, false);
	Assert (!PasteComponentAsNew(vtransform1c2,true)); // can't paste new transform
	Assert (PasteComponentValues(vtransform1c2,true));

	// paste into transform1c and transform2
	PasteComponentValues (vtransform1c2,false);
	Assert (Vector3f(1,2,3) == transform1c->GetLocalPosition()); // get position from copied one
	Assert (Vector3f(1,2,3) == transform2->GetLocalPosition()); // get position from copied one

	DestroyObjectHighLevel (&go1); // go1c will be destroyed since it's a child
	DestroyObjectHighLevel (&go2);

	ClearComponentInPasteboard (); // clean up anything component related in clipboard
}


void TestEditorComponentUtility() // called from HighLevelTest.cpp
{
	TestComponentCopyPaste ();
	TestComponentCopyPasteMultiSelect ();
}

#endif