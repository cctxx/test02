#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_AUDIO_FMOD

#include "AudioClip.h"
#include "AudioSource.h"
#include "Utilities/Conversion.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/SwapEndianArray.h"
#include "AudioManager.h"
#include "Runtime/Video/MoviePlayback.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Misc/UTF8.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Utilities/PathNameUtility.h"

#include "WavReader.h"
#include "OggReader.h"

#if UNITY_EDITOR //in editor, include nonfmod vorbis first, to make sure we actually use that, and not the fmod version.
#include "../../External/Audio/libvorbis/include/vorbis/codec.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/AssetPipeline/AudioImporter.h"
#endif

#include "Runtime/Audio/correct_fmod_includer.h"

#if ENABLE_WWW
#include "Runtime/Export/WWW.h"
#endif

#include "Runtime/Misc/BuildSettings.h"

#if UNITY_EDITOR
#include <fstream>
#endif


//kMono8 = 1, kMono16, kStereo8, kStereo16, kOggVorbis
static FMOD_SOUND_FORMAT FORMAT_TO_FMOD_FORMAT[] = { FMOD_SOUND_FORMAT_NONE , FMOD_SOUND_FORMAT_PCM8, FMOD_SOUND_FORMAT_PCM16, FMOD_SOUND_FORMAT_PCM8, FMOD_SOUND_FORMAT_PCM16, FMOD_SOUND_FORMAT_PCM16  };
static FMOD_SOUND_TYPE FORMAT_TO_FMOD_TYPE[] = {FMOD_SOUND_TYPE_UNKNOWN, FMOD_SOUND_TYPE_RAW, FMOD_SOUND_TYPE_RAW, FMOD_SOUND_TYPE_RAW, FMOD_SOUND_TYPE_RAW, FMOD_SOUND_TYPE_OGGVORBIS  };


#if ENABLE_PROFILER
int AudioClip::s_AudioClipCount = 0;
#endif


void AudioClip::InitializeClass ()
{
	#if UNITY_EDITOR
	RegisterAllowNameConversion (AudioClip::GetClassStringStatic(), "m_UseRuntimeDecompression", "m_DecompressOnLoad");
	#endif
}

AudioClip::AudioClip (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode), 	
m_Channels(0),
m_Sound(NULL),	
m_Frequency(0),
m_BitsPerSample(0),
m_3D(true),
m_UseHardware(false),
m_Format(FMOD_SOUND_FORMAT_NONE),
m_Type(FMOD_SOUND_TYPE_UNKNOWN),
#if UNITY_EDITOR
m_EditorSoundType(FMOD_SOUND_TYPE_UNKNOWN),
m_EditorSoundFormat(FMOD_SOUND_FORMAT_NONE),
#endif
#if ENABLE_WWW
m_StreamData(NULL),
m_ReadAllowed(true),
#endif
m_ExternalStream(false),
m_MoviePlayback(NULL),
m_OpenState(FMOD_OPENSTATE_CONNECTING),
m_LoadFlag(kDecompressOnLoad),
m_WWWStreamed(false),
m_PCMArray(SCRIPTING_NULL),
m_PCMArrayGCHandle(0),
m_CachedSetPositionCallbackMethod(SCRIPTING_NULL),
m_CachedPCMReaderCallbackMethod(SCRIPTING_NULL),
m_DecodeBufferSize(0),
#if ENABLE_MONO
monoDomain(NULL),
#endif
m_UserGenerated(false),
m_UserLengthSamples(0),
m_UserIsStream(true),
m_AudioData(kMemAudio)
{
#if ENABLE_PROFILER
	s_AudioClipCount++;
#endif
}

#if ENABLE_WWW
bool AudioClip::InitStream (WWW* streamData, MoviePlayback* movie, bool realStream /* = false */, FMOD_SOUND_TYPE fmodSoundType /* = FMOD_SOUND_TYPE_UNKNOWN */)
{
	AssertIf(m_MoviePlayback != NULL);
	AssertIf(m_StreamData != NULL);

	// Web streaming
	if (streamData)
	{
		// If the audiotype isn't specified, guess the audio type from the url to avoid going thru all available codecs (this seriously hit performance for a net stream)
		std::string ext = ToLower(GetPathNameExtension(streamData->GetUrl()));
		
		if (fmodSoundType == FMOD_SOUND_TYPE_UNKNOWN)
			m_Type = GetFormatFromExtension(ext);
		else
			m_Type = fmodSoundType;
		
		if (m_Type == FMOD_SOUND_TYPE_UNKNOWN)
		{
			ErrorStringObject(Format("Unable to determine the audio type from the URL (%s) . Please specify the type.", streamData->GetUrl() ), this);
			// right now we're trying to load sound in AwakeFromLoad (and do only that) - so we skip the call
			HackSetAwakeWasCalled();
			return false;
		}

		if (realStream && IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_1_a3))
		{
			if(m_Type == FMOD_SOUND_TYPE_XM || m_Type == FMOD_SOUND_TYPE_IT || m_Type == FMOD_SOUND_TYPE_MOD || m_Type == FMOD_SOUND_TYPE_S3M)
			{
				ErrorStringObject("Tracker files (XM/IT/MOD/S3M) cannot be streamed in realtime but must be fully downloaded before they can play.", this);
				HackSetAwakeWasCalled(); // to avoid error message: "Awake has not been called '' (AudioClip). Figure out where the object gets created and call AwakeFromLoad correctly."
				return false;
			}
		}

		if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
		{
#if UNITY_EDITOR
			BuildTargetPlatform targetPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget ();	
			if	(
				 (m_Type == FMOD_SOUND_TYPE_OGGVORBIS && ((targetPlatform == kBuild_Android)||(targetPlatform == kBuild_iPhone)||(targetPlatform == kBuildBB10)||(targetPlatform == kBuildTizen)))
				 ||
				 (m_Type == FMOD_SOUND_TYPE_MPEG && !((targetPlatform == kBuild_Android)||(targetPlatform == kBuild_iPhone)||(targetPlatform == kBuildBB10)||(targetPlatform == kBuildTizen)))
				 )
#else
			if	(
				 (m_Type == FMOD_SOUND_TYPE_OGGVORBIS && (UNITY_ANDROID | UNITY_IPHONE | UNITY_BB10 | UNITY_TIZEN) != 0)
				 ||
				 (m_Type == FMOD_SOUND_TYPE_MPEG && (UNITY_ANDROID | UNITY_IPHONE | UNITY_BB10 | UNITY_TIZEN) == 0)
				 )
#endif
			{
				ErrorStringObject(Format("Streaming of '%s' on this platform is not supported", ext.c_str()), this);
				// right now we're trying to load sound in AwakeFromLoad (and do only that) - so we skip the call
				HackSetAwakeWasCalled();
				return false;	
			}
		}
		
		m_StreamData = streamData;
		m_StreamData->SetAudioClip( this );
		m_StreamData->Retain(); // Make sure the WWW object doesn't dissappear if the mono side of the WWW object is deleted before we are done.
		m_ExternalStream = true;
		m_WWWStreamed = realStream;
		// reserve queue space
		m_AudioQueueMutex.Lock();
		m_AudioBufferQueue.reserve(kAudioQueueSize);
		m_AudioQueueMutex.Unlock();
		LoadSound();
	}
	
	m_MoviePlayback = movie;
	
	if (movie)
	{
		m_ExternalStream = true;
		LoadSound();
	}
	
	#if !UNITY_RELEASE
	// right now we're trying to load sound in AwakeFromLoad (and do only that) - so we skip the call
	HackSetAwakeWasCalled();
	#endif
	
	return true;
}
#endif

