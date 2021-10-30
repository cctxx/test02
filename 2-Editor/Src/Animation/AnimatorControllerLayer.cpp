
#include "UnityPrefix.h"
#include "AnimatorControllerLayer.h"

#include "Runtime/Animation/AnimatorController.h"
#include "Editor/Src/Animation/StateMachine.h"


#define DIRTY_AND_INVALIDATE() m_Controller->OnInvalidateAnimatorController(); m_Controller->SetDirty(); 

INSTANTIATE_TEMPLATE_TRANSFER(AnimatorControllerLayer)

template<class TransferFunction>
void AnimatorControllerLayer::Transfer (TransferFunction& transfer)
{		
	transfer.SetVersion (3);	

	TRANSFER (m_Name);	
	TRANSFER (m_StateMachine);

	if(transfer.IsOldVersion(2))
		transfer.Transfer(m_Mask, "m_HumanMask");	
	else
		TRANSFER(m_Mask);

	TRANSFER_ENUM(m_BlendingMode);
	TRANSFER (m_SyncedLayerIndex);
	TRANSFER (m_StateMachineMotionSetIndex);
	TRANSFER (m_DefaultWeight);
	TRANSFER (m_IKPass);			
	TRANSFER (m_SyncedLayerAffectsTiming);

	transfer.Align();
	TRANSFER (m_Controller);
}


AnimatorControllerLayer::AnimatorControllerLayer ()	
	: 	m_StateMachine(0),
	m_Mask(0), 
	m_DefaultWeight(0),
	m_IKPass(false), 
	m_SyncedLayerAffectsTiming(false),
	m_SyncedLayerIndex(-1), 
	m_StateMachineMotionSetIndex(0), 
	m_BlendingMode(AnimatorLayerBlendingModeOverride)	
{

}


char const* AnimatorControllerLayer::GetName() const
{
	return m_Name.c_str();
}


void AnimatorControllerLayer::SetName(char const *name)
{
	if (strcmp (GetName(), name) != 0)
	{ 
		// make name unique		
		string uniqueName = m_Controller->MakeUniqueLayerName(name);
		m_Name = uniqueName;

		if(m_StateMachine)
			m_StateMachine->SetName(name);
		DIRTY_AND_INVALIDATE();
	}
}

StateMachine* AnimatorControllerLayer::GetStateMachine() const
{
	if(GetSyncedLayerIndex() > -1)
		return m_Controller->GetLayer(GetSyncedLayerIndex())->GetStateMachine();

	return m_StateMachine;
}

void AnimatorControllerLayer::SetStateMachine(StateMachine* stateMachine)
{
	if(PPtr<StateMachine>(stateMachine) != m_StateMachine)
	{
		m_StateMachine = stateMachine;
		m_StateMachine->SetName(GetName());
		DIRTY_AND_INVALIDATE();
	}
}

AvatarMask* AnimatorControllerLayer::GetMask() const
{
	return m_Mask;
}

void AnimatorControllerLayer::SetMask(AvatarMask* mask)
{
	if(PPtr<AvatarMask>(mask) != m_Mask)
	{
		m_Mask =mask;
		DIRTY_AND_INVALIDATE();
	}
}

AnimatorLayerBlendingMode AnimatorControllerLayer::GetBlendingMode() const
{
	return m_BlendingMode;
}

void AnimatorControllerLayer::SetBlendingMode(AnimatorLayerBlendingMode mode)
{
	if(mode != m_BlendingMode)
	{
		m_BlendingMode = mode;
		DIRTY_AND_INVALIDATE();
	}
}

int AnimatorControllerLayer::GetSyncedLayerIndex() const
{
	return m_SyncedLayerIndex;
}

void AnimatorControllerLayer::SetSyncedLayerIndex(int index)
{
	if(index != m_SyncedLayerIndex)
	{
		if(GetSyncedLayerIndex() > -1)
		{
			StateMachine* prevStateMachine = GetStateMachine();
			prevStateMachine->RemoveMotionSet(GetStateMachineMotionSetIndex());

			for(int i = 0 ; i < m_Controller->GetLayerCount(); i++)
			{
				AnimatorControllerLayer* currentLayer = m_Controller->GetLayer(i);
				if( currentLayer!=this && currentLayer->GetSyncedLayerIndex() == m_SyncedLayerIndex && currentLayer->GetStateMachineMotionSetIndex() > m_StateMachineMotionSetIndex)
					currentLayer->SetStateMachineMotionSetIndex(currentLayer->GetStateMachineMotionSetIndex()-1);					
			}

			m_SyncedLayerIndex = -1;
			m_StateMachineMotionSetIndex = 0;
		}

		if(index > -1)
		{
			StateMachine* stateMachine = m_Controller->GetLayer(index)->GetStateMachine();
			stateMachine->AddMotionSet();
			m_StateMachineMotionSetIndex = stateMachine->GetMotionSetCount()-1;
			m_SyncedLayerIndex = index;
		}
		DIRTY_AND_INVALIDATE();
	}
}

void AnimatorControllerLayer::SetSyncedLayerIndexInternal(int index)
{
	m_SyncedLayerIndex = index;
}

int AnimatorControllerLayer::GetStateMachineMotionSetIndex()
{
	return m_StateMachineMotionSetIndex; 
}

void AnimatorControllerLayer::SetStateMachineMotionSetIndex(int index)
{
	m_StateMachineMotionSetIndex = index; 
}

bool AnimatorControllerLayer::GetIKPass() const
{
	return m_IKPass;
}

void AnimatorControllerLayer::SetIKPass(bool ik)
{
	if(ik != m_IKPass)
	{
		m_IKPass = ik;
		DIRTY_AND_INVALIDATE();
	}
}

bool AnimatorControllerLayer::GetSyncedLayerAffectsTiming()
{
	return m_SyncedLayerAffectsTiming;
}

void AnimatorControllerLayer::SetSyncedLayerAffectsTiming(bool affects)
{
	if( affects != m_SyncedLayerAffectsTiming)
	{
		m_SyncedLayerAffectsTiming = affects;
		DIRTY_AND_INVALIDATE();
	}
}



float AnimatorControllerLayer::GetDefaultWeight() const
{
	return m_DefaultWeight;
}

void AnimatorControllerLayer::SetDefaultWeight(float weigth)
{
	if(weigth != m_DefaultWeight)
	{
		m_DefaultWeight = weigth;
		DIRTY_AND_INVALIDATE();
	}
}

AnimatorController* AnimatorControllerLayer::GetController() const
{
	return m_Controller;
}

void AnimatorControllerLayer::SetController(AnimatorController* controller)
{
	m_Controller = controller;
}

#undef DIRTY_AND_INVALIDATE
