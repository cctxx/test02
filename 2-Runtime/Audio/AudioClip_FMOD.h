#ifndef AUDIOCLIP_FMOD_H
#define AUDIOCLIP_FMOD_H

#if ENABLE_AUDIO

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Audio/correct_fmod_includer.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Serialize/CacheWrap.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Scripting/ScriptingUtility.h"


#if ENABLE_WWW
class WWW;
#endif

class AudioSource;
class MoviePlayback;
namespace FMOD { class Channel; class Sound; }

const unsigned kAudioQueueSize = (4 * 16384);


class AudioClip : public NamedObject
{
	
public:
	REGISTER_DERIVED_CLASS 	 (AudioClip, NamedObject)
	DECLARE_OBJECT_SERIALIZE (AudioClip)

	AudioClip (MemLabelId label, ObjectCreationMode mode);
	// virtual ~AudioClip (); - declared-by-macro 
	
#if ENABLE_WWW
	// WARNING: don't call AwakeFromLoad if you use InitStream
	bool InitStream (WWW* streamData, MoviePlayback* movie, bool realStream = false, FMOD_SOUND_TYPE fmodSoundType = FMOD_SOUND_TYPE_UNKNOWN );

	// Is reading the data from this clip allowed by webplayer security
	void SetReadAllowed (bool allowed) { m_ReadAllowed = allowed; }
	bool GetReadAllowed () const { return m_ReadAllowed; }
#endif	
	bool InitWSound (FMOD::Sound* sound);
	bool CreateUserSound(const std::string& name, unsigned lengthSamples, short channels, unsigned frequency, bool _3D, bool stream);
	
	void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	void AwakeFromLoadThreaded ();
	
	
public:	
	enum  { kDecompressOnLoad = 0, kCompressedInMemory = 1, kStreamFromDisc };
	
	FMOD_SOUND_TYPE GetType() const { return m_Type; }
	void SetType(FMOD_SOUND_TYPE type) { m_Type = type; } 
	FMOD_SOUND_FORMAT GetFormat() const { return m_Format; }
	
	bool Get3D() const { return m_3D; }
	void Set3D(bool threeD) { m_3D = threeD; }	
	
	FMOD::Channel* CreateChannel(AudioSource* forSource = NULL);
	FMOD::Sound* CreateSound();
	
	unsigned int GetSampleCount() const;
	unsigned int GetChannelCount() const;
	unsigned int GetBitRate() const;
	unsigned int GetFrequency() const;
	unsigned int GetSize() const;
	unsigned int GetLength() const;
	float GetLengthSec() const;
	unsigned int GetBitsPerSample() const;
	MoviePlayback* GetMovie() const;	
	
	void SetData(const float* data, unsigned lengthSamples, unsigned offsetSamples = 0);
	void GetData(float* data, unsigned lengthSamples, unsigned offsetSamples = 0) const;
	
	virtual int GetRuntimeMemorySize () const;
	
	void AddSyncPoint( string name, UInt32 atSample );
	
	bool Is3D() const { return m_3D; }
	bool IsMovieAudio() const { return m_MoviePlayback != NULL; }
	FMOD::Sound* GetSound() const { return m_Sound; }
	void ReleaseSound();	
	
	bool IsOggCompressible() const;
	
	bool IsStream() const { return (GetMode()&FMOD_CREATESTREAM); }
	
	bool IsWWWStreamed() const { return m_WWWStreamed; }

	bool IsFMODStream() const;

	bool ReadyToPlay();
	
	int GetLoadFlag() const { return m_LoadFlag; }
	void SetLoadFlag( unsigned flag ) { Assert(flag < 3); m_LoadFlag = flag; }
	
	bool IsHardware() const { return m_UseHardware; }
	
	// Attached movie clip
	void SetMoviePlayback(MoviePlayback* movie);
	MoviePlayback* GetMoviePlayback() const;
	
	/**
	 * Queue data in to clip
	 * This is read by streamed sound
	 * @param buffer Audio data to queue
	 * @param size Size of Audio data
	 **/
	bool QueueAudioData(void* buffer, unsigned size);
	
	/**
	 * Top audio data from the quene
	 * @note buf must allocate size bytes
	 * @param size The size in bytes you want to top
	 * @return The audio data
	 **/
	bool GetQueuedAudioData(void** buf, unsigned size);
	
	void ClearQueue();
	
	// Set audiodata
	// WARNING: don't call AwakeFromLoad if you use SetAudioData
	bool SetAudioDataSwap(dynamic_array<UInt8>& buffer,							  
					  bool threeD,
					  bool hardware,
					  int loadFlag,
					  bool externalStream,
					  FMOD_SOUND_TYPE type,
					  FMOD_SOUND_FORMAT format);
	
	
	bool SetAudioData(const UInt8* buffer,
					  unsigned size,
					  bool threeD,
					  bool hardware,
					  int loadFlag,
					  bool externalStream,
					  FMOD_SOUND_TYPE type,
					  FMOD_SOUND_FORMAT format);
	
	// Set audiodata for playing in the editor
	void SetEditorAudioData(const dynamic_array<UInt8>& buffer, FMOD_SOUND_TYPE type, FMOD_SOUND_FORMAT);
	
	
#if UNITY_EDITOR
	bool WriteRawDataToFile(const std::string& path) const;
#endif	
	
	static void InitializeClass ();
	static void CleanupClass ();
	
	void Cleanup();
	void Reload();
	
