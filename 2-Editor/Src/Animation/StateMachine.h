#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Serialize/SerializeTraits.h"

#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Animation/AnimatorController.h"
#include "Editor/Src/Animation/BlendTree.h"

#include "Runtime/mecanim/statemachine/statemachine.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Misc/UserList.h"

class UserList;
class Transition;
class State;
class StateMachine;
class Animator;

typedef std::vector<PPtr<Transition> >				TransitionVector;
typedef std::vector<PPtr<State> >					StateVector;	
typedef std::vector<PPtr<StateMachine> >			StateMachineVector;	

typedef std::map<PPtr<State> , TransitionVector >   TransitionMap;


namespace mecanim 
{
	namespace statemachine
	{
		struct StateMachineConstant;
	}
}

enum TransitionConditionMode
{
	kConditionModeIf = mecanim::statemachine::kConditionModeIf,
	kConditionModeIfNot = mecanim::statemachine::kConditionModeIfNot,
	kConditionModeGreater = mecanim::statemachine::kConditionModeGreater,
	kConditionModeLess = mecanim::statemachine::kConditionModeLess,
	kConditionModeExitTime = mecanim::statemachine::kConditionModeExitTime,
	kConditionModeEquals = mecanim::statemachine::kConditionModeEquals,
	kConditionModeNotEqual = mecanim::statemachine::kConditionModeNotEqual,
	kConditionModeCount = mecanim::statemachine::kConditionModeCount
};


static const float DefaultTransitionDurationInSeconds = 0.25f;



class Condition
{
public:
	DEFINE_GET_TYPESTRING (Condition)

	Condition()	:
	m_ConditionMode(kConditionModeExitTime),
	m_ConditionEvent(""), 
	m_EventTreshold(0.f),
	m_ExitTime(0.9f)
	{
	}
		
	int						m_ConditionMode;				//eConditionMode
	UnityStr				m_ConditionEvent;			
	float					m_EventTreshold;// m_ParameterThreshold
	float					m_ExitTime;



	template<class TransferFunction>
	void Transfer (TransferFunction& transfer)
	{		
		TRANSFER(m_ConditionMode);
		TRANSFER(m_ConditionEvent);		
		TRANSFER(m_EventTreshold);
		TRANSFER(m_ExitTime);
	}

};

typedef std::vector<Condition> ConditionVector;

class Transition  : public NamedObject
{	
public :		 
	REGISTER_DERIVED_CLASS (Transition, NamedObject);
	DECLARE_OBJECT_SERIALIZE (Transition);

	Transition(MemLabelId label, ObjectCreationMode mode);
	
	virtual void AwakeFromLoad (AwakeFromLoadMode mode);
	
	virtual void SetName (char const* name);	
	
	std::string				GetDisplayName(bool shortName) const ; 
	std::string				GetUniqueName(bool shortName = false) const ; 

	int						GetUniqueNameHash() const;

	void					SetSrcState(State* state);
	State*					GetSrcState();

	void					SetDstState(State* state);
	State*					GetDstState();

	int						GetConditionCount();
	void					AddCondition();
	void					RemoveCondition(int index);

		
	std::string				GetConditionParameter(int index)const;
	void					SetConditionParameter(int index, std::string const& event);

	float					GetParameterTreshold(int index)const;
	void					SetParameterTreshold(int index, float threshold);

	float					GetExitTime(int index)const;
	void					SetExitTime(int index, float exit);

	TransitionConditionMode	GetConditionMode(int index)const;
	void					SetConditionMode(int index, TransitionConditionMode mode);
	
	float					GetTransitionDuration() const;
	void					SetTransitionDuration(float duration);

	float					GetTransitionOffset() const;
	void					SetTransitionOffset(float offset);
	

	bool					GetAtomic() const;
	void					SetAtomic(bool atomic); 

	bool					GetSolo() const;
	void					SetSolo(bool solo);

	bool					GetMute() const;
	void					SetMute(bool mute);			

	UserList&               GetUserList () { return m_UserList; }

	bool					ValidateConditionIndex(int index) const;
	
private:

	ConditionVector			m_Conditions;

	PPtr<State>				m_DstState;
	PPtr<State>				m_SrcState;	
	
	float					m_TransitionDuration;
	float					m_TransitionOffset;		

	bool					m_Atomic;
	bool					m_Solo;
	bool					m_Mute;
	
	mutable UserList         m_UserList;

	
};

class State : public NamedObject 
{	
public:	

	REGISTER_DERIVED_CLASS (State, NamedObject)
	DECLARE_OBJECT_SERIALIZE (State)	

	State(MemLabelId label, ObjectCreationMode mode);

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void CheckConsistency();
		
	
	virtual void SetName (char const* name);	

	std::string					GetUniqueName() const ;

	int							GetUniqueNameHash() const;
		
	GET_SET_COMPARE_DIRTY (Vector3f, Position, m_Position);		
	
	Motion*						GetMotion(int motionSetIndex);
	void 						SetMotion(int motionSetIndex, Motion* motion);
				
	bool						GetIKOnFeet() const ;
	void						SetIKOnFeet(bool ikOnFeet);

	float						GetSpeed() const ;
	void						SetSpeed(float speed);

	bool						GetMirror() const;
	void						SetMirror(bool mirror);

	std::string					GetTag()const ;
	void						SetTag(const string& tag);	

	float						GetCycleOffset() const;

	BlendTree*					CreateBlendTree(int motionSetIndex);

	StateMachine*				GetStateMachine();
	
	
	UserList&                   GetUserList () { return m_UserList; }

