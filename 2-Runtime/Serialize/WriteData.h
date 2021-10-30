#ifndef WRITE_DATA_H
#define WRITE_DATA_H

#include "SerializationMetaFlags.h"

struct WriteData
{
	LocalIdentifierInFileType localIdentifierInFile;
	SInt32                    instanceID;
	BuildUsageTag             buildUsage;
	
	WriteData () : localIdentifierInFile(0), instanceID(0) { }
	
	WriteData (LocalIdentifierInFileType local, SInt32 instance, const BuildUsageTag& tag)
	: localIdentifierInFile (local), instanceID(instance), buildUsage(tag)
	{ }
	
	WriteData (LocalIdentifierInFileType local, SInt32 instance)
	: localIdentifierInFile (local), instanceID(instance)
	{ }
	
	friend bool operator < (const WriteData& lhs, const WriteData& rhs)
	{
		return lhs.localIdentifierInFile < rhs.localIdentifierInFile;
	}
};
#endif