	static FMOD_SOUND_TYPE GetFormatFromExtension(const std::string& ext);
	static bool IsFormatSupportedByPlatform(const std::string& ext);
	
private:
	// Serialized data 
	int	m_Frequency;
	FMOD_SOUND_TYPE m_Type;
	FMOD_SOUND_FORMAT m_Format;
	int	m_Channels;
	bool m_3D; 	
	bool m_UseHardware; ///< IPhone only (for now)
	int	m_BitsPerSample;
	typedef dynamic_array<UInt8> TAudioData;
	TAudioData m_AudioData; // contains the entire audio file
#if UNITY_EDITOR	
	TAudioData m_EditorAudioData; // if m_AudioData contains "undecodable" data (XMA, M4A) use this instead (usually OGGVORBIS)
	FMOD_SOUND_TYPE m_EditorSoundType;
	FMOD_SOUND_FORMAT m_EditorSoundFormat;
#endif
	
	// buffer
	typedef UNITY_VECTOR(kMemAudioData, UInt8) TAudioQueue;
	TAudioQueue m_AudioBufferQueue;
	Mutex m_AudioQueueMutex;
	
	// Load flag
	int	  m_LoadFlag; 
	
	StreamingInfo m_StreamingInfo;
	
	bool m_UserGenerated;
	unsigned m_UserLengthSamples;
	bool m_UserIsStream;
	
#if ENABLE_WWW
	WWW *m_StreamData; 
	bool m_ReadAllowed;
#endif
	
	bool m_ExternalStream;
	
	//this is used, if this clip is tied to a movie to control playback
	MoviePlayback *m_MoviePlayback;
	
private:
	FMOD::Sound* m_Sound;
	FMOD_OPENSTATE m_OpenState;
	
	// Script callbacks
	#if ENABLE_MONO
	MonoDomain* monoDomain;
	#endif
	ScriptingArrayPtr m_PCMArray;
	int m_PCMArrayGCHandle;
	ScriptingMethodPtr m_CachedPCMReaderCallbackMethod;	
	ScriptingMethodPtr m_CachedSetPositionCallbackMethod;
	
	ScriptingArrayPtr GetScriptPCMArray(unsigned length);
	unsigned m_DecodeBufferSize;
	
private:
	// Cached sounds for streamed audio.
	// FMOD doesn't support multiple channels for a streamed sound (due to file ptrs, buffers)	
	typedef	std::vector<std::pair<FMOD::Sound*, FMOD::Channel*> > TFMODSounds;
	mutable TFMODSounds m_CachedSounds;
	
	bool LoadSound();
	void GetSoundProps();
	FMOD::Channel* GetCachedChannel();
	FMOD_CREATESOUNDEXINFO GetExInfo() const;
	FMOD_MODE GetMode() const;
	const UInt8* GetAudioData() const;
	
#if UNITY_IPHONE
	FMOD::Channel* CreateIOSStreamChannel();
#endif
	
	/**
	 * Convert old asset data to new (by reconstructing the a wav header)
	 * @param frequency Frequency
	 * @param size The size of the data
	 * @param data Array of raw data
	 **/
	void ConvertOldAsset(int frequency, int size, int channels, int bitsPerSample, UInt8* raw);
	
	template<class TransferFunc>
	void TransferToFlash(TransferFunc& transfer);
	
	void CreateScriptCallback();
private:
	// Streaming
	bool m_WWWStreamed;
	
	struct movieUserData {
		MoviePlayback* movie;
		unsigned read;
	};
	
#if ENABLE_WWW
	struct wwwUserData {
		bool seek;
		WWW* stream;
		unsigned pos;
		unsigned filesize;
	};
	
	
	// Callbacks for WWW streaming
	static FMOD_RESULT F_CALLBACK WWWOpen( 
										  const char *  www, // WWW class pointer
										  int  unicode,  
										  unsigned int *  filesize,  
										  void **  handle,  
										  void **  userdata 
										  );
	
	static FMOD_RESULT F_CALLBACK WWWClose( 
										   void *  handle,  
										   void *  userdata 
										   );
	static FMOD_RESULT F_CALLBACK WWWRead( 
										  void *  handle,  
										  void *  buffer,  
										  unsigned int  sizebytes,  
										  unsigned int *  bytesread,  
										  void *  userdata 
										  );
	static FMOD_RESULT F_CALLBACK WWWSeek( 
										  void *  handle,  
										  unsigned int  pos,  
										  void *  userdata 
										  );
#endif
	// Callbacks for movie streaming
	static FMOD_RESULT F_CALLBACK movieopen( 
											const char *  buffer,  
											int  unicode,  
											unsigned int *  filesize,  
											void **  handle,  
											void **  userdata 
											);
	
	static FMOD_RESULT F_CALLBACK movieclose( 
											 void *  handle,  
											 void *  userdata 
											 );
	static FMOD_RESULT F_CALLBACK movieread( 
											void *  handle,  
											void *  buffer,  
											unsigned int  sizebytes,  
											unsigned int *  bytesread,  
											void *  userdata 
											);
	static FMOD_RESULT F_CALLBACK movieseek( 
											void *  handle,  
											unsigned int  pos,  
											void *  userdata 
											);
	
	
	// Callbacks for PCM streaming
	static FMOD_RESULT F_CALLBACK pcmread( 
										  FMOD_SOUND *  sound,  
										  void *  data,  
										  unsigned int  datalen 
										  );
	
	// Callbacks for PCM streaming
	static FMOD_RESULT F_CALLBACK ScriptPCMReadCallback( 
														FMOD_SOUND *  sound,  
														void *  data,  
														unsigned int  datalen 
														);
	
	static FMOD_RESULT F_CALLBACK ScriptPCMSetPositionCallback(
															   FMOD_SOUND *  sound, 
															   int  subsound, 
															   unsigned int  position, 
															   FMOD_TIMEUNIT  postype);
	
	
	friend class AudioManager;	

#if ENABLE_PROFILER
	static int s_AudioClipCount;
#endif
};

#endif //ENABLE_AUDIO
#endif
