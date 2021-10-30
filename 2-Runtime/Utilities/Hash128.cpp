#include "UnityPrefix.h"
#include "Hash128.h"

std::string Hash128ToString(const Hash128& digest)
{
	std::string name;
	name.resize (32);
	for(int i=0; i < 16; i++)
		sprintf(&name[i*2],"%02hhx",digest.hashData.bytes[i]);
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


Hash128 StringToHash128(const std::string& name)
{
	Hash128 digest;
	int end = name.size() > 32 ? 16 : name.size()/2;
	for( int i = 0; i < end; ++i ) {
		digest.hashData.bytes[i] = HexToNumber(name[i*2]) * 16 + HexToNumber(name[i*2+1]);
	}
	return digest;
}