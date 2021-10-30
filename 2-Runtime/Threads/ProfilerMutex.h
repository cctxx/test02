#ifndef __PROFILERMUTEX_H
#define __PROFILERMUTEX_H

#include "Mutex.h"

#if ENABLE_PROFILER
#include "Runtime/Profiler/Profiler.h"
#endif

#define THREAD_LOCK_WARNINGS 0
#define THREAD_LOCK_TIMING 0

#if THREAD_LOCK_WARNINGS || (ENABLE_PROFILER && SUPPORT_THREADS)

#define AQUIRE_AUTOLOCK(mutex,profilerInformation) ProfilerMutexAutoLock aquireAutoLock (mutex, #mutex, profilerInformation, __FILE__, __LINE__)
#define AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(mutex,profilerInformation) ProfilerMutexAutoLock aquireAutoLock (mutex, #mutex, Thread::mainThreadId, profilerInformation, __FILE__, __LINE__)
#define LOCK_MUTEX(mutex,profilerInformation) ProfilerMutexLock(mutex,#mutex,profilerInformation,__FILE__,__LINE__)

double GetTimeSinceStartup ();

inline void ProfilerMutexLock (Mutex& mutex, char const* mutexName, ProfilerInformation& information, char const* file, int line)
{
#if ENABLE_PROFILER || THREAD_LOCK_WARNINGS

	if (mutex.TryLock())
		return;

	#if THREAD_LOCK_WARNINGS
	DebugStringToFile (std::string ("Mutex '") + mutexName + "' already locked: " + information.name, 0, file, line, kScriptingWarning);
	#endif

	PROFILER_AUTO_THREAD_SAFE(information, NULL)

#if THREAD_LOCK_TIMING
	double start = GetTimeSinceStartup ();
	while (!mutex.TryLock())
		Sleep (10);

	double duration = GetTimeSinceStartup () - start;
	DebugStringToFile (std::string ("Mutex '") + mutexName + "' obtained after: " + FloatToString(duration, "%6.3f") + " s", 0, file, line, kScriptingWarning);
#else
	mutex.Lock();
#endif

#else
	mutex.Lock();
#endif
}

inline void ProfilerMutexLock (Mutex& mutex, char const* mutexName, Thread::ThreadID threadID, ProfilerInformation& information, char const* file, int line)
{
#if ENABLE_PROFILER || THREAD_LOCK_WARNINGS
	if (mutex.TryLock())
		return;
	
	#if THREAD_LOCK_WARNINGS
	if (Thread::EqualsCurrentThreadID(threadID))
	{
		DebugStringToFile (std::string ("Mutex '") + mutexName + "' already locked: " + information.name, 0, file, line, kScriptingWarning);
	}	
	#endif
	
	PROFILER_AUTO_THREAD_SAFE(information, NULL)

#if THREAD_LOCK_TIMING
	double start = GetTimeSinceStartup ();
	while (!mutex.TryLock())
		Sleep (10);

	double duration = GetTimeSinceStartup () - start;
	DebugStringToFile (std::string ("Mutex '") + mutexName + "' obtained after: " + FloatToString(duration, "%6.3f") + " s", 0, file, line, kScriptingWarning);
#else
	mutex.Lock();
#endif

#else
	mutex.Lock();
#endif
}

class ProfilerMutexAutoLock
{
public:
	ProfilerMutexAutoLock (Mutex& mutex, char const* mutexName, ProfilerInformation& profilerInformation, char const* file, int line)
	:	m_Mutex (&mutex)
	{
		ProfilerMutexLock(mutex, mutexName, profilerInformation, file, line);
	}

	ProfilerMutexAutoLock (Mutex& mutex, char const* mutexName, Thread::ThreadID threadID, ProfilerInformation& profilerInformation, char const* file, int line)
	:	m_Mutex (&mutex)
	{
		ProfilerMutexLock(mutex, mutexName, threadID, profilerInformation, file, line);
	}
	
	~ProfilerMutexAutoLock()
	{
		m_Mutex->Unlock();
	}
	
private:
	ProfilerMutexAutoLock(const ProfilerMutexAutoLock&);
	ProfilerMutexAutoLock& operator=(const ProfilerMutexAutoLock&);
	
private:
	Mutex*	m_Mutex;
};

#else

#define AQUIRE_AUTOLOCK(mutex,profilerInformation) Mutex::AutoLock aquireAutoLock (mutex)
#define AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(mutex,profilerInformation) Mutex::AutoLock aquireAutoLock (mutex)
#define LOCK_MUTEX(mutex,profilerInformation) (mutex).Lock ()


#endif

#endif
