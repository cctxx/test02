#pragma once

#include "Runtime/Audio/correct_fmod_includer.h" // can't forward declare enums (@TODO use ints?)

// macros/helpers
#if ENABLE_AUDIO_FMOD
#define UNITYVEC2FMODVEC(v) *(reinterpret_cast<FMOD_VECTOR*>(&v))  
#define UNITYVEC2FMODVECPTR(v) (reinterpret_cast<FMOD_VECTOR*>(&v)) 

#define FMOD_ASSERT(x) {Assert(x == FMOD_OK);\
if (x != FMOD_OK){	ErrorString(FMOD_ErrorString(result));}}

typedef FMOD::Channel AudioChannel;


#endif //ENABLE_AUDIO_FMOD
