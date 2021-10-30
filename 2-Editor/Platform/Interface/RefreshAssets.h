#pragma once

#include "Runtime/Utilities/GUID.h"
#include <set>
#include "Runtime/Utilities/PathNameUtility.h"
class AssetDatabase;

void RecursiveRefresh (const char *path, std::set<std::string >* needRefresh, std::set<std::string >* needRefreshMeta, std::set<std::string>* needRemoveMetafile, AssetDatabase* database, int* metaFileCount, int options);

inline bool IsMetaFile (const char* name, size_t length)
{
	if (length < 5)
		return false;
		
	name += length-5;
		
	if (name[0] != '.')
		return false;
	if (name[1] != 'm' && name[1] != 'M')
		return false;
	if (name[2] != 'e' && name[2] != 'E')
		return false;
	if (name[3] != 't' && name[3] != 'T')
		return false;
	if (name[4] != 'a' && name[4] != 'A')
		return false;
	return true;
}