bool AudioClip::CreateUserSound(const std::string& name, unsigned lengthSamples, short channels, unsigned frequency, bool _3D, bool stream)
{
	Reset();
	Cleanup();
	
	m_UserGenerated = true;
	m_UserLengthSamples = lengthSamples;
	m_UserIsStream = stream;
	m_Channels = channels;
	m_Frequency = frequency;
	m_3D = _3D;
	m_Format = FMOD_SOUND_FORMAT_PCMFLOAT;
	m_BitsPerSample = 32;
	SetName(name.c_str());

	CreateScriptCallback();
	
	m_Sound = CreateSound();

#if !UNITY_RELEASE
	// right now we're trying to load sound in AwakeFromLoad (and do only that) - so we skip the call
	HackSetAwakeWasCalled();
#endif
	
	return true;
}

bool AudioClip::InitWSound (FMOD::Sound* sound)
{
	Assert(sound);
	Cleanup();
	CreateScriptCallback();
	m_Sound = sound;
	m_OpenState = FMOD_OPENSTATE_READY;
	GetSoundProps();
	sound->setUserData((void*)this);
	
	return true;
}



AudioClip::~AudioClip ()
{
#if ENABLE_PROFILER
	s_AudioClipCount--;
#endif

	Cleanup();
#if ENABLE_WWW
	if (m_StreamData)
	{
		m_StreamData->SetAudioClip( NULL );
		m_StreamData->Release();
	}
#endif
	if (m_MoviePlayback)
		m_MoviePlayback->SetMovieAudioClip(NULL);
}

FMOD_SOUND_TYPE AudioClip::GetFormatFromExtension(const std::string& ext)
{
	std::string ext_lower = ToLower(ext);
	FMOD_SOUND_TYPE type = FMOD_SOUND_TYPE_UNKNOWN;
	if (ext_lower == "ogg")
		type = FMOD_SOUND_TYPE_OGGVORBIS;
	else
	if (ext_lower == "mp2" || ext_lower == "mp3")
		type = FMOD_SOUND_TYPE_MPEG;
	else
	if (ext_lower == "wav")
		type = FMOD_SOUND_TYPE_WAV;
	else
	if (ext_lower == "it")
		type = FMOD_SOUND_TYPE_IT;
	else
	if (ext_lower == "xm")
		type = FMOD_SOUND_TYPE_XM;
	else
	if (ext_lower == "s3m")
		type = FMOD_SOUND_TYPE_S3M;
	else
	if (ext_lower == "mod")
		type = FMOD_SOUND_TYPE_MOD;
	
	return type;
}

bool AudioClip::IsFormatSupportedByPlatform(const std::string& ext)
{
	FMOD_SOUND_TYPE type = GetFormatFromExtension(ext);
	if(type == FMOD_SOUND_TYPE_UNKNOWN)
		return false;
	if(type == FMOD_SOUND_TYPE_OGGVORBIS && (UNITY_IPHONE || UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN))
		return false;
	if(type == FMOD_SOUND_TYPE_MPEG && !UNITY_IPHONE && !UNITY_ANDROID && !UNITY_BB10 && !UNITY_TIZEN)
		return false;
	return true;
}

void AudioClip::Cleanup()
{	
	if (!m_CachedSounds.empty())
	{
		for (TFMODSounds::iterator it = m_CachedSounds.begin(); it != m_CachedSounds.end(); ++it)
		{
			if ((*it).second)
				(*it).second->stop();
			if ((*it).first)
				(*it).first->release();
		}
		m_CachedSounds.clear();
	}
	else
	{
		if (m_Sound)
		{	
			m_Sound->release();
		}
	}
	
	m_Sound = NULL;		
}


void AudioClip::CleanupClass()
{
	
}


