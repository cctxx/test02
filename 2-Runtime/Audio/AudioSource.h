#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#if ENABLE_AUDIO
#include "Runtime/GameCode/Behaviour.h"
#include "AudioParameters.h"
#include "AudioSourceFilter.h"
#include "Runtime/Math/Vector3.h"
#include <vector>
#include "AudioClip.h"
#include "AudioLowPassFilter.h"
#include "AudioBehaviour.h"
#if UNITY_FLASH
#include "PlatformDependent/FlashSupport/cpp/AudioChannel.h"
#endif


class AudioSource : public AudioBehaviour
{
public:

	REGISTER_DERIVED_CLASS   (AudioSource, AudioBehaviour)
	DECLARE_OBJECT_SERIALIZE (AudioSource)

	/**
	 * Construction/Destruction
	 **/
	AudioSource (MemLabelId label, ObjectCreationMode mode);
	// virtual ~AudioSource (); declared-by-macro

	
	/**
	 * Transport
	 **/
	/// Plays a sound one time, then forgets about it (You cannot stop a one shot sound)
	void PlayOneShot (AudioClip& clip, float volumeMultiplier = 1.0F);
	/// Pause one shot sounds
	void PauseOneShots ();
	/// Resumes one shot sounds
	void ResumeOneShots ();
	/// Plays the active audioclip at (future) scheduled time. If time < 0 it specifies a delay
	void Play(double time = 0.0);
	/// Pauses the active audioclip
	void Pause ();
	/// Stops the active audio clip
	void Stop (bool stopOneShots);
	/// Stops a specific channel
	void Stop(AudioChannel* channel);
	/// Is the audio source currently playing? (Only looks at the main audio source, OneShots are ignored)
	bool IsPlaying () const;
	/// Is the audio source currently paused? (Only looks at the main audio source, OneShots are ignored)
	bool IsPaused () const;

	bool IsPlayingScripting ();
	
	// positions
	// seconds
	float GetSecPosition() const;
	void SetSecPosition(float secPosition);

	UInt32 GetSamplePosition() const;
	void SetSamplePosition(UInt32 position);
	void SetScheduledStartTime(double time);
	void SetScheduledEndTime(double time);
	void CorrectScheduledTimeAfterUnpause(UInt64 delay);
	
	// Get Length
	float GetLength() const;
	
	/// Get/Set PlayOnAwake
	bool GetPlayOnAwake() const { return m_PlayOnAwake; }
	void SetPlayOnAwake(bool playOnAwake);	
	bool GetIgnoreListenerPause() const { return m_AudioParameters.ignoreListenerPause; }
	void SetIgnoreListenerPause(bool ignoreListenerPause);	
	bool HasScheduledStartDelay() const { return m_HasScheduledStartDelay; }
	bool HasScheduledEndDelay() const { return m_HasScheduledEndDelay; }
	bool HasScheduledTime() const { return m_HasScheduledStartDelay | m_HasScheduledEndDelay; }
	
	/**
	 * Behaviour implementation
	 **/
	virtual void Deactivate (DeactivateOperation operation);
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	virtual void CheckConsistency ();
	virtual void Update ();
	virtual void FixedUpdate ();

	static void InitializeClass ();	
	static void CleanupClass();
	
	/**
	 *
	 **/
	void AddToManager();
	void RemoveFromManager();
	
	void OnAddComponent();
	
	
	virtual void Reset();
	
	void Cleanup();	

public:
	// GET/SETTERS	
	bool GetLoop () const;
	void SetLoop (bool loop);
	
	/// Get/Set pitch of the sound 
	float GetPitch() const;
	void  SetPitch(float pitch);
	
	// Get/Set volume of the sound
	float GetVolume() const;
	void  SetVolume(float volume);
	
	// Sets how much the 3d engine has an effect on the channel.
	float GetPanLevel() const;
	void SetPanLevel(float level);
	
	// Sets the doppler scale for this AudioSource
	float GetDopplerLevel() const;
	void SetDopplerLevel(float level);
	
	// Sets the spread angle of a 3d stereo or multichannel sound in speaker space.
	// 0 = all sound channels are located at the same speaker location and is 'mono'. 
	// 360 = all subchannels are located at the opposite speaker location to the speaker location that it should be according to 3D position. Default = 0.  
	float GetSpread() const;
	void SetSpread(float spread);
	
