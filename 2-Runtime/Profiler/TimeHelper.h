#ifndef _TIMEHELPER_H_
#define _TIMEHELPER_H_

// The profiler interprets this value as nanoseconds
typedef UInt64 ProfileTimeFormat;
#define kInvalidProfileTime (~ProfileTimeFormat(0))

#if UNITY_IPHONE || UNITY_OSX

	extern "C" { uint64_t mach_absolute_time(void); }
		
	#define ABSOLUTE_TIME UInt64
	#define ABSOLUTE_TIME_INIT(VAR) VAR = 0u;

	#define START_TIME mach_absolute_time()
	#define ELAPSED_TIME(VAR) (START_TIME - VAR)
	#define COMBINED_TIME(VAR1, VAR2) (VAR1 + VAR2)
	#define SUBTRACTED_TIME(VAR1, VAR2) (VAR1 - VAR2)


	ABSOLUTE_TIME SubtractAbsoluteTimeClamp(ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs);
	ABSOLUTE_TIME DivideAbsoluteTime(ABSOLUTE_TIME elapsedTime, int divisor);
	inline bool IsEqualAbsoluteTime (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { return rhs == lhs; }
	inline bool IsSmallerAbsoluteTime (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { return rhs > lhs; }

	ProfileTimeFormat GetProfileTime(ABSOLUTE_TIME elapsedTime);
	ABSOLUTE_TIME ProfileTimeToAbsoluteTime(ProfileTimeFormat elapsedTime);

#elif UNITY_ANDROID || UNITY_PEPPER || UNITY_LINUX || UNITY_FLASH || UNITY_WEBGL || UNITY_BB10 || UNITY_TIZEN

	#include <sys/time.h>
	
	#define ABSOLUTE_TIME UInt64
	#define ABSOLUTE_TIME_INIT(VAR) VAR = 0ull;

	inline const ABSOLUTE_TIME _StartTime() { timeval time; gettimeofday(&time, 0); return time.tv_usec * 1000ULL + time.tv_sec * 1000000000ULL; }
	#define START_TIME _StartTime()
	#define ELAPSED_TIME(VAR) (START_TIME - VAR)
	#define COMBINED_TIME(VAR1, VAR2) (VAR1 + VAR2)
	#define SUBTRACTED_TIME(VAR1, VAR2) (VAR1 - VAR2)

	ABSOLUTE_TIME SubtractAbsoluteTimeClamp(ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs);
	ABSOLUTE_TIME DivideAbsoluteTime(ABSOLUTE_TIME elapsedTime, int divisor);
	inline bool IsEqualAbsoluteTime (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { return rhs == lhs; }
	inline bool IsSmallerAbsoluteTime (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { return rhs > lhs; }

	ProfileTimeFormat GetProfileTime(ABSOLUTE_TIME elapsedTime);
	ABSOLUTE_TIME ProfileTimeToAbsoluteTime(ProfileTimeFormat elapsedTime);

#elif UNITY_WIN || UNITY_XENON 
	#define ABSOLUTE_TIME UInt64
	#define ABSOLUTE_TIME_INIT(VAR) VAR = 0u;

	#define START_TIME GetStartTime()
	#define ELAPSED_TIME(VAR) GetElapsedTime(VAR)
	#define COMBINED_TIME(VAR1, VAR2) VAR1 + VAR2
	#define SUBTRACTED_TIME(VAR1, VAR2) VAR1 - VAR2
	
	inline ABSOLUTE_TIME SubtractAbsoluteTimeClamp(ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { if (lhs > rhs) return lhs - rhs; else return 0; }
	ABSOLUTE_TIME DivideAbsoluteTime(ABSOLUTE_TIME elapsedTime, int divisor);
	inline bool IsEqualAbsoluteTime (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { return lhs == rhs; }
	inline bool IsSmallerAbsoluteTime (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { return lhs < rhs; }
	
	ABSOLUTE_TIME GetStartTime();
	ABSOLUTE_TIME GetElapsedTime(ABSOLUTE_TIME startTime);
	ProfileTimeFormat GetProfileTime(ABSOLUTE_TIME elapsedTime);
	ABSOLUTE_TIME ProfileTimeToAbsoluteTime(ProfileTimeFormat elapsedTime);


#elif UNITY_PS3

	#include <sys/time_util.h>

	#define ABSOLUTE_TIME UInt64
	#define ABSOLUTE_TIME_INIT(VAR) VAR = 0u;

	inline const ABSOLUTE_TIME _StartTime() { ABSOLUTE_TIME tb; SYS_TIMEBASE_GET(tb); return tb; }
	#define START_TIME _StartTime()
	#define ELAPSED_TIME(VAR) START_TIME - VAR
	#define COMBINED_TIME(VAR1, VAR2) VAR1 + VAR2
	#define SUBTRACTED_TIME(VAR1, VAR2) VAR1 - VAR2
	ProfileTimeFormat GetProfileTime(ABSOLUTE_TIME elapsedTime);

	inline ABSOLUTE_TIME SubtractAbsoluteTimeClamp(ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { if (lhs > rhs) return lhs - rhs; else return 0; }
	ABSOLUTE_TIME DivideAbsoluteTime(ABSOLUTE_TIME elapsedTime, int divisor);
	inline bool IsEqualAbsoluteTime (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { return lhs == rhs; }
	inline bool IsSmallerAbsoluteTime (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { return lhs < rhs; }
#elif UNITY_WII
	#include <revolution/os.h>
	#define ABSOLUTE_TIME OSTime
	#define ABSOLUTE_TIME_INIT(VAR) VAR = 0u;

	#define START_TIME OSGetTime()
	#define ELAPSED_TIME(VAR) GetElapsedTime(VAR)
	#define COMBINED_TIME(VAR1, VAR2) VAR1 + VAR2
	#define SUBTRACTED_TIME(VAR1, VAR2) VAR1 - VAR2
	
	inline ABSOLUTE_TIME SubtractAbsoluteTimeClamp(ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { if (lhs > rhs) return lhs - rhs; else return 0; }
	ABSOLUTE_TIME DivideAbsoluteTime(ABSOLUTE_TIME elapsedTime, int divisor);
	inline bool IsEqualAbsoluteTime (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { return lhs == rhs; }
	inline bool IsSmallerAbsoluteTime (ABSOLUTE_TIME lhs, ABSOLUTE_TIME rhs) { return lhs < rhs; }
	
	ABSOLUTE_TIME GetStartTime();
	ABSOLUTE_TIME GetElapsedTime(ABSOLUTE_TIME startTime);
	ProfileTimeFormat GetProfileTime(ABSOLUTE_TIME elapsedTime);
	ABSOLUTE_TIME ProfileTimeToAbsoluteTime(ProfileTimeFormat elapsedTime);

#else
	#error IMPLEMENT ME
#endif

inline float ProfileTimeToSeconds (ProfileTimeFormat elapsedTime) { return elapsedTime * 0.000000001; }
inline float AbsoluteTimeToSeconds (ABSOLUTE_TIME absoluteTime) { return ProfileTimeToSeconds(GetProfileTime(absoluteTime)); }
inline float GetElapsedTimeInSeconds (ABSOLUTE_TIME elapsedTime) { return ProfileTimeToSeconds(GetProfileTime(ELAPSED_TIME(elapsedTime))); }

inline float AbsoluteTimeToMilliseconds (ABSOLUTE_TIME time)
{
	return AbsoluteTimeToSeconds(time) * 1000.0F;
}

#endif /*_TIMEHELPER_H_*/
