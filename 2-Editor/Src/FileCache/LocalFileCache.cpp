#include "UnityPrefix.h"
#include "LocalFileCache.h"
#include "Runtime/Utilities/DateTime.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/AssetPipeline/MdFourGenerator.h"
#include "Editor/Src/File/FileScanning.h"

LocalFileCache::LocalFileCache(const std::string& cachePath, UInt64 maximumCacheSize)
: m_CachePath(cachePath)
, m_MaximumCacheSize(maximumCacheSize)
, m_CacheSize(GetCurrentCacheSize())
{
}

void LocalFileCache::DirtyModificationDate(const MdFour& hash)
{
	std::string path = GetPath(hash);
	SetContentModificationDateToCurrentTime(path);
}

void LocalFileCache::AddFile(const MdFour& hash)
{
	std::string path = GetPath(hash);
	m_CacheSize += GetFileLength(path);
		
	if (m_CacheSize > m_MaximumCacheSize)
		ReduceCacheSize();
}

std::string LocalFileCache::GetPath(const MdFour& hash)
{
	std::string path = m_CachePath + "/" + MdFourToString(hash);
	return path;
}

void LocalFileCache::RemoveFile(const MdFour& hash)
{
	std::string path = GetPath(hash);
	m_CacheSize -= GetFileLength(path);
	OnRemoveFile(hash);
}

typedef std::pair<const DirectoryEntry*, DateTime> TFileAndDateTime;
struct SortFilesByDateTime : std::binary_function<TFileAndDateTime, TFileAndDateTime, bool>
{
	bool operator ()(const TFileAndDateTime& lhs, const TFileAndDateTime& rhs) const
	{
		return lhs.second < rhs.second;
	}
};

void LocalFileCache::ReduceCacheSize ()
{
	if (m_CacheSize < m_MaximumCacheSize)
		return;

	// Extract all modification dates
	dynamic_array<DirectoryEntry> entries(kMemTempAlloc);
	GetDirectoryContents(m_CachePath, entries, kFlatSearch);

	dynamic_array<TFileAndDateTime> sorted(kMemTempAlloc);
	sorted.reserve(entries.size());
	for (dynamic_array<DirectoryEntry>::const_iterator it = entries.begin(); it != entries.end(); ++it)
	{
		const DirectoryEntry& entry = *it;
		if (entry.type == kFile)
		{
			std::string path = string(m_CachePath) + "/" + entry.name;
			sorted.push_back(std::make_pair(&entry, GetContentModificationDate(path)));
		}
	}

	// Sort
	std::sort(sorted.begin(), sorted.end(), SortFilesByDateTime());

	// Remove smallest until cache has at least 10% free space
	UInt64 cutoff = m_MaximumCacheSize * 0.9f;
	for (dynamic_array<TFileAndDateTime>::const_iterator it = sorted.begin(); (m_CacheSize > cutoff) && (it != sorted.end()); ++it)
	{
		const DirectoryEntry* entry = it->first;
		MdFour hash = StringToMdFour(entry->name);
		RemoveFile(hash);
	}
}

UInt64 LocalFileCache::GetCurrentCacheSize() const
{
	UInt64 size = 0;

	dynamic_array<DirectoryEntry> entries(kMemTempAlloc);
	GetDirectoryContents(m_CachePath, entries, kFlatSearch);
	for (dynamic_array<DirectoryEntry>::const_iterator it = entries.begin(); it != entries.end(); ++it)
	{
		const DirectoryEntry& entry = *it;
		if (entry.type == kFile)
		{
			std::string path = string(m_CachePath) + "/" + entry.name;
			int len = GetFileLength(path);
			if (len != -1)
				size += len;
		}
	}

	return size;
}
