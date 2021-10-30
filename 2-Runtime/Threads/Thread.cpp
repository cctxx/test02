#include "UnityPrefix.h"

#if SUPPORT_THREADS
#include "Thread.h"
#include "ThreadHelper.h"
#include "ThreadUtility.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Allocator/MemoryManager.h"

#if !UNITY_EXTERNAL_TOOL
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Allocator/MemoryMacros.h" 
#endif

#if SUPPORT_ERROR_EXIT
#include "Runtime/Utilities/ErrorExit.h"
#endif

Thread::ThreadID Thread::mainThreadId = Thread::GetCurrentThreadID();

UNITY_THREAD_FUNCTION_RETURN_SIGNATURE Thread::RunThreadWrapper (void* ptr)
{
	Thread* thread = (Thread*)ptr;
	
	#if ENABLE_MEMORY_MANAGER
	GetMemoryManager().ThreadInitialize();
	#endif

	thread->m_Thread.Enter(thread);				// PlatformThread (Posix/Winapi/Custom)

	ThreadHelper::SetThreadName(thread);

	void *result = NULL;
#if SUPPORT_ERROR_EXIT
	ERROR_EXIT_THREAD_ENTRY();
	result = thread->m_EntryPoint(thread->m_UserData);
	ERROR_EXIT_THREAD_EXIT();
#else // SUPPORT_ERROR_EXIT
	result = thread->m_EntryPoint(thread->m_UserData);
#endif // SUPPORT_ERROR_EXIT

	// NOTE: code below will not execute if thread is terminated
	thread->m_Running = false;
	UnityMemoryBarrier();
	
	#if ENABLE_MEMORY_MANAGER
	GetMemoryManager().ThreadCleanup();
	#endif

	thread->m_Thread.Exit(thread, result);				// PlatformThread (Posix/Winapi/Custom)

	return reinterpret_cast<UNITY_THREAD_FUNCTION_RETURNTYPE>(result);
}

Thread::Thread ()
:	m_UserData(NULL)
,	m_EntryPoint(NULL)
,	m_Running(false)
,	m_ShouldQuit(false)
,	m_Priority(kNormalPriority)
,	m_Name(NULL)
{
}

Thread::~Thread()
{
	AssertMsg(!m_Running, "***Thread '%s' is still running!***", m_Name);
}

void Thread::Run(void* (*entry_point) (void*), void* data, const UInt32 stackSize, int processor)
{
	Assert(!m_Running);

	m_ShouldQuit = false;
	m_UserData = data;
	m_EntryPoint = entry_point;
	m_Running = true;

	m_Thread.Create(this, stackSize, processor);
}

void Thread::WaitForExit(bool signalQuit)
{
	if (m_Running && signalQuit)
			SignalQuit();

	m_Thread.Join(this);				// PlatformThread (Posix/Winapi/Custom)

	Assert(!m_Running && "Thread shouldn't be running anymore");
	m_Running = false;
}

Thread::ThreadID Thread::GetCurrentThreadID()	
{
	return PlatformThread::GetCurrentThreadID();
}

void Thread::Sleep (double time)
{
	ThreadHelper::Sleep(time);
}

void Thread::SetPriority(ThreadPriority prio)
{
	if (!m_Running || m_Priority == prio)
		return;

	m_Priority = prio;
	m_Thread.UpdatePriority(this);				// PlatformThread (Posix/Winapi/Custom)
}

void Thread::SetCurrentThreadProcessor(int processor)
{
	ThreadHelper::SetThreadProcessor(NULL, processor);
}

double Thread::GetThreadRunningTime(ThreadID threadId)
{
	return ThreadHelper::GetThreadRunningTime(threadId);
}

#endif
