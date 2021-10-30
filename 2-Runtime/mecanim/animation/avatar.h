#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/bind.h"
#include "Runtime/Math/Simd/float4.h"

#include "Runtime/mecanim/skeleton/skeleton.h"
#include "Runtime/mecanim/human/human.h"
#include "Runtime/mecanim/animation/clipmuscle.h"
#include "Runtime/mecanim/animation/curvedata.h"
#include "Runtime/mecanim/statemachine/statemachine.h"

#include "Runtime/Serialize/Blobification/offsetptr.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Animation/MecanimArraySerialization.h" 

typedef UInt32 BindingHash;

namespace mecanim
{

	static const uint32_t DISABLED_SYNCED_LAYER_IN_NON_PRO = 0xffffffff;
	struct ValueArrayConstant;

	typedef int ProcessString(mecanim::String const&);
	
	namespace animation
	{
		struct ClipMuscleConstant;
		struct AvatarConstant;
		struct ControllerConstant;

		enum LayerBlendingMode
		{
			kLayerBlendingModeOverride,
			kLayerBlendingModeAdditive		
		};
	
		struct LayerConstant
		{
			DEFINE_GET_TYPESTRING(LayerConstant)

			LayerConstant():m_IKPass(true), m_SyncedLayerAffectsTiming(false), m_LayerBlendingMode(kLayerBlendingModeOverride){}

			uint32_t				m_StateMachineIndex;
			uint32_t				m_StateMachineMotionSetIndex;

			human::HumanPoseMask	m_BodyMask;
			OffsetPtr<skeleton::SkeletonMask> m_SkeletonMask;
									
			uint32_t				m_Binding;
			uint32_t				m_LayerBlendingMode; //LayerBlendingMode
			float					m_DefaultWeight;
			bool					m_IKPass;
			bool					m_SyncedLayerAffectsTiming;

			template<class TransferFunction>
			inline void Transfer (TransferFunction& transfer)
			{				

				TRANSFER(m_StateMachineIndex);
				TRANSFER(m_StateMachineMotionSetIndex);
				TRANSFER(m_BodyMask);
				TRANSFER(m_SkeletonMask);				
				TRANSFER(m_Binding);			
				TRANSFER((int&)m_LayerBlendingMode);
				TRANSFER(m_DefaultWeight);
				TRANSFER(m_IKPass);
				TRANSFER(m_SyncedLayerAffectsTiming);
				transfer.Align();	
				
				
			}
		};

		struct ControllerConstant
		{
			DEFINE_GET_TYPESTRING(ControllerConstant)

			ControllerConstant():	m_LayerCount(0),
									m_StateMachineCount(0) {}

			uint32_t													m_LayerCount;
			OffsetPtr< OffsetPtr<LayerConstant> >						m_LayerArray;

			uint32_t													m_StateMachineCount;
			OffsetPtr< OffsetPtr<statemachine::StateMachineConstant> >	m_StateMachineArray;

			OffsetPtr<ValueArrayConstant>								m_Values;													
			OffsetPtr<ValueArray>										m_DefaultValues;		

			template<class TransferFunction>
			inline void Transfer (TransferFunction& transfer)
			{
				TRANSFER_BLOB_ONLY(m_LayerCount);
				MANUAL_ARRAY_TRANSFER2( OffsetPtr<mecanim::animation::LayerConstant>, m_LayerArray, m_LayerCount);
				
				TRANSFER_BLOB_ONLY(m_StateMachineCount);
				MANUAL_ARRAY_TRANSFER2( OffsetPtr<mecanim::statemachine::StateMachineConstant>, m_StateMachineArray, m_StateMachineCount);
				
				TRANSFER(m_Values);							
				TRANSFER(m_DefaultValues);
				
			}	

			static void InitializeClass();
		};

		struct SkeletonTQSMap
		{
			SkeletonTQSMap() : m_TIndex(-1), m_QIndex(-1), m_SIndex(-1) {};

			int32_t m_TIndex;
			int32_t m_QIndex;
			int32_t m_SIndex;
		};
		
		struct ClipBindings
		{
			ClipBindings () : m_PositionIndex(0), m_QuaternionIndex(0), m_ScaleIndex(0), m_FloatIndex(0), m_IntIndex(0), m_IntegerRemap(0)  {}
			
