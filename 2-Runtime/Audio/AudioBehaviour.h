#ifndef __AUDIOTYPES_H__
#define __AUDIOTYPES_H__
#if ENABLE_AUDIO

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

// macros/helpers
#define UNITYVEC2FMODVEC(v) *(reinterpret_cast<FMOD_VECTOR*>(&v))  
#define UNITYVEC2FMODVECPTR(v) (reinterpret_cast<FMOD_VECTOR*>(&v)) 

class AudioBehaviour : public Behaviour
{
public:	
	REGISTER_DERIVED_ABSTRACT_CLASS (AudioBehaviour, Behaviour)
	
	AudioBehaviour (MemLabelId label, ObjectCreationMode mode);
};

#endif //ENABLE_AUDIO
#endif // __AUDIOTYPES_H__
