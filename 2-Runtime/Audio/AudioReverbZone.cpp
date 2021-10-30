#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#if ENABLE_AUDIO_FMOD
#include "AudioReverbZone.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "AudioManager.h"
#include "Runtime/Graphics/Transform.h"
#include "AudioBehaviour.h"
#include "Runtime/Audio/correct_fmod_includer.h"
#include "Runtime/Utilities/Utility.h"
#include "AudioTypes.h" 

extern FMOD_REVERB_PROPERTIES ReverbPresets [];

AudioReverbZone::AudioReverbZone (MemLabelId label, ObjectCreationMode mode) :
	Super(label, mode),
	m_MinDistance(10.0f),
	m_MaxDistance(15.0f),
	m_FMODReverb(NULL),
	m_ReverbPreset(1),
	m_Node(this)
{
	ChangeProperties();
}

AudioReverbZone::~AudioReverbZone ()
{
	Cleanup();	
	GetAudioManager().RemoveAudioReverbZone(this);
}

void AudioReverbZone::Cleanup()
{
	if (m_FMODReverb)
	{
		m_FMODReverb->release();
		m_FMODReverb = NULL;
	}
}

void AudioReverbZone::VerifyValues()
{
	if (m_MinDistance < 0)
		m_MinDistance = 0;
	if (m_MaxDistance < m_MinDistance)
		m_MaxDistance = m_MinDistance;	
	m_Room = clamp(m_Room, -10000, 0);
	m_RoomHF = clamp(m_RoomHF,  -10000, 0);
	m_RoomLF = clamp(m_RoomLF,  -10000, 0);
	m_DecayTime = clamp(m_DecayTime, 0.1f, 20.0f);
	m_DecayHFRatio = clamp(m_DecayHFRatio, 0.1f, 2.0f);
	m_Reflections = clamp(m_Reflections, -10000, 1000);
	m_ReflectionsDelay = clamp(m_ReflectionsDelay,0.0f, 0.3f);
	m_Reverb = clamp(m_Reverb, -10000, 2000);
	m_ReverbDelay = clamp(m_ReverbDelay, 0.0f, 0.1f);
	m_HFReference = clamp(m_HFReference, 1000.0f, 20000.0f);
	m_LFReference = clamp(m_LFReference, 20.0f, 1000.0f);
	m_RoomRolloffFactor = clamp(m_RoomRolloffFactor, 0.0f, 10.0f);
	m_Diffusion = clamp(m_Diffusion, 0.0f, 100.0f);
	m_Density = clamp(m_Density, 0.0f, 100.0f);
}


void AudioReverbZone::Reset()
{
	Super::Reset();
	
	m_MinDistance = 10.0f;
	m_MaxDistance = 15.0f;
	m_ReverbPreset = 1;
	
	ChangeProperties();
}

void AudioReverbZone::CheckConsistency ()
{
	Super::CheckConsistency();
	VerifyValues();
}


void AudioReverbZone::RemoveFromManager()
{
	if (m_FMODReverb)
		m_FMODReverb->setActive(false);
	GetAudioManager().RemoveAudioReverbZone(this);
}


void AudioReverbZone::AddToManager()
{
	Init();
	GetAudioManager().AddAudioReverbZone(this);
}

void AudioReverbZone::Init()
{
	if (m_FMODReverb == NULL)
	{
		FMOD_RESULT result = GetAudioManager().GetFMODSystem()->createReverb( &m_FMODReverb );
		if (FMOD_OK != result)
			ErrorString(FMOD_ErrorString(result)); 
	}
	
	m_FMODReverb->setActive(true);
	
	SetFMODValues();

	// needed, otherwise sounds playing in the very first frame may not be connected to the master reverb dsp unit
	GetAudioManager().GetFMODSystem()->update();
}

void AudioReverbZone::SetFMODValues ()
{
	if (m_FMODReverb)
	{
		Vector3f p = GetComponent(Transform).GetPosition(); 
		FMOD_RESULT result;
		result = m_FMODReverb->set3DAttributes(reinterpret_cast<FMOD_VECTOR*> (&p), m_MinDistance, m_MaxDistance);
		Assert(FMOD_OK == result);
		FMOD_REVERB_PROPERTIES prop = GetReverbProperty(m_ReverbPreset);
		result = m_FMODReverb->setProperties(&prop);
		Assert(FMOD_OK == result);
	}
}

void AudioReverbZone::ChangeProperties()
{
	FMOD_REVERB_PROPERTIES prop = GetReverbProperty(m_ReverbPreset);
	// read back values
	m_Room = prop.Room;
	m_RoomHF = prop.RoomHF;
	m_DecayTime = prop.DecayTime; 
	m_DecayHFRatio = prop.DecayHFRatio;
	m_Reflections = prop.Reflections;
	m_ReflectionsDelay = prop.ReflectionsDelay;
	m_Reverb = prop.Reverb;
	m_ReverbDelay = prop.ReverbDelay;
	m_HFReference = prop.HFReference;
	m_RoomRolloffFactor = 0.0F;
	m_Diffusion = prop.Diffusion;
	m_Density = prop.Density;
	m_RoomLF = prop.RoomLF;
	m_LFReference = prop.LFReference;
	
}

