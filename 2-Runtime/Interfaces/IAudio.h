#pragma once

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Audio/correct_fmod_includer.h"

class Object;
class MovieTexture;
class WWW;
class AudioClip;
struct AudioStats;
namespace FMOD { class DSP; }
class AudioCustomFilter;
class MonoBehaviour;

class IAudio
{
public:
	virtual void SetPause( bool pause ) = 0;
	virtual void FixedUpdate() = 0;
	virtual void Update() = 0;

	virtual void StopVideoTextures() = 0;
	virtual void UpdateVideoTextures() = 0;
	virtual void PauseVideoTextures() = 0;

#if ENABLE_WWW
#if ENABLE_MOVIES
	virtual MovieTexture* CreateMovieTextureFromWWW(WWW& www) = 0;
#endif
	virtual AudioClip* CreateAudioClipFromWWW(WWW& www, bool threeD, bool stream, FMOD_SOUND_TYPE audioType) = 0;
	virtual AudioClip* CreateAudioClipOGGFromWWW(WWW& www) = 0;	
#endif
	
	virtual bool IsFormatSupportedByPlatform(const char* type) = 0;

#if ENABLE_AUDIO_FMOD
	virtual FMOD::DSP* GetOrCreateDSPFromCustomFilter(AudioCustomFilter* filter) = 0;
	virtual AudioCustomFilter* CreateAudioCustomFilter(MonoBehaviour* mb) = 0;
	virtual FMOD::DSP* GetDSPFromAudioCustomFilter(AudioCustomFilter* filter) = 0;
	virtual void SetBypassOnDSP(FMOD::DSP* dsp, bool state) = 0;
#endif

#if ENABLE_PROFILER
	virtual void GetProfilerStats(AudioStats& stats) = 0;
#endif
	virtual void AudioManagerAwakeFromLoad(AwakeFromLoadMode mode) = 0;
};

IAudio* CreateAudioModule();


IAudio* EXPORT_COREMODULE GetIAudio();
void EXPORT_COREMODULE SetIAudio(IAudio* physics);