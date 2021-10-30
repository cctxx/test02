#ifndef __AUDIOLOWPASS_FILTER_H__
#define __AUDIOLOWPASS_FILTER_H__
#include "AudioSourceFilter.h"
#include "Runtime/Audio/AudioSource.h"
#include "Runtime/Animation/AnimationCurveUtility.h"
#include "Runtime/Math/AnimationCurve.h"

#if ENABLE_AUDIO_FMOD

class AudioLowPassFilter : public AudioFilter {
public:
	REGISTER_DERIVED_CLASS   (AudioLowPassFilter, AudioFilter)
		DECLARE_OBJECT_SERIALIZE (AudioLowPassFilter)
	AudioLowPassFilter (MemLabelId label, ObjectCreationMode mode);
	
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	
	virtual void CheckConsistency ();
	virtual void Update();
	virtual void AddToManager();

	float GetCutoffFrequency() const { return m_CutoffFrequency; }
	void SetCutoffFrequency(float value) { m_CutoffFrequency = value; Update(); SetDirty();}
	float GetLowpassResonanceQ() const { return m_LowpassResonanceQ; }
	void SetLowpassResonanceQ(float value) { m_LowpassResonanceQ = value; Update(); SetDirty();}	

	AnimationCurve& GetCustomLowpassLevelCurve ();
	const AnimationCurve& GetCustomLowpassLevelCurve () const;
	void SetCustomLowpassLevelCurve(const AnimationCurve& curve);
	
	virtual void Reset();
private:
	AnimationCurve m_LowpassLevelCustomCurve;
	float m_CutoffFrequency;
	float m_LowpassResonanceQ;
	bool m_NeedToNormalizeCurve;
};


#endif // ENABLE_AUDIO
#endif // __AUDIOLOWPASS_FILTER_H__
