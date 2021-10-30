#ifndef FLOATCONVERSION_H
#define FLOATCONVERSION_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <math.h>

#if !UNITY_EXTERNAL_TOOL
#include "Runtime/Utilities/LogAssert.h"
#endif

#if defined(SN_TARGET_PS3)
#	include <ppu_intrinsics.h>
#elif defined(__GNUC__) && defined(__ppc__)
#	include <ppc_intrinsics.h>
#endif

#ifndef kPI
	#define kPI 3.14159265358979323846264338327950288419716939937510F
#endif

const float kBiggestFloatSmallerThanOne = 0.99999994f;
const double kBiggestDoubleSmallerThanOne = 0.99999999999999989;

#if defined(_XBOX)
#define __FSELF __fself
#elif defined(SN_TARGET_PS3)
#define __FSELF __fsels
#endif

inline float FloatMin(float a, float b)
{
#if defined(_XBOX) || defined(SN_TARGET_PS3)
	return __FSELF((a)-(b), b, a);
#else
	return std::min(a, b);
#endif
}

inline float FloatMax(float a, float b)
{
#if defined(_XBOX) || defined(SN_TARGET_PS3)
	return __FSELF((a)-(b), a, b);
#else
	return std::max(a, b);
#endif
}

inline float Abs (float v)
{
#if defined(__ppc__) && (defined(__MWERKS__) || defined(SN_TARGET_PS3))
	return __fabsf(v);
#elif defined(_XBOX)
	return __fabs(v);
#else
	return v < 0.0F ? -v : v;
#endif
}

inline double Abs (double v)
{
	return v < 0.0 ? -v : v;	
}

inline int Abs (int v)
{
	return v < 0 ? -v : v;
}

// Floor, ceil and round functions.
//
// When changing or implementing these functions, make sure the tests in MathTest.cpp
// still pass.
//
// Floor: rounds to the largest integer smaller than or equal to the input parameter.
// Ceil: rounds to the smallest integer larger than or equal to the input parameter.
// Round: rounds to the nearest integer. Ties (0.5) are rounded up to the smallest integer
// larger than or equal to the input parameter.
// Chop/truncate: use a normal integer cast.
//
// Windows:
// Casts are as fast as a straight fistp on an SSE equipped CPU. This is by far the most common
// scenario and will result in the best code for most users. fistp will use the rounding mode set
// in the control register (round to nearest by default), and needs fiddling to work properly.
// This actually makes code that attempt to use fistp slower than a cast.
// Unless we want round to nearest, in which case fistp should be the best choice, right? But
// it is not. The default rounding mode is round to nearest, but in case of a tie (0.5), round to 
// nearest even is used. Thus 0.5 is rounded down to 0, 1.5 is rounded up to 2.
// Conclusion - fistp is useless without stupid fiddling around that actually makes is slower than
// an SSE cast.
//
// OS X Intel:
// Needs investigating
//
// OS X PowerPC:
// Needs investigating
//
// Xbox 360:
// Needs investigating
//
// PS3:
// Needs investigating
//
// iPhone:
// Needs investigating
//
// Android:
// Needs investigating


inline int FloorfToInt (float f)
{
	DebugAssertIf (f < INT_MIN || f > INT_MAX);
	return f >= 0 ? (int)f : (int)(f - kBiggestFloatSmallerThanOne);
}

inline UInt32 FloorfToIntPos (float f)
{
	DebugAssertIf (f < 0 || f > UINT_MAX);
	return (UInt32)f;
}

inline float Floorf (float f)
{
	// Use std::floor().
	// We are interested in reliable functions that do not lose precision.
	// Casting to int and back to float would not be helpful.
	return floor (f);
}

inline double Floord (double f)
{
	// Use std::floor().
	// We are interested in reliable functions that do not lose precision.
	// Casting to int and back to float would not be helpful.
	return floor (f);
}


inline int CeilfToInt (float f)
{
	DebugAssertIf (f < INT_MIN || f > INT_MAX);
	return f >= 0 ? (int)(f + kBiggestFloatSmallerThanOne) : (int)(f);
}