bool AudioClip::ReadyToPlay ()
{	
	if(
#if ENABLE_WWW
		!m_StreamData && 
#endif
		 !m_MoviePlayback)		
		return true;
		
	if (m_OpenState == FMOD_OPENSTATE_READY)
		return true;
		
	
	// try to...
	LoadSound();
	
	if (!m_Sound)		
	{
		m_OpenState = FMOD_OPENSTATE_CONNECTING;
		return false;
	}
	
	m_OpenState = FMOD_OPENSTATE_READY;	

	return true;	
}

void AudioClip::SetData(const float* data, unsigned lengthSamples, unsigned offsetSamples /*= 0*/)
{
	if (m_Sound)
	{
		if (IsFMODStream())
		{
			ErrorStringObject("Cannot set data on streamed sample", this);
			return;
		}
		
		//Address of a pointer that will point to the first part of the locked data.
		void *ptr1 = NULL;
		
		//Address of a pointer that will point to the second part of the locked data. This will be null if the data locked hasn't wrapped at the end of the buffer.
		void *ptr2 = NULL;
		
		//Length of data in bytes that was locked for ptr1
		unsigned len1 = 0;
		
		//	Length of data in bytes that was locked for ptr2. This will be 0 if the data locked hasn't wrapped at the end of the buffer.	
		unsigned len2 = 0;
        
        unsigned samplesToCopy = lengthSamples;
        unsigned clipSampleCount = GetSampleCount();
        if (lengthSamples > clipSampleCount)
        {
            WarningString(Format("Data too long to fit the audioclip: %s. %i sample(s) discarded", GetName(), (lengthSamples - clipSampleCount)));
            samplesToCopy = clipSampleCount;
        }
        
        //Offset in bytes to the position you want to lock in the sample buffer.
		int offsetBytes = offsetSamples * m_Channels * (m_BitsPerSample / 8);
		
		//Number of bytes you want to lock in the sample buffer.
		int	lengthBytes = samplesToCopy * m_Channels * (m_BitsPerSample / 8);
		
		FMOD_RESULT result = m_Sound->lock(offsetBytes, lengthBytes, &ptr1, &ptr2, &len1, &len2);
		
		FMOD_ASSERT(result);
		
		if (ptr2 == NULL)
		{
			ArrayFromNormFloat(m_Format, data, data + samplesToCopy * m_Channels, ptr1);
		}
		else // wrap
		{
			ArrayFromNormFloat(m_Format, data, data + (len1 / sizeof(float)), ptr1);
			ArrayFromNormFloat(m_Format, data + len1 / sizeof(float), data + (len1 + len2) / sizeof(float), ptr2);
		}
		
		m_Sound->unlock(ptr1, ptr2, len1, len2);
	}
}
						   

void AudioClip::GetData(float* data, unsigned lengthSamples, unsigned offsetSamples /*= 0*/) const
{
	if (m_Sound)
	{
		if (IsFMODStream())
		{
			ErrorStringObject("Cannot get data from streamed sample", this);
			return;
		}
		
		//Address of a pointer that will point to the first part of the locked data.
		void *ptr1 = NULL;
		
		//Address of a pointer that will point to the second part of the locked data. This will be null if the data locked hasn't wrapped at the end of the buffer.
		void *ptr2 = NULL;
		
		//Length of data in bytes that was locked for ptr1
		unsigned len1 = 0;
		
		//	Length of data in bytes that was locked for ptr2. This will be 0 if the data locked hasn't wrapped at the end of the buffer.	
		unsigned len2 = 0;
        
        unsigned samplesToCopy = lengthSamples;
        unsigned clipSampleCount = GetSampleCount();
        if (lengthSamples > clipSampleCount)
        {
            WarningString(Format("Data longer than the AudioClip: %s. %i sample(s) copied", GetName(), clipSampleCount));
            samplesToCopy = clipSampleCount;
        }
        
		//Offset in bytes to the position you want to lock in the sample buffer.
		int offsetBytes = offsetSamples * m_Channels * (m_BitsPerSample / 8);
		
		//Number of bytes you want to lock in the sample buffer.
		int	lengthBytes = samplesToCopy  * m_Channels * (m_BitsPerSample / 8);
		
		unsigned totalLengthBytes;
		m_Sound->getLength(&totalLengthBytes, FMOD_TIMEUNIT_PCMBYTES);
		Assert ( totalLengthBytes >= lengthBytes );
		
		FMOD_RESULT result = m_Sound->lock(offsetBytes, lengthBytes, &ptr1, &ptr2, &len1, &len2);
		
		FMOD_ASSERT(result);
		
		if (ptr2 == NULL)
		{
			ArrayToNormFloat(m_Format, ptr1, ((char*)ptr1 + len1), data);
		}
		else // wrap
		{
			if (len1 + len2 > lengthBytes)
			{
				WarningString(Format("Array can not hold the number of samples (%d)", (len1 + len2) - lengthBytes));
			}
			else
			{
				ArrayToNormFloat(m_Format, ptr1, ((char*)ptr1 + len1), data);
				ArrayToNormFloat(m_Format, ptr2, ((char*)ptr2 + len2), data + (len1 / 4));
			}			
		}
		
		m_Sound->unlock(ptr1, ptr2, len1, len2);
	}
}


// Set the attached movie clip
void AudioClip::SetMoviePlayback(MoviePlayback* movie)
{
	m_MoviePlayback = movie;

	if (!movie)
		return;
	
	m_ExternalStream = true;
	
	// unload any attached www data
#if ENABLE_WWW
	if (m_StreamData)
		m_StreamData->Release();
	m_StreamData = NULL;
#endif
	
	m_Channels = movie->GetMovieAudioChannelCount();
	m_Frequency = movie->GetMovieAudioRate();
	m_Format = FMOD_SOUND_FORMAT_PCM16;
	m_Type = FMOD_SOUND_TYPE_RAW;
	m_BitsPerSample = 16;
	
	m_OpenState = FMOD_OPENSTATE_CONNECTING;	
}


