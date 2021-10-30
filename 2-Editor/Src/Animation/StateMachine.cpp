
#include "StateMachine.h"

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferUtility.h"

#include "Editor/Src/Animation/BlendTree.h"
#include "Runtime/Animation/Animator.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Runtime/Misc/UserList.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Animation/AnimationClipStats.h"

#include "Runtime/Serialize/PersistentManager.h"

#include "Editor/Src/EditorHelper.h"


#define DIRTY_AND_INVALIDATE() m_UserList.SendMessage (kDidModifyMotion); SetDirty();  
#define INVALIDATE()           m_UserList.SendMessage (kDidModifyMotion);



StateMachine* GetRootStateMachine(StateMachine *stateMachine)
{
	if(stateMachine->GetParentStateMachine() == 0)
	{
		return stateMachine;
	}
	else
	{
		return GetRootStateMachine(stateMachine->GetParentStateMachine());
	}
}

State*  GetState(StateVector const& arStateModels, const std::string& stateName)
{
	for(unsigned int i = 0 ; i < arStateModels.size() ; ++i)
	{		
		if(arStateModels[i]->GetUniqueName() == stateName)
		{
			return arStateModels[i];
		}		
	}
	return 0;
}	

int  GetStateIndex(StateVector const& arStateModels, const std::string& stateName)
{
	for(unsigned int i = 0 ; i < arStateModels.size() ; ++i)
	{		
		if(arStateModels[i]->GetUniqueName() == stateName)
		{
			return i;
		}		
	}
	return -1;
}	

void CollectAllStates(StateVector &states, StateMachine const *pStateMachine)
{
	for(unsigned int i = 0 ; i <pStateMachine->GetStateCount() ; i++)
	{
		states.push_back(pStateMachine->GetState(i));					
	}
	for(unsigned int i = 0 ; i < pStateMachine->GetStateMachineCount(); i++)
	{
		CollectAllStates(states, pStateMachine->GetStateMachine(i));
	}
}

void CollectAllSubStateMachines(StateMachineVector &subStateMachines,  StateMachine const * stateMachine)
{
	for (int i = 0; i < stateMachine->GetStateMachineCount(); i++)
	{
		subStateMachines.push_back(stateMachine->GetStateMachine(i));
		CollectAllSubStateMachines(subStateMachines, stateMachine->GetStateMachine(i));
	}
}



IMPLEMENT_CLASS (Transition)
IMPLEMENT_OBJECT_SERIALIZE (Transition)
template<class TransferFunction>
void Transition::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);	
	
	
	transfer.Transfer(m_SrcState, "m_SrcState", kStrongPPtrMask);
	transfer.Transfer(m_DstState, "m_DstState", kStrongPPtrMask);
	
	TRANSFER(m_TransitionDuration);
	TRANSFER(m_TransitionOffset);		
	TRANSFER(m_Conditions);	
	
	TRANSFER(m_Atomic);
	TRANSFER(m_Solo);
	TRANSFER(m_Mute);


	if (transfer.IsReadingBackwardsCompatible())
	{
		UnityStr m_UserName;
		TRANSFER(m_UserName);

		if(m_UserName != "")
		{
			m_Name.assign (m_UserName.c_str(), GetMemoryLabel());
		}
	}
}

Transition::Transition(MemLabelId label, ObjectCreationMode mode): 
NamedObject(label, mode),
m_TransitionDuration(0.1f),
m_TransitionOffset(0.0f),
m_Atomic(true),
m_Solo(false),
m_Mute(false),
m_UserList(this)
{
	m_Conditions.push_back(Condition());
}

Transition::~Transition()
{
}

void Transition::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
	
	INVALIDATE();
}


void Transition::SetName (char const* name)
{
	if (strcmp (m_Name.c_str (), name) != 0)
	{
		Super::SetName(name);
		DIRTY_AND_INVALIDATE();
	}	
}


std::string Transition::GetDisplayName(bool shortName) const
{
	if(m_Name != "")
		return m_Name.c_str();
	return GetUniqueName(shortName);
}

std::string Transition::GetUniqueName(bool shortName) const
{	
	if(shortName)		
		return BuildTransitionName(m_SrcState ? m_SrcState->GetName() : "AnyState" , m_DstState ? m_DstState->GetName() : "AnyState" );
	else
		return BuildTransitionName(m_SrcState ? m_SrcState->GetUniqueName() : "AnyState" , m_DstState ? m_DstState->GetUniqueName() : "AnyState" );
}


int	Transition::GetUniqueNameHash() const
{
	return  mecanim::processCRC32 (GetUniqueName().c_str());	
}



void Transition::SetSrcState(State* state) 
{
	m_SrcState = state; 
	DIRTY_AND_INVALIDATE();
}

State* Transition::GetSrcState() 
{
	return m_SrcState ;
}

void Transition::SetDstState(State* state) 
{
	if(m_DstState != PPtr<State>(state)) 
	{
		m_DstState = state; 
		DIRTY_AND_INVALIDATE();
	}
}

State* Transition::GetDstState()
{
	return m_DstState ;
}

int Transition::GetConditionCount()
{ 
	return m_Conditions.size(); 
}

void Transition::AddCondition()
{ 
	m_Conditions.push_back(Condition());
	DIRTY_AND_INVALIDATE();

}

void Transition::RemoveCondition(int index)
{ 
	if(ValidateConditionIndex(index))
	{
		m_Conditions.erase(m_Conditions.begin() + index); 
		DIRTY_AND_INVALIDATE();
	}
	
}
	
std::string Transition::GetConditionParameter(int index)const
{
	if(ValidateConditionIndex(index))
		return m_Conditions[index].m_ConditionEvent;
	
	return "";

}

void Transition::SetConditionParameter(int index, std::string const& event)
{
	if(ValidateConditionIndex(index))
	{
		if(m_Conditions[index].m_ConditionEvent != event)
		{
			m_Conditions[index].m_ConditionEvent = event; 
			DIRTY_AND_INVALIDATE();
		}
	}
}		

float Transition::GetParameterTreshold(int index)const
{
	if(ValidateConditionIndex(index))
		return m_Conditions[index].m_EventTreshold;
	else
		return 0;
}

void Transition::SetParameterTreshold(int index, float threshold)
{
	if(ValidateConditionIndex(index))
	{
		if(m_Conditions[index].m_EventTreshold != threshold)
		{
			m_Conditions[index].m_EventTreshold = threshold; 
			DIRTY_AND_INVALIDATE();
		}
	}
}

float Transition::GetExitTime(int index)const
{
	if(ValidateConditionIndex(index))
		return m_Conditions[index].m_ExitTime;
	
	return 0;
}

