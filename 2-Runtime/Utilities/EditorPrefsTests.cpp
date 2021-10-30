#include "UnityPrefix.h"
#include "Runtime/Utilities/PlayerPrefs.h"

#if UNITY_EDITOR && ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

#if UNITY_OSX
	// Defined in EditorPrefsTests.mm
	void InitNSAutoreleasePool();
	void ReleaseNSAutoreleasePool();

	#define INIT_TEST InitNSAutoreleasePool()
	#define RELEASE_TEST ReleaseNSAutoreleasePool()
#else
	#define INIT_TEST
	#define RELEASE_TEST
#endif

// We test PlayerPrefs in Integration tests
// EditorPrefs are not exposed on C# side, so we need to tests them in C++

SUITE (EditorPrefsTests)
{
	TEST(TestEditorPrefs)
	{
		INIT_TEST;
		
		const std::string
			kBoolKeyName = "someBool",
			kIntKeyName = "someInt",
			kFloatKeyName = "someFloat",
			kStringKeyName = "someString",
			kNonExistingKeyName = "someName";		
		const std::string
			kBoolKeyNameCaps = "someBOOL",
			kIntKeyNameCaps = "someINT",
			kFloatKeyNameCaps = "someFLOAT",
			kStringKeyNameCaps = "someSTRING";
		const bool kStoredBool = true, kStoredBoolCaps = false, kDefaultBool = false;
		const int kStoredInt = 3, kStoredIntCaps = 4, kDefaultInt = 7;
		const float kStoredFloat = 5.7f, kStoredFloatCaps = 6.23f, kDefaultFloat = 7.17f;
		const std::string kStoredString = "This is the string", kStoredStringCaps = "CAPs string", kDefaultString = "Did not find meh";

		// Making sure there are no permanently stored keys
		EditorPrefs::DeleteKey(kBoolKeyName);
		EditorPrefs::DeleteKey(kIntKeyName);
		EditorPrefs::DeleteKey(kFloatKeyName);
		EditorPrefs::DeleteKey(kStringKeyName);

		EditorPrefs::DeleteKey(kBoolKeyNameCaps);
		EditorPrefs::DeleteKey(kIntKeyNameCaps);
		EditorPrefs::DeleteKey(kFloatKeyNameCaps);
		EditorPrefs::DeleteKey(kStringKeyNameCaps);

		CHECK(EditorPrefs::SetBool(kBoolKeyName, kStoredBool));
		CHECK(EditorPrefs::SetInt(kIntKeyName, kStoredInt));
		CHECK(EditorPrefs::SetFloat(kFloatKeyName, kStoredFloat));
		CHECK(EditorPrefs::SetString(kStringKeyName, kStoredString));

		CHECK_EQUAL(EditorPrefs::GetBool(kBoolKeyName, kDefaultBool), kStoredBool);
		CHECK_EQUAL(EditorPrefs::GetInt(kBoolKeyName, kDefaultInt), 1); // SetBool fallsback to SetInt, so this call suceeds getting value of kBoolKeyName
		CHECK_EQUAL(EditorPrefs::GetFloat(kBoolKeyName, kDefaultFloat), kDefaultFloat);
		CHECK_EQUAL(EditorPrefs::GetString(kBoolKeyName, kDefaultString), kDefaultString);

		CHECK_EQUAL(EditorPrefs::GetBool(kIntKeyName, kDefaultBool), true); // GetBool fallsback to GetInt, so this call succeeds getting value of kIntKeyName
		CHECK_EQUAL(EditorPrefs::GetInt(kIntKeyName, kDefaultInt), kStoredInt);
		CHECK_EQUAL(EditorPrefs::GetFloat(kIntKeyName, kDefaultFloat), kDefaultFloat);
		CHECK_EQUAL(EditorPrefs::GetString (kIntKeyName, kDefaultString), kDefaultString);

		CHECK_EQUAL(EditorPrefs::GetBool(kFloatKeyName, kDefaultBool), kDefaultBool);
		CHECK_EQUAL(EditorPrefs::GetInt (kFloatKeyName, kDefaultInt), kDefaultInt);
		CHECK_EQUAL(EditorPrefs::GetFloat (kFloatKeyName, kDefaultFloat), kStoredFloat);
		CHECK_EQUAL(EditorPrefs::GetString (kFloatKeyName, kDefaultString), kDefaultString);

		CHECK_EQUAL(EditorPrefs::GetBool(kStringKeyName, kDefaultBool), kDefaultBool);
		CHECK_EQUAL(EditorPrefs::GetInt (kStringKeyName, kDefaultInt), kDefaultInt);
		CHECK_EQUAL(EditorPrefs::GetFloat (kStringKeyName, kDefaultFloat), kDefaultFloat);
		CHECK_EQUAL(EditorPrefs::GetString (kStringKeyName, kDefaultString), kStoredString);

		CHECK_EQUAL(EditorPrefs::GetBool(kNonExistingKeyName, kDefaultBool), kDefaultBool);
		CHECK_EQUAL(EditorPrefs::GetInt (kNonExistingKeyName, kDefaultInt), kDefaultInt);
		CHECK_EQUAL(EditorPrefs::GetFloat (kNonExistingKeyName, kDefaultFloat), kDefaultFloat);
		CHECK_EQUAL(EditorPrefs::GetString (kNonExistingKeyName, kDefaultString), kDefaultString);

		CHECK(EditorPrefs::HasKey(kBoolKeyName));
		CHECK(EditorPrefs::HasKey(kIntKeyName));
		CHECK(EditorPrefs::HasKey(kFloatKeyName));
		CHECK(EditorPrefs::HasKey(kStringKeyName));
		CHECK(!EditorPrefs::HasKey(kNonExistingKeyName));

		// Case sensitivity tests
		CHECK_EQUAL(EditorPrefs::GetBool(kBoolKeyNameCaps, kDefaultBool), kDefaultBool);
		CHECK_EQUAL(EditorPrefs::GetInt(kIntKeyNameCaps, kDefaultInt), kDefaultInt);
		CHECK_EQUAL(EditorPrefs::GetFloat(kFloatKeyNameCaps, kDefaultFloat), kDefaultFloat);
		CHECK_EQUAL(EditorPrefs::GetString(kStringKeyNameCaps, kDefaultString), kDefaultString);

		CHECK(!EditorPrefs::HasKey(kBoolKeyNameCaps));
		CHECK(!EditorPrefs::HasKey(kIntKeyNameCaps));
		CHECK(!EditorPrefs::HasKey(kFloatKeyNameCaps));
		CHECK(!EditorPrefs::HasKey(kStringKeyNameCaps));

		CHECK(EditorPrefs::SetBool(kBoolKeyNameCaps, kStoredBoolCaps));
		CHECK(EditorPrefs::SetInt(kIntKeyNameCaps, kStoredIntCaps));
		CHECK(EditorPrefs::SetFloat(kFloatKeyNameCaps, kStoredFloatCaps));
		CHECK(EditorPrefs::SetString(kStringKeyNameCaps, kStoredStringCaps));

		CHECK_EQUAL(EditorPrefs::GetBool(kBoolKeyName, kDefaultBool), kStoredBool);
		CHECK_EQUAL(EditorPrefs::GetInt(kIntKeyName, kDefaultInt), kStoredInt);
		CHECK_EQUAL(EditorPrefs::GetFloat(kFloatKeyName, kDefaultFloat), kStoredFloat);
		CHECK_EQUAL(EditorPrefs::GetString(kStringKeyName, kDefaultString), kStoredString);

		CHECK_EQUAL(EditorPrefs::GetBool(kBoolKeyNameCaps, kDefaultBool), kStoredBoolCaps);
		CHECK_EQUAL(EditorPrefs::GetInt(kIntKeyNameCaps, kDefaultInt), kStoredIntCaps);
		CHECK_EQUAL(EditorPrefs::GetFloat(kFloatKeyNameCaps, kDefaultFloat), kStoredFloatCaps);
		CHECK_EQUAL(EditorPrefs::GetString(kStringKeyNameCaps, kDefaultString), kStoredStringCaps);
				
		RELEASE_TEST;
	}

	
	// Set value using one type, then set with another type; Check values for both types
	#define TEST_OVERRIDE(TypeName1, TypeName2, Expected1, Expected2) \
		EditorPrefs::DeleteKey(kKeyName); \
		CHECK(EditorPrefs::Set##TypeName1(kKeyName, kStored##TypeName1)); \
		CHECK(EditorPrefs::Set##TypeName2(kKeyName, kStored##TypeName2)); \
		CHECK_EQUAL(EditorPrefs::Get##TypeName1(kKeyName, kDefault##TypeName1), Expected1); \
		CHECK_EQUAL(EditorPrefs::Get##TypeName2(kKeyName, kDefault##TypeName2), Expected2); \

	TEST(EditorPrefsOverriding)
	{
		INIT_TEST;
		
		const std::string kKeyName = "MyKey";
		const bool kStoredBool = true, kDefaultBool = false;
		const int kStoredInt = 3, kDefaultInt = 7;
		const float kStoredFloat = 5.7f, kDefaultFloat = 7.17f;
		const std::string kStoredString = "This is the string", kDefaultString = "Did not find meh";		

		TEST_OVERRIDE(Bool, Int, true, kStoredInt); // ints and bool are stored the same, so value is shared
		TEST_OVERRIDE(Bool, Float, kDefaultBool, kStoredFloat);
		TEST_OVERRIDE(Bool, String, kDefaultBool, kStoredString);

		TEST_OVERRIDE(Int, Bool, 1, true); // ints and bool are stored the same, so value is shared
		TEST_OVERRIDE(Int, Float, kDefaultInt, kStoredFloat);
		TEST_OVERRIDE(Int, String, kDefaultInt, kStoredString);

		TEST_OVERRIDE(Float, Bool, kDefaultFloat, kStoredBool);
		TEST_OVERRIDE(Float, Int, kDefaultFloat, kStoredInt);
		TEST_OVERRIDE(Float, String, kDefaultFloat, kStoredString);

		TEST_OVERRIDE(String, Bool, kDefaultString, kStoredBool);
		TEST_OVERRIDE(String, Int, kDefaultString, kStoredInt);
		TEST_OVERRIDE(String, Float, kDefaultString, kStoredFloat);
	 
		RELEASE_TEST;
	}
}

#endif
