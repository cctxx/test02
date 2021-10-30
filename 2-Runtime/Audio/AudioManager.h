#ifndef AUDIOMANAGER_H
#define AUDIOMANAGER_H

#include "Configuration/UnityConfigure.h"
#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Profiler/ProfilerStats.h"
#include "Runtime/Misc/Player.h"
#include "Runtime/Audio/AudioTypes.h"
#include "Runtime/Mono/MonoIncludes.h"
#if ENABLE_AUDIO_FMOD
#include "Runtime/Audio/AudioScriptBufferManager.h"
#endif

#if ENABLE_AUDIO

#if UNITY_FLASH || UNITY_WEBGL
#include "PlatformDependent/FlashSupport/cpp/AudioChannel.h"
#endif

#ifndef FMOD_ASSERT
#define FMOD_ASSERT(x) {Assert(x == FMOD_OK);\
if (x != FMOD_OK){	ErrorString(FMOD_ErrorString(x));}}
#endif


#include "Runtime/Audio/correct_fmod_includer.h" // can't forward declare enums (@TODO use ints?)

class AudioSource;
class AudioListener;
class AudioClip;
class WWW;
class AudioFilter;
class AudioReverbZone;
struct MonoDomain;
struct MonoThread;
#if ENABLE_AUDIO_FMOD
namespace FMOD { class System; class Sound; class Channel; class ChannelGroup; }
#endif
namespace Unity { class GameObject; }


enum{
	kVelocityUpdateModeAuto = 0,
	kVelocityUpdateModeFixed = 1,
	kVelocityUpdateModeDynamic = 2,
};


