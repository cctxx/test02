#pragma once

#include "Runtime/mecanim/animation/blendtree.h"
#include "Runtime/mecanim/generic/valuearray.h"

#include "Runtime/Serialize/Blobification/offsetptr.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Animation/MecanimArraySerialization.h" 

namespace mecanim
{

namespace statemachine
{
	static const mecanim::int32_t s_AnyTransitionEncodeKey = 20000;	
	static const mecanim::int32_t s_DynamicTransitionEncodeKey = 30000;
	
	enum ConditionMode
	{
		//kConditionModeGraph = 0,
		kConditionModeIf = 1, // backward compat
		kConditionModeIfNot,
		kConditionModeGreater,
		kConditionModeLess,
		kConditionModeExitTime,
		kConditionModeEquals,
		kConditionModeNotEqual,
		kConditionModeCount
	};	

	struct GotoStateInfo
	{
		GotoStateInfo():m_StateID(0),m_NormalizedTime(0),m_TransitionDuration(0),m_TransitionTime(0), m_DenormalizedTimeOffset(0){}

		uint32_t	m_StateID;
		float		m_NormalizedTime;
		float		m_TransitionDuration;
		float		m_TransitionTime;
		float		m_DenormalizedTimeOffset; // used to offset by seconds, internally used when transitions are taken
	};

	
	struct BlendNodeLayer
	{
		BlendNodeLayer() : m_OutputCount(0), m_OutputIndexArray(0), m_OutputBlendArray(0), m_ReverseArray(0), m_MirrorArray(0), m_CycleOffsetArray(0) {}
		uint32_t			m_OutputCount;		
		uint32_t*			m_OutputIndexArray;
		float*				m_OutputBlendArray;
		bool*				m_ReverseArray;
		bool*				m_MirrorArray;
		float*				m_CycleOffsetArray;
	};

	struct BlendNode
	{	
		BlendNode(): m_BlendNodeLayer(0), m_CurrentTime(0),m_PreviousTime(0), m_IKOnFeet(false){}

		BlendNodeLayer*		m_BlendNodeLayer;
	
		float				m_CurrentTime;
		float				m_PreviousTime;		
		bool				m_IKOnFeet;
	};


	struct ConditionConstant
	{
		DEFINE_GET_TYPESTRING(ConditionConstant)

		ConditionConstant() :	m_ConditionMode(kConditionModeIf),	
								m_EventID(0),
								m_EventThreshold(0.f),
								m_ExitTime(0)
		{ 			
		}

		uint32_t					m_ConditionMode;		
		uint32_t					m_EventID;			
		
