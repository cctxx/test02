#include "UnityPrefix.h"
#include "DestroyDelayed.h"
#include "CallDelayed.h"
#include "Runtime/Misc/GameObjectUtility.h"

void DelayedDestroyCallback (Object* o, void* userData);

void DelayedDestroyCallback (Object* o, void* userData)
{
	DestroyObjectHighLevel (o);
}

void DestroyObjectDelayed (Object* o, float time)
{
	CallDelayed (DelayedDestroyCallback, o, time);
}
