#include "UnityPrefix.h"

#include "Runtime/Animation/CalculateAnimatorSkinMatrices.h"
#include "Runtime/Animation/Avatar.h"
#include "Runtime/Animation/Animator.h"
#include "Runtime/Animation/MecanimUtility.h"
#include "Runtime/Filters/Mesh/MeshSkinning.h"
#include "Runtime/mecanim/animation/avatar.h"
#include "Runtime/mecanim/skeleton/skeleton.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Profiler/Profiler.h"

PROFILER_INFORMATION(gMeshSkinningPullMatrices, "MeshSkinning.CalculateSkinningMatrices", kProfilerRender)

void* DoCalculateAnimatorSkinMatrices (void* userData)
{
	PROFILER_AUTO(gMeshSkinningPullMatrices, NULL);

	CalculateSkinMatricesTask& task = *static_cast<CalculateSkinMatricesTask*>(userData);
	const mecanim::skeleton::SkeletonPose* animatedPose =
		reinterpret_cast<const mecanim::skeleton::SkeletonPose*>(task.skeletonPose);

	for (int i=0; i<task.bindPoseCount; i++)
	{
		UInt16 skeletonIndex = task.skeletonIndices[i];
		xform2unity (animatedPose->m_X[skeletonIndex], task.outPose[i]);
	}

	MultiplyMatrixArrayWithBase4x4 (&task.rootPose, task.outPose, task.bindPose, task.outPose, task.bindPoseCount);
	return NULL;
}

static void CalculateDefaultPoseWorldSpaceMatrices (
	const Transform& transform,
	const mecanim::animation::AvatarConstant* avatarConstant,
	const UInt16* skeletonIndices,
	Matrix4x4f* outWorldSpaceMatrices,
	size_t size)
{
	// Extract default skeleton pose from avatar and setup root position according to transform.
	Assert (avatarConstant);

	mecanim::memory::MecanimAllocator tempAlloc (kMemTempAlloc);
	mecanim::skeleton::SkeletonPose* tempDefaultGPose = mecanim::skeleton::CreateSkeletonPose(avatarConstant->m_AvatarSkeleton.Get(), tempAlloc);

	SkeletonPoseCopy (avatarConstant->m_DefaultPose.Get(), tempDefaultGPose);
	tempDefaultGPose->m_X[0] = xformFromUnity (transform.GetPosition(), transform.GetRotation(), transform.GetWorldScaleLossy());
	mecanim::skeleton::SkeletonPoseComputeGlobal (avatarConstant->m_AvatarSkeleton.Get(), tempDefaultGPose, tempDefaultGPose);
	for (int i=0;i<size;i++)
	{
		UInt16 skeletonIndex = skeletonIndices[i];
		xform2unity (tempDefaultGPose->m_X[skeletonIndex], outWorldSpaceMatrices[i]);
	}

	mecanim::skeleton::DestroySkeletonPose(tempDefaultGPose, tempAlloc);
}

bool CalculateWordSpaceMatrices (Animator* animator, const UInt16* skeletonIndices, Matrix4x4f* outWorldSpaceMatrices, size_t size)
{
	const mecanim::skeleton::SkeletonPose* animatedPose = animator->GetGlobalSpaceSkeletonPose ();

	if (animatedPose)
	{
		for (int i=0;i<size;i++)
		{
			UInt16 skeletonIndex = skeletonIndices[i];
			xform2unity (animatedPose->m_X[skeletonIndex], outWorldSpaceMatrices[i]);
		}
	}
	else
	{
		const mecanim::animation::AvatarConstant* avatarConstant = animator->GetAvatarConstant ();
		if (avatarConstant == NULL)
		{
			// Slow code path:
			Avatar* avatar = animator->GetAvatar ();
			if (avatar)
				avatarConstant = avatar->GetAsset ();
			if (avatarConstant == NULL)
				return false;
		}
		CalculateDefaultPoseWorldSpaceMatrices (animator->GetComponent(Transform), avatarConstant, skeletonIndices, outWorldSpaceMatrices, size);
	}
	return true;
}
