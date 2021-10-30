#include "UnityPrefix.h"


#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"

#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/mecanim/generic/stringtable.h"

#include "Runtime/mecanim/statemachine/statemachine.h"

#include "Runtime/Math/Simd/math.h"
 
namespace mecanim
{
namespace statemachine
{	

	int32_t GetStateIndex(StateMachineConstant const* apStateMachineConstant, uint32_t id)
	{
		for(uint32_t i=0;i<apStateMachineConstant->m_StateConstantCount;++i)
		{
			if (CompareStateID (apStateMachineConstant->m_StateConstantArray[i].Get(), id))
				return i;
		}
		return -1;
	}



	TransitionConstant const* GetTransitionConstant(StateMachineConstant const* apStateMachineConstant, StateConstant const* apStateConstant, uint32_t id)
	{
		if( id >= s_DynamicTransitionEncodeKey)
			return 0;
		else if( id >= s_AnyTransitionEncodeKey)
			return apStateMachineConstant->m_AnyStateTransitionConstantArray[id-s_AnyTransitionEncodeKey].Get();
		else
			return apStateConstant->m_TransitionConstantArray[id].Get();		 
	}


	bool IsCurrentTransitionAtomic(StateMachineConstant const* apStateMachineConstant, StateMachineMemory *apStateMachineMemory)
	{
		TransitionConstant const* transition = GetTransitionConstant(apStateMachineConstant, apStateMachineConstant->m_StateConstantArray[apStateMachineMemory->m_CurrentStateIndex].Get(), apStateMachineMemory->m_TransitionId);
		return transition->m_Atomic;		
	}

	TransitionWorkspace* GetTransitionWorkspace(StateMachineWorkspace const* apStateMachineWorkspace, StateWorkspace const* apStateWorkspace, uint32_t id)
	{
		if( id >  s_AnyTransitionEncodeKey)
			return apStateMachineWorkspace->m_AnyStateTransitionWorkspaceArray[id-s_AnyTransitionEncodeKey];
		
		else
			return apStateWorkspace->m_TransitionWorkspaceArray[id];		 
	}

	TransitionConstant *CreateTransitionConstant(ConditionConstant** apConditionsConstantArray, uint32_t aConditionConstantCount, 
		uint32_t aDestinationState,float aTransitionDuration, float aTransitionOffset, bool aAtomic, uint32_t aID, uint32_t aUserID,  memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(TransitionConstant);

		TransitionConstant * transitionConstant	= arAlloc.Construct<TransitionConstant >(); 		
		transitionConstant->m_ConditionConstantArray = arAlloc.ConstructArray< OffsetPtr<ConditionConstant> >(aConditionConstantCount);
		transitionConstant->m_ConditionConstantCount = aConditionConstantCount;									

		transitionConstant->m_TransitionDuration = aTransitionDuration;		
		transitionConstant->m_TransitionOffset = aTransitionOffset;						
		transitionConstant->m_Atomic = aAtomic;
		transitionConstant->m_ID = aID;
		transitionConstant->m_UserID= aUserID;


		transitionConstant->m_DestinationState = aDestinationState;	


		uint32_t i;
		for(i=0;i<aConditionConstantCount;i++)
			transitionConstant->m_ConditionConstantArray[i] = apConditionsConstantArray[i];

		return transitionConstant;
	}

	void DestroyTransitionConstant(TransitionConstant *apTransitionConstant, memory::Allocator& arAlloc)
	{
		if(apTransitionConstant)
		{		
			arAlloc.Deallocate(apTransitionConstant->m_ConditionConstantArray);
			arAlloc.Deallocate(apTransitionConstant);
		}
	}
		

	TransitionWorkspace* CreateTransitionWorkspace(TransitionConstant const* apTransitionConstant, memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(TransitionWorkspace);
		TransitionWorkspace* transitionWorkspace = arAlloc.Construct<TransitionWorkspace>(); 		
		transitionWorkspace->m_ConditionConstantCount = apTransitionConstant->m_ConditionConstantCount;				

		return transitionWorkspace;
	}

	void DestroyTransitionWorkspace(TransitionWorkspace* apTransitionWorkspace, memory::Allocator& arAlloc)
	{
		if(apTransitionWorkspace)
		{						
			arAlloc.Deallocate(apTransitionWorkspace);
		}
	}


	ConditionConstant *CreateConditionConstant( uint32_t aConditionMode, uint32_t aEventID, float aEventThreshold, float aExitTime, memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(ConditionConstant);
		ConditionConstant* conditionConstant				= arAlloc.Construct<ConditionConstant>(); 				
	
		conditionConstant->m_ConditionMode = aConditionMode;
		
		if(aConditionMode == kConditionModeIf || aConditionMode == kConditionModeIfNot || aConditionMode == kConditionModeGreater || aConditionMode == kConditionModeLess || aConditionMode == kConditionModeEquals || aConditionMode == kConditionModeNotEqual)
		{
			conditionConstant->m_EventID = aEventID;
			if(aConditionMode == kConditionModeGreater || aConditionMode == kConditionModeLess || aConditionMode == kConditionModeEquals || aConditionMode == kConditionModeNotEqual)
			{
				conditionConstant->m_EventThreshold = aEventThreshold;
			}
		}		
		else if(aConditionMode == kConditionModeExitTime)
		{
			conditionConstant->m_ExitTime = aExitTime;
		}				

		return conditionConstant;
	}
	void DestroyConditionConstant(ConditionConstant *apConditionConstant, memory::Allocator& arAlloc)
	{
		if(apConditionConstant)
		{			
			arAlloc.Deallocate(apConditionConstant);
		}
	}

	static int GetBlendTreeIndex(const StateConstant& arStateConstant, mecanim::int32_t  aMotionSetIndex)
	{
		return arStateConstant.m_BlendTreeConstantIndexArray[aMotionSetIndex];
	}

	animation::BlendTreeConstant const* GetBlendTreeConstant(const StateConstant& arStateConstant, mecanim::int32_t  aMotionSetIndex)
	{
		int blendTreeIndex = GetBlendTreeIndex(arStateConstant,aMotionSetIndex);		
		return blendTreeIndex != -1 ? arStateConstant.m_BlendTreeConstantArray[blendTreeIndex].Get() : 0;
	}
	
	animation::BlendTreeMemory *GetBlendTreeMemory(const StateConstant& arStateConstant,StateMemory& arStateMemory, mecanim::int32_t  aMotionSetIndex)
	{
		int blendTreeIndex = GetBlendTreeIndex(arStateConstant,aMotionSetIndex);		
		return blendTreeIndex != -1 ? arStateMemory.m_BlendTreeMemoryArray[blendTreeIndex].Get() : 0;
	}