inline UInt32 CeilfToIntPos (float f)
{
	DebugAssertIf (f < 0 || f > UINT_MAX);
	return (UInt32)(f + kBiggestFloatSmallerThanOne);
}

inline float Ceilf (float f)
{
	// Use std::ceil().
	// We are interested in reliable functions that do not lose precision.
	// Casting to int and back to float would not be helpful.
	return ceil (f);
}

inline double Ceild (double f)
{
	// Use std::ceil().
	// We are interested in reliable functions that do not lose precision.
	// Casting to int and back to float would not be helpful.
	return ceil (f);
}


inline int RoundfToInt (float f)
{
	return FloorfToInt (f + 0.5F);
}

inline UInt32 RoundfToIntPos (float f)
{
	return FloorfToIntPos (f + 0.5F);
}

inline float Roundf (float f)
{
	return Floorf (f + 0.5F);
}

inline double Roundf (double f)
{
	return Floord (f + 0.5);
}


///  Fast conversion of float [0...1] to 0 ... 65535
inline int NormalizedToWord (float f)
{
	f = FloatMax (f, 0.0F);
	f = FloatMin (f, 1.0F);
	return RoundfToIntPos (f * 65535.0f);
}

///  Fast conversion of float [0...1] to 0 ... 65535
inline float WordToNormalized (int p)
{
	AssertIf(p < 0 || p > 65535);
	return (float)p / 65535.0F;
}

///  Fast conversion of float [0...1] to 0 ... 255
inline int NormalizedToByte (float f)
{
	f = FloatMax (f, 0.0F);
	f = FloatMin (f, 1.0F);
	return RoundfToIntPos (f * 255.0f);
}

///  Fast conversion of float [0...1] to 0 ... 255
inline float ByteToNormalized (int p)
{
	AssertIf(p < 0 || p > 255);
	return (float)p / 255.0F;
}


// Returns float remainder for t / length
inline float Repeat (float t, float length)
{
	return t - Floorf (t / length) * length;
}

// Returns double remainder for t / length
inline double RepeatD (double t, double length)
{
	return t - floor (t / length) * length;
}

// Returns relative angle on the interval (-pi, pi]
inline float DeltaAngleRad (float current, float target)
{
	float delta = Repeat ((target - current), 2 * kPI);
	if (delta > kPI)
		delta -= 2 * kPI;
	return delta;
}

// Returns true if the distance between f0 and f1 is smaller than epsilon
inline bool CompareApproximately (float f0, float f1, float epsilon = 0.000001F)
{
	float dist = (f0 - f1);
	dist = Abs (dist);
	return dist < epsilon;
}

/// CopySignf () returns x with its sign changed to y's.
inline float CopySignf (float x, float y)
{
	union
	{
		float f;
		UInt32 i;
	} u, u0, u1;
	u0.f = x; u1.f = y;
	UInt32 a    = u0.i;
	UInt32 b    = u1.i;
	SInt32 mask = 1 << 31;
	UInt32 sign = b & mask;
	a &= ~mask;
	a |= sign;

	u.i = a;
	return u.f;
}

inline int CompareFloatRobustSignUtility (float A)
{
    // The sign bit of a number is the high bit.
	union
	{
		float f;
		int i;
	} u;
	u.f = A;
    return (u.i) & 0x80000000;
}

inline bool CompareFloatRobust (float f0, float f1, int maxUlps = 10)
{
    // After adjusting floats so their representations are lexicographically
    // ordered as twos-complement integers a very small positive number
    // will compare as 'close' to a very small negative number. If this is
    // not desireable, and if you are on a platform that supports
    // subnormals (which is the only place the problem can show up) then
    // you need this check.
    // The check for A == B is because zero and negative zero have different
    // signs but are equal to each other.
    if (CompareFloatRobustSignUtility(f0) != CompareFloatRobustSignUtility(f1))
        return f0 == f1;

	union
	{
		float f;
		int i;
	} u0, u1;
	u0.f = f0;
	u1.f = f1;
    int aInt = u0.i;
    // Make aInt lexicographically ordered as a twos-complement int
    if (aInt < 0)
        aInt = 0x80000000 - aInt;
    // Make bInt lexicographically ordered as a twos-complement int
    int bInt = u1.i;
    if (bInt < 0)
        bInt = 0x80000000 - bInt;

    // Now we can compare aInt and bInt to find out how far apart A and B
    // are.
    int intDiff = Abs (aInt - bInt);
    if (intDiff <= maxUlps)
        return true;
    return false;
}