void Transition::SetExitTime(int index, float exit)
{
	if(ValidateConditionIndex(index))
	{
		exit = exit > 0 ? exit : 0;	
		if(!CompareApproximately(m_Conditions[index].m_ExitTime, exit))
		{
			m_Conditions[index].m_ExitTime = exit;  
			DIRTY_AND_INVALIDATE();
		}
	}
}

TransitionConditionMode Transition::GetConditionMode(int index)const
{
	if(ValidateConditionIndex(index))
		return (TransitionConditionMode)m_Conditions[index].m_ConditionMode;
	else
		return (TransitionConditionMode)0;
}

void Transition::SetConditionMode(int index, TransitionConditionMode mode)
{
	if(ValidateConditionIndex(index))
	{
		if(m_Conditions[index].m_ConditionMode != mode)
		{
			m_Conditions[index].m_ConditionMode = mode;  
			DIRTY_AND_INVALIDATE();
		}
	}
}

float Transition::GetTransitionDuration() const
{
	return m_TransitionDuration;
}

void Transition::SetTransitionDuration(float duration)
{
	duration = duration > 0 ? duration : 0; 	 

	if(!CompareApproximately(m_TransitionDuration, duration))
	{
		m_TransitionDuration = duration;
		DIRTY_AND_INVALIDATE();
	}
}

float Transition::GetTransitionOffset() const
{
	return m_TransitionOffset;
}

void Transition::SetTransitionOffset(float offset)
{
	offset = offset > 0 ? offset : 0;	
	offset = offset < 1 ? offset : 1;	

	if(!CompareApproximately(m_TransitionOffset, offset))
	{
		m_TransitionOffset = offset;
		DIRTY_AND_INVALIDATE();
	}
}

bool Transition::GetAtomic() const
{
	return m_Atomic;
}

void Transition::SetAtomic(bool atomic)
{
	if(m_Atomic != atomic)
	{
		m_Atomic = atomic;
		DIRTY_AND_INVALIDATE();
	}
}

bool Transition::GetSolo() const
{
	return m_Solo;
}

void Transition::SetSolo(bool solo)
{
	if(m_Solo != solo)
	{
		m_Solo = solo;
		DIRTY_AND_INVALIDATE();
	}
}

bool Transition::GetMute() const
{
	return m_Mute;
}

void Transition::SetMute(bool mute)
{
	if(m_Mute != mute)
	{
		m_Mute = mute;
		DIRTY_AND_INVALIDATE();
	}
}

bool Transition::ValidateConditionIndex(int index) const
{
	if(index >= 0 && index < m_Conditions.size())
	{
		return true;	
	}
	
	ErrorString("Invalid Condition index");
	return false;
	
}


IMPLEMENT_CLASS (State)
IMPLEMENT_OBJECT_SERIALIZE (State)
template<class TransferFunction>
void State::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);	
	
	TRANSFER(m_Speed);	
	TRANSFER(m_CycleOffset);	

	transfer.Transfer(m_Motions, "m_Motions", kStrongPPtrMask);

	TRANSFER(m_ParentStateMachine);	

	TRANSFER(m_Position);

	TRANSFER(m_IKOnFeet);
	TRANSFER(m_Mirror);
	transfer.Align();
	

	if (transfer.IsReadingBackwardsCompatible())
	{
		UnityStr m_ShortName;
		TRANSFER(m_ShortName);

		if(!m_ShortName.empty())
		{
			m_Name.assign (m_ShortName.c_str(), GetMemoryLabel());
		}
	}
	
	TRANSFER(m_Tag);
}


State::State(MemLabelId label, ObjectCreationMode mode)
:NamedObject(label, mode),
m_Speed(1.f),
m_CycleOffset(0),
m_Position(50,50,0),
m_IKOnFeet(false),
m_Mirror(false),
m_Tag(""),
m_UserList(this)
{
}

State::~State()
{		
}

void State::CheckConsistency()
{
	Super::CheckConsistency();

	for(int i = 0 ; i < m_Motions.size(); i++)
	{	
		if(!m_Motions[i].IsNull())
		{
			if(!m_Motions[i]->ValidateIfRetargetable())
			{
				m_Motions[i] = 0;
				DIRTY_AND_INVALIDATE();							
			}
		}		
	}

}

void State::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
		
	
	INVALIDATE ();
}

void State::SetName (char const* name)
{
	string uniqueName = m_ParentStateMachine->MakeUniqueStateName(name, this);
	if (strcmp (m_Name.c_str (), uniqueName.c_str()) != 0)
	{
		Super::SetName(uniqueName.c_str());
		DIRTY_AND_INVALIDATE();
	}	
}

std::string State::GetUniqueName() const
{	
	string ret = "";
	if(m_ParentStateMachine)
	{
		ret += m_ParentStateMachine->GetName();
		ret += ".";
	}
	ret += GetName();
	return ret;
}

int	State::GetUniqueNameHash() const
{
	return  mecanim::processCRC32 (GetUniqueName().c_str());
}

Motion*	State::GetMotion(int motionSetIndex)
{
	if(ValidateMotionSetIndex(motionSetIndex))	
		return m_Motions[motionSetIndex];

	return 0;
}

void State::SetMotion(int motionSetIndex, Motion* motion)
{	
	if (!GetBuildSettings().hasAdvancedVersion && motionSetIndex > 0)
	{
		ErrorString("Sync Layer is only supported in Unity Pro.");
		return;
	}

	if(motion && !motion->ValidateIfRetargetable())
		motion = 0;
	
	if(ValidateMotionSetIndex(motionSetIndex))	
	{	
		if(m_Motions[motionSetIndex]!= PPtr<Motion>(motion))
		{
			m_Motions[motionSetIndex] = motion;
			DIRTY_AND_INVALIDATE();
		}
	}
}


bool State::GetIKOnFeet() const
{
	return m_IKOnFeet;

}
void State::SetIKOnFeet(bool ikOnFeet)
{
	if(ikOnFeet != m_IKOnFeet)
	{
		m_IKOnFeet= ikOnFeet;
		DIRTY_AND_INVALIDATE();
	}
}

float State::GetSpeed() const 
{
	return m_Speed;
}

void State::SetSpeed(float speed)
{
	if(m_Speed != speed)
	{
		m_Speed = speed;
		DIRTY_AND_INVALIDATE();
	}
}

bool State::GetMirror() const 
{
	return m_Mirror;
}

void State::SetMirror(bool mirror)
{
	if(mirror != m_Mirror)
	{
		m_Mirror = mirror;
		DIRTY_AND_INVALIDATE();
	}
}

std::string State::GetTag() const
{
	return m_Tag;
}

void State::SetTag(const string& tag)
{
	if(m_Tag != tag)
	{
		m_Tag = tag;
		DIRTY_AND_INVALIDATE();
	}

}

float State::GetCycleOffset() const 
{
	return m_CycleOffset;
}

