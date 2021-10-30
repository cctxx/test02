/*
 *  AudioCustomFilter.h
 *  Xcode
 *
 *  Created by Søren Christiansen on 9/12/10.
 *  Copyright 2010 Unity Technologies. All rights reserved.
 *
 */
#ifndef __AUDIO_CUSTOM_FILTER_H__
#define __AUDIO_CUSTOM_FILTER_H__

#if ENABLE_AUDIO_FMOD

#include "AudioSourceFilter.h"

class MonoBehaviour;
struct MonoThread;
struct MonoDomain;
class AudioSource;

#define MAX_CHANNELS 8

class AudioCustomFilter 
{
public:
	AudioCustomFilter(MonoBehaviour* behaviour);
	virtual ~AudioCustomFilter();
	
	FMOD::DSP* GetOrCreateDSP();
	FMOD::DSP* GetDSP();
public:
	#if ENABLE_MONO
	MonoDomain* monoDomain;
	#endif
	
	void SetPlayingSource(AudioSource* source) { m_playingSource = source; }
	AudioSource* GetPlayingSource() const { return m_playingSource; }
	void WaitForScriptCallback();
	void Cleanup();
private:
	FMOD::DSP* m_DSP;	
	
	MonoBehaviour* m_behaviour;
	
	bool m_InScriptCallback;
	AudioSource* m_playingSource;
private:
	void Init();

public:
#if UNITY_EDITOR
	float GetMaxIn(short channel) const; 
	float GetMaxOut(short channel) const;
	
	
	int channelCount;
	UInt64 processTime; // time in nanoseconds
private:
	float maxIn[MAX_CHANNELS];
	float maxOut[MAX_CHANNELS];
#endif
	
private:
	static FMOD_RESULT F_CALLBACK readCallback(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int outchannels); 
};

#endif // ENABLE_AUDIO_FMOD
#endif // __AUDIO_CUSTOM_FILTER_H__