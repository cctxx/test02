/*
 *  AudioSource.Callbacks.cpp
 *  Xcode
 *
 *  Created by Søren Christiansen on 10/12/09.
 *  Copyright 2009 Unity Technologies. All rights reserved.
 *
 */
#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#if ENABLE_AUDIO
#include "AudioSource.h"
#include "AudioManager.h"
#include "Runtime/Utilities/Utility.h"

/**
 * Do log, linear and custom rolloff calculation to avoid interpolating the (animation)curve
 **/
float AudioSource::CalculateVolumeModifierForDistance(float distance)
{	
	float maxDistance, minDistance, rolloffScale;
	RolloffMode rolloffMode;	
	
	rolloffScale = GetAudioManager().GetRolloffScale();
	
	maxDistance = GetMaxDistance();
	minDistance = GetMinDistance();
	rolloffMode = GetRolloffMode();	
		
	register float gain = 1.0f;
	
	switch (rolloffMode) {
		case kRolloffLinear:
			{
				float range = maxDistance-minDistance;
				if (range<=0) 
					gain=1.0f;
				else
				{
					distance = maxDistance - distance;
					gain = distance / range;
				}
			}
			break;
		case kRolloffLogarithmic:
			{
				if ((distance > minDistance) && (rolloffScale != 1.0f))
				{
					distance -= minDistance;
					distance *= rolloffScale;
					distance += minDistance;
				} 
				
				if (distance < .000001f)
				{
					distance = .000001f;
				}
				gain = minDistance / distance;
			}
			break;
		case kRolloffCustom:
			{
				//@TODO: maxDistance can be 0.0F in that case the audio will play back incorrectly because gain becomes nan
				if (maxDistance > 0.0F) 
				{
					const AnimationCurve& curve = GetCustomRolloffCurve();
					gain = curve.Evaluate(distance / maxDistance);
				}
			}			
			break;
		default:
			break;
	}
	
	Assert(IsFinite(gain));
	
	if (gain < 0.0f)
		gain = 0.0f;
	if (gain > 1.0f)
		gain = 1.0f;
	
	return gain;	
}

#if ENABLE_AUDIO_FMOD
float F_CALLBACK AudioSource::rolloffCallback( 
											  FMOD_CHANNEL *  c_channel,  
											  float  distance)
{	
	FMOD::Channel* channel = (FMOD::Channel*)c_channel;
	AudioSource* audioSource = AudioSource::GetAudioSourceFromChannel(channel);
	
	if (audioSource == NULL)
		return 0.0f;
	
	return audioSource->CalculateVolumeModifierForDistance(distance);
}

#endif
#endif //ENABLE_AUDIO