BlendTree*	State::CreateBlendTree(int motionSetIndex)
{
	if(ValidateMotionSetIndex(motionSetIndex))	
	{
		BlendTree* blendTree = CreateObjectFromCode<BlendTree>();					
		blendTree->SetHideFlags( this->TestHideFlag(kDontSave) ? kHideInHierarchy | kHideInspector | kDontSave :  kHideInHierarchy | kHideInspector);
		if(IsPersistent()) 
			AddAssetToSameFile(*blendTree, *this, true);	

		m_Motions[motionSetIndex] = blendTree;

		DIRTY_AND_INVALIDATE();

		return blendTree;
	}

	return 0;
}

StateMachine* State::GetStateMachine()
{
	return m_ParentStateMachine;
}


void State::AddMotionSet()
{
	m_Motions.push_back(PPtr<Motion>(0));
}

void State::RemoveMotionSet(int index)
{
	if(ValidateMotionSetIndex(index))
		m_Motions.erase(m_Motions.begin() + index);	
}

BlendParameterList State::CollectParameters()
{
	BlendParameterList ret;
	for(MotionVector::const_iterator it = m_Motions.begin() ; it  != m_Motions.end() ;it++)
	{
		Motion const* motion = (*it);
		if(motion)
		{
			BlendTree *blendTree = dynamic_pptr_cast<BlendTree*>(motion);

			if(blendTree)
			{
				blendTree->GetParameterList(ret);
			}
		}			
	}

	return ret;
}

bool State::ValidateMotionSetIndex(int index) const
{
	if(index >= 0 && index < m_Motions.size())
	{		
		return true;
	}

	ErrorString("Invalid MotionSet index");
	return false;	
}

StateMachine::StateMachine(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode),
m_AnyStatePosition (50, 20, 0),
m_ParentStateMachinePosition (800, 20, 0),
m_UserList(this)
{	
	m_MotionSetCount = 1;	
}

StateMachine::~StateMachine()
{	

}


void StateMachine::SetName (char const* name)
{
	string uniqueName = GetRootStateMachine(this)->MakeUniqueStateMachineName(name, this);	
	if (strcmp (m_Name.c_str (), uniqueName.c_str()) != 0)
	{
		Super::SetName(uniqueName.c_str());
		DIRTY_AND_INVALIDATE();
	}	
}

void StateMachine::InitializeClass ()
{
}

void StateMachine::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
	
	INVALIDATE ();
}

void StateMachine::CheckConsistency()
{	
	Super::CheckConsistency(); 
	bool dirty_and_invalidate = false;

	// removed delete states (can appear after undo)
	StateVector toRemove;	
	for(StateVector::const_iterator it = m_States.begin() ; it != m_States.end() ; it++)
	{
		if(it->IsNull())
			toRemove.push_back(*it);
	}
	for(StateVector::const_iterator it = toRemove.begin() ; it != toRemove.end(); it++)
	{	
		m_States.erase(std::remove(m_States.begin(), m_States.end(), *it));		
		dirty_and_invalidate = true;
	}
	toRemove.clear();

	for(TransitionMap::iterator itState = m_OrderedTransitions.begin() ; itState != m_OrderedTransitions.end(); ++itState)
	{	
		TransitionVector transitionToRemove;
		for(TransitionVector::const_iterator itTransition = itState->second.begin(); itTransition != itState->second.end(); ++itTransition)
		{
			if(itTransition->GetInstanceID() == 0 || itTransition->IsNull() )
				transitionToRemove.push_back(*itTransition);			
		}		

		for(TransitionVector::iterator itTransition = transitionToRemove.begin() ; itTransition != transitionToRemove.end(); ++itTransition)
		{
			TransitionVector::iterator itErase = std::remove(itState->second.begin(), itState->second.end(), *itTransition);
			itState->second.erase(itErase, itState->second.end());

			dirty_and_invalidate = true;			
		}
	}
	
	// removed delete stateMachines (can appear after undo)
	StateMachineVector stateMachinesToRemove;	
	for(StateMachineVector::const_iterator it = m_ChildStateMachine.begin() ; it != m_ChildStateMachine.end() ; it++)
	{
		if(it->IsNull())
			stateMachinesToRemove.push_back(*it);
	}
	for(StateMachineVector::const_iterator it = stateMachinesToRemove.begin() ; it != stateMachinesToRemove.end(); it++)
	{	
		m_ChildStateMachine.erase(std::remove(m_ChildStateMachine.begin(), m_ChildStateMachine.end(), *it));		
		dirty_and_invalidate = true;
	}
	stateMachinesToRemove.clear();

	for(StateMachineVector::iterator it = m_ChildStateMachine.begin() ; it != m_ChildStateMachine.end() ; ++it)
	{
		(*it)->SetParentStateMachine(this);
	}
	
	if(dirty_and_invalidate) 
		DIRTY_AND_INVALIDATE();
}

IMPLEMENT_OBJECT_SERIALIZE (StateMachine)
IMPLEMENT_CLASS_HAS_INIT (StateMachine)


template<class T>
void StateMachine::AnyStateTransitionsBackwardsCompatibility(T& transfer)
{
	/// This was introduced and removed during 4.0 Alpha-Beta
	if (transfer.IsVersionSmallerOrEqual (1))
	{
		TransitionVector m_AnyStateTransitions;
		transfer.Transfer (m_AnyStateTransitions,"m_AnyStateTransitions", kHideInEditorMask|kStrongPPtrMask);			

		if(m_AnyStateTransitions.size() > 0)
		{
			m_OrderedTransitions.erase(0);

			for(TransitionVector::const_iterator it = m_AnyStateTransitions.begin() ; it < m_AnyStateTransitions.end(); it++)
			{
				if(!(*it).IsNull())
					AddTransition(*it, false);		
			}
		}
	}
}

template<class TransferFunction>
void StateMachine::Transfer (TransferFunction& transfer)
{
	transfer.SetVersion(2);

	Super::Transfer (transfer);	
	
	transfer.Transfer (m_DefaultState,"m_DefaultState",kHideInEditorMask|kStrongPPtrMask);
	transfer.Transfer (m_States,"m_States",kHideInEditorMask|kStrongPPtrMask);	
	transfer.Transfer (m_ChildStateMachine,"m_ChildStateMachine",kHideInEditorMask|kStrongPPtrMask);	
	transfer.Transfer (m_ChildStateMachinePosition,"m_ChildStateMachinePosition",kHideInEditorMask);		
	transfer.Transfer (m_OrderedTransitions, "m_OrderedTransitions",  kHideInEditorMask|kStrongPPtrMask);
		
	transfer.Transfer (m_MotionSetCount, "m_MotionSetCount", kHideInEditorMask);

	transfer.Transfer (m_AnyStatePosition, "m_AnyStatePosition", kHideInEditorMask);
	transfer.Transfer (m_ParentStateMachinePosition, "m_ParentStateMachinePosition", kHideInEditorMask);

	AnyStateTransitionsBackwardsCompatibility(transfer);
	


}

