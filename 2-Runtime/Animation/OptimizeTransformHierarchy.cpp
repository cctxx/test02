#include "UnityPrefix.h"

#include "OptimizeTransformHierarchy.h"

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Animation/Animator.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Animation/Avatar.h"
#include "Runtime/Animation/AvatarBuilder.h"
#include "Runtime/mecanim/animation/avatar.h"
#include "Runtime/mecanim/skeleton/skeleton.h"

#include <map>

using namespace std;
using namespace mecanim::animation;
using namespace mecanim::skeleton;
using namespace mecanim;

// static function forward declarations
static void FlattenHierarchy (GameObject& gameObject);
static void FlattenHierarchyRecurse (Transform& transform, Transform& root);

void OptimizeTransformHierarchy(GameObject& root, const UnityStr* exposedTransforms, size_t exposedTransformCount)
{
	Animator* animator = root.QueryComponent(Animator);
	if (!animator)
		return;

	if(!animator->IsOptimizable())
		return;

	Transform* rootTransform = animator->GetAvatarRoot();
	if(!rootTransform)
		return;

	GameObject& effectiveRoot = rootTransform->GetGameObject();

	// 1. Take care of the SkinnedMeshRenderers, so that the will work without the transform hierarchy.
	dynamic_array<SkinnedMeshRenderer*> skins (kMemTempAlloc);
	GetComponentsInChildren(effectiveRoot, true, ClassID(SkinnedMeshRenderer), reinterpret_cast<dynamic_array<Unity::Component*>&>(skins));
	for (int i=0;i<skins.size();i++)
	{
		SkinnedMeshRenderer& skin = *skins[i];

		// The root bone of the SkinnedMeshRenderer will always be itself in optimized mode.
		// We'll directly set the correct value to it through the transform binding.
		if (skin.GetRootBone())
		{
			const Transform& skinRoot = *skin.GetRootBone();
			Transform& skinTransform = skin.GetComponent(Transform);
			skinTransform.SetPositionAndRotation(skinRoot.GetPosition(), skinRoot.GetRotation());
			// TODO: handle scale if needed later
		}
		skin.SetRootBone(NULL);

		// Clear the Transform array, we'll use skeleton indices array instead.
		skin.SetBones(dynamic_array<PPtr<Transform> > ());
	}

	// 2. Flatten the transform hierarchy
	FlattenHierarchy (effectiveRoot);

	// 3. Remove the unnecessary transforms
	// Here, we can safely remove transforms that are:
	//		a) human bones, because the Avatar will take care of them
	//		b) referred by SkinnedMeshRenderer, because the SkinnedMeshRenderer can work with skeleton indices.
	UNITY_VECTOR(kMemTempAlloc, UnityStr) exposedTransformNames(exposedTransformCount);
	for (int i = 0; i < exposedTransformCount; i++)
		exposedTransformNames[i] = GetLastPathNameComponent(exposedTransforms[i].c_str(), exposedTransforms[i].size());
	RemoveUnnecessaryTransforms (root, NULL,
		exposedTransformCount ? &(exposedTransformNames[0]) : NULL,
		exposedTransformCount, false);

	// Finally, set the Animator to be optimized mode.
	animator->SetHasTransformHierarchy(false);
}

