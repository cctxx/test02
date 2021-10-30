#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#if ENABLE_AUDIO
#include "AudioSource.h"
#include "AudioManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Profiler/Profiler.h"
#include "AudioClip.h"
#include "Runtime/Video/MoviePlayback.h"
#include "Runtime/Dynamics/RigidBody.h"
#include "Runtime/Animation/AnimationCurveUtility.h"
#include "AudioListener.h"
#include "AudioSourceFilter.h"
#include "AudioLowPassFilter.h"
#include "AudioEchoFilter.h"
#include "AudioChorusFilter.h"
#include "AudioCustomFilter.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/GameCode/CallDelayed.h"
#include "Runtime/Audio/correct_fmod_includer.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Animation/AnimationCurveUtility.h"
#include "Runtime/Interfaces/IPhysics.h"

#if SUPPORT_REPRODUCE_LOG
#include "Runtime/Misc/ReproductionLog.h"
#include <fstream>
#endif

#define Unity_HiWord(x) ((UInt32)((UInt64)(x) >> 32))
#define Unity_LoWord(x) ((UInt32)(x))

inline void ReplaceOrAddSingleCurveValue(float value, AnimationCurve& curve) 
{
	AnimationCurve::Keyframe key(0.0f,value);
	curve.Assign(&key, &key + 1);
}

// ------------------------------------------------------------------------


#if ENABLE_PROFILER
int AudioSource::s_AudioSourceCount = 0;
#endif


AudioSource::AudioSource (MemLabelId label, ObjectCreationMode mode) : 
	Super(label, mode)
	,m_PlayOnAwake(true)
	,m_VelocityUpdateMode(kVelocityUpdateModeAuto)
	,m_LastUpdatePosition(Vector3f(0,0,0))
	,m_Channel(NULL)
	,m_Node (this)
	,m_IgnoreListenerVolume(false)
	,m_samplePosition(0)
	,m_pause(true)
	,m_ScheduledSource(this)
	,m_HasScheduledStartDelay(false)
	,m_HasScheduledEndDelay(false)
#if ENABLE_AUDIO_FMOD
	,m_dryGroup(NULL)
	,m_wetGroup(NULL)
	,m_PlayingDSP(NULL)
#endif
{
#if ENABLE_PROFILER
	s_AudioSourceCount++;
#endif

	m_AudioParameters.pitch = 1.0;
	m_AudioParameters.volume = 1.0f;
	m_AudioParameters.priority = 128;
	m_AudioParameters.loop = false;
	m_AudioParameters.pan = 0.0f;
	m_AudioParameters.dopplerLevel = 1.0f;	
	m_AudioParameters.minDistance = 1.0f;
	m_AudioParameters.maxDistance = 500.0f;
	m_AudioParameters.insideConeAngle = 360.0f;
	m_AudioParameters.outsideConeAngle = 360.0f;
	m_AudioParameters.outsideConeVolume = 1.0f;
	m_AudioParameters.mute = false;
	m_AudioParameters.rolloffMode = kRolloffLogarithmic;
#if UNITY_WII
	m_AudioParameters.starving = false;
#endif
	m_AudioParameters.bypassEffects = false;
	m_AudioParameters.bypassListenerEffects = false;
	m_AudioParameters.bypassReverbZones = false;
	m_AudioParameters.ignoreListenerPause = false;
	// curves
	ReplaceOrAddSingleCurveValue(1.0f, m_AudioParameters.panLevelCustomCurve);
	ReplaceOrAddSingleCurveValue(0.0f, m_AudioParameters.spreadCustomCurve);	

	if(GetAudioManagerPtr() != NULL)
		AssignProps();
}

void AudioSource::Reset()
{
	Super::Reset();
	
	m_AudioParameters.pitch = 1.0;
	m_AudioParameters.volume = 1.0f;
	m_AudioParameters.priority = 128;
	m_AudioParameters.loop = false;
	m_AudioParameters.pan = 0.0f;
	m_AudioParameters.dopplerLevel = 1.0f;	
	m_AudioParameters.minDistance = 1.0f;
	m_AudioParameters.maxDistance = 500.0f;
	m_AudioParameters.insideConeAngle = 360.0f;
	m_AudioParameters.outsideConeAngle = 360.0f;
	m_AudioParameters.outsideConeVolume = 1.0f;
	m_AudioParameters.mute = false;
	m_AudioParameters.rolloffMode = kRolloffLogarithmic;
	m_AudioParameters.bypassEffects = false;
	m_AudioParameters.bypassListenerEffects = false;
	m_AudioParameters.bypassReverbZones = false;
	m_AudioParameters.ignoreListenerPause = false;

	m_PlayOnAwake = true;
	
	// Curves initial values will be handled in CheckConsistency
	m_AudioParameters.rolloffCustomCurve.ResizeUninitialized (0);
	m_AudioParameters.panLevelCustomCurve.ResizeUninitialized (0);
	m_AudioParameters.spreadCustomCurve.ResizeUninitialized (0);
	CheckConsistency ();
}

AudioSource::~AudioSource ()
{
#if ENABLE_PROFILER
	s_AudioSourceCount--;
#endif

#if ENABLE_AUDIO_FMOD
	TearDownGroups(); 

	Assert (m_dryGroup == NULL);
	Assert (m_wetGroup == NULL);
#endif
}

void AudioSource::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	if (IsActive())
		SetupGroups();
	
	AssignProps();	
	
	if (IsActive() && awakeMode & (kDidLoadFromDisk | kInstantiateOrCreateFromCodeAwakeFromLoad | kActivateAwakeFromLoad))
		m_LastUpdatePosition = GetComponent(Transform).GetPosition();

	// This calls AddToManager which performs PlayOnAwake
	// Thus we call it at the end after properties have been setup.
	Super::AwakeFromLoad (awakeMode);
}

void AudioSource::CheckConsistency ()
{
	const float epsilon = 0.000001f;
	
	m_AudioParameters.volume = clamp01(m_AudioParameters.volume);
	m_AudioParameters.priority = clamp(m_AudioParameters.priority,0,255);
	m_AudioParameters.pitch = clamp(m_AudioParameters.pitch,-3.0f,3.0f);
	m_AudioParameters.dopplerLevel = clamp(m_AudioParameters.dopplerLevel,0.0f,5.0f);
	m_AudioParameters.minDistance = m_AudioParameters.minDistance < 0.0f?0.0f:m_AudioParameters.minDistance;
	m_AudioParameters.maxDistance = m_AudioParameters.maxDistance < m_AudioParameters.minDistance + epsilon ? m_AudioParameters.minDistance + epsilon : m_AudioParameters.maxDistance;
	
	if (m_AudioParameters.rolloffCustomCurve.GetKeyCount() < 1)
	{
		m_AudioParameters.rolloffCustomCurve.AddKey(AnimationCurve::Keyframe(0.0f,1.0f));
		m_AudioParameters.rolloffCustomCurve.AddKey(AnimationCurve::Keyframe(1.0f,0.0f));
	}
	if (m_AudioParameters.rolloffCustomCurve.GetKeyCount() == 1)
		m_AudioParameters.rolloffCustomCurve.GetKey(0).value = clamp(m_AudioParameters.rolloffCustomCurve.GetKey(0).value, 0.0f, 1.0f);
	
	if (m_AudioParameters.panLevelCustomCurve.GetKeyCount() < 1)
		ReplaceOrAddSingleCurveValue(1.0f, m_AudioParameters.panLevelCustomCurve);
	if (m_AudioParameters.panLevelCustomCurve.GetKeyCount() == 1)
		m_AudioParameters.panLevelCustomCurve.GetKey(0).value = clamp(m_AudioParameters.panLevelCustomCurve.GetKey(0).value, 0.0f, 1.0f);
	
	if (m_AudioParameters.spreadCustomCurve.GetKeyCount() < 1)		
		ReplaceOrAddSingleCurveValue(0.0f, m_AudioParameters.spreadCustomCurve);
	if (m_AudioParameters.spreadCustomCurve.GetKeyCount() == 1)
		m_AudioParameters.spreadCustomCurve.GetKey(0).value = clamp(m_AudioParameters.spreadCustomCurve.GetKey(0).value, 0.0f, 1.0f);
}