void StateMachine::GetAnimationClips(AnimationClipVector& clips, int motionSetIndex)
{	
	StateVector allStates;
	CollectAllStates(allStates, this);

	
	for(int i = 0 ; i < allStates.size() ; i++)
	{
		State* currentState = allStates[i];
		
		Motion* motion = currentState->GetMotion(motionSetIndex);		
		BlendTree* tree  = dynamic_pptr_cast<BlendTree*>(motion);
		AnimationClip* clip = dynamic_pptr_cast<AnimationClip*>(motion);
				
		if (tree)
		{
			AnimationClipVector treeClips = tree->GetAnimationClips();
			clips.insert(clips.end(), treeClips.begin(), treeClips.end());			
		}		
		else if(clip)
		{
			clips.push_back(clip);		
		}		
	}			
}

void StateMachine::GetBlendTrees(BlendTreeVector& trees)
{
	StateVector allStates;
	CollectAllStates(allStates, this);

	for(int i = 0 ; i < allStates.size() ; i++)
	{
		State* currentState = allStates[i];		
		for(int j = 0 ; j < GetMotionSetCount() ; j++)
		{
			BlendTree* tree  = dynamic_pptr_cast<BlendTree*>(currentState->GetMotion(j));		
			if (tree)
			{
				trees.push_back(tree);
			}
		}
	}
}

State* StateMachine::AddState(const std::string& name)
{	
	State* state = CreateObjectFromCode<State>();		
	for(int i = 0 ; i < GetMotionSetCount(); i++)
		state->AddMotionSet();

	AddState(state);	
	state->SetName(name.c_str());

	return state;
}

void StateMachine::AddState(State* state)
{
	state->m_ParentStateMachine = this;	
	state->SetName(state->GetName());
	
	m_States.push_back(state);
	
	state->SetHideFlags( this->TestHideFlag(kDontSave) ? kHideInHierarchy | kHideInspector | kDontSave :  kHideInHierarchy | kHideInspector);
	if(IsPersistent()) 
		AddAssetToSameFile(*state, *this, true);			
	
	
	if(DefaultState() == 0) // 1st state added make default
		SetDefaultState(state);

	DIRTY_AND_INVALIDATE();
}


bool StateMachine::RemoveState(State* state)
{		
	RemoveAllTransitionTo(state);
	RemoveAllTransitionFrom(state);

	if(!RemoveStateFromArray(state))
	{		
		for(int i = 0 ; i < GetStateMachineCount(); i++)
		{
			if(GetStateMachine(i)->RemoveState(state))
				return true;
		}

		return false;
	}

	return true;
}

int	StateMachine::GetStateCount()const
{
	return m_States.size();
}

State* StateMachine::GetState(int i)const
{
	if(ValidateStateIndex(i))
		return m_States[i];
	else 
		return 0;
}


void StateMachine::AddTransition(Transition *transition, bool noDirty)
{
	if(transition->GetSrcState() == transition->GetDstState()) return;

	StateMachine* rootStateMachine = GetRootStateMachine(this);					
	rootStateMachine->AddOrderedTransition(transition);			
}

static bool GetShouldBlendTransition (Motion* motion)
{
	AnimationClip* clip = dynamic_pptr_cast<AnimationClip*> (motion);
	if (clip == NULL)
		return true;
	AnimationClipStats stats;
	clip->GetStats (stats);
	
	// If we only have PPtr Curves (for example sprites)
	// Then we dont want to create a transition that blends. We just want to switch to the new clip immediately.
	if (stats.totalCurves == stats.pptrCurves && stats.pptrCurves > 0)
		return false;
	
	return true;
}

Transition* StateMachine::AddTransition(State* src, State* dst)
{
	Transition* transition = CreateObjectFromCode<Transition>();			
	transition->SetHideFlags( this->TestHideFlag(kDontSave) ? kHideInHierarchy | kHideInspector | kDontSave :  kHideInHierarchy | kHideInspector);
	if(IsPersistent()) 
		AddAssetToSameFile(*transition, *this, true);
	
	
	transition->SetSrcState(src);	
	transition->SetDstState(dst);	

	AddTransition(transition);
		
	if(src && src->GetMotion(0)) 
	{	
		float averageDuration, transitionDurationNormalized;
		// Default transition blending
		if (GetShouldBlendTransition (src->GetMotion(0)))
		{
			averageDuration = src->GetMotion(0)->GetAverageDuration();
			transitionDurationNormalized = averageDuration > 0.f ? DefaultTransitionDurationInSeconds / averageDuration : DefaultTransitionDurationInSeconds;
		}
		// Sprite animations for example should not be blended but switch instantly
		else
		{
			averageDuration = 0.0F;
			transitionDurationNormalized = 0.0F;
		}
		
		transition->SetTransitionDuration( transitionDurationNormalized );
		transition->SetExitTime(0, 1-transitionDurationNormalized);
	}

	return transition;

}


void StateMachine::RemoveTransition(Transition* transition, bool deleteTransition)
{	
	if(m_ParentStateMachine.IsNull())
	{
		if(transition)
		{							
			TransitionMap::iterator it = m_OrderedTransitions.find(transition->GetSrcState());
			if(it != m_OrderedTransitions.end())
			{
				TransitionVector::iterator transitionIt = std::remove(it->second.begin(), it->second.end(), PPtr<Transition>(transition));
				it->second.erase(transitionIt, it->second.end());	

				if(deleteTransition)
					DestroySingleObject(transition);

				DIRTY_AND_INVALIDATE();
			}		
		}	
	}
	else 
	{
		m_ParentStateMachine->RemoveTransition(transition, deleteTransition);
	}

}


StateMachine* StateMachine::AddStateMachine(const std::string& name)
{
	StateMachine* stateMachine = CreateObjectFromCode<StateMachine>();		
	stateMachine->SetName(name.c_str());
	AddStateMachine(stateMachine);	
	
	return stateMachine;
}

