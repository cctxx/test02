#include "UnityPrefix.h"
#include "AnimationManager.h"
#include "Runtime/Input/TimeManager.h"
#include "Animation.h"
#include "AnimationState.h"
#include "AnimationStateNetworkProvider.h"
#include "Runtime/Core/Callbacks/PlayerLoopCallbacks.h"

static AnimationManager* gAnimationManager = NULL;

void AnimationManager::Update ()
{
	// Update animations
	double time = GetCurTime();
	AnimationList* animations = NULL;
	if (GetTimeManager ().IsUsingFixedTimeStep ())
		animations = &m_FixedAnimations;
	else
		animations = &m_Animations;
		
	// Animation List node can be destroyed in UpdateAnimation, if somebody writes an AnimationEvent
	// to do that. So we have to use SafeListIterator
	SafeIterator<AnimationList> j (*animations);
	while (j.Next())
	{
		Animation& animation = **j;
		animation.UpdateAnimation(time);
	}
}

void AnimationManager::InitializeClass ()
{
	Assert(gAnimationManager == NULL);
	gAnimationManager = UNITY_NEW_AS_ROOT (AnimationManager, kMemAnimation, "AnimationManager", "");

	REGISTER_PLAYERLOOP_CALL (LegacyFixedAnimationUpdate, GetAnimationManager().Update());
	REGISTER_PLAYERLOOP_CALL (LegacyAnimationUpdate, GetAnimationManager().Update ());
	
	InitializeAnimationStateNetworkProvider();
}

void AnimationManager::CleanupClass ()
{
	Assert(gAnimationManager != NULL);
	UNITY_DELETE(gAnimationManager, kMemAnimation);

	CleanupAnimationStateNetworkProvider();
}

AnimationManager& GetAnimationManager ()
{
	return *gAnimationManager;
}
