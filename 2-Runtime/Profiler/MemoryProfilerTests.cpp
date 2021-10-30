#include "UnityPrefix.h"
#include "MemoryProfiler.h"

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

#if ENABLE_PROFILER
#include "Runtime/Profiler/MemoryProfiler.h"

SUITE (MemoryProfilerTests)
{
	struct MemoryProfilerFixture
	{
		MemoryProfilerFixture()
		{
			// string allocates length + 1 rounded up to mod 16 
			stringlength = 32;
#if UNITY_OSX
			// on osx, there is allocated room for refcounting as well
			stringlength += 2;
#endif
		}
		~MemoryProfilerFixture()
		{
		}
		int stringlength;
	};

	struct VectorOfStrings
	{
		UNITY_VECTOR(kMemDefault, UnityStr) vec;
	};

	struct StructWithStrings
	{
		UnityStr str1;
		UnityStr str2;
	};

	TEST_FIXTURE(MemoryProfilerFixture, GetRelatedMemorySize_StringVector_MatchesExpectedSize)
	{
		VectorOfStrings* vec = UNITY_NEW_AS_ROOT(VectorOfStrings, kMemDefault, "", "");
		vec->vec.reserve(10);
		{
			UnityStr str("HelloWorld HelloWorld");
			SET_ALLOC_OWNER(vec);
			vec->vec.push_back(str);
		}
		
		CHECK_EQUAL(GetMemoryProfiler()->GetRelatedMemorySize(vec), sizeof(UnityStr)*10 + sizeof(UNITY_VECTOR(kMemDefault, UnityStr)) + stringlength);
		UNITY_DELETE(vec, kMemDefault);
	}

	TEST_FIXTURE(MemoryProfilerFixture, GetRelatedMemorySize_StringLosingOwner_MatchesExpectedSize)
	{
		StructWithStrings* strings;
		{
			VectorOfStrings* vec = UNITY_NEW_AS_ROOT(VectorOfStrings,kMemDefault, "", "");	
			SET_ALLOC_OWNER(vec);
			strings = UNITY_NEW(StructWithStrings, kMemDefault);
			strings->str1 = "HelloWorld HelloWorld";
			strings->str2 = "HelloWorld HelloWorld";
			CHECK_EQUAL(GetMemoryProfiler()->GetRelatedMemorySize(vec), sizeof(UNITY_VECTOR(kMemDefault, UnityStr)) + 2 * stringlength + sizeof(StructWithStrings) );
			UNITY_DELETE(vec, kMemDefault);
		}
		UNITY_DELETE(strings, kMemDefault);
	}

}

#endif
#endif