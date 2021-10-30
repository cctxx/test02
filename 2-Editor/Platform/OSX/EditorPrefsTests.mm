#include "Projects/PrecompiledHeaders/UnityPrefix.h"

NSAutoreleasePool* gPool = NULL;

void InitNSAutoreleasePool()
{
	Assert(!gPool);
	gPool = [[NSAutoreleasePool alloc]init];
}

void ReleaseNSAutoreleasePool()
{
	Assert(gPool);
	[gPool release];
	gPool = NULL;
}