	// Sets the priority of the [[AudioSource]]
	// Unity is virtualizing AudioSources, when there's more AudioSources playing than available hardware channels. 
	// The AudioSources with lowest priority (and audibility) is virtualized first.
	// Priority is an integer between 0 and 256. 0=highest priority, 256=lowest priority
	int GetPriority() const;
	void SetPriority(int priority);
	
	// Un- / Mutes the AudioSource. Mute sets the volume=0, Un-Mute restore the original volume.
	bool GetMute() const;
	void SetMute(bool mute);
	
	// Within the Min distance the AudioSource will cease to grow louder in volume. 
	// Outside the min distance the volume starts to attenuate.
	float GetMinDistance() const;
	void SetMinDistance(float minDistance);
	
	// (Logarithmic rolloff) MaxDistance is the distance a sound stops attenuating at.
	// (Linear rolloff) MaxDistance is the distance where the sound is completely inaudible.
	float GetMaxDistance() const;
	void SetMaxDistance(float maxDistance);
	
	// Inside cone angle, in degrees. This is the angle within which the sound is at its normal volume. 
	// Must not be greater than outsideconeangle. Default = 360.
	float GetInsideConeAngle() const;
	void SetInsideConeAngle(float angle);
	
	/// Outside cone angle, in degrees. This is the angle outside of which the sound is at its outside volume. 
	/// Must not be less than insideconeangle. Default = 360. 
	float GetOutsideConeAngle() const;
	void SetOutsideConeAngle(float angle);
		
	/// Cone outside volume, from 0 to 1.0. Default = 1.0.  
	float GetOutsideConeVolume() const; 
	void SetOutsideConeVolume(float volume); 
	
	/// Set/Get rolloff mode
	RolloffMode GetRolloffMode() const;
	void SetRolloffMode(RolloffMode mode);
	
	/// Set/Get Custom rolloff curve
	AnimationCurve& GetCustomRolloffCurve();
	const AnimationCurve& GetCustomRolloffCurve() const;
	void SetCustomRolloffCurve(const AnimationCurve&);
	
	/// Set/Get PanLevel distance curve
	AnimationCurve& GetCustomPanLevelCurve();
	const AnimationCurve& GetCustomPanLevelCurve() const;
	void SetCustomPanLevelCurve(const AnimationCurve&);
	
	/// Set/Get spread distance curve
	AnimationCurve& GetCustomSpreadCurve();
	const AnimationCurve& GetCustomSpreadCurve() const;
	void SetCustomSpreadCurve(const AnimationCurve&);
		
	/// Sets a audiosource pan position linearly. Only works for 2D clips.
	/// -1.0 to 1.0. -1.0 is full left. 0.0 is center. 1.0 is full right.
	/// Only sounds that are mono or stereo can be panned. Multichannel sounds (ie >2 channels) cannot be panned.
	float GetPan() const;
	void SetPan(float pan);

	/// Bypass/ignore any applied effects on AudioSource
	bool GetBypassEffects() const;
	void SetBypassEffects(bool bypassEffect);		

	/// Bypass/ignore any applied effects on listener
	bool GetBypassListenerEffects() const;
	void SetBypassListenerEffects(bool bypassListenerEffects);		

	/// Bypass effect of reverb zones on this AudioSource
	bool GetBypassReverbZones() const;
	void SetBypassReverbZones(bool bypassReverbZones);

#if ENABLE_AUDIO_FMOD		
	/// Gets the current output pcm data
	void GetOutputData(float* samples, int numSamples, int channelOffset);
	/// Gets the current spectrum data
	void GetSpectrumData(float* samples, int numSamples, int channelOffset, FMOD_DSP_FFT_WINDOW windowType);
#endif
	/// Sets the currently active audio clip
	void 	SetAudioClip(AudioClip *clip);
	AudioClip *GetAudioClip () const {return m_AudioClip; }	
	
	int GetVelocityUpdateMode() const         { return m_VelocityUpdateMode; }
	void SetVelocityUpdateMode(int update) { m_VelocityUpdateMode=update; }	
	
	bool GetIgnoreListenerVolume() const { return m_IgnoreListenerVolume; }
	void SetIgnoreListenerVolume(bool ignore);

private:
#if DOXYGEN
	int Priority; ///< Sets the priority of the source. A sound with a lower priority will more likely be stolen by high priorities sounds.
	float DopplerLevel; ///< Sets the specific doppler scale for the source.
	float MinDistance; ///< Within the minDistance, the volume will stay at the loudest possible.  Outside of this mindistance it begins to attenuate.
	float MaxDistance; ///< MaxDistance is the distance a sound stops attenuating at. 
	float Pan2D; ///< Sets a source's pan position linearly. Only applicable on 2D sounds.
	