void AudioSource::AssignProps()
{
#if ENABLE_AUDIO_FMOD
	SetOutsideConeAngle(m_AudioParameters.outsideConeAngle);
	SetInsideConeAngle(m_AudioParameters.insideConeAngle);
	SetOutsideConeVolume(m_AudioParameters.outsideConeVolume);
	SetupGroups();
	SetDopplerLevel(m_AudioParameters.dopplerLevel);
	SetPitch(m_AudioParameters.pitch);
#endif
	
	SetPriority(m_AudioParameters.priority);
	SetMinDistance(m_AudioParameters.minDistance);
	SetMaxDistance(m_AudioParameters.maxDistance);
	SetPan(m_AudioParameters.pan);
	
	SetVolume(m_AudioParameters.volume);
	SetLoop(m_AudioParameters.loop);
	SetMute(m_AudioParameters.mute);	
	
	if (m_AudioClip && m_Channel)
		m_Channel->setMode(m_AudioClip->Get3D()?FMOD_3D:FMOD_2D);

	SetRolloffMode(m_AudioParameters.rolloffMode);
}

void AudioSource::CorrectScheduledTimeAfterUnpause(UInt64 delay)
{
#if ENABLE_AUDIO_FMOD
	if(m_Channel != NULL)
	{
		unsigned hiclock, loclock;
		if(m_HasScheduledStartDelay)
		{
			m_Channel->getDelay(FMOD_DELAYTYPE_DSPCLOCK_START, &hiclock, &loclock);
			FMOD_64BIT_ADD(hiclock, loclock, Unity_HiWord(delay), Unity_LoWord(delay));
			m_Channel->setDelay(FMOD_DELAYTYPE_DSPCLOCK_START, hiclock, loclock);
		}
		if(m_HasScheduledEndDelay)
		{
			m_Channel->getDelay(FMOD_DELAYTYPE_DSPCLOCK_END, &hiclock, &loclock);
			FMOD_64BIT_ADD(hiclock, loclock, Unity_HiWord(delay), Unity_LoWord(delay));
			m_Channel->setDelay(FMOD_DELAYTYPE_DSPCLOCK_END, hiclock, loclock);
		}
	}
#endif
}


void AudioSource::SetAudioClip(AudioClip *clip) 
{ 
	if(m_AudioClip == PPtr<AudioClip> (clip))
		return;
	Stop(true);
	m_AudioClip = clip;
	SetDirty ();
}

void AudioSource::Deactivate (DeactivateOperation operation)
{
	// We don't want audio to stop playing when in loading another level,
	// so we just ignore the ignore deactivate completely
	if (operation != kDeprecatedDeactivateToggleForLevelLoad)
	{
		Super::Deactivate (operation);
		TearDownGroups ();
	}	
}

/**
 * Setup effect and non-effect groups  
 **/
void AudioSource::SetupGroups()
{		
#if ENABLE_AUDIO_FMOD
	Assert (GetAudioManagerPtr());

	FMOD_RESULT result;
	
	if (!m_dryGroup)
	{
		result = GetAudioManager().GetFMODSystem()->createChannelGroup("Dry Group", &m_dryGroup);
		FMOD_ASSERT(result);
		
	}
	if (!m_wetGroup)
	{		
		result = GetAudioManager().GetFMODSystem()->createChannelGroup("Wet Group", &m_wetGroup);
		FMOD_ASSERT(result);
	}

	Assert(m_wetGroup);
	Assert(m_dryGroup);

	FMOD::ChannelGroup* newParentGroup;
	if(m_AudioParameters.bypassListenerEffects)
		newParentGroup = (m_IgnoreListenerVolume) ? GetAudioManager().GetChannelGroup_NoFX_IgnoreVolume() : GetAudioManager().GetChannelGroup_NoFX_UseVolume();
	else
		newParentGroup = (m_IgnoreListenerVolume) ? GetAudioManager().GetChannelGroup_FX_IgnoreVolume() : GetAudioManager().GetChannelGroup_FX_UseVolume();

	if(m_AudioParameters.bypassEffects)
	{
		// Connect dry group directly to new parent group (thus bypassing wet group)
		FMOD_RESULT result;
		FMOD::ChannelGroup* parentGroup;
		result = m_dryGroup->getParentGroup(&parentGroup); FMOD_ASSERT(result);
		if(parentGroup != newParentGroup)
		{
			result = newParentGroup->addGroup(m_dryGroup); FMOD_ASSERT(result);
		}
		FMOD::DSP* dsphead = NULL;
		result = m_wetGroup->getDSPHead(&dsphead); FMOD_ASSERT(result);
		result = dsphead->disconnectAll(false, true); FMOD_ASSERT(result);
	}
	else
	{
		// Connect dry group to wet group (where DSP effects are attached) and add wet group to new parent group
		FMOD_RESULT result;
		FMOD::ChannelGroup* parentGroup;
		result = m_dryGroup->getParentGroup(&parentGroup); FMOD_ASSERT(result);
		if(parentGroup != m_wetGroup)
		{
			result = m_wetGroup->addGroup(m_dryGroup); FMOD_ASSERT(result);
		}
		// addGroup must be called in any case since getParentGroup will still return old parent group even though the DSP is no longer connected (and is not visible in the FMOD profiler)
		result = newParentGroup->addGroup(m_wetGroup); FMOD_ASSERT(result);
	}

	FMOD_REVERB_CHANNELPROPERTIES rev;
	memset(&rev, 0, sizeof(rev));
	if(m_AudioParameters.bypassReverbZones)
		rev.Room = -10000;
	if(m_Channel != NULL)
	{
		result = m_Channel->setReverbProperties(&rev);
		//FMOD_ASSERT(result);
	}
	for(TOneShots::iterator it = m_OneShots.begin(); it != m_OneShots.end(); ++it)
	{
		FMOD::Channel* channel = (*it)->channel;
		if(channel != NULL)
		{
			result = channel->setReverbProperties(&rev);
			//FMOD_ASSERT(result);
		}
	}

#endif //ENABLE_AUDIO_FMOD
}

/**
 * tear down effect and non-effect groups  
 **/
void AudioSource::TearDownGroups()
{		
#if ENABLE_AUDIO_FMOD
	FMOD_RESULT result;
	
	if (m_dryGroup)
	{
		result = m_dryGroup->release();
		FMOD_ASSERT(result);
		m_dryGroup = NULL;		
	}
	if (m_wetGroup)
	{		
		result = m_wetGroup->release();
		FMOD_ASSERT(result);
		m_wetGroup = NULL;		
	}	
#endif
}

