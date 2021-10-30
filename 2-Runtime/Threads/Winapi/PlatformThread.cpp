#include "UnityPrefix.h"

#if SUPPORT_THREADS

#ifndef THREAD_API_WINAPI
#define THREAD_API_WINAPI (UNITY_WIN || UNITY_XENON || UNITY_WINRT)
#endif

#endif // SUPPORT_THREADS

#if THREAD_API_WINAPI

#include "PlatformThread.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/ThreadHelper.h"

#include "Runtime/Utilities/Word.h"

// module@TODO : Move this to PlatformThread.h
#if UNITY_WINRT
#include "PlatformDependent/MetroPlayer/Win32Threads.h"
#endif

PlatformThread::PlatformThread()
:	m_Thread(NULL)
#if !UNITY_WINRT
,	m_ThreadId(0)
#endif
{
}

PlatformThread::~PlatformThread()
{
	AssertMsg(m_Thread == NULL, "***Thread was not cleaned up!***");
}


void PlatformThread::Create(const Thread* thread, const UInt32 stackSize, const int processor)
{
#if UNITY_WINRT
	m_Thread = win32::CreateThread(Thread::RunThreadWrapper, (LPVOID) thread);
#else // UNITY_WINRT
	DWORD creationFlags = 0;
#if UNITY_XENON
	if (processor != DEFAULT_UNITY_THREAD_PROCESSOR)
		creationFlags = CREATE_SUSPENDED;
#endif

	m_Thread = ::CreateThread(NULL, stackSize, Thread::RunThreadWrapper, (LPVOID) thread, creationFlags, &m_ThreadId);
	Assert(NULL != m_Thread);

#if UNITY_XENON
	if (processor != DEFAULT_UNITY_THREAD_PROCESSOR)
	{
		ThreadHelper::SetThreadProcessor(thread, processor);
		ResumeThread(m_Thread);
	}
#endif

#endif // UNITY_WINRT

}

void PlatformThread::Enter(const Thread* thread)
{
	if (thread->m_Priority != kNormalPriority)
		UpdatePriority(thread);
}

void PlatformThread::Exit(const Thread* thread, void* result)
{
}

void PlatformThread::Join(const Thread* thread)
{
#if !UNITY_WINRT		// Why doesn't WINRT store the thread ID ?
	if (Thread::EqualsCurrentThreadID(m_ThreadId))
	{
		ErrorStringMsg("***Thread '%s' tried to join itself!***", thread->m_Name);
	}
#endif

	if (thread->m_Running)
	{
		DWORD waitResult = WaitForSingleObjectEx(m_Thread, INFINITE, FALSE);
		Assert(WAIT_OBJECT_0 == waitResult);
	}

	if (m_Thread != NULL)
	{
		BOOL closeResult = CloseHandle(m_Thread);
		Assert(FALSE != closeResult);
	}
	m_Thread = NULL;
}

void PlatformThread::UpdatePriority(const Thread* thread) const
{
	ThreadPriority p = thread->m_Priority;

#if UNITY_WINRT

	#pragma message("todo: implement")	// ?!-

#else

    int iPriority;
    switch (p)
    {
	case kLowPriority:
			iPriority = THREAD_PRIORITY_LOWEST;
			break;

		case kBelowNormalPriority:
			iPriority = THREAD_PRIORITY_BELOW_NORMAL;
			break;

		case kNormalPriority:
			iPriority = THREAD_PRIORITY_NORMAL;
			break;
		case kHighPriority:
			iPriority = THREAD_PRIORITY_HIGHEST;
			break;

		default:
			AssertString("Undefined thread priority");
    }

    int res = SetThreadPriority(m_Thread, iPriority);
	AssertIf(res == 0);

#endif
}

PlatformThread::ThreadID PlatformThread::GetCurrentThreadID()
{
	return GetCurrentThreadId();
}

#endif // THREAD_API_PTHREAD
