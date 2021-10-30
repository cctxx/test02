#include "UnityPrefix.h"
#include "ImportAnimationUtility.h"

#include "Runtime/Animation/AnimationUtility.h"
#include "Runtime/Animation/AnimationCurveUtility.h"
#include "Runtime/Animation/MecanimClipBuilder.h"

#include <string>


template <class T>
struct HasAnimatedMuscleTransform
{
	std::string mPath;
	HasAnimatedMuscleTransform(std::string const & path):mPath(path){}
	bool operator()(T& curve)
	{		
		return curve.path == mPath && !IsConstantCurve(curve.curve);
	}
};


template <class T>
struct HasPathPredicate
{
	std::string mPath;
	HasPathPredicate(std::string const & path):mPath(path){}
	bool operator()(T& curve){return curve.path == mPath;}
};

template <>
struct HasPathPredicate<TransformMaskElement>
{
	std::string mPath;
	HasPathPredicate(std::string const & path):mPath(path){}
	bool operator()(TransformMaskElement const& element){return element.m_Path == mPath && element.m_Weight > 0.f;}
};

template <typename CURVETYPE, typename CURVEVECTOR> void RemoveMaskedCurve(AvatarMask::ElementList const& mask, CURVEVECTOR& curves)
{
	CURVEVECTOR removeCurves;
	typename CURVEVECTOR::iterator it;
	for(it = curves.begin();it!=curves.end();++it)
	{
		AvatarMask::ElementList::const_iterator maskIt = std::find_if( mask.begin(), mask.end(), HasPathPredicate<AvatarMask::ElementList::value_type>(it->path));
		if(maskIt == mask.end())
			removeCurves.push_back(*it);
	}

	for(it = removeCurves.begin(); it != removeCurves.end(); ++it)
	{
		typename CURVEVECTOR::iterator end = std::remove_if(curves.begin(),curves.end(), HasPathPredicate<CURVETYPE>(it->path));
		curves.erase(end, curves.end());
	}
}


void RemovedMaskedCurve(AnimationClip& clip, const ClipAnimationInfo& clipInfo)
{
	// - Clear muscle curves not in mask
	mecanim::human::HumanPoseMask humanMask = HumanPoseMaskFromBodyMask(clipInfo.bodyMask);

	for(int muscleIter = 0; muscleIter < mecanim::animation::s_ClipMuscleCurveCount; muscleIter++)
	{
		if(!mecanim::animation::GetMuscleCurveInMask(humanMask,muscleIter))
		{
			AnimationClip::FloatCurves &floatCurves = clip.GetFloatCurves();

			bool curveFound = false;

			for (AnimationClip::FloatCurves::iterator curveIter = floatCurves.begin(); !curveFound && curveIter !=  floatCurves.end(); curveIter++)
			{
				AnimationClip::FloatCurve& floatCurve = *curveIter;

				if(floatCurve.classID == ClassID(Animator))
				{
					if(floatCurve.attribute == mecanim::animation::GetMuscleCurveName(muscleIter).c_str())
					{
						floatCurves.erase(curveIter);
						curveFound = true;
					}
				}
			}
		}
	}

	AnimationClip::Vector3Curves& positionCurves = clip.GetPositionCurves();
	AnimationClip::QuaternionCurves& quaternionCurves = clip.GetRotationCurves();
	AnimationClip::Vector3Curves& scaleCurves = clip.GetScaleCurves();

	// by default transform mask is created empty, 
	// for human clip the default behavior is to remove all curve except muscle curve
	if(clip.IsHumanMotion() && clipInfo.transformMask.size() == 0)
	{
		positionCurves.clear();
		quaternionCurves.clear();
		scaleCurves.clear();
	}
	else if(clipInfo.transformMask.size() > 0)
	{
		RemoveMaskedCurve<AnimationClip::Vector3Curve, AnimationClip::Vector3Curves>(clipInfo.transformMask, positionCurves);
		RemoveMaskedCurve<AnimationClip::QuaternionCurve, AnimationClip::QuaternionCurves>(clipInfo.transformMask, quaternionCurves);
		RemoveMaskedCurve<AnimationClip::Vector3Curve, AnimationClip::Vector3Curves>(clipInfo.transformMask, scaleCurves);
	}
}

