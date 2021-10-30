#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#if ENABLE_AUDIO_FMOD
#include "AudioReverbFilter.h"
#include "Runtime/Utilities/Utility.h"
#include "AudioTypes.h" 

/*                                    Inst Env  Diffus  Room   RoomHF  RmLF DecTm   DecHF  DecLF   Refl  RefDel   Revb  RevDel  ModTm  ModDp   HFRef    LFRef   Diffus  Densty  FLAGS */
#define FMOD_PRESET_DRUGGED          {  0,  23,   0.50f, -1000,  0,      0,   8.39f,  1.39f, 1.0f,  -115,  0.002f,  985, 0.030f, 0.250f, 0.00f,  5000.0f, 250.0f, 100.0f, 100.0f, 0x1f }
#define FMOD_PRESET_DIZZY            {  0,  24,    0.60f, -1000,  -400,   0,   17.23f, 0.56f, 1.0f,  -1713, 0.020f,  -613, 0.030f,  0.250f,  0.310f, 5000.0f, 250.0f, 100.0f, 100.0f, 0x1f }
#define FMOD_PRESET_PSYCHOTIC        {  0,  25,   0.50f, -1000,  -151,   0,   7.56f,  0.91f, 1.0f,  -626,  0.020f,    774, 0.030f,  0.250f, 0.00f, 5000.0f, 250.0f,  100.0f, 100.0f, 0x1f }

	
FMOD_REVERB_PROPERTIES ReverbPresets [] =
{
	FMOD_PRESET_OFF,             
	FMOD_PRESET_GENERIC,         
	FMOD_PRESET_PADDEDCELL,    
	FMOD_PRESET_ROOM,         
	FMOD_PRESET_BATHROOM,        
	FMOD_PRESET_LIVINGROOM,      
	FMOD_PRESET_STONEROOM,       
	FMOD_PRESET_AUDITORIUM,      
	FMOD_PRESET_CONCERTHALL,     
	FMOD_PRESET_CAVE,           
	FMOD_PRESET_ARENA,           
	FMOD_PRESET_HANGAR,          
	FMOD_PRESET_CARPETTEDHALLWAY,
	FMOD_PRESET_HALLWAY,         
	FMOD_PRESET_STONECORRIDOR,   
	FMOD_PRESET_ALLEY,           
	FMOD_PRESET_FOREST,          
	FMOD_PRESET_CITY,            
	FMOD_PRESET_MOUNTAINS,       
	FMOD_PRESET_QUARRY,          
	FMOD_PRESET_PLAIN,           
	FMOD_PRESET_PARKINGLOT,      
	FMOD_PRESET_SEWERPIPE,       
	FMOD_PRESET_UNDERWATER,      
	FMOD_PRESET_DRUGGED,         
	FMOD_PRESET_DIZZY,           
	FMOD_PRESET_PSYCHOTIC
};

AudioReverbFilter::AudioReverbFilter (MemLabelId label, ObjectCreationMode mode) :
Super(label, mode),
m_DryLevel(0.0f), // Dry Level : Mix level of dry signal in output in mB. Ranges from -10000.0 to 0.0. Default is 0. 
m_Room(0.0f),    // Room : Room effect level at low frequencies in mB. Ranges from -10000.0 to 0.0. Default is 0.0. 
m_RoomHF(0.0f),	 // Room HF : Room effect high-frequency level re. low frequency level in mB. Ranges from -10000.0 to 0.0. Default is 0.0. 
m_RoomRolloff(10.0f), // Room Rolloff : Like DS3D flRolloffFactor but for room effect. Ranges from 0.0 to 10.0. Default is 10.0 
m_DecayTime(1.0f), // Reverberation decay time at low-frequencies in seconds. Ranges from 0.1 to 20.0. Default is 1.0. 
m_DecayHFRatio(0.5f), // Decay HF Ratio : High-frequency to low-frequency decay time ratio. Ranges from 0.1 to 2.0. Default is 0.5. 
m_ReflectionsLevel(-10000.0f), // Early reflections level relative to room effect in mB. Ranges from -10000.0 to 1000.0. Default is -10000.0. 
m_ReverbLevel(0.0f), //  Reverb : Late reverberation level relative to room effect in mB. Ranges from -10000.0 to 2000.0. Default is 0.0. 
m_ReverbDelay(0.04f), // Late reverberation delay time relative to first reflection in seconds. Ranges from 0.0 to 0.1. Default is 0.04. 
m_Diffusion(100.0f), //  Reverberation diffusion (echo density) in percent. Ranges from 0.0 to 100.0. Default is 100.0. 
m_Density(100.0f), // Reverberation density (modal density) in percent. Ranges from 0.0 to 100.0. Default is 100.0. 
m_HFReference(5000.0f), // HF Reference : Reference high frequency in Hz. Ranges from 20.0 to 20000.0. Default is 5000.0. 
m_RoomLF(0.0f), // Room effect low-frequency level in mB. Ranges from -10000.0 to 0.0. Default is 0.0. 
m_LFReference(250.0f), // Reference low-frequency in Hz. Ranges from 20.0 to 1000.0. Default is 250.0. 
m_ReflectionsDelay(0.0f), // Late reverberation level relative to room effect in mB. Ranges from -10000.0 to 2000.0. Default is 0.0.
m_ReverbPreset(27)
{
	m_Type = FMOD_DSP_TYPE_SFXREVERB;
}

