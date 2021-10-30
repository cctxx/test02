#ifndef __AUDIODISTORTION_FILTER_H__ 
#define __AUDIODISTORTION_FILTER_H__ 

#if ENABLE_AUDIO_FMOD
#include "AudioSourceFilter.h"

class AudioDistortionFilter : public AudioFilter {
public:
	REGISTER_DERIVED_CLASS   (AudioDistortionFilter, AudioFilter)
	DECLARE_OBJECT_SERIALIZE (AudioDistortionFilter)
	AudioDistortionFilter (MemLabelId label, ObjectCreationMode mode);

	virtual void CheckConsistency ();
	virtual void Update();
	virtual void AddToManager();
	virtual void Reset();

	float GetDistortionLevel() const { return m_DistortionLevel; }
	void SetDistortionLevel(const float distortionLevel) { m_DistortionLevel = distortionLevel; Update(); SetDirty(); }

private:
	float m_DistortionLevel; // Distortion value. 0.0 to 1.0. Default = 0.5.
};

#endif // ENABLE_AUDIO
#endif // AUDIODISTORTION
