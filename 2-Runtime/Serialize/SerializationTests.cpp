#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_UNIT_TESTS

#include "Runtime/Testing/Testing.h"
#include "Runtime/Testing/TestFixtures.h"

SUITE (SerializationTests)
{
	//-------------------------------------------------------------------------

	DEFINE_TRANSFER_TEST_FIXTURE (DidReadExistingProperty)
	{
		float m_FloatProperty;
		TRANSFER (m_FloatProperty);
		if (transfer.IsReading ())
		{
			CHECK (transfer.DidReadLastProperty ());
		}
	}

	TEST_FIXTURE (DidReadExistingPropertyTestFixture, SafeBinaryRead_DidReadLastProperty_WithExistingProperty_IsTrue)
	{
		DoSafeBinaryTransfer ();
	}

	TEST_FIXTURE (DidReadExistingPropertyTestFixture, YAMLRead_DidReadLastProperty_WithExistingProperty_IsTrue)
	{
		DoTextTransfer ();
	}

	//-------------------------------------------------------------------------

	DEFINE_TRANSFER_TEST_FIXTURE (DidNotReadMissingProperty)
	{
		float m_Foobar;
		TRANSFER (m_Foobar);

		if (transfer.IsReading ())
		{
			UnityStr value = "foobar";
			TRANSFER (value);

			CHECK (!transfer.DidReadLastProperty ());
			CHECK (value == "foobar");
		}
	}

	TEST_FIXTURE (DidNotReadMissingPropertyTestFixture, SafeBinaryRead_DidReadLastProperty_WithMissingProperty_IsFalse)
	{
		DoSafeBinaryTransfer ();
	}

	TEST_FIXTURE (DidNotReadMissingPropertyTestFixture, YAMLRead_DidReadLastProperty_WithMissingProperty_IsFalse)
	{
		DoTextTransfer ();
	}

	//-------------------------------------------------------------------------

#define kDoubleValue 0.1
#define kFloatValue -2.5f
#define kIntValue 1337
#define kLongLongValue 1234567890123456789LL
#define kCharValue 'X'
#define kBoolValue true
#define kStringValue "UnityFTW"
#define kVectorSize 3

template<class T>
struct FloatingPointConsistencyTest 
{
	#define kNumFloatValues 12
	T values[kNumFloatValues];
	
	T Get(int i)
	{
		switch (i)
		{
			case 0: return std::numeric_limits<T>::min();
			case 1: return std::numeric_limits<T>::max();
			case 2: return std::numeric_limits<T>::denorm_min();
			case 3: return std::numeric_limits<T>::infinity();
			case 4: return -std::numeric_limits<T>::infinity();
			case 5: return std::numeric_limits<T>::quiet_NaN();
			case 6: return std::numeric_limits<T>::epsilon();
			case 7: return -0.0;
			case 8: return (T)12345678901234567890.123456789012345678900;
			case 9: return (T)0.1;
			case 10: return (T)(1.0 / 3.0);
			case 11: return (T)(3 * 1024 * 1024 * 0.19358);
			default: ErrorString("Should not happen!"); return 0;
		}
	}

	void FillStruct ()
	{
		for (int i=0; i<kNumFloatValues; i++)
			values[i] = Get(i);
	}
	
	void VerifyStruct ()
	{
		for (int i=0; i<kNumFloatValues; i++)
		{
			T expected = Get(i);

			// Use memcmp instead of == to test for negative zero and NaN.
			CHECK (memcmp (&expected, values+i, sizeof(T)) == 0);
		}
	}
	
	DECLARE_SERIALIZE (FloatingPointConsistencyTest)
};

template<class T>
template<class TransferFunction> inline
void FloatingPointConsistencyTest<T>::Transfer (TransferFunction& transfer)
{
	for (int i=0; i<kNumFloatValues; i++)
		TRANSFER(values[i]);
}

struct TestStruct {
	float m_Float;
	int m_Int;
	long long m_LongLong;
	char m_Char;
	bool m_Bool;

	void FillStruct ()
	{
		m_Float = kFloatValue;
		m_Int = kIntValue;
		m_Char = kCharValue;
		m_Bool = kBoolValue;
		m_LongLong = kLongLongValue;
	}
	
	void VerifyStruct ()
	{
		CHECK_EQUAL (m_Float, kFloatValue);
		CHECK_EQUAL (m_Int, kIntValue);
		CHECK_EQUAL (m_Char, kCharValue);
		CHECK_EQUAL (m_Bool, kBoolValue);
		CHECK_EQUAL (m_LongLong, kLongLongValue);
	}
	DECLARE_SERIALIZE (TestStruct)
};

struct TestStruct2 {
	FloatingPointConsistencyTest<float> m_FloatTest;
	FloatingPointConsistencyTest<double> m_DoubleTest;
	UnityStr m_String;
	TestStruct m_Struct;
	std::vector<TestStruct> m_Vector;
	std::map<int, TestStruct> m_Map;
	char m_TypelessData[kVectorSize];

	void FillStruct ()
	{
		m_FloatTest.FillStruct();
		m_DoubleTest.FillStruct();
		m_String = kStringValue;
		m_Struct.FillStruct();
		m_Vector.resize(kVectorSize);
		for (int i=0;i<kVectorSize;i++)
			m_Vector[i].FillStruct();
		m_Map[42].FillStruct();
		m_Map[666].FillStruct();
		m_Map[23].FillStruct();
		for (int i=0;i<kVectorSize;i++)
			m_TypelessData[i] = i;
	}

	void VerifyStruct ()
	{
		CHECK_EQUAL (m_String, kStringValue);
		m_Struct.VerifyStruct();
		CHECK_EQUAL (m_Vector.size(), kVectorSize);
		for (int i=0;i<kVectorSize;i++)
			m_Vector[i].VerifyStruct();
		
		m_Map[42].VerifyStruct();
		m_Map[666].VerifyStruct();
		m_Map[23].VerifyStruct();
		for (int i=0;i<kVectorSize;i++)
			CHECK_EQUAL (m_TypelessData[i], i);
		m_FloatTest.VerifyStruct();
		m_DoubleTest.VerifyStruct();
	}

	DECLARE_SERIALIZE (TestStruct2)
};

template<class TransferFunction> inline
void TestStruct::Transfer (TransferFunction& transfer)
{
	TRANSFER(m_Float);
	TRANSFER(m_Int);
	TRANSFER(m_Char);
	TRANSFER(m_Bool);
	TRANSFER(m_LongLong);
}

template<class TransferFunction> inline
void TestStruct2::Transfer (TransferFunction& transfer)
{
	TRANSFER(m_FloatTest);
	TRANSFER(m_DoubleTest);
	TRANSFER(m_String);
	TRANSFER(m_Struct);
	TRANSFER(m_Vector);

	TRANSFER(m_Map);
	transfer.TransferTypelessData (kVectorSize, m_TypelessData, 0);
}

#if SUPPORT_TEXT_SERIALIZATION
TEST (SerialializeYAMLStruct)
{
	TestStruct2 input;
	input.FillStruct ();

	YAMLWrite write (0);
	input.Transfer( write );
	std::string str;
	write.OutputToString(str);
	TestStruct2 output;

	YAMLRead read (str.c_str(), str.size(), 0);
	output.Transfer( read );

	output.VerifyStruct();

} //TEST
#endif

} //SUITE

#endif