			// Maps from the curve float array of the clip to the ValueArrayConstant.
			// This allows us to bring the curve float array into a ValueArray that has the same layout between all clips.
			int16_t* m_PositionIndex;
			int16_t* m_QuaternionIndex;
			int16_t* m_ScaleIndex;
			int16_t* m_FloatIndex;
			int16_t* m_IntIndex;
			
			// Points directly to AnimationClipBindingConstant pptrCurveMapping.
			int32_t* m_IntegerRemap;
		};

		struct AnimationSet
		{	
			struct Clip
			{
				Clip() : m_Clip(0), m_TotalUsedOptimizedCurveCount(0), m_ClipIndex(-1) {}

				// The referenced constant clip
				ClipMuscleConstant*	m_Clip;
				int32_t m_ClipIndex;
				// The amount of ConstantClip curves that need to be sampled (Constant curves are often ignored during binding if they are determined to have no impact)
				uint32_t            m_TotalUsedOptimizedCurveCount;
				// Binding indices to index from the curve float[] to the ValueArray
				ClipBindings        m_Bindings;
			};

			AnimationSet() :	m_MaxBlendState(0),
								m_LayerCount(0), 
								m_ClipPerLayer(0),
								m_ClipConstant(0), 
								m_AdditionalCount(0), 
								m_AdditionalIndexArray(0),
								m_DynamicFullValuesConstant(0),
								m_DynamicValuesMaskArray(0),
								m_GravityWeightIndex(-1),
								m_IntegerRemapStride(-1)
								{}

			uint32_t										m_MaxBlendState;
			uint32_t										m_LayerCount;
			uint32_t*										m_ClipPerLayer;

			Clip**											m_ClipConstant;
			
			uint32_t										m_AdditionalCount;
			int32_t*										m_AdditionalIndexArray;

			mecanim::ValueArrayConstant*					m_DynamicFullValuesConstant;
			mecanim::ValueArrayMask**						m_DynamicValuesMaskArray;

			int32_t											m_GravityWeightIndex;
			int32_t											m_IntegerRemapStride;
		};

		struct ControllerBindingConstant
		{
			ControllerBindingConstant():	m_DynamicValuesDefault(0),
											m_SkeletonTQSMap(0),
											m_RootMotionLayerMask(0),
											m_Avatar(0),
											m_Controller(0),
											m_DynamicValuesConstant(0),
											m_AnimationSet(0) {}

			ValueArrayConstant*								m_DynamicValuesConstant;
			ValueArray*										m_DynamicValuesDefault;		

			SkeletonTQSMap*									m_SkeletonTQSMap;

			bool*											m_RootMotionLayerMask;

			AvatarConstant const*							m_Avatar;
			ControllerConstant const*						m_Controller;
			AnimationSet const*								m_AnimationSet;
		};

		struct AnimationSetMemory
		{
			AnimationSetMemory() : m_LayerCount(0), m_ClipPerLayer(0), m_ClipMemory(0), m_ClipOutput(0) {}

			uint32_t				m_LayerCount;
			uint32_t*				m_ClipPerLayer;
			ClipMemory***			m_ClipMemory;
			ClipOutput*				m_ClipOutput;
		};

		template<bool dynamic>
		struct BlendingState
		{
			BlendingState():
				m_DynamicValuesBlending(0),
				m_MotionBlending(0),
				m_HumanPoseBlending(0),
				m_BlendFactor(0)
			{}

			ValueArray**				m_DynamicValuesBlending;
			MotionOutput**				m_MotionBlending;
			human::HumanPose**			m_HumanPoseBlending;
			float* m_BlendFactor;

			uint32_t					m_Size;
		};

		template<>
		struct BlendingState<false>
		{
			BlendingState(): m_BlendFactor(0) {}

			OffsetPtr<ValueArray> m_DynamicValuesBlending;
			OffsetPtr<ValueArrayMask> m_DynamicValuesBlendingMask;
			OffsetPtr<MotionOutput>	m_MotionBlending;
			OffsetPtr<human::HumanPose>	m_HumanPoseBlending;
			float m_BlendFactor;
		};

		struct BlendingClip
		{
			BlendingClip() : m_ClipIndex(-1), m_LayerIndex(-1), m_Weight(0), m_PrevTime(0), m_Time(0), m_Reverse(false) {}