void DeoptimizeTransformHierarchy(Unity::GameObject& root)
{
	Animator* animator = root.QueryComponent(Animator);
	if (!animator)
		return;

	if(!animator->IsOptimizable())
		return;

	Transform* avatarRoot = animator->GetAvatarRoot();
	if(!avatarRoot)
		return;
	GameObject& effectiveRoot = avatarRoot->GetGameObject();

	// 1. Figure out the skeletonPaths from the unstripped avatar
	const Avatar& unstrippedAvatar = *animator->GetAvatar();
	const TOSVector& tos = unstrippedAvatar.GetTOS();
	const AvatarConstant* avatarConstant = unstrippedAvatar.GetAsset();
	const Skeleton& skeleton = *avatarConstant->m_AvatarSkeleton;
	const SkeletonPose& skeletonPose = *avatarConstant->m_DefaultPose;
	
	UNITY_VECTOR(kMemTempAlloc, UnityStr) skeletonPaths;
	for (int i = 0; i < skeleton.m_Count; i++)
	{
		UnityStr path = "";
		TOSVector::const_iterator it = tos.find(skeleton.m_ID[i]);
		if (it != tos.end())
			path = it->second;
		skeletonPaths.push_back(path.c_str());
	}
	
	// 2. Restore the original transform hierarchy
	// Prerequisite: skeletonPaths follow pre-order traversal
	Transform& rootTransform = effectiveRoot.GetComponent(Transform);
	for (int i = 1; i < skeletonPaths.size(); i++) // start from 1, skip the root transform because it will always be there.
	{
		const UnityStr& unflattenPath = skeletonPaths[i];
		UnityStr transformName = GetLastPathNameComponent(unflattenPath);
		Transform* curTransform = FindTransformWithName(&rootTransform, transformName.c_str());
		if (curTransform == NULL)
		{
			// Create a new GameObject with just transform component
			GameObject& go = CreateGameObjectWithHideFlags (transformName, true, 0, "Transform", NULL);
			curTransform = go.QueryComponent(Transform);
		}

		// insert it at the right position of the hierarchy
		Transform* parentTransform = &rootTransform;
		UnityStr parentPath = DeleteLastPathNameComponent(unflattenPath);
		if (parentPath.length() > 0)
			parentTransform = FindRelativeTransformWithPath(rootTransform, parentPath.c_str());
		curTransform->SetParent(parentTransform);

		Vector3f t, s; Quaternionf q;
		xform2unityNoNormalize(skeletonPose.m_X[i], t, q, s);
		curTransform->SetLocalPositionWithoutNotification(t);
		curTransform->SetLocalRotationWithoutNotification(q);
		curTransform->SetLocalScaleWithoutNotification(s);
	}
	
	// 3. Restore the values in SkinnedMeshRenderer
	dynamic_array<SkinnedMeshRenderer*> skins (kMemTempAlloc);
	GetComponentsInChildren(effectiveRoot, true, ClassID(SkinnedMeshRenderer), reinterpret_cast<dynamic_array<Unity::Component*>&>(skins));
	for (int i = 0; i < skins.size(); i++)
	{
		SkinnedMeshRenderer& skin = *skins[i];
		
		// a. root bone
		Transform* rootBoneTransform = NULL;
		BindingHash rootPathHash = 0;
		const Mesh* mesh = skin.GetMesh();
		if (mesh)
			rootPathHash = mesh->GetRootBonePathHash();
		if (rootPathHash)
		{
			int skeletonIndex = mecanim::skeleton::SkeletonFindNode(&skeleton, rootPathHash);
			const UnityStr& rootPath = skeletonPaths[skeletonIndex];
			rootBoneTransform = FindRelativeTransformWithPath(rootTransform, rootPath.c_str());
		}
		if (rootBoneTransform != skin.QueryComponent(Transform))
			skin.SetRootBone(rootBoneTransform);
		
		// b. skeleton
		if (mesh)
		{
			const dynamic_array<BindingHash>& bonePathHashes = mesh->GetBonePathHashes();
			dynamic_array<PPtr<Transform> > boneTransforms;
			boneTransforms.resize_initialized (bonePathHashes.size());
			for (int j = 0; j < boneTransforms.size(); j++)
			{
				BindingHash bonePathHash = bonePathHashes[j];
				int boneIndexInUnstrippedSkeleton = SkeletonFindNode(&skeleton, bonePathHash);
				AssertIf(boneIndexInUnstrippedSkeleton == -1);
				const UnityStr& bonePath = skeletonPaths[boneIndexInUnstrippedSkeleton];
				Transform* boneTransform = FindRelativeTransformWithPath(rootTransform, bonePath.c_str());
				boneTransforms[j] = boneTransform;
			}
			skin.SetBones(boneTransforms);
		}
	}

	// 4. Animator
	animator->SetHasTransformHierarchy(true);
}

static void FlattenHierarchyRecurse (Transform& transform, Transform& root)
{
	while (transform.GetChildrenCount () > 0)
	{
		Transform& child = transform.GetChild (0);
		child.SetParent (&root);
		FlattenHierarchyRecurse (child, root);
	}
}

