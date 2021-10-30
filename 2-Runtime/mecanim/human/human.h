#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/bitset.h"
#include "Runtime/Math/Simd/xform.h"
#include "Runtime/mecanim/math/axes.h"
#include "Runtime/mecanim/math/collider.h"

#include "Runtime/mecanim/human/handle.h"
#include "Runtime/mecanim/human/hand.h"

#include "Runtime/Serialize/Blobification/offsetptr.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Animation/MecanimArraySerialization.h"

namespace mecanim
{

namespace skeleton { struct Skeleton; struct SkeletonPose; }

namespace human
{
	enum Bones
	{
		kHips = 0,
		kLeftUpperLeg,
		kRightUpperLeg,
		kLeftLowerLeg,
		kRightLowerLeg,
		kLeftFoot,
		kRightFoot,
		kSpine,
		kChest,
		kNeck,
		kHead,
		kLeftShoulder,
		kRightShoulder,
		kLeftUpperArm,
		kRightUpperArm,
		kLeftLowerArm,
		kRightLowerArm,
		kLeftHand,
		kRightHand,
		kLeftToes,
		kRightToes,
		kLeftEye,
		kRightEye,
		kJaw,
		kLastBone
	};

	enum BodyDoF
	{
		kSpineFrontBack = 0,
		kSpineLeftRight,
		kSpineRollLeftRight,
		kChestFrontBack,
		kChestLeftRight,
		kChestRollLeftRight,
		kLastBodyDoF
	};

	enum HeadDoF
	{
		kNeckFrontBack = 0,
		kNeckLeftRight,
		kNeckRollLeftRight,
		kHeadFrontBack,
		kHeadLeftRight,
		kHeadRollLeftRight,
		kLeftEyeDownUp,
		kLeftEyeLeftRight,
		kRightEyeDownUp,
		kRightEyeLeftRight,
		kJawDownUp,
		kJawLeftRight,
		kLastHeadDoF
	};

	enum LegDoF
	{
		kUpperLegFrontBack = 0,
		kUpperLegInOut,
		kUpperLegRollInOut,
		kLegCloseOpen,
		kLegRollInOut,
		kFootCloseOpen,
		kFootInOut,
		kToesUpDown,
		kLastLegDoF
	};

	enum ArmDoF
	{
		kShoulderDownUp = 0,
		kShoulderFrontBack,
		kArmDownUp,
		kArmFrontBack,
		kArmRollInOut,
		kForeArmCloseOpen,
		kForeArmRollInOut,
		kHandDownUp,
		kHandInOut,
		kLastArmDoF
	};

	enum DoF
	{
		kBodyDoFStart = 0,
		kHeadDoFStart = kBodyDoFStart + kLastBodyDoF,
		kLeftLegDoFStart = kHeadDoFStart + kLastHeadDoF,
		kRightLegDoFStart = kLeftLegDoFStart + kLastLegDoF,
		kLeftArmDoFStart = kRightLegDoFStart + kLastLegDoF,
		kRightArmDoFStart = kLeftArmDoFStart + kLastArmDoF,
		kLastDoF = kRightArmDoFStart + kLastArmDoF
	};

	enum Goal
	{
		kLeftFootGoal,
		kRightFootGoal,
		kLeftHandGoal,
		kRightHandGoal,
		kLastGoal
	};

	struct GoalInfo
	{
		int32_t m_Index;
		int32_t m_TopIndex;
		int32_t m_MidIndex;
		int32_t m_EndIndex;
	};

	const static GoalInfo s_HumanGoalInfo[kLastGoal] = 
	{
		{ kLeftFoot, kLeftUpperLeg, kLeftLowerLeg, kLeftFoot },
		{ kRightFoot, kRightUpperLeg, kRightLowerLeg, kRightFoot },
		{ kLeftHand, kLeftUpperArm, kLeftLowerArm, kLeftHand },
		{ kRightHand, kRightUpperArm, kRightLowerArm, kRightHand }
	};

	enum HumanPoseMaskInfo
	{
		kMaskRootIndex = 0,
		kMaskDoFStartIndex = kMaskRootIndex + 1,
		kMaskGoalStartIndex = kMaskDoFStartIndex + kLastDoF,
		kMaskLeftHand = kMaskGoalStartIndex + kLastGoal,
		kMaskRightHand = kMaskLeftHand + 1,
		kLastMaskIndex = kMaskRightHand +1
	};	

	typedef mecanim::bitset<kLastMaskIndex> HumanPoseMask;

	bool MaskHasLegs(const HumanPoseMask& mask);

	HumanPoseMask FullBodyMask();

	struct Human
	{
		DEFINE_GET_TYPESTRING(Human)

		Human();

		math::xform				m_RootX;