MoviePlayback* AudioClip::GetMoviePlayback() const
{
	return m_MoviePlayback;
}


/**
 * Queue PCM data into clip
 * This is read by streamed sound
 * @param buffer Audio data to queue
 * @param size Size of Audio data
 **/
bool AudioClip::QueueAudioData(void* buffer, unsigned size)
{
	Mutex::AutoLock lock (m_AudioQueueMutex);
	if (m_AudioBufferQueue.size() + size < kAudioQueueSize)
	{	
		m_AudioBufferQueue.insert ( m_AudioBufferQueue.end(), (UInt8*)buffer, (UInt8*)buffer+size );
		return true;
	}
	
	return false;	
}


/**
 * Top audio data from the quene
 * @note buf must allocate size bytes
 * @param size The size in bytes you want to top
 * @return The audio data
 **/
bool AudioClip::GetQueuedAudioData(void** buf, unsigned size)
{
	Mutex::AutoLock lock(m_AudioQueueMutex);
	if (m_AudioBufferQueue.size() < size)
		return false;
	
	memcpy( *buf, &m_AudioBufferQueue[0], size);
	
	m_AudioBufferQueue.erase(m_AudioBufferQueue.begin(), m_AudioBufferQueue.begin() + size);
	
	return true;
}


void AudioClip::ClearQueue()
{
	Mutex::AutoLock lock(m_AudioQueueMutex);

	m_AudioBufferQueue.clear();
}



unsigned int AudioClip::GetSampleCount() const
{
	if (m_MoviePlayback)
	{
		if (m_MoviePlayback->GetMovieTotalDuration() < 0)
			return 0;
		else
			return (unsigned int)(m_MoviePlayback->GetMovieTotalDuration() * m_Frequency * m_Channels);
	}
		
	if (m_Sound == NULL)
		return 0;
	unsigned int sc = 0;
	m_Sound->getLength(&sc, FMOD_TIMEUNIT_PCM);
	return sc;
}


unsigned int AudioClip::GetChannelCount() const
{
	return m_Channels;
}
unsigned int AudioClip::GetBitRate() const
{
	return m_Frequency * m_BitsPerSample;	
}

unsigned int AudioClip::GetBitsPerSample() const
{
	return m_BitsPerSample;
}

unsigned int AudioClip::GetFrequency() const
{
	return m_Frequency;
}


int AudioClip::GetRuntimeMemorySize() const
{
	return Super::GetRuntimeMemorySize() + GetSize();
}

// Return the total memory used by this audioclip for the current target
unsigned int AudioClip::GetSize() const
{
	unsigned totalSize = 0;
	unsigned fmodSize = 0;
	if (m_Sound) m_Sound->getMemoryInfo(FMOD_MEMBITS_ALL, 0, &fmodSize, NULL);
	
	// compressed in memory
	if ( m_LoadFlag == kCompressedInMemory )
	{
		switch (m_Type) {
			case FMOD_SOUND_TYPE_XMA:
			case FMOD_SOUND_TYPE_GCADPCM:
				totalSize += m_AudioData.capacity();
				break;
			case FMOD_SOUND_TYPE_OGGVORBIS:
				totalSize += m_AudioData.capacity(); // stream from memory
			default:
				totalSize += fmodSize;
				break;
		}
	}
	else
	if ( m_LoadFlag == kDecompressOnLoad )
	// decompressed
	{
		totalSize += fmodSize;
	}
	else
	// stream from disc
	if ( m_LoadFlag == kStreamFromDisc )
	{
		unsigned streamBufferSize = 0;
		GetAudioManager().GetFMODSystem()->getStreamBufferSize(&streamBufferSize, NULL);
		totalSize += streamBufferSize;
	}	
	
	return totalSize;	
}

unsigned int AudioClip::GetLength() const
{
	if (m_MoviePlayback)
		return (unsigned int)(m_MoviePlayback->GetMovieTotalDuration() * 1000.0f);
	
	if (m_Sound == NULL)
		return 0;
	unsigned int sc = 0;
	m_Sound->getLength(&sc, FMOD_TIMEUNIT_MS);
	return sc;
}

float AudioClip::GetLengthSec() const
{
	if (m_MoviePlayback)
		return m_MoviePlayback->GetMovieTotalDuration();
	
	if (m_Sound == NULL)
		return 0;
	unsigned int sc = 0;
	m_Sound->getLength(&sc, FMOD_TIMEUNIT_MS);
	return sc / 1000.f;
}

MoviePlayback* AudioClip::GetMovie() const
{
	return m_MoviePlayback;
}


bool AudioClip::IsOggCompressible() const
{
	return	(m_Type != FMOD_SOUND_TYPE_MOD) &&
	(m_Type != FMOD_SOUND_TYPE_S3M) &&
	(m_Type != FMOD_SOUND_TYPE_MIDI) &&
	(m_Type != FMOD_SOUND_TYPE_XM) &&
	(m_Type != FMOD_SOUND_TYPE_IT) &&
	(m_Type != FMOD_SOUND_TYPE_SF2) &&
	((m_Type == FMOD_SOUND_TYPE_WAV) && 
	 (m_Format != FMOD_SOUND_FORMAT_PCM32));	
}

bool AudioClip::IsFMODStream () const
{
	bool isStream = false;

	if (m_Sound)
	{
		FMOD_MODE mode;
		m_Sound->getMode(&mode);
		isStream = (mode & FMOD_CREATESTREAM) != 0;
	}

	return isStream;
}


