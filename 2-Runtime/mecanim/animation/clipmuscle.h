#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/Math/Simd/float4.h"
#include "Runtime/mecanim/animation/curvedata.h"
#include "Runtime/mecanim/human/human.h"
#include "Runtime/mecanim/human/hand.h"

#include "Runtime/Serialize/Blobification/offsetptr.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Animation/MecanimArraySerialization.h"

namespace mecanim
{

namespace animation
{
	const int32_t s_ClipMuscleCurveTQCount = 7;
	const int32_t s_ClipMuscleDoFBegin = (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount;
	const int32_t s_ClipMotionCurveCount = 7;
	const int32_t s_ClipMuscleCurveCount = s_ClipMotionCurveCount + (1 + human::kLastGoal) * 7 + human::kLastDoF + 2 * hand::s_DoFCount;

	int32_t FindMuscleIndex(uint32_t id);
	mecanim::String const& GetMuscleCurveName(int32_t curveIndex);
	float GetXformCurveValue(math::xform const& x, int32_t index);
	float GetMuscleCurveValue(human::HumanPose const& pose, math::xform const &motionX, int32_t curveIndex);
	bool GetMuscleCurveInMask(human::HumanPoseMask const& mask, int32_t curveIndex);
	int32_t GetMuscleCurveTQIndex(int32_t curveIndex);

	void InitializeMuscleClipTables ();
	
	enum { kTargetReference, kTargetRoot, kTargetLeftFoot, kTargetRightFoot, kTargetLeftHand, kTargetRightHand };

	struct ValueDelta
	{
		DEFINE_GET_TYPESTRING(ValueDelta)

		float m_Start;
		float m_Stop;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_Start);
			TRANSFER(m_Stop);	
		}
	};

	// Constant 
	// @TODO: create a smaller constant to use with non humanoid clips. There is too much overhead here
	struct ClipMuscleConstant
	{
		DEFINE_GET_TYPESTRING(ClipMuscleConstant)

		ClipMuscleConstant() :
			m_StartX(math::xformIdentity()),
			m_LeftFootStartX(math::xformIdentity()),
			m_RightFootStartX(math::xformIdentity()),
			m_MotionStartX(math::xformIdentity()),
			m_MotionStopX(math::xformIdentity()),
			m_ValueArrayCount(0),
			m_AverageSpeed(math::float4::zero()),
			m_Mirror(false),
			m_StartTime(0),
			m_StopTime(1),
			m_LoopTime(false),
			m_LoopBlend(false),
			m_LoopBlendOrientation(false),
			m_LoopBlendPositionXZ(false), 
			m_LoopBlendPositionY(false), 
			m_KeepOriginalOrientation(false),
			m_KeepOriginalPositionY(true),
			m_KeepOriginalPositionXZ(false),
			m_HeightFromFeet(false),
			m_OrientationOffsetY(0), 
			m_Level(0),
			m_CycleOffset(0),			
			m_AverageAngularSpeed(0)
		{
			int32_t i;
			for(i = 0; i < s_ClipMuscleCurveCount; i++)
			{
				m_IndexArray[i] = -1;
			}
		}

		human::HumanPose m_DeltaPose;			
		
		math::xform	m_StartX;
		math::xform m_LeftFootStartX;
		math::xform m_RightFootStartX;
		
		math::xform	m_MotionStartX;
		math::xform	m_MotionStopX;
		
		math::float4 m_AverageSpeed;

		OffsetPtr<Clip>	m_Clip;
		
		float	m_StartTime;
		float	m_StopTime;
		float	m_OrientationOffsetY;
		float	m_Level;
		float	m_CycleOffset;
		float	m_AverageAngularSpeed;

		int32_t				m_IndexArray[s_ClipMuscleCurveCount];

		uint32_t				m_ValueArrayCount;
		OffsetPtr<ValueDelta> m_ValueArrayDelta;

		bool	m_Mirror;
		bool	m_LoopTime;
		bool	m_LoopBlend;
		bool	m_LoopBlendOrientation;
		bool	m_LoopBlendPositionY;
		bool	m_LoopBlendPositionXZ;

		bool	m_KeepOriginalOrientation;
		bool	m_KeepOriginalPositionY;
		bool	m_KeepOriginalPositionXZ;
		bool	m_HeightFromFeet;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			transfer.SetVersion(2);

			TRANSFER(m_DeltaPose);			
			TRANSFER(m_StartX);
			TRANSFER(m_LeftFootStartX);
			TRANSFER(m_RightFootStartX);
			TRANSFER(m_MotionStartX);
			TRANSFER(m_MotionStopX);
			TRANSFER(m_AverageSpeed);
			TRANSFER(m_Clip);
			TRANSFER(m_StartTime);
			TRANSFER(m_StopTime);
			TRANSFER(m_OrientationOffsetY);
			TRANSFER(m_Level);
			TRANSFER(m_CycleOffset);

			TRANSFER(m_AverageAngularSpeed);

			STATIC_ARRAY_TRANSFER(mecanim::int32_t, m_IndexArray, s_ClipMuscleCurveCount);

			TRANSFER_BLOB_ONLY(m_ValueArrayCount);
			MANUAL_ARRAY_TRANSFER2(ValueDelta, m_ValueArrayDelta, m_ValueArrayCount);

			TRANSFER(m_Mirror);
			
			TRANSFER(m_LoopTime);
			TRANSFER(m_LoopBlend);