void AudioSource::Play(double time)
{	
	if(GetAudioManager().IsAudioDisabled())
		return;

	if (! (GetEnabled() && IsActive()) )
	{
		WarningStringObject("Can not play a disabled audio source", this);	
		if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
			return;
	}

	SetupGroups();
	
	/**
	 * Play/unpause main clip, if any
	 **/
	AudioClip* clip = m_AudioClip;
	if (clip != NULL && !clip->ReadyToPlay())
		return;

	if (m_Channel)
	{
		FMOD_RESULT result;

#if ENABLE_AUDIO_FMOD
		result = m_Channel->setChannelGroup(m_dryGroup);
#endif

		bool paused;
		result = m_Channel->getPaused(&paused);
		if (result == FMOD_OK && paused)
		{
			AssignProps();
			paused = IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a3) && GetAudioManager().GetPause() && !m_AudioParameters.ignoreListenerPause; // overwrite to restart sound
			m_Channel->setPaused(paused);
			GetAudioManager ().AddAudioSource (this, paused); // to make sure source is put into active or paused sources lists
			return;
		}
		else
			Stop(!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a1));
	}
	
	if (!m_Channel)
	{		
		if (clip != NULL)
			m_Channel = clip->CreateChannel(this);
		else 
		{		
	#if ENABLE_AUDIO_FMOD			
			TFilters filters;
			if ( GetFilterComponents(filters, true) )	
			{
				AudioCustomFilter* customFilter = NULL;
				filters[0]->getUserData((void**)&customFilter);
				if (customFilter)
				{
					customFilter->SetPlayingSource(this);
					m_PlayingDSP = filters[0];
					m_PlayingDSP->remove();
					FMOD_RESULT result = GetAudioManager().GetFMODSystem()->playDSP(FMOD_CHANNEL_FREE, filters[0], true, &m_Channel);	
					FMOD_ASSERT( result );
					result = m_Channel->setMode ( FMOD_3D );
					FMOD_ASSERT ( result );
				}	
				else
					WarningString(Format("Only custom filters can be played. Please add a custom filter or an audioclip to the audiosource (%s).", GetGameObjectPtr()?GetGameObjectPtr()->GetName():""));
			}
	#endif
		}		
	}	

	if (m_Channel)
	{
		FMOD_RESULT result;

#if ENABLE_AUDIO_FMOD
		result = m_Channel->setChannelGroup(m_dryGroup);
		ApplyFilters();
#endif		
		m_Channel->setUserData(this);
		AssignProps();
		UpdateParameters(m_Channel);

#if ENABLE_MOVIES			
		// is this a movie playback - note movieplayback (this is *so* ugly)
		if ( clip&&clip->GetMovie() )
			clip->GetMovie()->SetAudioChannel(m_Channel);
#endif		
		// set cached position
		m_Channel->setPosition(m_samplePosition, FMOD_TIMEUNIT_PCM);

		// play in sync (this will add it to the manager as well)
		GetAudioManager().ScheduleSource(this, time);

		m_pause = false;
		bool paused = IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a3) && GetAudioManager().GetPause() && !m_AudioParameters.ignoreListenerPause;
		m_Channel->setPaused(paused);
		GetAudioManager ().AddAudioSource (this, paused); // to make sure source is put into active or paused sources lists
	}
}

void AudioSource::Pause()
{
	m_pause = true;
	if (m_Channel)
	{
		m_Channel->setPaused(true);
	}
}
void AudioSource::PauseOneShots ()
{
	// pause one shots
	TOneShots::iterator it = m_OneShots.begin();
	for (; it != m_OneShots.end();++it)
	{
		OneShot& os = **it;
		os.channel->setPaused(true);
	}	
}

void AudioSource::ResumeOneShots ()
{
	/**
	 * play paused oneshots
	 **/
	// play one shots
	TOneShots::iterator it = m_OneShots.begin();
	for (; it != m_OneShots.end();++it)
	{
		OneShot& os = **it;
		os.channel->setPaused(IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a3) && GetAudioManager().GetPause() && !m_AudioParameters.ignoreListenerPause);
	}
}

void AudioSource::Cleanup()
{
	Stop(true);
#if ENABLE_AUDIO_FMOD	
	// cleanup filters
	const GameObject* go = GetGameObjectPtr();
	
	if (go)
	{	
		for (int i=0;i<go->GetComponentCount();i++)
		{
			AudioFilter* filter = NULL;
			filter = dynamic_pptr_cast<AudioFilter*> (&go->GetComponentAtIndex(i));
			if ( filter ) 
				filter->Cleanup();
			else
			{	
				MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&go->GetComponentAtIndex(i));
				if ( behaviour )
				{ 
					AudioCustomFilter* filter = behaviour->GetAudioCustomFilter();
					if ( filter ) filter->Cleanup();
				}
			}
		}
	}
	
	TearDownGroups();
	
#endif
}



void AudioSource::Stop(bool stopOneShots)
{
	m_HasScheduledStartDelay = false;
	m_HasScheduledEndDelay = false;
	
	if (m_Channel)
		m_Channel->stop();

	m_Channel = NULL; 	
	
	if(stopOneShots)
	{
	// stop one shots (and delete them from the list)
	TOneShots::iterator it = m_OneShots.begin();
	for (; it != m_OneShots.end(); ++it)
	{
		OneShot* os = *it;
		os->channel->stop();	
		delete os;
	}
	m_OneShots.clear();
	}

#if ENABLE_AUDIO_FMOD
	if (m_PlayingDSP)
		m_PlayingDSP->remove();
	
	AudioCustomFilter* filter = NULL;
	m_PlayingDSP->getUserData((void**)&filter);
	if (filter)
		filter->SetPlayingSource(NULL);
	
	m_PlayingDSP = NULL;
#endif

	if(m_OneShots.empty())
	{
#if ENABLE_AUDIO_FMOD
		// remove filters
		TFilters filters;
		if (GetFilterComponents(filters, false))
		{	
			for (TFilters::const_iterator it=filters.begin();it!=filters.end();it++)
			{
				FMOD::DSP* dsp = *it;		
				FMOD_RESULT result;
				result = dsp->remove();
				FMOD_ASSERT(result);
			}	

			filters.clear();
		}
#endif

		GetAudioManager ().RemoveAudioSource (this);
	}
}


void AudioSource::Stop(AudioChannel* channel)
{
	if (m_Channel == channel)
	{
		m_HasScheduledStartDelay = false;
		m_HasScheduledEndDelay = false;

		m_Channel->stop();
		m_Channel = NULL; 
	
#if ENABLE_AUDIO_FMOD
		if (m_PlayingDSP)
			m_PlayingDSP->remove();
	
		AudioCustomFilter* filter = NULL;
		m_PlayingDSP->getUserData((void**)&filter);
		if (filter)
			filter->SetPlayingSource(NULL);
		m_PlayingDSP = NULL;
#endif
	}
	else
	{
		// stop one shots (and delete them from the list)
		TOneShots::iterator it = m_OneShots.begin();
		for (; it != m_OneShots.end(); ++it)
		{
			OneShot* os = *it;
			if(os->channel == channel)
			{
				m_OneShots.erase(it);
				os->channel->stop();	
				delete os;
				break;
			}
		}
	}
	
	if(m_Channel == NULL && m_OneShots.empty())
	{
#if ENABLE_AUDIO_FMOD
		// remove filters
		TFilters filters;
		if (GetFilterComponents(filters, false))
		{	
			for (TFilters::const_iterator it=filters.begin();it!=filters.end();it++)
			{
				FMOD::DSP* dsp = *it;		
				FMOD_RESULT result;
				result = dsp->remove();
				FMOD_ASSERT(result);
			}	
	
			filters.clear();
		}
#endif
		GetAudioManager ().RemoveAudioSource (this);
	}
}


