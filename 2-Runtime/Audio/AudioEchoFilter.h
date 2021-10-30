#ifndef __AUDIOECHO_FILTER_H__
#define __AUDIOECHO_FILTER_H__

#if ENABLE_AUDIO_FMOD

#include "AudioSourceFilter.h"

class AudioEchoFilter : public AudioFilter
{
public:
	REGISTER_DERIVED_CLASS   (AudioEchoFilter, AudioFilter)
		DECLARE_OBJECT_SERIALIZE (AudioEchoFilter)
	AudioEchoFilter (MemLabelId label, ObjectCreationMode mode);

	virtual void CheckConsistency ();
	virtual void AddToManager();
	virtual void Reset();

	void Update();

	float GetDelay() const { return m_Delay; }
	void SetDelay(const float delay) { m_Delay = (unsigned)delay; Update(); SetDirty(); }

	float GetDecayRatio() const { return m_DecayRatio;  }
	void SetDecayRatio(const float decay) { m_DecayRatio = decay; Update(); SetDirty(); }
	
	float GetDryMix() const { return m_DryMix; }
	void SetDryMix(const float drymix) { m_DryMix = drymix; Update(); SetDirty(); }
	
	float GetWetMix() const { return m_WetMix; }
	void SetWetMix(const float wetmix) { m_WetMix = wetmix; Update(); SetDirty();  }	

private:
	unsigned m_Delay; // Echo delay in ms. 10 to 5000. Default = 500.  
	float m_DecayRatio; // Echo decay per delay. 0 to 1. 1.0 = No decay, 0.0 = total decay (ie simple 1 line delay). Default = 0.5. 
	float m_DryMix; // Volume of original signal to pass to output. 0.0 to 1.0. Default = 1.0. 
	float m_WetMix; // Volume of echo signal to pass to output. 0.0 to 1.0. Default = 1.0.	
};

#endif //ENABLE_AUDIO
#endif // ___AUDIOECHO_FILTER_H__