void StateMachine::AddStateMachine(StateMachine* stateMachine)
{
	
	stateMachine->SetHideFlags( this->TestHideFlag(kDontSave) ? kHideInHierarchy | kHideInspector | kDontSave :  kHideInHierarchy | kHideInspector);
	if(IsPersistent()) 
		AddAssetToSameFile(*stateMachine, *this, true);		
		
	StateVector allStates;
	CollectAllStates(allStates, stateMachine); // subStatemachine states are here also
	
	StateMachine *rootStateMachine = GetRootStateMachine(this);
	
	// AnyState transition
	TransitionVector localTransitions = stateMachine->GetOrderedTransitionsFromState(0);
	for(TransitionVector::iterator it = localTransitions.begin() ; it !=  localTransitions.end() ; it++)
		rootStateMachine->AddOrderedTransition(*it);	

	for(int i = 0 ; i < allStates.size() ; i++)
	{
		State* currentState = allStates[i];
		TransitionVector localTransitions = stateMachine->GetOrderedTransitionsFromState(currentState);

		for(TransitionVector::iterator it = localTransitions.begin() ; it !=  localTransitions.end() ; it++)
			rootStateMachine->AddOrderedTransition(*it);			
	}

	for(int i = 0 ; i < GetMotionSetCount(); i++)
		stateMachine->AddMotionSet();

	StateMachineVector allStateMachines;
	CollectAllSubStateMachines(allStateMachines, stateMachine);
	for(StateMachineVector::iterator it = allStateMachines.begin(); it != allStateMachines.end(); it++)
	{
		(*it)->SetName((*it)->GetName()); // corrects stateMachine names to make them unique.
	}

	stateMachine->SetParentStateMachine(this);
	stateMachine->SetName(stateMachine->GetName());// make sure name is unique
	m_ChildStateMachine.push_back(stateMachine);
	Vector3f defaultPos(50,50,0); 
	m_ChildStateMachinePosition.push_back(defaultPos);
	
	DIRTY_AND_INVALIDATE();		
}



bool StateMachine::RemoveStateMachineFromArray(StateMachine *stateMachine, bool deleteStateMachine)
{
	PPtr<StateMachine> stateMachinePtr(stateMachine);	
	int index = std::distance(m_ChildStateMachine.begin(), std::find(m_ChildStateMachine.begin(), m_ChildStateMachine.end(), stateMachinePtr));
	if(index < m_ChildStateMachine.size())
	{
		StateMachineVector::iterator it = std::remove(m_ChildStateMachine.begin(), m_ChildStateMachine.end(), stateMachinePtr);					
		m_ChildStateMachine.erase(it, m_ChildStateMachine.end());
		m_ChildStateMachinePosition.erase(m_ChildStateMachinePosition.begin()+index);

		if(deleteStateMachine)
			DestroySingleObject(stateMachine);		

		DIRTY_AND_INVALIDATE();	
		return true;
	}
	else
	{
		for(int i = 0 ; i <  GetStateMachineCount() ; i++)
		{
			if(GetStateMachine(i)->RemoveStateMachineFromArray(stateMachine, deleteStateMachine))
				return true;
		}
	}
	
	return false;
}


bool StateMachine::RemoveStateMachine(StateMachine* stateMachine)
{
	if(stateMachine)
	{
		PPtr<StateMachine> stateMachinePtr(stateMachine);	

		StateVector allStates;
		CollectAllStates(allStates, stateMachine);

		for(int i = 0 ; i <  allStates.size(); i++)
		{
			RemoveState(allStates[i]);			
		}
		
		for(int i = 0 ; i <  stateMachine->GetStateMachineCount() ; i++)
		{
			stateMachine->RemoveStateMachine(stateMachine->GetStateMachine(i));			
		}
		
		

		if(!RemoveStateMachineFromArray(stateMachine))
		{				
			for(int i = 0 ; i <  GetStateMachineCount() ; i++)
			{
				if(GetStateMachine(i)->RemoveStateMachine(stateMachine))
					return true;
			}		
		}
		
		return true;
	}

	return false;
}

int StateMachine::GetStateMachineCount()const
{
	return m_ChildStateMachine.size();
}

StateMachine* StateMachine::GetStateMachine(int i)const
{
	if(ValidateStateMachineIndex(i))
		return m_ChildStateMachine[i];
	else 
		return NULL;
}

StateMachine* StateMachine::GetParentStateMachine() const
{
	return m_ParentStateMachine;
}

void StateMachine::SetParentStateMachine( StateMachine * stateMachine)
{
	m_ParentStateMachine = stateMachine;
}

void StateMachine::SetDefaultState(State * state)
{		
	m_DefaultState = state; 	
	DIRTY_AND_INVALIDATE();	
}


bool StateMachine::HasState(const State *state, bool recursive) const
{
	if(state == 0) return true;
	StateVector::const_iterator it = std::find(m_States.begin(), m_States.end(), PPtr<State>(state));
	if(it != m_States.end())
		return true;
	
	if(recursive)
	{
		for(int i = 0 ; i < GetStateMachineCount(); i++)
		{
			if(GetStateMachine(i)->HasState(state,recursive))
				return true;
		}
	}
	return false;
}

bool StateMachine::HasStateMachine(const StateMachine *stateMachine, bool recursive) const
{
	StateMachineVector::const_iterator it = std::find(m_ChildStateMachine.begin(), m_ChildStateMachine.end(), PPtr<StateMachine>(stateMachine));
	if(it != m_ChildStateMachine.end())
		return true;

	if(recursive)
	{
		for(int i = 0 ; i < GetStateMachineCount(); i++)
		{
			if(GetStateMachine(i)->HasStateMachine(stateMachine,recursive))
				return true;
		}
	}
	return false;

}

int	StateMachine::GetTransitionCount()
{
	int count = 0;
	
	for(TransitionMap::iterator it = m_OrderedTransitions.begin() ; it !=  m_OrderedTransitions.end(); it++)
	{
		count += it->second.size();
	}
	return count;
}

void StateMachine::MoveState(State *state, StateMachine *targetStateMachine)
{	
	TransitionVector transitions = GetOrderedTransitionsFromState(state);
	TransitionVector toTransitions = GetTransitionsToState(state);
	transitions.insert(transitions.end(), toTransitions.begin(), toTransitions.end());

	for(TransitionVector::iterator it = transitions.begin(); it != transitions.end() ; it++)
	{
		RemoveTransition(*it, false);
	}

	state->GetStateMachine()->RemoveStateFromArray(state,false);	
	GetPersistentManager ().MakeObjectUnpersistent (state->GetInstanceID (), kDestroyFromFile);	
	targetStateMachine->AddState(state);

	//to be sure transitions are added in 1st common parent
	for(TransitionVector::iterator it = transitions.begin(); it != transitions.end() ; it++)
	{
		AddTransition(*it);
	}	
	
}

void StateMachine::MoveStateMachine(StateMachine *stateMachine, StateMachine *targetStateMachine)
{		
	RemoveStateMachineFromArray(stateMachine, false);	
	GetPersistentManager ().MakeObjectUnpersistent (stateMachine->GetInstanceID (), kDestroyFromFile);	
	targetStateMachine->AddStateMachine(stateMachine); 

	/// Move states to be sure transitions are added in 1st common parent
	StateVector states;
	CollectAllStates(states,stateMachine);
	for(StateVector::iterator it = states.begin(); it != states.end(); it++)
	{
		MoveState(*it, stateMachine); 
	}
	
}



