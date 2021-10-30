#pragma once

#include "Runtime/BaseClasses/NamedObject.h"
#include "Editor/Src/Animation/AnimatorControllerParameter.h"
#include "Runtime/mecanim/animation/avatar.h"
#include "Editor/Src/Animation/AvatarMask.h"


template<class T>
class PPtr;
class AnimationClip;
class StateMachine;
class AvatarMask;
class AnimatorController;


enum AnimatorLayerBlendingMode
{
	AnimatorLayerBlendingModeOverride = mecanim::animation::kLayerBlendingModeOverride,
	AnimatorLayerBlendingModeAdditive = mecanim::animation::kLayerBlendingModeAdditive
};

class AnimatorControllerLayer 
{

public:
	
	AnimatorControllerLayer ();	
	DECLARE_SERIALIZE (AnimatorControllerLayer)
	
	char const* GetName() const;
	void SetName(char const *name);

	StateMachine* GetStateMachine() const;
	void SetStateMachine(StateMachine* stateMachine);

	AvatarMask* GetMask() const;
	void SetMask(AvatarMask* mask);

	AnimatorLayerBlendingMode GetBlendingMode() const;
	void SetBlendingMode(AnimatorLayerBlendingMode mode);

	int GetSyncedLayerIndex() const;
	void SetSyncedLayerIndex(int index);
	void SetSyncedLayerIndexInternal(int index);

	int GetStateMachineMotionSetIndex();
	void SetStateMachineMotionSetIndex(int index);

	bool GetIKPass() const;
	void SetIKPass(bool ik);

	bool GetSyncedLayerAffectsTiming();
	void SetSyncedLayerAffectsTiming(bool affects);

	float GetDefaultWeight() const;
	void SetDefaultWeight(float weight);

	AnimatorController* GetController() const;
	void SetController(AnimatorController* controller);

private:

	UnityStr					m_Name;
	PPtr<StateMachine>			m_StateMachine;
	PPtr<AvatarMask>			m_Mask;
	AnimatorLayerBlendingMode	m_BlendingMode;
	int							m_SyncedLayerIndex;	
	int							m_StateMachineMotionSetIndex;	

	float						m_DefaultWeight;
	bool						m_IKPass;		
	bool						m_SyncedLayerAffectsTiming;
	

private:

	PPtr<AnimatorController> m_Controller ; // used to set unique name,  and manage synced layers

};

