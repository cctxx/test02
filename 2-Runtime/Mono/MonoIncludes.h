#ifndef MONOINCLUDES_H
#define MONOINCLUDES_H

#include "Configuration/UnityConfigure.h"
#include "MonoTypes.h"
#include "MonoTypeSignatures.h"

#ifndef __cplusplus
#error somewhat unexpected
#endif


extern "C"
{

// on windows and linux, we always load mono functions dynamically

#if ENABLE_MONO

#define mono_string_chars(s) ((gunichar2*)&((s)->firstCharacter))
#define mono_string_length(s) ((s)->length)
	
#if (WEBPLUG || UNITY_WIN || UNITY_LINUX) && !UNITY_PEPPER

#define DO_API(r,n,p)	extern EXPORT_COREMODULE r (*n) p;
#include "MonoFunctions.h"

#else // WEBPLUG || UNITY_WIN || UNITY_LINUX

#define DO_API(r,n,p)	r n p;
#define DO_API_NO_RETURN(r,n,p)	DOES_NOT_RETURN r n p;
#include "MonoFunctions.h"

#endif // WEBPLUG || UNITY_WIN || UNITY_LINUX

#endif // ENABLE_MONO

}

#endif
