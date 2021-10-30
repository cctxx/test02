#include "UnityPrefix.h"
#if ENABLE_PROFILER

#include "GPUProfiler.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/GfxTimerQuery.h"
#include "ProfilerImpl.h"
#include "ProfilerFrameData.h"

PROFILER_INFORMATION(gBeginQueriesProf, "GPUProfiler.BeginQueries", kProfilerOverhead)
PROFILER_INFORMATION(gEndQueriesProf, "GPUProfiler.EndQueries", kProfilerOverhead)


void GPUProfiler::GPUTimeSample()
{
	// GPU samples should only be added on the main thread.
	DebugAssert (Thread::CurrentThreadIsMainThread ());
	
	UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
	DebugAssert(prof);
	if (!prof->GetIsActive() || !gGraphicsCaps.hasTimerQuery || gGraphicsCaps.buggyTimerQuery)
		return;

	GfxTimerQuery* timer = ProfilerFrameData::AllocTimerQuery();
	timer->Measure();
	ProfilerData::GPUTime sample = {prof->GetActiveSampleIndex(), timer, 0xFFFFFFFF, g_CurrentGPUSection};
	prof->AddGPUSample(sample);
}

void GPUProfiler::BeginFrame()
{	
	PROFILER_AUTO(gBeginQueriesProf, NULL);
	GetGfxDevice().BeginTimerQueries();
}

void GPUProfiler::EndFrame()
{
	GPU_TIMESTAMP();
	PROFILER_AUTO(gEndQueriesProf, NULL);
	GetGfxDevice().EndTimerQueries();
}

bool GPUProfiler::CollectGPUTime( dynamic_array<ProfilerData::GPUTime>& gpuSamples, bool wait )
{
	if(!gGraphicsCaps.hasTimerQuery)
		return false;

	UInt32 flags = wait ? GfxTimerQuery::kWaitAll : GfxTimerQuery::kWaitRenderThread;

	// Gather query times
	for(int i = 0; i < gpuSamples.size(); i++)
	{
		ProfilerData::GPUTime& sample = gpuSamples[i];
		if (sample.timerQuery != NULL)
		{
			ProfileTimeFormat elapsed = sample.timerQuery->GetElapsed(flags);
			sample.gpuTimeInMicroSec = elapsed == kInvalidProfileTime? 0xFFFFFFFF: elapsed/1000;
			if (wait || sample.gpuTimeInMicroSec != 0xFFFFFFFF)
			{
				// Recycle query object
				ProfilerFrameData::ReleaseTimerQuery(sample.timerQuery);
				sample.timerQuery = NULL;
			}
		}
	}
	return true;
}

int GPUProfiler::ComputeGPUTime( dynamic_array<ProfilerData::GPUTime>& gpuSamples )
{
	if (!CollectGPUTime(gpuSamples, true))
		return 0;

	// Why is the first sample invalid?
	if (!gpuSamples.empty())
		gpuSamples[0].gpuTimeInMicroSec = 0;

	int totalTimeMicroSec = 0;
	for(int i = 0; i < gpuSamples.size(); i++)
	{
		totalTimeMicroSec += gpuSamples[i].gpuTimeInMicroSec;
	}	
	return totalTimeMicroSec;
}

void GPUProfiler::ClearTimerQueries ( dynamic_array<ProfilerData::GPUTime>& gpuSamples )
{
	for(int i = 0; i < gpuSamples.size(); i++)
	{
		ProfilerData::GPUTime& sample = gpuSamples[i];
		if (sample.timerQuery != NULL)
		{
			// Recycle query object
			ProfilerFrameData::ReleaseTimerQuery(sample.timerQuery);
			sample.timerQuery = NULL;
		}
	}
}

#endif // ENABLE_PROFILER
