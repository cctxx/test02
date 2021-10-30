#ifndef _GPUPROFILER_H_
#define _GPUPROFILER_H_

#if ENABLE_PROFILER

struct ProfilerSample;
#include "ProfilerImpl.h"
#include "Runtime/Utilities/dynamic_array.h"

class GPUProfiler
{
public:
	static void BeginFrame();
	static void EndFrame();

	static void GPUTimeSample ( );

	static bool CollectGPUTime ( dynamic_array<ProfilerData::GPUTime>& gpuSamples, bool wait );
	static int ComputeGPUTime ( dynamic_array<ProfilerData::GPUTime>& gpuSamples);
	static void ClearTimerQueries ( dynamic_array<ProfilerData::GPUTime>& gpuSamples );
};

#endif
#endif