			int m_ClipIndex;
			int m_LayerIndex;
			float m_Weight;
			float m_PrevTime;
			float m_Time;
			bool m_Reverse;
		};

		struct ControllerMemory
		{
			DEFINE_GET_TYPESTRING(ControllerMemory)

			ControllerMemory():	m_StateMachineCount(0),
								m_LayerCount(0) {}

			uint32_t													m_StateMachineCount;
			OffsetPtr< OffsetPtr<statemachine::StateMachineMemory> >	m_StateMachineMemory;

			uint32_t													m_LayerCount;
			OffsetPtr<BlendingState<false> >							m_InteruptedTransitionsBlendingStateArray;
			OffsetPtr<float>											m_LayerWeights;

			OffsetPtr<ValueArray>										m_Values;

			template<class TransferFunction>
			inline void Transfer (TransferFunction& transfer)
			{
				TRANSFER_BLOB_ONLY(m_StateMachineCount);
				MANUAL_ARRAY_TRANSFER2( OffsetPtr<mecanim::statemachine::StateMachineMemory>, m_StateMachineMemory, m_StateMachineCount);

				TRANSFER_BLOB_ONLY(m_LayerCount);
				MANUAL_ARRAY_TRANSFER2( BlendingState<false>, m_InteruptedTransitionsBlendingStateArray, m_LayerCount);						
				MANUAL_ARRAY_TRANSFER2( float, m_LayerWeights, m_LayerCount);

				TRANSFER(m_Values);							
			}
		};

		struct ControllerWorkspace 
		{
			ControllerWorkspace() :	m_StateMachineWorkspace(0),
									m_StateMachineOutput(0),
									m_BlendingState(0),
									m_BlendingClipCount(0),
									m_BlendingClipArray(0),
									m_ValueArrayStart(0),
									m_ValueArrayStop(0),
									m_ReadMask(0),
									m_BlendMask(0),
									m_DefaultMask(0),
									m_DoIK(false),
									m_DoWrite(false) {}
	
			statemachine::StateMachineWorkspace**   m_StateMachineWorkspace;
			statemachine::StateMachineOutput**		m_StateMachineOutput;
			
			uint32_t								m_StateMachineCount;

			float*									m_MotionSetTimingWeightArray;

			BlendingState<true>*					m_BlendingState;
			BlendingState<false>					m_BlendingStateWs;

			int										m_BlendingClipCount;
			BlendingClip*							m_BlendingClipArray;

			ValueArray								*m_ValueArrayStart;
			ValueArray								*m_ValueArrayStop;

			ValueArrayMask							*m_ReadMask;
			ValueArrayMask							*m_BlendMask;
			ValueArrayMask							*m_DefaultMask;

			bool									m_DoIK;
			bool									m_DoWrite;
		};

		struct ExposedTransform
		{
			DEFINE_GET_TYPESTRING(ExposedTransform);

			// For SkinnedMeshRenderer, the following two indices are different
			// - 'skeletonIndex'
			//		corresponds to the SkinnedMeshRenderer itself
			// - 'skeletonIndexForUpdateTransform'
			//		corresponds to the root bone of the SkinnedMeshRenderer
			uint32_t	skeletonIndex;
			uint32_t    skeletonIndexForUpdateTransform;

			BindingHash	transformPath;		// flattened path

			template<class TransferFunction>
			inline void Transfer (TransferFunction& transfer)
			{
				TRANSFER(skeletonIndex);
				TRANSFER(skeletonIndexForUpdateTransform);
				TRANSFER(transformPath);
			}	
		};

		struct AvatarConstant
		{
			DEFINE_GET_TYPESTRING(AvatarConstant)

			AvatarConstant() :	m_SkeletonNameIDCount(0), 
								m_HumanSkeletonIndexCount(0),
								m_HumanSkeletonReverseIndexCount(0),
								m_RootMotionBoneIndex(-1), 
								m_RootMotionBoneX(math::xformIdentity()),
								m_RootMotionSkeletonIndexCount(0) {}

			OffsetPtr<skeleton::Skeleton>			m_AvatarSkeleton;
			OffsetPtr<skeleton::SkeletonPose>		m_AvatarSkeletonPose;

			OffsetPtr<skeleton::SkeletonPose>		m_DefaultPose;	// The default pose when model is imported.