static void FlattenHierarchy (GameObject& gameObject)
{
	Transform& root = gameObject.GetComponent(Transform);
	int size = root.GetChildrenCount ();
	dynamic_array<Transform*> children(size, kMemTempAlloc);
	for (int i=0;i < size;++i)
		children[i] = &root.GetChild(i);

	for (int i=0;i < size;++i)
		FlattenHierarchyRecurse (*children[i], root);
}

void RemoveUnnecessaryTransforms (Unity::GameObject& gameObject, const HumanDescription* human, const UnityStr* exposedTransforms, size_t exposedTransformCount, bool doKeepSkeleton)
{
	// false : unnecessary
	map<Transform*, bool> transformToMark;
	Transform& rootTransform = gameObject.GetComponent(Transform);

	// 1. Initialize the map
	dynamic_array<Transform*> allTransforms (kMemTempAlloc);
	GetComponentsInChildren(gameObject, true, ClassID(Transform), reinterpret_cast<dynamic_array<Unity::Component*>&>(allTransforms));
	for (int i=0; i<allTransforms.size(); i++)
	{
		Transform* transform = allTransforms[i];
		bool isNecessary = (transform->GetGameObject().GetComponentCount() > 1);
		transformToMark.insert(pair<Transform*, bool>(transform, isNecessary));
	}

	// 2. Handle human bones
	if (human)
	{
		for (int i=0; i<allTransforms.size(); i++)
		{
			Transform* transform = allTransforms[i];
			map<Transform*, bool>::iterator it = transformToMark.find(transform);
			AssertIf(it == transformToMark.end());
			
			// human bones should not be removed
			if (std::find_if(human->m_Human.begin(), human->m_Human.end(), FindBoneName( transform->GetName() )) != human->m_Human.end() )
				it->second = true;
			else if (human->m_RootMotionBoneName.length() > 0)
			{
				// root bone should not be removed (for Humanoid & Generic root motion)
				if (human->m_RootMotionBoneName.compare(transform->GetName()) == 0)
					it->second = true;
			}
		}
	}

	// 3. Handle exposed transforms
	for (int i=0; i<exposedTransformCount; i++)
	{
		const UnityStr& path = exposedTransforms[i];
		Transform* transform = FindRelativeTransformWithPath(rootTransform, path.c_str());
		map<Transform*, bool>::iterator it = transformToMark.find(transform);
		AssertIf(it == transformToMark.end());
		it->second = true;
	}

	// 4. Handle SkinnedMeshRenderers
	if (doKeepSkeleton)
	{
		dynamic_array<SkinnedMeshRenderer*> skins (kMemTempAlloc);
		GetComponentsInChildren(gameObject, true, ClassID(SkinnedMeshRenderer), reinterpret_cast<dynamic_array<Unity::Component*>&>(skins));

		for (int i=0; i<skins.size(); i++)
		{
			const SkinnedMeshRenderer& skin = *skins[i];
			const dynamic_array<PPtr<Transform> >& bones = skin.GetBones();
			for (int j=0; j<bones.size(); j++)
			{
				Transform* transform = bones[j];
				map<Transform*, bool>::iterator it = transformToMark.find(transform);
				AssertIf(it == transformToMark.end());
				it->second = true;
			}
		}
	}

	// 5. Handle parent transforms
	for (int i=allTransforms.size()-1; i>=0; i--)
	{
		Transform* transform = allTransforms[i];
		map<Transform*, bool>::const_iterator it = transformToMark.find(transform);
		AssertIf(it == transformToMark.end());
		if (!it->second)
			continue;

		Transform* parent = transform->GetParent();
		if (!parent)
			continue;
		map<Transform*, bool>::iterator itParent = transformToMark.find(parent);
		AssertIf(itParent == transformToMark.end());
		itParent->second = true;
	}

	// Remove unnecessary transforms
	for (int i=allTransforms.size()-1; i>=0; i--)
	{
		Transform* transform = allTransforms[i];
		map<Transform*, bool>::const_iterator it = transformToMark.find(transform);
		AssertIf(it == transformToMark.end());
		if (it->second)
			continue;
		DestroyObjectHighLevel(&(transform->GetGameObject()));
	}
}

