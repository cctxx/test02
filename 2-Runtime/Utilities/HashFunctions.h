#ifndef __HASH_FUNCTIONS_H
#define __HASH_FUNCTIONS_H

#include <string>
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/dense_hash_map.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Allocator/MemoryMacros.h"

struct InstanceIDHashFunctor
{
	inline size_t operator()(SInt32 x) const {

		UInt32 a = static_cast<UInt32> (x);
		a = (a+0x7ed55d16) + (a<<12);
		a = (a^0xc761c23c) ^ (a>>19);
		a = (a+0x165667b1) + (a<<5);
		a = (a+0xd3a2646c) ^ (a<<9);
		a = (a+0xfd7046c5) + (a<<3);
		a = (a^0xb55a4f09) ^ (a>>16);

		return a;
	}
};

typedef pair<const SInt32, Object*> InstanceIdToObjectPair;
struct InstanceIdToObjectPtrHashMap :
	dense_hash_map<SInt32, Object*, InstanceIDHashFunctor, std::equal_to<SInt32>, STL_ALLOCATOR( kMemSTL, InstanceIdToObjectPair ) >
{
	typedef dense_hash_map<SInt32, Object*, InstanceIDHashFunctor, std::equal_to<SInt32>, STL_ALLOCATOR( kMemSTL, InstanceIdToObjectPair )> base_t;
	InstanceIdToObjectPtrHashMap (int n)
		:	base_t (n)
	{
		set_empty_key (-1);
		set_deleted_key (-2);
	}
};

// http://www.cse.yorku.ca/~oz/hash.html
// Other hashes here: http://burtleburtle.net/bob/
struct djb2_hash
{
	inline size_t operator()(const std::string& str) const
	{
		unsigned long hash = 5381;
		char c;
		const char* s = str.c_str ();

		while ((c = *s++))
			hash = ((hash << 5) + hash) ^ c;

		return hash;
	}
};

#if UNITY_EDITOR
struct djb2_hash_lowercase
{
	inline size_t operator() (const std::string& str) const
	{
		unsigned long hash = 5381;
		char c;
#if UNITY_OSX
		const char* s = NULL;
		bool ascii = !RequiresNormalization (str);
		if (ascii)
			s = ToLower (str).c_str();
		else
			s =  ToLower (NormalizeUnicode (str, true)).c_str();		
#else
		std::string help = ToLower(str);
		const char* s = help.c_str();
#endif

		while ((c = *s++))
			hash = ((hash << 5) + hash) ^ c;

		return hash;
	}
};
#endif


bool ComputeMD5Hash( const UInt8* data, size_t dataSize, UInt8 outHash[16] );
bool ComputeSHA1Hash( const UInt8* data, size_t dataSize, UInt8 outHash[20] );

#endif
