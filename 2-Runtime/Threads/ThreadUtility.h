#ifndef __THREAD_UTILITY_H
#define __THREAD_UTILITY_H

#if UNITY_OSX
#ifndef __ppc__
#include <libkern/OSAtomic.h>
#endif
#endif

#if UNITY_IPHONE
#include <libkern/OSAtomic.h>
#endif

#if UNITY_PEPPER
inline int NoBarrier_AtomicExchange(volatile int* ptr,
                                         int new_value) {
  __asm__ __volatile__("xchgl %1,%0"  // The lock prefix is implicit for xchg.
                       : "=r" (new_value)
                       : "m" (*ptr), "0" (new_value)
                       : "memory");
  return new_value;  // Now it's the previous value.
}
#endif

// Memory barrier.
//
// Necessary to call this when using volatile to order writes/reads in multiple threads.
inline void UnityMemoryBarrier()
{
	#if UNITY_WIN || UNITY_XENON
	#ifdef MemoryBarrier
	MemoryBarrier();
	#else
	long temp;
	__asm xchg temp,eax;
	#endif
	
	#elif UNITY_OSX
	
	OSMemoryBarrier();
	
	#elif UNITY_PS3

	__lwsync();

	#elif UNITY_IPHONE
	// No need for memory barriers on iPhone and Android - single CPU
	OSMemoryBarrier();
	
	#elif UNITY_PEPPER

	#if defined(__x86_64__)

	// 64-bit implementations of memory barrier can be simpler, because it
	// "mfence" is guaranteed to exist.
	__asm__ __volatile__("mfence" : : : "memory");

	#else

/*	if (AtomicOps_Internalx86CPUFeatures.has_sse2) {
		__asm__ __volatile__("mfence" : : : "memory");
	} else { // mfence is faster but not present on PIII*/
		int x = 0;
		NoBarrier_AtomicExchange(&x, 0);  // acts as a barrier on PIII
//	}

	#endif	
	
	#elif UNITY_ANDROID

	#elif UNITY_WII

	#elif UNITY_TIZEN

		__sync_synchronize();

	#elif UNITY_LINUX || UNITY_BB10
		#ifdef __arm__
			__asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 5" : : "r" (0) : "memory");
		#else
			__sync_synchronize();
		#endif
	#elif UNITY_FLASH || UNITY_WEBGL
		// Flash has no threads
	#else
	
	#error "Unknown platform, implement memory barrier if at all possible"
	#endif
}

#endif