	float m_Pitch; ///< Sets the frequency of the sound. Use this to slow down or speed up the sound.
	float m_Volume; ///< Sets the volume of the sound. 
	
	// rolloff
	RolloffMode rolloffMode; ///< enum { Logarithmic Rolloff=0, Linear Rolloff, Custom Rolloff }
	
	bool Loop; ///< Set the source to loop. If loop points are defined in the clip, these will be respected.
	bool Mute; ///< Mutes the sound.
	
	bool BypassEffects;	///< Bypass/ignore any applied effects on AudioSource
	bool BypassListenerEffects;	///< Bypass/ignore any applied effects from listener
	bool BypassReverbZones;	///< Bypass/ignore any reverb zones
	bool IgnoreListenerPause; ///< Allow source to play even though AudioListener is paused (for GUI sounds)

#else
	AudioParameters m_AudioParameters;	
#endif
	
	
private:
	/**
	 * OneShots
	 **/
	struct OneShot
	{
		AudioChannel* channel;
		AudioClip* clip;
		float volumeScale;
		AudioSource* audioSource;
	};
	
	typedef std::vector<OneShot*> TOneShots;
	
	TOneShots m_OneShots;

	/**
	 * Update channel properties
	 * @param channel The channel to update
	 * @param oneshot Is this a oneshot?
	 * @return True if channel was update. False if the channel is invalid
	 **/
	inline bool UpdateParameters(AudioChannel* channel, OneShot* oneshot = NULL);

	/**
	 * Create a custom rolloff curve from old <3.0 parameters
	 **/
	void CreateOpenALRolloff(float rolloffFactor, float minVolume, float maxVolume);
	
	/**
	 * Setup effect and non-effect groups  
	 **/
	void SetupGroups();
	void TearDownGroups();
	void SetChannelGroup(AudioChannel* channel);

#if ENABLE_AUDIO_FMOD		
	/**
	 * Apply filters
	 **/
	void ApplyFilters();
#endif
	
private: 
	PPtr<AudioClip> m_AudioClip;
	ListNode<AudioSource> m_Node;		
	AudioChannel* m_Channel;	
	

	AudioManager::AudioScheduledSource m_ScheduledSource;	

#if ENABLE_AUDIO_FMOD		
	// channel group, filter/non-filter group and for oneshot
	FMOD::ChannelGroup* m_dryGroup; // No Effect unit
	FMOD::ChannelGroup* m_wetGroup; // Effect unit
#endif	
	/** 
	 * backward compatibility props
	 **/
	bool m_IgnoreListenerVolume;	
	
	bool    m_PlayOnAwake;		///<Play the sound when the scene loads.
	bool	m_HasScheduledStartDelay;
	bool	m_HasScheduledEndDelay;
	int		m_VelocityUpdateMode;		
	Vector3f m_LastUpdatePosition;
	
	// cached position
	unsigned m_samplePosition;
	// cache pause
	bool m_pause;
	
	void DoUpdate();
	void UpdateQueue();
	void SetupQueue();
	void AssignProps();
	float CalculateVolumeModifierForDistance(float distance);

#if ENABLE_AUDIO_FMOD	
	FMOD::DSP* m_PlayingDSP;
	typedef std::vector<FMOD::DSP*> TFilters;
	bool GetFilterComponents(TFilters &filters, bool create) const;
#endif
	
	friend class AudioManager;

#if ENABLE_PROFILER
	static int s_AudioSourceCount;
#endif

#if ENABLE_AUDIO_FMOD
private: // callbacks
	//static FMOD_RESULT F_CALLBACK channelCallback(
	//	FMOD_CHANNEL *channel, FMOD_CHANNEL_CALLBACKTYPE type, void *commanddata1, void *commanddata2);
	static float F_CALLBACK rolloffCallback( 
											 FMOD_CHANNEL *  channel,  
											 float  distance 
	); 
#endif //ENABLE_AUDIO_FMOD
	
public: // static helper functions
	static AudioSource* GetAudioSourceFromChannel(AudioChannel* channel);
};

#endif //ENABLE_AUDIO
#endif