	StateConstant* CreateStateConstant(TransitionConstant** apTransitionConstantArray, uint32_t aTransitionConstantCount, 																							
										float aSpeed, bool aIKOnFeet, bool aMirror, float aCycleOffset, animation::BlendTreeConstant** apBlendTreeConstantArray,
										uint32_t aMotionSetCount, uint32_t nameID, uint32_t pathID, uint32_t aTagID, bool aLoop, memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(StateConstant);
		StateConstant* stateConstant				= arAlloc.Construct<StateConstant>(); 

		stateConstant->m_TransitionConstantCount	= aTransitionConstantCount;	
		stateConstant->m_Speed						= aSpeed;
		stateConstant->m_IKOnFeet					= aIKOnFeet;		
		stateConstant->m_Mirror						= aMirror;
		stateConstant->m_CycleOffset				= aCycleOffset;
		stateConstant->m_PathID						= pathID;
		stateConstant->m_NameID						= nameID;
		stateConstant->m_TagID						= aTagID;
		stateConstant->m_MotionSetCount				= aMotionSetCount;
		stateConstant->m_BlendTreeCount				= 0 ;
		stateConstant->m_Loop						= aLoop;

		stateConstant->m_BlendTreeConstantIndexArray	= arAlloc.ConstructArray<int32_t>(aMotionSetCount);
		stateConstant->m_LeafInfoArray					= arAlloc.ConstructArray<LeafInfoConstant>(aMotionSetCount);		

		stateConstant->m_TransitionConstantArray	= arAlloc.ConstructArray< OffsetPtr<TransitionConstant> >(aTransitionConstantCount);				
						
		uint32_t i;
		for(i = 0 ; i < aTransitionConstantCount; i++) 
			stateConstant->m_TransitionConstantArray[i] = apTransitionConstantArray[i];

		for(i = 0 ; i < aMotionSetCount; i++) 
		{
			if(apBlendTreeConstantArray[i] != 0)
			{
				stateConstant->m_BlendTreeConstantIndexArray[i] = stateConstant->m_BlendTreeCount ;
				stateConstant->m_LeafInfoArray[i].m_Count = animation::GetLeafCount(*apBlendTreeConstantArray[i]);	
				stateConstant->m_LeafInfoArray[i].m_IDArray = arAlloc.ConstructArray<uint32_t>(stateConstant->m_LeafInfoArray[i].m_Count);					
				animation::FillLeafIDArray(*apBlendTreeConstantArray[i], stateConstant->m_LeafInfoArray[i].m_IDArray.Get());
				stateConstant->m_BlendTreeCount++;
			}
			else
			{
				stateConstant->m_BlendTreeConstantIndexArray[i] = -1;
				stateConstant->m_LeafInfoArray[i].m_Count = 0;
				stateConstant->m_LeafInfoArray[i].m_IDArray = 0 ;
			}
			
		}
									
		stateConstant->m_BlendTreeConstantArray	= arAlloc.ConstructArray< OffsetPtr<animation::BlendTreeConstant> >(stateConstant->m_BlendTreeCount);
		uint32_t currentTreeCount = 0;		
		for(i = 0 ; i < aMotionSetCount ; i++)
		{
			if(apBlendTreeConstantArray[i] != 0)
			{
				stateConstant->m_BlendTreeConstantArray[currentTreeCount] = apBlendTreeConstantArray[i];
				currentTreeCount++;				
			}						
		}				
												
		return stateConstant;
	}

	void DestroyStateConstant(StateConstant* apStateConstant, memory::Allocator& arAlloc)
	{
		if(apStateConstant)
		{
			for(uint32_t i = 0 ; i < apStateConstant->m_MotionSetCount; i++)
			{
				arAlloc.Deallocate(apStateConstant->m_LeafInfoArray[i].m_IDArray);
			}
			arAlloc.Deallocate(apStateConstant->m_LeafInfoArray);	
			arAlloc.Deallocate(apStateConstant->m_BlendTreeConstantArray);
			arAlloc.Deallocate(apStateConstant->m_BlendTreeConstantIndexArray);
												
			arAlloc.Deallocate(apStateConstant->m_TransitionConstantArray);
			arAlloc.Deallocate(apStateConstant);
		}
	}			

	StateMemory* CreateStateMemory(StateConstant const* apStateConstant, StateMachineConstant const* apParentStateMachineConstant, memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(StateMemory);
		StateMemory *stateMemory = arAlloc.Construct<StateMemory>(); 	

		stateMemory->m_BlendTreeCount = apStateConstant->m_BlendTreeCount;
		stateMemory->m_BlendTreeMemoryArray	= arAlloc.ConstructArray< OffsetPtr<animation::BlendTreeMemory> >(stateMemory->m_BlendTreeCount);
		
		for(int blendTreeIter = 0 ; blendTreeIter < stateMemory->m_BlendTreeCount; blendTreeIter++)
		{
			stateMemory->m_BlendTreeMemoryArray[blendTreeIter] = animation::CreateBlendTreeMemory(apStateConstant->m_BlendTreeConstantArray[blendTreeIter].Get(), arAlloc);
		}				

		return stateMemory;
	}

	void DestroyStateMemory(StateMemory* apStateMemory, memory::Allocator& arAlloc)
	{
		if(apStateMemory)
		{					
			for(int i = 0 ; i < apStateMemory->m_BlendTreeCount; i++)
			{
				arAlloc.Deallocate(apStateMemory->m_BlendTreeMemoryArray[i]);
			}				

			arAlloc.Deallocate(apStateMemory->m_BlendTreeMemoryArray);		
			arAlloc.Deallocate(apStateMemory);		
		}
	}