bool StateMachine::RemoveStateFromArray(State *state, bool deleteState)
{
	PPtr<State> statePtr = state;
	if(std::find(m_States.begin(), m_States.end(), statePtr) != m_States.end())
	{
		StateVector::iterator it = std::remove(m_States.begin(), m_States.end(), statePtr);
		m_States.erase(it, m_States.end());	

		if(deleteState)
		{
			DestroySingleObject(state);			
		}
		DIRTY_AND_INVALIDATE();
		return true;
	}

	return false;
}


string StateMachine::MakeUniqueStateName(const string& newName, State const* target) const
{
	string attemptName = newName;
	int attempt = 0;
	while (true)
	{
		int i = 0;
		for (i = 0; i < GetStateCount(); i++)
		{
			if (GetState(i) != target && attemptName == GetState(i)->GetName())
			{
				attemptName = newName;
				attemptName += Format(" %d", attempt);
				attempt++;
				break;
			}
		}
		if (i == GetStateCount())
			break;
	}

	return attemptName;
}


string StateMachine::MakeUniqueStateMachineName(const string& newName, StateMachine const * target) const
{
	string attemptName = newName;
	int attempt = 0;

	StateMachineVector allStateMachines;
	allStateMachines.push_back(this);
	CollectAllSubStateMachines(allStateMachines, this);
	while (true)
	{
		int i = 0;
		for (i = 0; i < allStateMachines.size(); i++)
		{
			if (allStateMachines[i] != PPtr<StateMachine>(target) && attemptName == allStateMachines[i]->GetName())
			{
				attemptName = newName;
				attemptName += Format(" %d", attempt);
				attempt++;
				break;
			}
		}
		if (i == allStateMachines.size())
			break;
	}

	return attemptName;
}

BlendParameterList StateMachine::CollectParameters()
{
	BlendParameterList ret;
	StateVector states;
	CollectAllStates(states, this);

	for(StateVector::const_iterator it = states.begin() ; it < states.end(); it++)
	{
		BlendParameterList stateParameters = (*it)->CollectParameters();
				
		for(BlendParameterList::const_iterator parameter = stateParameters.begin(); parameter != stateParameters.end(); parameter++)
		{
			if(std::find(ret.begin(), ret.end(), *parameter) == ret.end())
			{
				ret.push_back(*parameter);				
			}
		}
	}

	for(TransitionMap::const_iterator it = m_OrderedTransitions.begin() ;  it != m_OrderedTransitions.end() ; it++)
	{
		for(TransitionVector::const_iterator transitionIt = it->second.begin() ; transitionIt != it->second.end(); transitionIt++)
		{
			int conditionCount = (*transitionIt)->GetConditionCount();
			for(int i = 0 ; i < conditionCount; i++)
			{
				if((*transitionIt)->GetConditionMode(i) != kConditionModeExitTime)
				{
					string blendParameter = (*transitionIt)->GetConditionParameter(i);
					if(std::find(ret.begin(), ret.end(), blendParameter) == ret.end())
					{
						ret.push_back(blendParameter);
					}
				}
			}
		}
	}

	return ret;

}

std::vector<PPtr<Object> > StateMachine::CollectObjectsUsingParameter(const string& parameterName)
{
	std::vector<PPtr<Object> > ret;
	StateVector states;
	CollectAllStates(states, this);

	for(StateVector::iterator it = states.begin() ; it < states.end(); it++)
	{
		BlendParameterList stateParameters = (*it)->CollectParameters();

		for(BlendParameterList::const_iterator parameter = stateParameters.begin(); parameter != stateParameters.end(); parameter++)
		{
			if(*parameter == parameterName)
			{
				ret.push_back(PPtr<Object>(*it));
			}
		}
	}

	for(TransitionMap::const_iterator it = m_OrderedTransitions.begin() ;  it != m_OrderedTransitions.end() ; it++)
	{
		for(TransitionVector::const_iterator transitionIt = it->second.begin() ; transitionIt != it->second.end(); transitionIt++)
		{
			int conditionCount = (*transitionIt)->GetConditionCount();
			for(int i = 0 ; i < conditionCount; i++)
			{
				if((*transitionIt)->GetConditionMode(i) != kConditionModeExitTime)
				{
					string blendParameter = (*transitionIt)->GetConditionParameter(i);

					if(blendParameter == parameterName)
						ret.push_back(PPtr<Object>(*transitionIt));					
				}
			}
		}
	}

	return ret;

}

