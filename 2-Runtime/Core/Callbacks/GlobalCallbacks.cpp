#include "UnityPrefix.h"
#include "GlobalCallbacks.h"

static GlobalCallbacks gGlobalCallback;

GlobalCallbacks& GlobalCallbacks::Get()
{
	return gGlobalCallback;
}
