#include "UnityPrefix.h"
#include "TextUtilities.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Misc/UTF8.h"

using std::string;

size_t FindTokenInText( const string& text, const string& token, size_t startPos )
{
	AssertIf( token.empty() );

	enum State { kStateText, kStateLineComment, kStateLongComment };
	State state = kStateText;
	int commentDepth = 0;

	size_t size = text.size();
	for( size_t i = startPos; i < size; ++i )
	{
		char ch = text[i];
		if( state == kStateText )
		{
			// we're in regular text
			if( i < size-1 && ch == '/' ) {
				// detect start of comments
				if( text[i+1] == '/' ) {
					state = kStateLineComment;
					++i;
					continue;
				}
				if( text[i+1] == '*' ) {
					state = kStateLongComment;
					commentDepth = 1;
					++i;
					continue;
				}
			}

			// still regular text - look for token at this very place
			if( ch == token[0] && size - i >= token.size() && (i == startPos || !isalnum(text[i-1])) )
			{
				if( token == text.substr( i, token.size() ) ) // found it!
					return i;
			}
		}
		else if( state == kStateLineComment )
		{
			// we're in one line comment - skip until next line
			if( IsNewline(ch) )
				state = kStateText;
		}
		else
		{
			// we're in long comment - skip until matching '*/'
			if( i < size-1 && ch == '*' && text[i+1] == '/' ) {
				--commentDepth;
				DebugAssertIf( commentDepth < 0 );
				if( commentDepth == 0 )
					state = kStateText;
				++i;
			}

			// track nested long comments
			if( i < size-1 && ch == '/' && text[i+1] == '*' ) {
				DebugAssertIf( commentDepth < 1 );
				++commentDepth;
				++i;
			}
		}
	}

	return string::npos; // not found
}


size_t CountLines( const std::string& text, size_t endPos )
{
	if( endPos >= text.size() ) {
		AssertString( "CountLines: endPos beyond end of text!" );
		endPos = text.size()-1;
	}
	int lineCount = 0;
	for( size_t i = 0; i <= endPos; i++ )
	{
		char c = text[i];
		// count lines with CRLF or CR
		if( c == '\r' )
		{
			++lineCount;
			// if next char is also CR, count two lines; skip next char anyway for possible LF
			if( i < endPos && text[i+1] == '\r' ) {
				++lineCount;
			}
			++i;
		}
		// count LF lines
		else if( c == '\n' )
		{
			++lineCount;
		}
	}
	return lineCount;
}

std::string ExtractRestOfLine( const std::string& text, size_t startPos )
{
	size_t size = text.size();
	size_t end = startPos;
	while( end < size && !IsNewline(text[end]) )
		++end;
	return text.substr( startPos, end - startPos );
}

size_t SkipWhiteSpace( const std::string& text, size_t startPos )
{
	size_t end = text.size();
	while( startPos < end && isspace(text[startPos]) )
		++startPos;
	return startPos;
}

size_t SkipToNextLine (const std::string& text, size_t startPos)
{
	size_t end = text.size();
	// skip until newlines
	while (startPos < end && !IsNewline(text[startPos]))
		++startPos;
	// skip over newlines
	while (startPos < end && IsNewline(text[startPos]))
		++startPos;
	return startPos;
}


std::string ReadNonWhiteSpace( const std::string& text, size_t& startPos )
{
	std::string res;

	size_t end = text.size();
	while( startPos < end && !isspace(text[startPos]) )
	{
		res += text[startPos];
		++startPos;
	}
	return res;
}


bool GetValueAfterPrefix (const std::string& token, const std::string& prefix, std::string& outValue)
{
	if (!BeginsWith (token, prefix))
		return false;
	outValue = token.substr (prefix.size(), token.size()-prefix.size());
	return true;
}

static bool HasUTF16BOM(const unsigned short* str)
{
	unsigned char c1 = ((unsigned char*)str)[0];
	unsigned char c2 = ((unsigned char*)str)[1];

	// check the BOM (Byte Order Mark)
	return /* LE */ (c1 == 0xFF && c2 == 0xFE) || /* BE */ (c1 == 0xFE && c2 == 0xFF);
}

static bool IsBigEndianUTF16BOM(const unsigned short* str)
{
	unsigned char c1 = ((unsigned char*)str)[0];
	unsigned char c2 = ((unsigned char*)str)[1];
    
	// check the BOM (Byte Order Mark)
	return /* BE */ (c1 == 0xFE && c2 == 0xFF);
}

static bool HasUTF8BOM(const unsigned char* str)
{
	unsigned char c1 = str[0];
	unsigned char c2 = str[1];
	unsigned char c3 = str[2];

	// check the BOM (Byte Order Mark)
	return (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF);
}

static bool IsBOMMatchingNativeByteOrder(const unsigned short* src)
{
#if UNITY_BIG_ENDIAN
	return IsBigEndianUTF16BOM(src);
#else
	return !IsBigEndianUTF16BOM(src);
#endif
}

static void CopyArraySwapped(const unsigned short* src, unsigned short* dst, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	{
		unsigned short v = *src++;
		*dst++ = v >> 8 | v << 8; // swap bytes
	}
}

