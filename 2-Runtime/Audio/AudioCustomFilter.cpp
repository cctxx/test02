/*
 *  AudioCustomFilter.cpp
 *  Xcode
 *
 *  Created by Søren Christiansen on 9/12/10.
 *  Copyright 2010 Unity Technologies. All rights reserved.
 *
 */
#include "UnityPrefix.h"
#if ENABLE_AUDIO_FMOD
#include "AudioCustomFilter.h"
#include "AudioSource.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Mono/MonoScriptCache.h"
#include "Runtime/Mono/MonoScopedThreadAttach.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Profiler/TimeHelper.h"

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/MonoCompile.h"
#endif

#include "Runtime/Scripting/Scripting.h"

AudioCustomFilter::AudioCustomFilter(MonoBehaviour* behaviour) :
m_behaviour(behaviour),
#if ENABLE_MONO
monoDomain(NULL),
#endif
m_DSP(NULL),
m_InScriptCallback(false),
m_playingSource(NULL)
#if UNITY_EDITOR
,
processTime(0.0),
channelCount(0)
#endif
{
	Init();
}

AudioCustomFilter::~AudioCustomFilter()
{	
	Cleanup();
}

void AudioCustomFilter::Init()
{	
	// setup dsp/callbacks
	if (!m_DSP)
	{ 
        FMOD_DSP_DESCRIPTION  dspdesc; 
		FMOD_RESULT result;
		
        memset(&dspdesc, 0, sizeof(FMOD_DSP_DESCRIPTION)); 

#if UNITY_EDITOR
        strcpy(dspdesc.name, m_behaviour->GetScriptClassName().c_str());
#endif
        dspdesc.channels     = 0;                   // 0 = whatever comes in, else specify. 
        dspdesc.read         = AudioCustomFilter::readCallback;
		dspdesc.userdata     = this;
		
		result = GetAudioManager().GetFMODSystem()->createDSP(&dspdesc, &m_DSP); 
		FMOD_ASSERT(result); 			
		
		m_DSP->setBypass(true);
    } 
	
	#if ENABLE_MONO
	monoDomain = mono_domain_get();
	#endif
}

void AudioCustomFilter::Cleanup()
{
	if (m_DSP)
	{
		// is this a playing dsp?
		if (m_playingSource)
		{
			// Doesn't matter whether argument is true or false, since sources playing DSP's don't play one-shots at the same time.
			m_playingSource->Stop(true);
		}
		
		FMOD_RESULT result = m_DSP->release();
		FMOD_ASSERT(result);
		m_DSP = NULL;
	}
}

FMOD::DSP* AudioCustomFilter::GetOrCreateDSP()
{
	if (!m_DSP)
		Init();
	return m_DSP;
}

FMOD::DSP* AudioCustomFilter::GetDSP()
{
	return m_DSP;
}

void AudioCustomFilter::WaitForScriptCallback()
{
	while (m_InScriptCallback)
		Thread::Sleep(0.01); // wait 10ms
	
}

#if UNITY_EDITOR
float AudioCustomFilter::GetMaxIn(short channel) const
{ 
	if (m_DSP) 
	{ 
		bool active, bypass; 
		m_DSP->getActive(&active);
		m_DSP->getBypass(&bypass);
		return active&&!bypass?maxIn[channel]:0.0f; 
	} 
	else return 0.0f; 
}

float AudioCustomFilter::GetMaxOut(short channel) const
{ 
	if (m_DSP) 
	{ 
		bool active, bypass; 
		m_DSP->getActive(&active);
		m_DSP->getBypass(&bypass);
		return active&&!bypass?maxOut[channel]:0.0f; 
	} 
	else return 0.0f; 
}
#endif // UNITY_EDITOR

FMOD_RESULT F_CALLBACK AudioCustomFilter::readCallback(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int outchannels)
{	
#if SUPPORT_MONO_THREADS

#if UNITY_EDITOR	
	if (IsCompiling())
		return FMOD_OK;
#endif
	
	AudioCustomFilter* filter;
	FMOD_RESULT result;
	FMOD::DSP* fmod_dsp = (FMOD::DSP*)dsp_state->instance;
	result = fmod_dsp->getUserData((void**)&filter);
	
	Assert (filter);
	
#if UNITY_EDITOR		
	ABSOLUTE_TIME start_time = START_TIME;
#endif
	if (!filter->m_behaviour->GetEnabled())
		return FMOD_OK;
	
	filter->m_InScriptCallback = true;
	{
		ScopedThreadAttach attach(filter->monoDomain);
		
		// reuse mono array
		MonoArray* array = NULL;
		GetAudioManager().GetScriptBufferManager().GetDSPFilterArray(length * inchannels, array);
		
		Assert(array);
		
		memcpy( &GetMonoArrayElement<float>( array, 0 ), inbuffer, length * 4 * inchannels );		
		
		ScriptingObjectPtr instance = Scripting::ScriptingWrapperFor( filter->m_behaviour );			
		if (instance)
		{
			MonoMethod* method = filter->m_behaviour->GetMethod(MonoScriptCache::kAudioFilterRead)->monoMethod;
			void* args[] = { array, &inchannels };
			MonoException* exception;
			mono_runtime_invoke( method, instance, args, &exception );	
			if (exception)
			{
				// handle
				Scripting::LogException(exception, Scripting::GetInstanceIDFromScriptingWrapper(instance));
			}
			else
			{
				memcpy(outbuffer, &GetMonoArrayElement<float>( array, 0 ), length * 4 * inchannels);
			}
		}
	}
	filter->m_InScriptCallback = false;
	
#if UNITY_EDITOR
	filter->processTime = GetProfileTime(ELAPSED_TIME(start_time));
	filter->channelCount = inchannels;
	for (int c = 0; c < inchannels && c < MAX_CHANNELS; c++) 
		filter->maxOut[c] = filter->maxIn[c] = -1.0f;
	for (int i = 0;i < length; i += inchannels)
		for (int c = 0; c < inchannels && c < MAX_CHANNELS; c++)
		{
			if (filter->maxIn[c]<inbuffer[i+c]) filter->maxIn[c] = inbuffer[i+c];
			if (filter->maxOut[c]<outbuffer[i+c]) filter->maxOut[c] = outbuffer[i+c];
		}
#endif
#endif // SUPPORT_THREADS
	
	return FMOD_OK;
}
#endif // ENABLE_AUDIO_FMOD
