#include "UnityPrefix.h"
#if ENABLE_AUDIO_FMOD
#include "AudioSourceFilter.h"

IMPLEMENT_CLASS(AudioFilter) 
IMPLEMENT_OBJECT_SERIALIZE (AudioFilter)
INSTANTIATE_TEMPLATE_TRANSFER (AudioFilter)

AudioFilter::~AudioFilter ()
{
	Cleanup();
}

void AudioFilter::Cleanup()
{
	if (m_DSP)
	{
		m_DSP->release();
		m_DSP = NULL;
	}
}

void AudioFilter::AddToManager()
{
	if (!m_DSP)
		Init();	
	Update();
	Assert(m_DSP);
	Assert(m_Type != FMOD_DSP_TYPE_UNKNOWN);
	m_DSP->setBypass(false);	
}

void AudioFilter::RemoveFromManager()
{
	if (m_DSP)
		m_DSP->setBypass(true);
}

void AudioFilter::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
	Update();
}

void AudioFilter::Init()
{
	if (m_DSP == NULL && m_Type != FMOD_DSP_TYPE_FORCEINT)
	{
		FMOD_RESULT result = GetAudioManager().GetFMODSystem()->createDSPByType(m_Type, &m_DSP);
		Assert (result == FMOD_OK);
		result = m_DSP->setBypass(!GetEnabled());
		Assert (result == FMOD_OK);
	}
}

FMOD::DSP* AudioFilter::GetDSP()
{
	if (!m_DSP)
		Init();
	return m_DSP;	
}

template<class TransferFunction>
void AudioFilter::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
}
#endif //ENABLE_AUDIO