	StateWorkspace* CreateStateWorkspace(StateConstant const* apStateConstant, uint32_t maxBlendedClip, memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(StateWorkspace);
		StateWorkspace* stateWorkspace = arAlloc.Construct<StateWorkspace>();	

		stateWorkspace->m_TransitionWorkspaceCount	= apStateConstant->m_TransitionConstantCount;
		stateWorkspace->m_TransitionWorkspaceArray	= arAlloc.ConstructArray<TransitionWorkspace*>(stateWorkspace->m_TransitionWorkspaceCount);
		stateWorkspace->m_BlendTreeInputArray		= arAlloc.ConstructArray<animation::BlendTreeInput*>(apStateConstant->m_MotionSetCount);
		stateWorkspace->m_BlendTreeOutputArray		= arAlloc.ConstructArray<animation::BlendTreeOutput*>(apStateConstant->m_MotionSetCount);
		stateWorkspace->m_BlendTreeWorkspaceArray	= arAlloc.ConstructArray<animation::BlendTreeWorkspace*>(apStateConstant->m_MotionSetCount);
		stateWorkspace->m_MotionSetCount			= apStateConstant->m_MotionSetCount;		
		
		for(uint32_t i = 0 ; i < stateWorkspace->m_TransitionWorkspaceCount ; i++)
		{				
			stateWorkspace->m_TransitionWorkspaceArray[i] = CreateTransitionWorkspace(apStateConstant->m_TransitionConstantArray[i].Get(), arAlloc);
		}

		for(uint32_t i = 0 ;  i < stateWorkspace->m_MotionSetCount ; i++)
		{
			animation::BlendTreeConstant const *blendTreeConstant  = statemachine::GetBlendTreeConstant(*apStateConstant,i);
			if(blendTreeConstant != 0)
			{
				stateWorkspace->m_BlendTreeInputArray[i]		=  animation::CreateBlendTreeInput(blendTreeConstant, arAlloc);
				stateWorkspace->m_BlendTreeOutputArray[i]		=  animation::CreateBlendTreeOutput(blendTreeConstant, maxBlendedClip, arAlloc);			
				stateWorkspace->m_BlendTreeWorkspaceArray[i]	=  animation::CreateBlendTreeWorkspace(blendTreeConstant,arAlloc);
			}
			else
			{
				stateWorkspace->m_BlendTreeInputArray[i] = 0;
				stateWorkspace->m_BlendTreeOutputArray[i] = 0;
				stateWorkspace->m_BlendTreeWorkspaceArray[i] = 0;
			}
		}		

		return stateWorkspace;
	}

	void DestroyStateWorkspace(StateWorkspace* apStateWorkspace, memory::Allocator& arAlloc)
	{
		if(apStateWorkspace)
		{			

			for(uint32_t i = 0 ; i <  apStateWorkspace->m_TransitionWorkspaceCount; i++)
			{
				DestroyTransitionWorkspace(apStateWorkspace->m_TransitionWorkspaceArray[i], arAlloc);				
			}			
			for(uint32_t i = 0 ; i < apStateWorkspace->m_MotionSetCount ; i++)
			{
				animation::DestroyBlendTreeInput(apStateWorkspace->m_BlendTreeInputArray[i], arAlloc);
				animation::DestroyBlendTreeOutput(apStateWorkspace->m_BlendTreeOutputArray[i], arAlloc);				
				animation::DestroyBlendTreeWorkspace(apStateWorkspace->m_BlendTreeWorkspaceArray[i], arAlloc);				
			}
						
			arAlloc.Deallocate(apStateWorkspace->m_BlendTreeInputArray);
			arAlloc.Deallocate(apStateWorkspace->m_BlendTreeOutputArray);
			arAlloc.Deallocate(apStateWorkspace->m_BlendTreeWorkspaceArray);
			
			arAlloc.Deallocate(apStateWorkspace->m_TransitionWorkspaceArray);					
			arAlloc.Deallocate(apStateWorkspace);			
		}		
	}


	StateOutput* CreateStateOutput(StateConstant const* apStateConstant, memory::Allocator& arAlloc)
	{		
		SETPROFILERLABEL(StateOutput);
		return arAlloc.Construct<StateOutput>();
	}

	void DestroyStateOutput(StateOutput* apStateOutput, memory::Allocator& arAlloc)
	{
		if(apStateOutput)
		{
			arAlloc.Deallocate(apStateOutput);
		}
	}

	StateMachineConstant* CreateStateMachineConstant(StateConstant** apStateConstantArray, uint32_t aStateConstantCount, uint32_t aDefaultState, 	
		TransitionConstant** apAnyStateTransitionConstantArray, uint32_t aAnyStateTransitionConstantCount, uint32_t aMotionSetCount, 		
		memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(StateMachineConstant);
		StateMachineConstant* stateMachineConstant					= arAlloc.Construct<StateMachineConstant>();
		stateMachineConstant->m_StateConstantArray					= arAlloc.ConstructArray< OffsetPtr<StateConstant> >(aStateConstantCount);					
		stateMachineConstant->m_AnyStateTransitionConstantArray		= arAlloc.ConstructArray< OffsetPtr<TransitionConstant> >(aAnyStateTransitionConstantCount);		

		stateMachineConstant->m_StateConstantCount					= aStateConstantCount;		
		stateMachineConstant->m_DefaultState						= aDefaultState;
		stateMachineConstant->m_AnyStateTransitionConstantCount		= aAnyStateTransitionConstantCount;
		stateMachineConstant->m_MotionSetCount						= aMotionSetCount;
						
		uint32_t i;
		for(i=0;i<aStateConstantCount;i++)
			stateMachineConstant->m_StateConstantArray[i] = apStateConstantArray[i];

		for(i=0;i<aAnyStateTransitionConstantCount;i++)
			stateMachineConstant->m_AnyStateTransitionConstantArray[i] = apAnyStateTransitionConstantArray[i];

		/////////////////////////////////////////////////////////
		// 
		uint32_t j;		
		for(j = 0 ; j < stateMachineConstant->m_MotionSetCount; j++)
		{		
			uint32_t clipOffset = 0 ; 
			for( i = 0 ; i < aStateConstantCount; i++)
			{
				stateMachineConstant->m_StateConstantArray[i]->m_LeafInfoArray[j].m_IndexOffset = clipOffset;				
				clipOffset += stateMachineConstant->m_StateConstantArray[i]->m_LeafInfoArray[j].m_Count;
			}
		}

		return stateMachineConstant;
	}

	void DestroyStateMachineConstant(StateMachineConstant* apStateMachineConstant, memory::Allocator& arAlloc)
	{
		if(apStateMachineConstant)
		{						
			arAlloc.Deallocate(apStateMachineConstant->m_AnyStateTransitionConstantArray);
			arAlloc.Deallocate(apStateMachineConstant->m_StateConstantArray);				

			arAlloc.Deallocate(apStateMachineConstant);
		}
	}			
	
	StateMachineInput* CreateStateMachineInput(StateMachineConstant const* apStateMachineConstant, memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(StateMachineInput);
		StateMachineInput* stateMachineInput		= arAlloc.Construct<StateMachineInput>();						
		return stateMachineInput;
	}

	void DestroyStateMachineInput(StateMachineInput* apStateMachineInput, memory::Allocator& arAlloc)
	{
		if(apStateMachineInput)
		{						
			arAlloc.Deallocate(apStateMachineInput);
		}
	}