FMOD::Channel* AudioClip::GetCachedChannel()
{
	// Assert this is a stream
	Assert(GetMode() & FMOD_CREATESTREAM);
	
	TFMODSounds::iterator it = m_CachedSounds.begin();
	
	for (; it != m_CachedSounds.end(); ++it)
	{
		m_Sound = (*it).first;
		FMOD::Channel* channel = (*it).second;
		FMOD_RESULT result = FMOD_OK;
		bool isPlaying = false, isPaused = false;
		if (channel)
		{
			result = channel->isPlaying(&isPlaying);
			result = channel->getPaused(&isPaused);
		}
		
		if (!isPlaying && !isPaused)
		{
			if (channel != NULL)
			{
				// Detach from old audiosource
				AudioSource* audioSource = AudioSource::GetAudioSourceFromChannel( channel );
				if (audioSource)
				{
					audioSource->Stop(channel);
					result = GetAudioManager().GetFMODSystem()->playSound(FMOD_CHANNEL_FREE, m_Sound, true, &channel);
				}
				else
					result = GetAudioManager().GetFMODSystem()->playSound(FMOD_CHANNEL_FREE, m_Sound, true, &channel);
			}
			else
			{
				result = GetAudioManager().GetFMODSystem()->playSound(FMOD_CHANNEL_FREE, m_Sound, true, &channel);
			}
		
			Assert (result == FMOD_OK);			
			(*it).second = channel;

			if (channel)
				return channel;		
		}
	}
	
	// no sounds available in the pool - create one
	m_Sound = CreateSound();
	
	if (m_Sound)
	{
		FMOD::Channel* channel = GetAudioManager().GetFreeFMODChannel( m_Sound );
		if (channel != NULL)
			m_CachedSounds.push_back( std::make_pair( m_Sound, channel ) );
		return channel;
	}
	
	return NULL;
}

#if UNITY_IPHONE
// IPHONE hardware
// Rationale: Lazy create sound on channel request (Play()) to not hold onto the hw decoder more than needed,
// if the sound is already created, then release it, to give it a chance to get the hw decoder upon create.
// if another stream is owing the hw decoder, fallback on software (CreateFMODSound() handles that)
// This add some latency on Play (but that's acceptable for streams)
FMOD::Channel* AudioClip::CreateIOSStreamChannel()
{
	FMOD::Channel* channel = NULL;
	if (m_UseHardware)
	{
		if (m_Sound) m_Sound->release();
		if (!m_StreamingInfo.IsValid())
		{
			m_Sound = GetAudioManager().CreateFMODSound((const char*)&m_AudioData[0], GetExInfo(), GetMode(), m_UseHardware);
		}
		else 
		{	
			m_Sound = GetAudioManager().CreateFMODSound(m_StreamingInfo.path.c_str(), GetExInfo(), GetMode(), m_UseHardware);
		}
		
		Assert(m_Sound);
		channel = GetAudioManager().GetFreeFMODChannel(m_Sound);
	}
	else
		channel = GetCachedChannel();
	
	return channel;
}
#endif // UNITY_IPHONE

FMOD::Channel* AudioClip::CreateChannel(AudioSource* forSource)  
{
	FMOD::Channel* channel = NULL;
	if (GetMode() & FMOD_CREATESTREAM)
	{	
#if ENABLE_WWW
		if (m_WWWStreamed)
		{
			// Recreate sound if it's streamed WWW/Custom (reusing a sound will trigger a seek - which is not supported) 
			if (m_Sound)
				m_Sound->release();
			m_Sound = CreateSound();
			Assert(m_Sound);
			channel = GetAudioManager().GetFreeFMODChannel(m_Sound);			
		}
		else
#endif // ENABLE_WWW
#if UNITY_IPHONE
			channel = CreateIOSStreamChannel();
#else // UNITY_IPHONE
			channel = GetCachedChannel();
#endif // UNITY_IPHONE	
	}
	else
	{	
		channel = GetAudioManager().GetFreeFMODChannel(m_Sound);
	}
	
	if (channel) channel->setMode(Is3D()?FMOD_3D:FMOD_2D);
	
	return channel;
}

FMOD_CREATESOUNDEXINFO AudioClip::GetExInfo() const
{
	FMOD_CREATESOUNDEXINFO exinfo;
	memset(&exinfo, 0, sizeof(exinfo));
	exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);	
#if UNITY_EDITOR
	exinfo.suggestedsoundtype = m_EditorAudioData.empty()?m_Type:m_EditorSoundType;
	exinfo.format = m_EditorAudioData.empty()?m_Format:m_EditorSoundFormat;
	exinfo.length = m_EditorAudioData.empty()?m_AudioData.size():m_EditorAudioData.size();
#else
	exinfo.suggestedsoundtype = m_Type;
	exinfo.format = m_Format; 
	exinfo.length = m_AudioData.size();
#endif
	
	exinfo.defaultfrequency = m_Frequency;
	exinfo.numchannels = m_Channels;
	if (m_UserGenerated)
	{
		exinfo.length = m_UserLengthSamples * m_Channels * 4;		
		exinfo.pcmreadcallback = AudioClip::ScriptPCMReadCallback; 
		exinfo.pcmsetposcallback = AudioClip::ScriptPCMSetPositionCallback;
	}
	
	if (m_StreamingInfo.IsValid())
	{
		exinfo.length = m_StreamingInfo.size;		
		exinfo.fileoffset = m_StreamingInfo.offset;
	}
	
	exinfo.userdata = (void*)this;
	
	return exinfo;
}

