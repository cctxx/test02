#ifndef _SERIALIZATIONUTILITY_H_
#define _SERIALIZATIONUTILITY_H_

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Serialize/SwapEndianBytes.h"

void WriteString(dynamic_array<int>& bitstream, const char* str);

template <typename stringtype>
void ReadString(int** bitstream, stringtype& str, bool swapdata)
{
	char* chars = (char*)*bitstream;	
	if(swapdata)
	{
		int wordcount = strlen(chars)/4 + 1;
		for(int i = 0; i < wordcount; i++)
			SwapEndianBytes((*bitstream)[i]);
	}
	str = stringtype((char*)*bitstream);
	(*bitstream) += str.length()/4 + 1;
}

void WriteIntArray(dynamic_array<int>& bitstream, int* data, int count);

template <typename writestruct>
void WriteIntArray(dynamic_array<int>& bitstream, writestruct& data)
{
	int count = sizeof(data)/4;
	WriteIntArray(bitstream, (int*)&data, count);
}

void ReadIntArray(int** bitstream, int* data, int count);

template <typename writestruct>
void ReadIntArray(int** bitstream, writestruct& data)
{
	int count = sizeof(data)/4;
	ReadIntArray(bitstream, (int*)&data, count);
}

#endif
