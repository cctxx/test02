/*
 *  AudioConversion.h
 *  audio-utils
 *
 *  Created by Søren Christiansen on 2/9/11.
 *  Copyright 2011 Unity Technologies. All rights reserved.
 *
 */
#pragma once

#include <algorithm>
#include <functional>
#include <assert.h>
#include "Runtime/Audio/correct_fmod_includer.h"
#include "Runtime/Utilities/dynamic_array.h"

using std::unary_function;
using std::transform;

typedef union {
	UInt8 int8[3];
} SInt24;

template<typename InType, typename OutType >
struct Reformat : public unary_function<InType, OutType> {
	float operator()(const InType x) { assert("NO REFORMAT BETWEEN THESE TYPES"); return OutType();	 }
};

// --> normalized float
template<typename InType >
struct Reformat<InType, float> : public unary_function<InType, float> {
	float operator()(const InType x) { return ( (float) x ) /  ( 1 << ( ( sizeof(InType) * 8) - 1 ) ) ; }
};

template< >
struct Reformat<SInt16, float> : public unary_function<SInt16, float> {
	float operator()(const SInt16 x) { return ( ((float)x) / ( 1 << 15)) ; }
};

template< >
struct Reformat<float, float> : public unary_function<float, float> {
	float operator()(const float x) { return x; }
};

template< >
struct Reformat<SInt24, float> : public unary_function<SInt24, float> {
	float operator()(const SInt24 x) 
	{
		int val;
		val  = ((unsigned int)x.int8[0] <<  8);
		val |= ((unsigned int)x.int8[1] << 16);
		val |= ((unsigned int)x.int8[2] << 24);
		val >>= 8;
		return (float)(val) * (1.0f / (float)(1<<23));
	}
};
	
// --> 16 bit
template< >
struct Reformat<UInt8, SInt16> {
	SInt16 operator()(const UInt8 x) { return (( ( UInt16 ) x << 8 )); }
};

template< >
struct Reformat<SInt24, SInt16> {
	SInt16 operator()(const SInt24 x) { 
		SInt16 out  = ( x.int8[2] << 8 );
		out |= ( x.int8[1] );
		return out; 
	}
};

template<typename T>
struct Reformat<float, T> {
	T operator()(const float x) { return (T)(x * ( 1 << ( ( sizeof(T) * 8) - 1 ))); }
};

template<>
struct Reformat<float, SInt16> {
	SInt16 operator()(const float x) { return SInt16(x * (1 << 15)); }
};

template<typename T>
struct NOP : public unary_function<T, T> {
	T operator()(const T x) { return x; }
};

// --> helper functors
template <typename FO1, typename FO2>
class Composer : public unary_function<typename FO1::argument_type, typename FO2::result_type> {
private:
	typedef typename FO2::result_type	FO2Result;
	typedef typename FO1::argument_type	FO1Arg;
    
public:
	FO1 fo1;  // first/inner function object to call
    FO2 fo2;  // second/outer function object to call
	
    // constructor: initialize function objects
    Composer (FO1 f1, FO2 f2)
	: fo1(f1), fo2(f2) {
    }
	
    // ''function call'': nested call of function objects
	FO2Result operator() (const FO1Arg v) {
        return fo2(fo1(v));
    }
};

template <typename FO1, typename FO2>
inline
Composer<FO1,FO2> compose (FO1 f1, FO2 f2) {
    return Composer<FO1,FO2> (f1, f2);
}

template<typename _InputIterator, typename _Function>
_Function
for_each_channel(_InputIterator __first, _InputIterator __last, _Function __f, int channel, int channels)
{
	for ( __first = __first + channel; __first < __last - channel; __first = __first+channels)
		__f(*__first);
	return __f;
}

// --> helper functions interface
typedef Reformat<SInt8, float> SInt8ToFloat;
typedef Reformat<SInt16, float> SInt16ToFloat;
typedef Reformat<SInt24, float> SInt24ToFloat;
typedef Reformat<SInt32, float> SInt32ToFloat;
typedef Reformat<float, float> FloatToFloat;
typedef Reformat<float, SInt8> FloatToSInt8;
typedef Reformat<float, SInt16> FloatToSInt16;
typedef Reformat<float, SInt24> FloatToSInt24;
typedef Reformat<float, SInt32> FloatToSInt32;

template<typename _Function>
inline void ArrayToNormFloat(FMOD_SOUND_FORMAT inFormat, const void* beginIterator, const void* endIterator, dynamic_array<float>& v, _Function __f)
{
	if (inFormat == FMOD_SOUND_FORMAT_PCM8)
		std::transform((const SInt8*) beginIterator, (const SInt8*) endIterator, std::back_inserter(v), compose( SInt8ToFloat(),  __f ));
	else if (inFormat == FMOD_SOUND_FORMAT_PCM16)
		std::transform((const SInt16*) beginIterator, (const SInt16*) endIterator, std::back_inserter(v), compose ( SInt16ToFloat(), __f));
	else if (inFormat == FMOD_SOUND_FORMAT_PCM24)
		std::transform((const SInt24*) beginIterator, (const SInt24*) endIterator, std::back_inserter(v), compose ( SInt24ToFloat(), __f));
	else if (inFormat == FMOD_SOUND_FORMAT_PCM32)
		std::transform((const SInt32*) beginIterator, (const SInt32*) endIterator, std::back_inserter(v), compose ( SInt32ToFloat(), __f));
	else if (inFormat == FMOD_SOUND_FORMAT_PCMFLOAT)
		std::transform((const float*) beginIterator, (const float*) endIterator, std::back_inserter(v), compose ( FloatToFloat(), __f));
}

