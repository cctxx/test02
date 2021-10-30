/*
 *  AudioScriptBufferManager.h
 *  AllTargets.workspace
 *
 *  Created by Søren Christiansen on 8/22/11.
 *  Copyright 2011 Unity Technologies. All rights reserved.
 *
 */
#ifndef UNITY_AUDIOSCRIPTBUFFERMANAGER_H
#define UNITY_AUDIOSCRIPTBUFFERMANAGER_H

#if ENABLE_AUDIO_FMOD

#include "Runtime/Scripting/Backend/ScriptingTypes.h"

struct MonoArray;


class AudioScriptBufferManager {
public:
	AudioScriptBufferManager(unsigned PCMArraySize, unsigned DSPFilterArraySize);
	~AudioScriptBufferManager();
	
	void Init(unsigned PCMArraySize, unsigned DSPFilterArraySize);
	void Cleanup();
	void DidReloadDomain();
	
	void GetDSPFilterArray(unsigned length, ScriptingArrayPtr &array);
	void GetPCMReadArray(unsigned length, ScriptingArrayPtr &array);

private:
	ScriptingArrayPtr m_PCMReadArray;
	int m_PCMReadArrayGCHandle;
	unsigned m_PCMReadArrayOrigLength;
	
	ScriptingArrayPtr m_DSPFilterArray;
	int m_DSPFilterArrayGCHandle;
	unsigned m_DSPFilterArrayOrigLength;
	
	#if ENABLE_MONO
	inline void PatchLength(ScriptingArrayPtr array, unsigned newlength);
	#endif
};

#endif // ENABLE_AUDIO_FMOD

#endif // UNITY_AUDIOSCRIPTBUFFERMANAGER_H

