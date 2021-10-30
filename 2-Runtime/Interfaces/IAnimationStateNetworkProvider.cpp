#include "UnityPrefix.h"
#include "IAnimationStateNetworkProvider.h"

static IAnimationStateNetworkProvider* gAnimationProvider;

IAnimationStateNetworkProvider* GetIAnimationStateNetworkProvider ()
{
	return gAnimationProvider;
}

void SetIAnimationStateNetworkProvider (IAnimationStateNetworkProvider* manager)
{
	gAnimationProvider = manager;
}