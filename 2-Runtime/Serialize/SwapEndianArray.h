#ifndef SWAPENDIANARRAY_H
#define SWAPENDIANARRAY_H

#include "SwapEndianBytes.h"

inline void SwapEndianArray (void* data, int bytesPerComponent, int count)
{
	
	if (bytesPerComponent == 2)
	{
		UInt16* p = (UInt16*)data;
		for (int i=0;i<count;i++)
			SwapEndianBytes (*p++);
	}	
	else if (bytesPerComponent == 4)
	{
		UInt32* p = (UInt32*)data;
		for (int i=0;i<count;i++)
			SwapEndianBytes (*p++);
	}
	else
	{
		AssertIf (bytesPerComponent != 1);
	}
}

#endif
