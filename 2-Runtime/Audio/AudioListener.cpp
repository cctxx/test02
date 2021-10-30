#include "UnityPrefix.h"
#include "AudioManager.h"
#include "AudioListener.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Camera/RenderManager.h"
#include "AudioSourceFilter.h"
#include "AudioEchoFilter.h"
#include "AudioChorusFilter.h"
#include "correct_fmod_includer.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Mono/MonoBehaviour.h"

#if ENABLE_AUDIO

AudioListener::AudioListener (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_Node(this)
,	m_VelocityUpdateMode (kVelocityUpdateModeAuto)
,   m_LastPosition(Vector3f(0,0,0))
{
}

AudioListener::~AudioListener ()
{
	GetAudioManager().RemoveAudioListener(this);
}

void AudioListener::Cleanup()
{
#if ENABLE_AUDIO_FMOD
	const GameObject* go = GetGameObjectPtr();
	if (!go)
		return;
	for (int i=0;i<go->GetComponentCount();i++)
	{
		AudioFilter* filter = dynamic_pptr_cast<AudioFilter*> (&go->GetComponentAtIndex(i));
		if (filter == NULL)
			continue;
		
		filter->Cleanup();
	}	
#endif
}

void AudioListener::RemoveFromManager () 
{
	GetAudioManager().RemoveAudioListener(this);
}

void AudioListener::AddToManager () 
{
	m_LastPosition = GetCurrentTransform().GetPosition();
	GetAudioManager().AddAudioListener(this);	
#if ENABLE_AUDIO_FMOD
	ApplyFilters();
#endif
}

void AudioListener::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
}

void AudioListener::SetAlternativeTransform(Transform* t)
{
	m_AltTransform = t;
} 	

const Transform& AudioListener::GetCurrentTransform() const
{
#if UNITY_EDITOR
	if (!IsWorldPlaying())
	{
		Transform* altTransform = m_AltTransform;
		if (altTransform)
			return *altTransform;
	}
#endif
	return GetComponent(Transform);
}

void AudioListener::DoUpdate ()
{	
	const Transform& transform = GetCurrentTransform();
	const Vector3f pos = transform.GetPosition();
	
	Vector3f vel = (pos - m_LastPosition) * GetInvDeltaTime ();
	GetAudioManager().UpdateListener(
		pos, 
		vel,
		NormalizeSafe(transform.TransformDirection( Vector3f (0.0f, 1.0f, 0.0f) )),
		NormalizeSafe(transform.TransformDirection( Vector3f (0.0f, 0.0f, 1.0f) ))
	);	
	m_LastPosition = pos;
}

void AudioListener::Update()
{
	if(m_VelocityUpdateMode == kVelocityUpdateModeAuto)
		m_VelocityUpdateMode = GetAudioManager().GetAutomaticUpdateMode( GetGameObjectPtr() );

	if(m_VelocityUpdateMode==kVelocityUpdateModeDynamic)
		DoUpdate();
}

void AudioListener::FixedUpdate()
{
	if(m_VelocityUpdateMode == kVelocityUpdateModeAuto)
		m_VelocityUpdateMode = GetAudioManager().GetAutomaticUpdateMode( GetGameObjectPtr());

	if(m_VelocityUpdateMode==kVelocityUpdateModeFixed)
		DoUpdate();
}

// Apply filters
#if ENABLE_AUDIO_FMOD
void AudioListener::ApplyFilters()
{
	const GameObject& go = GetGameObject();
	for (int i=0;i<go.GetComponentCount();i++)
	{
		FMOD::DSP* dsp = NULL;
		
		AudioFilter* filter = NULL;
		filter = dynamic_pptr_cast<AudioFilter*> (&go.GetComponentAtIndex(i));
		if ( filter && GetBuildSettings().hasAdvancedVersion ) 
			dsp = filter->GetDSP();
		
		#if ENABLE_SCRIPTING
		if (!dsp)
		{	
			MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&go.GetComponentAtIndex(i));
			if ( behaviour ) dsp = behaviour->GetOrCreateDSP();
		}
		#endif
		
		if (dsp == NULL)
			continue;
		
		FMOD_RESULT result;
		result = dsp->remove();
		FMOD_ASSERT(result);
		result = GetAudioManager().GetChannelGroup_FX_IgnoreVolume()->addDSP(dsp, 0);
		FMOD_ASSERT(result);
	}
}
#endif

void AudioListener::OnAddComponent()
{
#if ENABLE_AUDIO_FMOD
	ApplyFilters();
#endif
}

void AudioListener::InitializeClass ()
{
	REGISTER_MESSAGE_VOID (AudioListener, kDidAddComponent, OnAddComponent);
}

void AudioListener::CleanupClass()
{
}

template<class TransferFunc>
void AudioListener::Transfer (TransferFunc& transfer) {
	Super::Transfer (transfer);
}

IMPLEMENT_CLASS_HAS_INIT (AudioListener)
IMPLEMENT_OBJECT_SERIALIZE (AudioListener)

#endif
