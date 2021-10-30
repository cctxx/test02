#include "UnityPrefix.h"
#include "AnimationStateNetworkProvider.h"
#include "Runtime/Interfaces/IAnimationStateNetworkProvider.h"
#include "AnimationState.h"
#include "Animation.h"

class AnimationStateNetworkProvider : public IAnimationStateNetworkProvider
{
public:
	
	virtual int  GetNetworkAnimationStateCount (Animation& animation)
	{
		return animation.GetAnimationStateCount();
	}

	virtual void GetNetworkAnimationState (Animation& animation, AnimationStateForNetwork* output, int count)
	{
		for (int i=0;i<count;i++)
		{
			AnimationState& state = animation.GetAnimationStateAtIndex(i);
			
			output[i].enabled = state.GetEnabled ();
			output[i].weight = state.GetWeight ();
			output[i].time = state.GetTime ();
		}
	}
	
	virtual void SetNetworkAnimationState (Animation& animation, const AnimationStateForNetwork* serialize, int count)
	{
		for (int i=0;i<count;i++)
		{
			AnimationState& state = animation.GetAnimationStateAtIndex(i);
			
			state.SetEnabled (serialize[i].enabled);
			state.SetWeight  (serialize[i].weight);
			state.SetTime    (serialize[i].time);
		}
	}
};

void InitializeAnimationStateNetworkProvider ()
{
	SetIAnimationStateNetworkProvider(UNITY_NEW_AS_ROOT(AnimationStateNetworkProvider, kMemPhysics, "AnimationStateNetworkInterface", ""));
}

void CleanupAnimationStateNetworkProvider ()
{
	AnimationStateNetworkProvider* module = reinterpret_cast<AnimationStateNetworkProvider*> (GetIAnimationStateNetworkProvider ());
	UNITY_DELETE(module, kMemPhysics);
	SetIAnimationStateNetworkProvider(NULL);
}