FMOD_MODE AudioClip::GetMode() const
{
	FMOD_MODE mode = (m_3D?FMOD_3D:FMOD_2D) | FMOD_LOOP_NORMAL | FMOD_3D_CUSTOMROLLOFF | FMOD_MPEGSEARCH;

#if UNITY_WII
	mode |= (m_UseHardware || m_Type == FMOD_SOUND_TYPE_GCADPCM) ? FMOD_HARDWARE : FMOD_SOFTWARE;
#else
	mode |= FMOD_SOFTWARE;
#endif


	if (m_ExternalStream)
	{		
		mode |= FMOD_CREATESTREAM;
	}
	else
	if (m_UserGenerated)
	{
		mode |= FMOD_OPENUSER;
		if (m_UserIsStream)
			mode |= FMOD_CREATESTREAM;
	}
	else
	if (m_StreamingInfo.IsValid())
		mode |= FMOD_CREATESTREAM;
	else
	{
		mode |= FMOD_OPENMEMORY;		
		
		if (m_LoadFlag == kCompressedInMemory)
		{		
			// if MP2/MP3, ADPCM, CELT, XMA then we can load the audio compressed directly into FMOD
			if (m_Type == FMOD_SOUND_TYPE_MPEG ||
				m_Type == FMOD_SOUND_TYPE_XMA  ||
				m_Type == FMOD_SOUND_TYPE_GCADPCM ||
				m_Type == FMOD_SOUND_TYPE_WAV ||
				m_Type == FMOD_SOUND_TYPE_AIFF ||
				m_Type == FMOD_SOUND_TYPE_IT ||
				m_Type == FMOD_SOUND_TYPE_XM ||
				m_Type == FMOD_SOUND_TYPE_S3M ||
				m_Type == FMOD_SOUND_TYPE_MOD )
			{
				// From docs - "...Can only be used in combination with FMOD_SOFTWARE...."
				if ((mode & FMOD_SOFTWARE) != 0) mode |= FMOD_CREATECOMPRESSEDSAMPLE;
			}
			else
			// if sound is ogg we have to stream it to FMOD to keep it compressed in memory
			// @TODO use CELT instead of OGG. Soon.
			{
				mode |= FMOD_CREATESTREAM;
			}		
		}
	}

	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
    {
		if(m_Type == FMOD_SOUND_TYPE_MPEG ||
           m_Type == FMOD_SOUND_TYPE_MOD || 
           m_Type == FMOD_SOUND_TYPE_IT ||
           m_Type == FMOD_SOUND_TYPE_S3M ||
           m_Type == FMOD_SOUND_TYPE_XM)
        {
			mode |= FMOD_ACCURATETIME;
        }
    }

	return mode;
}

const UInt8* AudioClip::GetAudioData() const
{
#if UNITY_EDITOR
	if (!m_EditorAudioData.empty())
		return &m_EditorAudioData[0];
	else
#endif
	return &m_AudioData[0];
}

FMOD::Sound* AudioClip::CreateSound() 
{
	// if external streaming (WWW or movie)
	if (m_ExternalStream)
	{		
#if ENABLE_WWW
		if (m_StreamData)
		{
			// Wait for the entire file to download	before reporting ready
			// @TODO we need a proper net stream solution	
			if (!m_WWWStreamed && ( m_StreamData->GetProgress() != 1.0f ))
				return NULL;
					
			return GetAudioManager().CreateFMODSoundFromWWW(m_StreamData,
															   m_3D, m_Type, m_Format, m_Frequency, m_Channels, m_WWWStreamed);
		}
		else 
#endif
			if (m_MoviePlayback)
			{	
				m_Frequency = m_MoviePlayback->GetMovieAudioRate();
				m_Channels = m_MoviePlayback->GetMovieAudioChannelCount();
				if (m_Frequency > 0 && m_Channels > 0)
					return GetAudioManager().CreateFMODSoundFromMovie(this, m_3D);
			}				
	}
	else
	{
		if (m_UserGenerated)
		{
			m_Sound = GetAudioManager().CreateFMODSound(GetName(), GetExInfo(), GetMode(), m_UseHardware);
		}
		else
		if (!m_StreamingInfo.IsValid())
		{
			if (m_AudioData.empty())
				return NULL;
			
			FMOD_MODE mode = GetMode();
			
			m_Sound = GetAudioManager().CreateFMODSound(GetAudioData(), GetExInfo(), mode, m_UseHardware);
			
			// Audio data is only loaded into FMOD once (if its not a stream). We can clear it after upload
#if !UNITY_EDITOR	
			if (!(mode & FMOD_CREATESTREAM))
				m_AudioData.clear();
#endif
		}
		else 
		{	
			// assert that no audio data is loaded in memory
			Assert (m_AudioData.empty());
			
			m_Sound = GetAudioManager().CreateFMODSound(m_StreamingInfo.path.c_str(), GetExInfo(), GetMode(), m_UseHardware);
		}
	}	
	
	return m_Sound;
}

void AudioClip::GetSoundProps()
{
	if (!m_Sound)
		return;
	
	m_Sound->getFormat(m_Type == FMOD_SOUND_TYPE_UNKNOWN? &m_Type : NULL, m_Format == FMOD_SOUND_FORMAT_NONE ? &m_Format : NULL, &m_Channels, &m_BitsPerSample);
	
	float ffrequency;
	m_Sound->getDefaults( &ffrequency,NULL, NULL, NULL);
	
	m_Frequency = (int)ffrequency;		
	
	FMOD_MODE mode;
	m_Sound->getMode(&mode);
	m_3D = (mode & FMOD_3D);
}

void AudioClip::Reload()
{
	if (m_AudioData.size() == 0)
	{
		GetPersistentManager().ReloadFromDisk(this);
		// FYI: ReloadFromDisk() will call LoadSound() and calling LoadSound() twice is a bad thing.
	}
	else
		LoadSound();
}

