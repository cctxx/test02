#include "UnityPrefix.h"
#include "PlayerLoopCallbacks.h"

PlayerLookCallbacks gPlayerLoopCallbacks;

PlayerLookCallbacks::PlayerLookCallbacks ()
{
	memset(this, 0, sizeof(PlayerLookCallbacks));
}