inline void ArrayToNormFloat(FMOD_SOUND_FORMAT inFormat, const void* beginIterator, const void* endIterator, dynamic_array<float>& v)
{
	if (inFormat == FMOD_SOUND_FORMAT_PCM8)
		std::transform((const SInt8*) beginIterator, (const SInt8*) endIterator, std::back_inserter(v), SInt8ToFloat());
	else if (inFormat == FMOD_SOUND_FORMAT_PCM16)
		std::transform((const SInt16*) beginIterator, (const SInt16*) endIterator, std::back_inserter(v), SInt16ToFloat());
	else if (inFormat == FMOD_SOUND_FORMAT_PCM24)
		std::transform((const SInt24*) beginIterator, (const SInt24*) endIterator, std::back_inserter(v), SInt24ToFloat());
	else if (inFormat == FMOD_SOUND_FORMAT_PCM32)
		std::transform((const SInt32*) beginIterator, (const SInt32*) endIterator, std::back_inserter(v), SInt32ToFloat());
	else if (inFormat == FMOD_SOUND_FORMAT_PCMFLOAT)
		std::transform((const float*) beginIterator, (const float*) endIterator, std::back_inserter(v), FloatToFloat());
}

template<typename T>
inline void ArrayFromNormFloat(FMOD_SOUND_FORMAT outFormat, const float* beginIterator, const float* endIterator, dynamic_array<T>& v)
{
	if (outFormat == FMOD_SOUND_FORMAT_PCM8)
		std::transform(beginIterator,  endIterator, std::back_inserter(v), FloatToSInt8());
	else if (outFormat == FMOD_SOUND_FORMAT_PCM16)
		std::transform( beginIterator,  endIterator, std::back_inserter(v), FloatToSInt16());
	//else if (outFormat == FMOD_SOUND_FORMAT_PCM24)
	//	std::transform( beginIterator,  endIterator, std::back_inserter(v), FloatToSInt24());
	else if (outFormat == FMOD_SOUND_FORMAT_PCM32)
		std::transform( beginIterator,  endIterator, std::back_inserter(v), FloatToSInt32());
	else if (outFormat == FMOD_SOUND_FORMAT_PCMFLOAT)
		std::transform( beginIterator,  endIterator, std::back_inserter(v), FloatToFloat());
}

inline void ArrayFromNormFloat(FMOD_SOUND_FORMAT outFormat, const float* beginIterator, const float* endIterator, void* outbuffer)
{
	if (outFormat == FMOD_SOUND_FORMAT_PCM8)
	{	
		SInt8* dstPtr = (SInt8*)outbuffer;
		while (beginIterator != endIterator)
		{
			*dstPtr = FloatToSInt8()(*beginIterator);  
			beginIterator++;
			dstPtr++;
		}
	}
	else
	if (outFormat == FMOD_SOUND_FORMAT_PCM16)
	{	
		SInt16* dstPtr = (SInt16*)outbuffer;
		while (beginIterator != endIterator)
		{
			*dstPtr = FloatToSInt16()(*beginIterator);  
			beginIterator++;
			dstPtr++;
		} 
	}
	else
	if (outFormat == FMOD_SOUND_FORMAT_PCM32)
	{	
		SInt32* dstPtr = (SInt32*)outbuffer;
		while (beginIterator != endIterator)
		{
			*dstPtr = FloatToSInt32()(*beginIterator);  
			beginIterator++;
			dstPtr++;
		} 
	}
	else
	if (outFormat == FMOD_SOUND_FORMAT_PCMFLOAT)
	{	
		memcpy (outbuffer, beginIterator, (endIterator - beginIterator) * sizeof(float));
	}
	else {
		Assert("Conversion NOT supported");
	}

}


inline void ArrayToNormFloat(FMOD_SOUND_FORMAT inFormat, const void* beginIterator, const void* endIterator, float* outbuffer)
{
	if (inFormat == FMOD_SOUND_FORMAT_PCM8)
	{	
		SInt8* srcPtr = (SInt8*)beginIterator;		
		while (srcPtr != (SInt8*)endIterator)
		{
			*outbuffer = SInt8ToFloat()(*srcPtr);  
			srcPtr++;
			outbuffer++;
		}
	}
	else
	if (inFormat == FMOD_SOUND_FORMAT_PCM16)
	{	
		SInt16* srcPtr = (SInt16*)beginIterator;		
		while (srcPtr != (SInt16*)endIterator)
		{
			*outbuffer = SInt16ToFloat()(*srcPtr);  
			srcPtr++;
			outbuffer++;
		}
	}
	else
	if (inFormat == FMOD_SOUND_FORMAT_PCM24)
	{	
		SInt24* srcPtr = (SInt24*)beginIterator;		
		while (srcPtr != (SInt24*)endIterator)
		{
			*outbuffer = SInt24ToFloat()(*srcPtr);  
			srcPtr++;
			outbuffer++;
		}
	}
	else
	if (inFormat == FMOD_SOUND_FORMAT_PCM32)
	{	
		SInt32* srcPtr = (SInt32*)beginIterator;		
		while (srcPtr != (SInt32*)endIterator)
		{
			*outbuffer = SInt32ToFloat()(*srcPtr);  
			srcPtr++;
			outbuffer++;
		}
	}
	else
	if (inFormat == FMOD_SOUND_FORMAT_PCMFLOAT)
	{	
		memcpy (outbuffer, beginIterator, ((float*)endIterator - (float*)beginIterator) * sizeof(float));
	}
	else {
		ErrorString("Conversion from this format NOT supported");
	}
}



template<typename InType, typename OutType>
inline void ReformatArray(const InType* inArray, const unsigned size, OutType* outArray)
{
	Reformat<InType, OutType> reformater;
	std::transform( inArray, inArray + size, outArray, reformater );	
}



