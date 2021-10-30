#pragma once

#include "Runtime/Allocator/STLAllocator.h"
#include "Runtime/Serialize/SerializeTraits.h"

template<class TransferFunction>
inline void TransferConstantString (ConstantString& constantString, const char* name, TransferMetaFlags transferFlags, MemLabelId label, TransferFunction& transfer)
{
	//@TODO: Make this string use temp data
	UnityStr tempStr;
	if (transfer.IsWriting())
		tempStr = constantString.c_str();
	
	transfer.Transfer(tempStr, name, transferFlags);
	
	if (transfer.IsReading())
		constantString.assign(tempStr.c_str(), label);
}
