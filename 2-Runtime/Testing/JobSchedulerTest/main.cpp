#include <cstdio>
#include <cstdlib>
#include "UnityPrefix.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/JobScheduler.h"
#include <time.h>
#include <math.h>

struct WorkData {
	float input;
	float output;
};

void* WorkFunction( void* data )
{
	WorkData* d = (WorkData*)data;
	d->output = 0.0f;
	for( int i = 0; i < 1000000; ++i ) {
		d->output += sinf(d->output) + cosf(d->input) - sinf(d->output + d->input * 3.0f);
	}

	return NULL;
}

// Windows, Core2Quad 2.40
// 200 jobs, 100000 iters:
// Sum=590573.192871
// 0=1.55s 1=0.80s 2=0.55s 3=0.45s 4=0.45s 5=0.44s 6=0.45s
// 100 jobs, 1000000 iters:
// Sum=2992744.398470
// 0=7.78s 1=3.94s 2=2.66s 3=2.00s 4=2.00s 5=2.00s 6=2.02s

void DoTests()
{
	JobScheduler	scheduler(3,1);

	JobScheduler::JobGroupID group = scheduler.BeginGroup();

	const int kJobs = 100;
	WorkData datas[kJobs];
	for( int i = 0; i < kJobs; ++i )
	{
		datas[i].input = i+1;
		scheduler.SubmitJob( group, WorkFunction, &datas[i], NULL );
	}
	scheduler.WaitForGroup(group);

	float sum = 0.0f;
	for( int i = 0; i < kJobs; ++i )
		sum += datas[i].output;
	printf("Sum of results: %f\n", sum);
}



int main()
{
	#if UNITY_WIN
	DWORD ttt0 = GetTickCount();
	#else
	timeval ttt0;
	gettimeofday( &ttt0, NULL );
	#endif

	DoTests();
	
	#if UNITY_WIN
	DWORD ttt1 = GetTickCount();
	float timeTaken = (ttt1-ttt0) * 0.001f;
	#else
	timeval ttt1;
	gettimeofday( &ttt1, NULL );
	timeval ttt2;
	timersub( &ttt1, &ttt0, &ttt2 );
	float timeTaken = ttt2.tv_sec + ttt2.tv_usec * 1.0e-6f;
	#endif
	
	printf( "Test time: %.2fs\n", timeTaken );

	return 0;
}
