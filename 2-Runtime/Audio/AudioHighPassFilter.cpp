#include "UnityPrefix.h"
#if ENABLE_AUDIO_FMOD
#include "AudioHighPassFilter.h"
#include "Runtime/Utilities/Utility.h"


AudioHighPassFilter::AudioHighPassFilter (MemLabelId label, ObjectCreationMode mode) :
Super(label, mode),
m_CutoffFrequency(5000.0f),
m_HighpassResonanceQ(1.0f)
{
	m_Type = FMOD_DSP_TYPE_HIGHPASS;
}

AudioHighPassFilter::~AudioHighPassFilter()
{}

void AudioHighPassFilter::AddToManager()
{
	Super::AddToManager();
}

void AudioHighPassFilter::Reset()
{
	Super::Reset();
	
	m_CutoffFrequency = 5000.0f;
	m_HighpassResonanceQ = 1.0f;
}

void AudioHighPassFilter::CheckConsistency()
{
	Super::CheckConsistency();
	// @TODO get output freq from audiomanager
	m_CutoffFrequency = clamp(m_CutoffFrequency, 10.0f, 22000.0f);
	m_HighpassResonanceQ = clamp(m_HighpassResonanceQ, 1.0f, 10.0f);
}

void AudioHighPassFilter::Update()
{
	if (m_DSP)
	{
		m_DSP->setParameter(FMOD_DSP_HIGHPASS_CUTOFF, m_CutoffFrequency);
		m_DSP->setParameter(FMOD_DSP_HIGHPASS_RESONANCE, m_HighpassResonanceQ);
	}
}


template<class TransferFunc>
void AudioHighPassFilter::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	TRANSFER(m_CutoffFrequency);
	TRANSFER(m_HighpassResonanceQ);
}


IMPLEMENT_CLASS (AudioHighPassFilter)
IMPLEMENT_OBJECT_SERIALIZE (AudioHighPassFilter)

#endif //ENABLE_AUDIO