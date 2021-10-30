/*
 *  AudioScriptBufferManager.cpp
 *  AllTargets.workspace
 *
 *  Created by Søren Christiansen on 8/22/11.
 *  Copyright 2011 Unity Technologies. All rights reserved.
 *
 */
#include "UnityPrefix.h"
#include "AudioScriptBufferManager.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"

#if ENABLE_AUDIO_FMOD

AudioScriptBufferManager::AudioScriptBufferManager(unsigned PCMArraySize, unsigned DSPFilterArraySize) :
	 m_PCMReadArray(SCRIPTING_NULL)
	,m_DSPFilterArray(SCRIPTING_NULL)
	,m_PCMReadArrayGCHandle(0)
	,m_DSPFilterArrayGCHandle(0)
	,m_DSPFilterArrayOrigLength(0)
	,m_PCMReadArrayOrigLength(0)
{
	Init(PCMArraySize, DSPFilterArraySize);
}

AudioScriptBufferManager::~AudioScriptBufferManager()
{
}

void AudioScriptBufferManager::DidReloadDomain()
{
	Cleanup();
	Init(m_PCMReadArrayOrigLength, m_DSPFilterArrayOrigLength);
}

void AudioScriptBufferManager::Init(unsigned PCMArraySize, unsigned DSPFilterArraySize)
{
	// Create shared MonoArray for callbacks and DSPs
	m_PCMReadArrayOrigLength = PCMArraySize;
	#if ENABLE_MONO
	ScriptingClassPtr klass = mono_class_from_name (mono_get_corlib (), "System", "Single");
	m_PCMReadArray = mono_array_new (mono_domain_get (), klass, m_PCMReadArrayOrigLength);
	m_PCMReadArrayGCHandle = mono_gchandle_new((MonoObject*)m_PCMReadArray, 1);	
	#else
	ScriptingClassPtr klass = GetScriptingTypeRegistry().GetType("System", "Single");
	m_PCMReadArray = CreateScriptingArray<float>(klass, m_PCMReadArrayOrigLength);
	m_PCMReadArrayGCHandle = scripting_gchandle_new(m_PCMReadArray);
	#endif
	
	m_DSPFilterArrayOrigLength = DSPFilterArraySize;
	#if ENABLE_MONO
	m_DSPFilterArray = mono_array_new (mono_domain_get (), klass, m_DSPFilterArrayOrigLength );
	m_DSPFilterArrayGCHandle = mono_gchandle_new((MonoObject*)m_DSPFilterArray, 1);
	#else
	m_DSPFilterArray = CreateScriptingArray<float>(klass, m_DSPFilterArrayOrigLength);
	m_DSPFilterArrayGCHandle = scripting_gchandle_new(m_DSPFilterArray);
	#endif
}

void AudioScriptBufferManager::Cleanup()
{
	#if ENABLE_SCRIPTING
	// Cleanup mono arrays
	if (m_PCMReadArray)
	{
		#if ENABLE_MONO
		PatchLength(m_PCMReadArray, m_PCMReadArrayOrigLength);
		#endif
		scripting_gchandle_free(m_PCMReadArrayGCHandle);
		m_PCMReadArray = SCRIPTING_NULL;
		m_PCMReadArrayGCHandle = 0;		
	}
	if (m_DSPFilterArray)
	{
		#if ENABLE_MONO
		PatchLength(m_DSPFilterArray, m_DSPFilterArrayOrigLength);
		#endif
		scripting_gchandle_free(m_DSPFilterArrayGCHandle);
		m_DSPFilterArray = SCRIPTING_NULL;
		m_DSPFilterArrayGCHandle = 0;		
	}
	#endif
}

void AudioScriptBufferManager::GetPCMReadArray(unsigned length, ScriptingArrayPtr &array)
{
	#if ENABLE_SCRIPTING
	Assert(length <= m_PCMReadArrayOrigLength);
	unsigned curLength = GetScriptingArraySize(m_PCMReadArray);
	if (length != curLength)
	{
		#if ENABLE_MONO
		PatchLength(m_PCMReadArray, length);
		#else
		ScriptingClassPtr klass = GetMonoManager().GetCommonClasses().floatSingle;
		array = CreateScriptingArray(Scripting::GetScriptingArrayStart<float>(m_PCMReadArray), length, klass);
		return;
		#endif
	}
	array = m_PCMReadArray;
	#endif
}

void AudioScriptBufferManager::GetDSPFilterArray(unsigned length, ScriptingArrayPtr &array)
{
	#if ENABLE_SCRIPTING
	Assert(length <= m_DSPFilterArrayOrigLength);
	unsigned curLength = GetScriptingArraySize(m_DSPFilterArray);
	if (length != curLength)
	{
		#if ENABLE_MONO
		PatchLength(m_DSPFilterArray, length);
		#else
		ScriptingClassPtr klass = GetMonoManager().GetCommonClasses().floatSingle;
		array = CreateScriptingArray(Scripting::GetScriptingArrayStart<float>(m_DSPFilterArray), length, klass);
		return;
		#endif
	}
	array = m_DSPFilterArray;
	#endif
}


#if ENABLE_MONO

void AudioScriptBufferManager::PatchLength(ScriptingArrayPtr array, unsigned newlength)
{
	char* pos = sizeof(uintptr_t)*3 + (char*)array;
	*((UInt32*)pos) = (UInt32)newlength;
}

#endif

#endif // ENABLE_AUDIO_FMOD