	StateMachineMemory* CreateStateMachineMemory(StateMachineConstant const* apStateMachineConstant, memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(StateMachineMemory);
		StateMachineMemory* stateMachineMemory = arAlloc.Construct<StateMachineMemory>();		
		stateMachineMemory->m_CurrentStateIndex = apStateMachineConstant->m_DefaultState;
		stateMachineMemory->m_StateMemoryCount = apStateMachineConstant->m_StateConstantCount;
		stateMachineMemory->m_StateMemoryArray = arAlloc.ConstructArray< OffsetPtr<StateMemory> >(stateMachineMemory->m_StateMemoryCount);
		
		stateMachineMemory->m_MotionSetAutoWeightArray = arAlloc.ConstructArray<float>(apStateMachineConstant->m_MotionSetCount);

		stateMachineMemory->m_MotionSetCount = apStateMachineConstant->m_MotionSetCount;
		
		for( uint32_t i = 0 ; i < stateMachineMemory->m_StateMemoryCount; i++)
		{						
			stateMachineMemory->m_StateMemoryArray[i] = CreateStateMemory(apStateMachineConstant->m_StateConstantArray[i].Get(), apStateMachineConstant, arAlloc);

			if(stateMachineMemory->m_StateMemoryArray[i].IsNull())
			{
				DestroyStateMachineMemory(stateMachineMemory, arAlloc);
				return 0;
			}
		}			
		return stateMachineMemory;
	}

	void DestroyStateMachineMemory(StateMachineMemory* apStateMachineMemory, memory::Allocator& arAlloc)
	{
		if(apStateMachineMemory)
		{			
			for(uint32_t i = 0 ; i < apStateMachineMemory->m_StateMemoryCount ; i++)
			{			
				DestroyStateMemory(apStateMachineMemory->m_StateMemoryArray[i].Get(), arAlloc);
			}
			
			arAlloc.Deallocate(apStateMachineMemory->m_MotionSetAutoWeightArray);
			arAlloc.Deallocate(apStateMachineMemory->m_StateMemoryArray);
			arAlloc.Deallocate(apStateMachineMemory);
		}
	}


	StateMachineWorkspace* CreateStateMachineWorkspace(StateMachineConstant const* apStateMachineConstant, uint32_t maxBlendState, memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(StateMachineWorkspace);
		StateMachineWorkspace* stateMachineWorkspace = arAlloc.Construct<StateMachineWorkspace>();

		stateMachineWorkspace->m_StateWorkspaceCount = apStateMachineConstant->m_StateConstantCount;
		stateMachineWorkspace->m_StateWorkspaceArray = arAlloc.ConstructArray<StateWorkspace*>(stateMachineWorkspace->m_StateWorkspaceCount);
		memset(&stateMachineWorkspace->m_StateWorkspaceArray[0], 0, sizeof(StateWorkspace*)*stateMachineWorkspace->m_StateWorkspaceCount);
		
		for( uint32_t i = 0 ; i < stateMachineWorkspace->m_StateWorkspaceCount; i++)
		{						
			stateMachineWorkspace->m_StateWorkspaceArray[i] = CreateStateWorkspace(apStateMachineConstant->m_StateConstantArray[i].Get(), maxBlendState, arAlloc);
			if(stateMachineWorkspace->m_StateWorkspaceArray[i] == 0)
			{
				DestroyStateMachineWorkspace(stateMachineWorkspace, arAlloc);
				return 0;
			}
		}			


		stateMachineWorkspace->m_AnyStateTransitionWorkspaceCount = apStateMachineConstant->m_AnyStateTransitionConstantCount;
		stateMachineWorkspace->m_AnyStateTransitionWorkspaceArray = arAlloc.ConstructArray<TransitionWorkspace*>(stateMachineWorkspace->m_AnyStateTransitionWorkspaceCount);
		for( uint32_t i = 0 ; i < stateMachineWorkspace->m_AnyStateTransitionWorkspaceCount; i++)
		{
			stateMachineWorkspace->m_AnyStateTransitionWorkspaceArray[i] = CreateTransitionWorkspace(apStateMachineConstant->m_AnyStateTransitionConstantArray[i].Get(), arAlloc);
		}

		return stateMachineWorkspace;

	}

	void DestroyStateMachineWorkspace(StateMachineWorkspace* apStateMachineWorkspace, memory::Allocator& arAlloc)
	{
		if(apStateMachineWorkspace)
		{
			for(uint32_t i = 0 ; i < apStateMachineWorkspace->m_StateWorkspaceCount; i++)
			{			
				DestroyStateWorkspace(apStateMachineWorkspace->m_StateWorkspaceArray[i], arAlloc);
			}

			for(uint32_t i = 0 ; i < apStateMachineWorkspace->m_AnyStateTransitionWorkspaceCount ; i++)
			{
				DestroyTransitionWorkspace(apStateMachineWorkspace->m_AnyStateTransitionWorkspaceArray[i], arAlloc);
			}
			
			arAlloc.Deallocate(apStateMachineWorkspace->m_StateWorkspaceArray);
			arAlloc.Deallocate(apStateMachineWorkspace->m_AnyStateTransitionWorkspaceArray);
			arAlloc.Deallocate(apStateMachineWorkspace);
		}
	}

	StateMachineOutput* CreateStateMachineOutput(StateMachineConstant const* apStateMachineConstant, uint32_t maxBlendedClip, memory::Allocator& arAlloc)
	{
		SETPROFILERLABEL(StateMachineOutput);
		StateMachineOutput* stateMachineOutput = arAlloc.Construct<StateMachineOutput>(); 
		stateMachineOutput->m_MotionSetCount = apStateMachineConstant->m_MotionSetCount;
		stateMachineOutput->m_Left.m_BlendNodeLayer  = arAlloc.ConstructArray<BlendNodeLayer>(apStateMachineConstant->m_MotionSetCount);
		stateMachineOutput->m_Right.m_BlendNodeLayer = arAlloc.ConstructArray<BlendNodeLayer>(apStateMachineConstant->m_MotionSetCount);

		for(int i=0;i<apStateMachineConstant->m_MotionSetCount;i++)
		{
			stateMachineOutput->m_Left.m_BlendNodeLayer[i].m_OutputBlendArray = arAlloc.ConstructArray<float>(maxBlendedClip);
			stateMachineOutput->m_Left.m_BlendNodeLayer[i].m_OutputIndexArray = arAlloc.ConstructArray<uint32_t>(maxBlendedClip);
			stateMachineOutput->m_Left.m_BlendNodeLayer[i].m_ReverseArray = arAlloc.ConstructArray<bool>(maxBlendedClip);
			stateMachineOutput->m_Left.m_BlendNodeLayer[i].m_MirrorArray = arAlloc.ConstructArray<bool>(maxBlendedClip);
			stateMachineOutput->m_Left.m_BlendNodeLayer[i].m_CycleOffsetArray = arAlloc.ConstructArray<float>(maxBlendedClip);

			stateMachineOutput->m_Right.m_BlendNodeLayer[i].m_OutputBlendArray = arAlloc.ConstructArray<float>(maxBlendedClip);
			stateMachineOutput->m_Right.m_BlendNodeLayer[i].m_OutputIndexArray = arAlloc.ConstructArray<uint32_t>(maxBlendedClip);
			stateMachineOutput->m_Right.m_BlendNodeLayer[i].m_ReverseArray = arAlloc.ConstructArray<bool>(maxBlendedClip);
			stateMachineOutput->m_Right.m_BlendNodeLayer[i].m_MirrorArray = arAlloc.ConstructArray<bool>(maxBlendedClip);
			stateMachineOutput->m_Right.m_BlendNodeLayer[i].m_CycleOffsetArray = arAlloc.ConstructArray<float>(maxBlendedClip);
		}

		return stateMachineOutput;
	}

