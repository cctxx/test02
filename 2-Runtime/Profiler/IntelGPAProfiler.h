#pragma once

#include "Configuration/UnityConfigure.h"

#define INTEL_GPA_PROFILER_AVAILABLE (ENABLE_PROFILER && UNITY_WIN && !UNITY_EDITOR && !WEBPLUG && !UNITY_64 && !UNITY_WINRT)

#if INTEL_GPA_PROFILER_AVAILABLE
#include "External/IntelGPASDK/include/ittnotify.h"

void InitializeIntelGPAProfiler();
extern __itt_domain* g_IntelGPADomain;

#define INTEL_GPA_INFORMATION_DATA __itt_string_handle* intelGPAData;
#define INTEL_GPA_INFORMATION_INITIALIZE() intelGPAData = __itt_string_handle_create(functionName)
#define INTEL_GPA_SAMPLE_BEGIN(info) __itt_task_begin(g_IntelGPADomain, __itt_null, __itt_null, (__itt_string_handle*) info->intelGPAData)
#define INTEL_GPA_SAMPLE_END() __itt_task_end(g_IntelGPADomain)

#else

#define INTEL_GPA_INFORMATION_DATA
#define INTEL_GPA_INFORMATION_INITIALIZE()
#define INTEL_GPA_SAMPLE_BEGIN(info)
#define INTEL_GPA_SAMPLE_END()

#endif
