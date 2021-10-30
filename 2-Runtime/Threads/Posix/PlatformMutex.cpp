#include "UnityPrefix.h"

#if SUPPORT_THREADS

#ifndef MUTEX_API_PTHREAD
#define MUTEX_API_PTHREAD (UNITY_OSX || UNITY_IPHONE || UNITY_ANDROID || UNITY_PEPPER || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN)
#endif

#endif // SUPPORT_THREADS

#if MUTEX_API_PTHREAD
// -------------------------------------------------------------------------------------------------
//  pthreads

#include "PlatformMutex.h"

#if defined(__native_client__)
#define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif

PlatformMutex::PlatformMutex ( )
{
	pthread_mutexattr_t    attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
	pthread_mutex_init(&mutex, &attr);
	pthread_mutexattr_destroy(&attr);
}

PlatformMutex::~PlatformMutex ()
{
	pthread_mutex_destroy(&mutex);
}

void PlatformMutex::Lock()
{
	pthread_mutex_lock(&mutex);
}

void PlatformMutex::Unlock()
{
	pthread_mutex_unlock(&mutex);
}

bool PlatformMutex::TryLock()
{
	return pthread_mutex_trylock(&mutex) == 0;
}

#endif // MUTEX_API_PTHREAD