AudioReverbFilter::~AudioReverbFilter()
{}

void AudioReverbFilter::AddToManager()
{
	Super::AddToManager();
}

void AudioReverbFilter::CheckConsistency()
{
	Super::CheckConsistency();
	m_DryLevel = clamp(m_DryLevel,-10000.0f, 0.0f);	
	m_Room = clamp(m_Room,-10000.0f, 0.0f);
	m_RoomHF = clamp(m_RoomHF,-10000.0f, 0.0f);
	m_RoomRolloff = clamp(m_RoomRolloff,-10000.0f, 0.0f);
	m_DecayTime = clamp(m_DecayTime,0.1f, 20.0f);
	m_DecayHFRatio = clamp(m_DecayHFRatio, 0.1f, 2.0f);
	m_ReflectionsLevel = clamp(m_ReflectionsLevel, -10000.0f, 1000.0f);
	m_ReverbLevel = clamp(m_ReverbLevel, -10000.0f, 2000.0f);
	m_ReverbDelay = clamp(m_ReverbDelay, 0.0f, 0.1f);
	m_Diffusion = clamp(m_Diffusion, 0.0f, 100.0f);
	m_Density = clamp(m_Density,0.0f, 100.0f);
	m_HFReference = clamp(m_HFReference, 20.0f, 20000.0f);
	m_RoomLF = clamp(m_RoomLF, -10000.0f, 0.0f);
	m_LFReference = clamp(m_LFReference, 20.0f, 10000.0f);	
}

void AudioReverbFilter::Reset()
{
	Super::Reset();
	
	m_DryLevel = 0.0f;
	m_Room = 0.0f;
	m_RoomHF = 0.0f;
	m_RoomRolloff = 10.0f;
	m_DecayTime = 1.0f;
	m_DecayHFRatio = 0.5f;
	m_ReflectionsLevel = -10000.0f;
	m_ReverbLevel = 0.0f;
	m_ReverbDelay = 0.04f;
	m_Diffusion = 100.0f;
	m_Density = 100.0f;
	m_HFReference = 5000.0f;
	m_RoomLF = 0.0f;
	m_LFReference = 250.0f;
	m_ReflectionsDelay = 0.0f;
	m_ReverbPreset = 27;	
}

void AudioReverbFilter::Update()
{
	ChangeProperties();
	if (m_DSP)
	{
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_DRYLEVEL, m_DryLevel);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_ROOM, m_Room);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_ROOMHF, m_RoomHF);
		//m_DSP->setParameter(FMOD_DSP_SFXREVERB_ROOMROLLOFFFACTOR, m_RoomRolloff);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_DECAYTIME, m_DecayTime);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_DECAYHFRATIO, m_DecayHFRatio);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_REFLECTIONSLEVEL, m_ReflectionsLevel);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_REFLECTIONSDELAY, m_ReflectionsDelay);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_REVERBLEVEL, m_ReverbLevel);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_REVERBDELAY, m_ReverbDelay);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_DIFFUSION, m_Diffusion);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_DENSITY, m_Density);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_HFREFERENCE, m_HFReference);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_ROOMLF, m_RoomLF);
		m_DSP->setParameter(FMOD_DSP_SFXREVERB_LFREFERENCE, m_LFReference);			
	}
}

void AudioReverbFilter::ChangeProperties()
{
	if (m_ReverbPreset < 27)
	{
		FMOD_REVERB_PROPERTIES prop = ReverbPresets[m_ReverbPreset];
		m_Room = prop.Room;
		m_RoomHF = prop.RoomHF;
		//m_RoomRolloff = prop.RoomRolloffFactor;
		m_DecayTime = prop.DecayTime;
		m_DecayHFRatio = prop.DecayHFRatio;
		m_ReflectionsLevel = prop.Reflections;
		m_ReverbLevel = prop.Reverb;
		m_ReverbDelay = prop.ReverbDelay;
		m_Diffusion = prop.Diffusion;
		m_Density = prop.Density;
		m_HFReference = prop.HFReference;
		m_RoomLF = prop.RoomLF;
		m_LFReference = prop.LFReference;
		SetDirty();
	}	
}

void AudioReverbFilter::SetReverbPreset(const int reverbPreset)
{
	m_ReverbPreset = reverbPreset;
	ChangeProperties();
	Update();
}

template<class TransferFunc>
void AudioReverbFilter::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	TRANSFER(m_DryLevel);
	TRANSFER(m_Room);
	TRANSFER(m_RoomHF);
	TRANSFER(m_RoomRolloff);
	TRANSFER(m_DecayTime);
	TRANSFER(m_DecayHFRatio);
	TRANSFER(m_ReflectionsLevel);
	TRANSFER(m_ReverbLevel);
	TRANSFER(m_ReverbDelay);
	TRANSFER(m_Diffusion);
	TRANSFER(m_Density);
	TRANSFER(m_HFReference);
	TRANSFER(m_RoomLF);
	TRANSFER(m_LFReference);
	TRANSFER(m_ReflectionsDelay);
	TRANSFER(m_ReverbPreset);
}


IMPLEMENT_CLASS (AudioReverbFilter)
IMPLEMENT_OBJECT_SERIALIZE (AudioReverbFilter)

#endif //ENABLE_AUDI