		float						m_EventThreshold;
		float						m_ExitTime;
		
		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_ConditionMode);			
			TRANSFER(m_EventID);
			TRANSFER(m_EventThreshold);
			TRANSFER(m_ExitTime);
		}
	};	

	struct TransitionConstant
	{
		DEFINE_GET_TYPESTRING(TransitionConstant)

		TransitionConstant() :	m_ConditionConstantCount(0),								
								m_DestinationState(0),
								m_ID(0),
								m_UserID(0),
								m_TransitionDuration(0),
								m_TransitionOffset(0),								
								m_Atomic(true)
								
		{}
								

		uint32_t									m_ConditionConstantCount;
		OffsetPtr< OffsetPtr<ConditionConstant> >	m_ConditionConstantArray;
		
		uint32_t									m_DestinationState;
		uint32_t									m_ID;
		uint32_t									m_UserID;
							
		float										m_TransitionDuration;		
		float										m_TransitionOffset;
		
		bool										m_Atomic;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_ConditionConstantCount);
			MANUAL_ARRAY_TRANSFER2( OffsetPtr<mecanim::statemachine::ConditionConstant>, m_ConditionConstantArray, m_ConditionConstantCount);								
			
			TRANSFER(m_DestinationState);
			TRANSFER(m_ID);
			TRANSFER(m_UserID);

			TRANSFER(m_TransitionDuration);
			TRANSFER(m_TransitionOffset);			
			
			TRANSFER(m_Atomic);
			transfer.Align();
		}
	};

	struct TransitionWorkspace
	{		
		TransitionWorkspace():	m_ConditionConstantCount(0)		
		{
		}		

		uint32_t	m_ConditionConstantCount;		
	};


	struct TransitionInput
	{
		TransitionInput():	m_Values(0),
							m_CurrentStatePreviousTime(0),
							m_CurrentStateDuration(0),							
							m_CurrentTime(0),
							m_PreviousTime()
		{}	
		ValueArray* m_Values;
		float m_CurrentStatePreviousTime;
		float m_CurrentStateDuration;		
		float m_CurrentTime;
		float m_PreviousTime;
	};


	struct TransitionOutput
	{	
		TransitionOutput():	m_DoTransition(false),
							m_TransitionStartTime(0),
							m_NextStateStartTime(0),
							m_NextStateStartInitialDeltaTime(0),
							m_TransitionDuration(0),
							m_TransitionOffset(0)
		{}
		
		bool m_DoTransition;
		float m_TransitionStartTime;
		float m_NextStateStartTime;
		float m_NextStateStartInitialDeltaTime;
		float m_TransitionDuration;
		float m_TransitionOffset;

	};

	struct TransitionMemory
	{	
		TransitionMemory(): m_ValuesConstant(0),
							m_InTransition(false)
		{}

		ValueArrayConstant const* m_ValuesConstant;
		bool m_InTransition;
	};


	struct LeafInfoConstant
	{
		DEFINE_GET_TYPESTRING(LeafInfoConstant)
		
		LeafInfoConstant() : m_Count(0), m_IndexOffset(0)
		{

		}

		uint32_t				m_Count;		
		OffsetPtr<uint32_t>		m_IDArray;			
		uint32_t				m_IndexOffset;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_Count);			
			MANUAL_ARRAY_TRANSFER2(mecanim::uint32_t, m_IDArray, m_Count);
			TRANSFER(m_IndexOffset);
		}
	};

	struct StateConstant
	{
		DEFINE_GET_TYPESTRING(StateConstant)

		StateConstant() :	m_TransitionConstantCount(0),	
							m_MotionSetCount(0),
							m_BlendTreeCount(0),
							m_NameID(0), 
							m_PathID(0), 
							m_TagID(0),
							m_Speed(1),
							m_CycleOffset(0),
							m_IKOnFeet(true),
							m_Loop(false),
							m_Mirror(false)
		{		
		}

		uint32_t												m_TransitionConstantCount;
		OffsetPtr< OffsetPtr<TransitionConstant> > 				m_TransitionConstantArray;

		uint32_t												m_MotionSetCount;
		OffsetPtr<int32_t>										m_BlendTreeConstantIndexArray;
		OffsetPtr<LeafInfoConstant>								m_LeafInfoArray;

		uint32_t												m_BlendTreeCount;
		OffsetPtr< OffsetPtr<animation::BlendTreeConstant> >	m_BlendTreeConstantArray;

		uint32_t												m_NameID;
		uint32_t												m_PathID;
		uint32_t												m_TagID;

		float													m_Speed;
		float													m_CycleOffset;
		bool													m_IKOnFeet;	
		bool													m_Loop;
		bool													m_Mirror;
	
		static void InitializeClass();

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_TransitionConstantCount);
			MANUAL_ARRAY_TRANSFER2( OffsetPtr<mecanim::statemachine::TransitionConstant>, m_TransitionConstantArray, m_TransitionConstantCount);			
			
			TRANSFER_BLOB_ONLY(m_MotionSetCount);
			MANUAL_ARRAY_TRANSFER2(int32_t, m_BlendTreeConstantIndexArray, m_MotionSetCount);
			MANUAL_ARRAY_TRANSFER2(LeafInfoConstant, m_LeafInfoArray, m_MotionSetCount);

			TRANSFER_BLOB_ONLY(m_BlendTreeCount);
			MANUAL_ARRAY_TRANSFER2( OffsetPtr<mecanim::animation::BlendTreeConstant>, m_BlendTreeConstantArray, m_BlendTreeCount);
			
			TRANSFER(m_NameID);
			TRANSFER(m_PathID);
			TRANSFER(m_TagID);		

			TRANSFER(m_Speed);
			TRANSFER(m_CycleOffset);
			
			TRANSFER(m_IKOnFeet);
			TRANSFER(m_Loop);
			TRANSFER(m_Mirror);

			transfer.Align();
		}
	};
	
	struct StateMemory
	{
		DEFINE_GET_TYPESTRING(StateMemory)

		StateMemory():	m_PreviousTime(0), m_Duration(0)
						{}
		uint32_t											m_BlendTreeCount;
		OffsetPtr< OffsetPtr<animation::BlendTreeMemory> >	m_BlendTreeMemoryArray;

		float		m_PreviousTime;		
		float		m_Duration;			
		
		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_BlendTreeCount);
			MANUAL_ARRAY_TRANSFER2( OffsetPtr<mecanim::animation::BlendTreeMemory>, m_BlendTreeMemoryArray, m_BlendTreeCount);

			TRANSFER(m_PreviousTime);
			TRANSFER(m_Duration);
		
			transfer.Align();
		}
	
	};

	struct StateWorkspace
	{		
		StateWorkspace():m_TransitionWorkspaceArray(0),						
						m_BlendTreeInputArray(0),
						m_BlendTreeOutputArray(0),	
						m_BlendTreeWorkspaceArray(0),
						m_TransitionWorkspaceCount(0)						
		{}

		TransitionWorkspace**				m_TransitionWorkspaceArray;				
		

		animation::BlendTreeInput**			m_BlendTreeInputArray;
		animation::BlendTreeOutput**		m_BlendTreeOutputArray;				
		animation::BlendTreeWorkspace**		m_BlendTreeWorkspaceArray;

		uint32_t m_TransitionWorkspaceCount;
		uint32_t m_MotionSetCount;
	};



	struct StateOutput
	{
		StateOutput() : m_BlendNode(0), m_StateDuration(0) {}

		float m_StateDuration;

		BlendNode* m_BlendNode;		
	};
	

	struct StateMachineConstant
	{
		DEFINE_GET_TYPESTRING(StateMachineConstant)

		StateMachineConstant() : m_StateConstantCount(0),
								 m_AnyStateTransitionConstantCount(0),
								 m_DefaultState(0),	
								 m_MotionSetCount(0)								 
		{}

		uint32_t									m_StateConstantCount;
		OffsetPtr< OffsetPtr<StateConstant> >		m_StateConstantArray;

		uint32_t									m_AnyStateTransitionConstantCount;
		OffsetPtr< OffsetPtr<TransitionConstant> >	m_AnyStateTransitionConstantArray;
				
		uint32_t									m_DefaultState;
		uint32_t									m_MotionSetCount;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_StateConstantCount);
			MANUAL_ARRAY_TRANSFER2( OffsetPtr<mecanim::statemachine::StateConstant> , m_StateConstantArray, m_StateConstantCount);

			TRANSFER_BLOB_ONLY(m_AnyStateTransitionConstantCount);
			MANUAL_ARRAY_TRANSFER2( OffsetPtr<mecanim::statemachine::TransitionConstant>, m_AnyStateTransitionConstantArray, m_AnyStateTransitionConstantCount);

			TRANSFER(m_DefaultState);
			TRANSFER(m_MotionSetCount);			
		}
	};

	struct StateMachineInput 
	{
		StateMachineInput() : m_Values(0)
		{}

		float			m_DeltaTime;
		ValueArray*		m_Values;

		float*          m_MotionSetTimingWeightArray; // allocated externally		
		
		GotoStateInfo*	m_GotoStateInfo;
	};

	struct StateMachineMemory
	{
		DEFINE_GET_TYPESTRING(StateMachineMemory)

		StateMachineMemory() :	m_MotionSetCount(0),
								m_StateMemoryCount(0),
								m_CurrentStateIndex(0),
								m_NextStateIndex(0),
								m_TransitionId(0),
								m_TransitionTime(0),
								m_TransitionDuration(0),
								m_TransitionOffset(0),
								m_InInterruptedTransition(false),
								m_InTransition(false),
								m_ActiveGotoState(false)
		{}
		
		uint32_t							m_MotionSetCount;
		OffsetPtr<float>					m_MotionSetAutoWeightArray;		

		uint32_t							m_StateMemoryCount;
		OffsetPtr<OffsetPtr<StateMemory> >	m_StateMemoryArray;

		uint32_t		m_CurrentStateIndex;
		uint32_t		m_NextStateIndex;
		uint32_t		m_TransitionId;
		
		float			m_TransitionTime;
		float			m_TransitionDuration;
		float			m_TransitionOffset;		
				
		bool			m_InInterruptedTransition;
		bool			m_InTransition;
		bool			m_ActiveGotoState;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_MotionSetCount);
			MANUAL_ARRAY_TRANSFER2( float, m_MotionSetAutoWeightArray, m_MotionSetCount);

			TRANSFER_BLOB_ONLY(m_StateMemoryCount);
			MANUAL_ARRAY_TRANSFER2( OffsetPtr<StateMemory>, m_StateMemoryArray, m_StateMemoryCount);			

			TRANSFER(m_CurrentStateIndex);
			TRANSFER(m_NextStateIndex);
			TRANSFER(m_TransitionId);
			
			TRANSFER(m_TransitionTime);
			TRANSFER(m_TransitionDuration);
			TRANSFER(m_TransitionOffset);

			TRANSFER(m_InInterruptedTransition);
			TRANSFER(m_InTransition);
			TRANSFER(m_ActiveGotoState);
			transfer.Align();
		}
	};

	struct StateMachineWorkspace
	{
		StateMachineWorkspace() :	m_StateWorkspaceArray(0),
									m_StateWorkspaceCount(0),
									m_AnyStateTransitionWorkspaceArray(0),
									m_AnyStateTransitionWorkspaceCount(0)									
		{}
		
		StateWorkspace**		m_StateWorkspaceArray;		
		uint32_t				m_StateWorkspaceCount;

		TransitionWorkspace**	m_AnyStateTransitionWorkspaceArray;		
		uint32_t				m_AnyStateTransitionWorkspaceCount;

		ValueArrayConstant*		m_ValuesConstant;
		
	};
	
	struct StateMachineOutput
	{
		StateMachineOutput(): m_BlendFactor(0),m_MotionSetCount(0)
		{}

		float			m_BlendFactor;

		BlendNode		m_Left;
		BlendNode		m_Right;

		uint32_t		m_MotionSetCount;
		
		
	};



	TransitionConstant const* GetTransitionConstant(StateMachineConstant const* apStateMachineConstant, StateConstant const* apStateConstant, uint32_t id);

	bool IsCurrentTransitionAtomic(StateMachineConstant const* apStateMachineConstant, StateMachineMemory *apStateMachineMemory);

	ConditionConstant *CreateConditionConstant( uint32_t aConditionMode, uint32_t aEventID, float aEventThreshold, float aExitTime, memory::Allocator& arAlloc);		
	void DestroyConditionConstant(ConditionConstant *apConditionConstant, memory::Allocator& arAlloc);		

	TransitionConstant *CreateTransitionConstant(ConditionConstant** apConditionsConstantArray, uint32_t aConditionConstantCount, uint32_t aDestinationState, 
		float aTransitionDuration, float aTransitionOffset, bool aAtomic, uint32_t aID, uint32_t aUserID, memory::Allocator& arAlloc);

	void DestroyTransitionConstant(TransitionConstant *apTransitionConstant, memory::Allocator& arAlloc);	
	

	TransitionWorkspace* CreateTransitionWorkspace(TransitionConstant const* apTransitionConstant, memory::Allocator& arAlloc);
	void DestroyTransitionWorkspace(TransitionWorkspace* apTransitionWorkspace, memory::Allocator& arAlloc);


	StateConstant* CreateStateConstant(TransitionConstant** apTransitionConstantArray, uint32_t aTransitionConstantCount, 																							
										float aSpeed, bool aIKOnFeet, bool aMirror, float aCycleOffset, animation::BlendTreeConstant** apBlendTreeConstantArray,
										uint32_t aMotionSetCount, uint32_t nameID, uint32_t pathID, uint32_t aTagID, bool aLoop, memory::Allocator& arAlloc);
	
	void DestroyStateConstant(StateConstant* apStateConstant, memory::Allocator& arAlloc);		

	void DestroyStateMemory(StateMemory* apStateMemory, memory::Allocator& arAlloc);

	StateWorkspace* CreateStateWorkspace(StateConstant const* apStateConstant, memory::Allocator& arAlloc);
	void DestroyStateWorkspace(StateWorkspace* apStateWorkspace, memory::Allocator& arAlloc);

	StateOutput* CreateStateOutput(StateConstant const* apStateConstant, memory::Allocator& arAlloc);
	void DestroyStateOutput(StateOutput* apStateOutput, memory::Allocator& arAlloc);

	StateMachineConstant* CreateStateMachineConstant(StateConstant** apStateConstantArray, uint32_t aStateConstantCount, uint32_t aDefaultState, 		
		TransitionConstant** apAnyStateTransitionConstantArray, uint32_t	aAnyStateTransitionConstantCount, uint32_t aMotionSetCount, 
		memory::Allocator& arAlloc);
	void DestroyStateMachineConstant(StateMachineConstant* apStateMachineConstant, memory::Allocator& arAlloc);
	
	void ClearStateMachineConstant(StateMachineConstant* apStateMachineConstant, memory::Allocator& arAlloc);

	StateMachineInput* CreateStateMachineInput(StateMachineConstant const* apStateMachineConstant, memory::Allocator& arAlloc);
	void DestroyStateMachineInput(StateMachineInput* apStateMachineInput, memory::Allocator& arAlloc);

	StateMachineMemory* CreateStateMachineMemory(StateMachineConstant const* apStateMachineConstant, memory::Allocator& arAlloc);
	void DestroyStateMachineMemory(StateMachineMemory* apStateMachineMemory, memory::Allocator& arAlloc);


	StateMachineWorkspace* CreateStateMachineWorkspace(StateMachineConstant const* apStateMachineConstant, uint32_t maxBlendedClip, memory::Allocator& arAlloc);
	void DestroyStateMachineWorkspace(StateMachineWorkspace* apStateMachineWorkspace, memory::Allocator& arAlloc);

	StateMachineOutput* CreateStateMachineOutput(StateMachineConstant const* apStateMachineConstant, uint32_t maxBlendedClip, memory::Allocator& arAlloc);
	void DestroyStateMachineOutput(StateMachineOutput* apStateMachineOutput, memory::Allocator& arAlloc);

	void EvaluateStateMachine(StateMachineConstant const* apStateMachineConstant, StateMachineInput const* apStateMachineInput, 
								StateMachineOutput * apStateMachineOutput, StateMachineMemory * apStateMachineMemory, 
								StateMachineWorkspace * apStateMachineWorkspace);

	animation::BlendTreeConstant const* GetBlendTreeConstant(const StateConstant& arStateConstant, mecanim::int32_t aMotionSetCount);
	animation::BlendTreeMemory *GetBlendTreeMemory(const StateConstant& arStateConstant,StateMemory& arStateMemory, mecanim::int32_t  aMotionSetIndex);	

	int32_t GetStateIndex(StateMachineConstant const* apStateMachineConstant, uint32_t id);

	inline int32_t CompareStateID (StateConstant const* state, uint32_t id) { return state->m_PathID == id || state->m_NameID == id; }
	}
}