void AddAdditionnalCurve(AnimationClip& clip, const ClipAnimationInfo& clipInfo)
{
	AnimationClip::FloatCurves &floatCurves = clip.GetFloatCurves();

	float stopTime = clip.GetAnimationClipSettings().m_StopTime;
	float startTime = clip.GetAnimationClipSettings().m_StartTime;
	for(int curveIter = 0; curveIter < clipInfo.curves.size(); curveIter++)
	{
		floatCurves.push_back(AnimationClip::FloatCurve());

		AnimationClip::FloatCurve& curve = floatCurves.back();

		curve.classID = ClassID(Animator);
		curve.attribute = clipInfo.curves[curveIter].name;
		curve.curve = clipInfo.curves[curveIter].curve;

		for(int keyIter = 0; keyIter < curve.curve.GetKeyCount(); keyIter++)
		{
			AnimationCurve::Keyframe& key = curve.curve.GetKey(keyIter);
			key.time *= (stopTime - startTime);
			key.time += startTime;
			key.inSlope /= (stopTime - startTime);
			key.outSlope /= (stopTime - startTime);
		}
	}
}

AvatarType AnimationTypeToAvatarType(ModelImporter::AnimationType type)
{
	switch(type)
	{
		case ModelImporter::kHumanoid:	return kHumanoid;
		default: return kGeneric;
	}
}


void UnrollMuscles(AnimationCurve& curve, int curveIndex, const mecanim::animation::AvatarConstant& avatarConstant, float startTime, float stopTime)
{
	///////////////////////////////////////
	// unroll muscles
	// case 504029: Spinning wrists again :(
		
	if(curveIndex >= mecanim::animation::s_ClipMuscleDoFBegin && curveIndex < mecanim::animation::s_ClipMuscleDoFBegin + mecanim::human::kLastDoF)
	{
		mecanim::int32_t muscleIndex = curveIndex - mecanim::animation::s_ClipMuscleDoFBegin;										
		
		float min = math::radians(-180);
		float max = math::radians(+180);

		int keyCount = curve.GetKeyCount();

		mecanim::human::GetMuscleRange(avatarConstant.m_Human.Get(),muscleIndex,min,max);

		int minKeyIndex = 0;
		float minKeyValue = 1e6;

		for(int keyIter = 0; keyIter < keyCount; keyIter++)
		{
			if(curve.GetKey(keyIter).time >= startTime && curve.GetKey(keyIter).time <= stopTime)
			{
				curve.GetKey(keyIter).value = math::LimitUnproject(min,max,curve.GetKey(keyIter).value);
							
				float absValue = math::abs(curve.GetKey(keyIter).value);

				if(absValue < minKeyValue)
				{
					minKeyIndex = keyIter;
					minKeyValue = absValue;
				}
			}		
		}
						
		float prevKeyValue = curve.GetKey(minKeyIndex).value;

		for(int keyIter = minKeyIndex + 1; keyIter < keyCount; keyIter++)
		{
			if(curve.GetKey(keyIter).time >= startTime && curve.GetKey(keyIter-1).time <= stopTime)
			{
				float keyValue = curve.GetKey(keyIter).value;
							
				float keyValueAlt = keyValue < 0 ? keyValue + math::radians(360) : keyValue - math::radians(360);
								
				keyValue = math::abs(keyValue - prevKeyValue) < math::abs(keyValueAlt - prevKeyValue) ? keyValue : keyValueAlt;

				curve.GetKey(keyIter).value = keyValue;

				prevKeyValue = keyValue;
			}
		}
		
		prevKeyValue = curve.GetKey(minKeyIndex).value;

		for(int keyIter = minKeyIndex - 1; keyIter >= 0; keyIter--)
		{
			if(curve.GetKey(keyIter).time >= startTime && curve.GetKey(keyIter).time <= stopTime)
			{
					
				float keyValue = curve.GetKey(keyIter).value;
							
				float keyValueAlt = keyValue < 0 ? keyValue + math::radians(360) : keyValue - math::radians(360);

				keyValue = math::abs(keyValue - prevKeyValue) < math::abs(keyValueAlt - prevKeyValue) ? keyValue : keyValueAlt;

				curve.GetKey(keyIter).value = keyValue;

				prevKeyValue = keyValue;
			}
		}
		

		for(int keyIter = 0; keyIter < keyCount; keyIter++)
		{
			if(curve.GetKey(keyIter).time >= startTime && curve.GetKey(keyIter).time <= stopTime)
			{				
				curve.GetKey(keyIter).value = math::LimitProject(min,max,curve.GetKey(keyIter).value);
			}
		}
		
		if (curve.GetKeyCount () >= 3)
		{
			Assert(IsFinite(curve.GetKey(0).value));
			Assert(IsFinite(curve.GetKey(1).value));
			Assert(IsFinite(curve.GetKey(2).value));
		}
	}						
}

