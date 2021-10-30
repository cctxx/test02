#include "UnityPrefix.h"

#include "OptimizeTransformHierarchy.h"

#include "Runtime/Animation/CharacterTestFixture.h"

#include "Runtime/Testing/Testing.h"
#include "Runtime/Testing/TestFixtures.h"

#include <string>
#include <vector>
#include <algorithm>

#if ENABLE_UNIT_TESTS

using namespace Unity;
using namespace std;

SUITE (OptimizeTransformHierarchy)
{
	TEST_FIXTURE (CharacterTestFixture, OptimizeTransformHierarchy_Remove_All_GameObjects_With_Transform_Only)
	{
		// Arrange
		MakeCharacter();

		// Act
		OptimizeTransformHierarchy(*root);

		// Assert
		Transform& rootTr = root->GetComponent(Transform);
		CHECK_EQUAL(MESH_RENDERER_COUNT+SKINNED_MESH_RENDERER_COUNT, GetAllChildrenCount(rootTr));

		CHECK(FindRelativeTransformWithPath(rootTr, "mr1") != NULL);
		CHECK(FindRelativeTransformWithPath(rootTr, "mr2") != NULL);
		CHECK(FindRelativeTransformWithPath(rootTr, "smr1") != NULL);
		CHECK(FindRelativeTransformWithPath(rootTr, "smr2") != NULL);
	}

	TEST_FIXTURE (CharacterTestFixture, OptimizeTransformHierarchy_Expose_Certain_Transforms)
	{
		// Arrange
		const UnityStr exposedPaths[] = {
			"b1/b1_1/b1_1_1",
			"b2/b2_1",
		};
		const int EXPOSED_COUNT = sizeof(exposedPaths)/sizeof(UnityStr);
		MakeCharacter(exposedPaths, EXPOSED_COUNT);

		// Act
		OptimizeTransformHierarchy(*root, exposedPaths, EXPOSED_COUNT);

		// Assert
		Transform& rootTr = root->GetComponent(Transform);
		CHECK_EQUAL(MESH_RENDERER_COUNT+SKINNED_MESH_RENDERER_COUNT+EXPOSED_COUNT, GetAllChildrenCount(rootTr));

		CHECK(FindRelativeTransformWithPath(rootTr, "mr1") != NULL);
		CHECK(FindRelativeTransformWithPath(rootTr, "mr2") != NULL);
		CHECK(FindRelativeTransformWithPath(rootTr, "smr1") != NULL);
		CHECK(FindRelativeTransformWithPath(rootTr, "smr2") != NULL);
		CHECK(FindRelativeTransformWithPath(rootTr, "b1_1_1") != NULL);
		CHECK(FindRelativeTransformWithPath(rootTr, "b2_1") != NULL);
	}

	TEST_FIXTURE (CharacterTestFixture, OptimizeTransformHierarchy_Flattened_Transforms_Have_Correct_TRS)
	{
		// Arrange
		const UnityStr exposedPaths[] = {
			"b1/b1_1/b1_1_1",
		};
		const int EXPOSED_COUNT = sizeof(exposedPaths)/sizeof(UnityStr);
		MakeCharacter(exposedPaths, EXPOSED_COUNT);

		// Act
		OptimizeTransformHierarchy(*root, exposedPaths, EXPOSED_COUNT);

		// Assert
		Transform& rootTr = root->GetComponent(Transform);
		Transform* mr1 = FindRelativeTransformWithPath(rootTr, "mr1");
		Transform* b1_1_1 = FindRelativeTransformWithPath(rootTr, "b1_1_1");
		CHECK(CompareApproximately(mr1->GetPosition(), Vector3f(3,1.5,0), Vector3f::epsilon));
		CHECK(CompareApproximately(b1_1_1->GetPosition(), Vector3f(3,1,0), Vector3f::epsilon));
	}

	TEST_FIXTURE (CharacterTestFixture, OptimizeTransformHierarchy_Set_Animator_HasTransformHierarchy_False)
	{
		// Arrange
		MakeCharacter();

		// Act
		OptimizeTransformHierarchy(*root);

		// Assert
		Animator& animator = root->GetComponent(Animator);
		CHECK(!animator.GetHasTransformHierarchy());
	}

	TEST_FIXTURE (CharacterTestFixture, OptimizeTransformHierarchy_Set_Bones_And_RootBone_of_SkinnedMeshRenderers)
	{
		// Arrange
		MakeCharacter();

		// Act
		OptimizeTransformHierarchy(*root);

		// Assert
		dynamic_array<SkinnedMeshRenderer*> skins;
		GetComponentsInChildren(*root, true, ClassID(SkinnedMeshRenderer), reinterpret_cast<dynamic_array<Unity::Component*>&>(skins));
		CHECK_EQUAL(2, skins.size());
		for (int i = 0; i < skins.size(); i++)
		{
			SkinnedMeshRenderer& skin = *skins[i];
			CHECK(NULL == skin.GetRootBone());
			CHECK_EQUAL(0, skin.GetBones().size());
		}
	}

	TEST_FIXTURE (CharacterTestFixture, DeoptimizeTransformHierarchy_Restore_Unstripped_Hierarchy)
	{
		// Arrange
		MakeCharacter();

		// Act
		OptimizeTransformHierarchy(*root);
		DeoptimizeTransformHierarchy(*root);

		// Assert
		Transform& rootTr = root->GetComponent(Transform);
		for (int i = 0; i < BONE_COUNT; i++)
			CHECK(FindRelativeTransformWithPath(rootTr, BONE_ARRAY[i].path) != NULL);
		for (int i = 0; i < MESH_RENDERER_COUNT; i++)
			CHECK(FindRelativeTransformWithPath(rootTr, MESH_RENDERER_ARRAY[i].path) != NULL);
		for (int i = 0; i < SKINNED_MESH_RENDERER_COUNT; i++)
			CHECK(FindRelativeTransformWithPath(rootTr, SKINNED_MESH_RENDERER_ARRAY[i].path) != NULL);
	}

	TEST_FIXTURE (CharacterTestFixture, DeoptimizeTransformHierarchy_Set_Bones_And_RootBone_of_SkinnedMeshRenderers)
	{
		// Arrange
		MakeCharacter();

		// Act
		OptimizeTransformHierarchy(*root);
		DeoptimizeTransformHierarchy(*root);

		// Assert
		dynamic_array<SkinnedMeshRenderer*> skins;
		GetComponentsInChildren(*root, true, ClassID(SkinnedMeshRenderer), reinterpret_cast<dynamic_array<Unity::Component*>&>(skins));
		CHECK_EQUAL(2, skins.size());
		for (int i = 0; i < skins.size(); i++)
		{
			SkinnedMeshRenderer* skin = skins[i];
			string boneNames("");
			for (int b = 0; b < skin->GetBones().size(); b++)
				boneNames += (skin->GetBones()[b]->GetName() + string(","));

			if (skin->GetName() == "smr1")
			{
				CHECK(skin->GetRootBone()->GetName() == string("b1"));
				CHECK(skin->GetBones().size() == 6);
				CHECK(boneNames.find("b1") != string::npos);
				CHECK(boneNames.find("b1_1") != string::npos);
				CHECK(boneNames.find("b1_1_1") != string::npos);
				CHECK(boneNames.find("b1_2") != string::npos);
				CHECK(boneNames.find("b1_2_1") != string::npos);
				CHECK(boneNames.find("b1_2_2") != string::npos);
			}
			else if (skin->GetName() == "smr2")
			{
				CHECK(skin->GetRootBone() == NULL);
				CHECK(skin->GetBones().size() == 3);
				CHECK(boneNames.find("b2_1_1") != string::npos);
				CHECK(boneNames.find("b2_1_2") != string::npos);
				CHECK(boneNames.find("b2_1_2_1") != string::npos);
			}
		}
	}

	TEST_FIXTURE (CharacterTestFixture, DeoptimizeTransformHierarchy_Restore_Transforms_With_Correct_TRS)
	{
		// Arrange
		MakeCharacter();

		// Act
		OptimizeTransformHierarchy(*root);
		DeoptimizeTransformHierarchy(*root);

		// Assert
		Transform& rootTr = root->GetComponent(Transform);
		Transform* tobeStrip1 =		FindRelativeTransformWithPath(rootTr, "b1/b1_2/b1_2_1/tobeStrip1");
		Transform* b1_1_1 =			FindRelativeTransformWithPath(rootTr, "b1/b1_1/b1_1_1");
		Transform* mr2 =			FindRelativeTransformWithPath(rootTr, "b1/b1_2/b1_2_1/mr2");
		Transform* smr2 =			FindRelativeTransformWithPath(rootTr, "b2/b2_1/smr2");
		CHECK(CompareApproximately(tobeStrip1->GetPosition(), Vector3f(5,5,5), Vector3f::epsilon));
		CHECK(CompareApproximately(b1_1_1->GetPosition(), Vector3f(3,1,0), Vector3f::epsilon));
		CHECK(CompareApproximately(mr2->GetPosition(), Vector3f(2,2.5,0), Vector3f::epsilon));
		CHECK(CompareApproximately(smr2->GetPosition(), Vector3f(9,9,9), Vector3f::epsilon));
	}

	TEST_FIXTURE (CharacterTestFixture, DeoptimizeTransformHierarchy_Set_Animator_HasTransformHierarchy_True)
	{
		// Arrange
		MakeCharacter();

		// Act
		OptimizeTransformHierarchy(*root);
		DeoptimizeTransformHierarchy(*root);

		// Assert
		Animator& animator = root->GetComponent(Animator);
		CHECK(animator.GetHasTransformHierarchy());
	}

	TEST_FIXTURE (CharacterTestFixture, RemoveUnnecessaryTransforms_Keep_Skeleton)
	{
		// Arrange
		MakeCharacter();

		// Act
		RemoveUnnecessaryTransforms(*root, NULL, NULL, 0, true);

		// Assert
		Transform& rootTr = root->GetComponent(Transform);
		CHECK_EQUAL(BONE_COUNT+MESH_RENDERER_COUNT+SKINNED_MESH_RENDERER_COUNT-3, GetAllChildrenCount(rootTr));
	}

	TEST_FIXTURE (CharacterTestFixture, RemoveUnnecessaryTransforms_Not_Keep_Skeleton)
	{
		// Arrange
		MakeCharacter();

		// Act
		RemoveUnnecessaryTransforms(*root, NULL, NULL, 0, false);

		// Assert
		Transform& rootTr = root->GetComponent(Transform);
		CHECK_EQUAL(BONE_COUNT+MESH_RENDERER_COUNT+SKINNED_MESH_RENDERER_COUNT-3-4, GetAllChildrenCount(rootTr));
	}

	TEST_FIXTURE (CharacterTestFixture, RemoveUnnecessaryTransforms_Consider_HumanDescription)
	{
		// Arrange
		MakeCharacter();
		HumanBone hb;
		hb.m_BoneName = "b1_2_2";
		HumanDescription hd;
		hd.m_Human.push_back(hb);

		// Act
		RemoveUnnecessaryTransforms(*root, &hd, NULL, 0, false);

		// Assert
		Transform& rootTr = root->GetComponent(Transform);
		CHECK_EQUAL(BONE_COUNT+MESH_RENDERER_COUNT+SKINNED_MESH_RENDERER_COUNT-3-3, GetAllChildrenCount(rootTr));
	}

	TEST_FIXTURE (CharacterTestFixture, RemoveUnnecessaryTransforms_Expose_Certain_Transforms)
	{
		// Arrange
		MakeCharacter();
		const UnityStr exposedPaths[] = {
			"b1/b1_2/b1_2_2",
			"b2/b2_1/b2_1_2",
		};
		const int EXPOSED_COUNT = sizeof(exposedPaths)/sizeof(UnityStr);

		// Act
		RemoveUnnecessaryTransforms(*root, NULL, exposedPaths, EXPOSED_COUNT, false);

		// Assert
		Transform& rootTr = root->GetComponent(Transform);
		CHECK_EQUAL(BONE_COUNT+MESH_RENDERER_COUNT+SKINNED_MESH_RENDERER_COUNT-3-2, GetAllChildrenCount(rootTr));
	}

	TEST_FIXTURE (CharacterTestFixture, GetUsefulTransformPaths)
	{
		// Arrange
		MakeCharacter();

		// Act
		Transform& rootTr = root->GetComponent(Transform);
		UNITY_VECTOR(kMemTempAlloc, UnityStr) outPaths;
		GetUsefulTransformPaths(rootTr, rootTr, outPaths);

		// Assert
		CHECK_EQUAL(MESH_RENDERER_COUNT+SKINNED_MESH_RENDERER_COUNT, outPaths.size());
		for (int i = 0; i < MESH_RENDERER_COUNT; ++i)
			CHECK(std::find(outPaths.begin(), outPaths.end(), MESH_RENDERER_ARRAY[i].path) != outPaths.end());
		for (int i = 0; i < SKINNED_MESH_RENDERER_COUNT; ++i)
			CHECK(std::find(outPaths.begin(), outPaths.end(), SKINNED_MESH_RENDERER_ARRAY[i].path) != outPaths.end());
	}
}

#endif