class AudioManager : public GlobalGameManager
{
public:
	REGISTER_DERIVED_CLASS   (AudioManager, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE   (AudioManager)

	AudioManager (MemLabelId label, ObjectCreationMode mode);
	// virtual ~AudioManager (); declared-by-macro

#if UNITY_WII
	void UpdateOnDiskEject();
#endif
	void Update ();	
	void FixedUpdate ();	

	void AddAudioSource (AudioSource* s, bool paused);
	void RemoveAudioSource (AudioSource* s);		
	void StopSources();
	
	void AddAudioListener (AudioListener* newListener);
	void RemoveAudioListener (AudioListener* newListener);
	AudioListener* GetAudioListener() const;
	

	
	void SetVolume (float volume);
	float GetVolume () const;
	void SetPause (bool pause);
	bool GetPause () const { return m_IsPaused; }
	int GetAutomaticUpdateMode(Unity::GameObject *go);
	
	// Manager implementation
	void AwakeFromLoad (AwakeFromLoadMode awakeMode);	
	static void InitializeClass ();
	static void CleanupClass ();
	
	void UpdateListener(
						const Vector3f& position, 
						const Vector3f& velocity, 
						const Vector3f& up,
						const Vector3f& forward);	

	struct AudioScheduledSource : public ListElement
	{
		AudioScheduledSource(AudioSource* src) : source(src), time(0.0) {};
		AudioSource* source;
		double time;
	};
	
private:

	float m_DefaultVolume;
	float m_Volume;
	float m_Rolloffscale;
	bool  m_IsPaused;
	
	typedef List< ListNode<AudioSource> > TAudioSources;
	typedef List< ListNode<AudioListener> > TAudioListeners;
	typedef List<AudioScheduledSource> TScheduledSources;
	typedef TAudioSources::iterator TAudioSourcesIterator;
	typedef TAudioListeners::iterator TAudioListenersIterator;
	typedef TScheduledSources::iterator TScheduledSourcesIterator;
	
	TAudioSources m_Sources; 
	TAudioSources m_PausedSources; 
	TAudioListeners m_Listeners;
	TScheduledSources m_ScheduledSources;
	
	void ProcessScheduledSources();
#if ENABLE_MICROPHONE
	FMOD::Sound* CreateSound (int deviceID, int lengthSec, int frequency);
#endif // ENABLE_MICROPHONE

public:
	float GetRolloffScale() const { return m_Rolloffscale; }
	bool IsAudioDisabled() const { return m_DisableAudio; }

	/// Schedule source to be played in sync. In this frame (if delay==0) or later
	inline UInt64 GetAccumulatedPauseTicks() const { return m_accPausedTicks; }
	void ScheduleSource(AudioSource* s, double time);
	void UnScheduleSource(AudioSource* s);
	double GetDSPTime() const;

#if ENABLE_AUDIO_FMOD
	void AddAudioReverbZone(AudioReverbZone* z);
	void RemoveAudioReverbZone(AudioReverbZone* z);
	
	// FMOD
	bool ValidateFMODResult(FMOD_RESULT result, const char* errmsg);
	bool InitFMOD();
	void InitScriptBufferManager();
	void ReloadFMODSounds();
	
	bool InitNormal();
#ifdef SUPPORT_REPRODUCE_LOG
	bool InitReproduction();
#endif

	void CloseFMOD();
	FMOD::System* GetFMODSystem() const { return m_FMODSystem;}
	static FMOD_RESULT F_CALLBACK systemCallback(FMOD_SYSTEM* c_system, FMOD_SYSTEM_CALLBACKTYPE type, void* data1, void* data2);
	
	// groups
	FMOD::ChannelGroup* GetChannelGroup_FX_IgnoreVolume() const { return m_ChannelGroup_FX_IgnoreVolume; }
	FMOD::ChannelGroup* GetChannelGroup_FX_UseVolume() const { return m_ChannelGroup_FX_UseVolume; }
	FMOD::ChannelGroup* GetChannelGroup_NoFX_IgnoreVolume() const { return m_ChannelGroup_NoFX_IgnoreVolume; }
	FMOD::ChannelGroup* GetChannelGroup_NoFX_UseVolume() const { return m_ChannelGroup_NoFX_UseVolume; }
	

	// FMOD profiling
	int GetMemoryAllocated() const;
	float GetCPUUsage() const;
	#if ENABLE_PROFILER
	void GetProfilerData( AudioStats& audioStats );
	#endif


	FMOD::Sound* CreateFMODSound(const void* buffer, FMOD_CREATESOUNDEXINFO exInfo, FMOD_MODE mode, bool useHardwareDecoder);
	
	#if UNITY_EDITOR
	FMOD::Sound* CreateFMODSound(const std::string& path, bool threeD,
								 bool hardware, bool openOnly = false );
	#endif
	
	
	FMOD::Sound* CreateFMODSoundFromWWW(WWW* webStream,
										 bool threeD,
										 FMOD_SOUND_TYPE suggestedtype,
										 FMOD_SOUND_FORMAT format,
										 unsigned freq,
										 unsigned channels,
										 bool stream);
	
	FMOD::Sound* CreateFMODSoundFromMovie(AudioClip* clip, bool threeD); 
	
	FMOD::Channel* GetFreeFMODChannel(FMOD::Sound* sound, bool paused = true); 
	
	unsigned GetPlayingChannelsForSound(FMOD::Sound* sound);
	const std::string& GetLastError() const { return m_LastErrorString; }	
	
	FMOD_SPEAKERMODE GetSpeakerMode() const { return m_speakerMode; }
	FMOD_SPEAKERMODE GetSpeakerModeCaps() const { return m_speakerModeCaps; }
	void SetSpeakerMode(FMOD_SPEAKERMODE speakerMode);
	
public:
	
#if ENABLE_MICROPHONE
	// Microphone
	const std::vector<std::string> GetRecordDevices() const;
	bool EndRecord(int deviceID);
	PPtr<AudioClip> StartRecord(int deviceID, bool loop, int lengthSec, int frequency);
	bool IsRecording(int deviceID) const;
	unsigned GetRecordPosition(int deviceID) const;
	int GetMicrophoneDeviceIDFromName(const std::string& name) const;
	void GetDeviceCaps(int deviceID, int *minFreq, int *maxFreq) const;

	mutable std::map<std::string, int> m_MicrophoneNameToIDMap;
#endif // ENABLE_MICROPHONE

	

private:	
	float m_SpeedOfSound;
	float m_DopplerFactor;

	typedef List< ListNode<AudioReverbZone> > TAudioReverbZones;
	typedef TAudioReverbZones::iterator TAudioReverbZonesIterator;
	TAudioReverbZones m_ReverbZones;

	// FMOD
	FMOD::System* m_FMODSystem;
	
	/*                                         +----------------+
	 *                                         | NoFX_UseVolume |
	 *                                        /+----------------+
	 *                 _+-------------------+/ 
	 *                / | NoFX_IgnoreVolume |<---EditorChannel plays directly into this group
	 *  +-----------+/  +-------------------+                  
	 *  |FMOD Master|                                             ...............
	 *  +-----------+\  +------------------+                      : AudioSource :
	 *     ^          \_| FX_IgnoreVolume  |                    __:__+-----+    :
	 *     |            +------------------+\ +--------------+ /  :  | Wet |    :
	 *     |                                 \| FX_UseVolume |/   :  +-----+    :
	 *     |                                  +--------------+\   :             :
	 *     Zone reverbs                                        \__:__+-----+    :
	 *                                                            :  | Dry |    :
	 *                                                            :  +-----+    :
	 *                                                            ...............
	 *       The AudioSource's Wet and Dry groups can be attached to the Listener, IgnoreVolume, IgnoreVolNoFX, or ListenerNoFX groups depending on the combinations of the bypassEffects and bypassListenerEffects properties.
	 *       Note that zone reverbs are applied by FMOD after the downmix of the master channel group, so in order to avoid reverb on previews the ignoreVolumeGroupNoFX uses FMOD's overrideReverbProperties to turn off the reverb.
	 */
	
	FMOD::ChannelGroup* m_ChannelGroup_FMODMaster;

	FMOD::ChannelGroup* m_ChannelGroup_FX_IgnoreVolume;
	FMOD::ChannelGroup* m_ChannelGroup_FX_UseVolume;
	FMOD::ChannelGroup* m_ChannelGroup_NoFX_IgnoreVolume;
	FMOD::ChannelGroup* m_ChannelGroup_NoFX_UseVolume;
	
	FMOD_SPEAKERMODE m_speakerMode; ///< TransferName{Default Speaker Mode} enum { Raw = 0, Mono = 1, Stereo = 2, Quad = 3, Surround = 4, Surround 5.1 = 5, Surround 7.1 = 6, Prologic DTS = 7 }
	FMOD_SPEAKERMODE m_activeSpeakerMode;
	FMOD_CAPS m_driverCaps;
	FMOD_SPEAKERMODE m_speakerModeCaps;
	
	// Phone DSP buffer size
	int m_DSPBufferSize; ///< enum { Default = 0, Best latency = 256, Good latency = 512, Best performance = 1024 }
	
	// error handling	
	std::string m_LastErrorString;
	FMOD_RESULT m_LastFMODErrorResult;
	
private:
	AudioScriptBufferManager* m_ScriptBufferManager;

public:
	AudioScriptBufferManager& GetScriptBufferManager();
	AudioScriptBufferManager* GetScriptBufferManagerPtr();

#if UNITY_EDITOR
public:
	void PlayClip(AudioClip& clip, int startSample = 0, bool loop = false, bool twoD = true);
	void StopClip(const AudioClip& clip);
	void PauseClip(const AudioClip& clip);
	void ResumeClip(const AudioClip& clip);
	bool IsClipPlaying(const AudioClip& clip);
	void StopAllClips();
	float GetClipPosition(const AudioClip& clip);	
	unsigned int GetClipSamplePosition(const AudioClip& clip);	
	void SetClipSamplePosition(const AudioClip& clip, unsigned int iSamplePosition);
	void LoopClip(const AudioClip& clip, bool loop);
	void ListenerCheck();

private:
	FMOD::Channel* m_EditorChannel;
#endif //UNITY_EDITOR
#endif //ENABLE_AUDIO_FMOD
	UInt64 m_accPausedTicks;
	UInt64 m_pauseStartTicks;
	bool m_DisableAudio; // Completely disable audio (in standalone builds only)
};

AudioManager& GetAudioManager ();
AudioManager* GetAudioManagerPtr ();

#endif // ENABLE_AUDIO
#endif // AUDIOMANAGER_H
