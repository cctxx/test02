#include "Runtime/Profiler/DeprecatedFrameStatsProfiler.h"
#include "Runtime/Profiler/TimeHelper.h"

#if UNITY_OSX || UNITY_IPHONE
#include <mach/mach_time.h>
#endif

FrameStats::Timestamp GetTimestamp()
{
#if UNITY_OSX || UNITY_IPHONE
	return mach_absolute_time();
#elif UNITY_ANDROID
	return START_TIME;
#else
	return (signed long long)(START_TIME);//GetTimeSinceStartup ()*1000000000.0);
#endif
}
static FrameStats gFrameStats;

FrameStats const& GetFrameStats()
{
	return gFrameStats;
}