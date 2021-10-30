#include "UnityPrefix.h"
#include "AudioBehaviour.h"

#if ENABLE_AUDIO

IMPLEMENT_CLASS (AudioBehaviour)

AudioBehaviour::AudioBehaviour (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{}

AudioBehaviour::~AudioBehaviour ()
{
}

#endif