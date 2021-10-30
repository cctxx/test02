#include "UnityPrefix.h"

#include "MecanimAnimation.h"

#include "Animator.h"
#include "Avatar.h"
#include "CalculateAnimatorSkinMatrices.h"
#include "Runtime/mecanim/animation/avatar.h"
#include "Runtime/mecanim/skeleton/skeleton.h"

#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/Word.h"

void MecanimAnimation::InitializeClass ()
{
	SetAnimationInterface(new MecanimAnimation());
}

void MecanimAnimation::CleanupClass ()
{
	MecanimAnimation* animation = reinterpret_cast<MecanimAnimation*>(GetAnimationInterface ());
	delete animation;
	SetAnimationInterface(NULL);
}

const void* MecanimAnimation::GetGlobalSpaceSkeletonPose(const Unity::Component& animatorComponent)
{
	const Animator& animator = static_cast<const Animator&>(animatorComponent);
	return animator.GetGlobalSpaceSkeletonPose();
}

CalculateAnimatorSkinMatricesFunc MecanimAnimation::GetCalculateAnimatorSkinMatricesFunc()
{
	return DoCalculateAnimatorSkinMatrices;
}

bool MecanimAnimation::CalculateWorldSpaceMatricesMainThread(Unity::Component& animatorComponent, const UInt16* indices, size_t count, Matrix4x4f* outMatrices)
{
	Animator& animator = static_cast<Animator&>(animatorComponent);
	AssertIf(animator.GetHasTransformHierarchy());

	return CalculateWordSpaceMatrices(&animator, indices, outMatrices, count);
}

bool MecanimAnimation::PathHashesToIndices(Unity::Component& animatorComponent, const BindingHash* bonePathHashes, size_t count, UInt16* outIndices)
{
	Animator& animator = static_cast<Animator&>(animatorComponent);
	if (animator.GetHasTransformHierarchy())
		return false;

	const mecanim::animation::AvatarConstant* avatarConstant = animator.GetAvatarConstant();
	if (!avatarConstant)
		return false;

	const mecanim::skeleton::Skeleton* skel = avatarConstant->m_AvatarSkeleton.Get();
	if (!skel)
		return false;

	bool doMatchSkeleton = true;
	for (int i = 0; i < count && doMatchSkeleton; i++)
	{
		int skeletonIndex = mecanim::skeleton::SkeletonFindNode(skel, bonePathHashes[i]);
		doMatchSkeleton = (skeletonIndex != -1);
		outIndices[i] = (UInt16)skeletonIndex;
	}

	if (!doMatchSkeleton)
	{
		const Avatar* avatar = animator.GetAvatar();
		Assert(avatar);
		ErrorStringObject(Format("The input bones do not match the skeleton of the Avatar(%s).\n"
			"Please check if the Avatar is generated in optimized mode, or if the Avatar is valid for the attached SkinnedMeshRenderer.", 
			avatar->GetName()).c_str(),
			avatar);
	}

	return doMatchSkeleton;
}