	void DestroyStateMachineOutput(StateMachineOutput* apStateMachineOutput, memory::Allocator& arAlloc)		
	{
		if(apStateMachineOutput)
		{
			for(int i=0;i<apStateMachineOutput->m_MotionSetCount;i++)
			{
				arAlloc.Deallocate(apStateMachineOutput->m_Right.m_BlendNodeLayer[i].m_OutputBlendArray);
				arAlloc.Deallocate(apStateMachineOutput->m_Right.m_BlendNodeLayer[i].m_OutputIndexArray);
				arAlloc.Deallocate(apStateMachineOutput->m_Right.m_BlendNodeLayer[i].m_ReverseArray);
				arAlloc.Deallocate(apStateMachineOutput->m_Right.m_BlendNodeLayer[i].m_MirrorArray);
				arAlloc.Deallocate(apStateMachineOutput->m_Right.m_BlendNodeLayer[i].m_CycleOffsetArray);

				arAlloc.Deallocate(apStateMachineOutput->m_Left.m_BlendNodeLayer[i].m_OutputBlendArray);
				arAlloc.Deallocate(apStateMachineOutput->m_Left.m_BlendNodeLayer[i].m_OutputIndexArray);
				arAlloc.Deallocate(apStateMachineOutput->m_Left.m_BlendNodeLayer[i].m_ReverseArray);
				arAlloc.Deallocate(apStateMachineOutput->m_Left.m_BlendNodeLayer[i].m_MirrorArray);
				arAlloc.Deallocate(apStateMachineOutput->m_Left.m_BlendNodeLayer[i].m_CycleOffsetArray);
			}

			arAlloc.Deallocate(apStateMachineOutput->m_Left.m_BlendNodeLayer);
			arAlloc.Deallocate(apStateMachineOutput->m_Right.m_BlendNodeLayer);
			arAlloc.Deallocate(apStateMachineOutput);			
		}		
	}	

	float DoBlendTreeEvaluation( const StateConstant& arStateConstant, StateOutput& arStateOutput, StateMemory& arStateMemory, StateWorkspace& arStateWorkspace, const ValueArrayConstant& arValues,  const StateMachineInput& arStateMachineInput, int blendTreeIndex, float weight)
	{	
		float duration = 0 ;		
		for(uint32_t i = 0 ; i < arStateConstant.m_MotionSetCount ; i++)
		{
			animation::BlendTreeConstant const* treeConstant = statemachine::GetBlendTreeConstant(arStateConstant, i);
			animation::BlendTreeMemory const *treeMemory = statemachine::GetBlendTreeMemory(arStateConstant,arStateMemory, i);

			if(treeConstant)
			{
				if(!treeConstant->m_BlendEventArrayConstant.IsNull())
				{	
					for(uint32_t k = 0 ; k < treeConstant->m_BlendEventArrayConstant->m_Count ; k++)
					{				
						float blendValue = 0.0f;																
						int32_t index = FindValueIndex(&arValues, treeConstant->m_BlendEventArrayConstant->m_ValueArray[k].m_ID);
						if(index >=0)
						{						
							arStateMachineInput.m_Values->ReadData(blendValue, arValues.m_ValueArray[index].m_Index);
						}				
						arStateWorkspace.m_BlendTreeInputArray[i]->m_BlendValueArray->WriteData(blendValue, treeConstant->m_BlendEventArrayConstant->m_ValueArray[k].m_Index);
					}		
				}
																									
				animation::EvaluateBlendTree(*treeConstant, *arStateWorkspace.m_BlendTreeInputArray[i], *treeMemory, *arStateWorkspace.m_BlendTreeOutputArray[i], *arStateWorkspace.m_BlendTreeWorkspaceArray[i]);			
				
				uint32_t index = 0 ;
				uint32_t leafIndex = 0;
				while(index < arStateWorkspace.m_BlendTreeOutputArray[i]->m_MaxBlendedClip && arStateWorkspace.m_BlendTreeOutputArray[i]->m_OutputBlendArray[index].m_ID != -1)
				{
					arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_OutputBlendArray[arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_OutputCount] = weight*arStateWorkspace.m_BlendTreeOutputArray[i]->m_OutputBlendArray[index].m_BlendValue;
					
					arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_ReverseArray[arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_OutputCount] = ( arStateWorkspace.m_BlendTreeOutputArray[i]->m_OutputBlendArray[index].m_Reverse && arStateConstant.m_Speed >= 0) ||
						( !arStateWorkspace.m_BlendTreeOutputArray[i]->m_OutputBlendArray[index].m_Reverse && arStateConstant.m_Speed < 0);
					
					arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_MirrorArray[arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_OutputCount] = ( arStateWorkspace.m_BlendTreeOutputArray[i]->m_OutputBlendArray[index].m_Mirror || arStateConstant.m_Mirror) &&
						!(arStateWorkspace.m_BlendTreeOutputArray[i]->m_OutputBlendArray[index].m_Mirror && arStateConstant.m_Mirror);
					
					arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_CycleOffsetArray[arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_OutputCount] = arStateWorkspace.m_BlendTreeOutputArray[i]->m_OutputBlendArray[index].m_CycleOffset + arStateConstant.m_CycleOffset;

					for( ; leafIndex< arStateConstant.m_LeafInfoArray[i].m_Count ; leafIndex++)
					{
						/// match leaf ID
						if(arStateWorkspace.m_BlendTreeOutputArray[i]->m_OutputBlendArray[index].m_ID == arStateConstant.m_LeafInfoArray[i].m_IDArray[leafIndex])
						{
							arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_OutputIndexArray[arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_OutputCount] = leafIndex + arStateConstant.m_LeafInfoArray[i].m_IndexOffset;
							arStateOutput.m_BlendNode->m_BlendNodeLayer[i].m_OutputCount++;	
							leafIndex++;
							break;
						}									
					}										
					
					index++;
				}	
				
				// sync layers affect timing
				float effectiveDurationWeight = 1 ; 
				for(int index = arStateConstant.m_MotionSetCount - 1 ; index >= (int)i+1; --index)
				{
					 if(statemachine::GetBlendTreeConstant(arStateConstant, index))  // if has motion 
						 effectiveDurationWeight -= (effectiveDurationWeight*arStateMachineInput.m_MotionSetTimingWeightArray[index]);
				}
			
				duration += arStateWorkspace.m_BlendTreeOutputArray[i]->m_Duration*arStateMachineInput.m_MotionSetTimingWeightArray[i]*effectiveDurationWeight;
				

			}
		}
		
		return duration;
	}
	
