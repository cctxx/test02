#ifndef SERIALIZECONVERSION_H
#define SERIALIZECONVERSION_H

#if SUPPORT_SERIALIZED_TYPETREES
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "SerializeTraits.h"

// Trys to convert from an old type to a new one
template<class OldFormat, class NewFormat>
bool StdTemplateConversionFunction (void* inData, SafeBinaryRead& transfer)
{
	NewFormat& data = *reinterpret_cast<NewFormat*> (inData);
	const TypeTree& oldTypeTree = transfer.GetActiveOldTypeTree ();
	AssertIf (SerializeTraits<OldFormat>::GetTypeString (NULL) != oldTypeTree.m_Type);
	OldFormat oldData;
	
	SafeBinaryRead safeRead;
	CachedReader& temp = safeRead.Init (transfer);
	
	safeRead.Transfer (oldData, oldTypeTree.m_Name.c_str ());
	
	temp.End ();
	
	data = oldData;

	return true;
}

#define REGISTER_CONVERTER(from, to)	\
SafeBinaryRead::RegisterConverter (SerializeTraits<from>::GetTypeString (NULL), SerializeTraits<to>::GetTypeString (NULL),				\
										StdTemplateConversionFunction<from, to>)

#endif
#endif