bool AudioSource::IsPlayingScripting ()
{
	bool res = IsPlaying ();
#if SUPPORT_REPRODUCE_LOG
	if (GetReproduceOutStream())
		*GetReproduceOutStream() << "Audio_IsPlaying " << (int)res << std::endl;
	else if (GetReproduceInStream() && GetReproduceVersion() >= 4)
	{
		if (!CheckReproduceTag("Audio_IsPlaying", *GetReproduceInStream()))
		{
			ErrorString("Grabbing Audio.IsPlaying but there are Audio_IsPlaying calls recorded");
			return res;
		}
		
		int temp = 0;
		*GetReproduceInStream()	>> temp;
		res = temp;
	}
#endif
	return res;
}

bool AudioSource::IsPlaying () const
{
	if (m_Channel)
	{
		FMOD_RESULT result;

		bool isPlaying, isPaused;
		
		result = m_Channel->isPlaying(&isPlaying);
		result = m_Channel->getPaused(&isPaused);
		
		return (result == FMOD_OK) && ((!m_pause) || ((isPlaying) && (!isPaused)));

	}
	return false;
}

bool AudioSource::IsPaused () const
{
	bool isPaused = false;
	if (m_Channel)
		m_Channel->getPaused(&isPaused);	
	return isPaused;
}


/**
 * Update channel properties
 * @param channel The channel to update
 * @param oneshot Is this a oneshot?
 **/

inline bool AudioSource::UpdateParameters(AudioChannel* channel, OneShot* oneshot /* = NULL */)
{
	// is this a valid channel?
	bool isPlaying = false;

	FMOD_RESULT result = channel==NULL?FMOD_ERR_INVALID_HANDLE:channel->isPlaying(&isPlaying);	

	if (result == FMOD_ERR_INVALID_HANDLE)
		return false;

	// Position
	Vector3f p = GetComponent(Transform).GetPosition(); 
	
	// Velocity
	// get velocity from rigidbody if present
	Vector3f v;
#if ENABLE_PHYSICS
	Rigidbody* rb = QueryComponent(Rigidbody);
	if (rb)
		v = GetIPhysics()->GetRigidBodyVelocity(*rb);
	else
#endif
		v = (p - m_LastUpdatePosition) * GetInvDeltaTime ();	
	
	// Orientate cone 
	Vector3f orient = NormalizeSafe( QuaternionToEuler( GetComponent(Transform).GetRotation() ) );

	channel->set3DConeOrientation(reinterpret_cast<FMOD_VECTOR*> (&orient));


#if ENABLE_AUDIO_FMOD
	FMOD_VECTOR& pos = UNITYVEC2FMODVEC(p); 
	FMOD_VECTOR& vel = UNITYVEC2FMODVEC(v);
	// Update position and velocity
	channel->set3DAttributes(&pos,&vel);
#endif	
	// update distance animated props
	// distance to listener
	// @TODO refactor when we support more than one listener
	const AudioListener* listener = GetAudioManager().GetAudioListener();
	if (listener != NULL)
	{
		const Vector3f& position = listener->GetPosition();
		float distance = Magnitude(p - position); 

		// Pan Level	
		const AnimationCurve& curve = m_AudioParameters.panLevelCustomCurve;
		if (curve.GetKeyCount() != 1)
		{
			std::pair<float, float> curverange = curve.GetRange();
			float panLevel = curve.Evaluate(clamp(distance / m_AudioParameters.maxDistance, curverange.first, curverange.second));
			channel->set3DPanLevel(panLevel);
		}
		else
		{
			channel->set3DPanLevel(curve.GetKey(0).value);	
		}
		
#if UNITY_FLASH || UNITY_WEBGL
		if (GetAudioClip()->Get3D())
			channel->SetDistanceVolumeMultiplier(CalculateVolumeModifierForDistance(distance));
#endif
		
		// Spread
		if (!m_AudioClip.IsNull())
		{
		
			const AnimationCurve& curve = m_AudioParameters.spreadCustomCurve;
			if (curve.GetKeyCount() != 1)
			{
				std::pair<float, float> curverange = curve.GetRange();
				float spread = curve.Evaluate(clamp(distance / m_AudioParameters.maxDistance, curverange.first, curverange.second));
				channel->set3DSpread(spread * 360.0f);			
			}
			else
			{
				channel->set3DSpread(curve.GetKey(0).value * 360.0f);	
			}
		}

#if ENABLE_AUDIO_FMOD
		// Low pass filter
		AudioLowPassFilter* lowPassFilter = QueryComponent(AudioLowPassFilter);
		if (lowPassFilter != NULL)
		{
			const AnimationCurve& curve = lowPassFilter->GetCustomLowpassLevelCurve();
			// Only use the curve if there's more than 1 key
			if (curve.GetKeyCount() > 1)
			{
				std::pair<float, float> curverange = curve.GetRange();
				float lowpassLevel = curve.Evaluate(clamp(distance / m_AudioParameters.maxDistance, curverange.first, curverange.second));
				lowpassLevel = 22000.0f - (21990.0f*lowpassLevel);
				lowPassFilter->SetCutoffFrequency(lowpassLevel);
			}
		}
#endif
	}	

	// update last position
	m_LastUpdatePosition = p;	
	return true;
}

void AudioSource::DoUpdate()
{
	if (! (GetEnabled() && IsActive() ) )
		return;

#if UNITY_WII
	// Check if we're not starving for data, starving can occur during disk eject
	FMOD_OPENSTATE openState;
	bool starving;
	bool diskbusy;
	m_AudioClip->GetSound()->getOpenState(&openState, NULL, &starving, &diskbusy);
	if (starving != m_AudioParameters.starving)
	{
		m_AudioParameters.starving = starving;
		if (starving)
		{
			// Mute all channels if we're starving for data, don't use AudioSource::SetMute because it will overwrite m_AudioParameters.mute
			if (m_Channel) m_Channel->setMute(true);
			for (TOneShots::iterator it = m_OneShots.begin(); it != m_OneShots.end();++it)
				(*it)->channel->setMute(true);
		}
		else
		{
			// Restore muteness
			SetMute(m_AudioParameters.mute);
		}
	}
#endif

	/**
	* One shots
	**/
	TOneShots::iterator it = m_OneShots.begin();
	for (; it != m_OneShots.end();)
	{
		OneShot* os = *it;
		bool playing = true;
		os->channel->isPlaying(&playing);
		
		if (!playing)		
		{
			os->channel->stop();
			delete os;
			it = m_OneShots.erase(it);
		}
		else 
		{					
			if (!UpdateParameters(os->channel, os))
			{
				os->channel->stop();
				delete os;
				it = m_OneShots.erase(it);
			}
			else
				++it;
		}		
	}
	
	if (!m_Channel)
		return;	
	
	//// why is this necessary???
	// check if channel is still valid and/or paused	
	bool isPaused = false;

	FMOD_RESULT result = m_Channel->getPaused(&isPaused);
	
	if (result == FMOD_ERR_INVALID_HANDLE)
	{
		m_HasScheduledStartDelay = false;
		m_HasScheduledEndDelay = false;
		m_Channel = NULL;
		return;
	}

	
	// @FIX Should we update parameters even though its paused!?
	if (isPaused)
		return;
	
	/**
	 * Source
	 **/	
	UpdateParameters(m_Channel);	
}