			uint32_t								m_SkeletonNameIDCount;
			OffsetPtr<uint32_t>						m_SkeletonNameIDArray;	// CRC(name)

			OffsetPtr<human::Human>					m_Human;

			uint32_t								m_HumanSkeletonIndexCount;
			OffsetPtr<int32_t>						m_HumanSkeletonIndexArray;

			// needed to update human pose and additonal bones in optimize mode
			// decided to put the info in constant for perf and memory reason vs doing masking at runtime
			uint32_t								m_HumanSkeletonReverseIndexCount;
			OffsetPtr<int32_t>						m_HumanSkeletonReverseIndexArray;

			int32_t									m_RootMotionBoneIndex;
			math::xform								m_RootMotionBoneX;
			OffsetPtr<skeleton::Skeleton>			m_RootMotionSkeleton;
			OffsetPtr<skeleton::SkeletonPose>		m_RootMotionSkeletonPose;
			uint32_t								m_RootMotionSkeletonIndexCount;
			OffsetPtr<int32_t>						m_RootMotionSkeletonIndexArray;

			template<class TransferFunction>
			inline void Transfer (TransferFunction& transfer)
			{
				transfer.SetVersion(3);

				TRANSFER(m_AvatarSkeleton);

				TRANSFER(m_AvatarSkeletonPose);
				TRANSFER(m_DefaultPose);

				TRANSFER_BLOB_ONLY(m_SkeletonNameIDCount);
				MANUAL_ARRAY_TRANSFER2(uint32_t,m_SkeletonNameIDArray,m_SkeletonNameIDCount);	

				TRANSFER(m_Human);

				TRANSFER_BLOB_ONLY(m_HumanSkeletonIndexCount);
				MANUAL_ARRAY_TRANSFER2(int32_t,m_HumanSkeletonIndexArray,m_HumanSkeletonIndexCount);	

				TRANSFER_BLOB_ONLY(m_HumanSkeletonReverseIndexCount);
				MANUAL_ARRAY_TRANSFER2(int32_t,m_HumanSkeletonReverseIndexArray,m_HumanSkeletonReverseIndexCount);	

				TRANSFER(m_RootMotionBoneIndex);
				TRANSFER(m_RootMotionBoneX);
				TRANSFER(m_RootMotionSkeleton);
				TRANSFER(m_RootMotionSkeletonPose);

				TRANSFER_BLOB_ONLY(m_RootMotionSkeletonIndexCount);
				MANUAL_ARRAY_TRANSFER2(int32_t,m_RootMotionSkeletonIndexArray,m_RootMotionSkeletonIndexCount);	

				transfer.Align();

				if (transfer.IsVersionSmallerOrEqual (1))
				{
					if(m_RootMotionBoneIndex != -1)
					{
						mecanim::memory::Allocator *alloc = reinterpret_cast<mecanim::memory::Allocator *>(transfer.GetUserData());

						m_RootMotionSkeleton = skeleton::CreateSkeleton(m_AvatarSkeleton->m_Count,m_AvatarSkeleton->m_AxesCount,*alloc);
						skeleton::SkeletonCopy(m_AvatarSkeleton.Get(),m_RootMotionSkeleton.Get());

						m_RootMotionSkeletonPose = skeleton::CreateSkeletonPose(m_RootMotionSkeleton.Get(),*alloc);
						skeleton::SkeletonPoseCopy(m_AvatarSkeletonPose.Get(),m_RootMotionSkeletonPose.Get());						
						
						m_RootMotionSkeletonIndexCount = m_AvatarSkeleton->m_Count;
						m_RootMotionSkeletonIndexArray = alloc->ConstructArray<mecanim::int32_t>(m_RootMotionSkeletonIndexCount);

						for(int i = 0; i < m_RootMotionSkeletonIndexCount; i++)
						{
							m_RootMotionSkeletonIndexArray[i] = i;
						}
					}
				}

				if (transfer.IsVersionSmallerOrEqual (2))
				{
					if(isHuman())
					{
						mecanim::memory::Allocator *alloc = reinterpret_cast<mecanim::memory::Allocator *>(transfer.GetUserData());

						m_HumanSkeletonReverseIndexCount = m_AvatarSkeleton->m_Count;
						m_HumanSkeletonReverseIndexArray = alloc->ConstructArray<mecanim::int32_t>(m_HumanSkeletonReverseIndexCount);
						skeleton::SkeletonBuildReverseIndexArray(m_HumanSkeletonReverseIndexArray.Get(),m_HumanSkeletonIndexArray.Get(),m_Human->m_Skeleton.Get(),m_AvatarSkeleton.Get());
					}
				}
			}

