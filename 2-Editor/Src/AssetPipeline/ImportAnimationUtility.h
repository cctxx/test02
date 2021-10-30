#pragma once

#include "Runtime/Animation/AvatarBuilder.h"
#include "ModelImporter.h"

class AnimationClip;
struct ClipAnimationInfo;

void RemovedMaskedCurve(AnimationClip& clip, const ClipAnimationInfo& clipInfo);
void AddAdditionnalCurve(AnimationClip& clip, const ClipAnimationInfo& clipInfo);

AvatarType AnimationTypeToAvatarType(ModelImporter::AnimationType type);

std::string GenerateMecanimClipsCurves(AnimationClip** clips, size_t size, ModelImporter::ClipAnimations const& clipsInfo, mecanim::animation::AvatarConstant const& avatarConstant, bool isHuman, HumanDescription const& humanDescription, GameObject& rootGameObject, AvatarBuilder::NamedTransforms const& namedTransform);

