#include "UnityPrefix.h"
#include "SerializationUtility.h"

void WriteString(dynamic_array<int>& bitstream, const char* str)
{
	int len = strlen(str); // length including null terminator
	int startindex = bitstream.size();
	bitstream.resize_initialized( startindex + len/4 + 1);
	memcpy((char*)&bitstream[startindex], str, len+1);
}

void WriteIntArray(dynamic_array<int>& bitstream, int* data, int count)
{
	for(int i = 0; i < count; i++)
		bitstream.push_back (data[i]);
}

void ReadIntArray(int** bitstream, int* data, int count)
{
	for(int i = 0; i < count; i++)
		data[i] = *((*bitstream)++);
}