bool AudioClip::LoadSound()
{
	Cleanup();
	
	AssertBreak(m_Sound == NULL);
	m_Sound = CreateSound();
	
	if (m_Sound == NULL)
	{
		return false;
	}

	m_OpenState = FMOD_OPENSTATE_READY;
	
	if (m_ExternalStream&&!m_Sound)		
	{
		m_OpenState = FMOD_OPENSTATE_CONNECTING;
		return false;
	}
	
	if ((GetMode()&FMOD_CREATESTREAM)&&m_Sound&&!m_UseHardware&&!m_WWWStreamed)
	{
		m_CachedSounds.push_back( std::make_pair( m_Sound, (FMOD::Channel*)NULL) );
	}
	
	GetSoundProps();	
	
#if UNITY_IPHONE
	// if it's a stream and it uses hardware, we release the sound here to relinqiush the hw decoder.
	if (m_UseHardware&&(GetMode()&FMOD_CREATESTREAM))
	{
		m_Sound->release();
		m_Sound = NULL;
	}	
#endif
	
	
	return true;
}

void AudioClip::ReleaseSound()
{
	if (m_Sound) 
		m_Sound->release(); 
	m_Sound = NULL;
}


bool AudioClip::SetAudioDataSwap(dynamic_array<UInt8>& buffer,							  
							 bool threeD,
							 bool hardware,
							 int loadFlag,
							 bool externalStream,
							 FMOD_SOUND_TYPE type,
							 FMOD_SOUND_FORMAT format)
{	
	m_ExternalStream = externalStream;
	m_LoadFlag = loadFlag;
	m_3D = threeD;
	m_UseHardware = hardware;
	m_Type = type;
	m_Format = format;
	Assert(buffer.owns_data());
	m_AudioData.swap(buffer);
	
	#if !UNITY_RELEASE
	// right now we're trying to load sound in AwakeFromLoad (and do only that) - so we skip the call
	HackSetAwakeWasCalled();
	#endif
	
	return LoadSound();
}


bool AudioClip::SetAudioData(const UInt8* buffer,		
							 unsigned size,
							 bool threeD,
							 bool hardware,
							 int loadFlag,
							 bool externalStream,
							 FMOD_SOUND_TYPE type,
							 FMOD_SOUND_FORMAT format)
{	
	m_ExternalStream = externalStream;
	m_LoadFlag = loadFlag;
	m_3D = threeD;
	m_UseHardware = hardware;
	m_Type = type;
	m_Format = format;
	m_AudioData.assign(buffer, buffer + size);
	Assert (m_AudioData.size() == size);
	
	#if !UNITY_RELEASE
	// right now we're trying to load sound in AwakeFromLoad (and do only that) - so we skip the call
	HackSetAwakeWasCalled();
	#endif
	
	return LoadSound();
}



#if UNITY_EDITOR

void AudioClip::SetEditorAudioData( const dynamic_array<UInt8>& buffer, FMOD_SOUND_TYPE type, FMOD_SOUND_FORMAT format )
{
	m_EditorAudioData.assign(&buffer[0], &buffer[0] + buffer.size());
	m_EditorSoundType = type;
	m_EditorSoundFormat = format;
}

bool AudioClip::WriteRawDataToFile( const string& path ) const
{
	// first load entire file into byte array
	// open the file
	std::ofstream file(path.c_str(), std::ios::binary);

	if(!file.is_open())		
	{
		return false; // file problably doesnt exist		
	}

	file.write((const char*)&m_AudioData[0], m_AudioData.size());

	if (!file.good())
	{
		file.close();
		return false;
	}

	file.close();

	return true;
}
#endif

void AudioClip::ConvertOldAsset(int frequency, int size, int channels, int bitsPerSample, UInt8* raw)
{	
	UInt8* data;
	UInt8* wav = CreateWAV(frequency, size, channels, bitsPerSample, &data);
	
	// get header info
	const int wav_size = GetWAVSize(wav);
	
	// copy in raw data
	memcpy(data, raw, size);
	
	m_AudioData.clear();
	m_AudioData.assign( wav, wav + wav_size);
	Assert ( wav_size == m_AudioData.size() );
	m_Channels = channels;
	m_Frequency = frequency;
	m_Format = FMOD_SOUND_FORMAT_PCM16;
	m_Type = FMOD_SOUND_TYPE_WAV;

	delete[] wav;
}


void AudioClip::CreateScriptCallback()
{
	#if ENABLE_SCRIPTING
	#if ENABLE_MONO
	monoDomain = mono_domain_get();
	#endif

	// cache script methods
	ScriptingObjectPtr instance = Scripting::ScriptingWrapperFor(this);
	
	if (instance)
	{
		// cache delegate invokers
		m_CachedPCMReaderCallbackMethod = GetScriptingMethodRegistry().GetMethod( "UnityEngine", "AudioClip", "InvokePCMReaderCallback_Internal" );
		m_CachedSetPositionCallbackMethod = GetScriptingMethodRegistry().GetMethod ( "UnityEngine", "AudioClip", "InvokePCMSetPositionCallback_Internal" );			
	}
	#endif
}


template<class TransferFunc>
void AudioClip::TransferToFlash(TransferFunc& transfer)
{
	Assert(transfer.IsWriting());
	int count = GetSampleCount();
	transfer.Transfer(count, "samplecount");
	transfer.Transfer (m_3D, "m_3D");
	if (transfer.GetBuildingTarget().platform==kBuildWebGL)
		// Since we can't do unaligned 
		transfer.Align();
	transfer.Transfer(m_AudioData, "m_AudioData");
	
}

