#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_MOVIES

#include "BaseVideoTexture.h"
#include "MoviePlayback.h"
#include "Runtime/Audio/AudioClip.h"
#if ENABLE_WWW
#include "Runtime/Export/WWW.h"
#endif
#include <vector>

class ColorRGBAf;
class MoviePlayback;


class MovieTexture: public BaseVideoTexture
{
private:

	std::vector<UInt8> m_MovieData;		//the raw Ogg movie data
	MoviePlayback m_MoviePlayback;	//class controlling playback state
	PPtr<AudioClip> m_AudioClip;		//attached AudioClip
#if ENABLE_WWW
	WWW *m_StreamData;					//if != NULL, use this instead of m_MovieData.
#else
	typedef void WWW;
#endif
	
protected: 
//	void DestroyTexture ();

	void TryLoadMovie ();
	
public:
	REGISTER_DERIVED_CLASS (MovieTexture, Texture)
	DECLARE_OBJECT_SERIALIZE (MovieTexture)
	
#if ENABLE_WWW
	// WARNING: don't call AwakeFromLoad if you use InitStream
	void InitStream (WWW * streamData);
#endif
	void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	
	void Rewind();
	virtual void Play();
	virtual void Pause();
	virtual void Stop ();
	virtual void Update ();
	virtual void UnloadFromGfxDevice(bool forceUnloadAll);
	virtual void UploadToGfxDevice();
	
	bool IsPlaying();
	bool GetLoop () {return m_MoviePlayback.GetLoop();}
	void SetLoop (bool l) {m_MoviePlayback.SetLoop(l);}

	bool ReadyToPlay ();
	
	virtual bool ShouldIgnoreInGarbageDependencyTracking ();
	MovieTexture (MemLabelId label, ObjectCreationMode mode, WWW* streamData = NULL);
			
	#if ENABLE_PROFILER || UNITY_EDITOR
	virtual int GetStorageMemorySize() const { return m_MovieData.size(); }
	#endif
	
	// WARNING: don't call AwakeFromLoad if you use SetMovieData
	void SetMovieData(const UInt8* data,long size);
	std::vector<UInt8> *GetMovieData() { return &m_MovieData; }
	
	AudioClip *GetMovieAudioClip() { return m_AudioClip; }
	void SetMovieAudioClip(AudioClip *clip);
	
	float GetMovieDuration() { return m_MoviePlayback.GetMovieTotalDuration(); }
};

#endif // ENABLE_MOVIES
