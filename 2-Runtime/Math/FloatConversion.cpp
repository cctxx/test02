#include "UnityPrefix.h"
#include "FloatConversion.h"

#if UNITY_EDITOR

#include "External/UnitTest++/src/UnitTest++.h"

FloatToHalfConverter::FloatToHalfConverter()
{
	for (int i = 0; i < 256; i++)
	{
		int e = i - 127;
		if (e < -24)
		{
			// Too small to represent becomes zero
			m_ExponentTable[i] = 0x0000;
			m_MantissaShift[i] = 24;
		}
		else if (e < -14)
		{
			// Small numbers become denormals
			m_ExponentTable[i] = 0x0400 >> (-14 - e);
			m_MantissaShift[i] = -1 - e;
		}
		else if (e < 16)
		{
			// Handle normalized numbers
			m_ExponentTable[i] = (15 + e) << 10;
			m_MantissaShift[i] = 13;
		}
		else if (e < 128)
		{
			// Large numbers become infinity
			m_ExponentTable[i] = 0x7C00;
			m_MantissaShift[i] = 24;
		}
		else
		{
			// Handle infinity and NaN
			m_ExponentTable[i] = 0x7C00;
			m_MantissaShift[i] = 13;
		}
	}
}

FloatToHalfConverter g_FloatToHalf;

SUITE (FloatConversionTests)
{
TEST(FloatConversionTests_FloatToHalf)
{
	// 1 bit sign
	for (int s = 0; s <= 1; s++)
	{
		// 5 bits exponent
		for (int ebits = 0; ebits < (1 << 5); ebits++)
		{
			// 10 bits mantissa
			for (int m = 0; m < (1 << 10); m++)
			{
				int orig = (s << 15) | (ebits << 10) | m;
				float val;
				HalfToFloat(orig, val);
				UInt16 conv;
				g_FloatToHalf.Convert(val, conv);
				CHECK_EQUAL(orig, conv);
			}
		}
	}
}

TEST(FloatConversionTests_IsFinite)
{
	float infF = std::numeric_limits<float>::infinity();
	CHECK(IsFinite(0.0f));
	CHECK(IsFinite(1.0f));
	CHECK(IsFinite(FLT_MIN));
	CHECK(IsFinite(FLT_MAX));
	CHECK(IsFinite(-FLT_MIN));
	CHECK(IsFinite(-FLT_MAX));
	CHECK(!IsFinite(infF));
	CHECK(!IsFinite(-infF));
	CHECK(!IsFinite(infF-infF));

	double infD = std::numeric_limits<double>::infinity();
	CHECK(IsFinite(0.0));
	CHECK(IsFinite(1.0));
	CHECK(IsFinite(DBL_MIN));
	CHECK(IsFinite(DBL_MAX));
	CHECK(IsFinite(-DBL_MIN));
	CHECK(IsFinite(-DBL_MAX));
	CHECK(!IsFinite(infD));
	CHECK(!IsFinite(-infD));
	CHECK(!IsFinite(infD-infD));
}
}

#endif // UNITY_EDITOR
