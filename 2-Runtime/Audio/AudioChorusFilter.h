#ifndef __AUDIOCHORUS_FILTER_H__ 
#define __AUDIOCHORUS_FILTER_H__ 

#if ENABLE_AUDIO_FMOD
#include "AudioSourceFilter.h"

class AudioChorusFilter : public AudioFilter
{
public:
	REGISTER_DERIVED_CLASS   (AudioChorusFilter, AudioFilter)
	DECLARE_OBJECT_SERIALIZE (AudioChorusFilter)
	AudioChorusFilter (MemLabelId label, ObjectCreationMode mode);

	virtual void CheckConsistency ();
	virtual void Update();
	virtual void AddToManager();
	virtual void Reset();

	float GetDelay() const { return m_Delay; }
	void SetDelay(const float delay) { m_Delay = delay; Update(); SetDirty(); }

	float GetRate() const { return m_Rate;  }
	void SetRate(const float rate) { m_Rate = rate; Update(); SetDirty(); }
	
	float GetDryMix() const { return m_DryMix; }
	void SetDryMix(const float drymix) { m_DryMix = drymix; Update(); SetDirty(); }
	
	float GetWetMix1() const { return m_WetMix1; }
	void SetWetMix1(const float wetmix) { m_WetMix1 = wetmix; Update(); SetDirty(); }	
	
	float GetWetMix2() const { return m_WetMix2; }
	void SetWetMix2(const float wetmix) { m_WetMix2 = wetmix; Update(); SetDirty();}	
	
	float GetWetMix3() const { return m_WetMix3; }
	void SetWetMix3(const float wetmix) { m_WetMix3 = wetmix; Update(); SetDirty(); }	
	
	float GetDepth() const { return m_Depth; }
	void SetDepth(const float depth) { m_Depth = depth; Update(); SetDirty();}

private:
	float m_DryMix; //	FMOD_DSP_CHORUS_DRYMIX,	Volume of original signal to pass to output. 0.0 to 1.0. Default = 0.5. 
	float m_WetMix1; // FMOD_DSP_CHORUS_WETMIX1, Volume of 1st chorus tap. 0.0 to 1.0. Default = 0.5. 
	float m_WetMix2; // FMOD_DSP_CHORUS_WETMIX2, Volume of 2nd chorus tap. This tap is 90 degrees out of phase of the first tap. 0.0 to 1.0. Default = 0.5.
	float m_WetMix3; // FMOD_DSP_CHORUS_WETMIX3,  Volume of 3rd chorus tap. This tap is 90 degrees out of phase of the second tap. 0.0 to 1.0. Default = 0.5. 
	float m_Delay; // FMOD_DSP_CHORUS_DELAY, Chorus delay in ms. 0.1 to 100.0. Default = 40.0 ms. 
	float m_Rate; // FMOD_DSP_CHORUS_RATE, Chorus modulation rate in hz. 0.0 to 20.0. Default = 0.8 hz. 
	float m_Depth; // FMOD_DSP_CHORUS_DEPTH, Chorus modulation depth. 0.0 to 1.0. Default = 0.03.
};

#endif //ENABLE_AUDIO
#endif // __AUDIOSOURCE_FILTER_H__ 
