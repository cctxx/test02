#include "UnityPrefix.h"
#include "IsPlaying.h"

#if GAMERELEASE
static bool gIsWorldPlaying = true;
#else
static bool gIsWorldPlaying = false;
#endif

bool IsWorldPlaying () { return gIsWorldPlaying; }
void SetIsWorldPlaying (bool isPlaying)
{
	gIsWorldPlaying = isPlaying;
}
