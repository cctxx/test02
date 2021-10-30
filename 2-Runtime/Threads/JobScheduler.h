#ifndef JOB_SCHEDULER_H
#define JOB_SCHEDULER_H

#include "SimpleLock.h"
#include "Semaphore.h"
#include "Runtime/Modules/ExportModules.h"

// On single-CPU platforms, this should never be used
#define ENABLE_JOB_SCHEDULER (ENABLE_MULTITHREADED_CODE || ENABLE_MULTITHREADED_SKINNING)

#if ENABLE_JOB_SCHEDULER

#define ENABLE_JOB_SCHEDULER_PROFILER (!UNITY_EXTERNAL_TOOL && ENABLE_PROFILER)


struct JobGroup;
struct JobInfo;
class Thread;

class EXPORT_COREMODULE JobScheduler
{
public:
	// Usage and restrictions:
	// Jobs in the same group must be submitted from the same thread
	// It's fine to submit jobs in separate groups from different threads
	// It's also fine to WaitForGroup() on a another thread than BeginGroup()
	//
	// Submitting jobs is lockless and wakes up idle worker threads if needed.
	// Worker threads are lockless when they keep consuming from the same queue.
	// Changing queues or going idle requires a lock. This is to keep track of
	// how many workers try to consume a queue when the group gets recycled.
	// Hopefully this is none, but being lockless we don't really know if anyone
	// got stuck during the size check on the queue and before reading the next
	// element atomically. The size check can safely use old values except when
	// the group is recycled. In that case we need to flush all worker threads
	// from accessing the queue and doing an invalid comparison.

	typedef void* (*JobFunction)(void*);
	typedef void* volatile ReturnCode;
	typedef int JobGroupID;
	
	JobScheduler( int numthreads, int maxGroups, int startProcessor = -1 );
	~JobScheduler();

	JobGroupID BeginGroup( int maxJobs ); 
	bool	IsGroupFinished( JobGroupID group );
	void	WaitForGroup( JobGroupID group );

	bool	SubmitJob( JobGroupID group, JobFunction func, void *data, ReturnCode *returnCode );
	int		GetThreadCount () const { return m_ThreadCount; }

	#if ENABLE_JOB_SCHEDULER_PROFILER
	void EndProfilerFrame (UInt32 frameIDAndValidFlag);
	#endif

private:
	JobGroupID BeginGroupInternal( int maxJobs, bool isBlocking ); 
	JobInfo* FetchNextJob( int& threadActiveGroup );
	JobInfo* FetchJobInGroup( int group );
	void ProcessJob( JobInfo& job, int group );
	void AwakeIdleWorkerThreads( int count );

	#if ENABLE_JOB_SCHEDULER_PROFILER
	struct WorkerProfilerInfo
	{
		int endFrameID;
		int pad[15]; // pad so individual threads don't hammer same cache line
	};
	static void HandleProfilerFrames (WorkerProfilerInfo* info, bool* insideFrame);
	#endif

	static void* WorkLoop( void* data );

private:
	JobGroup*	m_Groups;
	int		m_GroupCount;
	int		m_ThreadCount;
	Thread*	m_Threads;
	volatile bool m_Quit;
	SimpleLock m_Lock;
	Semaphore m_AwakeSemaphore;
	volatile int m_ThreadsIdle;
	volatile int m_PriorityGroup;

	#if ENABLE_JOB_SCHEDULER_PROFILER
	WorkerProfilerInfo* m_ProfInfo;
	int m_WorkerThreadCounter;
	#endif
};

#if !UNITY_EXTERNAL_TOOL
void CreateJobScheduler();
void DestroyJobScheduler();
EXPORT_COREMODULE JobScheduler& GetJobScheduler();
#endif

#endif // ENABLE_MULTITHREADED_CODE


#endif
