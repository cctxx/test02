#include "UnityPrefix.h"

#include "Runtime/mecanim/animation/avatar.h"

#include "Runtime/mecanim/generic/valuearray.h"
#include "Runtime/mecanim/generic/crc32.h"
#include "Runtime/mecanim/generic/stringtable.h"
#include "Runtime/mecanim/skeleton/skeleton.h"
#include "Runtime/mecanim/human/human.h"
#include "Runtime/mecanim/animation/clipmuscle.h"
#include "Runtime/mecanim/animation/curvedata.h"
#include "Runtime/mecanim/statemachine/statemachine.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"

#include "Runtime/Misc/BuildSettings.h"

namespace mecanim
{

namespace memory
{
	Profiler* Profiler::s_Profiler = NULL;
}

namespace animation
{
	
	AnimationSet* CreateAnimationSet(ControllerConstant const* controller, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(AnimationSet);

		AnimationSet* animationSet = alloc.Construct<AnimationSet>();

		animationSet->m_LayerCount = controller->m_LayerCount;

		animationSet->m_ClipPerLayer = alloc.ConstructArray<uint32_t>(animationSet->m_LayerCount);
		memset(animationSet->m_ClipPerLayer,0,sizeof(uint32_t)*animationSet->m_LayerCount);

		animationSet->m_ClipConstant = alloc.ConstructArray<AnimationSet::Clip*>(animationSet->m_LayerCount);

		animationSet->m_AdditionalCount = controller->m_Values->m_Count;
		animationSet->m_AdditionalIndexArray = alloc.ConstructArray<int32_t>(animationSet->m_AdditionalCount);		
		for(int i = 0; i < animationSet->m_AdditionalCount; i++)
			animationSet->m_AdditionalIndexArray[i] = -1; 

		animationSet->m_DynamicValuesMaskArray = alloc.ConstructArray<ValueArrayMask *>(animationSet->m_LayerCount);	

		for(int layerIter = 0; layerIter < animationSet->m_LayerCount; layerIter++)
		{
			int clipCount = 0;

			mecanim::uint32_t stateMachineIndex = controller->m_LayerArray[layerIter]->m_StateMachineIndex;

			if(stateMachineIndex != DISABLED_SYNCED_LAYER_IN_NON_PRO)
			{
				mecanim::uint32_t motionSetIndex = controller->m_LayerArray[layerIter]->m_StateMachineMotionSetIndex;
					
				const statemachine::StateMachineConstant* stateMachineConstant = controller->m_StateMachineArray[stateMachineIndex].Get();
			
				for(int stateIter = 0; stateIter < stateMachineConstant->m_StateConstantCount; stateIter++)
				{
					statemachine::StateConstant const& stateConstant = *controller->m_StateMachineArray[stateMachineIndex]->m_StateConstantArray[stateIter];

					clipCount += stateConstant.m_LeafInfoArray[motionSetIndex].m_Count;

					for(int blendTreeIter = 0; blendTreeIter < stateConstant.m_BlendTreeCount; blendTreeIter++)
					{
						uint32_t blendCount = GetMaxBlendCount(*stateConstant.m_BlendTreeConstantArray[blendTreeIter]);
						
						if(blendCount > animationSet->m_MaxBlendState) 
						{
							animationSet->m_MaxBlendState = blendCount; 
						}
					}
				}
			}
		
			animationSet->m_ClipPerLayer[layerIter] = clipCount;
			animationSet->m_ClipConstant[layerIter] = alloc.ConstructArray<AnimationSet::Clip>(clipCount);	

			animationSet->m_DynamicValuesMaskArray[layerIter] = 0;
		}

		// if there is no blend tree the worst case is when we are doing a transition between two state
		// in this case we need to evaluate two clip.
		animationSet->m_MaxBlendState = math::maximum<uint32_t>(animationSet->m_MaxBlendState*2, 2); 

		return animationSet;
	}

	void DestroyAnimationSet(AnimationSet* animationSet, memory::Allocator& alloc)
	{
		if(animationSet)
		{
			alloc.Deallocate(animationSet->m_ClipPerLayer);	

			for(int layerIter = 0; layerIter < animationSet->m_LayerCount; layerIter++)
			{
				alloc.Deallocate(animationSet->m_ClipConstant[layerIter]);
				DestroyValueArrayMask(animationSet->m_DynamicValuesMaskArray[layerIter],alloc);
			}

			alloc.Deallocate(animationSet->m_ClipConstant);
			alloc.Deallocate(animationSet->m_AdditionalIndexArray);						
			alloc.Deallocate(animationSet->m_DynamicValuesMaskArray);						
			DestroyValueArrayConstant(animationSet->m_DynamicFullValuesConstant, alloc);
			alloc.Deallocate(animationSet);
		}
	}

	AnimationSetMemory*	CreateAnimationSetMemory(AnimationSet const* animationSet, bool allowConstantCurveOptimization, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(AnimationSetMemory);

		AnimationSetMemory* memory = alloc.Construct<AnimationSetMemory>();

		memory->m_LayerCount = animationSet->m_LayerCount;
		memory->m_ClipPerLayer = alloc.ConstructArray<uint32_t>(animationSet->m_LayerCount);
		memory->m_ClipMemory = alloc.ConstructArray<ClipMemory**>(animationSet->m_LayerCount);		
		
		uint32_t maxCurvesCount = 0;
		for(int layerIter = 0; layerIter < animationSet->m_LayerCount; layerIter++)
		{
			memory->m_ClipPerLayer[layerIter] = animationSet->m_ClipPerLayer[layerIter];
			memory->m_ClipMemory[layerIter] = alloc.ConstructArray<ClipMemory*>(animationSet->m_ClipPerLayer[layerIter]);			
			
			for(int clipIter = 0; clipIter < memory->m_ClipPerLayer[layerIter]; clipIter++)
			{
				const ClipMuscleConstant* clip = animationSet->m_ClipConstant[layerIter][clipIter].m_Clip;
				
				if(clip != 0)
				{
					uint32_t usedCurves;
					if (allowConstantCurveOptimization)
						usedCurves = animationSet->m_ClipConstant[layerIter][clipIter].m_TotalUsedOptimizedCurveCount;
					else
						usedCurves = GetClipCurveCount(*clip);
					
					maxCurvesCount = math::maximum(maxCurvesCount, usedCurves);
					memory->m_ClipMemory[layerIter][clipIter] = mecanim::animation::CreateClipMemory(clip->m_Clip.Get(), usedCurves, alloc);
				}
				else
				{
					memory->m_ClipMemory[layerIter][clipIter] = 0;		
				}
			}
		}

		memory->m_ClipOutput = mecanim::animation::CreateClipOutput(maxCurvesCount, alloc);

		return memory;
	}

	void DestroyAnimationSetMemory(AnimationSetMemory* animationSetMemory, memory::Allocator& alloc)
	{
		if (animationSetMemory)
		{
			for(int layerIter = 0; layerIter < animationSetMemory->m_LayerCount; layerIter++)
			{
				for(int clipIter = 0 ; clipIter < animationSetMemory->m_ClipPerLayer[layerIter]; clipIter++)
				{
					mecanim::animation::DestroyClipMemory(animationSetMemory->m_ClipMemory[layerIter][clipIter], alloc);
				}

				alloc.Deallocate(animationSetMemory->m_ClipMemory[layerIter]);
			}

			alloc.Deallocate(animationSetMemory->m_ClipPerLayer);
			alloc.Deallocate(animationSetMemory->m_ClipMemory);
			mecanim::animation::DestroyClipOutput(animationSetMemory->m_ClipOutput, alloc);
			alloc.Deallocate(animationSetMemory);
		}
	}

	void AvatarConstant::InitializeClass()
	{
		RegisterAllowNameConversion("AvatarConstant", "m_Skeleton", "m_AvatarSkeleton");
		RegisterAllowNameConversion("AvatarConstant", "m_SkeletonPose", "m_AvatarSkeletonPose");
	}

	AvatarConstant*   CreateAvatarConstant(	skeleton::Skeleton* skeleton, 
											skeleton::SkeletonPose* skeletonPose, 
											skeleton::SkeletonPose* defaultPose,
											human::Human* human, 
											skeleton::Skeleton* rootMotionSkeleton,
											int rootMotionIndex,
											math::xform const& rootMotionX,
											memory::Allocator& alloc)
	{
		SETPROFILERLABEL(AvatarConstant);

		AvatarConstant* cst = alloc.Construct<AvatarConstant>();

		cst->m_AvatarSkeleton = skeleton;
		cst->m_AvatarSkeletonPose = skeletonPose;
		cst->m_DefaultPose = defaultPose;
		cst->m_Human = human;
		cst->m_RootMotionSkeleton = rootMotionSkeleton;
		cst->m_RootMotionBoneIndex = rootMotionIndex;
		cst->m_RootMotionBoneX = rootMotionX;

		if(human != 0)
		{
			cst->m_HumanSkeletonIndexCount = human->m_Skeleton->m_Count;
			cst->m_HumanSkeletonIndexArray = alloc.ConstructArray<mecanim::int32_t>(cst->m_HumanSkeletonIndexCount);
			skeleton::SkeletonBuildIndexArray(cst->m_HumanSkeletonIndexArray.Get(),human->m_Skeleton.Get(),skeleton);
			cst->m_HumanSkeletonReverseIndexCount = cst->m_AvatarSkeleton->m_Count;
			cst->m_HumanSkeletonReverseIndexArray = alloc.ConstructArray<mecanim::int32_t>(cst->m_HumanSkeletonReverseIndexCount);
			skeleton::SkeletonBuildReverseIndexArray(cst->m_HumanSkeletonReverseIndexArray.Get(),cst->m_HumanSkeletonIndexArray.Get(),human->m_Skeleton.Get(),skeleton);
		}
		else if(rootMotionIndex != -1)
		{
			cst->m_RootMotionSkeletonIndexCount = rootMotionSkeleton->m_Count;
			cst->m_RootMotionSkeletonIndexArray = alloc.ConstructArray<mecanim::int32_t>(cst->m_RootMotionSkeletonIndexCount);
			skeleton::SkeletonBuildIndexArray(cst->m_RootMotionSkeletonIndexArray.Get(),cst->m_RootMotionSkeleton.Get(),skeleton);
		}

		return cst;
	}

	void DestroyAvatarConstant(AvatarConstant* constant, memory::Allocator& alloc)
	{
		if(constant)
		{
			alloc.Deallocate(constant->m_HumanSkeletonIndexArray);
			alloc.Deallocate(constant->m_HumanSkeletonReverseIndexArray);
			alloc.Deallocate(constant->m_RootMotionSkeletonIndexArray);
			alloc.Deallocate(constant);
		}
	}

	void ClearAvatarConstant(AvatarConstant * constant, memory::Allocator& alloc)
	{
			
	}

	AvatarInput* CreateAvatarInput(AvatarConstant const* constant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(AvatarInput);
		AvatarInput* input	= alloc.Construct<AvatarInput>();

		return input;
	}

	void DestroyAvatarInput(AvatarInput* input, memory::Allocator& alloc)
	{
		if(input)
		{
			alloc.Deallocate(input->m_GotoStateInfos);
			alloc.Deallocate(input);
		}
	}

