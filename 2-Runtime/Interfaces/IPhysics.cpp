#include "UnityPrefix.h"
#include "IPhysics.h"

static IPhysics* gIPhysics = NULL;

IPhysics* GetIPhysics()
{
	return gIPhysics;
}

void SetIPhysics(IPhysics* phys)
{
	gIPhysics = phys;
}