void AudioReverbZone::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
	ChangeProperties();
	SetFMODValues();
	SetDirty(); // ??
}

float AudioReverbZone::GetMinDistance() const
{
	return m_MinDistance;
}

float AudioReverbZone::GetMaxDistance() const
{
	return m_MaxDistance;
}

void AudioReverbZone::Update()
{
	Assert (m_FMODReverb);
	
	// Position
	Vector3f p = GetComponent(Transform).GetPosition(); 
	FMOD_VECTOR& pos = UNITYVEC2FMODVEC(p); 		
	m_FMODReverb->set3DAttributes(&pos, m_MinDistance, m_MaxDistance);
}

void AudioReverbZone::SetMinDistance(float minDistance) 
{
	m_MinDistance = minDistance;
	VerifyValues();
	SetFMODValues();
	SetDirty();
}

void AudioReverbZone::SetMaxDistance(float maxDistance) 
{
	m_MaxDistance = maxDistance;
	VerifyValues();
	SetFMODValues();
	SetDirty();
}

void AudioReverbZone::SetReverbPreset(int reverbPreset) 
{
	if (m_ReverbPreset != reverbPreset)
	{
		m_ReverbPreset = reverbPreset;
		ChangeProperties();
		SetDirty();
	}
}

FMOD_REVERB_PROPERTIES AudioReverbZone::GetReverbProperty(int preset)
{
	if (preset < 27)
		return ReverbPresets [preset];
	else
	{
		FMOD_REVERB_PROPERTIES prop = FMOD_PRESET_OFF;
		prop.Room = m_Room; // room effect level (at mid frequencies)
		prop.RoomHF = m_RoomHF; // relative room effect level at high frequencies	
		prop.DecayTime = m_DecayTime; // reverberation decay time at mid frequencies
		prop.DecayHFRatio =  m_DecayHFRatio; //  high-frequency to mid-frequency decay time ratio	
		prop.Reflections = m_Reflections; // early reflections level relative to room effect
		prop.ReflectionsDelay = m_ReflectionsDelay; //  initial reflection delay time
		prop.Reverb = m_Reverb; //  late reverberation level relative to room effect
		prop.ReverbDelay = m_ReverbDelay; //  late reverberation delay time relative to initial reflection 
		prop.HFReference = m_HFReference; // reference high frequency (hz)
		//prop.RoomRolloffFactor = m_RoomRolloffFactor; //  like rolloffscale in global settings, but for reverb room size effect
		prop.Diffusion = m_Diffusion; //  Value that controls the echo density in the late reverberation decay
		prop.Density =  m_Density; // Value that controls the modal density in the late reverberation decay
		prop.RoomLF = m_RoomLF; 
		prop.LFReference = m_LFReference; // reference low frequency (hz)
		return prop;
	}		
}


int AudioReverbZone::GetReverbPreset() const
{
	return m_ReverbPreset;
}


template<class TransferFunc>
void AudioReverbZone::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	TRANSFER(m_MinDistance);
	TRANSFER(m_MaxDistance);
	TRANSFER(m_ReverbPreset);
	TRANSFER(m_Room); // room effect level (at mid frequencies)
	TRANSFER(m_RoomHF); // relative room effect level at high frequencies	
	TRANSFER(m_DecayTime); // reverberation decay time at mid frequencies
	TRANSFER(m_DecayHFRatio); //  high-frequency to mid-frequency decay time ratio	
	TRANSFER(m_Reflections); // early reflections level relative to room effect
	TRANSFER(m_ReflectionsDelay); //  initial reflection delay time
	TRANSFER(m_Reverb); //  late reverberation level relative to room effect
	TRANSFER(m_ReverbDelay); //  late reverberation delay time relative to initial reflection 
	TRANSFER(m_HFReference); // reference high frequency (hz)
	TRANSFER(m_RoomRolloffFactor); //  like rolloffscale in global settings, but for reverb room size effect
	TRANSFER(m_Diffusion); //  Value that controls the echo density in the late reverberation decay
	TRANSFER(m_Density); // Value that controls the modal density in the late reverberation decay
	TRANSFER(m_LFReference); // reference low frequency (hz)
	TRANSFER(m_RoomLF);
}


IMPLEMENT_CLASS (AudioReverbZone)
IMPLEMENT_OBJECT_SERIALIZE (AudioReverbZone)
#endif //ENABLE_AUDIO
