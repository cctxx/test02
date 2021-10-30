#pragma once

#include "Runtime/Serialize/SerializeUtility.h"

// Holds a 128 bit hash value
struct Hash128
{
	union
	{
		unsigned char bytes[16];
		UInt64        u64[2];
		UInt32        u32[4];
	} hashData ;
	
	friend inline bool operator == (const Hash128& lhs, const Hash128& rhs) { return lhs.hashData.u64[0] == rhs.hashData.u64[0] && lhs.hashData.u64[1] == rhs.hashData.u64[1]; }
	friend inline bool operator != (const Hash128& lhs, const Hash128& rhs) { return lhs.hashData.u64[0] != rhs.hashData.u64[0] || lhs.hashData.u64[1] != rhs.hashData.u64[1]; }
	
	Hash128 () { hashData.u64[0] = 0; hashData.u64[1] = 0; }
	
	DECLARE_SERIALIZE_NO_PPTR (Hash128)
};

// String conversion
std::string Hash128ToString(const Hash128& digest);
Hash128 StringToHash128(const std::string& name);

// Serialization
template<class T>
void Hash128::Transfer (T& transfer)
{
	UInt8* bytes = hashData.bytes;
	
	TRANSFER(bytes[0]);
	TRANSFER(bytes[1]);
	TRANSFER(bytes[2]);
	TRANSFER(bytes[3]);
	TRANSFER(bytes[4]);
	TRANSFER(bytes[5]);
	TRANSFER(bytes[6]);
	TRANSFER(bytes[7]);
	TRANSFER(bytes[8]);
	TRANSFER(bytes[9]);
	TRANSFER(bytes[10]);
	TRANSFER(bytes[11]);
	TRANSFER(bytes[12]);
	TRANSFER(bytes[13]);
	TRANSFER(bytes[14]);
	TRANSFER(bytes[15]);
}