// Convert any text to UTF8 (try to detect src encoding)
std::string ConvertToUTF8(
								 const void* str,
								 unsigned byteLen,
								 bool &encodingDetected)
{
	if(byteLen == 0)
	{
		encodingDetected = false;
		return std::string("");
	}

	const char* charPtr = (const char*)str;
	if (charPtr[0] == 0 && byteLen == 1)
	{
		// Handle this case for empty strings.
		// Note that AssetDatabase.LoadAssetAtPath(path, typeof(TextAsset)) will also call this function
		// so we need to be able to handle loading of arbitrary binary data here too.
		encodingDetected = false;
		return std::string("");
	}

	// is this UTF-16
	const unsigned short* ushortPtr = (const unsigned short*)str;
	if (byteLen >= 2 && HasUTF16BOM(ushortPtr))
	{
		dynamic_array<UnicodeChar> source(kMemTempAlloc);
		std::string utf8;
		size_t sourceLen = byteLen / 2 - 1;
		source.resize_uninitialized(sourceLen);

		if (IsBOMMatchingNativeByteOrder(ushortPtr))
		{
			memcpy(source.data(), ushortPtr+1, sourceLen * sizeof(UnicodeChar));
		}
		else
		{
			// Endian mismatch. We swap bytes to match native endianess
			CopyArraySwapped(ushortPtr+1, source.data(), sourceLen);
		}
		ConvertUTF16toUTF8 (source, utf8);
		encodingDetected = true;
		return utf8;
	}

	if (byteLen >= 3 && HasUTF8BOM((const unsigned char*)charPtr))
	{
		encodingDetected = true;
		return std::string(charPtr+3, byteLen-3);
	}

	encodingDetected = false;
	return std::string(charPtr, byteLen);
}

#if ENABLE_UNIT_TESTS
#include "../../External/UnitTest++/src/UnitTest++.h"
static void CreateUnicodeCharFromString(const std::string& str, dynamic_array<UnicodeChar>& unistr)
{
	CHECK(ConvertUTF8toUTF16(str, unistr));
	CHECK_EQUAL(unistr.size(), str.size());
}
static void TestConversions(const std::string& str)
{
	std::string testStr;
	dynamic_array<UnicodeChar> unistr;
	CreateUnicodeCharFromString(str, unistr);
	CHECK(ConvertUTF16toUTF8(unistr, testStr));
	CHECK_EQUAL(str, testStr);
}
static void TestBOMString(const std::string& str, unsigned char bom1, unsigned char bom2)
{
	dynamic_array<UnicodeChar> unistr;
	CreateUnicodeCharFromString(str, unistr);
	bool encodingDetected;
	dynamic_array<UnicodeChar> bomstr;
	bomstr.resize_uninitialized(str.size() + 1);
	unsigned char* bomptr = (unsigned char*)&bomstr[0];
	std::string bomstr8;
	bomptr[0] = bom1;
	bomptr[1] = bom2;
	
	// Make sure the data actually matches the BOM
	size_t sourceLen = str.size();
	
	if (IsBOMMatchingNativeByteOrder(bomstr.data()))
		memcpy(bomstr.data() + 1, unistr.data(), sourceLen * sizeof(UnicodeChar));
	else
		// Endian mismatch. We swap bytes to match native endianess
		CopyArraySwapped(unistr.data(), bomstr.data() + 1, sourceLen);
	
	CHECK_EQUAL(str, ConvertToUTF8((unsigned short*)&bomstr[0], sizeof(UnicodeChar) * bomstr.size(), encodingDetected));
	CHECK(encodingDetected);
}
static void TestNonUnicode(const std::string& str)
{
	bool encodingDetected;
	CHECK_EQUAL(str, ConvertToUTF8(str.c_str(), str.size(), encodingDetected));
	CHECK(!encodingDetected);
}
SUITE (TextUtilitiesTest)
{
TEST(TextUtilitiesTests_UTF8toUTF16)
{
	TestConversions(std::string(""));
	TestConversions(std::string("A"));
	TestConversions(std::string("AB"));
	TestConversions(std::string("ABC"));
	TestConversions(std::string("Test123~/<>"));
}
TEST(TextUtilitiesTests_NonUnicode)
{
	TestNonUnicode(std::string(""));
	TestNonUnicode(std::string("A"));
	TestNonUnicode(std::string("AB"));
	TestNonUnicode(std::string("ABC"));
	TestNonUnicode(std::string("Test123~/<>"));
}
TEST(TextUtilitiesTests_UTF16BOMLE)
{
	TestBOMString(std::string("~"), 0xFF, 0xFE);
	TestBOMString(std::string("A"), 0xFF, 0xFE);
	TestBOMString(std::string("AB"), 0xFF, 0xFE);
	TestBOMString(std::string("ABC"), 0xFF, 0xFE);
	TestBOMString(std::string("Test123~/<>"), 0xFF, 0xFE);
}
TEST(TextUtilitiesTests_UTF16BOMBE)
{
	TestBOMString(std::string("~"), 0xFE, 0xFF);
	TestBOMString(std::string("A"), 0xFE, 0xFF);
	TestBOMString(std::string("AB"), 0xFE, 0xFF);
	TestBOMString(std::string("ABC"), 0xFE, 0xFF);
	TestBOMString(std::string("Test123~/<>"), 0xFE, 0xFF);
}
}
#endif