// Returns the t^2
template<class T>
T Sqr (const T& t)
{
	return t * t;
}

#define kDeg2Rad (2.0F * kPI / 360.0F)
#define kRad2Deg (1.F / kDeg2Rad)

inline float Deg2Rad (float deg)
{
	// TODO : should be deg * kDeg2Rad, but can't be changed, 
	// because it changes the order of operations and that affects a replay in some RegressionTests
	return deg / 360.0F * 2.0F * kPI;
}

inline float Rad2Deg (float rad)
{
	// TODO : should be rad * kRad2Deg, but can't be changed, 
	// because it changes the order of operations and that affects a replay in some RegressionTests
	return rad / 2.0F / kPI * 360.0F;
}

inline float Lerp (float from, float to, float t)
{
	return to * t + from * (1.0F - t);
}

inline bool IsNAN (float value)
{
	#if defined __APPLE_CC__
		return value != value;
	#elif _MSC_VER
		return _isnan(value) != 0;
	#else
		return isnan (value);
	#endif
}

inline bool IsNAN (double value)
{
	#if defined __APPLE_CC__
		return value != value;
	#elif _MSC_VER
		return _isnan(value) != 0;
	#else
		return isnan (value);
	#endif
}

inline bool IsPlusInf(float value)		{ return value == std::numeric_limits<float>::infinity (); }
inline bool IsMinusInf(float value)		{ return value == -std::numeric_limits<float>::infinity ();	}

inline bool IsFinite(const float& value)
{
	// Returns false if value is NaN or +/- infinity
	UInt32 intval = *reinterpret_cast<const UInt32*>(&value);
	return (intval & 0x7f800000) != 0x7f800000;
}

inline bool IsFinite(const double& value)
{
	// Returns false if value is NaN or +/- infinity
	UInt64 intval = *reinterpret_cast<const UInt64*>(&value);
	return (intval & 0x7ff0000000000000LL) != 0x7ff0000000000000LL;
}

inline float InvSqrt (float p) { return 1.0F / sqrt (p); }
inline float Sqrt (float p) { return sqrt (p); }

/// - Almost highest precision sqrt
/// - Returns 0 if value is 0 or -1
/// inline float FastSqrt (float value)

/// - Almost highest precision inv sqrt
/// - if value == 0 or -0 it returns 0.
/// inline float FastInvSqrt (float value)

/// - Low precision inv sqrt approximately
/// - if value == 0 or -0 it returns nan or undefined
/// inline float FastestInvSqrt (float value)

#if defined(__ppc__) || defined(SN_TARGET_PS3)

#if UNITY_WII
// Copied from <CodeWarrior>\PowerPC_EABI_Support\MSL\MSL_C\PPC_EABI\Include\math_ppc_inlines.h
// Requires hardware floating to be enabled
// P.S I've also profiled with function below which uses fabs(x) == 0.0F, it's two times slower than this one
inline float FastSqrt (float x)
{
	static const double _half=.5f;
	static const double _three=3.0f;

	if(x > 0.0f)
	{
		double xd = (double)x;
		double guess = __frsqrte(xd);		  		 	/* returns an approximation to	*/
		guess = _half*guess*(_three - guess*guess*xd);	/* now have 12 sig bits 		*/
		guess = _half*guess*(_three - guess*guess*xd);	/* now have 24 sig bits			*/
		return (float)(xd * guess);
	}
	else if (x < 0.0)
		return NAN;
	else
		return x;
}
#else
/// - Accurate to 1 bit precision
/// - returns zero if x is zero
inline float FastSqrt (float x)
{
    const float half = 0.5;
    const float one = 1.0;
    float B, y0, y1;

	// This'll NaN if it hits frsqrte. Handle both +0.0 and -0.0
    if (fabs(x) == 0.0F)
      return x;

    B = x;
    
#if defined(__GNUC__) && !defined(SN_TARGET_PS3)
    y0 = __frsqrtes(B);
#else
    y0 = __frsqrte(B);
#endif
    // First refinement step
    
    y1 = y0 + half*y0*(one - B*y0*y0);
    
    // Second refinement step -- copy the output of the last step to the input of this step
    
    y0 = y1;
    y1 = y0 + half*y0*(one - B*y0*y0);
    
    // Get sqrt(x) from x * 1/sqrt(x)
    return x * y1;
}
#endif

