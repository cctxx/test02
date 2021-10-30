#include "UnityPrefix.h"
#include "Runtime/Interfaces/IAudio.h"
#include "AudioManager.h"
#include "Runtime/Video/BaseVideoTexture.h"
#include "Runtime/Video/MovieTexture.h"
#include "Runtime/Audio/AudioClip.h"
#include "Runtime/Export/WWW.h"
#include "Runtime/Audio/AudioCustomFilter.h"

class AudioModule : public IAudio
{
	virtual void SetPause( bool paused )
	{
		GetAudioManager().SetPause(paused);
	}

	virtual void FixedUpdate()
	{
		GetAudioManager().FixedUpdate();
	}

	virtual void Update()
	{
		GetAudioManager().Update();
	}

	virtual void StopVideoTextures()
	{
		BaseVideoTexture::StopVideoTextures();
	}

	virtual void PauseVideoTextures()
	{
		BaseVideoTexture::PauseVideoTextures();
	}

	virtual void UpdateVideoTextures()
	{
		BaseVideoTexture::UpdateVideoTextures();
	}
#if ENABLE_WWW
#if ENABLE_MOVIES
	virtual MovieTexture* CreateMovieTextureFromWWW(WWW& www)
	{
		MovieTexture* tex = NEW_OBJECT(MovieTexture);
		tex->Reset();
		tex->InitStream(&www);	
		return tex;
	}
#endif
	virtual AudioClip* CreateAudioClipFromWWW(WWW& www, bool threeD, bool stream, FMOD_SOUND_TYPE audioType)
	{
		AudioClip* clip = NEW_OBJECT(AudioClip);

		// only allow sample read if the security policy allows it
		WWW::SecurityPolicy policy = www.GetSecurityPolicy();
		if (policy != WWW::kSecurityPolicyAllowAccess)
			clip->SetReadAllowed(false);
		
		clip->Reset();		
		clip->Set3D(threeD);

#if !UNITY_FLASH
		if (!clip->InitStream(&www, NULL, stream, audioType))
#else
		if (!clip->InitStream(&www, NULL, stream))
#endif

		{
			DestroySingleObject(clip);
			return NULL;
		}
		return clip;
	}
#endif
	
	virtual AudioClip* CreateAudioClipOGGFromWWW(WWW& www) 
	{
#if ENABLE_AUDIO_FMOD
		AudioClip* clip = NEW_OBJECT (AudioClip);
		clip->Reset();

		clip->SetName(GetLastPathNameComponent(www.GetUrl()).c_str());

		if (!clip->SetAudioData(www.GetData(), www.GetSize(), false, false,true, false, FMOD_SOUND_TYPE_OGGVORBIS, FMOD_SOUND_FORMAT_PCM16))
		{
			DestroySingleObject(clip);
			clip = NULL;
		}
		return clip;
#else
		return 0;
#endif
	}
	
	virtual bool IsFormatSupportedByPlatform(const char* type)
	{
#if ENABLE_AUDIO_FMOD
		return AudioClip::IsFormatSupportedByPlatform(type);
#endif
	}

#if ENABLE_AUDIO_FMOD
	virtual FMOD::DSP* GetOrCreateDSPFromCustomFilter(AudioCustomFilter* filter)
	{
		return filter->GetOrCreateDSP();
	}

	virtual AudioCustomFilter* CreateAudioCustomFilter(MonoBehaviour* mb)
	{
		return new AudioCustomFilter(mb);
	}

	virtual FMOD::DSP* GetDSPFromAudioCustomFilter(AudioCustomFilter* filter)
	{
		return filter->GetDSP();
	}

	virtual void SetBypassOnDSP(FMOD::DSP* dsp, bool state)
	{
		dsp->setBypass(state);
	}
#endif

#if ENABLE_PROFILER
	virtual void GetProfilerStats(AudioStats& stats)
	{
		GetAudioManager().GetProfilerData(stats);		
	}
#endif

	virtual void AudioManagerAwakeFromLoad(AwakeFromLoadMode mode)
	{
		GetAudioManager().AwakeFromLoad(mode);
	}
};


IAudio* CreateAudioModule()
{
	return new AudioModule();
}