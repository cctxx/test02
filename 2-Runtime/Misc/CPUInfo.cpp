#include "UnityPrefix.h"
#include "CPUInfo.h"
#if UNITY_ANDROID
	#include <cpu-features.h>
#endif

bool CPUInfo::m_Initialized = false;
bool CPUInfo::m_IsSSESupported = false;
bool CPUInfo::m_IsSSE2Supported = false;
bool CPUInfo::m_IsSSE3Supported = false;
bool CPUInfo::m_IsSupplementalSSE3Supported = false;
bool CPUInfo::m_IsSSE41Supported = false;
bool CPUInfo::m_IsSSE42Supported = false;
bool CPUInfo::m_IsAVXSupported = false;
bool CPUInfo::m_IsAVX2Supported = false;
bool CPUInfo::m_IsAVX512Supported = false;
bool CPUInfo::m_IsFP16CSupported = false;
bool CPUInfo::m_IsFMASupported = false;
bool CPUInfo::m_IsNEONSupported = false;

unsigned int CPUInfo::m_CPUIDFeatures = 0;


#if UNITY_SUPPORTS_SSE

static inline UInt64 xgetbv_impl()
{
#	if defined(__GNUC__) || defined(__clang__)
	UInt32 eax, edx;

	__asm __volatile (
	".byte 0x0f, 0x01, 0xd0" // xgetbv instruction isn't supported by some older assemblers, so just emit it raw
		: "=a"(eax), "=d"(edx) : "c"(0)
	);

	return ((UInt64)edx << 32) | eax;
#	elif defined(_MSC_VER) && !UNITY_XENON
	return 0;//_xgetbv(_XCR_XFEATURE_ENABLED_MASK);
#	else
	return 0;
#	endif
}

static void cpuid_ex_impl(UInt32 eax, UInt32 ecx, UInt32* abcd)
{
	#if defined(_MSC_VER)

		__cpuidex((int*)abcd, eax, ecx);

	#else
		UInt32 ebx, edx;
		#if defined(__i386__) && defined (__PIC__)
			// for PIC under 32 bit: EBX can't be modified
			__asm__ ("movl %%ebx, %%edi \n\t cpuid \n\t xchgl %%ebx, %%edi" : "=D" (ebx), "+a" (eax), "+c" (ecx), "=d" (edx));
		#else
			__asm__ ("cpuid" : "+b" (ebx), "+a" (eax), "+c" (ecx), "=d" (edx));
		#endif
		abcd[0] = eax; abcd[1] = ebx; abcd[2] = ecx; abcd[3] = edx;
	#endif
}

#endif // UNITY_SUPPORTS_SSE


CPUInfo::CPUInfo()
{
#if UNITY_SUPPORTS_SSE
	int data[4] = {0};

	// Add more code here to extract vendor string or what ever is needed
	__cpuid (data, 0);
	unsigned int cpuData0 = data[0];
	unsigned int cpuInfo2 = 0;
	if (cpuData0 >= 1) {
		__cpuid(data, 1);
		cpuInfo2 = data[2];
		m_CPUIDFeatures = data[3];
	}

	// SSE/2 support
	m_IsSSESupported = (m_CPUIDFeatures & CPUID_FEATURES_SSE) != 0;
	m_IsSSE2Supported = (m_CPUIDFeatures & CPUID_FEATURES_SSE2) != 0;
	
	// SSE 3.x
	m_IsSSE3Supported = ((cpuInfo2 & (1<<0)) != 0);
	m_IsSupplementalSSE3Supported = ((cpuInfo2 & (1<<9)) != 0);

	// SSE 4.x support
	m_IsSSE41Supported = ((cpuInfo2 & (1<<19)) != 0);
	m_IsSSE42Supported = ((cpuInfo2 & (1<<20)) != 0);

	// AVX support
	m_IsAVXSupported =
		((cpuInfo2 & (1<<28)) != 0) && // AVX support in CPU
		((cpuInfo2 & (1<<27)) != 0) && // OS support for AVX (XSAVE/XRESTORE on context switches)
		((xgetbv_impl() & 6) == 6); // XMM & YMM registers will be preserved on context switches
	
	if (m_IsAVXSupported)
	{
		if (cpuData0 >= 7)
		{
			UInt32 regs7[4] = {0};
			cpuid_ex_impl(0x7, 0, regs7);
			m_IsAVX2Supported = ((regs7[1] & (1<<5)) != 0);
			m_IsAVX512Supported = ((regs7[1] & (1<<16)) != 0);
		}
	}

	m_IsFP16CSupported = ((cpuInfo2 & (1<<29)) != 0);
	m_IsFMASupported = ((cpuInfo2 & (1<<12)) != 0);

#elif UNITY_ANDROID && UNITY_SUPPORTS_NEON
	m_CPUIDFeatures = android_getCpuFeatures();
	m_IsNEONSupported = (m_CPUIDFeatures & ANDROID_CPU_ARM_FEATURE_NEON) != 0;
#endif

	m_Initialized = true;
}

CPUInfo g_cpuInfo;
