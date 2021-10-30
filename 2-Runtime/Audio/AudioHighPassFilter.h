#ifndef __AUDIOHIGHPASS_FILTER_H__
#define __AUDIOHIGHPASS_FILTER_H__

#include "AudioSourceFilter.h"
#if ENABLE_AUDIO_FMOD

class AudioHighPassFilter : public AudioFilter
{
public:
	REGISTER_DERIVED_CLASS   (AudioHighPassFilter, AudioFilter)
	DECLARE_OBJECT_SERIALIZE (AudioHighPassFilter)

	AudioHighPassFilter (MemLabelId label, ObjectCreationMode mode);

	virtual void CheckConsistency ();
	virtual void AddToManager();
	virtual void Reset();

	void Update();

	float GetCutoffFrequency() const { return m_CutoffFrequency; }
	void SetCutoffFrequency(float value) { m_CutoffFrequency = value; Update(); SetDirty();}

	float GetHighpassResonanceQ() const { return m_HighpassResonanceQ; }
	void SetHighpassResonanceQ(float value) { m_HighpassResonanceQ = value; Update(); SetDirty();}	

private:
	float m_CutoffFrequency;
	float m_HighpassResonanceQ;
};

#endif //ENABLE_AUDIO_FMOD
#endif // __AUDIOHIGHPASS_FILTER_H__
