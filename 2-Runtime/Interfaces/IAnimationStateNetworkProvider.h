#pragma once

class Animation;

struct AnimationStateForNetwork
{
	bool  enabled;
	float weight;
	float time;
};

class IAnimationStateNetworkProvider
{
public:
	
	virtual int  GetNetworkAnimationStateCount (Animation& animation) = 0;
	virtual void GetNetworkAnimationState (Animation& animation, AnimationStateForNetwork* state, int count) = 0;
	virtual void SetNetworkAnimationState (Animation& animation, const AnimationStateForNetwork* serialize, int count) = 0;
};

EXPORT_COREMODULE IAnimationStateNetworkProvider*	GetIAnimationStateNetworkProvider ();
EXPORT_COREMODULE void								SetIAnimationStateNetworkProvider (IAnimationStateNetworkProvider* manager);