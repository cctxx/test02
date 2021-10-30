#pragma once

#if ENABLE_UNIT_TESTS

#include "Runtime/Testing/Testing.h"
#include "Runtime/Testing/TestFixtures.h"

#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Animation/Avatar.h"
#include "Runtime/Animation/AvatarBuilder.h"
#include "Runtime/Animation/Animator.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"

#include "Runtime/Animation/OptimizeTransformHierarchy.h"


// y
// |
// |
// 3   b1_2_2
// |    |       mr2
// |    |        |
// 2   b1_2----b1_2_1                 b2_1_2-----b2_1_2_1
// |  	|               mr1              |
// |    |               |                |
// 1    b1-----b1_1---b1_1_1     b2-----b2_1------b2_1_1
// | 
// 0____1_______2_______3_________4_______5_________6_________ x
//
// smr1 {b1(root), b1_1, b1_1_1, b1_2, b1_2_1, b1_2_2}
// smr2 {b2_1_1, b2_1_2, b2_1_2_1}

struct TransformDescriptor {
	const char* path;
	float t[3];
	float r[3];
	float s[3];
};
const TransformDescriptor BONE_ARRAY[] = {
	{"b1",										{1,1,0}, {0,0,0}, {1,1,1} },
	{"b1/b1_1",									{2,1,0}, {0,0,0}, {1,1,1} },
	{"b1/b1_1/b1_1_1",							{3,1,0}, {0,0,0}, {1,1,1} },
	{"b1/b1_2",									{1,2,0}, {0,0,0}, {1,1,1} },
	{"b1/b1_2/b1_2_1",							{2,2,0}, {0,0,0}, {1,1,1} },
	{"b1/b1_2/b1_2_1/tobeStrip1",				{5,5,5}, {0,0,0}, {1,1,1} },
	{"b1/b1_2/b1_2_2",							{1,3,0}, {0,0,0}, {1,1,1} },
	{"b2",										{4,1,0}, {0,0,0}, {1,1,1} },
	{"b2/b2_1",									{5,1,0}, {0,0,0}, {1,1,1} },
	{"b2/b2_1/b2_1_1",							{6,1,0}, {0,0,0}, {1,1,1} },
	{"b2/b2_1/b2_1_2",							{5,2,0}, {0,0,0}, {1,1,1} },
	{"b2/b2_1/b2_1_2/b2_1_2_1",					{6,2,0}, {0,0,0}, {1,1,1} },
	{"b2/b2_1/b2_1_2/b2_1_2_1/tobeStrip3",		{6,6,6}, {0,0,0}, {1,1,1} },
	{"tobeStrip2",								{7,7,7}, {0,0,0}, {1,1,1} },
};
const int BONE_COUNT = sizeof(BONE_ARRAY)/sizeof(BONE_ARRAY[0]);
const TransformDescriptor MESH_RENDERER_ARRAY[] = {
	{"b1/b1_1/b1_1_1/mr1",						{3,1.5,0}, {0,0,0}, {1,1,1} },
	{"b1/b1_2/b1_2_1/mr2",						{2,2.5,0}, {0,0,0}, {1,1,1} },
};
const int MESH_RENDERER_COUNT = sizeof(MESH_RENDERER_ARRAY)/sizeof(MESH_RENDERER_ARRAY[0]);
const TransformDescriptor SKINNED_MESH_RENDERER_ARRAY[] = {
	{"smr1",									{8,8,8}, {0,0,0}, {1,1,1} },
	{"b2/b2_1/smr2",							{9,9,9}, {0,0,0}, {1,1,1} },
};
const int SKINNED_MESH_RENDERER_COUNT = sizeof(SKINNED_MESH_RENDERER_ARRAY)/sizeof(SKINNED_MESH_RENDERER_ARRAY[0]);

class CharacterTestFixture : public TestFixtureBase
{
protected:
	Unity::GameObject*	root;
	Avatar*				avatar;
	Avatar*				unstrippedAvatar;

	CharacterTestFixture():root(NULL), avatar(NULL), unstrippedAvatar(NULL) {}

	~CharacterTestFixture()
	{
		DestroyObjects();
	}

	void MakeCharacter(const UnityStr* extraExposedPaths = NULL, int extraExposedPathCount = 0)
	{
		root = &CreateGameObjectWithHideFlags("root", true, 0, "Transform", "Animator", NULL);
		Transform& rootTr = root->GetComponent(Transform);
		AttachGameObjects(rootTr);

		CreateAvatars(extraExposedPaths, extraExposedPathCount);

		Animator& animator = root->GetComponent(Animator);
		animator.SetAvatar(avatar);
		animator.AwakeFromLoad(kDefaultAwakeFromLoad);
	}

