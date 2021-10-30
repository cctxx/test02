#include "UnityPrefix.h"
#include "AudioLowPassFilter.h"
#include "Runtime/Utilities/Utility.h"

#if ENABLE_AUDIO_FMOD

AudioLowPassFilter::AudioLowPassFilter (MemLabelId label, ObjectCreationMode mode) :
Super(label, mode),
m_CutoffFrequency(5000.0f),
m_LowpassResonanceQ(1.0f),
m_NeedToNormalizeCurve(false)
{
	m_Type = FMOD_DSP_TYPE_LOWPASS;
}

AudioLowPassFilter::~AudioLowPassFilter()
{}

void AudioLowPassFilter::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	
	if (m_NeedToNormalizeCurve)
	{
		AudioSource* source = QueryComponent(AudioSource);
		if (source)
			ScaleCurveTime(m_LowpassLevelCustomCurve, 1 / source->GetMaxDistance());
	}
}

void AudioLowPassFilter::Reset()
{
	Super::Reset();
	
	m_CutoffFrequency = 5000.0f;
	m_LowpassResonanceQ = 1.0f;
	m_NeedToNormalizeCurve = false;
	
	// Curve initial values will be handled in CheckConsistency
	m_LowpassLevelCustomCurve.ResizeUninitialized (0);
	CheckConsistency ();
}

void AudioLowPassFilter::AddToManager()
{
	Super::AddToManager();
}

void AudioLowPassFilter::CheckConsistency()
{
	Super::CheckConsistency();
	// @TODO get output freq from audiomanager
	m_CutoffFrequency = clamp(m_CutoffFrequency, 10.0f, 22000.0f);
	m_LowpassResonanceQ = clamp(m_LowpassResonanceQ, 1.0f, 10.0f);
	
	if (m_LowpassLevelCustomCurve.GetKeyCount() < 1)
		m_LowpassLevelCustomCurve.AddKey(AnimationCurve::Keyframe(0.0f, 1 - m_CutoffFrequency / 22000.0f));
}

void AudioLowPassFilter::Update()
{
	if (m_DSP)
	{
		m_DSP->setParameter(FMOD_DSP_LOWPASS_CUTOFF, m_CutoffFrequency);
		m_DSP->setParameter(FMOD_DSP_LOWPASS_RESONANCE, m_LowpassResonanceQ);
	}
}
/// Set/Get spread curve
AnimationCurve& AudioLowPassFilter::GetCustomLowpassLevelCurve ()
{
	return m_LowpassLevelCustomCurve;
}


const AnimationCurve& AudioLowPassFilter::GetCustomLowpassLevelCurve () const
{
	return m_LowpassLevelCustomCurve;
}

void AudioLowPassFilter::SetCustomLowpassLevelCurve(const AnimationCurve& curve)
{
	m_LowpassLevelCustomCurve = curve;
	SetDirty();
}


template<class TransferFunc>
void AudioLowPassFilter::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	
	// 3.5  3: Normalized curve values
	// <3.5	2: Non-Normalized curves
	transfer.SetVersion (3);
	
	TRANSFER(m_CutoffFrequency);
	TRANSFER(m_LowpassResonanceQ);
	
	transfer.Transfer(m_LowpassLevelCustomCurve, "lowpassLevelCustomCurve");
	
	if (transfer.IsVersionSmallerOrEqual(2))
	{
		m_NeedToNormalizeCurve = true;
	}	
}


IMPLEMENT_CLASS (AudioLowPassFilter)
IMPLEMENT_OBJECT_SERIALIZE (AudioLowPassFilter)

#endif //ENABLE_AUDIO