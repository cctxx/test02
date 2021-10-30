#ifndef _COLLECT_PROFILER_STATS_H_
#define _COLLECT_PROFILER_STATS_H_

#include "Configuration/UnityConfigure.h"

struct AllProfilerStats;
struct MemoryStats;

#if ENABLE_PROFILER

void CollectProfilerStats (AllProfilerStats& stats);
ProfilerString GetMiniMemoryOverview();

#endif

unsigned GetUsedHeapSize();

#endif