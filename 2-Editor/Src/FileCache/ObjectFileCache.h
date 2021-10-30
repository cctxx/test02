#pragma once
#include "Configuration/UnityConfigure.h"
#include "Editor/Src/FileCache/LocalFileCache.h"
#include "Runtime/Utilities/dynamic_array.h"

struct MdFour;

class ObjectFileCache : public LocalFileCache
{
public:
	ObjectFileCache(const std::string& cachePath, UInt64 maximumCacheSize);

	SInt32 LoadAndGetInstanceID(const MdFour& hash, LocalIdentifierInFileType fileID = 1);
	void   Store(const MdFour& hash, dynamic_array<Object*>& objects);

protected:
	virtual void OnRemoveFile(const MdFour& hash);
};
