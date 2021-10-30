#include "UnityPrefix.h"

#if SUPPORT_THREADS

#ifndef THREAD_API_PTHREAD
#define THREAD_API_PTHREAD (UNITY_OSX || UNITY_PS3 || UNITY_IPHONE || UNITY_ANDROID || UNITY_PEPPER || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN)
#endif

#endif // SUPPORT_THREADS

#if THREAD_API_PTHREAD

#include "PlatformThread.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/ThreadHelper.h"

#include "Runtime/Utilities/Word.h"
//#include "Runtime/Utilities/Utility.h"

// module@TODO : Move this to PlatformThread.h
#if UNITY_PS3
#	include <sys/timer.h>
#	include "pthread_ext/pthread_ext.h"
#	define pthread_create pthread_ext_create
#endif

#if UNITY_PEPPER && defined(__native_client__)
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/nacl_syscalls.h>
#endif

#if UNITY_ANDROID
#include <jni.h>
	JavaVM* GetJavaVm();
	extern "C" void* GC_lookup_thread(pthread_t id);
	extern "C" void GC_delete_thread(pthread_t id);
#endif

PlatformThread::PlatformThread()
:	m_Thread((ThreadID)NULL)
{
}

PlatformThread::~PlatformThread()
{
	AssertMsg(m_Thread == (ThreadID)NULL, "***Thread was not cleaned up!***");
}


void PlatformThread::Create(const Thread* thread, const UInt32 stackSize, const int processor)
{
	m_DefaultPriority = 0;
	m_Processor = processor;

	if(stackSize)
	{
		pthread_attr_t attr;
		memset(&attr, 0, sizeof(attr));

// module@TODO : Implement pthread_attr_init/pthread_attr_setstacksize in PlatformThread.h
#if UNITY_PS3
		attr.stacksize = stackSize;
		attr.name = "_UNITY_";
#else
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, stackSize);
#endif
		pthread_create(&m_Thread, &attr, Thread::RunThreadWrapper, (void*)thread);
	}
	else {
		pthread_create(&m_Thread, NULL, Thread::RunThreadWrapper, (void*)thread);
	}

#if UNITY_OSX || UNITY_IPHONE || UNITY_PS3
	// Y U no want on Linux?
	struct sched_param param;
	int outputPolicy;
	if (pthread_getschedparam(m_Thread, &outputPolicy, &param) == 0)
		m_DefaultPriority = param.sched_priority;
	AssertIf(m_DefaultPriority == 0);	// Y U no like 0 priority?
#endif

	if (thread->m_Priority != kNormalPriority)
		UpdatePriority(thread);
}

void PlatformThread::Enter(const Thread* thread)
{
	ThreadHelper::SetThreadProcessor(thread, m_Processor);
}

void PlatformThread::Exit(const Thread* thread, void* result)
{
#if UNITY_ANDROID
	if (GC_lookup_thread(pthread_self()))
		GC_delete_thread(pthread_self());
	GetJavaVm()->DetachCurrentThread();
	pthread_exit(result);
#endif
}

void PlatformThread::Join(const Thread* thread)
{
	if (Thread::EqualsCurrentThreadID(m_Thread))
	{
		ErrorStringMsg("***Thread '%s' tried to join itself!***", thread->m_Name);
	}

	if (m_Thread)
	{
		int error = pthread_join(m_Thread, NULL);
		if (error)
			ErrorString(Format("Error joining threads: %d", error));

		m_Thread = 0;
	}
}

void PlatformThread::UpdatePriority(const Thread* thread) const
{
#if UNITY_PEPPER || UNITY_BB10

	// No thread priority in NaCl yet.
	// For BB10 the user Unity is run as lacks permission to set priority.

#else	// Default POSIX impl below

	ThreadPriority p = thread->m_Priority;

#if UNITY_OSX || UNITY_IPHONE || UNITY_PS3
	AssertIf(m_DefaultPriority == 0);
#endif

	struct sched_param param;
	int policy;
	ErrorIf(pthread_getschedparam(m_Thread, &policy, &param));
#if UNITY_PS3
	int min = 3071;
	int max = 0;
#else
	int min = sched_get_priority_min(policy);
	int max = sched_get_priority_max(policy);
#endif

    int iPriority;
    switch (p)
    {
		case kLowPriority:
			iPriority = min;
			break;

		case kBelowNormalPriority:
			iPriority = min + (m_DefaultPriority-min)/2;
			break;

		case kNormalPriority:
			iPriority = m_DefaultPriority;
			break;

		case kHighPriority:
			iPriority = max;
			break;

		default:
			iPriority = min;
			AssertString("Undefined thread priority");
			break;
    }

	if (param.sched_priority != iPriority)
	{
		param.sched_priority = iPriority;
		ErrorIf(pthread_setschedparam(m_Thread, policy, &param));
	}

#endif
}

PlatformThread::ThreadID PlatformThread::GetCurrentThreadID()
{
	return (ThreadID)pthread_self();
}

#endif // THREAD_API_PTHREAD