	void EvaluateState(	ValueArrayConstant const* apValues, 
						StateConstant const *apStateConstant, 
						StateMachineInput const* apStateMachineInput,				
						StateMachineMemory* apStateMachineMemory,
						StateOutput *apStateOutput, 
						StateMemory *apStateMemory, 
						StateWorkspace *apStateWorkspace)
	{		
		for(int i = 0 ; i < apStateConstant->m_MotionSetCount ; i++)
		{
			apStateOutput->m_BlendNode->m_BlendNodeLayer[i].m_OutputCount = 0; 
		}
			
		apStateOutput->m_BlendNode->m_IKOnFeet	= apStateConstant->m_IKOnFeet;		

		float deltaTime = apStateMachineInput->m_DeltaTime;
				
		float speed = IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1) ? math::abs(apStateConstant->m_Speed) : 1.f;

		apStateOutput->m_StateDuration = DoBlendTreeEvaluation( *apStateConstant, *apStateOutput, *apStateMemory, *apStateWorkspace, *apValues, *apStateMachineInput, 0, 1.f) / speed;	

		if( apStateOutput->m_StateDuration != 0)
		{
			deltaTime /= apStateOutput->m_StateDuration;	
		}

		if(apStateMachineMemory->m_ActiveGotoState && apStateMachineInput->m_GotoStateInfo->m_StateID == 0)
		{
			apStateOutput->m_BlendNode->m_CurrentTime	= apStateMachineInput->m_GotoStateInfo->m_NormalizedTime + (apStateMachineInput->m_GotoStateInfo->m_DenormalizedTimeOffset/apStateOutput->m_StateDuration); 
			apStateOutput->m_BlendNode->m_PreviousTime	= apStateMachineInput->m_GotoStateInfo->m_NormalizedTime - deltaTime;
			apStateMachineMemory->m_ActiveGotoState = false;
			apStateMachineInput->m_GotoStateInfo->m_DenormalizedTimeOffset = 0.0f;
		}
		else
		{
			apStateOutput->m_BlendNode->m_CurrentTime	= apStateMemory->m_PreviousTime + deltaTime;
			apStateOutput->m_BlendNode->m_PreviousTime	= apStateMemory->m_PreviousTime ;
		}
		
		apStateMemory->m_PreviousTime				= apStateOutput->m_BlendNode->m_CurrentTime;	
		apStateMemory->m_Duration					= apStateOutput->m_StateDuration;
	}

	void EvaluateTransition ( TransitionConstant const* apTransitionConstant, TransitionInput const *apTransitionInput,  TransitionOutput * apTransitionOutput, TransitionMemory const* apTransitionMemory, TransitionWorkspace const* apTransitionWorkspace)
	{
		apTransitionOutput->m_DoTransition = apTransitionConstant->m_ConditionConstantCount > 0;
		apTransitionOutput->m_TransitionDuration = apTransitionConstant->m_TransitionDuration;
		apTransitionOutput->m_TransitionOffset = apTransitionConstant->m_TransitionOffset;		
		apTransitionOutput->m_TransitionStartTime = 0.0f;
		apTransitionOutput->m_NextStateStartTime = apTransitionConstant->m_TransitionOffset;
	
		
		for( uint32_t conditionIndex = 0 ; apTransitionOutput->m_DoTransition && conditionIndex < apTransitionConstant->m_ConditionConstantCount ; conditionIndex++)
		{
			apTransitionOutput->m_DoTransition = false;
			const ConditionConstant* currentCondition = apTransitionConstant->m_ConditionConstantArray[conditionIndex].Get();
			
			if(	currentCondition->m_ConditionMode == kConditionModeIf || 
						currentCondition->m_ConditionMode == kConditionModeIfNot || 
						currentCondition->m_ConditionMode == kConditionModeGreater || 
						currentCondition->m_ConditionMode == kConditionModeLess ||
						currentCondition->m_ConditionMode == kConditionModeEquals ||
						currentCondition->m_ConditionMode == kConditionModeNotEqual)
			{
				int32_t index = FindValueIndex(apTransitionMemory->m_ValuesConstant, currentCondition->m_EventID);				
				
				if(index > -1)
				{
					ValueConstant const& valueConstant = apTransitionMemory->m_ValuesConstant->m_ValueArray[index];
					if(currentCondition->m_ConditionMode == kConditionModeIf || currentCondition->m_ConditionMode == kConditionModeIfNot)
					{
						bool booleanEvent;
						apTransitionInput->m_Values->ReadData(booleanEvent, valueConstant.m_Index);
						apTransitionOutput->m_DoTransition = currentCondition->m_ConditionMode == kConditionModeIf ? booleanEvent : !booleanEvent;
					}
					else if (currentCondition->m_ConditionMode == kConditionModeEquals || currentCondition->m_ConditionMode == kConditionModeNotEqual)
					{
						mecanim::int32_t intEvent;
						apTransitionInput->m_Values->ReadData(intEvent, valueConstant.m_Index);
						apTransitionOutput->m_DoTransition = currentCondition->m_ConditionMode == kConditionModeEquals ? intEvent == currentCondition->m_EventThreshold : intEvent != currentCondition->m_EventThreshold;
					}
					else
					{										
						if(valueConstant.m_Type == mecanim::kFloatType)
						{
							float floatEvent;
							apTransitionInput->m_Values->ReadData(floatEvent, valueConstant.m_Index);
							apTransitionOutput->m_DoTransition = currentCondition->m_ConditionMode == kConditionModeGreater ? floatEvent > currentCondition->m_EventThreshold : floatEvent < currentCondition->m_EventThreshold;
						}
						else if(valueConstant.m_Type == mecanim::kInt32Type)
						{
							mecanim::int32_t intEvent;
							apTransitionInput->m_Values->ReadData(intEvent, valueConstant.m_Index);
							apTransitionOutput->m_DoTransition = currentCondition->m_ConditionMode == kConditionModeGreater ? intEvent > currentCondition->m_EventThreshold : intEvent < currentCondition->m_EventThreshold;
						}
						
					}												
				}
			}
			else if(currentCondition->m_ConditionMode == kConditionModeExitTime) 
			{
				float relativeTimeError = 0;
				if(currentCondition->m_ExitTime <= 1)
				{
					float previousTimeLow = math::fmod(apTransitionInput->m_PreviousTime,1);
					float currentTimeLow = math::fmod(apTransitionInput->m_CurrentTime,1);
		
					float previousTimeHigh = previousTimeLow;
					float currentTimeHigh = currentTimeLow;

					if(previousTimeLow > currentTimeLow) 
					{
						previousTimeLow -= 1;
						currentTimeHigh += 1;
					}

					if(previousTimeLow < currentCondition->m_ExitTime && currentTimeLow >= currentCondition->m_ExitTime)
					{
						apTransitionOutput->m_DoTransition = true;
						relativeTimeError = (currentTimeLow - currentCondition->m_ExitTime);				
					}
					else if(previousTimeHigh < currentCondition->m_ExitTime && currentTimeHigh >= currentCondition->m_ExitTime)
					{
						apTransitionOutput->m_DoTransition = true;
						relativeTimeError = (currentTimeHigh - currentCondition->m_ExitTime);																							
					}
				}
				else if ( apTransitionInput->m_PreviousTime < currentCondition->m_ExitTime && apTransitionInput->m_CurrentTime >= currentCondition->m_ExitTime)
				{
					apTransitionOutput->m_DoTransition = true;
					relativeTimeError = (apTransitionInput->m_CurrentTime - currentCondition->m_ExitTime);				
				}	

				if(apTransitionOutput->m_DoTransition)
				{
					apTransitionOutput->m_TransitionStartTime = apTransitionOutput->m_TransitionDuration == 0 ? 1  : (relativeTimeError / apTransitionOutput->m_TransitionDuration);
					apTransitionOutput->m_NextStateStartTime = apTransitionOutput->m_TransitionOffset;
					apTransitionOutput->m_NextStateStartInitialDeltaTime = relativeTimeError; // this is in source state normalized time
				}			
			}															
		}

	}