			m_LoopTime = transfer.IsVersionSmallerOrEqual(1) ? m_LoopBlend : m_LoopTime;

			TRANSFER(m_LoopBlendOrientation);
			TRANSFER(m_LoopBlendPositionY);
			TRANSFER(m_LoopBlendPositionXZ);

			TRANSFER(m_KeepOriginalOrientation);
			TRANSFER(m_KeepOriginalPositionY);
			TRANSFER(m_KeepOriginalPositionXZ);
			TRANSFER(m_HeightFromFeet);

			transfer.Align();
		}
	};

	// Input
	struct ClipMuscleInput
	{
		ClipMuscleInput() : m_Time(0),
							m_PreviousTime(0),
							m_TargetIndex(kTargetReference),
							m_TargetTime(1), m_Reverse(false), m_Mirror(false), m_CycleOffset(0){}

		float	m_Time;
		float	m_PreviousTime;
		
		int32_t	m_TargetIndex;
		float	m_TargetTime;
		bool	m_Reverse;
		bool	m_Mirror;
		float	m_CycleOffset;
	};

	// Output
	struct MotionOutput
	{
		DEFINE_GET_TYPESTRING(MotionOutput)

		MotionOutput() :		m_DX(math::xformIdentity()),
								m_MotionX(math::xformIdentity()),
								m_MotionStartX(math::xformIdentity()),
								m_MotionStopX(math::xformIdentity()),
								m_PrevRootX(math::xformIdentity()),								
								m_PrevLeftFootX(math::xformIdentity()),
								m_PrevRightFootX(math::xformIdentity()),
								m_TargetX(math::xformIdentity()),
								m_GravityWeight(0) {}

		math::xform			m_DX;	
		math::xform			m_MotionX;
		math::xform			m_MotionStartX;
		math::xform			m_MotionStopX;

		math::xform			m_PrevRootX;
		math::xform			m_PrevLeftFootX;
		math::xform			m_PrevRightFootX;

		math::xform			m_TargetX;

		float				m_GravityWeight;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_DX);	
			TRANSFER(m_MotionX);
			TRANSFER(m_MotionStartX);
			TRANSFER(m_MotionStopX);
			TRANSFER(m_PrevRootX);
			TRANSFER(m_PrevLeftFootX);
			TRANSFER(m_PrevRightFootX);
			TRANSFER(m_TargetX);
			TRANSFER(m_GravityWeight);
		};
	};

	void MotionOutputClear(MotionOutput *output);
	void MotionOutputCopy(MotionOutput *output, MotionOutput const *motion, bool hasRootMotion, bool isHuman, human::HumanPoseMask const &poseMask);
	void MotionOutputBlend(MotionOutput *output, MotionOutput **outputArray, float *weigh, uint32_t count, bool hasRootMotion, bool isHuman, human::HumanPoseMask const &poseMask);
	void MotionAddAdditiveLayer(MotionOutput *output, MotionOutput const *motion, float weight, bool hasRootMotion, bool isHuman, human::HumanPoseMask const &poseMask);						
	void MotionAddOverrideLayer(MotionOutput *output, MotionOutput const *motion, float weight, bool hasRootMotion, bool isHuman, human::HumanPoseMask const &poseMask);						

	ClipMuscleConstant* CreateClipMuscleConstant(Clip * clip, memory::Allocator& alloc);
	void DestroyClipMuscleConstant(ClipMuscleConstant * constant, memory::Allocator& alloc);

	ClipMuscleInput* CreateClipMuscleInput(ClipMuscleConstant const* constant, memory::Allocator& alloc);
	void DestroyClipMuscleInput(ClipMuscleInput * input, memory::Allocator& alloc);

	float ComputeClipTime(float normalizedTimeIn, float startTime, float stopTime, float cycleOffset, bool loop, bool reverse, float &nomralizedTimeOut, float &timeInt);

	void EvaluateClipRootMotionDeltaX(const ClipMuscleConstant& constant, const ClipMuscleInput &input, MotionOutput &output, ClipMemory &memory);
	
	void EvaluateClipMusclePrevTime(const ClipMuscleConstant& constant, const ClipMuscleInput &input, MotionOutput &output, ClipMemory &memory);
	void EvaluateClipMuscle(const ClipMuscleConstant& constant, const ClipMuscleInput &input, const float *valuesOutput, MotionOutput &motionOutput, human::HumanPose &humanPose, ClipMemory &memory);

	void ComputeClipMuscleDeltaPose(ClipMuscleConstant const &constant, float startTime, float stopTime, human::HumanPose &deltaPose, math::xform &startX, math::xform &leftFootStartX, math::xform &rightFootStartX, memory::Allocator& alloc);
	
	void GetHumanPose(const ClipMuscleConstant& constant, const float* values, human::HumanPose &humanPose);	
	void GetHumanPose(const ClipMuscleConstant& constant, const ValueDelta* values, human::HumanPose &humanPose);
	
	void InitClipMuscleDeltaValues(ClipMuscleConstant& constant);
	void InitClipMuscleDeltaPose(ClipMuscleConstant& constant);
	void InitClipMuscleAverageSpeed(ClipMuscleConstant& constant, int steps = 20);

	void InitMuscleClipIndexArray(ClipMuscleConstant& constant, memory::Allocator& alloc);
	size_t GetClipCurveCount(const ClipMuscleConstant& constant);
}
}