	AvatarMemory* CreateAvatarMemory(AvatarConstant const* constant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(AvatarMemory);
		AvatarMemory* mem = alloc.Construct<AvatarMemory>();

		return mem;
	}

	void DestroyAvatarMemory(AvatarMemory* memory, memory::Allocator& alloc)
	{
		if(memory)
		{
			alloc.Deallocate(memory);
		}
	}

	AvatarWorkspace* CreateAvatarWorkspace(AvatarConstant const* constant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(AvatarWorkspace);
		AvatarWorkspace* ws	= alloc.Construct<AvatarWorkspace>();
				
		if(constant->isHuman())
		{
			if(!constant->m_AvatarSkeleton.IsNull())
			{
				ws->m_BodySkeletonPoseWs = skeleton::CreateSkeletonPose(constant->m_Human->m_Skeleton.Get(), alloc);	
				ws->m_BodySkeletonPoseWsA = skeleton::CreateSkeletonPose(constant->m_Human->m_Skeleton.Get(), alloc);	
				ws->m_BodySkeletonPoseWsB = skeleton::CreateSkeletonPose(constant->m_Human->m_Skeleton.Get(), alloc);	

				ws->m_HumanPoseWs = alloc.Construct<human::HumanPose>();
			}
		}
		else if(constant->m_RootMotionBoneIndex != -1)
		{
			if(!constant->m_RootMotionSkeleton.IsNull())
			{
				ws->m_RootMotionSkeletonPoseWsA = skeleton::CreateSkeletonPose(constant->m_RootMotionSkeleton.Get(), alloc);		
				ws->m_RootMotionSkeletonPoseWsB = skeleton::CreateSkeletonPose(constant->m_RootMotionSkeleton.Get(), alloc);		
			}
		}

		return ws;
	}

	void DestroyAvatarWorkspace(AvatarWorkspace* workspace, memory::Allocator& alloc)
	{
		if(workspace)
		{
			skeleton::DestroySkeletonPose(workspace->m_BodySkeletonPoseWsB, alloc);
			skeleton::DestroySkeletonPose(workspace->m_BodySkeletonPoseWsA, alloc);
			skeleton::DestroySkeletonPose(workspace->m_BodySkeletonPoseWs, alloc);
			skeleton::DestroySkeletonPose(workspace->m_RootMotionSkeletonPoseWsB, alloc);
			skeleton::DestroySkeletonPose(workspace->m_RootMotionSkeletonPoseWsA, alloc);
			alloc.Deallocate(workspace->m_HumanPoseWs);

			alloc.Deallocate(workspace);
		}
	}

	AvatarOutput* CreateAvatarOutput(AvatarConstant const* constant, bool hasTransformHierarchy, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(AvatarOutput);
		AvatarOutput* out	= alloc.Construct<AvatarOutput>();

		if(hasTransformHierarchy)
		{
			if(!constant->m_Human.IsNull())
			{
				if(!constant->m_AvatarSkeleton.IsNull() && constant->m_AvatarSkeleton->m_Count > 0)
				{
					out-> m_SkeletonPoseOutput = skeleton::CreateSkeletonPose(constant->m_AvatarSkeleton.Get(), alloc);
				}
			}
		}
		else
		{
			if(!constant->m_AvatarSkeleton.IsNull() && constant->m_AvatarSkeleton->m_Count > 0)
			{
				out->m_SkeletonPoseOutput = skeleton::CreateSkeletonPose(constant->m_AvatarSkeleton.Get(), alloc);
			}
		}

		if(constant->m_RootMotionBoneIndex != -1 || !constant->m_Human.IsNull())
		{
			out->m_MotionOutput = alloc.Construct<MotionOutput>();

			if(!constant->m_Human.IsNull())
			{
				out->m_HumanPoseBaseOutput = alloc.Construct<human::HumanPose>();
				out->m_HumanPoseOutput = alloc.Construct<human::HumanPose>();
			}
		}

		return out;
	}

	void DestroyAvatarOutput(AvatarOutput* output, memory::Allocator& alloc)
	{
		if(output)
		{
			if(output->m_DynamicValuesOutput) DestroyValueArray(output->m_DynamicValuesOutput,alloc);
			skeleton::DestroySkeletonPose(output->m_SkeletonPoseOutput, alloc);
			alloc.Deallocate(output->m_MotionOutput);
			alloc.Deallocate(output->m_HumanPoseOutput);
			alloc.Deallocate(output->m_HumanPoseBaseOutput);
			alloc.Deallocate(output);
		}
	}

	void ControllerConstant::InitializeClass()
	{
		RegisterAllowNameConversion("ControllerConstant", "m_HumanLayerCount", "m_LayerCount");
		RegisterAllowNameConversion("ControllerConstant", "m_HumanLayerArray", "m_LayerArray");
	}

	ControllerConstant* CreateControllerConstant(	uint32_t layerCount, LayerConstant** layerArray,
													uint32_t stateMachineCount, statemachine::StateMachineConstant** stateMachineConstantArray,
													ValueArrayConstant* values, ValueArray* defaultValues,
													memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ControllerConstant);

		ControllerConstant* controller = alloc.Construct<ControllerConstant>();

		controller->m_LayerCount = layerCount;
		controller->m_LayerArray = alloc.ConstructArray< OffsetPtr<LayerConstant> >(controller->m_LayerCount);

		uint32_t i;
		for(i=0;i<controller->m_LayerCount;i++)
			controller->m_LayerArray[i] = layerArray[i];
		
		controller->m_StateMachineCount = stateMachineCount;
		controller->m_StateMachineArray	= alloc.ConstructArray< OffsetPtr<statemachine::StateMachineConstant> >(controller->m_StateMachineCount);

		for(i=0;i<controller->m_StateMachineCount;i++)
			controller->m_StateMachineArray[i] = stateMachineConstantArray[i];

		controller->m_Values = values;		
		controller->m_DefaultValues = defaultValues;

		return controller;
	}
	
	void DestroyControllerConstant(ControllerConstant* controller, memory::Allocator& alloc)
	{
		if(controller)
		{
			alloc.Deallocate(controller->m_LayerArray);
			alloc.Deallocate(controller->m_StateMachineArray);
			alloc.Deallocate(controller);
		}
	}
		

	LayerConstant* CreateLayerConstant(mecanim::uint32_t stateMachineIndex, mecanim::uint32_t motionSetIndex, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(LayerConstant);

		LayerConstant* cst = alloc.Construct<LayerConstant>();
		cst->m_StateMachineIndex = stateMachineIndex; 
		cst->m_StateMachineMotionSetIndex = motionSetIndex;
		return cst;
	}

	void DestroyLayerConstant(LayerConstant* constant, memory::Allocator& alloc)
	{
		if(constant)
		{		
			alloc.Deallocate(constant);
		}
	}

	ControllerMemory* CreateControllerMemory(ControllerConstant const* controller, AvatarConstant const *avatar, AnimationSet const *animationSet, const ValueArrayConstant* dynamicValueConstant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ControllerMemory);

		ControllerMemory* mem = alloc.Construct<ControllerMemory>();

		mem->m_LayerCount = controller->m_LayerCount;
		mem->m_StateMachineCount = controller->m_StateMachineCount;
		mem->m_StateMachineMemory = alloc.ConstructArray< OffsetPtr<statemachine::StateMachineMemory> >(mem->m_StateMachineCount);
		mem->m_InteruptedTransitionsBlendingStateArray	= alloc.ConstructArray< BlendingState<false> > (mem->m_LayerCount);
		mem->m_LayerWeights								= alloc.ConstructArray<float>(mem->m_LayerCount);

		mem->m_Values = CreateValueArray( controller->m_Values.Get(), alloc);
		ValueArrayCopy(controller->m_DefaultValues.Get(), mem->m_Values.Get());

		for(int layerIter = 0; layerIter < controller->m_LayerCount; layerIter++) 
		{
			mem->m_LayerWeights[layerIter] = controller->m_LayerArray[layerIter]->m_DefaultWeight;
		
			mem->m_InteruptedTransitionsBlendingStateArray[layerIter].m_DynamicValuesBlending = mecanim::CreateValueArray(dynamicValueConstant, alloc);
			mem->m_InteruptedTransitionsBlendingStateArray[layerIter].m_DynamicValuesBlendingMask = mecanim::CreateValueArrayMask(dynamicValueConstant, alloc);
		
			if(avatar->isHuman())
			{
				mem->m_InteruptedTransitionsBlendingStateArray[layerIter].m_MotionBlending = alloc.Construct<mecanim::animation::MotionOutput>();
				mem->m_InteruptedTransitionsBlendingStateArray[layerIter].m_HumanPoseBlending = alloc.Construct<mecanim::human::HumanPose>();
			}
			else if(avatar->m_RootMotionBoneIndex != -1)
			{
				mem->m_InteruptedTransitionsBlendingStateArray[layerIter].m_MotionBlending = alloc.Construct<mecanim::animation::MotionOutput>();
				mem->m_InteruptedTransitionsBlendingStateArray[layerIter].m_HumanPoseBlending = 0;
			}
			else
			{
				mem->m_InteruptedTransitionsBlendingStateArray[layerIter].m_MotionBlending = 0;
				mem->m_InteruptedTransitionsBlendingStateArray[layerIter].m_HumanPoseBlending = 0;
			}
		}

		for(int stateMachineIter = 0; stateMachineIter < mem->m_StateMachineCount; stateMachineIter++)
		{
			mem->m_StateMachineMemory[stateMachineIter] = statemachine::CreateStateMachineMemory(controller->m_StateMachineArray[stateMachineIter].Get(), alloc);						
		}