void AudioSource::AddToManager () 
{
	if (m_PlayOnAwake && IsWorldPlaying() && !IsPlaying() && IsActive() && GetEnabled())
		Play();
}

void AudioSource::RemoveFromManager () 
{
	Stop(true);
}

PROFILER_INFORMATION(gAudioSourceUpdateProfile, "AudioSource.Update", kProfilerAudio)

void AudioSource::Update()
{
	PROFILER_AUTO (gAudioSourceUpdateProfile, NULL);
	
	if(m_VelocityUpdateMode == kVelocityUpdateModeAuto)
		m_VelocityUpdateMode = GetAudioManager().GetAutomaticUpdateMode( GetGameObjectPtr() );
		
	if(m_VelocityUpdateMode==kVelocityUpdateModeDynamic)
		DoUpdate();
}

void AudioSource::FixedUpdate()
{
	if(m_VelocityUpdateMode == kVelocityUpdateModeAuto)
		m_VelocityUpdateMode = GetAudioManager().GetAutomaticUpdateMode( GetGameObjectPtr());
	
	if(m_VelocityUpdateMode==kVelocityUpdateModeFixed)
		DoUpdate();
}


// PROPERTIES

#define SetFMODParam(setFunction, value)\
if(m_Channel)m_Channel->setFunction(value);\
for (TOneShots::iterator it = m_OneShots.begin(); it != m_OneShots.end();++it)\
	(*it)->channel->setFunction(value);

#define SetFMODParam2(setFunction, value1, value2)\
if(m_Channel)m_Channel->setFunction(value1, value2);\
for (TOneShots::iterator it = m_OneShots.begin(); it != m_OneShots.end();++it)\
	(*it)->channel->setFunction(value1, value2);

#define SetFMODParam3(setFunction, value1, value2, value3)\
if(m_Channel)m_Channel->setFunction(value1, value2, value2);\
for (TOneShots::iterator it = m_OneShots.begin(); it != m_OneShots.end();++it)\
	(*it)->channel->setFunction(value1, value2, value3);


/// Get/Set pitch of the sound 
float AudioSource::GetPitch() const
{
	return m_AudioParameters.pitch;
}
void AudioSource::SetPitch(float pitch)
{
	AudioClip* clip = m_AudioClip;
	if(!IsFinite(pitch))
	{
		ErrorString("Attempt to set pitch to infinite value in AudioSource::SetPitch ignored!");
		return;
	}
	if(IsNAN(pitch))
	{
		ErrorString("Attempt to set pitch to NaN value in AudioSource::SetPitch ignored!");
		return;
	}
#if ENABLE_AUDIO_FMOD
	if (clip&&clip->IsMovieAudio())
		pitch = clamp(pitch,0.0f,3.0f);
#endif
	m_AudioParameters.pitch = pitch;
	if (m_Channel&&clip)
	{
#if ENABLE_AUDIO_FMOD
		if(IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a1))
		{
			FMOD::Sound* sound = NULL;
			if(m_Channel->getCurrentSound(&sound) == FMOD_OK)
			{
				float frequency;
				if(sound->getDefaults(&frequency, NULL, NULL, NULL) == FMOD_OK)
				{
					frequency *= m_AudioParameters.pitch;
					Assert(IsFinite(frequency));
					Assert(!IsNAN(frequency));
					m_Channel->setFrequency(frequency);
				}
			}
		}
		else
#endif
		{
			float frequency = m_AudioParameters.pitch * clip->GetFrequency();
			Assert(IsFinite(frequency));
			Assert(!IsNAN(frequency));
			m_Channel->setFrequency(frequency);	
		}
	}
	for (TOneShots::iterator it = m_OneShots.begin(); it != m_OneShots.end();++it)
	{
#if ENABLE_AUDIO_FMOD
		if(IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a1))
		{
			FMOD::Channel* channel = (*it)->channel;
			FMOD::Sound* sound = NULL;
			if(channel->getCurrentSound(&sound) == FMOD_OK)
			{
				float frequency;
				if(sound->getDefaults(&frequency, NULL, NULL, NULL) == FMOD_OK)
					channel->setFrequency(m_AudioParameters.pitch * frequency);
			}
		}
		else
#endif
			(*it)->channel->setFrequency(m_AudioParameters.pitch * (*it)->clip->GetFrequency());
	}
}

void AudioSource::SetVolume (float gain)
{			
	m_AudioParameters.volume = clamp01(gain);
	
	if(m_Channel)m_Channel->setVolume(m_AudioParameters.volume);
	for (TOneShots::iterator it = m_OneShots.begin(); it != m_OneShots.end();++it)
		(*it)->channel->setVolume(m_AudioParameters.volume * (*it)->volumeScale);
}


// Get/Set volume of the sound
float AudioSource::GetVolume() const
{
	return m_AudioParameters.volume;
}

void AudioSource::SetPlayOnAwake(bool playOnAwake)
{
	m_PlayOnAwake = playOnAwake;
	SetDirty();
}

void AudioSource::SetIgnoreListenerPause(bool ignoreListenerPause)
{
	m_AudioParameters.ignoreListenerPause = ignoreListenerPause;
	SetDirty();
}

bool AudioSource::GetLoop () const
{
	return m_AudioParameters.loop;
}

void AudioSource::SetLoop (bool loop)
{

	if (m_Channel)
	{	
		m_Channel->setMode( loop?FMOD_LOOP_NORMAL:FMOD_LOOP_OFF );
	}
	// always turn looping off for oneshots
	for (TOneShots::iterator it = m_OneShots.begin(); it != m_OneShots.end();++it)
		(*it)->channel->setMode( FMOD_LOOP_OFF );

	m_AudioParameters.loop = loop;
}

// Sets how much the 3d engine has an effect on the channel.
float AudioSource::GetPanLevel() const
{
	return m_AudioParameters.panLevelCustomCurve.GetKey(0).value;
}
void AudioSource::SetPanLevel(float level)
{
	// As panlevel is a curve
	// setting level resets the curve to 1 key
	level = clamp01(level);
	ReplaceOrAddSingleCurveValue(level,	m_AudioParameters.panLevelCustomCurve);	
}

// Sets the doppler scale for this AudioSource
float AudioSource::GetDopplerLevel() const
{
#if ENABLE_AUDIO_FMOD
	return m_AudioParameters.dopplerLevel;
#else
	return 0;
#endif

}
void AudioSource::SetDopplerLevel(float level)
{
#if ENABLE_AUDIO_FMOD
	m_AudioParameters.dopplerLevel = clamp(level, 0.0F, 5.0F);
	SetFMODParam(set3DDopplerLevel, m_AudioParameters.dopplerLevel);
#endif
}

