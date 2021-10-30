#include "UnityPrefix.h"
#include "TimeHelper.h"

#if UNITY_IPHONE || UNITY_OSX

#include <mach/mach_time.h>

static bool     g_ProfileTimeInited = false;
static UInt64	g_Numer;
static UInt64	g_Denom;

static void InitTime()
{
	if( !g_ProfileTimeInited )
	{
		mach_timebase_info_data_t timeInfo;
		mach_timebase_info(&timeInfo);
		
		g_Numer = timeInfo.numer;
		g_Denom = timeInfo.denom;
		
		g_ProfileTimeInited = true;
	}
}

ProfileTimeFormat GetProfileTime(UInt64 elapsedTime)
{
	InitTime();
	return ( (elapsedTime*g_Numer) / g_Denom );
}

ABSOLUTE_TIME ProfileTimeToAbsoluteTime(ProfileTimeFormat elapsedTime)
{
	InitTime();
	return ( (elapsedTime*g_Denom) / g_Numer );
}

ABSOLUTE_TIME DivideAbsoluteTime(ABSOLUTE_TIME elapsedTime, int divisor)
{
	return ProfileTimeToAbsoluteTime(GetProfileTime(elapsedTime) / (UInt64)divisor);
}


ABSOLUTE_TIME SubtractAbsoluteTimeClamp (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs)
{
	return (lhs < rhs) ? (ABSOLUTE_TIME)0 : lhs-rhs;
}

#elif UNITY_WIN | UNITY_XENON

// Timer granularity of QuertPerformanceCounters is that of frequency.
// Usually that is around 2M - resulting in a granularity of 500ns.
// This is not optimal for performance profiling

ABSOLUTE_TIME GetStartTime()
{
	LARGE_INTEGER start;
	
	QueryPerformanceCounter(&start);

	return (UInt64) start.QuadPart;
}

ABSOLUTE_TIME GetElapsedTime(ABSOLUTE_TIME startTime)
{
	LARGE_INTEGER end;

	QueryPerformanceCounter(&end);
	
	return (UInt64) end.QuadPart - startTime;
}

static bool s_HaveFrequency = false;
static LARGE_INTEGER s_Frequency;

ProfileTimeFormat GetProfileTime(ABSOLUTE_TIME elapsedTime)
{
	if (!s_HaveFrequency)
	{
		QueryPerformanceFrequency (&s_Frequency);
		s_HaveFrequency = true;
	}
	
	return (elapsedTime * 1000000000LL) / s_Frequency.QuadPart;
}

ABSOLUTE_TIME ProfileTimeToAbsoluteTime(ProfileTimeFormat elapsedTime)
{
	if (!s_HaveFrequency)
	{
		QueryPerformanceFrequency (&s_Frequency);
		s_HaveFrequency = true;
	}
	
	return (elapsedTime * s_Frequency.QuadPart) / 1000000000LL;
}

ABSOLUTE_TIME DivideAbsoluteTime(ABSOLUTE_TIME elapsedTime, int divisor)
{
	return elapsedTime / (UInt64)divisor;
}

#elif UNITY_PS3

#include <sys/sys_time.h>

inline ProfileTimeFormat timebase_frequency()
{
	static ProfileTimeFormat tf = sys_time_get_timebase_frequency();
	return tf;
}

ProfileTimeFormat GetProfileTime(ABSOLUTE_TIME elapsedTime)
{
	return (elapsedTime * 1000000000LL) / timebase_frequency() ;
}

ABSOLUTE_TIME ProfileTimeToAbsoluteTime(ProfileTimeFormat elapsedTime)
{
	return (elapsedTime * timebase_frequency()) / 1000000000LL;
}

ABSOLUTE_TIME DivideAbsoluteTime(ABSOLUTE_TIME elapsedTime, int divisor)
{
	return elapsedTime / (UInt64)divisor;
}

#elif UNITY_ANDROID || UNITY_PEPPER || UNITY_LINUX || UNITY_FLASH || UNITY_WEBGL || UNITY_BB10 || UNITY_TIZEN

ProfileTimeFormat GetProfileTime(ABSOLUTE_TIME elapsedTime)
{
	return elapsedTime;
}

ABSOLUTE_TIME ProfileTimeToAbsoluteTime(ProfileTimeFormat elapsedTime)
{
	return elapsedTime;
}

ABSOLUTE_TIME DivideAbsoluteTime(ABSOLUTE_TIME elapsedTime, int divisor)
{
	return elapsedTime / (UInt64)divisor;
}

ABSOLUTE_TIME SubtractAbsoluteTimeClamp(ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs)
{
	if (IsSmallerAbsoluteTime(lhs, rhs))
	{
		ABSOLUTE_TIME zero;
		ABSOLUTE_TIME_INIT(zero);
		return zero;
	}
	else
	{
		return lhs - rhs;
	}
}

#elif UNITY_WII

ABSOLUTE_TIME GetStartTime()
{
	// ToDo
	return 0;
}
ABSOLUTE_TIME GetElapsedTime(ABSOLUTE_TIME startTime)
{
	// ToDo:
	return 0;
}
ProfileTimeFormat GetProfileTime(ABSOLUTE_TIME elapsedTime)
{
	return OSTicksToNanoseconds (elapsedTime);
}

ABSOLUTE_TIME ProfileTimeToAbsoluteTime(ProfileTimeFormat elapsedTime)
{
	return OSNanosecondsToTicks (elapsedTime);
}

ABSOLUTE_TIME DivideAbsoluteTime(ABSOLUTE_TIME elapsedTime, int divisor)
{
	return elapsedTime / (UInt64)divisor;
}

#else

#error IMPLEMENT ME

#endif