#include "UnityPrefix.h"
#include "AudioChorusFilter.h"
#include "Runtime/Utilities/Utility.h"

#if ENABLE_AUDIO_FMOD

AudioChorusFilter::AudioChorusFilter (MemLabelId label, ObjectCreationMode mode) :
Super(label, mode),
m_DryMix(0.5f),
m_WetMix1(0.5f),
m_WetMix2(0.5f),
m_WetMix3(0.5f),
m_Delay(40.0f),
m_Rate(0.8f),
m_Depth(0.03f)
{
	m_Type = FMOD_DSP_TYPE_CHORUS;
}

AudioChorusFilter::~AudioChorusFilter()
{}


void AudioChorusFilter::AddToManager()
{
	Super::AddToManager();
}

void AudioChorusFilter::Reset()
{
	Super::Reset();
	
	m_DryMix = 0.5f;
	m_WetMix1 = 0.5f;
	m_WetMix2 = 0.5f;
	m_WetMix3 = 0.5f;
	m_Delay = 40.0f;
	m_Rate = 0.8f;
	m_Depth = 0.03f;
}

void AudioChorusFilter::CheckConsistency()
{
	Super::CheckConsistency();
	m_DryMix = clamp(m_DryMix,0.0f,1.0f);
	m_WetMix1 = clamp(m_WetMix1,0.0f,1.0f);
	m_WetMix2 = clamp(m_WetMix2, 0.0f, 1.0f);
	m_WetMix3 = clamp(m_WetMix3, 0.0f, 1.0f);
	m_Delay = clamp(m_Delay, 0.1f, 100.0f);
	m_Rate = clamp(m_Rate,0.0f,20.0f);
	m_Depth = clamp(m_Depth,0.0f,1.0f);
}

void AudioChorusFilter::Update()
{
	if (m_DSP)
	{
		m_DSP->setParameter(FMOD_DSP_CHORUS_DRYMIX, m_DryMix);
		m_DSP->setParameter(FMOD_DSP_CHORUS_WETMIX1, m_WetMix1);
		m_DSP->setParameter(FMOD_DSP_CHORUS_WETMIX2, m_WetMix2);
		m_DSP->setParameter(FMOD_DSP_CHORUS_WETMIX3, m_WetMix3);
		m_DSP->setParameter(FMOD_DSP_CHORUS_DELAY, m_Delay);
		m_DSP->setParameter(FMOD_DSP_CHORUS_RATE, m_Rate);
		m_DSP->setParameter(FMOD_DSP_CHORUS_DEPTH, m_Depth);
	}
}


template<class TransferFunc>
void AudioChorusFilter::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	TRANSFER(m_DryMix);
	TRANSFER(m_WetMix1);
	TRANSFER(m_WetMix2);
	TRANSFER(m_WetMix3);
	TRANSFER(m_Delay);
	TRANSFER(m_Rate);
	TRANSFER(m_Depth);
}


IMPLEMENT_CLASS (AudioChorusFilter)
IMPLEMENT_OBJECT_SERIALIZE (AudioChorusFilter)

#endif //ENABLE_AUDIO