// Sets the spread angle a 3d stereo ogr multichannel cound in speaker space.
// 0 = all sound channels are located at the same speaker location and is 'mono'. 
// 360 = all subchannels are located at the opposite speaker location to the speaker location that it should be according to 3D position. Default = 0.  
// Sets how much the 3d engine has an effect on the channel.
float AudioSource::GetSpread() const
{
	return m_AudioParameters.spreadCustomCurve.GetKey(0).value * 360.0f;
}
void AudioSource::SetSpread(float spread)
{
	// As spread is a curve
	// setting level resets the curve to 1 key
	spread = clamp(spread,0.0f, 360.0f);
	ReplaceOrAddSingleCurveValue(spread / 360.0f, m_AudioParameters.spreadCustomCurve);	
}

// Sets the priority of the [[AudioSource]]
// Unity is virtualizing AudioSources, when there's more AudioSources playing than available hardware channels. 
// The AudioSources with lowest priority (and audibility) is virtualized first.
// Priority is an integer between 0 and 256. 0=highest priority, 256=lowest priority
int AudioSource::GetPriority() const
{
	return m_AudioParameters.priority;
}

void AudioSource::SetPriority(int priority)
{
	m_AudioParameters.priority = clamp(priority, 0, 255);
	SetFMODParam(setPriority, m_AudioParameters.priority);
}

// Un- / Mutes the AudioSource. Mute sets the volume=0, Un-Mute restore the original volume.
bool AudioSource::GetMute() const
{
	return m_AudioParameters.mute;
}
void AudioSource::SetMute(bool mute)
{
	m_AudioParameters.mute = mute;
#if UNITY_WII
	// If we're starving for data, auto mute channels
	mute |= m_AudioParameters.starving;
#endif
	if (m_Channel)
		m_Channel->setMute(mute);
	
	for (TOneShots::iterator it = m_OneShots.begin(); it != m_OneShots.end();++it)
		(*it)->channel->setMute(mute);
}

// Within the Min distance the AudioSource will cease to grow louder in volume. 
// Outside the min distance the volume starts to attenuate.
float AudioSource::GetMinDistance() const
{
	return m_AudioParameters.minDistance;
}
void AudioSource::SetMinDistance(float minDistance)
{
	m_AudioParameters.minDistance = max(minDistance, 0.0F);
	SetFMODParam2(set3DMinMaxDistance, m_AudioParameters.minDistance, m_AudioParameters.maxDistance);
}

// (Logarithmic rolloff) MaxDistance is the distance a sound stops attenuating at.
// (Linear rolloff) MaxDistance is the distance where the sound is completely inaudible.
float AudioSource::GetMaxDistance() const
{
	return m_AudioParameters.maxDistance;
}
void AudioSource::SetMaxDistance(float maxDistance)
{
	m_AudioParameters.maxDistance = max(maxDistance, m_AudioParameters.minDistance);
	SetFMODParam2(set3DMinMaxDistance, m_AudioParameters.minDistance, m_AudioParameters.maxDistance);
}

#if ENABLE_AUDIO_FMOD
// Inside cone angle, in degrees. This is the angle within which the sound is at its normal volume. 
// Must not be greater than outsideconeangle. Default = 360.
float AudioSource::GetInsideConeAngle() const
{
	return m_AudioParameters.insideConeAngle;
}
void AudioSource::SetInsideConeAngle(float angle)
{
	m_AudioParameters.insideConeAngle = angle;
	SetFMODParam3(set3DConeSettings, m_AudioParameters.insideConeAngle, m_AudioParameters.outsideConeAngle, m_AudioParameters.outsideConeVolume); 
}

/// Outside cone angle, in degrees. This is the angle outside of which the sound is at its outside volume. 
/// Must not be less than insideconeangle. Default = 360. 
float AudioSource::GetOutsideConeAngle() const
{
	return m_AudioParameters.outsideConeAngle;
}
void AudioSource::SetOutsideConeAngle(float angle)
{
	m_AudioParameters.outsideConeAngle = angle;
	
	SetFMODParam3(set3DConeSettings, m_AudioParameters.insideConeAngle, m_AudioParameters.outsideConeAngle, m_AudioParameters.outsideConeVolume); 
}

/// Cone outside volume, from 0 to 1.0. Default = 1.0.  
float AudioSource::GetOutsideConeVolume() const
{
	return m_AudioParameters.outsideConeVolume;
}
void AudioSource::SetOutsideConeVolume(float volume)
{
	m_AudioParameters.outsideConeVolume = clamp01(volume);
	
	SetFMODParam3(set3DConeSettings, m_AudioParameters.insideConeAngle, m_AudioParameters.outsideConeAngle, m_AudioParameters.outsideConeVolume); 
}
#endif
/// Set/Get rolloff mode
RolloffMode AudioSource::GetRolloffMode() const
{
	return m_AudioParameters.rolloffMode;
}

void AudioSource::SetRolloffMode(RolloffMode mode)
{
	m_AudioParameters.rolloffMode = mode;
}

/// Set/Get Custom rolloff curve
AnimationCurve& AudioSource::GetCustomRolloffCurve()
{
	return m_AudioParameters.rolloffCustomCurve;
}

const AnimationCurve& AudioSource::GetCustomRolloffCurve() const
{
	return m_AudioParameters.rolloffCustomCurve;
}

void AudioSource::SetCustomRolloffCurve(const AnimationCurve& curve)
{
	m_AudioParameters.rolloffCustomCurve = curve;
}

/// Set/Get Pan Level curve
AnimationCurve& AudioSource::GetCustomPanLevelCurve()
{
	return m_AudioParameters.panLevelCustomCurve;
}

const AnimationCurve& AudioSource::GetCustomPanLevelCurve() const          
{
	return m_AudioParameters.panLevelCustomCurve;
}

void AudioSource::SetCustomPanLevelCurve(const AnimationCurve& curve)
{
	m_AudioParameters.panLevelCustomCurve = curve;
}

/// Set/Get Pan Level curve
AnimationCurve& AudioSource::GetCustomSpreadCurve()
{
	return m_AudioParameters.spreadCustomCurve;
}

const AnimationCurve& AudioSource::GetCustomSpreadCurve() const          
{
	return m_AudioParameters.spreadCustomCurve;
}

void AudioSource::SetCustomSpreadCurve(const AnimationCurve& curve)
{
	m_AudioParameters.spreadCustomCurve = curve;
}

/// Sets a channels pan position linearly. Only works for 2D clips.
/// -1.0 to 1.0. -1.0 is full left. 0.0 is center. 1.0 is full right.
/// Only sounds that are mono or stereo can be panned. Multichannel sounds (ie >2 channels) cannot be panned.
float AudioSource::GetPan() const
{
	return m_AudioParameters.pan;
}
void AudioSource::SetPan(float pan)
{
	m_AudioParameters.pan = clamp(pan, -1.0f, 1.0f);
	SetFMODParam(setPan, m_AudioParameters.pan);
}

/// Bypass/ignore any applied effects on AudioSource
bool AudioSource::GetBypassEffects() const
{
	return m_AudioParameters.bypassEffects;
}

void AudioSource::SetBypassEffects(bool bypassEffect)
{
	m_AudioParameters.bypassEffects = bypassEffect;
	SetupGroups();
}

/// Bypass/ignore any applied effects on AudioSource
bool AudioSource::GetBypassListenerEffects() const
{
	return m_AudioParameters.bypassListenerEffects;
}

void AudioSource::SetBypassListenerEffects(bool bypassListenerEffect)
{
	m_AudioParameters.bypassListenerEffects = bypassListenerEffect;
	SetupGroups();
}

bool AudioSource::GetBypassReverbZones() const
{
	return m_AudioParameters.bypassReverbZones;
}

