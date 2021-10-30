#ifndef CPUINFO_H
#define CPUINFO_H

#define CPUID_FEATURES_SSE  (1 << 25) 
#define CPUID_FEATURES_SSE2 (1 << 26)

#if defined(_MSC_VER) && !UNITY_XENON 

// define __cpuid intrinsic
#include <intrin.h>

#elif defined(__GNUC__) && UNITY_OSX && UNITY_SUPPORTS_SSE

// This has to be implemented in a PIC friendly way or osx web players will not compile
// http://sam.zoy.org/blog/2007-04-13-shlib-with-non-pic-code-have-inlin

#define __cpuid(array, func) \
{ \
	__asm__ __volatile__("pushl %%ebx      \n\t" /* save %ebx */ \
						 "cpuid            \n\t" \
						 "movl %%ebx, %1   \n\t" /* save what cpuid just put in %ebx */ \
						 "popl %%ebx       \n\t" /* restore the old %ebx */ \
						 : "=a"(array[0]), "=r"(array[1]), "=c"(array[2]), "=d"(array[3]) \
						 : "a"(func) \
						 : "cc");\
} 

#else
#define __cpuid(a, b) 
#endif


class CPUInfo
{
public:
	CPUInfo();	

	inline unsigned int GetCPUIDFeatures() {return m_CPUIDFeatures;}

#ifdef UNITY_SUPPORTS_SSE
#	if UNITY_AUTO_DETECT_VECTOR_UNIT
		static __forceinline bool HasSSESupport()
		{
			AssertMsg(m_Initialized, "CPUInfo has not been initialized");
			return m_IsSSESupported;
		}

		static __forceinline bool HasSSE2Support()
		{
			AssertMsg(m_Initialized, "CPUInfo has not been initialized");
			return m_IsSSE2Supported;
		}

#	else
	static inline bool HasSSESupport()
	{
		return true;
	}

	static inline bool HasSSE2Support()
	{
		return true;
	}
#	endif
#endif

	static inline bool HasSSE3Support() { return m_IsSSE3Supported; }
	static inline bool HasSupplementalSSE3Support() { return m_IsSupplementalSSE3Supported; }
	static inline bool HasSSE41Support() { return m_IsSSE41Supported; }
	static inline bool HasSSE42Support() { return m_IsSSE42Supported; }
	static inline bool HasAVXSupport() { return m_IsAVXSupported; }
	static inline bool HasAVX2Support() { return m_IsAVX2Supported; }
	static inline bool HasAVX512Support() { return m_IsAVX512Supported; }
	static inline bool HasFP16CSupport() { return m_IsFP16CSupported; }
	static inline bool HasFMASupport() { return m_IsFMASupported; }

#if UNITY_SUPPORTS_NEON
	#if UNITY_ANDROID
		static inline bool HasNEONSupport()
		{
			AssertMsg(m_Initialized, "CPUInfo has not been initialized");
			return m_IsNEONSupported;
		}
	#else
		static inline bool HasNEONSupport()
		{
			return true;
		}
	#endif
#else
	static inline bool HasNEONSupport()
	{
		return false;
	}
#endif

private:
	static bool m_Initialized;
	
	static bool m_IsSSESupported;
	static bool m_IsSSE2Supported;
	static bool m_IsSSE3Supported;
	static bool m_IsSupplementalSSE3Supported;
	static bool m_IsSSE41Supported; // SSE 4.1
	static bool m_IsSSE42Supported; // SSE 4.2
	static bool m_IsAVXSupported;
	static bool m_IsAVX2Supported;
	static bool m_IsAVX512Supported;
	static bool m_IsFP16CSupported; // FP16 conversions supported
	static bool m_IsFMASupported; // fused multiply-add instructions
	
	static bool m_IsNEONSupported;
	
	static unsigned int m_CPUIDFeatures;

};
#endif // CPUINFO_H
