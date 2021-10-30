#include "UnityPrefix.h"
#include "IAnimation.h"

static IAnimation* gAnimation = NULL;
IAnimation* GetAnimationInterface()
{
	return gAnimation;
}

void SetAnimationInterface(IAnimation* theInterface)
{
	gAnimation = theInterface;
}
