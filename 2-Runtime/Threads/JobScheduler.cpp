#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "JobScheduler.h"

#if ENABLE_JOB_SCHEDULER

#include "Thread.h"
#include "ThreadUtility.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Mutex.h"
#include "Semaphore.h"
#include "Event.h"
#if !UNITY_EXTERNAL_TOOL
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Profiler/Profiler.h"
#endif
#include "AtomicOps.h"

// ------------------------------------------------------------------------

struct JobInfo
{
	JobScheduler::JobFunction func;
	void* userData;
	JobScheduler::ReturnCode* returnCode;
};

// ------------------------------------------------------------------------

struct JobGroup
{
	enum
	{
		kSizeIncrement = 256,
		kTaskCountUnused = -1
	};
	JobGroup() : taskCount(kTaskCountUnused), activeThreads(0), nextJob(0), jobsAdded(0) {}

	volatile int taskCount;
	volatile int activeThreads;
	volatile int nextJob;
	volatile int jobsAdded;
	dynamic_array<JobInfo> jobQueue;
	Semaphore doneSemaphore;
};

// ------------------------------------------------------------------------

JobInfo* JobScheduler::FetchNextJob( int& activeGroup )
{
	if( activeGroup == m_PriorityGroup )
	{
		JobInfo* job = FetchJobInGroup(activeGroup);
		if( job )
			return job;
	}

	// We need a lock to change groups!
	SimpleLock::AutoLock lock(m_Lock);
	if( activeGroup != -1 )
		m_Groups[activeGroup].activeThreads--;
	int group = m_PriorityGroup;
	for( int i = 0; i < m_GroupCount; i++ )
	{
		JobInfo* job = FetchJobInGroup(group);
		if( job )
		{
			m_Groups[group].activeThreads++;
			// Set priority group to one that is actually available
			// Less work for other threads to find a good group
			m_PriorityGroup = group;
			activeGroup = group;
			return job;
		}
		if( ++group >= m_GroupCount )
			group = 0;
	}
	activeGroup = -1;
	return NULL;
}

JobInfo* JobScheduler::FetchJobInGroup( int group )
{
	JobGroup& jg = m_Groups[group];
	int curJob = jg.nextJob;
	while( curJob < jg.jobsAdded )
	{
		if( AtomicCompareExchange(&jg.nextJob, curJob + 1, curJob) )
			return &jg.jobQueue[curJob];

		curJob = jg.nextJob;
	}
	return NULL;
}

void JobScheduler::ProcessJob( JobInfo& job, int group )
{
	DebugAssert( job.func );
	void* ret = job.func(job.userData);
	if( job.returnCode )
	{
		UnityMemoryBarrier();
		*job.returnCode = ret;
	}

	// Signal if we are the last to finish
	JobGroup& jg = m_Groups[group];
	if( AtomicDecrement(&jg.taskCount) == 0 )
		jg.doneSemaphore.Signal();
}


void JobScheduler::AwakeIdleWorkerThreads( int count )
{
	for( int i = 0; i < count && m_ThreadsIdle > 0; i++ )
	{
		if( AtomicDecrement(&m_ThreadsIdle) < 0 )
		{
			AtomicIncrement(&m_ThreadsIdle);
			break;
		}
		m_AwakeSemaphore.Signal();
	}
}

void* JobScheduler::WorkLoop( void* data )
{
	int activeGroup = -1;
	JobScheduler* js = (JobScheduler*)data;

	#if ENABLE_JOB_SCHEDULER_PROFILER
	profiler_initialize_thread ("Worker Thread", true);
	int workThreadIndex = AtomicIncrement (&js->m_WorkerThreadCounter);
	WorkerProfilerInfo* profInfo = &js->m_ProfInfo[workThreadIndex];
	bool insideFrame = false;
	#endif
	
	while( !js->m_Quit )
	{
		#if ENABLE_JOB_SCHEDULER_PROFILER
		HandleProfilerFrames (profInfo, &insideFrame);
		#endif

		JobInfo* job = js->FetchNextJob(activeGroup);
		if( job )
		{
			js->ProcessJob(*job, activeGroup);
		}
		else
		{
			AtomicIncrement(&js->m_ThreadsIdle);
			js->m_AwakeSemaphore.WaitForSignal();
		}
	}
	
	#if ENABLE_JOB_SCHEDULER_PROFILER
	if (insideFrame)
	{
		//@TODO: Is this actually necessary? (It seems like cleanup thread should take care of killing it all?)
		profiler_set_active_seperate_thread(false);
		profiler_end_frame_seperate_thread(0);
	}
	profiler_cleanup_thread();
	#endif
	
	return NULL;
}


