#ifndef WRITE_TYPE_TO_BUFFER_H
#define WRITE_TYPE_TO_BUFFER_H

#include "Runtime/Allocator/MemoryManager.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Serialize/TransferFunctions/StreamedBinaryWrite.h"

template <typename TYPE> void WriteTypeToVector (TYPE& object, dynamic_array<UInt8>* data, int options = 0)
{
	data->clear ();
	MemoryCacheWriter memoryCache (*data);
	StreamedBinaryWrite<false> writeStream;
	CachedWriter& writeCache = writeStream.Init (0, BuildTargetSelection::NoTarget());
	
	writeCache.InitWrite(memoryCache);
	writeStream.Transfer(object, "Base");
	
	if (!writeCache.CompleteWriting () || writeCache.GetPosition() != data->size ())
		ErrorString ("Error while writing serialized data.");	
}
#endif
