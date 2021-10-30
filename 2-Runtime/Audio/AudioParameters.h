#ifndef ___AUDIOPARAMETERS_H__
#define ___AUDIOPARAMETERS_H__

#include "Runtime/Math/Vector3.h"
#include "Runtime/Audio/correct_fmod_includer.h"
#include "Runtime/Math/AnimationCurve.h"

enum RolloffMode { kRolloffLogarithmic=0, kRolloffLinear, kRolloffCustom };

struct AudioParameters 
{
	// Animated props
	AnimationCurve panLevelCustomCurve;	
	AnimationCurve spreadCustomCurve;
	AnimationCurve rolloffCustomCurve;
		
	float insideConeAngle;		
	float outsideConeAngle;
	float outsideConeVolume;
	
	int priority;
	float dopplerLevel;
	float minDistance;
	float maxDistance;
	float pan;
	
	float pitch;
	float volume;
	
	// rolloff
	RolloffMode rolloffMode; ///< enum { kRolloffLogarithmic=0, kRolloffLinear, kRolloffCustom }
	
	bool loop; // <-- this will be replaced by a loop node
	bool mute;	

#if UNITY_WII
	bool starving;  // For streaming sounds, when data isn't coming due disk eject 
#endif
		
	bool bypassEffects; // Bypass/ignore any applied effects from AudioSource
	bool bypassListenerEffects; // Bypass/ignore any applied effects from AudioListener
	bool bypassReverbZones; // Bypass/ignore any applied effects from reverb zones
	bool ignoreListenerPause;
};



#endif // ___AUDIOPARAMETERS_H__