#if ENABLE_JOB_SCHEDULER_PROFILER
void JobScheduler::HandleProfilerFrames (WorkerProfilerInfo* profInfo, bool* insideFrame)
{
	// Don't do fancy synchnonization here; all we need is for worker
	// threads to do begin/end profiler frame once in a while.
	// Worst case, we'll get a missing profiler info for a frame.
	int endFrameID = profInfo->endFrameID;
	if (endFrameID != -1)
	{
		if (*insideFrame)
		{
			profiler_set_active_seperate_thread (false);
			profiler_end_frame_seperate_thread (endFrameID);
			*insideFrame = false;
		}
		profiler_begin_frame_seperate_thread (kProfilerGame);
		profiler_set_active_seperate_thread (true);
		*insideFrame = true;

		profInfo->endFrameID = -1;
		UnityMemoryBarrier();
	}
}
#endif



JobScheduler::JobScheduler( int numthreads, int maxGroups, int startProcessor )
:	m_ThreadCount( numthreads )
,	m_GroupCount(maxGroups)
,	m_Quit(false)
,	m_ThreadsIdle(0)
,	m_PriorityGroup(0)
{
	m_Groups = new JobGroup[maxGroups];
	if( m_ThreadCount > 0 )
	{
		#if ENABLE_JOB_SCHEDULER_PROFILER
		m_WorkerThreadCounter = -1;
		m_ProfInfo = new WorkerProfilerInfo[numthreads];
		memset (m_ProfInfo, -1, sizeof(m_ProfInfo[0])*numthreads);
		#endif

		m_Threads = new Thread[numthreads];

		for( int i = 0; i < numthreads; ++i )
		{
			int processor = DEFAULT_UNITY_THREAD_PROCESSOR;
			if( startProcessor >= 0 )
			{
				processor = startProcessor + i;
			}
			m_Threads[i].SetName ("UnityWorker");
			m_Threads[i].Run( WorkLoop, this, DEFAULT_UNITY_THREAD_STACK_SIZE, processor );
		}
	}
	else
	{
		m_Threads = NULL;
		#if ENABLE_JOB_SCHEDULER_PROFILER
		m_WorkerThreadCounter = -1;
		m_ProfInfo = NULL;
		#endif
	}
}

JobScheduler::~JobScheduler()
{
	if( m_ThreadCount > 0 )
	{
		m_Quit = true;
		UnityMemoryBarrier();
		// wait while threads exit
		// first signal as many times as there are threads, then wait for each to exit
		for( int i = 0; i < m_ThreadCount; ++i )
			m_AwakeSemaphore.Signal();
		for( int i = 0; i < m_ThreadCount; ++i )
			m_Threads[i].WaitForExit();
		delete[] m_Threads;

		#if ENABLE_JOB_SCHEDULER_PROFILER
		delete[] m_ProfInfo;
		#endif
	}

	delete[] m_Groups;
}

bool JobScheduler::SubmitJob( JobGroupID group, JobFunction func, void *data, ReturnCode *returnCode )
{
	AssertIf( func == NULL );
	if( m_ThreadCount <= 0 )
	{
		// not multi-threaded: execute job right now
		void* z = func( data );
		if( returnCode )
			*returnCode = z;
		return true;
	}

	if( group >= m_GroupCount || group < 0)
	{
		ErrorString( "Invalid job group ID" );
		return false;
	}

	JobGroup& jg = m_Groups[group];
	AtomicIncrement(&jg.taskCount);
	int jobIndex = jg.jobsAdded;
	JobInfo& job = jg.jobQueue[jobIndex];
	job.func = func;
	job.userData = data;
	job.returnCode = returnCode;
	int nextIndex = AtomicIncrement(&jg.jobsAdded);
	// This may fail if you add jobs from multiple threads to the same group
	Assert(nextIndex == jobIndex + 1);
	AwakeIdleWorkerThreads(nextIndex - jg.nextJob);
	return true;
}

JobScheduler::JobGroupID JobScheduler::BeginGroup( int maxJobs )
{
	// See if we can allocate a group without blocking.
	for( int isBlocking = 0; isBlocking < 2; ++isBlocking )
	{
		// If a group still has active threads we can't use it immediately after WaitForGroup().
		// By blocking we guarantee to find a group, as long as we stay within maxGroups.
		JobGroupID id = BeginGroupInternal(maxJobs, isBlocking != 0);
		if( id != -1 )
			return id;
	}
	ErrorString("JobScheduler: too many job groups");
	return -1;
}