		return mem;
	}

	void DestroyControllerMemory(ControllerMemory* controllerMemory, memory::Allocator& alloc)
	{
		if(controllerMemory)
		{
			for(int layerIter = 0; layerIter < controllerMemory->m_LayerCount; layerIter++) 
			{
				if(!controllerMemory->m_InteruptedTransitionsBlendingStateArray[layerIter].m_DynamicValuesBlending.IsNull()) DestroyValueArray(controllerMemory->m_InteruptedTransitionsBlendingStateArray[layerIter].m_DynamicValuesBlending.Get(),alloc);
				if(!controllerMemory->m_InteruptedTransitionsBlendingStateArray[layerIter].m_DynamicValuesBlendingMask.IsNull()) DestroyValueArrayMask(controllerMemory->m_InteruptedTransitionsBlendingStateArray[layerIter].m_DynamicValuesBlendingMask.Get(),alloc);
				if(!controllerMemory->m_InteruptedTransitionsBlendingStateArray[layerIter].m_MotionBlending.IsNull()) alloc.Deallocate(controllerMemory->m_InteruptedTransitionsBlendingStateArray[layerIter].m_MotionBlending.Get());
				if(!controllerMemory->m_InteruptedTransitionsBlendingStateArray[layerIter].m_HumanPoseBlending.IsNull()) alloc.Deallocate(controllerMemory->m_InteruptedTransitionsBlendingStateArray[layerIter].m_HumanPoseBlending.Get());
			}

			for(int smIter = 0; smIter < controllerMemory->m_StateMachineCount; smIter++) 
			{
				statemachine::DestroyStateMachineMemory(controllerMemory->m_StateMachineMemory[smIter].Get(), alloc);								
			}
		
			DestroyValueArray(controllerMemory->m_Values.Get(), alloc);
			alloc.Deallocate(controllerMemory->m_LayerWeights);
			alloc.Deallocate(controllerMemory->m_InteruptedTransitionsBlendingStateArray);
			alloc.Deallocate(controllerMemory->m_StateMachineMemory);

			alloc.Deallocate(controllerMemory);
		}
	}

	BlendingState<true>* CreateBlendingState(uint32_t size, bool createMotionState, bool createHumanPose, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(BlendingState_true);

		BlendingState<true>* blendingState = alloc.Construct< BlendingState<true> >();

		blendingState->m_DynamicValuesBlending = alloc.ConstructArray<ValueArray*>(size);
		blendingState->m_BlendFactor = alloc.ConstructArray<float>(size);

		memset(&blendingState->m_DynamicValuesBlending[0], 0, sizeof(ValueArray*) * size);
		memset(&blendingState->m_BlendFactor[0], 0, sizeof(float) * size);

		if(createMotionState || createHumanPose)
		{
			blendingState->m_MotionBlending = alloc.ConstructArray<MotionOutput*>(size);
			memset(&blendingState->m_MotionBlending[0], 0, sizeof(MotionOutput*) * size);
		}

		if(createHumanPose)
		{
			blendingState->m_HumanPoseBlending = alloc.ConstructArray<human::HumanPose*>(size);
			memset(&blendingState->m_HumanPoseBlending[0], 0, sizeof(human::HumanPose*) * size);
		}

		blendingState->m_Size = size;

		return blendingState;
	}

	void  DestroyBlendingState(BlendingState<true>* blendingState, memory::Allocator& alloc)
	{
		if(blendingState)
		{
			for(uint32_t i=0;i<blendingState->m_Size;++i)
			{
				DestroyValueArray(blendingState->m_DynamicValuesBlending[i],alloc);
				if(blendingState->m_MotionBlending) alloc.Deallocate(blendingState->m_MotionBlending[i]);
				if(blendingState->m_HumanPoseBlending) alloc.Deallocate(blendingState->m_HumanPoseBlending[i]);
			}

			alloc.Deallocate(blendingState->m_DynamicValuesBlending);
			alloc.Deallocate(blendingState->m_MotionBlending);
			alloc.Deallocate(blendingState->m_HumanPoseBlending);
			alloc.Deallocate(blendingState->m_BlendFactor);
			
			alloc.Deallocate(blendingState);
		}
	}

	ControllerWorkspace *CreateControllerWorkspace(ControllerConstant const* controller, AvatarConstant const *avatar, AnimationSet const *animationSet, const ValueArrayConstant* dynamicValueConstant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ControllerWorkspace);
		bool forRootMotion = avatar->m_RootMotionBoneIndex != -1;
		bool forHuman =  avatar->isHuman();

		ControllerWorkspace* ws	= alloc.Construct<ControllerWorkspace>();
		
		ws->m_StateMachineOutput = alloc.ConstructArray<statemachine::StateMachineOutput*>(controller->m_StateMachineCount);
		ws->m_StateMachineWorkspace = alloc.ConstructArray<statemachine::StateMachineWorkspace*>(controller->m_StateMachineCount);
		ws->m_StateMachineCount = controller->m_StateMachineCount;

		uint32_t i;		
		uint32_t maxMotionSet = 0; 
		for(i = 0 ; i < controller->m_StateMachineCount; ++i)
		{
			maxMotionSet = math::maximum<uint32_t>(maxMotionSet, controller->m_StateMachineArray[i]->m_MotionSetCount);			
		}
		ws->m_MotionSetTimingWeightArray = alloc.ConstructArray<float>(maxMotionSet);	
		
		for(int stateMachineIter = 0; stateMachineIter < ws->m_StateMachineCount; stateMachineIter++)
		{					
			ws->m_StateMachineOutput[stateMachineIter]		= statemachine::CreateStateMachineOutput(controller->m_StateMachineArray[stateMachineIter].Get(), animationSet->m_MaxBlendState, alloc);
			ws->m_StateMachineWorkspace[stateMachineIter]	= statemachine::CreateStateMachineWorkspace(controller->m_StateMachineArray[stateMachineIter].Get(), animationSet->m_MaxBlendState, alloc);			
		}

		ws->m_BlendingState = CreateBlendingState(animationSet->m_MaxBlendState, forRootMotion, forHuman, alloc);
	
		for(int blendStateIter = 0; blendStateIter < ws->m_BlendingState->m_Size; blendStateIter++)
		{
			ws->m_BlendingState->m_DynamicValuesBlending[blendStateIter] = mecanim::CreateValueArray(dynamicValueConstant, alloc);
		
			if(forHuman)
			{
				{
					SETPROFILERLABEL(MotionOutput);
					ws->m_BlendingState->m_MotionBlending[blendStateIter] = alloc.Construct<mecanim::animation::MotionOutput>();	
				}
				{
					SETPROFILERLABEL(HumanPose);
					ws->m_BlendingState->m_HumanPoseBlending[blendStateIter] = alloc.Construct<mecanim::human::HumanPose>();	
				}
			}
			else if(forRootMotion)
			{
				ws->m_BlendingState->m_MotionBlending[blendStateIter] = alloc.Construct<mecanim::animation::MotionOutput>();	
			}
		}
				
		ws->m_BlendingStateWs.m_DynamicValuesBlending = mecanim::CreateValueArray(dynamicValueConstant, alloc);

		if (forHuman)
		{
			ws->m_BlendingStateWs.m_MotionBlending = alloc.Construct<animation::MotionOutput>();	
			ws->m_BlendingStateWs.m_HumanPoseBlending = alloc.Construct<human::HumanPose>();	
		}
		else if (forRootMotion)
		{
			ws->m_BlendingStateWs.m_MotionBlending = alloc.Construct<mecanim::animation::MotionOutput>();	
		}

		ws->m_ValueArrayStart = CreateValueArray(dynamicValueConstant, alloc);
		ws->m_ValueArrayStop = CreateValueArray(dynamicValueConstant, alloc);
		ws->m_BlendingClipArray =  alloc.ConstructArray<mecanim::animation::BlendingClip>(controller->m_LayerCount*animationSet->m_MaxBlendState);

		ws->m_ReadMask = CreateValueArrayMask(dynamicValueConstant, alloc);
		ws->m_BlendMask = CreateValueArrayMask(dynamicValueConstant, alloc);
		ws->m_DefaultMask = CreateValueArrayMask(dynamicValueConstant, alloc);

		return ws;
	}
	
	void DestroyControllerWorkspace(ControllerWorkspace* controllerWorkspace, memory::Allocator& alloc)
	{
		if(controllerWorkspace)
		{
			uint32_t i;
			for(i=0;i<controllerWorkspace->m_StateMachineCount;i++)
			{							
				DestroyStateMachineOutput(controllerWorkspace->m_StateMachineOutput[i], alloc);
				DestroyStateMachineWorkspace(controllerWorkspace->m_StateMachineWorkspace[i], alloc);
			}

			DestroyBlendingState(controllerWorkspace->m_BlendingState, alloc);

			DestroyValueArray(controllerWorkspace->m_BlendingStateWs.m_DynamicValuesBlending.Get(), alloc);
			alloc.Deallocate(controllerWorkspace->m_BlendingStateWs.m_MotionBlending);
			alloc.Deallocate(controllerWorkspace->m_BlendingStateWs.m_HumanPoseBlending);

			alloc.Deallocate(controllerWorkspace->m_BlendingClipArray);

			alloc.Deallocate(controllerWorkspace->m_MotionSetTimingWeightArray);

			alloc.Deallocate(controllerWorkspace->m_StateMachineWorkspace);
			alloc.Deallocate(controllerWorkspace->m_StateMachineOutput);			
			DestroyValueArray(controllerWorkspace->m_ValueArrayStart,alloc);
			DestroyValueArray(controllerWorkspace->m_ValueArrayStop,alloc);

			DestroyValueArrayMask(controllerWorkspace->m_ReadMask,alloc);
			DestroyValueArrayMask(controllerWorkspace->m_BlendMask,alloc);
			DestroyValueArrayMask(controllerWorkspace->m_DefaultMask,alloc);

			alloc.Deallocate(controllerWorkspace);
		}
	}

	void UpdateLeafNodeDuration(const ControllerConstant &controllerConstant, const AnimationSet &animationSet, ControllerMemory &controllerMemory)
	{
		for(int layerIter = 0; layerIter < controllerConstant.m_LayerCount; layerIter++)
		{
			mecanim::uint32_t stateMachineIndex = controllerConstant.m_LayerArray[layerIter]->m_StateMachineIndex;  
			mecanim::uint32_t motionSetIndex = controllerConstant.m_LayerArray[layerIter]->m_StateMachineMotionSetIndex;

			const statemachine::StateMachineConstant& stateMachineConstant = *controllerConstant.m_StateMachineArray[stateMachineIndex].Get();

			for(int stateIter = 0; stateIter < stateMachineConstant.m_StateConstantCount; stateIter++)
			{
				statemachine::StateConstant const &stateConstant = *stateMachineConstant.m_StateConstantArray[stateIter].Get();

				for(int leafIter = 0; leafIter < stateConstant.m_LeafInfoArray[motionSetIndex].m_Count ; leafIter++)
				{
					int clipIndex = leafIter + stateConstant.m_LeafInfoArray[motionSetIndex].m_IndexOffset;

					AnimationSet::Clip& setClip = animationSet.m_ClipConstant[layerIter][clipIndex];
					
					statemachine::GetBlendTreeMemory(stateConstant,*controllerMemory.m_StateMachineMemory[stateMachineIndex]->m_StateMemoryArray[stateIter],motionSetIndex)->m_NodeDurationArray[leafIter] = setClip.m_Clip != 0 ?  (setClip.m_Clip->m_StopTime - setClip.m_Clip->m_StartTime) : 0.0f;					
				}	
			}										
		}
	}

	void SetIKOnFeet(bool left,AvatarConstant const &avatar, const AvatarInput &input, AvatarMemory &memory, AvatarWorkspace &workspace, AvatarOutput &output)
	{
		float deltaTime = input.m_DeltaTime;		
		bool stabilizeFeet = input.m_StabilizeFeet;		
				
		math::xform avatarX = memory.m_AvatarX;		

		math::float1 scale(avatar.m_Human->m_Scale);

		math::xform ddx = math::xformInvMulNS(avatarX,workspace.m_AvatarX);
		float dSpeedT = math::length(ddx.t).tofloat() / deltaTime;
		float dSpeedQ = math::length(math::doubleAtan(math::quat2Qtan(ddx.q))).tofloat() /deltaTime;

		int32_t footIndex = left ? human::kLeftFoot : human::kRightFoot;			
		int32_t goalIndex = left ? human::kLeftFootGoal : human::kRightFootGoal;

		output.m_HumanPoseOutput->m_GoalArray[goalIndex].m_WeightT = 1;
		output.m_HumanPoseOutput->m_GoalArray[goalIndex].m_WeightR = 1;

		if(stabilizeFeet && footIndex != -1 && memory.m_FirstEval==0)
		{
			float speedT = left ? workspace.m_LeftFootSpeedT : workspace.m_RightFootSpeedT;
			float speedQ = left ? workspace.m_LeftFootSpeedQ : workspace.m_RightFootSpeedQ;

			speedT += dSpeedT;
			speedQ += dSpeedQ;

			math::xform &footX = left ? memory.m_LeftFootX : memory.m_RightFootX;				

			math::xform footGoalX0 = output.m_HumanPoseOutput->m_GoalArray[goalIndex].m_X;
			math::xform footGoalX = footGoalX0;

			math::xform goalDX = math::xformInvMulNS(footX,footGoalX);

			float goalDTLen = math::length(goalDX.t).tofloat();

			if(goalDTLen > 0)
			{
				float goalSpeedT = goalDTLen / deltaTime;
				speedT = math::cond(speedT > 0.1f, speedT, 0.0f);
				speedT = math::cond(speedT > 1.0f, 2.0f * speedT, speedT);
				float goalSpeedTClamp = math::minimum(goalSpeedT,speedT);
				goalDX.t = goalDX.t * math::float1(goalSpeedTClamp/goalSpeedT);
			}

			float goalDQLen = math::length(math::doubleAtan(math::quat2Qtan(goalDX.q))).tofloat();

			if(goalDQLen > 0)
			{
				float goalSpeedQ = goalDQLen / deltaTime;
				speedQ = math::cond(speedQ > math::radians(10.0f), speedQ, 0.0f);
				speedQ = math::cond(speedQ > math::radians(100.0f), 2.0f * speedQ, speedQ);
				float goalSpeedQClamp = math::minimum(goalSpeedQ,speedQ);
				goalDX.q = math::qtan2Quat(math::halfTan(math::doubleAtan(math::quat2Qtan(goalDX.q))*math::float1(goalSpeedQClamp/goalSpeedQ)));
			}

			footX = math::xformMul(footX,goalDX);

			output.m_HumanPoseOutput->m_GoalArray[goalIndex].m_X = footX;
		}

		math::float4 feetSpacing = math::quatMulVec(output.m_HumanPoseOutput->m_GoalArray[goalIndex].m_X.q, math::float4(0,0,(left?-1:1)*avatar.m_Human->m_Scale*avatar.m_Human->m_FeetSpacing,0));
		feetSpacing.y() = math::float1::zero();

		output.m_HumanPoseOutput->m_GoalArray[goalIndex].m_X.t += avatarX.s * feetSpacing;				
	}	
	
	void EvaluateAvatarSM(	AvatarConstant const* constant, 
							AvatarInput const* input, 
							AvatarOutput * output, 
							AvatarMemory * memory, 
							AvatarWorkspace * workspace,
							ControllerConstant const* controllerConstant)
	{
		if(controllerConstant)
		{
			workspace->m_IKOnFeet = false;

			uint32_t i;

			for(i = 0; i < controllerConstant->m_StateMachineCount; i++)
			{
				statemachine::StateMachineInput stateMachineInput;
				stateMachineInput.m_MotionSetTimingWeightArray = workspace->m_ControllerWorkspace->m_MotionSetTimingWeightArray;

				for(int layerIndex = 0; layerIndex < controllerConstant->m_LayerCount; layerIndex++)
				{									
					const uint32_t stateMachineIndex = controllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;
					const uint32_t motionSetIndex = controllerConstant->m_LayerArray[layerIndex]->m_StateMachineMotionSetIndex;

					if(stateMachineIndex != DISABLED_SYNCED_LAYER_IN_NON_PRO)
					{
						if(i == stateMachineIndex )						
						{
							if(motionSetIndex == 0)
								stateMachineInput.m_GotoStateInfo = &input->m_GotoStateInfos[layerIndex];

							if( controllerConstant->m_LayerArray[layerIndex]->m_SyncedLayerAffectsTiming || motionSetIndex == 0 )
								stateMachineInput.m_MotionSetTimingWeightArray[motionSetIndex] = motionSetIndex == 0 ? 1 : memory->m_ControllerMemory->m_LayerWeights[layerIndex];
							else 
								stateMachineInput.m_MotionSetTimingWeightArray[motionSetIndex] = 0 ;
						}
					}
				}
				

				stateMachineInput.m_DeltaTime = input->m_DeltaTime;
						
				stateMachineInput.m_Values = memory->m_ControllerMemory->m_Values.Get();
				workspace->m_ControllerWorkspace->m_StateMachineWorkspace[i]->m_ValuesConstant = const_cast<mecanim::ValueArrayConstant *>(controllerConstant->m_Values.Get());

				statemachine::EvaluateStateMachine(	controllerConstant->m_StateMachineArray[i].Get(), 
													&stateMachineInput, 
													workspace->m_ControllerWorkspace->m_StateMachineOutput[i],
													memory->m_ControllerMemory->m_StateMachineMemory[i].Get(), 
													workspace->m_ControllerWorkspace->m_StateMachineWorkspace[i]);

				workspace->m_IKOnFeet |= workspace->m_ControllerWorkspace->m_StateMachineOutput[i]->m_Left.m_IKOnFeet;
				workspace->m_IKOnFeet |= workspace->m_ControllerWorkspace->m_StateMachineOutput[i]->m_Right.m_IKOnFeet;
			}
		}
	}

	void AdjustPoseForMotion(ControllerBindingConstant const* controllerBindingConstant, math::xform const &motionX, ValueArray &values, skeleton::SkeletonPose &pose, skeleton::SkeletonPose &poseWs)
	{	
		AvatarConstant const *constant = controllerBindingConstant->m_Avatar;

		int lastIndex = (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1)) ? constant->m_RootMotionSkeleton->m_Count-1 : constant->m_RootMotionBoneIndex ;

		SkeletonPoseFromValue(*constant->m_RootMotionSkeleton.Get(), *constant->m_AvatarSkeletonPose.Get(), values, controllerBindingConstant->m_SkeletonTQSMap, constant->m_RootMotionSkeletonIndexArray.Get(), pose,lastIndex, 0);
	
		skeleton::SkeletonPoseComputeGlobal(constant->m_RootMotionSkeleton.Get(),&pose,&poseWs);
		pose.m_X[0] = motionX;
		
		if(constant->m_RootMotionBoneIndex > 0)
		skeleton::SkeletonPoseComputeGlobal(constant->m_RootMotionSkeleton.Get(),&pose,&poseWs,lastIndex-1,0);

		skeleton::SkeletonPoseComputeLocal(constant->m_RootMotionSkeleton.Get(),&poseWs,&pose,lastIndex,lastIndex);
		pose.m_X[0] = math::xformIdentity();

		ValueFromSkeletonPose(*constant->m_RootMotionSkeleton.Get(), pose, controllerBindingConstant->m_SkeletonTQSMap, constant->m_RootMotionSkeletonIndexArray.Get(), values, lastIndex, 0);
	}

	void ComputeRootMotion(ControllerBindingConstant const* controllerBindingConstant, MotionOutput const &motionOutput, ValueArray &values, AvatarWorkspace *workspace)
	{
		AdjustPoseForMotion(controllerBindingConstant, motionOutput.m_MotionStartX, *workspace->m_ControllerWorkspace->m_ValueArrayStart, *workspace->m_RootMotionSkeletonPoseWsA, *workspace->m_RootMotionSkeletonPoseWsB);
		AdjustPoseForMotion(controllerBindingConstant, motionOutput.m_MotionStopX, *workspace->m_ControllerWorkspace->m_ValueArrayStop, *workspace->m_RootMotionSkeletonPoseWsA, *workspace->m_RootMotionSkeletonPoseWsB);
		AdjustPoseForMotion(controllerBindingConstant, motionOutput.m_MotionX, values, *workspace->m_RootMotionSkeletonPoseWsA, *workspace->m_RootMotionSkeletonPoseWsB);
	}

	void EvaluateBlendNode(	ControllerBindingConstant const* controllerBindingConstant,
							AvatarInput const* input, 
							int32_t layerIndex,
							bool additive,
							bool left,
							uint32_t &clipCount,
							ControllerMemory *memory,
							AvatarWorkspace * workspace,
							AvatarOutput * avatarOutput,
							AnimationSetMemory* animationSetMemory,
							float *blendFactor)
	{
		AvatarConstant const *constant = controllerBindingConstant->m_Avatar;
		ControllerConstant const *controllerConstant = controllerBindingConstant->m_Controller;
		AnimationSet const *animationSet = controllerBindingConstant->m_AnimationSet;		

		bool hasRootMotion = constant->m_RootMotionBoneIndex != -1;
		bool isHuman = constant->isHuman();

		uint32_t stateMachineIndex = controllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;
		uint32_t motionSetIndex = controllerConstant->m_LayerArray[layerIndex]->m_StateMachineMotionSetIndex;
		
		if(stateMachineIndex == DISABLED_SYNCED_LAYER_IN_NON_PRO) 
			return ; 
		
		statemachine::StateMachineOutput& stateMachineOutput = *workspace->m_ControllerWorkspace->m_StateMachineOutput[stateMachineIndex];
		statemachine::BlendNode &blendNode = left ? stateMachineOutput.m_Left : stateMachineOutput.m_Right;

		animation::ClipOutput* clipOutput = animationSetMemory->m_ClipOutput;										
			
		for(int32_t outputIter=0; outputIter < blendNode.m_BlendNodeLayer[motionSetIndex].m_OutputCount; outputIter++)
		{
			uint32_t index = blendNode.m_BlendNodeLayer[motionSetIndex].m_OutputIndexArray[outputIter];

			AnimationSet::Clip& setClip = animationSet->m_ClipConstant[layerIndex][index];
			
			if (setClip.m_Clip)
			{			
				animation::ClipMuscleConstant const& muscleConstant = *setClip.m_Clip;

				if(hasRootMotion || isHuman)
				{	
					ClipMuscleInput muscleIn;
					muscleIn.m_Time = blendNode.m_CurrentTime;
					muscleIn.m_PreviousTime = blendNode.m_PreviousTime;
					muscleIn.m_Mirror			= blendNode.m_BlendNodeLayer[motionSetIndex].m_MirrorArray[outputIter];
					muscleIn.m_CycleOffset		= blendNode.m_BlendNodeLayer[motionSetIndex].m_CycleOffsetArray[outputIter];
					muscleIn.m_Reverse			= blendNode.m_BlendNodeLayer[motionSetIndex].m_ReverseArray[outputIter];

					animation::MotionOutput* motionOutput = workspace->m_ControllerWorkspace->m_BlendingState->m_MotionBlending[clipCount];		

					if(isHuman) EvaluateClipMusclePrevTime(muscleConstant,muscleIn,*motionOutput,*animationSetMemory->m_ClipMemory[layerIndex][index]);
					else if(hasRootMotion) EvaluateClipRootMotionDeltaX(muscleConstant,muscleIn,*motionOutput,*animationSetMemory->m_ClipMemory[layerIndex][index]);
				}

				ValueArray &values = avatarOutput != 0 && clipCount == 0 ? *avatarOutput->m_DynamicValuesOutput : *workspace->m_ControllerWorkspace->m_BlendingState->m_DynamicValuesBlending[clipCount];
				
				ClipInput in;
				float timeInt;
				float normalizedTime = 0;
				in.m_Time = ComputeClipTime(blendNode.m_CurrentTime,muscleConstant.m_StartTime,muscleConstant.m_StopTime,
					muscleConstant.m_CycleOffset+blendNode.m_BlendNodeLayer[motionSetIndex].m_CycleOffsetArray[outputIter],
					muscleConstant.m_LoopTime,
					blendNode.m_BlendNodeLayer[motionSetIndex].m_ReverseArray[outputIter],
					normalizedTime,timeInt);
				
				ValueArrayMask& readMask = *workspace->m_ControllerWorkspace->m_ReadMask;
				ValueArrayMask& blendMask = *workspace->m_ControllerWorkspace->m_BlendMask;
	
				EvaluateClip(muscleConstant.m_Clip.Get(),&in,animationSetMemory->m_ClipMemory[layerIndex][index], clipOutput);
				ValuesFromClip(*controllerBindingConstant->m_DynamicValuesDefault, muscleConstant, *clipOutput, setClip.m_Bindings, animationSet->m_IntegerRemapStride, values, readMask);

				OrValueMask(&blendMask,&readMask);

				if(muscleConstant.m_LoopTime && muscleConstant.m_LoopBlend)
				{
					DeltasFromClip(muscleConstant, setClip.m_Bindings, readMask, *workspace->m_ControllerWorkspace->m_ValueArrayStart, *workspace->m_ControllerWorkspace->m_ValueArrayStop);
				}
				
				if(hasRootMotion || isHuman)
				{	
					ClipMuscleInput muscleIn;

					muscleIn.m_Time = blendNode.m_CurrentTime;
					muscleIn.m_PreviousTime = blendNode.m_PreviousTime;

					animation::MotionOutput* motionOutput = workspace->m_ControllerWorkspace->m_BlendingState->m_MotionBlending[clipCount];

					if(isHuman)
					{
						muscleIn.m_TargetIndex = input->m_TargetIndex;
						muscleIn.m_TargetTime =  input->m_TargetTime;
						muscleIn.m_TargetIndex = muscleIn.m_TargetIndex > int32_t(animation::kTargetReference) ?  muscleIn.m_TargetIndex : int32_t(animation::kTargetReference);
						muscleIn.m_TargetIndex = muscleIn.m_TargetIndex  < int32_t(animation::kTargetRightHand) ?  muscleIn.m_TargetIndex : int32_t(animation::kTargetRightHand);

						muscleIn.m_Mirror = blendNode.m_BlendNodeLayer[motionSetIndex].m_MirrorArray[outputIter];
						muscleIn.m_CycleOffset = blendNode.m_BlendNodeLayer[motionSetIndex].m_CycleOffsetArray[outputIter];
						muscleIn.m_Reverse = blendNode.m_BlendNodeLayer[motionSetIndex].m_ReverseArray[outputIter];

						human::HumanPose *humanPose = workspace->m_ControllerWorkspace->m_BlendingState->m_HumanPoseBlending[clipCount];					
						EvaluateClipMuscle(muscleConstant,muscleIn,clipOutput->m_Values,*motionOutput,*humanPose,*animationSetMemory->m_ClipMemory[layerIndex][index]);

						if(additive)
						{	
							// put goals in same space as GetHumanPose						
							// @todo check with bob why referential spaces are different
							for(int i = 0; i < human::kLastGoal; i++)
								humanPose->m_GoalArray[i].m_X = math::xformInvMul(humanPose->m_RootX,humanPose->m_GoalArray[i].m_X);
							humanPose->m_RootX = math::xformMul(motionOutput->m_MotionX, humanPose->m_RootX);

							GetHumanPose(muscleConstant,muscleConstant.m_ValueArrayDelta.Get(),*workspace->m_HumanPoseWs);														
							human::HumanPoseSub(*humanPose,*humanPose,*workspace->m_HumanPoseWs);
						}
					}
			 		else if(hasRootMotion)
					{
						ComputeRootMotion(controllerBindingConstant,*motionOutput,values,workspace);
					}

					if(animationSet->m_GravityWeightIndex != -1 && readMask.m_FloatValues[animationSet->m_GravityWeightIndex])
					{
						values.ReadData(motionOutput->m_GravityWeight, animationSet->m_GravityWeightIndex);
					}
					else
					{
						motionOutput->m_GravityWeight =  muscleConstant.m_LoopBlendPositionY ? 1 : 0;										
					}
				}

				if(additive)
				{
					ValueArraySub(*workspace->m_ControllerWorkspace->m_ValueArrayStart,values, &readMask);
				}

				if(muscleConstant.m_LoopTime && muscleConstant.m_LoopBlend)
				{
					ValueArrayLoop(*workspace->m_ControllerWorkspace->m_ValueArrayStart, *workspace->m_ControllerWorkspace->m_ValueArrayStop,values,normalizedTime, readMask);
				}

				blendFactor[clipCount] = (left ? ( 1.f - stateMachineOutput.m_BlendFactor) :  stateMachineOutput.m_BlendFactor) * blendNode.m_BlendNodeLayer[motionSetIndex].m_OutputBlendArray[outputIter];

				BlendingClip &blendingClip = workspace->m_ControllerWorkspace->m_BlendingClipArray[workspace->m_ControllerWorkspace->m_BlendingClipCount];
				blendingClip.m_ClipIndex = setClip.m_ClipIndex;
				blendingClip.m_LayerIndex = layerIndex;
				blendingClip.m_Weight = blendFactor[clipCount];
				blendingClip.m_PrevTime = ComputeClipTime(blendNode.m_PreviousTime,muscleConstant.m_StartTime,muscleConstant.m_StopTime,muscleConstant.m_CycleOffset,muscleConstant.m_LoopTime,blendNode.m_BlendNodeLayer[motionSetIndex].m_ReverseArray[outputIter],normalizedTime,timeInt);
				blendingClip.m_Time = in.m_Time;
				blendingClip.m_Reverse = blendNode.m_BlendNodeLayer[motionSetIndex].m_ReverseArray[outputIter];

				workspace->m_ControllerWorkspace->m_BlendingClipCount++;

				clipCount++;
			}			
		}
	}

	bool BlendDynamicStates(ValueArray const &valuesDefault, ValueArrayMask const &valueMask, ValueArray **valuesArray, float *blendFactor, int clipCount, ValueArray &valuesOutput, ValueArray *valuesInterupted, ValueArrayMask *valuesMaskInterupted, AvatarOutput *avatarOutput)
	{
		bool fastCopy = false;

		if(clipCount > 0)
		{
			if(clipCount == 1 && CompareApproximately(blendFactor[0],1))
			{
				if(avatarOutput == 0)
				{
					ValueArrayCopy(valuesArray[0],&valuesOutput, &valueMask);
				}
				else
				{
					fastCopy = true;
				}
			}
			else
		{
			ValueArray *values0 = valuesArray[0];
			valuesArray[0] = avatarOutput ? avatarOutput->m_DynamicValuesOutput : valuesArray[0];
				ValueArrayBlend(&valuesDefault,&valuesOutput, valuesArray, blendFactor, clipCount, &valueMask);
			valuesArray[0] = values0;
		}
		}
		else if(clipCount == 0)
		{
			ValueArrayCopy(&valuesDefault,&valuesOutput, &valueMask);
		}
										
		if(valuesInterupted != 0)
		{
			ValueArrayCopy(&valuesOutput,valuesInterupted, &valueMask);
			CopyValueMask(valuesMaskInterupted,&valueMask);
		}

		return fastCopy;
	}

	void BlendMotionStates(MotionOutput **motionArray, float *blendFactor, int clipCount, MotionOutput *motionOutput, MotionOutput *motionInterupted, bool hasRootMotion, bool isHuman, human::HumanPoseMask const &mask)
	{
		if(clipCount > 0)
		{
			if(clipCount == 1 && CompareApproximately(blendFactor[0],1))
			{
				MotionOutputCopy(motionOutput,motionArray[0], hasRootMotion, isHuman, mask);
			}
			else
		{
			MotionOutputBlend(motionOutput,motionArray,blendFactor,clipCount, hasRootMotion, isHuman, mask);
		}
		}
										
		if(motionInterupted)
		{
			MotionOutputCopy(motionInterupted,motionOutput, hasRootMotion, isHuman, mask);
		}
	}

	void BlendHumanStates(human::HumanPose **poseArray, float *blendFactor, int clipCount, human::HumanPose *poseOut, human::HumanPose *poseInterupted)
	{
		if(clipCount > 0)
		{
			if(clipCount == 1 && CompareApproximately(blendFactor[0],1))
			{
				human::HumanPoseCopy(*poseOut,*poseArray[0]);
			}
			else
		{
			human::HumanPoseBlend(*poseOut,poseArray,blendFactor,clipCount);
		}
		}
										
		if(poseInterupted)
		{
			human::HumanPoseCopy(*poseInterupted,*poseOut);
		}
	}

	int EvaluateOneLayer(		ControllerBindingConstant const* controllerBindingConstant,
								AvatarInput const* input, 
								int32_t layerIndex,
								BlendingState<false> * output, 
								AvatarMemory * memory, 
								AvatarWorkspace * workspace,
								AvatarOutput * avatarOutput,
								AnimationSetMemory* animationSetMemory,
								bool &fastCopy)
	{	
		AvatarConstant const *constant = controllerBindingConstant->m_Avatar;
		ControllerConstant const *controllerConstant = controllerBindingConstant->m_Controller;

		bool hasRootMotion = constant->m_RootMotionBoneIndex != -1;
		bool isHuman = constant->isHuman();
		bool additive = controllerConstant->m_LayerArray[layerIndex]->m_LayerBlendingMode == kLayerBlendingModeAdditive;

		uint32_t stateMachineIndex = controllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;

		statemachine::StateMachineMemory* stateMachineMemory = memory->m_ControllerMemory->m_StateMachineMemory[stateMachineIndex].Get();
		statemachine::StateMachineOutput& stateMachineOutput = *workspace->m_ControllerWorkspace->m_StateMachineOutput[stateMachineIndex];
		
		BlendingState<true>& blendingStates = *workspace->m_ControllerWorkspace->m_BlendingState;
		BlendingState<false>& interruptedBlendingState = memory->m_ControllerMemory->m_InteruptedTransitionsBlendingStateArray[layerIndex];

		uint32_t clipCount = 0;	

		SetValueMask(workspace->m_ControllerWorkspace->m_BlendMask,false);

		if(stateMachineMemory->m_InInterruptedTransition)
		{
			blendingStates.m_BlendFactor[clipCount] = ( 1.f - stateMachineOutput.m_BlendFactor);

			ValueArrayCopy(interruptedBlendingState.m_DynamicValuesBlending.Get(),blendingStates.m_DynamicValuesBlending[clipCount]);
			CopyValueMask(workspace->m_ControllerWorkspace->m_ReadMask,interruptedBlendingState.m_DynamicValuesBlendingMask.Get());
			if(hasRootMotion || isHuman)
			{
				human::HumanPoseMask mask = controllerConstant->m_LayerArray[layerIndex]->m_BodyMask;
				MotionOutputCopy(blendingStates.m_MotionBlending[clipCount], interruptedBlendingState.m_MotionBlending.Get(), hasRootMotion, isHuman, mask);
			}
			if(isHuman) human::HumanPoseCopy(*blendingStates.m_HumanPoseBlending[clipCount],*interruptedBlendingState.m_HumanPoseBlending.Get());

			clipCount++;
		}
		else
		{ 			
			EvaluateBlendNode(controllerBindingConstant,input,layerIndex,additive,true,clipCount, memory->m_ControllerMemory.Get(), workspace, avatarOutput, animationSetMemory, blendingStates.m_BlendFactor);
		}
		
		EvaluateBlendNode(controllerBindingConstant,input,layerIndex,additive,false,clipCount, memory->m_ControllerMemory.Get(), workspace, avatarOutput, animationSetMemory, blendingStates.m_BlendFactor);

		bool isInterruptable = stateMachineMemory->m_InTransition;; // Now with dynamic transition, every transition can be interruptable

		fastCopy = BlendDynamicStates(	*controllerBindingConstant->m_DynamicValuesDefault, 
							*workspace->m_ControllerWorkspace->m_BlendMask, 
							blendingStates.m_DynamicValuesBlending, 
							blendingStates.m_BlendFactor, 
							clipCount, 
							*output->m_DynamicValuesBlending.Get(), 
							isInterruptable ? interruptedBlendingState.m_DynamicValuesBlending.Get() : 0,
							isInterruptable ? interruptedBlendingState.m_DynamicValuesBlendingMask.Get() : 0,
							avatarOutput);

		if(hasRootMotion || isHuman) 
		{	
			human::HumanPoseMask mask = controllerConstant->m_LayerArray[layerIndex]->m_BodyMask;

			BlendMotionStates(blendingStates.m_MotionBlending,blendingStates.m_BlendFactor,clipCount,output->m_MotionBlending.Get(),isInterruptable ? interruptedBlendingState.m_MotionBlending.Get() : 0,hasRootMotion,isHuman,mask);
		}

		assert(clipCount <= blendingStates.m_Size);

		if(isHuman) 
		{
			BlendHumanStates(blendingStates.m_HumanPoseBlending,blendingStates.m_BlendFactor,clipCount,output->m_HumanPoseBlending.Get(),isInterruptable ? interruptedBlendingState.m_HumanPoseBlending.Get() : 0);

			if(input->m_DeltaTime != 0)
			{				
				float deltaTime = input->m_DeltaTime;

				for(int32_t clipIter = 0; clipIter < clipCount; clipIter++)
				{
					math::xform leftFootDX = math::xformInvMul(blendingStates.m_MotionBlending[clipIter]->m_PrevLeftFootX,math::xformMul(blendingStates.m_MotionBlending[clipIter]->m_DX,blendingStates.m_HumanPoseBlending[clipIter]->m_GoalArray[human::kLeftFootGoal].m_X));
					math::xform rightFootDX = math::xformInvMul(blendingStates.m_MotionBlending[clipIter]->m_PrevRightFootX,math::xformMul(blendingStates.m_MotionBlending[clipIter]->m_DX,blendingStates.m_HumanPoseBlending[clipIter]->m_GoalArray[human::kRightFootGoal].m_X));

					workspace->m_LeftFootSpeedT = math::maximum<float>(workspace->m_LeftFootSpeedT,math::length(leftFootDX.t).tofloat()/deltaTime);
					workspace->m_LeftFootSpeedQ = math::maximum<float>(workspace->m_LeftFootSpeedQ,math::length(math::doubleAtan(math::quat2Qtan(leftFootDX.q))).tofloat()/deltaTime);
					workspace->m_RightFootSpeedT = math::maximum<float>(workspace->m_RightFootSpeedT,math::length(rightFootDX.t).tofloat()/deltaTime);
					workspace->m_RightFootSpeedQ = math::maximum<float>(workspace->m_RightFootSpeedQ,math::length(math::doubleAtan(math::quat2Qtan(rightFootDX.q))).tofloat()/deltaTime);
				}				
			}			
		}

		return clipCount;
	}

	void AddMotionLayer(MotionOutput const &motionIn, int index, float weight, bool additive, MotionOutput &motionOut, bool hasRootMotion, bool isHuman, human::HumanPoseMask const &poseMask)
	{
		if(index == 0)
		{
			motionOut = motionIn;
		}
		else if(weight > 0)
		{
			if(additive)
			{
				MotionAddAdditiveLayer(&motionOut,&motionIn,weight,hasRootMotion,isHuman,poseMask);
			}
			else
			{
				MotionAddOverrideLayer(&motionOut,&motionIn,weight,hasRootMotion,isHuman,poseMask);
			}
		}
	}

	void AddHumanLayer(human::Human const &human, human::HumanPose const &poseIn,human::HumanPoseMask const &poseMask,int index, float weight, bool additive,human::HumanPose &pose,human::HumanPose &poseBase)
	{
		if(index == 0)
		{			
			human::HumanPoseCopy(poseBase,poseIn, poseMask);
			human::HumanPoseCopy(pose,poseIn, poseMask);
		}
		else if(weight > 0)
		{
			if(additive)
			{
				mecanim::human::HumanPoseMask mask = poseMask;
				mask.set(mecanim::human::kMaskLeftHand,mask.test(mecanim::human::kMaskLeftHand) && human.m_HasLeftHand);
				mask.set(mecanim::human::kMaskRightHand,mask.test(mecanim::human::kMaskRightHand) && human.m_HasRightHand);

				human::HumanPoseAddAdditiveLayer(pose, poseIn, weight, mask);		

				if(mask.test(human::kMaskRootIndex))
				{
					human::HumanPoseAddAdditiveLayer(poseBase,poseIn,weight, mask);
				}
			}
			else				
			{
				mecanim::human::HumanPoseMask mask = poseMask;
				mask.set(mecanim::human::kMaskLeftHand,mask.test(mecanim::human::kMaskLeftHand) && human.m_HasLeftHand);
				mask.set(mecanim::human::kMaskRightHand,mask.test(mecanim::human::kMaskRightHand) && human.m_HasRightHand);

				human::HumanPoseAddOverrideLayer(pose,poseIn,weight,mask);												

				if(mask.test(human::kMaskRootIndex))
				{
					human::HumanPoseAddOverrideLayer(poseBase,poseIn,weight, mask);
				}
			}
		}				
	}

	void EvaluateAvatarLayers(	ControllerBindingConstant const* controllerBindingConstant, 
								AvatarInput const* input, 
								AvatarOutput * output, 
								AvatarMemory * memory, 
								AvatarWorkspace * workspace,
								AnimationSetMemory* animationSetMemory)
	{		
		AvatarConstant const *constant = controllerBindingConstant->m_Avatar;
		ControllerConstant const *controllerConstant = controllerBindingConstant->m_Controller;
		AnimationSet const *animationSet = controllerBindingConstant->m_AnimationSet;

		bool hasRootMotion = constant->m_RootMotionBoneIndex != -1;
		bool isHuman = constant->isHuman();

		SetValueMask(workspace->m_ControllerWorkspace->m_DefaultMask,true);
		workspace->m_ControllerWorkspace->m_BlendingClipCount = 0;

		if(hasRootMotion || isHuman) MotionOutputClear(output->m_MotionOutput);
		if(isHuman)
		{
			workspace->m_LeftFootSpeedT = 0;
			workspace->m_LeftFootSpeedQ = 0;
			workspace->m_RightFootSpeedT = 0;
			workspace->m_RightFootSpeedQ = 0;
		}

		if(controllerConstant && !memory->m_ControllerMemory->m_StateMachineMemory.IsNull())
		{
			for(int layerIter = 0; layerIter < controllerConstant->m_LayerCount; layerIter++)
			{		
				BlendingState<false> &layerOutput = workspace->m_ControllerWorkspace->m_BlendingStateWs;
			
				const uint32_t stateMachineIndex = controllerConstant->m_LayerArray[layerIter]->m_StateMachineIndex;
				if(stateMachineIndex != DISABLED_SYNCED_LAYER_IN_NON_PRO)
				{
					const uint32_t motionSetIndex = controllerConstant->m_LayerArray[layerIter]->m_StateMachineMotionSetIndex;
					float layerWeight = layerIter == 0 ? 1 : memory->m_ControllerMemory->m_LayerWeights[layerIter] * memory->m_ControllerMemory->m_StateMachineMemory[stateMachineIndex]->m_MotionSetAutoWeightArray[motionSetIndex];
					bool additive = controllerConstant->m_LayerArray[layerIter]->m_LayerBlendingMode == kLayerBlendingModeAdditive;

					bool fastCopy = false;
					int clipCount = EvaluateOneLayer(controllerBindingConstant, input, layerIter, &layerOutput, memory, workspace, layerIter == 0 ? output : 0, animationSetMemory,fastCopy);
		
					AndValueMask(workspace->m_ControllerWorkspace->m_BlendMask, animationSet->m_DynamicValuesMaskArray[layerIter]);

					if (fastCopy)
					{
						CopyValueMask(workspace->m_ControllerWorkspace->m_DefaultMask,workspace->m_ControllerWorkspace->m_BlendMask);
						InvertValueMask(workspace->m_ControllerWorkspace->m_DefaultMask);
					}
					else
					{
						ValueArrayAdd(controllerBindingConstant->m_DynamicValuesDefault, layerOutput.m_DynamicValuesBlending.Get(),workspace->m_ControllerWorkspace->m_BlendMask,layerWeight,additive,output->m_DynamicValuesOutput,workspace->m_ControllerWorkspace->m_DefaultMask);
					}
				
					if(hasRootMotion || isHuman) 
					{	
						if(layerIter == 0 && clipCount == 0)
						{
							MotionOutputClear(output->m_MotionOutput);
						}
						else
						{
							bool rootMotionMask = hasRootMotion && controllerBindingConstant->m_RootMotionLayerMask[layerIter];
							AddMotionLayer(*layerOutput.m_MotionBlending.Get(),layerIter,layerWeight,additive,*output->m_MotionOutput,rootMotionMask,isHuman,controllerConstant->m_LayerArray[layerIter]->m_BodyMask);
						}
					}

					if(isHuman)
					{
						if(layerIter == 0 && clipCount == 0)
						{
							HumanPoseClear(*output->m_HumanPoseOutput);
							HumanPoseClear(*output->m_HumanPoseBaseOutput);
						}
						else
						{
							AddHumanLayer(*constant->m_Human.Get(),*layerOutput.m_HumanPoseBlending.Get(),controllerConstant->m_LayerArray[layerIter]->m_BodyMask,layerIter,layerWeight,additive,*output->m_HumanPoseOutput,*output->m_HumanPoseBaseOutput);
						}
				}
			}	
			}	
		
			ValueArrayCopy(controllerBindingConstant->m_DynamicValuesDefault,output->m_DynamicValuesOutput,workspace->m_ControllerWorkspace->m_DefaultMask);
			ValueArrayCopy(controllerBindingConstant->m_DynamicValuesConstant, output->m_DynamicValuesOutput,controllerConstant->m_Values.Get(),memory->m_ControllerMemory->m_Values.Get(),animationSet->m_AdditionalIndexArray);
		}

		if(isHuman)
		{
			/////////////////////////////////////////////////////////////////
			// Pivot management
			if(memory->m_FirstEval==0)
			{				
				math::float1 pivotWeight(memory->m_PivotWeight);
				math::float4 prevPivot = math::lerp(output->m_MotionOutput->m_PrevLeftFootX.t,output->m_MotionOutput->m_PrevRightFootX.t,pivotWeight);
				math::float4 deltaPivot = memory->m_Pivot-prevPivot;
				deltaPivot.y() = 0;				
				deltaPivot *= input->m_FeetPivotActive;
				output->m_MotionOutput->m_DX.t = math::xformMulVec(output->m_MotionOutput->m_DX,deltaPivot);
			}

			memory->m_PivotWeight = math::saturate(0.5f * (0.5f + 2.0f * workspace->m_LeftFootSpeedT) / (0.5f + workspace->m_LeftFootSpeedT + workspace->m_RightFootSpeedT));			

			math::xform lLeftFootX = output->m_HumanPoseOutput->m_GoalArray[human::kLeftFootGoal].m_X;
			math::xform lRightFootX = output->m_HumanPoseOutput->m_GoalArray[human::kRightFootGoal].m_X;
			memory->m_Pivot = math::lerp(lLeftFootX.t,lRightFootX.t,math::float1(memory->m_PivotWeight));
			/////////////////////////////////////////////////////////////////
			
			math::float1 scale(constant->isHuman() ? constant->m_Human->m_Scale : 1);			
			workspace->m_AvatarX = memory->m_AvatarX;			
			math::xform dx = output->m_MotionOutput->m_DX;
			dx.t *= scale;
			workspace->m_AvatarX = math::xformMul(workspace->m_AvatarX,dx);			
		}
	}

	void EvaluateAvatarX(	AvatarConstant const* constant, 
							AvatarInput const* input, 
							AvatarOutput * output, 
							AvatarMemory * memory, 
							AvatarWorkspace * workspace)
	{
		bool isHuman = constant->isHuman();
		int rootMotionIndex = constant->m_RootMotionBoneIndex;

		if(isHuman || rootMotionIndex != -1)
		{									
			math::xform dx = output->m_MotionOutput->m_DX;
			if(isHuman) dx.t *= math::float1(constant->m_Human->m_Scale);
			memory->m_AvatarX = math::xformMul(memory->m_AvatarX, dx); // @Sonny: Can memory avatarX.s degenerate here even if dx.s == 1?
		}
	}

	void EvaluateAvatarRetarget(	AvatarConstant const* constant, 
									AvatarInput const* input, 
									AvatarOutput * output, 
									AvatarMemory * memory, 
									AvatarWorkspace * workspace,
									ControllerConstant const* controllerConstant)
	{
		if(controllerConstant && constant->isHuman())
		{
			math::xform avatarX = memory->m_AvatarX;
						
			human::HumanPose humanPose;
			human::HumanPose *humanPosePtr = 0;

			if(input->m_LayersAffectMassCenter)
			{
				human::HumanPoseCopy(*output->m_HumanPoseBaseOutput,*output->m_HumanPoseOutput);				
			}
			else if (controllerConstant->m_LayerCount > 1)
			{
				human::HumanPoseCopy(humanPose,*output->m_HumanPoseOutput);
				humanPosePtr = &humanPose;
			}			

			human::RetargetTo(	constant->m_Human.Get(),
								output->m_HumanPoseBaseOutput,
								humanPosePtr,
								avatarX,
								output->m_HumanPoseOutput,
								workspace->m_BodySkeletonPoseWs,
								workspace->m_BodySkeletonPoseWsA);

			if(workspace->m_IKOnFeet)
			{
				SetIKOnFeet(true,*constant, *input, *memory, *workspace, *output);
				SetIKOnFeet(false,*constant, *input, *memory, *workspace, *output);

				/* Usefull to debug IK on arms in avatar preview
				output->m_HumanPose.m_GoalArray[human::kLeftHandGoal].m_WeightT = 1;
				output->m_HumanPose.m_GoalArray[human::kRightHandGoal].m_WeightT = 1;
				output->m_HumanPose.m_GoalArray[human::kLeftHandGoal].m_WeightR = 1;
				output->m_HumanPose.m_GoalArray[human::kRightHandGoal].m_WeightR = 1;
				*/
			}
		}
	}

	void EvaluateAvatarIK(	AvatarConstant const* constant, 
							AvatarInput const* input, 
							AvatarOutput * output, 
							AvatarMemory * memory, 
							AvatarWorkspace * workspace,
							ControllerConstant const* controllerConstant)
	{
		if(controllerConstant && constant->isHuman())
		{
			////////////////////////////////////////////////////////////////////////////////////////////////
			//			
			bool needIK = any(output->m_HumanPoseOutput->m_LookAtWeight > math::float4::zero());
			for(int i = 0; !needIK && i < mecanim::human::kLastGoal; i++) needIK |= (output->m_HumanPoseOutput->m_GoalArray[i].m_WeightT > 0 || output->m_HumanPoseOutput->m_GoalArray[i].m_WeightR > 0); 

			workspace->m_BodySkeletonPoseWs->m_X[0] = output->m_HumanPoseOutput->m_RootX;

			if(needIK)
			{
				skeleton::SkeletonPoseComputeGlobal(constant->m_Human->m_Skeleton.Get(), workspace->m_BodySkeletonPoseWs, workspace->m_BodySkeletonPoseWsA);
				FullBodySolve(constant->m_Human.Get(), output->m_HumanPoseOutput, workspace->m_BodySkeletonPoseWs, workspace->m_BodySkeletonPoseWsA, workspace->m_BodySkeletonPoseWsB);
			}
			
			memory->m_LeftFootX = output->m_HumanPoseOutput->m_GoalArray[human::kLeftFootGoal].m_X;
			memory->m_RightFootX = output->m_HumanPoseOutput->m_GoalArray[human::kRightFootGoal].m_X;			

			output->m_HumanPoseOutput->m_LookAtWeight = math::float4::zero();
			for(int i = 0; i < mecanim::human::kLastGoal; i++) 
			{
				output->m_HumanPoseOutput->m_GoalArray[i].m_WeightT = 0;
				output->m_HumanPoseOutput->m_GoalArray[i].m_WeightR = 0; 
			}
		}
	}

	void EvaluateAvatarEnd(	AvatarConstant const* constant, 
							AvatarInput const* input, 
							AvatarOutput * output, 
							AvatarMemory * memory, 
							AvatarWorkspace * workspace,
							ControllerConstant const* controllerConstant)
	{
		if(constant->isHuman())
		{
			int32_t rootIndex = constant->m_HumanSkeletonIndexArray[0];

			skeleton::SkeletonPoseCopy(workspace->m_BodySkeletonPoseWs,workspace->m_BodySkeletonPoseWsA);			
			TwistSolve(constant->m_Human.Get(), workspace->m_BodySkeletonPoseWsA,workspace->m_BodySkeletonPoseWsB);

			skeleton::SkeletonPoseCopy(constant->m_AvatarSkeletonPose.Get(),output->m_SkeletonPoseOutput);
			output->m_SkeletonPoseOutput->m_X[0] = memory->m_AvatarX;
			skeleton::SkeletonPoseComputeGlobal(constant->m_AvatarSkeleton.Get(),output->m_SkeletonPoseOutput,output->m_SkeletonPoseOutput,rootIndex,0);
			skeleton::SkeletonPoseComputeGlobal(constant->m_Human->m_Skeleton.Get(),workspace->m_BodySkeletonPoseWsA,workspace->m_BodySkeletonPoseWsA,1,1);				
			workspace->m_BodySkeletonPoseWsA->m_X[0] = output->m_SkeletonPoseOutput->m_X[rootIndex];
			skeleton::SkeletonPoseComputeLocal(constant->m_AvatarSkeleton.Get(),output->m_SkeletonPoseOutput,output->m_SkeletonPoseOutput,rootIndex,0);
			skeleton::SkeletonPoseComputeLocal(constant->m_Human->m_Skeleton.Get(),workspace->m_BodySkeletonPoseWsA,workspace->m_BodySkeletonPoseWsA,1,1);
			workspace->m_BodySkeletonPoseWsA->m_X[0] = output->m_SkeletonPoseOutput->m_X[rootIndex];

			skeleton::SkeletonPoseCopy(	workspace->m_BodySkeletonPoseWsA, 
										output->m_SkeletonPoseOutput,
										constant->m_HumanSkeletonIndexCount,
										constant->m_HumanSkeletonIndexArray.Get());
		}
	}

	void ValuesFromClip(	mecanim::ValueArray const &defaultValues,
							mecanim::animation::ClipMuscleConstant const &cst, 
							mecanim::animation::ClipOutput const &clip,
							const ClipBindings& bindings,
							int32_t integerRemapStride,
							mecanim::ValueArray &values,
							mecanim::ValueArrayMask& valueArrayMask)
	{
		float* RESTRICT output;
		bool* RESTRICT mask;
		const float* RESTRICT inputArray = clip.m_Values;

		// Extract position values from curvedata (float[])
		output = reinterpret_cast<float*> (values.m_PositionValues.Get());
		mask = valueArrayMask.m_PositionValues.Get();
		for (int valueIndex=0;valueIndex<values.m_PositionCount;valueIndex++)
		{
			int curveIndex = bindings.m_PositionIndex[valueIndex];
			
			if (curveIndex == -1)
			{
				values.m_PositionValues[valueIndex] = defaultValues.m_PositionValues[valueIndex];
				mask[valueIndex] = false;
			}
			else
			{
				float* outputValue = output + valueIndex * 4;
				outputValue[0] = inputArray[curveIndex+0];
				outputValue[1] = inputArray[curveIndex+1];					
				outputValue[2] = inputArray[curveIndex+2];					
				outputValue[3] = 0.0F;
				mask[valueIndex] = true;
			}
		}

		// Extract quaternion values from curvedata (float[])
		output = reinterpret_cast<float*> (values.m_QuaternionValues.Get());
		mask = valueArrayMask.m_QuaternionValues.Get();
		for (int valueIndex=0;valueIndex<values.m_QuaternionCount;valueIndex++)
		{
			int curveIndex = bindings.m_QuaternionIndex[valueIndex];
			if (curveIndex == -1)
			{
				values.m_QuaternionValues[valueIndex] = defaultValues.m_QuaternionValues[valueIndex];
				mask[valueIndex] = false;
			}
			else
			{
				float* outputValue = output + valueIndex * 4;
				outputValue[0] = inputArray[curveIndex+0];
				outputValue[1] = inputArray[curveIndex+1];					
				outputValue[2] = inputArray[curveIndex+2];					
				outputValue[3] = inputArray[curveIndex+3];
				mask[valueIndex] = true;
			}
		}

		// Extract scale values from curvedata (float[])
		output = reinterpret_cast<float*> (values.m_ScaleValues.Get());
		mask = valueArrayMask.m_ScaleValues.Get();
		for (int valueIndex=0;valueIndex<values.m_ScaleCount;valueIndex++)
		{
			int curveIndex = bindings.m_ScaleIndex[valueIndex];
			
			if (curveIndex == -1)
			{
				values.m_ScaleValues[valueIndex] = defaultValues.m_ScaleValues[valueIndex];
				mask[valueIndex] = false;
			}
			else
			{
				float* outputValue = output + valueIndex * 4;
				outputValue[0] = inputArray[curveIndex+0];
				outputValue[1] = inputArray[curveIndex+1];					
				outputValue[2] = inputArray[curveIndex+2];					
				outputValue[3] = 1.0F;
				mask[valueIndex] = true;
			}
		}

		// Extract float values from curvedata (float[])
		output = reinterpret_cast<float*> (values.m_FloatValues.Get());
		mask = valueArrayMask.m_FloatValues.Get();
		for (int valueIndex=0;valueIndex<values.m_FloatCount;valueIndex++)
		{
			int curveIndex = bindings.m_FloatIndex[valueIndex];
			
			if (curveIndex == -1)
			{
				values.m_FloatValues[valueIndex] = defaultValues.m_FloatValues[valueIndex];
				mask[valueIndex] = false;
			}
			else
			{
				output[valueIndex] = inputArray[curveIndex];
				mask[valueIndex] = true;
			}
		}
		
		// Extract integer values from curvedata (float[])
		// Used for PPtr animation. The integers actually represent an instanceID.
		mask = valueArrayMask.m_IntValues.Get();
		for (int valueIndex=0;valueIndex<values.m_IntCount;valueIndex++)
		{
			int curveIndex = bindings.m_IntIndex[valueIndex];
			
			if (curveIndex == -1)
			{
				values.m_IntValues[valueIndex] = defaultValues.m_IntValues[valueIndex];
				mask[valueIndex] = false;
			}
			else
			{
				uint32_t index = (uint32_t)inputArray[curveIndex];
				const uint8_t* integerRemapBuffer = reinterpret_cast<const uint8_t*> (bindings.m_IntegerRemap);
				int32_t valueInt32 = *reinterpret_cast<const int32_t*> (integerRemapBuffer + (index * integerRemapStride));

				values.m_IntValues[valueIndex] = valueInt32;
				mask[valueIndex] = true;
			}
		}
	}

	void DeltasFromClip(	ClipMuscleConstant const &cst,
							const ClipBindings& bindings,
							const ValueArrayMask& mask,
							mecanim::ValueArray &starts,
							mecanim::ValueArray &stops)
	{

		ATTRIBUTE_ALIGN(ALIGN4F) float start[4];
		ATTRIBUTE_ALIGN(ALIGN4F) float stop[4];

		int curveIter;
		
		// Positions
		for (int valueIndex=0;valueIndex<starts.m_PositionCount;valueIndex++)
		{
			if (!mask.m_PositionValues[valueIndex])
				continue;
			
			curveIter = bindings.m_PositionIndex[valueIndex];
			
			start[0] = cst.m_ValueArrayDelta[curveIter+0].m_Start;
			start[1] = cst.m_ValueArrayDelta[curveIter+1].m_Start;
			start[2] = cst.m_ValueArrayDelta[curveIter+2].m_Start;
			start[3] = 0;
			
			stop[0] = cst.m_ValueArrayDelta[curveIter+0].m_Stop;
			stop[1] = cst.m_ValueArrayDelta[curveIter+1].m_Stop;
			stop[2] = cst.m_ValueArrayDelta[curveIter+2].m_Stop;
			stop[3] = 0;
			
			math::float4 start4 = math::load(start);
			math::float4 stop4 = math::load(stop);
			
			starts.WritePosition(start4,valueIndex);
			stops.WritePosition(stop4,valueIndex);
		}

		// Quaternions
		for (int valueIndex=0;valueIndex<starts.m_QuaternionCount;valueIndex++)
		{
			if (!mask.m_QuaternionValues[valueIndex])
				continue;
			
			curveIter = bindings.m_QuaternionIndex[valueIndex];
			
			start[0] = cst.m_ValueArrayDelta[curveIter+0].m_Start;
			start[1] = cst.m_ValueArrayDelta[curveIter+1].m_Start;
			start[2] = cst.m_ValueArrayDelta[curveIter+2].m_Start;
			start[3] = cst.m_ValueArrayDelta[curveIter+3].m_Start;
			
			stop[0] = cst.m_ValueArrayDelta[curveIter+0].m_Stop;
			stop[1] = cst.m_ValueArrayDelta[curveIter+1].m_Stop;
			stop[2] = cst.m_ValueArrayDelta[curveIter+2].m_Stop;
			stop[3] = cst.m_ValueArrayDelta[curveIter+3].m_Stop;
			
			math::float4 start4 = math::load(start);
			math::float4 stop4 = math::load(stop);
			
			starts.WriteQuaternion(start4,valueIndex);
			stops.WriteQuaternion(stop4,valueIndex);
		}
		
		// Scales
		for (int valueIndex=0;valueIndex<starts.m_ScaleCount;valueIndex++)
		{
			if (!mask.m_ScaleValues[valueIndex])
				continue;
			
			curveIter = bindings.m_ScaleIndex[valueIndex];
			
			start[0] = cst.m_ValueArrayDelta[curveIter+0].m_Start;
			start[1] = cst.m_ValueArrayDelta[curveIter+1].m_Start;
			start[2] = cst.m_ValueArrayDelta[curveIter+2].m_Start;
			start[3] = 0;
			
			stop[0] = cst.m_ValueArrayDelta[curveIter+0].m_Stop;
			stop[1] = cst.m_ValueArrayDelta[curveIter+1].m_Stop;
			stop[2] = cst.m_ValueArrayDelta[curveIter+2].m_Stop;
			stop[3] = 0;
			
			math::float4 start4 = math::load(start);
			math::float4 stop4 = math::load(stop);
			
			starts.WriteScale(start4,valueIndex);
			stops.WriteScale(stop4,valueIndex);
		}

		// Generic floats
		for (int valueIndex=0;valueIndex<starts.m_FloatCount;valueIndex++)
		{
			if (!mask.m_FloatValues[valueIndex])
				continue;
			
			curveIter = bindings.m_FloatIndex[valueIndex];
			
			start[0] = cst.m_ValueArrayDelta[curveIter+0].m_Start;
			stop[0] = cst.m_ValueArrayDelta[curveIter+0].m_Stop;
			
			starts.WriteData(start[0],valueIndex);
			stops.WriteData(stop[0],valueIndex);	
		}
	}

	void SkeletonPoseFromValue(skeleton::Skeleton const &skeleton, skeleton::SkeletonPose const &defaultPose, ValueArray const &values, SkeletonTQSMap const *skeletonTQSMap, skeleton::SkeletonPose &pose, int32_t const*humanReverseIndex, bool skipRoot)
	{
		for (int index=skipRoot?1:0;index<skeleton.m_Count;index++)
		{
			if(!(humanReverseIndex != 0 && humanReverseIndex[index] != -1))
			{
				if (skeletonTQSMap[index].m_TIndex != -1)
					pose.m_X[index].t = values.ReadPosition(skeletonTQSMap[index].m_TIndex);
				else
					pose.m_X[index].t = defaultPose.m_X[index].t;
			
				if (skeletonTQSMap[index].m_QIndex != -1)
					pose.m_X[index].q = values.ReadQuaternion(skeletonTQSMap[index].m_QIndex);
				else
					pose.m_X[index].q = defaultPose.m_X[index].q;
			
				if (skeletonTQSMap[index].m_SIndex != -1)
					pose.m_X[index].s = values.ReadScale(skeletonTQSMap[index].m_SIndex);
				else
					pose.m_X[index].s = defaultPose.m_X[index].s;
			}
		}
	}
	
	void SkeletonPoseFromValue(skeleton::Skeleton const &skeleton, skeleton::SkeletonPose const &defaultPose, ValueArray const &values, SkeletonTQSMap const *skeletonTQSMap, int32_t const *indexArray, skeleton::SkeletonPose &pose,int index, int stopIndex)
	{
		if(index != -1)
		{
			if(index != stopIndex)
			{
				SkeletonPoseFromValue(skeleton,defaultPose, values,skeletonTQSMap,indexArray,pose,skeleton.m_Node[index].m_ParentId,stopIndex);

				int avatarIndex = indexArray[index];

				if(skeletonTQSMap[avatarIndex].m_TIndex != -1)
				{
					pose.m_X[index].t = values.ReadPosition(skeletonTQSMap[avatarIndex].m_TIndex);
				}
				else
				{
					pose.m_X[index].t = defaultPose.m_X[avatarIndex].t;
				}

				if(skeletonTQSMap[avatarIndex].m_QIndex != -1)
				{
					pose.m_X[index].q = values.ReadQuaternion(skeletonTQSMap[avatarIndex].m_QIndex);
				}
				else
				{
					pose.m_X[index].q = defaultPose.m_X[avatarIndex].q;
				}

				if(skeletonTQSMap[avatarIndex].m_SIndex != -1)
				{
					pose.m_X[index].s = values.ReadScale(skeletonTQSMap[avatarIndex].m_SIndex);
				}
				else
				{
					pose.m_X[index].s = defaultPose.m_X[avatarIndex].s;
				}
			}
		}
	}
	
	void ValueFromSkeletonPose(skeleton::Skeleton const &skeleton, skeleton::SkeletonPose const &pose, SkeletonTQSMap const *skeletonTQSMap, ValueArray &values)
	{
		for (int index=0;index<skeleton.m_Count;index++)
		{
			if(skeletonTQSMap[index].m_TIndex != -1)
			{
				values.WritePosition(pose.m_X[index].t, skeletonTQSMap[index].m_TIndex);
			}
			
			if(skeletonTQSMap[index].m_QIndex != -1)
			{
				values.WriteQuaternion(pose.m_X[index].q, skeletonTQSMap[index].m_QIndex);
			}
			
			if(skeletonTQSMap[index].m_SIndex != -1)
			{
				values.WriteScale(pose.m_X[index].s, skeletonTQSMap[index].m_SIndex);
			}
		}
	}

	void ValueFromSkeletonPose(skeleton::Skeleton const &skeleton, skeleton::SkeletonPose const &pose, SkeletonTQSMap const *skeletonTQSMap,int32_t const *indexArray, ValueArray &values, int index, int stopIndex)
	{
		if(index != -1)
		{
			if(index != stopIndex)
			{
				ValueFromSkeletonPose(skeleton,pose,skeletonTQSMap,indexArray, values,skeleton.m_Node[index].m_ParentId,stopIndex);

				int avatarIndex = indexArray[index];

				if(skeletonTQSMap[avatarIndex].m_TIndex != -1)
				{
					values.WritePosition(pose.m_X[index].t, skeletonTQSMap[avatarIndex].m_TIndex);
				}

				if(skeletonTQSMap[avatarIndex].m_QIndex != -1)
				{
					values.WriteQuaternion(pose.m_X[index].q, skeletonTQSMap[avatarIndex].m_QIndex);
				}

				if(skeletonTQSMap[avatarIndex].m_SIndex != -1)
				{
					values.WriteScale(pose.m_X[index].s, skeletonTQSMap[avatarIndex].m_SIndex);
				}
			}
		}
	}
}
}