	void DestroyObjects()
	{
		if (root)
		{
			DestroyObjectHighLevel(root);
			root = NULL;
		}
		if (avatar)
		{
			DestroyObjectHighLevel(avatar);
			avatar = NULL;
		}
		if (unstrippedAvatar)
		{
			DestroyObjectHighLevel(unstrippedAvatar);
			unstrippedAvatar = NULL;
		}
	}

	void CreateAvatars(const UnityStr* extraExposedPaths = NULL, int extraExposedPathCount = 0)
	{
		HumanDescription dummyHd;
		std::string errStr;

		AvatarBuilder::Options options;
		options.avatarType = kGeneric;
		options.readTransform = true;

		//// 1. create unstripped avatar
		//unstrippedAvatar = NEW_OBJECT (Avatar);
		//unstrippedAvatar->Reset();
		//errStr = AvatarBuilder::BuildAvatar(*unstrippedAvatar, *root, true, dummyHd, options);
		//CHECK_EQUAL (string(""), errStr);
		//unstrippedAvatar->AwakeFromLoad(kDefaultAwakeFromLoad);

		//// 2. strip
		//RemoveUnnecessaryTransforms(*root, 
		//	&dummyHd,
		//	extraExposedPaths,
		//	extraExposedPathCount,
		//	true);

		// 3. create stripped avatar
		avatar = NEW_OBJECT (Avatar);
		avatar->Reset();

		errStr = AvatarBuilder::BuildAvatar(*avatar,
			*root, true, dummyHd, options);
		CHECK_EQUAL (string(""), errStr);
		avatar->AwakeFromLoad(kDefaultAwakeFromLoad);
	}

	void AttachGameObjects(Transform& rootTr)
	{
		dynamic_array<PPtr<Transform> > smrBones[SKINNED_MESH_RENDERER_COUNT];

		dynamic_array<TransformDescriptor> transforms;
		for (int i = 0; i < BONE_COUNT; i++)	transforms.push_back(BONE_ARRAY[i]);
		for (int i = 0; i < MESH_RENDERER_COUNT; i++)		transforms.push_back(MESH_RENDERER_ARRAY[i]);
		for (int i = 0; i < SKINNED_MESH_RENDERER_COUNT; i++)		transforms.push_back(SKINNED_MESH_RENDERER_ARRAY[i]);
		for (int i = 0; i < transforms.size(); i++)
		{
			string path(transforms[i].path);

			string parentPath = DeleteLastPathNameComponent(path);
			Transform* parent = FindRelativeTransformWithPath(rootTr, parentPath.c_str());
			CHECK(parent != NULL);

			string goName = GetLastPathNameComponent(path);
			GameObject& go = CreateGameObjectWithHideFlags(goName, true, 0, "Transform", NULL);
			Transform& tr = go.GetComponent(Transform);
			tr.SetPosition(Vector3f(transforms[i].t[0], transforms[i].t[1], transforms[i].t[2]));
			tr.SetParent(parent);

			if (goName.compare(0, strlen("b1"), "b1") == 0)
				smrBones[0].push_back(&tr);
			else if (goName.compare(0, strlen("b2_1_"), "b2_1_") == 0)
				smrBones[1].push_back(&tr);
		}

		for (int i = 0; i < MESH_RENDERER_COUNT; i++)
		{
			Transform* tr = FindRelativeTransformWithPath(rootTr, MESH_RENDERER_ARRAY[i].path);
			AddComponent(tr->GetGameObject(), "MeshRenderer");
		}

		for (int i = 0; i < SKINNED_MESH_RENDERER_COUNT; i++)
		{
			Transform* tr = FindRelativeTransformWithPath(rootTr, SKINNED_MESH_RENDERER_ARRAY[i].path);
			AddComponent(tr->GetGameObject(), "SkinnedMeshRenderer");
			SkinnedMeshRenderer& smr = tr->GetComponent(SkinnedMeshRenderer);
			if (i == 0)
				smr.SetRootBone(smrBones[i][0]);
			smr.SetBones(smrBones[i]);
		}
	}

	int GetAllChildrenCount(Transform& tr)
	{
		int count = tr.GetChildrenCount();
		for (int i = 0; i < tr.GetChildrenCount(); i++)
			count += GetAllChildrenCount(tr.GetChild(i));
		return count;
	}

};

#endif
