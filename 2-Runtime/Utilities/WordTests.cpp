#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS

#include "Word.h"
#include "Runtime/Testing/Testing.h"

using namespace std;

SUITE (WordTests)
{
	TEST (IntToString_Works)
	{
		CHECK (IntToString (123456) == "123456");
		CHECK (IntToString (-123456) == "-123456");
	}

	TEST (Int64ToString_Works)
	{
		CHECK (Int64ToString (1099511627776) == "1099511627776");
		CHECK (Int64ToString (-1099511627776) == "-1099511627776");
	}

	TEST (UnsignedIntToString_Works)
	{
		CHECK (IntToString (123456) == "123456");
	}

	TEST (UnsignedInt64ToString_Works)
	{
		CHECK (UnsignedInt64ToString (1099511627776) == "1099511627776");
	}

	TEST (Word_EndsWith)
	{
		CHECK(EndsWith("abc","c"));
		CHECK(EndsWith("abc","bc"));
		CHECK(EndsWith("abc","abc"));
		CHECK(EndsWith("abc",""));
		CHECK(!EndsWith("abc","d"));
		CHECK(!EndsWith("abc","abcd"));
	}

	TEST (Word_IsStringNumber)
	{
		CHECK_EQUAL(true, IsStringNumber ("-1"));
		CHECK_EQUAL(true, IsStringNumber ("+2"));
		CHECK_EQUAL(false, IsStringNumber ("2+"));
		CHECK_EQUAL(false, IsStringNumber ("a"));
		CHECK_EQUAL(false, IsStringNumber ("1b"));
	}

	TEST (Word_ReplaceString)
	{
		string s;
		s = "foo bar foo"; replace_string (s, "foo", "x"); CHECK_EQUAL("x bar x", s);
		s = "foo bar foo"; replace_string (s, "", ""); CHECK_EQUAL("foo bar foo", s);
	}

	TEST (Word_SimpleStringToFloatWorks)
	{
		int len;
		CHECK_EQUAL (0.0f, SimpleStringToFloat("0",&len)); CHECK_EQUAL(1,len);
		CHECK_EQUAL (0.0f, SimpleStringToFloat("0.0",&len)); CHECK_EQUAL(3,len);
		CHECK_EQUAL (0.0f, SimpleStringToFloat(".0",&len)); CHECK_EQUAL(2,len);
		CHECK_EQUAL (12.05f, SimpleStringToFloat("12.05",&len)); CHECK_EQUAL(5,len);
		CHECK_EQUAL (-3.5f, SimpleStringToFloat("-3.5",&len)); CHECK_EQUAL(4,len);
		CHECK_EQUAL (3.14f, SimpleStringToFloat("3.14",&len)); CHECK_EQUAL(4,len);
		CHECK_EQUAL (-1024.5f, SimpleStringToFloat("-1024.500",&len)); CHECK_EQUAL(9,len);
	}

	TEST (Word_Trim)
	{
		string s;
		s=Trim("   \tspaces in front\n"); CHECK_EQUAL("spaces in front\n",s);
		s=Trim("spaces behind   \t  \t\t"); CHECK_EQUAL("spaces behind",s);
		s=Trim("\t\t\t\tspaces at both ends   \t  \t\t"); CHECK_EQUAL("spaces at both ends",s);
		s=Trim(""); CHECK_EQUAL("",s);
		s=Trim("\t\t\t   \t  \t"); CHECK_EQUAL("",s);
		s=Trim("\n\n Custom Whitespace\r\n","\r\n"); CHECK_EQUAL(" Custom Whitespace",s);
	}

	TEST (Word_Split)
	{
		const int kNumtests = 6;
		const int kMaxtokens = 3;
		const char splitChar = ';';
		const char* splitChars = ";/";
	
		string inputs[kNumtests] =
		{ 
			"Normal;string;split",
			"Adjacent;;separators",
			"NoSeparators",
			"EndWithSeparator;",
			";StartWithSeparator",
			";" // No non-separators
		};

		string inputsMulti[kNumtests] =
		{ 
			"Normal;string/split",
			"Adjacent/;separators",
			"NoSeparators",
			"EndWithSeparator;/",
			";StartWithSeparator",
			";" // No non-separators
		};

		int outputSizes[kNumtests] =
		{
			3, 2, 1, 1, 1, 0
		};

		string outputTokens[][kMaxtokens] =
		{
			{ "Normal", "string", "split" },
			{ "Adjacent", "separators", "" },
			{ "NoSeparators", "", "" },
			{ "EndWithSeparator", "", "" },
			{ "StartWithSeparator", "", "" },
			{ "", "", "" }
		};

		for (int test = 0; test < kNumtests; ++test)
		{
			string s = inputs[test];
			vector<string> tokens;
			Split (inputs[test], splitChar, tokens);
			CHECK_EQUAL (outputSizes[test], tokens.size ());	        // Verify number of tokens
			for (int token = 0; token < outputSizes[test]; ++token)
			{
				CHECK_EQUAL (outputTokens[test][token], tokens[token]); // Verify each token
			}
		}

		for (int test = 0; test < kNumtests; ++test)
		{
			string s = inputs[test];
			vector<string> tokens;
			Split (inputsMulti[test], splitChars, tokens);
			CHECK_EQUAL (outputSizes[test], tokens.size ());	        // Verify number of tokens
			for (int token = 0; token < outputSizes[test]; ++token)
			{
				CHECK_EQUAL (outputTokens[test][token], tokens[token]); // Verify each token
			}
		}
	}
}

#endif // ENABLE_UNIT_TESTS
