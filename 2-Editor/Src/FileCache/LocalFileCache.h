#pragma once
#include "Configuration/UnityConfigure.h"

struct MdFour;

class LocalFileCache
{
public:
	LocalFileCache(const std::string& cachePath, UInt64 maximumCacheSize);
	
	void DirtyModificationDate(const MdFour& hash);
	void AddFile(const MdFour& hash);
	std::string GetPath(const MdFour& hash);

protected:
	const std::string m_CachePath;
	const UInt64      m_MaximumCacheSize;
	UInt64            m_CacheSize;
	
	void RemoveFile(const MdFour& hash);
	void ReduceCacheSize();

	virtual void OnRemoveFile(const MdFour& hash) = 0;

private:
	UInt64 GetCurrentCacheSize() const;
};
