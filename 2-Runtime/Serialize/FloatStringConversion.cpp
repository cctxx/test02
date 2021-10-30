#include "UnityPrefix.h"

#if UNITY_EDITOR
// This are the definitions of std::numeric_limits<>::max_digits10, which we cannot use
// because it is only in the C++11 standard.
const int kMaxFloatDigits = std::floor(std::numeric_limits<float>::digits * 3010.0/10000.0 + 2);
const int kMaxDoubleDigits = std::floor(std::numeric_limits<double>::digits * 3010.0/10000.0 + 2);

#include "External/gdtoa/gdtoa.h"

bool FloatToStringAccurate (float f, char* buffer, size_t maximumSize)
{
	return g_ffmt (buffer, (float*)&f, kMaxFloatDigits, maximumSize) != NULL;
}

bool DoubleToStringAccurate (double f, char* buffer, size_t maximumSize)
{
	return g_dfmt (buffer, (double*)&f, kMaxDoubleDigits, maximumSize) != NULL;
}

bool FloatToStringAccurate (float f, UnityStr& output)
{
	char buf[64];
	if (FloatToStringAccurate (f, buf, 64))
	{
		output = buf;
		return true;
	}
	else
		return false;
}

bool DoubleToStringAccurate (double f, UnityStr& output)
{
	char buf[64];
	if (DoubleToStringAccurate (f, buf, 64))
	{
		output = buf;
		return true;
	}
	else
		return false;
}

float StringToFloatAccurate (const char* buffer)
{
	return strtof (buffer, NULL);
}

double StringToDoubleAccurate (const char* buffer)
{
	return strtod (buffer, NULL);
}

#if ENABLE_UNIT_TESTS
#include "../../External/UnitTest++/src/UnitTest++.h"

SUITE (FloatStringConversionTests)
{
TEST(FloatToStringConversion_AccurateWorks)
{
	// Make sure no locale is used
	CHECK_EQUAL (0.0F, StringToFloatAccurate("0,5"));
	
	UnityStr buf; 

	FloatToStringAccurate(1.0F, buf);
	CHECK_EQUAL ("1", buf);

	CHECK_EQUAL (1.0F, StringToFloatAccurate("1.0"));


	FloatToStringAccurate(1.5F, buf);
	CHECK_EQUAL ("1.5", buf);
	CHECK_EQUAL (1.5F, StringToFloatAccurate("1.5"));
}
}
#endif

#endif