/// - Accurate to 1 bit precision
/// - returns zero if f is zero
inline float FastInvSqrt( float f ) 
{
	float result;
	float estimate, estimate2;
	float oneHalf = 0.5f;
	float one = oneHalf + oneHalf;
	//Calculate a 5 bit starting estimate for the reciprocal sqrt
#if defined(__GNUC__) && !defined(SN_TARGET_PS3)
    estimate = estimate2 = __frsqrtes ( f );
#else
    estimate = estimate2 = __frsqrte ( f );
#endif

	//if you require less precision, you may reduce the number of loop iterations
	estimate = estimate + oneHalf * estimate * ( one - f * estimate * estimate );
	estimate = estimate + oneHalf * estimate * ( one - f * estimate * estimate );
	
#if defined(__GNUC__) && !defined(SN_TARGET_PS3)
	result = __fsels( -f, estimate2, estimate );
#else
	result = __fsel( -f, estimate2, estimate );
#endif
	return result;
}

/// Fast inverse sqrt function
inline float FastestInvSqrt (float value)
{
	#if defined (__ppc__) && (defined (__MWERKS__) || defined(SN_TARGET_PS3))
	return (float)__frsqrte (value);
	#elif defined (__ppc__)
		return (float)__frsqrtes(value);
	#else
		return 1.0F / sqrtf (value);
	#endif
}

#else

inline float FastSqrt (float value)
{
	return sqrtf (value);
}

inline float FastInvSqrt( float f ) 
{
	// The Newton iteration trick used in FastestInvSqrt is a bit faster on
	// Pentium4 / Windows, but lower precision. Doing two iterations is precise enough,
	// but actually a bit slower.
	if (fabs(f) == 0.0F)
		return f;
	return 1.0F / sqrtf (f);
}

inline float FastestInvSqrt( float f )
{
	union
	{
		float f;
		int i;
	} u;
	float fhalf = 0.5f*f;
	u.f = f;
	int i = u.i;
	i = 0x5f3759df - (i>>1);
	u.i = i;
	f = u.f;
	f = f*(1.5f - fhalf*f*f);
	// f = f*(1.5f - fhalf*f*f); // uncommenting this would be two iterations
	return f;
}

#endif

inline float SqrtImpl (float f)
{
	#if UNITY_WII || UNITY_FLASH
		return FastSqrt (f); 
	#else
		return sqrt (f); 
	#endif
}
inline float Sin (float f)
{
	return sinf (f);
}

inline float Pow (float f, float f2)
{
	return powf (f, f2);
}

inline float Cos (float f)
{
	return cosf (f);
}

inline float Sign (float f)
{
#if defined(_XBOX)
	return __fsel(f, 1.0f, -1.0f);
#else
	if (f < 0.0F)
		return -1.0F;
	else
		return 1.0;
#endif
}

#if UNITY_EDITOR

class FloatToHalfConverter
{
public:
	FloatToHalfConverter();

	void Convert(const float& src, UInt16& dest)
	{
		UInt32 bits = *reinterpret_cast<const UInt32*>(&src);
		UInt8 index = UInt8(bits >> 23);
		UInt32 sign = bits & 0x80000000;
		UInt32 mantissa = bits & 0x007fffff;
		dest = (sign >> 16) | m_ExponentTable[index] | (mantissa >> m_MantissaShift[index]);
	}

private:
	UInt16 m_ExponentTable[256];
	UInt8 m_MantissaShift[256];
};

extern FloatToHalfConverter g_FloatToHalf;

#endif // UNITY_EDITOR

#if UNITY_SUPPORTS_SSE
#include "Runtime/Math/Simd/SimdMath.h"

