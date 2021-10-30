#include "UnityPrefix.h"

#include "ExtensionsGLES.h"
#include "IncludesGLES.h"

#if GFX_SUPPORTS_OPENGLESXX

#if UNITY_IPHONE
	#include <dlfcn.h>
#endif

void* GetGLExtProcAddress(const char* name)
{
#if GFX_SUPPORTS_EGL && !UNITY_WIN
	return (void*) eglGetProcAddress(name);
#elif UNITY_IPHONE

	// on ios we link to framework, so symbols are already resolved
	static void* selfHandle = 0;
	if(!selfHandle)
		selfHandle = dlopen(0, RTLD_LOCAL | RTLD_LAZY);

	return selfHandle ? dlsym(selfHandle, name) : 0;
#else
	return 0;

#endif
}

#endif