mecanim::statemachine::StateMachineConstant* StateMachine::BuildRuntimeAsset(UserList& dependencies, TOSVector& tos, int layerIndex, mecanim::memory::Allocator& alloc)const
{
	dependencies.AddUser(m_UserList);
	
	std::vector<mecanim::statemachine::StateConstant*> stateConstant;
				
	StateVector allStates;
	CollectAllStates(allStates, this);
	

	for(unsigned int i = 0 ; i < allStates.size() ; ++i)
	{
		State& state = *allStates[i];

		dependencies.AddUser(state.GetUserList());
		
		std::vector<mecanim::statemachine::TransitionConstant*> transitionConstants;
		TransitionMap::const_iterator it = m_OrderedTransitions.find(PPtr<State>(&state));

		bool looping = true;

		if(it!= m_OrderedTransitions.end())
		{
			bool hasSolo = false;
			for(unsigned int j = 0 ; j < it->second.size(); j++)
			{
				if(it->second[j]->GetSolo())
					hasSolo = true;
			}

			for(unsigned int j = 0 ; j < it->second.size(); j++)
			{	
				Transition& transition = *it->second[j];
				dependencies.AddUser(transition.GetUserList());
				bool buildTransition = transition.GetMute() ? false : hasSolo ? transition.GetSolo() ? true : false : true;
				
				if(buildTransition)
				{
					mecanim::statemachine::TransitionConstant* currentTransition = BuildTransitionConstant(transition, dependencies, allStates, tos, alloc);

					if(currentTransition)				
						transitionConstants.push_back(currentTransition);			
				}
			}
		}
		
			
		std::vector<mecanim::animation::BlendTreeConstant*> blendTreeConstantVector;
		blendTreeConstantVector.resize(GetMotionSetCount());

		for(int j = 0 ; j < GetMotionSetCount() ; j++)
		{
			Motion *motion =  state.GetMotion(j);
					
			AnimationClip* clip  = dynamic_pptr_cast<AnimationClip*>(motion);
			BlendTree* tree  = dynamic_pptr_cast<BlendTree*>(motion);
					
			if(clip)
			{					
				std::string path = clip->GetName();
				mecanim::uint32_t clipID = ProccessString(tos, path);					
				blendTreeConstantVector[j] = mecanim::animation::CreateBlendTreeConstant(clipID, alloc);

				if(j == 0)
					looping &= clip->GetAnimationClipSettings().m_LoopTime;
			}
			else if(tree)
			{			
				blendTreeConstantVector[j] = tree->BuildRuntimeAsset(dependencies, tos, alloc);

				if(j==0)
				{
					AnimationClipVector allClips = tree->GetAnimationClips();
					for(AnimationClipVector::const_iterator clipsIt = allClips.begin() ; clipsIt < allClips.end() ; clipsIt++)
						looping &= (*clipsIt)->GetAnimationClipSettings().m_LoopTime;
				}
			}		
			else
			{
				blendTreeConstantVector[j] = 0;
			}			
		}
	
		
		mecanim::statemachine::TransitionConstant** transitionConstantArray = transitionConstants.size() ? &transitionConstants.front() : 0;
					
		
		mecanim::uint32_t stateNameID = ProccessString(tos, state.GetName());
		mecanim::uint32_t statePathID = ProccessString(tos, state.GetUniqueName());
		mecanim::uint32_t tagID = ProccessString(tos, state.GetTag());


		mecanim::animation::BlendTreeConstant** blendTreeConstant = blendTreeConstantVector.size() > 0 ? &blendTreeConstantVector[0] : 0 ;
			
		mecanim::statemachine::StateConstant* stateCst = mecanim::statemachine::CreateStateConstant(transitionConstantArray, transitionConstants.size(),
																						state.GetSpeed(), state.GetIKOnFeet(),state.GetMirror(),state.GetCycleOffset(),
																						blendTreeConstant, GetMotionSetCount(),																				
																						stateNameID, statePathID, tagID, looping, alloc);				
		stateConstant.push_back(stateCst);
	}
		

	std::vector<mecanim::statemachine::TransitionConstant*> anyTransitionConstant;	
	TransitionVector allAnyStateTransitions = GetOrderedTransitionsFromState(0);	
	bool hasSolo = false;
	for(int i = 0 ; i < allAnyStateTransitions.size() && !hasSolo; i++)
	{
		if(allAnyStateTransitions[i]->GetSolo())
			hasSolo = true;
	}
	for(int i = 0 ; i < allAnyStateTransitions.size(); i++)
	{
		dependencies.AddUser(allAnyStateTransitions[i]->GetUserList());
		bool buildTransition = allAnyStateTransitions[i]->GetMute() ? false : hasSolo ? allAnyStateTransitions[i]->GetSolo() ? true : false : true;

		if(buildTransition)
		{
			mecanim::statemachine::TransitionConstant* currentTransition = BuildTransitionConstant(*allAnyStateTransitions[i], dependencies, allStates, tos, alloc);
			if(currentTransition)				
				anyTransitionConstant.push_back(currentTransition);		
		}
	}	

		
	mecanim::statemachine::StateConstant** stateConstantArray = stateConstant.size() ? &stateConstant.front() : 0;	
	mecanim::statemachine::TransitionConstant** anyTransitionConstantArray = anyTransitionConstant.size() ? &anyTransitionConstant.front() : 0 ;	

	int detaultStateIndex = m_DefaultState ? GetStateIndex(allStates, m_DefaultState->GetUniqueName()) : 0;
	detaultStateIndex = math::clamp(detaultStateIndex, 0, static_cast<int>(allStates.size())); 

	return CreateStateMachineConstant(stateConstantArray, allStates.size(), detaultStateIndex, anyTransitionConstantArray, anyTransitionConstant.size(), GetMotionSetCount(), alloc); 
}


void StateMachine::RemoveAllTransitionTo(State *state)
{	
	if(m_ParentStateMachine.IsNull())
	{
		for(TransitionMap::const_iterator i = m_OrderedTransitions.begin() ; i !=  m_OrderedTransitions.end(); i++)
		{		
			for(unsigned j = 0 ; j < i->second.size(); j++)
			{
				if(i->second[j]->GetDstState() == state)
				{
					RemoveTransition(i->second[j]);
					j--;
				}
			}
		}
	}
	else
	{
		m_ParentStateMachine->RemoveAllTransitionTo(state);
	}
	
}

void StateMachine::RemoveAllTransitionFrom(State *state)
{
	TransitionVector transitions = GetOrderedTransitionsFromState(state);
	int transitionCount = transitions.size();
	for(int i = 0 ; i < transitionCount; i++)
	{
		RemoveTransition(transitions[i]);
	}			
}

mecanim::statemachine::TransitionConstant* StateMachine::BuildTransitionConstant(Transition& model, UserList& dependencies, StateVector const& allState, TOSVector& tos, mecanim::memory::Allocator& alloc)  const
{	
	std::vector<mecanim::statemachine::ConditionConstant*> conditions;	
	mecanim::uint32_t eventID = -1;		
	TransitionConditionMode conditionMode;

	for(int i = 0 ; i < model.GetConditionCount() ; i++)
	{
		conditionMode = model.GetConditionMode(i);
		if(conditionMode == kConditionModeIf || 
			conditionMode == kConditionModeIfNot ||
			conditionMode == kConditionModeGreater ||
			conditionMode == kConditionModeLess ||
			conditionMode == kConditionModeEquals || 
			conditionMode == kConditionModeNotEqual )
		{	
			eventID = ProccessString(tos, model.GetConditionParameter(i));
		}

		float eventThreshold		= model.GetParameterTreshold(i);
		float exitTime				= model.GetExitTime(i);

		conditions.push_back(mecanim::statemachine::CreateConditionConstant(conditionMode, eventID, eventThreshold, exitTime, alloc));
	}					
	
	mecanim::uint32_t transitionID = ProccessString(tos,  model.GetUniqueName());	
	mecanim::uint32_t transitionUserID = ProccessString(tos,  model.GetName());	

	mecanim::statemachine::ConditionConstant** conditionArray = conditions.size() > 0 ? &conditions[0] : 0;

	return mecanim::statemachine::CreateTransitionConstant(conditionArray, conditions.size(), 															
															GetStateIndex(allState, model.GetDstState()->GetUniqueName()), 
															model.GetTransitionDuration(), model.GetTransitionOffset(), 
															model.GetAtomic(), transitionID, transitionUserID, alloc);
}