std::string GenerateMecanimClipsCurves(AnimationClip** clips, size_t size, ModelImporter::ClipAnimations const& clipsInfo, mecanim::animation::AvatarConstant const& avatarConstant, bool isHuman, HumanDescription const& humanDescription, Unity::GameObject& rootGameObject, AvatarBuilder::NamedTransforms const& namedTransform)
{
	string invalidAnimationWarning;

	int rootMotionIndex = avatarConstant.m_RootMotionBoneIndex;

	mecanim::memory::MecanimAllocator alloc(kMemTempAlloc);

	mecanim::skeleton::SkeletonPose*	avatarLclPose = mecanim::skeleton::CreateSkeletonPose(avatarConstant.m_AvatarSkeleton.Get(), alloc);
	mecanim::skeleton::SkeletonPose*	avatarGblPose = mecanim::skeleton::CreateSkeletonPose(avatarConstant.m_AvatarSkeleton.Get(), alloc);

	mecanim::skeleton::SkeletonPose*	humanLclPose = 0;
	mecanim::skeleton::SkeletonPose*	humanPoseA = 0;
	mecanim::skeleton::SkeletonPose*	humanPoseB = 0;
	mecanim::skeleton::SkeletonPose*	humanPoseC = 0;
	mecanim::skeleton::SkeletonPose*	humanPoseD = 0;

	math::xform motionX;
	mecanim::human::HumanPose pose;

	if(isHuman)
	{
		humanLclPose = mecanim::skeleton::CreateSkeletonPose(avatarConstant.m_Human->m_Skeleton.Get(), alloc);
		humanPoseA = mecanim::skeleton::CreateSkeletonPose(avatarConstant.m_Human->m_Skeleton.Get(), alloc);
		humanPoseB = mecanim::skeleton::CreateSkeletonPose(avatarConstant.m_Human->m_Skeleton.Get(), alloc);
		humanPoseC = mecanim::skeleton::CreateSkeletonPose(avatarConstant.m_Human->m_Skeleton.Get(), alloc);
		humanPoseD = mecanim::skeleton::CreateSkeletonPose(avatarConstant.m_Human->m_Skeleton.Get(), alloc);
	}

	for(int i = 0; i < size; i++)
	{
		AnimationClip* animationClip = clips[i];

		// compute frame count
		float period = 1.0f/animationClip->GetSampleRate();

		float startTime = animationClip->GetRange().first;
		float stopTime = animationClip->GetRange().second;

		if(stopTime > startTime)
		{
			AnimationCurve curveArray[mecanim::animation::s_ClipMuscleCurveCount];

			int curveCount = isHuman ? mecanim::animation::s_ClipMuscleCurveCount : 7;
			int curveStart = rootMotionIndex != -1 ? 0 : 7;

			int keyCount = ceil((stopTime-startTime)/period)+1;

			for(int curveIter = curveStart; curveIter < curveCount; curveIter++)
			{
				curveArray[curveIter].ResizeUninitialized(keyCount);
			}

			for(int keyIter = 0; keyIter < keyCount; keyIter++)
			{
				float time = startTime + float(keyIter)*period;

				SampleAnimation(rootGameObject, *animationClip, time, 1 /* Wrap Mode = Once */);

				AvatarBuilder::ReadFromLocalTransformToSkeletonPose(avatarLclPose, namedTransform);

				if(isHuman)
				{
					// [case 493451] Attached scene does not play animation correctly, must bake all animation for node between root and hips transform
					mecanim::int32_t rootIndex = avatarConstant.m_HumanSkeletonIndexArray[0];

					mecanim::skeleton::SkeletonPoseComputeGlobal(avatarConstant.m_AvatarSkeleton.Get(),avatarLclPose,avatarGblPose, rootIndex, 0);
					mecanim::skeleton::SkeletonPoseCopy(avatarConstant.m_AvatarSkeleton.Get(),avatarLclPose,avatarConstant.m_Human->m_Skeleton.Get(),humanLclPose);
					humanLclPose->m_X[0] = avatarGblPose->m_X[rootIndex];

					mecanim::human::RetargetFrom(avatarConstant.m_Human.Get(),humanLclPose,&pose,humanPoseA,humanPoseB,humanPoseC,humanPoseD);
				}
				else if(rootMotionIndex != -1)
				{
					mecanim::skeleton::SkeletonPoseComputeGlobal(avatarConstant.m_AvatarSkeleton.Get(),avatarLclPose,avatarGblPose);
					motionX = avatarGblPose->m_X[rootMotionIndex];
					motionX.q = math::quatMul(motionX.q,math::quatConj(avatarConstant.m_RootMotionBoneX.q));
				}

				for(int curveIter = curveStart; curveIter < curveCount; curveIter++)
				{
					AnimationCurve::Keyframe* keyFrame = &curveArray[curveIter].GetKey(keyIter);

					keyFrame->value = mecanim::animation::GetMuscleCurveValue(pose,motionX,curveIter);
					keyFrame->time = time;
					keyFrame->inSlope = 0;
					keyFrame->outSlope = 0;
					keyFrame->tangentMode = 0;

					// unroll quaternions
					if(keyIter > 0)
					{
						int tqIndex = mecanim::animation::GetMuscleCurveTQIndex(curveIter);
						if(tqIndex == 6)
						{
							math::float4 qprev,q;

							qprev.x() = curveArray[curveIter-3].GetKey(keyIter-1).value;
							qprev.y() = curveArray[curveIter-2].GetKey(keyIter-1).value;
							qprev.z() = curveArray[curveIter-1].GetKey(keyIter-1).value;
							qprev.w() = curveArray[curveIter-0].GetKey(keyIter-1).value;

							q.x() = curveArray[curveIter-3].GetKey(keyIter).value;
							q.y() = curveArray[curveIter-2].GetKey(keyIter).value;
							q.z() = curveArray[curveIter-1].GetKey(keyIter).value;
							q.w() = curveArray[curveIter-0].GetKey(keyIter).value;

							if(math::dot(qprev,q) < math::float1::zero()) q = -q;

							curveArray[curveIter-3].GetKey(keyIter).value = q.x().tofloat();
							curveArray[curveIter-2].GetKey(keyIter).value = q.y().tofloat();
							curveArray[curveIter-1].GetKey(keyIter).value = q.z().tofloat();
							curveArray[curveIter-0].GetKey(keyIter).value = q.w().tofloat();  
						}
					}
				}
			}

			if(isHuman)
			{
				// Remove animations that are handled by muscle clip
				for(int j = 0 ; j < namedTransform.size(); j++)
				{
					string transformPath = namedTransform[j].path;

					mecanim::uint32_t pathHash = mecanim::processCRC32(transformPath.c_str());

					if(mecanim::skeleton::SkeletonFindNode(avatarConstant.m_Human->m_Skeleton.Get(),pathHash) != -1)
					{
						AnimationClip::Vector3Curves& positionCurves = animationClip->GetPositionCurves();
						AnimationClip::QuaternionCurves& quaternionCurves = animationClip->GetRotationCurves();
						AnimationClip::Vector3Curves& scaleCurves = animationClip->GetScaleCurves();

						HumanBoneList::const_iterator boneIt = std::find_if(humanDescription.m_Human.begin(), humanDescription.m_Human.end(), FindBoneName( namedTransform[j].name));
						if(boneIt != humanDescription.m_Human.begin() && boneIt != humanDescription.m_Human.end())
						{
							if(std::find_if(positionCurves.begin(), positionCurves.end(), HasAnimatedMuscleTransform<AnimationClip::Vector3Curve>(transformPath)) != positionCurves.end())
							{								
								invalidAnimationWarning += Format("MuscleClip '%s'conversion warning: '%s' has translation animation. It is not supported.\n", rootGameObject.GetName(), transformPath.c_str());								
							}

							if(std::find_if(scaleCurves.begin(), scaleCurves.end(), HasAnimatedMuscleTransform<AnimationClip::Vector3Curve>(transformPath)) != scaleCurves.end())
							{								
								invalidAnimationWarning += Format("MuscleClip '%s' conversion warning: '%s' has scale animation'. It is not supported.\n", rootGameObject.GetName(), transformPath.c_str());								
							}
						}

						AnimationClip::Vector3Curves::iterator posEnd = std::remove_if(positionCurves.begin(),positionCurves.end(), HasPathPredicate<AnimationClip::Vector3Curve>(transformPath));
						positionCurves.erase(posEnd, positionCurves.end());

						AnimationClip::QuaternionCurves::iterator rotEnd = std::remove_if(quaternionCurves.begin(),quaternionCurves.end(), HasPathPredicate<AnimationClip::QuaternionCurve>(transformPath));
						quaternionCurves.erase(rotEnd, quaternionCurves.end());

						AnimationClip::Vector3Curves::iterator scaleEnd = std::remove_if(scaleCurves.begin(),scaleCurves.end(), HasPathPredicate<AnimationClip::Vector3Curve>(transformPath));
						scaleCurves.erase(scaleEnd, scaleCurves.end());
					}
				}
			}

			AnimationClip::FloatCurves& curves = animationClip->GetFloatCurves();

			float startTime = i < clipsInfo.size() ?  clipsInfo[i].firstFrame / animationClip->GetSampleRate()	: -1e6;
			float stopTime	= i < clipsInfo.size() ?  clipsInfo[i].lastFrame / animationClip->GetSampleRate()	: 1e6;
			for(int curveIter = curveStart; curveIter < curveCount; curveIter++)
			{								
				UnrollMuscles(curveArray[curveIter], curveIter, avatarConstant, startTime, stopTime);

				RecalculateSplineSlope(curveArray[curveIter]);

				curves.push_back(AnimationClip::FloatCurve());
				curves.back().classID = ClassID(Animator);
				curves.back().attribute = mecanim::animation::GetMuscleCurveName(curveIter).c_str();
				curves.back().curve = curveArray[curveIter];
			}
		}
	}

	mecanim::skeleton::DestroySkeletonPose(avatarLclPose, alloc);
	mecanim::skeleton::DestroySkeletonPose(avatarGblPose, alloc);

	if(isHuman)
	{
		mecanim::skeleton::DestroySkeletonPose(humanLclPose, alloc);
		mecanim::skeleton::DestroySkeletonPose(humanPoseA, alloc);
		mecanim::skeleton::DestroySkeletonPose(humanPoseB, alloc);
		mecanim::skeleton::DestroySkeletonPose(humanPoseC, alloc);
		mecanim::skeleton::DestroySkeletonPose(humanPoseD, alloc);
	}

	return invalidAnimationWarning;
}
