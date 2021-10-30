#include "UnityPrefix.h"

#include "MecanimUtility.h"

#include "Runtime/mecanim/generic/crc32.h"

#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Allocator/MemoryManager.h"

std::string BuildTransitionName(std::string srcStateName, std::string dstStateName)
{
	return srcStateName + " -> " + dstStateName;
}

std::string FileName(const std::string &fullpath)
{
	std::string fullpathCopy(fullpath);
	ConvertSeparatorsToUnity(fullpathCopy);	
	return GetLastPathNameComponent(StandardizePathName(fullpathCopy));
}
std::string FileNameNoExt(const std::string &fullpath)
{		
	return DeletePathNameExtension(FileName(fullpath));
}

unsigned int ProccessString(TOSVector& tos, std::string const& str)
{
	unsigned int crc32 = mecanim::processCRC32(str.c_str());
	TOSVector::iterator it = tos.find(crc32);
	if(it == tos.end())
	{
		tos.insert( std::make_pair(crc32, str) );
	}
	return crc32;
}

std::string FindString(TOSVector const& tos, unsigned int crc32)
{
	TOSVector::const_iterator it = tos.find(crc32);
	if(it!=tos.end())
		return it->second;

	return std::string("");
}