/// Bypass effect of reverb zones on this AudioSource
void AudioSource::SetBypassReverbZones(bool bypassReverbZones)
{
	m_AudioParameters.bypassReverbZones = bypassReverbZones;
	SetupGroups();
}

float AudioSource::GetSecPosition() const
{

	float time = 0.0f;
	if (m_Channel)
	{
		unsigned position = 0;

		m_Channel->getPosition(&position, FMOD_TIMEUNIT_MS);
		time = (float)position / 1000.f;
	}
	else
		time = m_AudioClip.IsNull()?0.0f:(float)m_samplePosition / (float)m_AudioClip->GetFrequency();
	
	#if SUPPORT_REPRODUCE_LOG
	if (GetReproduceOutStream())
	{
		*GetReproduceOutStream() << "Audio_Position ";
		WriteFloat(*GetReproduceOutStream(), time);
		*GetReproduceOutStream() << std::endl;
	}
	else if (GetReproduceInStream() && GetReproduceVersion() >= 4)
	{
		if (!CheckReproduceTag("Audio_Position", *GetReproduceInStream()))
		{
			ErrorString("Grabbing Audio.IsPlaying but there are no realtime calls recorded");
			return time;
		}
		ReadFloat(*GetReproduceInStream(), time);
	}
	#endif
	return time;

}



void AudioSource::SetSecPosition(float secPosition)
{
	if (m_Channel)
		m_Channel->setPosition((unsigned int)(secPosition * 1000.f), FMOD_TIMEUNIT_MS);
	m_samplePosition = (unsigned int)(m_AudioClip.IsNull() ? 0.0f : (float)(secPosition * m_AudioClip->GetFrequency()));
}



UInt32 AudioSource::GetSamplePosition() const
{
	unsigned position = m_samplePosition;
	if (m_Channel)
		m_Channel->getPosition(&position, FMOD_TIMEUNIT_PCM);
	return position;

}



void AudioSource::SetSamplePosition(UInt32 position)
{
	if (m_Channel)
		m_Channel->setPosition(position, FMOD_TIMEUNIT_PCM);
	m_samplePosition = position;
}


void AudioSource::SetScheduledStartTime(double time)
{	
#if ENABLE_AUDIO_FMOD
	if (m_Channel)
	{
		m_HasScheduledStartDelay = true;
		int sampleRate;
		GetAudioManager().GetFMODSystem()->getSoftwareFormat(&sampleRate, NULL, NULL, NULL, NULL, NULL);
		UInt64 sample = (UInt64)(time * sampleRate) + GetAudioManager().GetAccumulatedPauseTicks();
		unsigned hiclock = sample >> 32;
		unsigned loclock = sample & 0xFFFFFFFF;
		m_Channel->setDelay(FMOD_DELAYTYPE_DSPCLOCK_START, hiclock, loclock); 
	}
#endif
}


void AudioSource::SetScheduledEndTime(double time)
{	
#if ENABLE_AUDIO_FMOD
	if (m_Channel)
	{
		m_HasScheduledEndDelay = true;
		int sampleRate;
		GetAudioManager().GetFMODSystem()->getSoftwareFormat(&sampleRate, NULL, NULL, NULL, NULL, NULL);
		UInt64 sample = (UInt64)(time * sampleRate) + GetAudioManager().GetAccumulatedPauseTicks();
		unsigned hiclock = sample >> 32;
		unsigned loclock = sample & 0xFFFFFFFF;
		m_Channel->setDelay(FMOD_DELAYTYPE_DSPCLOCK_END, hiclock, loclock); 
	}
#endif
}


#if ENABLE_AUDIO_FMOD

void AudioSource::GetOutputData(float* samples, int numSamples, int channelOffset)
{
	if (m_Channel)
	{
		m_Channel->getWaveData(samples, numSamples, channelOffset);		
	}
}

/// Gets the current spectrum data
void AudioSource::GetSpectrumData(float* samples, int numSamples, int channelOffset, FMOD_DSP_FFT_WINDOW windowType)
{
	if (m_Channel)
	{
		m_Channel->getSpectrum(samples, numSamples, channelOffset, windowType);
	}
}

// Apply filters
void AudioSource::ApplyFilters()
{
	if (!m_wetGroup)
		return;

	TFilters filters;
	GetFilterComponents(filters, true);
	
	for (TFilters::const_iterator it=filters.begin();it!=filters.end();it++)
	{
		FMOD::DSP* dsp = *it;
		
		if (dsp == m_PlayingDSP)
			continue;

		FMOD_RESULT result;
		result = dsp->remove();
		FMOD_ASSERT(result);
		result = m_wetGroup->addDSP(dsp, 0);
		FMOD_ASSERT(result);
	}		
}

#endif //ENABLE_AUDIO_FMOD

void AudioSource::PlayOneShot( AudioClip& clip, float volumeScale )
{
	if(GetAudioManager().IsAudioDisabled())
		return;

	if (! (GetEnabled() && IsActive()) )
	{
		WarningStringObject("Can not play a disabled audio source", this);	
		if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
			return; 
	}
	
	SetupGroups();

	OneShot* oneshot = new OneShot();
	oneshot->channel = clip.CreateChannel(this);
	Assert(oneshot->channel);
#if ENABLE_AUDIO_FMOD
	ApplyFilters();
#endif
	
	uintptr_t ptr = reinterpret_cast<uintptr_t>(oneshot) | 1;
	oneshot->channel->setUserData(reinterpret_cast<void*>(ptr));
	oneshot->clip = const_cast<AudioClip*>(&clip);
	oneshot->volumeScale = volumeScale;	
	oneshot->audioSource = this;

	if (oneshot->channel)
	{
#if ENABLE_AUDIO_FMOD	
		if(m_dryGroup != NULL)
			oneshot->channel->setChannelGroup(m_dryGroup);

		Vector3f pos = GetComponent(Transform).GetPosition() ; 
		FMOD_VECTOR fpos = UNITYVEC2FMODVEC(pos);
		oneshot->channel->set3DAttributes(
								   &fpos,
								   0 );			
#endif
		
		// start!		
		m_OneShots.push_back(oneshot);
		AssignProps(); // Apply all parameters
		bool paused = IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a3) && GetAudioManager().GetPause() && !m_AudioParameters.ignoreListenerPause;
		oneshot->channel->setPaused(paused);
		GetAudioManager ().AddAudioSource (this, paused); // to make sure source is put into active or paused sources lists
	}
	else
	{
		delete oneshot;
	}
}

void AudioSource::SetIgnoreListenerVolume(bool ignore)
{
	if (m_IgnoreListenerVolume == ignore)
		return;

	m_IgnoreListenerVolume = ignore;
	SetupGroups();
}

float inline CalcOALGain(float rolloffFactor, float volume, float minVolume, float maxVolume, float distance)
{
	float gain = 1.0f;				
	if (1.0f + rolloffFactor * (distance - 1.0f) >	0.0f)
		gain = 1.0f / ( 1.0f + rolloffFactor * (distance - 1.0f));
	
	gain *= volume;
	gain = std::min(gain, maxVolume);
	gain = std::max(gain, minVolume);
	return gain;
}