	void EvaluateStateMachine(	StateMachineConstant const* apStateMachineConstant, 
								StateMachineInput const* apStateMachineInput, 
								StateMachineOutput * apStateMachineOutput, 
								StateMachineMemory * apStateMachineMemory, 
								StateMachineWorkspace * apStateMachineWorkspace)
	{
		
			/// Initialize workspace values	
		apStateMachineOutput->m_BlendFactor = 0;		
		for(uint32_t i = 0 ; i < apStateMachineConstant->m_MotionSetCount; i++)
		{
			apStateMachineOutput->m_Right.m_BlendNodeLayer[i].m_OutputCount	= 0;
			apStateMachineOutput->m_Left.m_BlendNodeLayer[i].m_OutputCount	= 0;
		}				

		if(apStateMachineConstant->m_StateConstantCount == 0)
		{
			for(int i = 0 ; i < apStateMachineConstant->m_MotionSetCount; i++)
				apStateMachineMemory->m_MotionSetAutoWeightArray[i] = 0;	
			return ; 
		}

		float deltaTime = apStateMachineInput->m_DeltaTime;

		const uint32_t currentStateIndex		= apStateMachineMemory->m_CurrentStateIndex;		
		const StateConstant* currentState		= apStateMachineConstant->m_StateConstantArray[currentStateIndex].Get();
		StateMemory* currentStateMemory			= apStateMachineMemory->m_StateMemoryArray[currentStateIndex].Get();
		StateWorkspace* currentStateWS  		= apStateMachineWorkspace->m_StateWorkspaceArray[currentStateIndex];		
		
		StateOutput outputSrc;
		outputSrc.m_BlendNode = &apStateMachineOutput->m_Left;
		EvaluateState(apStateMachineWorkspace->m_ValuesConstant, currentState, apStateMachineInput, apStateMachineMemory, &outputSrc, currentStateMemory, currentStateWS);
					
		TransitionInput transitionInput;
		transitionInput.m_Values					= apStateMachineInput->m_Values;
		transitionInput.m_CurrentStatePreviousTime	= currentStateMemory->m_PreviousTime;
		transitionInput.m_CurrentStateDuration		= outputSrc.m_StateDuration;
		transitionInput.m_CurrentTime				= outputSrc.m_BlendNode->m_CurrentTime;
		transitionInput.m_PreviousTime				= outputSrc.m_BlendNode->m_PreviousTime;

		TransitionOutput transitionOutput;

		TransitionMemory transitionMemory;
		transitionMemory.m_ValuesConstant = apStateMachineWorkspace->m_ValuesConstant;		

		bool startTransition = false;
		bool wasInTransition = apStateMachineMemory->m_InTransition ;

		const TransitionConstant* currentTransition =  wasInTransition ? GetTransitionConstant(apStateMachineConstant, apStateMachineConstant->m_StateConstantArray[apStateMachineMemory->m_CurrentStateIndex].Get(), apStateMachineMemory->m_TransitionId) : 0;
		bool isCurrentTransitionAnyState = apStateMachineMemory->m_TransitionId >= s_AnyTransitionEncodeKey;

		for(int i = 0 ; i < apStateMachineConstant->m_MotionSetCount; i++)
			apStateMachineMemory->m_MotionSetAutoWeightArray[i] = outputSrc.m_BlendNode->m_BlendNodeLayer[i].m_OutputCount  ? 1 : 0;		

		////////////////////////////////////////////////////////
		// Validate if we need to transition
		if(apStateMachineMemory->m_ActiveGotoState)
		{	
			int32_t nextState = GetStateIndex(apStateMachineConstant, apStateMachineInput->m_GotoStateInfo->m_StateID);
			if(nextState != -1)
			{
				startTransition = true;

				apStateMachineMemory->m_ActiveGotoState = false;

				apStateMachineMemory->m_InTransition = true;
				apStateMachineMemory->m_NextStateIndex = nextState;	
				apStateMachineMemory->m_TransitionId = s_DynamicTransitionEncodeKey;
				apStateMachineMemory->m_TransitionDuration = apStateMachineInput->m_GotoStateInfo->m_TransitionDuration;
				apStateMachineMemory->m_TransitionOffset = apStateMachineInput->m_GotoStateInfo->m_NormalizedTime;
				apStateMachineMemory->m_TransitionTime = apStateMachineInput->m_GotoStateInfo->m_TransitionTime;

				apStateMachineMemory->m_StateMemoryArray[apStateMachineMemory->m_NextStateIndex]->m_PreviousTime = apStateMachineInput->m_GotoStateInfo->m_NormalizedTime + apStateMachineMemory->m_TransitionTime;
				apStateMachineMemory->m_InInterruptedTransition = wasInTransition;
			}
		}
		
		// Dynamic transition cannot be interrupted by any transition
		for(uint32_t run = 0 ;  apStateMachineMemory->m_TransitionId != s_DynamicTransitionEncodeKey && !startTransition && run < 2 ; run++) // run 0 for AnyState, run 1 for transition in current state
		{
			/////////////////////			
			int transitionCount = 0 ; 

			if(apStateMachineMemory->m_InTransition)
			{
				if(!currentTransition->m_Atomic) 
				{	
					if ( run  == 0 )
						transitionCount = isCurrentTransitionAnyState ? apStateMachineMemory->m_TransitionId  - s_AnyTransitionEncodeKey : apStateMachineConstant->m_AnyStateTransitionConstantCount;
					else
						transitionCount = !isCurrentTransitionAnyState? apStateMachineMemory->m_TransitionId : 0 ; // anyState has higher priority					
				}				
				// else -> the transition is atomic, cannot be interrupted, transitionCount = 0
			}
			else
			{
				 transitionCount = run == 0 ? apStateMachineConstant->m_AnyStateTransitionConstantCount  : currentState->m_TransitionConstantCount;
			}			

			for(uint32_t i = 0 ; !startTransition && i < transitionCount; i++)
			{
				const TransitionConstant* transitionConstant	= run == 0 ? apStateMachineConstant->m_AnyStateTransitionConstantArray[i].Get() : currentState->m_TransitionConstantArray[i].Get();
				TransitionWorkspace* transitionWorkspace		= run == 0 ? apStateMachineWorkspace->m_AnyStateTransitionWorkspaceArray[i] : currentStateWS->m_TransitionWorkspaceArray[i];

				transitionMemory.m_InTransition					= apStateMachineMemory->m_InTransition;

				EvaluateTransition(transitionConstant, &transitionInput, &transitionOutput, &transitionMemory, transitionWorkspace);

				if(transitionOutput.m_DoTransition)
				{
					startTransition = true;
							
					apStateMachineMemory->m_InTransition = true;
					apStateMachineMemory->m_NextStateIndex = transitionConstant->m_DestinationState;	
					apStateMachineMemory->m_TransitionId = run == 0 ? (i + s_AnyTransitionEncodeKey) : i;
					apStateMachineMemory->m_TransitionDuration = transitionOutput.m_TransitionDuration;
					apStateMachineMemory->m_TransitionOffset = transitionOutput.m_TransitionOffset;
					apStateMachineMemory->m_TransitionTime = transitionOutput.m_TransitionStartTime;					
					apStateMachineMemory->m_InInterruptedTransition = wasInTransition;										
					
					apStateMachineMemory->m_ActiveGotoState = true;
					apStateMachineInput->m_GotoStateInfo->m_StateID = 0; 
					apStateMachineInput->m_GotoStateInfo->m_NormalizedTime = transitionOutput.m_NextStateStartTime;
					apStateMachineInput->m_GotoStateInfo->m_DenormalizedTimeOffset = transitionOutput.m_NextStateStartInitialDeltaTime * currentStateMemory->m_Duration; // denormalize source state time to denormalizedTime

				}		
			}
		}
			
		
		if(apStateMachineMemory->m_InTransition)
		{						
			const int32_t nextStateIndex = apStateMachineMemory->m_NextStateIndex;
			const StateConstant* nextState	= apStateMachineConstant->m_StateConstantArray[nextStateIndex].Get();
			StateMemory* nextStateMemory	= apStateMachineMemory->m_StateMemoryArray[nextStateIndex].Get();
			StateWorkspace* nextStateWS	= apStateMachineWorkspace->m_StateWorkspaceArray[nextStateIndex];

			if(wasInTransition) //  delta already applied in m_TransitionStartTime when transition was just triggered, so dont add twice.
				apStateMachineMemory->m_TransitionTime += apStateMachineMemory->m_TransitionDuration == 0 ? 1 : (deltaTime / (apStateMachineMemory->m_TransitionDuration * (outputSrc.m_StateDuration != 0 ? outputSrc.m_StateDuration : 1.0f)));
			apStateMachineOutput->m_BlendFactor = math::saturate(apStateMachineMemory->m_TransitionTime);				
							
									
			StateOutput outputDst;
			outputDst.m_BlendNode = &apStateMachineOutput->m_Right;
			EvaluateState(apStateMachineWorkspace->m_ValuesConstant, nextState, apStateMachineInput, apStateMachineMemory, &outputDst, nextStateMemory, nextStateWS);

			for(int i = 0 ; i < apStateMachineConstant->m_MotionSetCount; i++)
			{
				if(apStateMachineOutput->m_Left.m_BlendNodeLayer[i].m_OutputCount == 0 &&  apStateMachineOutput->m_Right.m_BlendNodeLayer[i].m_OutputCount > 0)
					apStateMachineMemory->m_MotionSetAutoWeightArray[i] = apStateMachineOutput->m_BlendFactor;
				else if(apStateMachineOutput->m_Right.m_BlendNodeLayer[i].m_OutputCount == 0 &&  apStateMachineOutput->m_Left.m_BlendNodeLayer[i].m_OutputCount > 0)
					apStateMachineMemory->m_MotionSetAutoWeightArray[i] = 1 - apStateMachineOutput->m_BlendFactor;

			}	

			/////////////////////////////////////////
			// Transition is finished
			if(apStateMachineMemory->m_TransitionTime >= 1)
			{
				for( uint32_t conditionIndex = 0 ; currentTransition != 0 && conditionIndex < currentTransition->m_ConditionConstantCount ; conditionIndex++)
				{
					const ConditionConstant* currentCondition = currentTransition->m_ConditionConstantArray[conditionIndex].Get();
			
					if(	currentCondition->m_ConditionMode == kConditionModeIf)
					{
						int32_t index = FindValueIndex(apStateMachineWorkspace->m_ValuesConstant, currentCondition->m_EventID);				
				
						if(index > -1)
						{
							ValueConstant const& valueConstant = apStateMachineWorkspace->m_ValuesConstant->m_ValueArray[index];
							if(valueConstant.m_Type == mecanim::kTriggerType)
								apStateMachineInput->m_Values->WriteData(false, valueConstant.m_Index);					
						}
					}
				}

				apStateMachineMemory->m_InTransition								= false;
				apStateMachineMemory->m_TransitionTime								= 0.f;
				apStateMachineMemory->m_TransitionId								= 0;
				apStateMachineMemory->m_TransitionDuration							= 0.f;
				apStateMachineMemory->m_TransitionOffset							= 0.f;				
				apStateMachineMemory->m_CurrentStateIndex							= apStateMachineMemory->m_NextStateIndex;								
				apStateMachineMemory->m_InInterruptedTransition						= false;

				
				BlendNodeLayer *nodeSwap = apStateMachineOutput->m_Left.m_BlendNodeLayer;
				apStateMachineOutput->m_Left						= apStateMachineOutput->m_Right;				
				apStateMachineOutput->m_Right.m_BlendNodeLayer		= nodeSwap;
				apStateMachineOutput->m_BlendFactor					= 0.f;
				apStateMachineOutput->m_Right.m_BlendNodeLayer[0].m_OutputCount		= 0;								
				
			}
		}	
	}

	void StateConstant::InitializeClass()
	{
		RegisterAllowNameConversion("StateConstant", "m_ID", "m_PathID");
	}


}//namespace statemachine

}//namespace mecanim
