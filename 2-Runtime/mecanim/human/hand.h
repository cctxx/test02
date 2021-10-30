#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/Math/Simd/xform.h"


#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Animation/MecanimArraySerialization.h"


namespace mecanim
{

namespace skeleton { struct Skeleton; struct SkeletonPose; struct SetupAxesInfo; }

namespace hand
{
	enum Fingers
	{
		kThumb = 0 ,
		kIndex,
		kMiddle,
		kRing,
		kLittle,
		kLastFinger
	};

	enum Phalanges
	{
		kProximal = 0,
		kIntermediate,
		kDistal,
		kLastPhalange
	};

	enum FingerDoF
	{
		kProximalDownUp = 0,
		kProximalInOut,
		kIntermediateCloseOpen,
		kDistalCloseOpen,
		kLastFingerDoF
	};

	const int32_t s_BoneCount = kLastFinger*kLastPhalange;
	const int32_t s_DoFCount = kLastFinger*kLastFingerDoF;

	inline int32_t GetBoneIndex(int32_t fingerIndex, int32_t phalangeIndex) { return fingerIndex * kLastPhalange + phalangeIndex; };
	inline int32_t GetFingerIndex(int32_t boneIndex) { return boneIndex / kLastPhalange; }; 
	inline int32_t GetPhalangeIndex(int32_t boneIndex) { return boneIndex % kLastPhalange; }; 
	inline int32_t GetDoFIndex(int32_t fingerIndex, int32_t phalangeDoFIndex) { return fingerIndex * kLastFingerDoF + phalangeDoFIndex; };

	const char* FingerName(uint32_t finger);
	const char* FingerDoFName(uint32_t finger);
	const char* PhalangeName(uint32_t finger);

	struct Hand
	{
		DEFINE_GET_TYPESTRING(Hand)

		Hand();
		int32_t		m_HandBoneIndex[s_BoneCount];
		
		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			STATIC_ARRAY_TRANSFER(int32_t, m_HandBoneIndex, s_BoneCount);
		}
	};
	
	struct HandPose
	{
		DEFINE_GET_TYPESTRING(HandPose)

		HandPose();

		math::xform m_GrabX;
		float m_DoFArray[s_DoFCount];
		float m_Override;
		float m_CloseOpen;
		float m_InOut;
		float m_Grab;	

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_GrabX);
			STATIC_ARRAY_TRANSFER(float, m_DoFArray, s_DoFCount);

			TRANSFER(m_Override);
			TRANSFER(m_CloseOpen);
			TRANSFER(m_InOut);
			TRANSFER(m_Grab);
		}
	};

	int32_t MuscleFromBone(int32_t aBoneIndex, int32_t aDoFIndex);
	int32_t BoneFromMuscle(int32_t aDoFIndex);

	Hand* CreateHand(memory::Allocator& alloc);
	void DestroyHand(Hand *hand, memory::Allocator& alloc);

	void HandSetupAxes(Hand const *hand, skeleton::SkeletonPose const *skeletonPose, skeleton::Skeleton *skeleton, bool aLeft);
	void HandCopyAxes(Hand const *srcHand, skeleton::Skeleton const *srcSkeleton, Hand const *hand, skeleton::Skeleton *skeleton);
	void HandPoseCopy(HandPose const *handPoseSrc, HandPose *handPoseDst);

	// Retargeting function set
	void HandPoseSolve(HandPose const* handPose,HandPose* handPoseOut);
	void Hand2SkeletonPose(Hand const *hand, skeleton::Skeleton const *skeleton, HandPose const *handPose, skeleton::SkeletonPose *skeletonPose);
	void Skeleton2HandPose(Hand const *hand, skeleton::Skeleton const *skeleton,skeleton::SkeletonPose const *skeletonPose, HandPose *handPose, float offset = 0.0f);
	// IK
	void FingerLengths(Hand const *hand, float *lengthArray);
	void FingerBaseFromPose(Hand const *hand,skeleton::SkeletonPose const *skeletonPose,math::float4 *positionArray);
	void FingerTipsFromPose(Hand const *hand,skeleton::Skeleton const *skeleton, skeleton::SkeletonPose const *skeletonPose,math::float4 *positionArray);
	
	void FingersIKSolve(Hand const *hand, skeleton::Skeleton const *skeleton,math::float4 const *positionArray, float *apWeightArray, skeleton::SkeletonPose *skeletonPose, skeleton::SkeletonPose *skeletonPoseWorkspace);

	mecanim::skeleton::SetupAxesInfo const& GetAxeInfo(uint32_t index);

}// namespace hand

}
