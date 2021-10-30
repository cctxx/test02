#include "UnityPrefix.h"

#if ENABLE_AUDIO_FMOD

#include "AudioEchoFilter.h"
#include "Runtime/Utilities/Utility.h"


AudioEchoFilter::AudioEchoFilter (MemLabelId label, ObjectCreationMode mode) :
Super(label, mode),
m_Delay(500),
m_DecayRatio(0.5f),
m_DryMix(1.0f),
m_WetMix(1.0f)
{
	m_Type = FMOD_DSP_TYPE_ECHO;
}

AudioEchoFilter::~AudioEchoFilter()
{}

void AudioEchoFilter::AddToManager()
{
	Super::AddToManager();
}

void AudioEchoFilter::Reset()
{
	Super::Reset();
	
	m_Delay = 500;
	m_DecayRatio = 0.5f;
	m_DryMix = 1.0f;
	m_WetMix = 1.0f;
}

void AudioEchoFilter::CheckConsistency()
{
	Super::CheckConsistency();
	m_Delay = clamp<int>(m_Delay,10,5000);
	m_DecayRatio = clamp (m_DecayRatio,0.0f,1.0f);
	m_DryMix = clamp(m_DryMix,0.0f,1.0f);
	m_WetMix = clamp(m_WetMix,0.0f,1.0f);
}

void AudioEchoFilter::Update()
{
	if (m_DSP)
	{
		m_DSP->setParameter(FMOD_DSP_ECHO_DELAY, m_Delay);
		m_DSP->setParameter(FMOD_DSP_ECHO_DECAYRATIO, m_DecayRatio);
		m_DSP->setParameter(FMOD_DSP_ECHO_DRYMIX, m_DryMix);
		m_DSP->setParameter(FMOD_DSP_ECHO_WETMIX, m_WetMix);					
	}
}


template<class TransferFunc>
void AudioEchoFilter::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	TRANSFER(m_Delay);
	TRANSFER(m_DecayRatio);
	TRANSFER(m_WetMix);
	TRANSFER(m_DryMix);
}


IMPLEMENT_CLASS (AudioEchoFilter)
IMPLEMENT_OBJECT_SERIALIZE (AudioEchoFilter)

#endif //ENABLE_AUDIO