void StateMachine::RenameParameter(const std::string& newName, const std::string& oldName)
{		
	StateVector allStates;
	CollectAllStates(allStates, this);
	
	for(int i = 0 ; i < allStates.size(); i++)
	{		
		for(int j = 0 ; j < GetMotionSetCount() ; j++)
		{	
			Motion* motion = allStates[i]->GetMotion(j);
			if(motion)
			{
				BlendTree* tree  = dynamic_pptr_cast<BlendTree*>(motion);
				if(tree)
				{
					tree->RenameParameter(newName, oldName);
				}
			}	
		}

		TransitionVector transitions = m_OrderedTransitions[allStates[i]];
		for(int j = 0 ; j < transitions.size(); j++)
		{
			Transition* transition = transitions[j]; 

			for(int k = 0 ; k < transition->GetConditionCount(); k++)
			{
				if(transition->GetConditionParameter(k) == oldName)			
					transition->SetConditionParameter(k, newName);
			}
		}
	}		
}

void StateMachine::AddFirstParameterOfType(const std::string& name, AnimatorControllerParameterType type)
{
	StateVector allStates;
	CollectAllStates(allStates, this);

	for(int i = 0 ; i < allStates.size(); i++)
	{		
		if(type ==  AnimatorControllerParameterTypeFloat)
		{		
			for(int j = 0 ; j < GetMotionSetCount() ; j++)
			{	
				Motion* motion = allStates[i]->GetMotion(j);
				if(motion)
				{
					BlendTree* tree  = dynamic_pptr_cast<BlendTree*>(motion);
					if(tree)
					{						
						tree->SetBlendParameter(name);
						tree->SetBlendParameterY(name);
					}
				}	
			}
		}

		TransitionVector transitions = m_OrderedTransitions[allStates[i]];
		for(int j = 0 ; j < transitions.size(); j++)
		{
			Transition* transition = transitions[j]; 

			for(int k = 0 ; k < transition->GetConditionCount(); k++)
			{				
				if(transition->GetConditionParameter(k) == "")
				{
					TransitionConditionMode mode = transition->GetConditionMode(k);
					if( ( type == AnimatorControllerParameterTypeBool && (mode == kConditionModeIf  || mode == kConditionModeIfNot)) || 
						( type == AnimatorControllerParameterTypeTrigger && mode == kConditionModeIf) ||
						( (type == AnimatorControllerParameterTypeFloat || type == AnimatorControllerParameterTypeInt) && (mode == kConditionModeGreater  || mode == kConditionModeLess)))
					{
						transition->SetConditionParameter(k, name);
					}					
				}						
			}
		}
	}		

}

void StateMachine::AddMotionSet()
{	
	m_MotionSetCount++;
	for(StateVector::iterator it = m_States.begin() ; it < m_States.end() ; it++)
		(*it)->AddMotionSet();

	for(StateMachineVector::iterator it = m_ChildStateMachine.begin() ; it < m_ChildStateMachine.end() ; it++)
		(*it)->AddMotionSet();

	DIRTY_AND_INVALIDATE();
	
}

void StateMachine::RemoveMotionSet(int index)
{	
	m_MotionSetCount--;
	for(StateVector::iterator it = m_States.begin() ; it < m_States.end() ; it++)
		(*it)->RemoveMotionSet(index);

	for(StateMachineVector::iterator it = m_ChildStateMachine.begin() ; it < m_ChildStateMachine.end() ; it++)
		(*it)->RemoveMotionSet(index);

	DIRTY_AND_INVALIDATE();
}
int	StateMachine::GetMotionSetCount() const
{
	return m_MotionSetCount;	
}

void StateMachine::FixStateParent()
{
	for(StateVector::iterator it = m_States.begin() ; it < m_States.end() ; it++)
		(*it)->m_ParentStateMachine = this;

	for(StateMachineVector::iterator it = m_ChildStateMachine.begin() ; it < m_ChildStateMachine.end() ; it++)
		(*it)->FixStateParent();

	DIRTY_AND_INVALIDATE();
}

TransitionVector StateMachine::GetOrderedTransitionsFromState(State *state) const
{	
	if(GetParentStateMachine() == 0)
	{
		TransitionMap::const_iterator it = m_OrderedTransitions.find(state);
		if(it != m_OrderedTransitions.end())
		{
			return it->second;	
		}
		
		TransitionVector ret;
		return ret;
	}

	else
	{
		return GetParentStateMachine()->GetOrderedTransitionsFromState(state);
	}	
}

void StateMachine::SetOrderedTransitionsFromState(State *state, TransitionVector& transitions)
{
	/// We dont want to actually delete the transitions, simply remove its ordered vector
	TransitionMap::iterator it = m_OrderedTransitions.find(state);
	if(it != m_OrderedTransitions.end())
		m_OrderedTransitions.erase(it);

	for(TransitionVector::const_iterator it = transitions.begin() ; it != transitions.end() ; it++)
	{
		AddTransition(*it);
	}
}

void StateMachine::AddOrderedTransition(Transition* transition)
{
	if(transition)
	{
		TransitionMap::iterator it = m_OrderedTransitions.find(transition->GetSrcState());
		if(it == m_OrderedTransitions.end())
		{
			TransitionVector vector;
			vector.push_back(transition);
			m_OrderedTransitions.insert( std::make_pair(transition->GetSrcState(), vector));
		}
		else
		{	
			it->second.push_back(transition);
		}		

		DIRTY_AND_INVALIDATE();
	}
}

TransitionVector StateMachine::GetTransitionsToState(State *state) const
{
	TransitionVector ret;

	for(TransitionMap::const_iterator i = m_OrderedTransitions.begin() ; i !=  m_OrderedTransitions.end(); i++)
	{		
		for(unsigned j = 0 ; j < i->second.size(); j++)
		{
			if(i->second[j]->GetDstState() == state)
			{
				ret.push_back(i->second[j]);
			}
		}
	}

	return ret;
}

void StateMachine::SyncTransitionsFromRoot()
{	
	StateVector states;
	CollectAllStates(states, this);

	StateMachine* root = GetRootStateMachine(this);

	if(this != root)
	{
		for(StateVector::const_iterator stateIt = states.begin(); stateIt != states.end() ; ++stateIt)
		{		
			m_OrderedTransitions.erase(*stateIt); // needed for states in substatemachine
			TransitionVector transitions = root->GetOrderedTransitionsFromState(*stateIt);

			for(TransitionVector::const_iterator transitionIt= transitions.begin() ; transitionIt != transitions.end(); ++transitionIt)
			{
				if(std::find(states.begin(), states.end(), PPtr<State>((*transitionIt)->GetDstState())) != states.end())
				{
					AddOrderedTransition(*transitionIt);
				}
			}
		}	
	}
}

bool StateMachine::ValidateStateIndex(int index) const 
{
	if(index >= 0 && index < GetStateCount())
	{
		return true;	
	}

	ErrorString("Invalid State Index");
	return false;	
}

bool StateMachine::ValidateStateMachineIndex(int index) const
{
	if(index >= 0 &&  index < GetStateMachineCount())
	{
		return true;	
	}

	ErrorString("Invalid StateMachine Index");
	return false;	
}
