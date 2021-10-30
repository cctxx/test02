#ifndef AUDIOCLIP_H
#define AUDIOCLIP_H

#if !(UNITY_FLASH || UNITY_WEBGL)
#include "Runtime/Audio/AudioClip_FMOD.h"
#else
#include "Runtime/Audio/AudioClip_Flash.h"
#endif // !UNITY_FLASH

#endif // AUDIOCLIP_H