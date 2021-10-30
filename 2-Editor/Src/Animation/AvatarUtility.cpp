#include "AvatarUtility.h"

#include "Runtime/Animation/Avatar.h"
#include "Runtime/Animation/Animator.h"

#include "Runtime/Filters/Mesh/MeshRenderer.h"

void AvatarUtility::SetHumanPose(Animator& animator, float* dof, int dofCount)
{
	AssertIf(dofCount != HumanTrait::MuscleCount);

	mecanim::human::HumanPose pose;

	for(int i=0;i<HumanTrait::MuscleCount;i++)
	{
		if(i<HumanTrait::LastDoF)
			pose.m_DoFArray[i] = dof[i];
		else if(i<HumanTrait::LastLeftFingerDoF)
			pose.m_LeftHandPose.m_DoFArray[i-HumanTrait::LastDoF] = dof[i];
		else if(i<HumanTrait::LastRightFingerDoF)
			pose.m_RightHandPose.m_DoFArray[i-HumanTrait::LastLeftFingerDoF] = dof[i];
	}

	animator.WriteHumanPose(pose);
}

void AvatarUtility::HumanGetColliderTransform(Avatar& avatar, int boneIndex, TransformX const& bone, TransformX& collider)
{
	mecanim::animation::AvatarConstant* cst = avatar.GetAsset();
	if(cst)
	{
		math::xform boneX = xformFromUnity(bone.position, bone.rotation, bone.scale);
		math::xform colliderX = HumanGetColliderXform(cst->m_Human.Get(), boneX, boneIndex);

		xform2unity(colliderX, collider.position, collider.rotation, collider.scale);
	}
}

void AvatarUtility::HumanSubColliderTransform(Avatar& avatar, int boneIndex, TransformX const& collider, TransformX& bone)
{
	mecanim::animation::AvatarConstant* cst = avatar.GetAsset();
	if(cst)
	{
		math::xform colliderX = xformFromUnity(collider.position, collider.rotation, collider.scale);
		math::xform boneX = HumanSubColliderXform(cst->m_Human.Get(), colliderX, boneIndex);

		xform2unity(boneX, bone.position, bone.rotation, bone.scale);
	}
}