		OffsetPtr<skeleton::Skeleton>		m_Skeleton;
		OffsetPtr<skeleton::SkeletonPose>	m_SkeletonPose;
		OffsetPtr<hand::Hand>				m_LeftHand;
		OffsetPtr<hand::Hand>				m_RightHand;

		uint32_t							m_HandlesCount;		
		OffsetPtr<human::Handle>			m_Handles;

		uint32_t							m_ColliderCount;
		OffsetPtr<math::Collider>			m_ColliderArray;

		int32_t					m_HumanBoneIndex[kLastBone];
		float					m_HumanBoneMass[kLastBone];
		int32_t					m_ColliderIndex[kLastBone];
		
		
		float					m_Scale;

		float					m_ArmTwist;
		float					m_ForeArmTwist;
		float					m_UpperLegTwist;
		float					m_LegTwist;

		float					m_ArmStretch;
		float					m_LegStretch;

		float					m_FeetSpacing;

		bool					m_HasLeftHand;
		bool					m_HasRightHand;


		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_RootX);

			TRANSFER(m_Skeleton);
			TRANSFER(m_SkeletonPose);
			TRANSFER(m_LeftHand);
			TRANSFER(m_RightHand);

			TRANSFER_BLOB_ONLY(m_HandlesCount);
			MANUAL_ARRAY_TRANSFER2(Handle, m_Handles, m_HandlesCount);

			TRANSFER_BLOB_ONLY(m_ColliderCount);
			MANUAL_ARRAY_TRANSFER2(math::Collider, m_ColliderArray, m_ColliderCount);

			STATIC_ARRAY_TRANSFER(mecanim::int32_t, m_HumanBoneIndex, kLastBone);
			STATIC_ARRAY_TRANSFER(float, m_HumanBoneMass, kLastBone);
			STATIC_ARRAY_TRANSFER(mecanim::int32_t, m_ColliderIndex, kLastBone);
			
			TRANSFER(m_Scale);

			TRANSFER(m_ArmTwist);
			TRANSFER(m_ForeArmTwist);
			TRANSFER(m_UpperLegTwist);
			TRANSFER(m_LegTwist);

			TRANSFER(m_ArmStretch);
			TRANSFER(m_LegStretch);

			TRANSFER(m_FeetSpacing);