			bool isHuman() const { return !m_Human.IsNull() && m_Human->GetTypeString() != 0 && m_Human->m_Skeleton->m_Count > 0; };

			static void InitializeClass();
		};

		struct AvatarInput 
		{
			AvatarInput() : m_GotoStateInfos(0), m_DeltaTime(0), m_TargetIndex(-1), m_TargetTime(1), m_FeetPivotActive(1), m_StabilizeFeet(false), m_ForceStateTime(false), m_StateTime(0), m_LayersAffectMassCenter(false) {}

			statemachine::GotoStateInfo*	m_GotoStateInfos;
			float			m_DeltaTime;
			int				m_TargetIndex;
			float			m_TargetTime;

			float			m_FeetPivotActive;
			bool			m_StabilizeFeet;

			bool			m_ForceStateTime;
			float			m_StateTime;
			bool			m_LayersAffectMassCenter;
		};
		
		struct AvatarMemory
		{
			DEFINE_GET_TYPESTRING(AvatarMemory)

			AvatarMemory() :	m_AvatarX(math::xformIdentity()),
								m_LeftFootX(math::xformIdentity()),
								m_RightFootX(math::xformIdentity()),
								m_Pivot(math::float4::zero()),
								m_PivotWeight(0.5),								
								m_FirstEval(1),
								m_SkeletonPoseOutputReady(0) {}

			OffsetPtr<ControllerMemory>		m_ControllerMemory;

			math::xform						m_AvatarX;
			
			math::xform						m_LeftFootX;
			math::xform						m_RightFootX;			
			math::float4					m_Pivot;

			float							m_PivotWeight;
			UInt8							m_FirstEval;
			UInt8							m_SkeletonPoseOutputReady;


			template<class TransferFunction>
			inline void Transfer (TransferFunction& transfer)
			{
				TRANSFER(m_ControllerMemory);
				TRANSFER(m_AvatarX);
				TRANSFER(m_LeftFootX);
				TRANSFER(m_RightFootX);
				TRANSFER(m_Pivot);
				TRANSFER(m_PivotWeight);
				TRANSFER(m_FirstEval);
				TRANSFER(m_SkeletonPoseOutputReady);
				transfer.Align();
			}
		};

		struct AvatarWorkspace 
		{
			AvatarWorkspace() : m_BodySkeletonPoseWs(0),
								m_BodySkeletonPoseWsA(0),
								m_BodySkeletonPoseWsB(0),
								m_RootMotionSkeletonPoseWsA(0),
								m_RootMotionSkeletonPoseWsB(0),
								m_HumanPoseWs(0),
								m_ControllerWorkspace(0),
								m_LeftFootSpeedT(0),
								m_LeftFootSpeedQ(0),
								m_RightFootSpeedT(0),
								m_RightFootSpeedQ(0),
								m_IKOnFeet(false)
								{} 
				
			skeleton::SkeletonPose*					m_BodySkeletonPoseWs;
			skeleton::SkeletonPose*					m_BodySkeletonPoseWsA;
			skeleton::SkeletonPose*					m_BodySkeletonPoseWsB;
		
			skeleton::SkeletonPose*					m_RootMotionSkeletonPoseWsA;
			skeleton::SkeletonPose*					m_RootMotionSkeletonPoseWsB;

			human::HumanPose*						m_HumanPoseWs;

			ControllerWorkspace*					m_ControllerWorkspace;
		
			math::xform								m_AvatarX;
			
			float									m_LeftFootSpeedT;
			float									m_LeftFootSpeedQ;
			float									m_RightFootSpeedT;
			float									m_RightFootSpeedQ;
			bool									m_IKOnFeet;
		};

		struct AvatarOutput 
		{
			AvatarOutput() :		m_DynamicValuesOutput(0),
									m_SkeletonPoseOutput(0),
									m_MotionOutput(0),
									m_HumanPoseBaseOutput(0),
									m_HumanPoseOutput(0) {}
	