	void						AddMotionSet();
	void						RemoveMotionSet(int index);

	BlendParameterList			CollectParameters();
	
private:	
	
	
	float						m_Speed;			
	float						m_CycleOffset;
	bool						m_IKOnFeet;	
	bool						m_Mirror;
		
	MotionVector				m_Motions; // one motion per motion set		

	PPtr<StateMachine>			m_ParentStateMachine;
	Vector3f					m_Position;
	UnityStr					m_Tag;
			
	UserList                    m_UserList;

	bool	ValidateMotionSetIndex(int index) const;
	

	friend class StateMachine;

};




class StateMachine : public NamedObject
{	
	
public:	
	REGISTER_DERIVED_CLASS (StateMachine, NamedObject)
	DECLARE_OBJECT_SERIALIZE (StateMachine)
	
	virtual void SetName (char const* name);	
	
	static void InitializeClass();
	static void CleanupClass () {}
	
	StateMachine (MemLabelId label, ObjectCreationMode mode);	

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void CheckConsistency();

	void					GetAnimationClips(AnimationClipVector& clips, int motionSetIndex);
	void					GetBlendTrees(BlendTreeVector& trees);

	
	State*					AddState(const std::string& name);
	void					AddState(State*);
	bool					RemoveState(State*);
	int						GetStateCount()const;
	State*					GetState(int i)const;

	Transition*				AddTransition(State* src, State* dst);
	void					RemoveTransition(Transition* transition, bool deleteTransition = true);	
	
	StateMachine*			AddStateMachine(const std::string& name);	
	void					AddStateMachine(StateMachine* stateMachine);	
	bool					RemoveStateMachine(StateMachine* stateMachine);
	int						GetStateMachineCount()const;
	StateMachine*			GetStateMachine(int i)const;
	

	StateMachine*			GetParentStateMachine()const;
	

	Vector3f				GetStateMachinePosition(int i)const{ return m_ChildStateMachinePosition[i];}
	void					SetStateMachinePosition(int i, Vector3f pos){ m_ChildStateMachinePosition[i] = pos; SetDirty();}	

	State*					DefaultState()const{return m_DefaultState;}
	void					SetDefaultState(State* state);
		
	bool					HasState(const State *state, bool recursive) const;
	bool					HasStateMachine(const StateMachine *stateMachine, bool recursive) const;
		
	int						GetTransitionCount();

	void					MoveState(State *state, StateMachine *targetStateMachine);
	void					MoveStateMachine(StateMachine *stateMachine, StateMachine *targetStateMachine);
				

	GET_SET_COMPARE_DIRTY (Vector3f, AnyStatePosition, m_AnyStatePosition);
	GET_SET_COMPARE_DIRTY (Vector3f, ParentStateMachinePosition, m_ParentStateMachinePosition);
	
	mecanim::statemachine::StateMachineConstant* BuildRuntimeAsset(UserList& dependency, TOSVector& tos, int layerIndex, mecanim::memory::Allocator& alloc) const;		
	

	void RenameParameter(const std::string& newName, const std::string& oldName);
	void AddFirstParameterOfType(const std::string& name, AnimatorControllerParameterType type);	

	void					AddMotionSet();
	void					RemoveMotionSet(int index);
	int						GetMotionSetCount() const;

	void					FixStateParent();

	TransitionVector GetOrderedTransitionsFromState(State *state) const;
	void SetOrderedTransitionsFromState(State *state, TransitionVector& transitions);
	void AddOrderedTransition(Transition* transition);

	TransitionVector GetTransitionsToState(State *state) const;

	void SyncTransitionsFromRoot();

	string MakeUniqueStateName(const string& newName, State const * target) const;
	string MakeUniqueStateMachineName(const string& newName, StateMachine const * target) const;

	BlendParameterList			CollectParameters();
	std::vector<PPtr<Object> >	CollectObjectsUsingParameter(const string& parameterName);
	
protected:

	void					RemoveAllTransitionTo(State *state); 
	void					RemoveAllTransitionFrom(State *state);

	void					AddTransition(Transition *transition, bool dirty = true);
	void					AddLocalTransition(Transition* transition, bool dirty = true);		

	bool					RemoveStateFromArray(State *state, bool deleteState =true);
	bool					RemoveStateMachineFromArray(StateMachine *stateMachine, bool deleteStateMachine =true);
		
	void					SetParentStateMachine(StateMachine* stateMachine);
	
	

private:
	PPtr<State>								m_DefaultState;	
	StateVector								m_States;	
	StateMachineVector						m_ChildStateMachine;	
	std::vector<Vector3f>					m_ChildStateMachinePosition;	
	PPtr<StateMachine>						m_ParentStateMachine;

	Vector3f								m_AnyStatePosition;
	Vector3f								m_ParentStateMachinePosition;
	
	TransitionMap							m_OrderedTransitions;		
	mutable UserList                        m_UserList;

	int										m_MotionSetCount;
		
	mecanim::statemachine::TransitionConstant* BuildTransitionConstant(Transition& model, UserList& userList, StateVector const& allStates, TOSVector& tos, mecanim::memory::Allocator& alloc) const ;	

	TransitionVector		GetAllLocalTransitionFromState(State* state)const;	

	void RemoveFromOrderedTransitions(Transition* transition);
	void RemoveFromOrderedTransitions(State* state);


	template<class T>
	void AnyStateTransitionsBackwardsCompatibility (T& transfer);
	
	bool ValidateStateIndex(int index) const ;
	bool ValidateStateMachineIndex(int index) const;
	
	friend class State;

};

#endif // STATEMACHINE_H