JobScheduler::JobGroupID JobScheduler::BeginGroupInternal( int maxJobs, bool isBlocking )
{
	// Find unused group. We need a lock for that.
	m_Lock.Lock();
	for( int i = 0; i < m_GroupCount; ++i )
	{
		JobGroup& group = m_Groups[i];
		if( group.taskCount == JobGroup::kTaskCountUnused 
			&& ( isBlocking || group.activeThreads == 0 ) )
		{
			// We consider finishing group a pending task
			// Keeps job group alive until everything is done
			group.taskCount = 1;

			// Spin while worker threads are using our group
			// Do this *after* we've marked it used (case 492417)
			while( group.activeThreads != 0 )
			{
				m_Lock.Unlock();
				m_Lock.Lock();
			}
			group.jobsAdded = 0;
			group.nextJob = 0;
			const int rounding = JobGroup::kSizeIncrement;
			int roundedSize = (maxJobs + rounding - 1) / rounding * rounding;
			group.jobQueue.reserve(roundedSize);
			group.jobQueue.resize_uninitialized(maxJobs);
			m_Lock.Unlock();
			return i;
		}
	}
	m_Lock.Unlock();
	return -1;
}

bool JobScheduler::IsGroupFinished( JobGroupID group )
{
	const JobGroup& jg = m_Groups[group];
	// Last reference is kept until WaitForGroup()
	return jg.taskCount == 1;
}

void JobScheduler::WaitForGroup( JobGroupID group )
{
	if( group >= m_GroupCount )
	{
		ErrorString( "Invalid job group ID" );
		return;
	}

	JobGroup& jg = m_Groups[group];

	// Release our reference to job group
	// Pending jobs (if any) also have refs
	if( AtomicDecrement(&jg.taskCount) != 0 )
	{
		// Set our group as having priority so other threads fetch from it
		m_PriorityGroup = group;

		for( ;; )
		{
			JobInfo* job = FetchJobInGroup(group);
			if( !job )
				break;
			ProcessJob(*job, group);
		}

		jg.doneSemaphore.WaitForSignal();		
		Assert(jg.nextJob == jg.jobsAdded);
		Assert(jg.taskCount == 0);
	}

	// Set count to kTaskCountUnused (-1)
	AtomicDecrement(&jg.taskCount);
}


#if ENABLE_JOB_SCHEDULER_PROFILER
void JobScheduler::EndProfilerFrame (UInt32 frameIDAndValidFlag)
{
	// Don't do fancy synchnonization here; all we need is for worker
	// threads to do begin/end profiler frame once in a while.
	// Worst case, we'll get a missing profiler info for a frame.
	for (int i = 0; i < m_ThreadCount; ++i)
	{
		m_ProfInfo[i].endFrameID = frameIDAndValidFlag;
	}
	UnityMemoryBarrier();
}
#endif


// ----------------------------------------------------------------------

#if !UNITY_EXTERNAL_TOOL

static JobScheduler* g_Scheduler = NULL;

void CreateJobScheduler()
{
	AssertIf( g_Scheduler );

	const int kMaxJobGroups = 16; // increase if we ever run out of concurrent separate job groups
#if UNITY_XENON
	// Use threads 1 (core 0) and 2/3 (core 1)
	int startProcessor = 1;
	int workerThreads = 3;
#elif UNITY_PS3
	int startProcessor = 1;
	int workerThreads = 1;
#elif UNITY_OSX
	int startProcessor = 1;
	int workerThreads = systeminfo::GetNumberOfCores() - 1;
	Thread::SetCurrentThreadProcessor(0);
#else
	int startProcessor = -1;
	int workerThreads = systeminfo::GetProcessorCount() - 1;
#endif

	// Don't use an unreasonable amount of threads on future hardware.
	// Mono GC has a 256 thread limit for the process (case 443576).
	if (workerThreads > 128)
		workerThreads = 128;
	
	g_Scheduler = new JobScheduler( workerThreads, kMaxJobGroups, startProcessor );
}

void DestroyJobScheduler()
{
	delete g_Scheduler;
	g_Scheduler = NULL;
}

JobScheduler& GetJobScheduler()
{
	AssertIf( !g_Scheduler );
	return *g_Scheduler;
}

#endif


#endif