			TRANSFER(m_HasLeftHand);
			TRANSFER(m_HasRightHand);
			transfer.Align();
		}
	};
	
	struct HumanGoal
	{
		DEFINE_GET_TYPESTRING(HumanGoal)

		HumanGoal() : m_WeightT(0.0f), m_WeightR(0.0f) {};
		
		math::xform m_X;
		float m_WeightT;
		float m_WeightR;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_X);
			TRANSFER(m_WeightT);
			TRANSFER(m_WeightR);
		}
	};

	struct HumanPose
	{
		DEFINE_GET_TYPESTRING(HumanPose)

		HumanPose();

		math::xform		m_RootX;
		math::float4	m_LookAtPosition;
		math::float4	m_LookAtWeight;

		HumanGoal		m_GoalArray[kLastGoal];
		hand::HandPose	m_LeftHandPose;
		hand::HandPose	m_RightHandPose;	

		float			m_DoFArray[kLastDoF];

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_RootX);
			TRANSFER(m_LookAtPosition);
			TRANSFER(m_LookAtWeight);

			STATIC_ARRAY_TRANSFER(HumanGoal, m_GoalArray, kLastGoal);
			TRANSFER(m_LeftHandPose);
			TRANSFER(m_RightHandPose);

			STATIC_ARRAY_TRANSFER(float, m_DoFArray, kLastDoF);			
		}
	};

	int32_t MuscleFromBone(int32_t boneIndex, int32_t doFIndex); 
	int32_t BoneFromMuscle(int32_t doFIndex);

	bool RequiredBone(uint32_t boneIndex);
	const char* BoneName(uint32_t boneIndex);
	const char* MuscleName(uint32_t boneIndex);

	Human* CreateHuman(skeleton::Skeleton *skeleton,skeleton::SkeletonPose *skeletonPose, uint32_t handlesCount, uint32_t colliderCount, memory::Allocator& alloc);
	void DestroyHuman(Human *human, memory::Allocator& alloc);

	void HumanAdjustMass(Human *human);
	void HumanSetupAxes(Human *human, skeleton::SkeletonPose const *skeletonPoseGlobal);
	void HumanSetupCollider(Human *human, skeleton::SkeletonPose const *skeletonPoseGlobal);
	void HumanCopyAxes(Human const *srcHuman, Human *human);
	math::Axes GetAxes(Human const *human, int32_t boneIndex);
	void GetMuscleRange(Human const *apHuman, int32_t aDoFIndex, float &aMin, float &aMax);
	math::float4 AddAxis(Human const *human, int32_t index, math::float4 const &q);
	math::float4 RemoveAxis(Human const *human, int32_t index, const math::float4 &q);
	math::xform NormalizedHandleX(Human const *human, int32_t handleIndex); 
	
	math::float4	HumanComputeBoneMassCenter(Human const *human, skeleton::SkeletonPose const *skeletonPoseGlobal, int32_t boneIndex);
	math::float4	HumanComputeMassCenter(Human const *human, skeleton::SkeletonPose const *skeletonPoseGlobal);
	float			HumanComputeMomentumOfInertia(Human const *human, skeleton::SkeletonPose const *skeletonPoseGlobal);
	math::float4	HumanComputeOrientation(Human const* human,skeleton::SkeletonPose const* apPoseGlobal);
	math::xform		HumanComputeRootXform(Human const* human,skeleton::SkeletonPose const* apPoseGlobal);
	float			HumanGetFootHeight(Human const* human, bool left);
	math::float4	HumanGetFootBottom(Human const* human, bool left);
	math::xform		HumanGetColliderXform(Human const* human, math::xform const& x, int32_t boneIndex);
	math::xform		HumanSubColliderXform(Human const* human, math::xform const& x, int32_t boneIndex);
    math::float4    HumanGetGoalOrientationOffset(Goal goalIndex);

	void HumanPoseClear(HumanPose& pose);
	void HumanPoseCopy(HumanPose &pose,HumanPose const &poseA, bool doFOnly = false); 
	void HumanPoseCopy(HumanPose &pose,HumanPose const &poseA, HumanPoseMask const &humanPoseMask);
	void HumanPoseAdd(HumanPose &pose,HumanPose const &poseA,HumanPose const &poseB); 
	void HumanPoseSub(HumanPose &pose,HumanPose const &poseA,HumanPose const &poseB); 
	void HumanPoseWeight(HumanPose &pose,HumanPose const &poseA, float weight); 
	void HumanPoseMirror(HumanPose &pose,HumanPose const &poseA);
	void HumanPoseBlend(HumanPose &pose, HumanPose **poseArray, float *weightArray, uint32_t count); 	
	void HumanPoseAddOverrideLayer(HumanPose &poseBase,HumanPose const &pose, float weight, HumanPoseMask const &humanPoseMask);
	void HumanPoseAddAdditiveLayer(HumanPose &poseBase,HumanPose const &pose, float weight, HumanPoseMask const &humanPoseMask);
	
	void			RetargetFrom(	Human const *human, 
									skeleton::SkeletonPose const *skeletonPose,
									HumanPose *humanPose, 
									skeleton::SkeletonPose *skeletonPoseWsRef,
									skeleton::SkeletonPose *skeletonPoseWsGbl,
									skeleton::SkeletonPose *skeletonPoseWsLcl,
									skeleton::SkeletonPose *skeletonPoseWsWs,
									int32_t maxFixIter = 5);

	void			RetargetTo(	Human const *human, 
								HumanPose const *humanPoseBase, 
								HumanPose const *humanPose, 
								const math::xform &x,
								HumanPose *humanPoseOut, 
								skeleton::SkeletonPose *skeletonPose, 
								skeleton::SkeletonPose *skeletonPoseWs);

	//	apSkeletonPoseWorkspace must be set to global pose before calling
	void			FullBodySolve(Human const *human, HumanPose const *humanPose, skeleton::SkeletonPose *skeletonPose, skeleton::SkeletonPose *skeletonPoseWorkspaceA, skeleton::SkeletonPose *skeletonPoseWorkspaceB);
	void			TwistSolve(Human const *human, skeleton::SkeletonPose *skeletonPose, skeleton::SkeletonPose *skeletonPoseWorkspace);

	float			DeltaPoseQuality(HumanPose &deltaPose, float tolerance = 0.15f);

	skeleton::SetupAxesInfo const& GetAxeInfo(uint32_t index);

}// namespace human

}

template<>
class SerializeTraits< mecanim::human::HumanPoseMask > : public SerializeTraitsBase< mecanim::human::HumanPoseMask >
{
public:

	inline static const char* GetTypeString (value_type*)	{ return "HumanPoseMask"; }
	inline static bool IsAnimationChannel ()	{ return false; }
	inline static bool MightContainPPtr ()	{ return false; }
	inline static bool AllowTransferOptimization ()	{ return true; }
	inline static bool IsContinousMemoryArray ()	{ return true; }
	
	typedef mecanim::human::HumanPoseMask value_type;
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.Transfer(data.word(0), "word0");
		if(1<value_type::Words+1)
			transfer.Transfer(data.word(1), "word1");
		if(2<value_type::Words+1)
			transfer.Transfer(data.word(2), "word2");
		if(3<value_type::Words+1)
			transfer.Transfer(data.word(3), "word3");
	}	
	
	static void resource_image_assign_external (value_type& data, void* begin, void* end)
	{
	}
};