#define SSE_CONST4(name, val) static const ALIGN16 UInt32 name[4] = { (val), (val), (val), (val) }
#define CONST_M128I(name) *(const __m128i *)&name

static ALIGN16 UInt16 source[] = {0,0,0,0,0,0,0,0};
static ALIGN16 float destination[] = {0.0,0.0,0.0,0.0};

static void HalfToFloat(UInt16 src, float& dest)
{
	SSE_CONST4(mask_nosign, 0x7fff);
	SSE_CONST4(smallest_normal, 0x0400);
	SSE_CONST4(infinity, 0x7c00);
	SSE_CONST4(expadjust_normal, (127 - 15) << 23);
	SSE_CONST4(magic_denorm, 113 << 23);
	
	source[0] = src;
	__m128i in = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source));
	__m128i mnosign = CONST_M128I(mask_nosign);
	__m128i eadjust = CONST_M128I(expadjust_normal);
	__m128i smallest = CONST_M128I(smallest_normal);
	__m128i infty = CONST_M128I(infinity);
	__m128i expmant = _mm_and_si128(mnosign, in);
	__m128i justsign = _mm_xor_si128(in, expmant);
	__m128i b_notinfnan = _mm_cmpgt_epi32(infty, expmant);
	__m128i b_isdenorm = _mm_cmpgt_epi32(smallest, expmant);
	__m128i shifted = _mm_slli_epi32(expmant, 13);
	__m128i adj_infnan = _mm_andnot_si128(b_notinfnan, eadjust);
	__m128i adjusted = _mm_add_epi32(eadjust, shifted);
	__m128i den1 = _mm_add_epi32(shifted, CONST_M128I(magic_denorm));
	__m128i adjusted2 = _mm_add_epi32(adjusted, adj_infnan);
	__m128 den2 = _mm_sub_ps(_mm_castsi128_ps(den1), *(const __m128 *)&magic_denorm);
	__m128 adjusted3 = _mm_and_ps(den2, _mm_castsi128_ps(b_isdenorm));
	__m128 adjusted4 = _mm_andnot_ps(_mm_castsi128_ps(b_isdenorm), _mm_castsi128_ps(adjusted2));
	__m128 adjusted5 = _mm_or_ps(adjusted3, adjusted4);
	__m128i sign = _mm_slli_epi32(justsign, 16);
	__m128 out = _mm_or_ps(adjusted5, _mm_castsi128_ps(sign));
	_mm_storeu_ps(destination, out);
	dest = destination[0];
#undef SSE_CONST4
#undef CONST_M128I
}

#else

static void HalfToFloat(UInt16 src, float& dest)
{
	// Integer alias
	UInt32& bits = *reinterpret_cast<UInt32*>(&dest);

	// Based on Fabian Giesen's public domain half_to_float_fast3
	static const UInt32 magic = { 113 << 23 };
	const float& magicFloat = *reinterpret_cast<const float*>(&magic);
	static const UInt32 shiftedExp = 0x7c00 << 13; // exponent mask after shift

	// Mask out sign bit
	bits = src & 0x7fff;
	if (bits)
	{
		// Move exponent + mantissa to correct bits
		bits <<= 13;
		UInt32 exponent = bits & shiftedExp;
		if (exponent == 0)
		{
			// Handle denormal
			bits += magic;
			dest -= magicFloat;
		}
		else if (exponent == shiftedExp) // Inf/NaN
			bits += (255 - 31) << 23;
		else
			bits += (127 - 15) << 23;
	}

	// Copy sign bit
	bits |= (src & 0x8000) << 16;
}

#endif

using std::cos;
using std::pow;
using std::atan2;
using std::acos;
using std::sin;
using std::sqrt;
using std::log;
using std::exp;

// On non-C99 platforms log2 is not available, so approximate it.
#if UNITY_WIN || UNITY_XENON || UNITY_ANDROID || UNITY_FLASH || UNITY_WEBGL
#define kNaturalLogarithm2 0.693147180559945309417
#define Log2(x) (logf(x) / kNaturalLogarithm2)
#else
#define Log2(x) log2f(x)
#endif


#endif