			ValueArray*				m_DynamicValuesOutput;

			skeleton::SkeletonPose*	m_SkeletonPoseOutput;

			MotionOutput*			m_MotionOutput;
			human::HumanPose*		m_HumanPoseBaseOutput;
			human::HumanPose*		m_HumanPoseOutput;
		};

		AvatarConstant*     CreateAvatarConstant(	skeleton::Skeleton* skeleton,
													skeleton::SkeletonPose* skeletonPose,
													skeleton::SkeletonPose* defaultPose,
													human::Human* human,
													skeleton::Skeleton* rootMotionSkeleton,
													int rootMotionIndex,
													math::xform const& rootMotionX,
													memory::Allocator& alloc);

		void				DestroyAvatarConstant(AvatarConstant* constant, memory::Allocator& alloc);

		void				InitializeAvatarConstant(AvatarConstant * constant, memory::Allocator& alloc);
		void				ClearAvatarConstant(AvatarConstant * constant, memory::Allocator& alloc);

		ControllerConstant* CreateControllerConstant(	uint32_t LayerCount, LayerConstant** layerArray,
														uint32_t stateMachineCount, statemachine::StateMachineConstant** stateMachineConstant,
														ValueArrayConstant* values, ValueArray* defaultValues,
														memory::Allocator& alloc);

		void				DestroyControllerConstant(ControllerConstant* controller, memory::Allocator& alloc);

		void				InitializeControllerConstant(ControllerConstant * controller, memory::Allocator& alloc);
		void				ClearControllerConstant(ControllerConstant * controller, memory::Allocator& alloc);
		
		LayerConstant* CreateLayerConstant(mecanim::uint32_t stateMachineIndex, mecanim::uint32_t motionSetIndex, memory::Allocator& alloc);
		void				DestroyLayerConstant(LayerConstant* constant, memory::Allocator& alloc);

		ControllerMemory* CreateControllerMemory(ControllerConstant const* controller, AvatarConstant const *avatar, AnimationSet const *animationSet, const ValueArrayConstant* dynamicValueConstant, memory::Allocator& alloc);
		void						DestroyControllerMemory(ControllerMemory* memory, memory::Allocator& alloc);

		ControllerWorkspace*		CreateControllerWorkspace(ControllerConstant const* controller, AvatarConstant const *avatar, AnimationSet const *animationSet, const ValueArrayConstant* dynamicValueConstant, memory::Allocator& alloc);
		void						DestroyControllerWorkspace(ControllerWorkspace* workspace, memory::Allocator& alloc);

		AvatarInput*		CreateAvatarInput(AvatarConstant const* constant, memory::Allocator& alloc);
		void				DestroyAvatarInput(AvatarInput* input, memory::Allocator& alloc);

		AvatarMemory*		CreateAvatarMemory(AvatarConstant const* constant, memory::Allocator& alloc);
		void				DestroyAvatarMemory(AvatarMemory* memory, memory::Allocator& alloc);

		AvatarWorkspace*	CreateAvatarWorkspace(AvatarConstant const* constant, memory::Allocator& alloc);
		void				DestroyAvatarWorkspace(AvatarWorkspace* workspace, memory::Allocator& alloc);

		AvatarOutput*		CreateAvatarOutput(AvatarConstant const* constant, bool hasTransformHierarchy, memory::Allocator& alloc);
		void				DestroyAvatarOutput(AvatarOutput* output, memory::Allocator& alloc);

		AnimationSet*		CreateAnimationSet(ControllerConstant const* controller, memory::Allocator& alloc);
		void				DestroyAnimationSet(AnimationSet* animationSet, memory::Allocator& alloc);

		AnimationSetMemory*	CreateAnimationSetMemory(AnimationSet const* animationSet, bool allowConstantCurveOptimization, memory::Allocator& alloc);
		void				DestroyAnimationSetMemory(AnimationSetMemory* animationSetMemory, memory::Allocator& alloc);		

		void UpdateLeafNodeDuration(const ControllerConstant &controllerConstant, const AnimationSet &animationSet, ControllerMemory &controllerMemory);

		void				SetIKOnFeet(		bool left,
												AvatarConstant const &avatar, 
												const AvatarInput &input, 
												AvatarMemory &memory, 
												AvatarWorkspace &workspace, 
												AvatarOutput &output);