template<class TransferFunc>
void AudioClip::Transfer (TransferFunc& transfer) {
	Super::Transfer (transfer);
	
	//if we're exporting to flash, write out data completely differently
	if (transfer.IsWritingGameReleaseData () && (transfer.GetBuildingTarget().platform==kBuildFlash || transfer.GetBuildingTarget().platform==kBuildWebGL))
	{
		TransferToFlash(transfer);
		return;
	}

	// 10/06-11: v4: Added editor only audio data
	// 25/05-10: v4: Stream replaces decompressOnLoad
	// 18/04-09: v3: FMOD asset data
	//             : v1-2: OpenAL asset data
	transfer.SetVersion (4);
	
	transfer.Transfer ((SInt32&)m_Format, "m_Format", kNotEditableMask);

	if (transfer.IsCurrentVersion())
	{
		transfer.Transfer ((SInt32&)m_Type, "m_Type", kNotEditableMask);
		transfer.Transfer (m_3D, "m_3D", kNotEditableMask); 
		transfer.Transfer (m_UseHardware, "m_UseHardware", kNotEditableMask);
		
		transfer.Align();		
		
		transfer.Transfer (m_LoadFlag, "m_Stream", kNotEditableMask);
		
		if (m_LoadFlag == kStreamFromDisc)
			transfer.EnableResourceImage(kStreamingResourceImage);
		
		if (!transfer.ReadStreamingInfo (&m_StreamingInfo))
			transfer.Transfer(m_AudioData, "m_AudioData");

		TRANSFER_EDITOR_ONLY(m_EditorAudioData);
		TRANSFER_EDITOR_ONLY((SInt32&)m_EditorSoundType);
		TRANSFER_EDITOR_ONLY((SInt32&)m_EditorSoundFormat);
	}
	else if (transfer.IsOldVersion(1) || transfer.IsOldVersion(2))
	{   // old/openal data
		SInt32 oldFormat = m_Format;
		m_Format = FORMAT_TO_FMOD_FORMAT[ oldFormat ];
		m_Type = FORMAT_TO_FMOD_TYPE[ oldFormat ];		
		
		if (transfer.IsOldVersion (2))
			transfer.Transfer (m_Frequency, "m_Frequency", kNotEditableMask);
		else
			transfer.Transfer (m_Frequency, "m_Freq", kNotEditableMask);
		unsigned size = 0;
		transfer.Transfer (size, "m_Size", kNotEditableMask);	

		
		// assumes that m_size is the sizes in bytes
		transfer.TransferTypeless (&size, "audio data", kHideInEditorMask);
				
		// clear audio data
		m_AudioData.clear();
		m_AudioData.resize_uninitialized(size);				
		
		// transfer data
		transfer.TransferTypelessData (size, &m_AudioData[0]);
		
		// data always have to be in little endian (WAV format)
#if !UNITY_BIG_ENDIAN
		if (transfer.ConvertEndianess ())
		{
			if (m_Type != FMOD_SOUND_TYPE_OGGVORBIS && m_Format == FMOD_SOUND_FORMAT_PCM16)
				SwapEndianArray (&m_AudioData[0], 2, size / 2);
		}
#else		
		if (!transfer.ConvertEndianess ())
		{
			if (m_Type != FMOD_SOUND_TYPE_OGGVORBIS && m_Format == FMOD_SOUND_FORMAT_PCM16)
				SwapEndianArray (&m_AudioData[0], 2, size / 2);
		}	
#endif
		
		bool decompressOnLoad = false;
		transfer.Transfer (decompressOnLoad, "m_DecompressOnLoad", kNotEditableMask);
		m_LoadFlag = decompressOnLoad ? kDecompressOnLoad : kCompressedInMemory;
		
		// make a qualified guess for old asset data
		//kMono8 = 1, kMono16, kStereo8, kStereo16, kOggVorbis
		m_Channels = 2;
		if (oldFormat == 1 || oldFormat == 2)
			m_Channels = 1;
		if (oldFormat == 5)
		{
			// we don't have any option but parsing the ogg file to obtain the channel count
			// use our own ogg reader for that
			int vorbisChannels = 0;
			if (CheckOggVorbisFile (&m_AudioData[0], m_AudioData.size(), &vorbisChannels))
				m_Channels = vorbisChannels;
		}
		
		// mono sounds were always 3D in OpenAL (in <=Unity 2.5) and 2D sounds not
		m_3D = m_Channels == 1;  
		
		// convert old data to new
		if (oldFormat != 5) 
			ConvertOldAsset(m_Frequency, size, m_Channels, 16, &m_AudioData[0]);
	}
	else if (transfer.IsOldVersion(3)) //	FMOD version
	{
		transfer.Transfer ((SInt32&)m_Type, "m_Type", kNotEditableMask);
		
		transfer.Transfer (m_3D, "m_3D", kNotEditableMask); 	
		
		transfer.Align();
		
		transfer.Transfer(m_AudioData, "m_AudioData");
				
		bool decompressOnLoad = false;
		transfer.Transfer (decompressOnLoad, "m_DecompressOnLoad", kNotEditableMask);
		m_LoadFlag = decompressOnLoad ? kDecompressOnLoad : kCompressedInMemory;
	}
}

void AudioClip::AwakeFromLoadThreaded ()
{
	Super::AwakeFromLoadThreaded();
	LoadSound();
}


void AudioClip::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	
	// Load data if not done from another thread already
	if ((awakeMode & kDidLoadThreaded) == 0)
	{
		LoadSound();
	}
}

IMPLEMENT_CLASS_HAS_INIT (AudioClip)
IMPLEMENT_OBJECT_SERIALIZE (AudioClip)

#endif //ENABLE_AUDIO
