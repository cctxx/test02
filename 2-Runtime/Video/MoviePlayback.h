#ifndef MOVIE_PLAYBACK
#define MOVIE_PLAYBACK

#include "Configuration/UnityConfigure.h"
#include "Runtime/Math/Color.h"

bool PlayFullScreenMovie (std::string const& path,
                          ColorRGBA32 const& backgroundColor,
                          unsigned long controlMode, unsigned long scalingMode);

#if ENABLE_MOVIES
#include "Runtime/Audio/correct_fmod_includer.h"
#include "External/theora/include/theora/theora.h"

#if UNITY_EDITOR
//editor uses custom ogg, not the one from fmod, because it also needs the encoding functionality, which is not present in the fmod one.
#include "../../External/Audio/libvorbis/include/vorbis/codec.h"
#else
#include <vorbis/codec.h> //rely on include directories to pick the ogg.h from fmod for this specific platform.
#endif

#include "Runtime/Audio/AudioClip.h"

#if ENABLE_WWW
#include "Runtime/Export/WWW.h"
#endif

struct MovieDataStream {
	UInt8 *data;
	long size;
	long position;
};

class MovieTexture;
class AudioClip;
class AudioSource;

class MoviePlayback
{
private:
	
	ogg_sync_state	m_OggSynchState;
	ogg_page		m_OggPage;

	ogg_stream_state m_TheoraStreamState;
	theora_info		m_TheoraInfo;
	theora_comment	m_TheoraComment;
	theora_state	m_TheoraState;

	ogg_stream_state m_VorbisStreamState;
	vorbis_info		m_VorbisInfo;
	vorbis_comment	m_VorbisComment;
	vorbis_dsp_state m_VorbisState;
	vorbis_block	m_VorbisBlock;

	bool			m_CanStartPlaying;
	bool			m_VideoBufferReady;
	double			m_VideoBufferTime;

	int				m_AudioBufferFill;
	bool			m_AudioBufferReady;
	ogg_int16_t*	m_AudioBuffer;
	ogg_int64_t		m_AudioBufferGranulePos; /* time position of last sample */
	double			m_AudioBufferTime;	//Real time when the last audio buffer was filled 
	
	bool			m_NoMoreData;	//Are we finished playing?
	MovieDataStream m_Data;	//Data buffer and position of stream
	double			m_StartTime;	//real time offset for start
	double			m_LastSampleTime;	//last sample played
	bool			m_IsPlaying;	//shall movie update in player loop?
	bool			m_Loop;		//is movie looping?
	
	bool			m_InitialisedLoad; //Have we initialised the theora and vorbis codec structs
	bool			m_VorbisInitialised; //Vorbis headers are initialised and ready for data.
	bool			m_VorbisStateInitialised; //The vorbis state struct is primed, but not necessarily fully ready
	bool			m_TheoraInitialised; //Theora headers are initialised and ready for data.
	bool			m_TheoraStateInitialised; //The vorbis state struct is primed, but not necessarily fully ready

	float			m_Duration;	//duration if known

	MovieTexture*	m_Texture;	
	AudioClip*		m_AudioClip;
#if ENABLE_WWW
	WWW*			m_DataStream; //if != NULL, use this as data.
#endif
	FMOD::Channel*	m_AudioChannel;

	void QueueOggPageIntoStream();
	double GetMovieTime(bool useAudio);
	void ChangeMovieData(UInt8 *data,long size);
	bool MovieStreamImage();
	bool InitStreams(int &theoraHeadersSeen, int &vorbisHeaderSeen);
	void Cleanup();
	void CleanupInfoStructures();
	int ReadBufferIntoOggStream();
	void PauseAudio();

public:
	MoviePlayback();
	~MoviePlayback();
	
	int GetMovieWidth();
	int GetMovieHeight();
	int GetMovieAudioRate();
	int GetMovieAudioChannelCount();
	int GetMovieBitrate();
	float GetMovieTotalDuration() const {return m_Duration;}

	bool IsPlaying() {return m_IsPlaying;}

	void SetLoop (bool l);
	bool GetLoop () {return m_Loop;}

	//Do we have a video and/or audio track?
	bool MovieHasAudio();
	bool MovieHasVideo();
	
	//Load movie from a data ptr
	bool LoadMovieData(UInt8 *data,long size);
	
#if ENABLE_WWW
	//Load movie from a web stream (and track data internally)
	bool LoadMovieData(WWW *stream);
#endif

	bool DidLoad() { return m_VorbisInitialised || m_TheoraInitialised; }
	
	void SetMovieTexture(MovieTexture *t) {m_Texture=t;}
	void SetMovieAudioClip(AudioClip *c) {m_AudioClip=c;}
	void SetAudioChannel(FMOD::Channel* channel);

	bool GetAudioBuffer(void** buffer, unsigned* size);
	
	void MoviePlaybackClose();

	void Play();  
	void Pause(); 
	void Stop ();
	void Rewind();
	
	bool Update();
};

#else // ENABLE_MOVIES

class WWW;
class MovieTexture;
class AudioClip;
class AudioSource;
namespace FMOD
{
	class Channel;
}

// dummy implementation
class MoviePlayback
{
public:
	MoviePlayback() {}
	~MoviePlayback() {}
	
	int GetMovieWidth() { return 320; }
	int GetMovieHeight() { return 240; }
	int GetMovieAudioRate() { return 22050; }
	int GetMovieAudioChannelCount() { return 1; }
	int GetMovieBitrate() { return 0; }
	float GetMovieTotalDuration() {return 0;}

	bool IsPlaying() {return false;}

	void SetLoop (bool l) {}
	bool GetLoop () {return false;}

	//Do we have a video and/or audio track?
	bool MovieHasAudio() { return false; }
	bool MovieHasVideo() { return false; }
	
	//Load movie from a data ptr
	bool LoadMovieData(UInt8 *data,long size) {
		return false;
	}
	
	//Load movie from a web stream (and track data internally)
	bool LoadMovieData(WWW *stream) {
		return false;
	}

	bool DidLoad() {return false; }
	
	void SetMovieTexture(MovieTexture *t) {}
	void SetMovieAudioClip(AudioClip *c) {}
	void SetAudioChannel(FMOD::Channel* channel) {}
	
	void MoviePlaybackClose() {}
	void Play() {}
	void Pause() {}
	void Stop () {}
	void Rewind() {}
	
	void Update() {}
};

inline void AddToUpdateList(MoviePlayback *m) {}
inline void RemoveFromUpdateList(MoviePlayback *m) {}

inline void UpdateMovies() {}
inline void ResetMovies() {}
inline void PauseMovies() {}

#endif

#endif
