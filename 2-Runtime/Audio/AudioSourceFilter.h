#ifndef __AUDIOSOURCE_FILTER_H__
#define __AUDIOSOURCE_FILTER_H__

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "AudioManager.h"
#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Audio/correct_fmod_includer.h"

#if ENABLE_AUDIO_FMOD

using namespace Unity;

class AudioFilter : public Behaviour
{
public:	
	REGISTER_DERIVED_ABSTRACT_CLASS (AudioFilter, Behaviour)
	DECLARE_OBJECT_SERIALIZE (AudioFilter)
	
	AudioFilter(MemLabelId label, ObjectCreationMode mode) : Behaviour(label, mode), m_DSP(NULL), m_Type(FMOD_DSP_TYPE_UNKNOWN) {}
	
	FMOD::DSP* GetDSP();
	
	virtual void RemoveFromManager();
	virtual void AddToManager();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	void Init();
	void Cleanup();
protected:
	FMOD_DSP_TYPE m_Type;
	FMOD::DSP* m_DSP;
	friend class AudioManager;
};

#endif //ENABLE_AUDIO
#endif // __AUDIOSOURCE_FILTER_H__
