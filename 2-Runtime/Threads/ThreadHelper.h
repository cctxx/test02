#ifndef THREADHELPER_H
#define THREADHELPER_H

#if SUPPORT_THREADS

#include "Thread.h"

// ThreadHelper is typically implemented on a per-platform basis, as it contains OS
// specific functionality outside regular POSIX / pthread / WinAPI threads.

class ThreadHelper
{
	friend class Thread;
	friend class PlatformThread;

protected:
	static void Sleep(double time);

	static void SetThreadName(const Thread* thread);
	static void SetThreadProcessor(const Thread* thread, int processor);

	static double GetThreadRunningTime(Thread::ThreadID thread);

private:
	ThreadHelper();
};

#endif //SUPPORT_THREADS

#endif
