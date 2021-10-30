#ifndef PREFETCH_H
#define PREFETCH_H

#if UNITY_PS3
#include <ppu_intrinsics.h>
#endif

// This assembly will not work for Jungle.  TODO: use arm version check here
#if defined(__arm__) && !UNITY_LINUX && !UNITY_WINRT

inline void Prefetch(const void* p)
{
	unsigned char* pCurr = (unsigned char*)p;
	asm volatile(
				 "pld	[%0]	\n\t"
				 : "=r" (pCurr)
				 : "0" (pCurr)
				 : "r0");
}

inline void Prefetch(const void* p, size_t size)
{
	unsigned char* pCurr = (unsigned char*)p;
	unsigned char* pEnd = pCurr + size;

	while (pCurr < pEnd)
	{
		asm volatile(
					 "pld	[%0]	\n\t"
					 : "=r" (pCurr)
					 : "0" (pCurr)
					 : "r0");
		pCurr += 32;
	}
}

#elif defined(_XBOX)
__forceinline void Prefetch(const void* p, size_t size = 32)
{
	__dcbt(0, p);
}
#elif UNITY_PS3
inline void Prefetch(const void* p, size_t size = 32)
{
	__dcbt(p);
}
#else
//@TODO: gcc __builtin_prefetch(p); profile & enable
inline void Prefetch(const void* /*p*/, size_t /*size*/ = 32) {  }
#endif

#endif
