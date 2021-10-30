#pragma once

/***
* defs.h - definitions/declarations for some commonly standard declaration
*
*
* Purpose:
*       This file defines the following ma keywords:
*	RESTRICT			
*	MECANIM_FORCE_INLINE	
*   STATIC_INLINE
*	ATTRIBUTE_ALIGN(a)	
*	EXPLICIT_TYPENAME
*	EXPLICIT_TEMPLATE
*	DLL_IMPORT			
*	DLL_EXPORT
*   DECLARE_C
*
****/

#if defined(__INTEL_COMPILER) || defined(__ICL)
	#include <cstddef>
	#define RESTRICT				__restrict
	#define MECANIM_FORCE_INLINE	__forceinline
	#define ATTRIBUTE_ALIGN(a)		__declspec(align(a))
	#define EXPLICIT_TEMPLATE		template
	#define DLL_IMPORT				__declspec(dllimport)
	#define DLL_EXPORT				__declspec(dllexport)
	#define ALIGN4F					16
	#define DECLARE_C				__cdecl

#elif defined(_MSC_VER) 
	#include <cstddef>
	#define RESTRICT				__restrict
	#define MECANIM_FORCE_INLINE	__forceinline
	#define ATTRIBUTE_ALIGN(a)		__declspec(align(a))
	#define EXPLICIT_TEMPLATE		template
	#define DLL_IMPORT				__declspec(dllimport)
	#define DLL_EXPORT				__declspec(dllexport)
	#define ALIGN4F					16
	#define DECLARE_C				__cdecl

	#pragma warning( disable : 4996)

#elif defined(__GNUC__)
	#include <cstddef>
	#if ((__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1)) || (__GNUC__ >= 4)
        #ifdef _DEBUG
            #ifndef MECANIM_FORCE_INLINE
                #define MECANIM_FORCE_INLINE		inline 
            #endif  
		    #define STATIC_INLINE		inline 
        #else
            #ifndef MECANIM_FORCE_INLINE
                #define MECANIM_FORCE_INLINE		inline __attribute__((always_inline))
            #endif
            #define STATIC_INLINE		inline __attribute__((always_inline))
        #endif
	#else
		#define STATIC_INLINE			extern inline
	#endif

	#define ATTRIBUTE_ALIGN(a)			__attribute__ ((aligned(a)))
	#define ALIGN4F						16

	#if ((__GNUC__ >= 3) && (__GNUC_MINOR__ >= 4)) || (__GNUC__ >= 4)
		#define EXPLICIT_TEMPLATE	template
	#endif	
#endif

#if defined(__GNUC__) && ((__GNUC__ <= 4) && (__GNUC_MINOR__ <= 2))
 #define TEMPLATE_SPEC(L, R) template<L,R>
#else
 #define TEMPLATE_SPEC(L, R) template<>
#endif

#ifndef RESTRICT
	#define RESTRICT
#endif

#ifndef MECANIM_FORCE_INLINE	
	#define MECANIM_FORCE_INLINE		inline
#endif

#ifndef STATIC_INLINE
	#define	STATIC_INLINE		static inline
#endif

#ifndef ATTRIBUTE_ALIGN
	#define ATTRIBUTE_ALIGN(a)
#endif

#ifndef EXPLICIT_TYPENAME
	#define EXPLICIT_TYPENAME	typename
#endif

#ifndef EXPLICIT_TEMPLATE
	#define EXPLICIT_TEMPLATE
#endif

#ifndef DLL_IMPORT
	#define DLL_IMPORT
#endif

#ifndef DLL_EXPORT
	#define DLL_EXPORT
#endif

#ifndef ALIGN4F
	#define ALIGN4F						16
#endif

#ifndef DECLARE_C
	#define DECLARE_C
#endif

