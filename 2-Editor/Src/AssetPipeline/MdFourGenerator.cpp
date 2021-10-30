#include "UnityPrefix.h"
#include "MdFourGenerator.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/StaticAssert.h"
#include "Runtime/Serialize/SwapEndianBytes.h"
#include <string.h>
#include "Runtime/Utilities/File.h"

MdFour::MdFour (const char* src, size_t len) {
	memcpy(bytes, src, len>16?16:len);
}

MdFour::MdFour () { memset(bytes,0,sizeof(bytes)); }
void MdFour::Reset() { memset(bytes,0,sizeof(bytes)); }

int MdFour::CompareTo(const MdFour& other) const { return memcmp(bytes, other.bytes, 16);}

//#define DEBUG_MD4_GENERATOR
#ifdef DEBUG_MD4_GENERATOR
void DebugBytes (const char* bytes, int len)
{
	printf_console("%d bytes {",len);
	for(int i=0; i < len && i < 20; i++)
		printf_console("%02hhx", bytes[i]);
	if(len > 20) printf_console("...");
	printf_console("}\n");
}
#define DebugFormat printf_console
#else
void DebugBytes (const char* bytes, int len) {}
void DebugFormat (const char* log, ...) {}
#endif

std::string MdFourToString(const MdFour& digest) {
	std::string name;
	name.resize (32);
	for(int i=0; i < 16; i++)
		sprintf(&name[i*2],"%02hhx",digest.bytes[i]);
	return name;
}

static inline int HexToNumber(char c)
{
	if( c >= '0' && c <= '9')
		return c-'0';
	if( c >= 'a' && c <= 'f')
		return c-'a'+10;
	if( c >= 'A' && c <= 'F')
		return c-'A'+10;
	return 0;
}


MdFour StringToMdFour(const std::string& name) {
	MdFour digest;
	int end = name.size() > 32 ? 16 : name.size()/2;
	for( int i = 0; i < end; ++i ) {
		digest.bytes[i] = HexToNumber(name[i*2]) * 16 + HexToNumber(name[i*2+1]);
	}
	return digest;
}

MdFourGenerator::MdFourGenerator()
{
	#if UNITY_OSX || UNITY_LINUX
	MD4_Init (&m_Acc);
	#elif UNITY_WIN
	m_CryptProvider = NULL;
	m_CryptHash = NULL;
	if( !CryptAcquireContext( &m_CryptProvider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) )
	{
		ErrorString( "Failed to create MD4 crypt provider" );
		return;
	}
	if( !CryptCreateHash( m_CryptProvider, CALG_MD4, 0, 0, &m_CryptHash ) )
	{
		ErrorString( "Failed to create MD4 hash" );
		return;
	}


	#else
	#error Unknown platform
	#endif

	m_Done = false;
}

void MdFourGenerator::Feed(const char* bytes, int length) {
	DebugBytes(bytes, length);
	#if UNITY_OSX || UNITY_LINUX
	MD4_Update(&m_Acc, bytes, length);
	#elif UNITY_WIN
	if( m_CryptHash != NULL )
	{
		if( !CryptHashData( m_CryptHash, reinterpret_cast<const unsigned char*>(bytes), length, 0 ) )
		{
			ErrorString( "Failed to feed MD4 data for hashing" );
		}
	}
	#else
	#error Unknown platform
	#endif
}

void MdFourGenerator::Feed(SInt32 data) {
	DebugFormat("Feeding int %08x to hash: ",data);
	#if UNITY_BIG_ENDIAN
	SwapEndianBytes(data);
	#endif
	Feed( reinterpret_cast<char *>(&data) , sizeof(int) );
}

void MdFourGenerator::Feed(UInt32 data) {
	DebugFormat("Feeding int %08x to hash: ",data);
#if UNITY_BIG_ENDIAN
	SwapEndianBytes(data);
#endif
	Feed( reinterpret_cast<char *>(&data) , sizeof(UInt32) );
}

void MdFourGenerator::Feed(ColorRGBA32 data) {
	DebugFormat("Feeding ColorRGBA32 #%02hhx%02hhx%02hhx%02hhx to hash: ",data.r, data.g, data.b, data.a);
	Feed( reinterpret_cast<char *>(&data) , sizeof(ColorRGBA32) );
}