AudioSource* AudioSource::GetAudioSourceFromChannel(AudioChannel* channel)
{
	Assert(channel);
	
	void* userData = NULL;
	AudioSource* audioSource = NULL;
	channel->getUserData(&userData);
	
	if (!userData) return NULL;
	
	uintptr_t ptr = reinterpret_cast<uintptr_t> (userData);
	
	// is it a OneShot?
	if (ptr & 1)
	{
		ptr = ptr & (~1);
		OneShot* oneshot = reinterpret_cast<OneShot*>(ptr);
		Assert (oneshot->audioSource);
		audioSource = oneshot->audioSource;
	}
	else // AudioSource
		audioSource = (AudioSource*)userData;	
	
	return audioSource;
}



/**
 * Create a custom rolloff curve from old <3.0 parameters
 **/

void AudioSource::CreateOpenALRolloff(float rolloffFactor, float minVolume, float maxVolume)
{	
	
#if ENABLE_AUDIO_FMOD
	AnimationCurve& curve = m_AudioParameters.rolloffCustomCurve;
	curve.RemoveKeys(curve.begin(), curve.end());	
		
	// insert first key
	AnimationCurve::Keyframe key;
	key.time = 0.0f;
	key.value = CalcOALGain(rolloffFactor, m_AudioParameters.volume, minVolume, maxVolume, 0.0f);	
	curve.AddKey (key);		
	
	for (float distance=0.1f;distance<m_AudioParameters.maxDistance;distance*=2)
	{
		AnimationCurve::Keyframe key;
		key.time = distance;
		key.value = CalcOALGain(rolloffFactor, m_AudioParameters.volume, minVolume, maxVolume, distance);
		// add some sensible in/out slopes
		float s = distance/10.0f;	
		key.inSlope = (key.value - CalcOALGain(rolloffFactor, m_AudioParameters.volume, minVolume, maxVolume, distance-s)) / s;
		key.outSlope = (CalcOALGain(rolloffFactor, m_AudioParameters.volume, minVolume, maxVolume,distance+s) - key.value) / s;	
		curve.AddKey (key);		
	}
	
	key.time = m_AudioParameters.maxDistance;
	key.value = CalcOALGain(rolloffFactor, m_AudioParameters.volume, minVolume, maxVolume, m_AudioParameters.maxDistance);
	curve.AddKey (key);
#endif
}

#if ENABLE_AUDIO_FMOD

bool AudioSource::GetFilterComponents(TFilters& filters, bool create) const
{
	const GameObject* go = GetGameObjectPtr();
	
	if (!go)
		return false;
	
	for (int i=0;i<go->GetComponentCount();i++)
	{
		AudioFilter* filter = NULL;
		FMOD::DSP* dsp = NULL;
		filter = dynamic_pptr_cast<AudioFilter*> (&go->GetComponentAtIndex(i));
		// built-in filter are only available in PRO
		if ( filter && GetBuildSettings().hasAdvancedVersion ) 
			dsp = filter->GetDSP();
		
		#if ENABLE_SCRIPTING
		if (!dsp)
		{	
			MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&go->GetComponentAtIndex(i));
			if ( behaviour )
			{
				if (create)
					dsp = behaviour->GetOrCreateDSP();
				else 
					dsp = behaviour->GetDSP();				
			}
		}
		#endif
		
		if (dsp)
			filters.push_back( dsp );
	}
	
	return !filters.empty();
}

#endif

template<class TransferFunc>
void AudioSource::Transfer (TransferFunc& transfer) {
	Super::Transfer (transfer);
	
	// 3.5  3: Normalized curve values
	// 3.0	2: FMOD custom rolloff data
	// <3.0 1: OpenAL rolloff data
	transfer.SetVersion (3);
	
	if (transfer.IsOldVersion(2) || transfer.IsCurrentVersion ())
	{   
		transfer.Transfer (m_AudioClip, "m_audioClip");
		TRANSFER_SIMPLE (m_PlayOnAwake);
		transfer.Align();
	
		transfer.Transfer (m_AudioParameters.volume, "m_Volume");
		transfer.Transfer (m_AudioParameters.pitch, "m_Pitch");
	
		transfer.Transfer (m_AudioParameters.loop, "Loop");		// repeat sound 
		transfer.Transfer(m_AudioParameters.mute, "Mute");
		transfer.Align();	
		
		transfer.Transfer(m_AudioParameters.priority, "Priority");
		transfer.Transfer(m_AudioParameters.dopplerLevel, "DopplerLevel");
		
		transfer.Transfer(m_AudioParameters.minDistance, "MinDistance");
		transfer.Transfer(m_AudioParameters.maxDistance, "MaxDistance");
	
		transfer.Transfer(m_AudioParameters.pan, "Pan2D");	
		
		SInt32 mode = (SInt32)m_AudioParameters.rolloffMode;
		transfer.Transfer(mode, "rolloffMode");
		m_AudioParameters.rolloffMode = (RolloffMode)mode;
		
		transfer.Transfer(m_AudioParameters.bypassEffects, "BypassEffects");
		transfer.Transfer(m_AudioParameters.bypassListenerEffects, "BypassListenerEffects");
		transfer.Transfer(m_AudioParameters.bypassReverbZones, "BypassReverbZones");

		transfer.Align();
		
		// @TODO strip these from the player build if they're empty?
		transfer.Transfer(m_AudioParameters.rolloffCustomCurve,"rolloffCustomCurve");
		transfer.Transfer(m_AudioParameters.panLevelCustomCurve, "panLevelCustomCurve");		
		transfer.Transfer(m_AudioParameters.spreadCustomCurve, "spreadCustomCurve");	
		
		if (transfer.IsOldVersion(2))
		{
			// Normalize curves so time goes from 0 to 1
			ScaleCurveTime (m_AudioParameters.rolloffCustomCurve, 1.0f / m_AudioParameters.maxDistance);
			ScaleCurveTime (m_AudioParameters.panLevelCustomCurve, 1.0f / m_AudioParameters.maxDistance);
			ScaleCurveTime (m_AudioParameters.spreadCustomCurve, 1.0f / m_AudioParameters.maxDistance);
		}
	}
	else
	{
		transfer.Transfer (m_AudioClip, "m_audioClip");
		TRANSFER_SIMPLE (m_PlayOnAwake);
		transfer.Align();
		
		transfer.Transfer (m_AudioParameters.volume, "m_Volume");
		transfer.Transfer (m_AudioParameters.pitch, "m_Pitch");
		
		float minVolume, maxVolume;
		transfer.Transfer (minVolume,"m_MinVolume");
		transfer.Transfer (maxVolume,"m_MaxVolume");
		float rolloffFactor;
		transfer.Transfer (rolloffFactor, "m_RolloffFactor");	
		
		transfer.Transfer (m_AudioParameters.loop, "Loop");		// repeat sound 
#if ENABLE_AUDIO_FMOD
		CreateOpenALRolloff(rolloffFactor, minVolume, maxVolume);
#endif		
		m_AudioParameters.rolloffMode = kRolloffCustom;
	}	
}


void AudioSource::OnAddComponent()
{
#if ENABLE_AUDIO_FMOD
	ApplyFilters();
#endif
}


void AudioSource::InitializeClass ()
{
	REGISTER_MESSAGE_VOID (AudioSource, kDidAddComponent, OnAddComponent);
}

void AudioSource::CleanupClass()
{
}

IMPLEMENT_CLASS_HAS_INIT (AudioSource)
IMPLEMENT_OBJECT_SERIALIZE (AudioSource)

#endif //ENABLE_AUDIO
