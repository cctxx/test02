#ifndef QTMOVIEIMPORTER
#define QTMOVIEIMPORTER

// There is no 64-bit QT on windows yet. Need to wait until QuickTime X is ported.
// Also, the old QT APIs are no longer supported in the 10.7 SDK on OS X.
// Need to switch to the new Cocoa APIs instead.
#define ENABLE_QTMOVIEIMPORTER (!(UNITY_64 && UNITY_WIN) && !defined(MAC_OS_X_VERSION_10_7))

#if ENABLE_QTMOVIEIMPORTER

#if UNITY_WIN
#pragma warning(disable:4005) // macro redefinition, happens in Quicktime headers
#endif

using namespace std;

#undef CopyMatrix

#include "Editor/Platform/Interface/Quicktime.h"
#include "ImageConverter.h"
#include "AudioVideoImporter.h"
#include <string>

#if defined(AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER) ||  MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
#define HAS_QT_7_HEADERS 1
#else
#define HAS_QT_7_HEADERS 0
#endif

#if !HAS_QT_7_HEADERS
typedef struct MovieAudioExtractionRefRecord*  MovieAudioExtractionRef;
#endif

class QuickTimeMovieImporter : IAudioVideoImporter
{
	Movie		m_Movie;
	Rect		m_Rect;
	GWorldPtr	m_GWorld;
	int			m_FrameCount;
	int			m_FrameIndex;
	bool		m_HasAudio;
	bool		m_HasVideo;
	double		m_FrameRate;
	double		m_MovieDuration;
	int			m_OggVideoBitrate;
	int			m_OggAudioBitrate;
		
	MovieAudioExtractionRef m_AudioSession;
	int		    m_AudioSamplingRate;
	int			m_AudioChannels;
	int			m_AudioChannelSize;
	std::string	m_AssetPath;

	UInt8*		m_FileDataPtr;
	
	std::string m_ErrorMessage;
	std::string	m_WarningMessage;
	
public: 
		
	QuickTimeMovieImporter ();
	
	~QuickTimeMovieImporter ();

	bool Open (const std::string& pathName);
	
	// IAudioTranscoder implementation	
	virtual bool TranscodeToOgg (const std::string& outPath, IAudioVideoImporter::ProgressbarCallback* progressCallback, bool importVideo);
	virtual bool GetNextAudioSamples(unsigned long *numSamples,void **bufferData, unsigned* sampleSize);
	virtual bool CanRead(const std::string& path) { /* bogus */ return true; }
	virtual FMOD_SOUND_FORMAT GetFMODFormat() const { return FMOD_SOUND_FORMAT_PCM16; }
	virtual FMOD_SOUND_TYPE   GetFMODType() const { return FMOD_SOUND_TYPE_RAW; }
	
	bool HasVideo () { return m_HasVideo; }
	int GetWidth () { return m_Rect.right; }
	int GetHeight () { return m_Rect.top; }
	int GetFrameCount () { return m_FrameCount; }
	float GetRecommendedFrameRate () { return m_FrameRate; }
	
	bool HasAudio () { return m_HasAudio; }

	double GetDuration () { return m_MovieDuration; }

	
	int GetOggVideoBitrate() { return m_OggVideoBitrate; }
	void SetOggVideoBitrate(int q) { m_OggVideoBitrate = q; }

	int GetOggAudioBitrate() { return m_OggAudioBitrate; }
	void SetOggAudioBitrate(int q) { m_OggAudioBitrate = q; }
	
	int GetAudioChannelSize() { return m_AudioChannelSize; }
	void SetAudioChannelSize(int q) { m_AudioChannelSize = q; }

	int GetAudioChannelCount () { return m_AudioChannels; }
	void SetAudioChannelCount (int c) { m_AudioChannels=c; }

	void SetAudioSamplingRate (int rate) { m_AudioSamplingRate = rate; }
	int GetAudioSamplingRate () { return m_AudioSamplingRate; }

	const std::string& GetWarningMessage() const { return m_WarningMessage; }
	const std::string& GetErrorMessage() const { return m_ErrorMessage; }

	bool SetRenderingContext (ImageReference& ref);
	void GoToFirstFrame ();	
	bool GetNextFrame ();
	
	bool SetupVideo();
	bool SetupAudio();
	
	
};
#endif
#endif