void MdFourGenerator::Feed(float data) {
	DebugFormat("Feeding float %f to hash: ",data);
	if(std::numeric_limits<float>::has_infinity && data == std::numeric_limits<float>::infinity() ) {
		Feed( "+inf" , 4 );
	}
	else if(std::numeric_limits<float>::has_infinity && data == -std::numeric_limits<float>::infinity() ) {
		Feed( "-inf" , 4 );
	}
	else if(std::numeric_limits<float>::has_quiet_NaN && data == std::numeric_limits<float>::quiet_NaN() ) {
		Feed( "qNaN" , 4 );
	}
	else if(std::numeric_limits<float>::has_signaling_NaN && data == std::numeric_limits<float>::signaling_NaN() ) {
		Feed( "sNaN" , 4 );
	}
	else {
		SInt64 quantisized = (double)data * 1000.0;
		#if UNITY_BIG_ENDIAN
		SwapEndianBytes(quantisized);
		#endif
		Feed( reinterpret_cast<char *>(&quantisized) , sizeof(SInt64) );
	}
}

void MdFourGenerator::Feed(double data) {
	DebugFormat("Feeding float %f to hash: ",data);
	if(std::numeric_limits<double>::has_infinity && data == std::numeric_limits<double>::infinity() ) {
		Feed( "+inf" , 4 );
	}
	else if(std::numeric_limits<double>::has_infinity && data == -std::numeric_limits<double>::infinity() ) {
		Feed( "-inf" , 4 );
	}
	else if(std::numeric_limits<double>::has_quiet_NaN && data == std::numeric_limits<double>::quiet_NaN() ) {
		Feed( "qNaN" , 4 );
	}
	else if(std::numeric_limits<double>::has_signaling_NaN && data == std::numeric_limits<double>::signaling_NaN() ) {
		Feed( "sNaN" , 4 );
	}
	else {
		SInt64 quantisized = (double)data * 1000.0;
		#if UNITY_BIG_ENDIAN
		SwapEndianBytes(quantisized);
		#endif
		Feed( reinterpret_cast<char *>(&quantisized) , sizeof(SInt64) );
	}
}

void MdFourGenerator::Feed(UInt64 data) {
	DebugFormat("Feeding uint64 %016llx to hash: ",data);
	#if UNITY_BIG_ENDIAN
	SwapEndianBytes(data);
	#endif
	Feed( reinterpret_cast<char *>(&data) , sizeof(UInt64) );
}

void MdFourGenerator::Feed(SInt64 data) {
	DebugFormat("Feeding uint64 %016llx to hash: ",data);
#if UNITY_BIG_ENDIAN
	SwapEndianBytes(data);
#endif
	Feed( reinterpret_cast<char *>(&data) , sizeof(SInt64) );
}
void MdFourGenerator::Feed(std::string data) {
	DebugFormat("Feeding string \"%s\" to hash: ",data.c_str());
	Feed( data.c_str(), data.size() );
}
void MdFourGenerator::Feed(char const* string) {
	DebugFormat("Feeding string \"%s\" to hash: ", string);
	Feed( string, strlen(string) );
}


void MdFourGenerator::FeedFromFile(std::string pathName)
{
	DebugFormat("Feeding file contents of \"%s\" into hash: ",pathName.c_str());
	File fh;
	if(fh.Open(pathName,File::kReadPermission))
	{
		const int block_size = 16 * 1024;
		char* buffer = new char[block_size];
		int bytes_read;
		while( (bytes_read = fh.Read(buffer,block_size)) > 0)
		{
			Feed(buffer,bytes_read);
		}
		fh.Close();
		delete[] buffer;
	}
}


MdFour MdFourGenerator::Finish() {
	MdFour result;
	if( !m_Done )
	{
		m_Done = true;

		#if UNITY_OSX || UNITY_LINUX

		MD4_Final(result.bytes, &m_Acc);

		#elif UNITY_WIN

		CompileTimeAssert(sizeof(result) == 16, "MdFour should be 16 bytes");
		if( m_CryptHash != NULL )
		{
			DWORD valueLen = sizeof(result);
			if( !CryptGetHashParam( m_CryptHash, HP_HASHVAL, result.bytes, &valueLen, 0 ) )
			{
				ErrorString( "Failed to compute MD4 hash" );
				result.Reset();
			}
		}

		if( m_CryptHash != NULL )
		{
			CryptDestroyHash( m_CryptHash );
			m_CryptHash = NULL;
		}
		if( m_CryptProvider != NULL )
		{
			CryptReleaseContext( m_CryptProvider, 0 );
			m_CryptProvider = NULL;
		}

		#else
		#error Unknown platform
		#endif
	}

	return result;
}

MdFour MdFourFile(std::string filename)
{
	MdFourGenerator generator;
	generator.FeedFromFile(filename);
	return generator.Finish();
}
