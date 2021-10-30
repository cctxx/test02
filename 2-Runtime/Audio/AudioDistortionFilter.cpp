#include "UnityPrefix.h"
#include "AudioDistortionFilter.h"
#include "Runtime/Utilities/Utility.h"

#if ENABLE_AUDIO_FMOD

AudioDistortionFilter::AudioDistortionFilter (MemLabelId label, ObjectCreationMode mode) :
Super(label, mode),
m_DistortionLevel(0.5f)
{
	m_Type = FMOD_DSP_TYPE_DISTORTION;
}

void AudioDistortionFilter::Reset()
{
	Super::Reset();
	m_DistortionLevel = 0.5f;
}

AudioDistortionFilter::~AudioDistortionFilter()
{}

void AudioDistortionFilter::AddToManager()
{
	Super::AddToManager();
}

void AudioDistortionFilter::CheckConsistency()
{
	Super::CheckConsistency();
	m_DistortionLevel = clamp(m_DistortionLevel,0.0f,1.0f);
}

void AudioDistortionFilter::Update()
{
	if (m_DSP)
		m_DSP->setParameter(FMOD_DSP_DISTORTION_LEVEL, m_DistortionLevel);				
}


template<class TransferFunc>
void AudioDistortionFilter::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	TRANSFER(m_DistortionLevel);
}


IMPLEMENT_CLASS (AudioDistortionFilter)
IMPLEMENT_OBJECT_SERIALIZE (AudioDistortionFilter)

#endif