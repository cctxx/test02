#include "UnityPrefix.h"
#include "IntelGPAProfiler.h"

#if INTEL_GPA_PROFILER_AVAILABLE

__itt_domain* g_IntelGPADomain;

void InitializeIntelGPAProfiler()
{
	g_IntelGPADomain = __itt_domain_createA("Unity.Player");
}


#endif