		void				EvaluateAvatarSM(	AvatarConstant const* constant, 
												AvatarInput const* input, 
												AvatarOutput * output, 
												AvatarMemory * memory, 
												AvatarWorkspace * workspace,
												ControllerConstant const* controllerConstant);

		void				EvaluateAvatarLayers(	ControllerBindingConstant const* controllerBindingConstant, 
													AvatarInput const* input, 
													AvatarOutput *output, 
													AvatarMemory *memory, 
													AvatarWorkspace *workspace,
													AnimationSetMemory* animationSetMemory);

		void				EvaluateAvatarX(	AvatarConstant const* constant, 
												AvatarInput const* input, 
												AvatarOutput *output, 
												AvatarMemory *memory, 
												AvatarWorkspace *workspace);

		void				EvaluateAvatarRetarget(	AvatarConstant const* constant, 
													AvatarInput const* input, 
													AvatarOutput *output, 
													AvatarMemory *memory, 
													AvatarWorkspace *workspace,
													ControllerConstant const* controllerConstant);

		void				EvaluateAvatarIK(	AvatarConstant const* constant, 
												AvatarInput const* input, 
												AvatarOutput *output, 
												AvatarMemory *memory, 
												AvatarWorkspace *workspace,
												ControllerConstant const* controllerConstant);

		void				EvaluateAvatarEnd(	AvatarConstant const* constant, 
													AvatarInput const* input, 
													AvatarOutput *output, 
													AvatarMemory *memory, 
													AvatarWorkspace *workspace,
													ControllerConstant const* controllerConstant);

	void ValuesFromClip(	mecanim::ValueArray const &valuesDefault,
							mecanim::animation::ClipMuscleConstant const &cst, 
							mecanim::animation::ClipOutput const &out,
							const ClipBindings& bindings,
							int32_t        integerRemapStride,
							mecanim::ValueArray &values,
							mecanim::ValueArrayMask &mask);

	void DeltasFromClip(	mecanim::animation::ClipMuscleConstant const &cst,
							const ClipBindings& bindings,
							const ValueArrayMask& mask,
							mecanim::ValueArray &starts,
							mecanim::ValueArray &stops);

	void SkeletonPoseFromValue(skeleton::Skeleton const &skeleton, skeleton::SkeletonPose const &defaultPose, ValueArray const &values, SkeletonTQSMap const *skeletonTQSMap, skeleton::SkeletonPose &pose,int32_t const *humanReverseIndex,bool skipRoot);
	void SkeletonPoseFromValue(skeleton::Skeleton const &skeleton, skeleton::SkeletonPose const &defaultPose, ValueArray const &values, SkeletonTQSMap const *skeletonTQSMap, int32_t const *indexArray, skeleton::SkeletonPose &pose,int index, int stopIndex);
	void ValueFromSkeletonPose(skeleton::Skeleton const &skeleton, skeleton::SkeletonPose const &pose, SkeletonTQSMap const *skeletonTQSMap, ValueArray &values);
	void ValueFromSkeletonPose(skeleton::Skeleton const &skeleton, skeleton::SkeletonPose const &pose, SkeletonTQSMap const *skeletonTQSMap, int32_t const *indexArray, ValueArray &values, int index, int stopIndex);
	}
}

template<>
class SerializeTraits< mecanim::animation::BlendingState<false> > : public SerializeTraitsBase< mecanim::animation::BlendingState<false> >
{
	public:

	typedef mecanim::animation::BlendingState<false>	value_type;
	inline static const char* GetTypeString (void*)	{ return "BlendingState<1>"; }
	inline static bool IsAnimationChannel ()	{ return false; }
	inline static bool MightContainPPtr ()	{ return true; }
	inline static bool AllowTransferOptimization ()	{ return false; }

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.Transfer(data.m_DynamicValuesBlending, "m_DynamicValuesBlending");
		transfer.Transfer(data.m_DynamicValuesBlendingMask, "m_DynamicValuesBlendingMask");
		transfer.Transfer(data.m_MotionBlending, "m_MotionBlending");
		transfer.Transfer(data.m_HumanPoseBlending, "m_HumanPoseBlending");
		transfer.Transfer(data.m_BlendFactor, "m_BlendFactor");
	}
};


