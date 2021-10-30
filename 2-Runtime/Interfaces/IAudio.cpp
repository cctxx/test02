#include "UnityPrefix.h"
#include "IAudio.h"

static IAudio* gAudio = NULL;

IAudio* GetIAudio()
{
	return gAudio;
}

void SetIAudio(IAudio* audio)
{
	gAudio = audio;
}