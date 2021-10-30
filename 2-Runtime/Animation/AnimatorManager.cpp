#include "UnityPrefix.h"
#include "AnimatorManager.h"
#include "Animator.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Core/Callbacks/PlayerLoopCallbacks.h"

static AnimatorManager* gAnimatorManager = NULL;

void AnimatorManager::FixedUpdateFKMove()
{
	if(m_FixedUpdateAvatars.size() > 0 && GetTimeManager().GetFixedDeltaTime())
	{
		Animator::UpdateAvatars(m_FixedUpdateAvatars.begin(), m_FixedUpdateAvatars.size(), GetTimeManager().GetFixedDeltaTime(),true,false);
	}
}

void AnimatorManager::FixedUpdateRetargetIKWrite()
{
	//@TODO: Why do we do FixedUpdateRetargetIKWrite?
	//       This visually wrong when you use physics interpolation to get smooth root motion, but then the animation of the characters limbs only updates at a very low physics framerate.
	//       It would be better to run UpdateRetargetWrite in the dynamic framerate.
	
	if(m_FixedUpdateAvatars.size() > 0 && GetTimeManager().GetFixedDeltaTime())
	{
		Animator::UpdateAvatars(m_FixedUpdateAvatars.begin(), m_FixedUpdateAvatars.size(), GetTimeManager().GetFixedDeltaTime(),false,true);
	}
}

void AnimatorManager::UpdateFKMove ()
{	
	if(m_UpdateAvatars.size() > 0 && GetTimeManager().GetDeltaTime())
	{
		Animator::UpdateAvatars(m_UpdateAvatars.begin(), m_UpdateAvatars.size(), GetDeltaTime(),true,false);
	}
}

void AnimatorManager::UpdateRetargetIKWrite ()
{	
	if(m_UpdateAvatars.size() > 0 && GetTimeManager().GetDeltaTime())
	{
		Animator::UpdateAvatars(m_UpdateAvatars.begin(), m_UpdateAvatars.size(), GetDeltaTime(),false,true);
	}
}

void AnimatorManager::AddAnimator (Animator& animator)
{
	if (animator.GetEnabled() && animator.IsActive() && animator.IsValid())
	{
		if(animator.GetAnimatePhysics())
		{
			animator.SetFixedBehaviourIndex(m_FixedUpdateAvatars.size());
			m_FixedUpdateAvatars.push_back(&animator);
		}
		else
		{
			animator.SetBehaviourIndex(m_UpdateAvatars.size());
			m_UpdateAvatars.push_back(&animator);
		}
	}
}

void AnimatorManager::RemoveAnimator (Animator& animator)
{
	int index = animator.GetBehaviourIndex();
	
	if(index != -1)
	{
		Animator* backBehaviour = m_UpdateAvatars.back();
		m_UpdateAvatars[index] = backBehaviour;
		backBehaviour->SetBehaviourIndex(index);
		animator.SetBehaviourIndex(-1);
		m_UpdateAvatars.pop_back();
	}

	int fixedIndex = animator.GetFixedBehaviourIndex();
	
	if(fixedIndex != -1)
	{
		Animator* backBehaviour = m_FixedUpdateAvatars.back();
		m_FixedUpdateAvatars[fixedIndex] = backBehaviour;
		backBehaviour->SetFixedBehaviourIndex(fixedIndex);
		animator.SetFixedBehaviourIndex(-1);
		m_FixedUpdateAvatars.pop_back();
	}
}

void AnimatorManager::InitializeClass ()
{
	Assert(gAnimatorManager == NULL);
	gAnimatorManager = UNITY_NEW_AS_ROOT (AnimatorManager, kMemAnimation, "AnimatorManager", "");
	
	REGISTER_PLAYERLOOP_CALL (AnimatorFixedUpdateRetargetIKWrite, GetAnimatorManager().FixedUpdateRetargetIKWrite());
	REGISTER_PLAYERLOOP_CALL (AnimatorUpdateRetargetIKWrite, GetAnimatorManager().UpdateRetargetIKWrite ());
	REGISTER_PLAYERLOOP_CALL (AnimatorUpdateFKMove, GetAnimatorManager().UpdateFKMove ());
	REGISTER_PLAYERLOOP_CALL (AnimatorFixedUpdateFKMove, GetAnimatorManager().FixedUpdateFKMove ());
}

void AnimatorManager::CleanupClass ()
{ 
	UNITY_DELETE (gAnimatorManager, kMemAnimation);
	gAnimatorManager = NULL;
	
	
}

AnimatorManager& GetAnimatorManager ()
{
	Assert(gAnimatorManager != NULL);
	return *gAnimatorManager;
}