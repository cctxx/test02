#include "UnityPrefix.h"
#include "FileCache.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Utilities/PathNameUtility.h"

#if SUPPORT_SERIALIZE_WRITE
#include "Runtime/Utilities/FileUtilities.h"
#endif

using namespace std;

#define USE_OPEN_FILE_CACHE UNITY_EDITOR

#if USE_OPEN_FILE_CACHE

struct OpenFilesCache
{
	enum { kOpenedFileCacheCount = 5 };
	File* m_Cache[kOpenedFileCacheCount];
	UInt32 m_TimeStamps[kOpenedFileCacheCount];
	UInt32 m_TimeStamp;
	
	OpenFilesCache ()
	{
		m_TimeStamp = 0;
		for (int i=0;i<kOpenedFileCacheCount;i++)
		{
			m_Cache[i] = NULL;
			m_TimeStamps[i] = 0;
		}
	}
	
	void OpenCached (File* theFile, const std::string& thePath)
	{
		m_TimeStamp++;

		// find cache, don't do anything if we are in the cache
		for (int i=0;i<kOpenedFileCacheCount;i++)
		{
			if (theFile == m_Cache[i])
			{
				m_TimeStamps[i] = m_TimeStamp;
				return;
			}
		}
		
		// Find Least recently used cache entry
		UInt32 lruTimeStamp = m_TimeStamps[0];
		int lruIndex = 0;
		for (int i=1;i<kOpenedFileCacheCount;i++)
		{
			if (m_TimeStamps[i] < lruTimeStamp)
			{
				lruTimeStamp = m_TimeStamps[i];
				lruIndex = i;
			}
		}
		
		// replace the least recently used cache entry
		if (m_Cache[lruIndex] != NULL)
		{
		#if UNITY_OSX
			m_Cache[lruIndex]->Lock (File::kNone, false);
		#endif
			m_Cache[lruIndex]->Close ();
		}
		
		m_Cache[lruIndex] = theFile;
		m_TimeStamps[lruIndex] = m_TimeStamp;
		
		if(!theFile->Open (thePath, File::kReadPermission, File::kSilentReturnOnOpenFail))
			ErrorString(Format("Could not open file %s for read", thePath.c_str()));

	#if UNITY_OSX
		theFile->Lock (File::kShared, false);
	#endif
	}

	void ForceCloseAll ()
	{
		// Find and close cache
		for (int i=0;i<kOpenedFileCacheCount;i++)
		{
			if (m_Cache[i] != NULL)
				ForceClose (m_Cache[i]);
		}
	}
	
	void ForceClose (File* cachable)
	{
		// Find and close cache
		for (int i=0;i<kOpenedFileCacheCount;i++)
		{
			if (m_Cache[i] == cachable)
			{
			#if UNITY_OSX
				m_Cache[i]->Lock (File::kNone, false);
			#endif
				m_Cache[i]->Close ();
				m_Cache[i] = NULL;
				m_TimeStamps[i] = 0;
				return;
			}
		}
	}
};
OpenFilesCache gOpenFilesCache;

void ForceCloseAllOpenFileCaches ()
{
	gOpenFilesCache.ForceCloseAll ();
}

#endif


CacheReaderBase::~CacheReaderBase ()
{}

CacheWriterBase::~CacheWriterBase ()
{}

FileCacherRead::FileCacherRead (const string& pathName, size_t cacheSize, size_t cacheCount)
{
	m_RootHeader = GET_CURRENT_ALLOC_ROOT_HEADER();
	m_Path = PathToAbsolutePath(pathName);
	
	// Initialize Cache
	m_MaxCacheCount = cacheCount;
	m_CacheSize = cacheSize;
	m_TimeStamp = 0;
	
	// Get File size
	m_FileSize = ::GetFileLength(m_Path);
	
	#if !USE_OPEN_FILE_CACHE
	if(!m_File.Open(m_Path, File::kReadPermission, File::kSilentReturnOnOpenFail))
		ErrorString(Format("Could not open file %s for read", m_Path.c_str()));

	// Make the file non-overwritable by the cache (to make caching behaviour imitate windows)
	#if UNITY_OSX
	m_File.Lock (File::kShared, false);
	#endif

	#endif
	#if DEBUG_LINEAR_FILE_ACCESS
	m_LastFileAccessPosition = 0;
	#endif
}

FileCacherRead::~FileCacherRead ()
{
	for (CacheBlocks::iterator i = m_CacheBlocks.begin ();i != m_CacheBlocks.end ();++i)
	{
		AssertIf (i->second.lockCount);
		UNITY_FREE(kMemFile,i->second.data);
	}

	m_CacheBlocks.clear ();

	#if USE_OPEN_FILE_CACHE
	gOpenFilesCache.ForceClose (&m_File);
	#else
	// Make the file overwritable by the cache.
	#if UNITY_OSX
	m_File.Lock (File::kNone, false);
	#endif

	m_File.Close();

	#endif
}

void FileCacherRead::ReadCacheBlock (CacheBlock& cacheBlock)
{		
	int block = cacheBlock.block;
	// Watch out for not reading over eof
	int readSize = min<int> (m_CacheSize, m_FileSize - block * m_CacheSize);
	
	// load the data from disk
	// only if the physical file contains any data for this block
	if (readSize > 0)
	{
		#if USE_OPEN_FILE_CACHE
		gOpenFilesCache.OpenCached (&m_File, m_Path);
		#endif
		
		#if DEBUG_LINEAR_FILE_ACCESS
		size_t position = block * m_CacheSize;
		#endif
		
		m_File.Read (block * m_CacheSize, cacheBlock.data, readSize);

		#if DEBUG_LINEAR_FILE_ACCESS
		// printf_console("ACCESS: [%08x] %s %i bytes @ %i to %08x\n",this, __FUNCTION__, readSize, block * m_CacheSize, cacheBlock.data);
		std::string fileName = GetLastPathNameComponent(m_Path);
		if (position < m_LastFileAccessPosition && fileName != "mainData" && fileName != "unity default resources")
		{
			ErrorString(Format("File access: %s is not linear  Reading: %d Seek position: %d", fileName.c_str(), position, m_LastFileAccessPosition));
		}
		
		m_LastFileAccessPosition = position + readSize;
		#endif
	}
}

void FileCacherRead::DirectRead (void* data, size_t position, size_t size)
{
	// load the data from disk
	// only if the physical file contains any data for this block
	FatalErrorIf (m_FileSize - position < size);

	#if USE_OPEN_FILE_CACHE
	gOpenFilesCache.OpenCached (&m_File, m_Path);
	#endif
	
	m_File.Read (position, data, size);

	#if DEBUG_LINEAR_FILE_ACCESS
	// printf_console("ACCCESS: [%08x] %s %i bytes @ %i to %08x\n",this, __FUNCTION__, size, position, data);
	std::string fileName = GetLastPathNameComponent(m_Path);
	if (position < m_LastFileAccessPosition && fileName != "mainData" && fileName != "unity default resources")
	{
		ErrorString(Format("File access: %s is not linear  Reading: %d Seek position: %d", fileName.c_str(), position, m_LastFileAccessPosition));
 	}
	
	m_LastFileAccessPosition = position + size;
	#endif
}


bool FileCacherRead::FreeSingleCache ()
{
	unsigned lowestTimeStamp = -1;
	CacheBlocks::iterator lowestCacheBlock = m_CacheBlocks.end ();
	
	CacheBlocks::iterator i;
	for (i=m_CacheBlocks.begin ();i != m_CacheBlocks.end ();i++)
	{
		if (i->second.lockCount == 0)
		{
			if (i->second.timeStamp < lowestTimeStamp)
			{
				lowestTimeStamp = i->second.timeStamp;
				lowestCacheBlock = i;
			}
		}
	}

	if (lowestCacheBlock != m_CacheBlocks.end ())
	{
		CacheBlock& block = lowestCacheBlock->second;
		UNITY_FREE(kMemFile,block.data);
		m_CacheBlocks.erase (lowestCacheBlock);
		return true;
	}
	else
		return false;
}

FileCacherRead::CacheBlock& FileCacherRead::AllocateCacheBlock (int block)
{
	AssertIf (m_CacheBlocks.count (block));
	CacheBlock cacheBlock;
	cacheBlock.block = block;
	cacheBlock.lockCount = 0;
	cacheBlock.timeStamp = 0;
	cacheBlock.data = (UInt8*)UNITY_MALLOC(MemLabelId(kMemFileId, m_RootHeader),m_CacheSize);
	m_CacheBlocks[block] = cacheBlock;
	return m_CacheBlocks[block];
}

void FileCacherRead::LockCacheBlock (int block, UInt8** startPos, UInt8** endPos)
{
	CacheBlock* newCacheBlock;
	CacheBlocks::iterator i = m_CacheBlocks.find (block);
	if (i != m_CacheBlocks.end ())
		newCacheBlock = &i->second;
	else
	{
		// Make room for a new cache block
		if (m_CacheBlocks.size () >= m_MaxCacheCount)
			FreeSingleCache ();
		
		newCacheBlock = &AllocateCacheBlock (block);
		ReadCacheBlock (*newCacheBlock);
	}	
	AssertIf (newCacheBlock->block != block);
	
	newCacheBlock->timeStamp = ++m_TimeStamp;
	newCacheBlock->lockCount++;
	
	*startPos = newCacheBlock->data;
	*endPos = newCacheBlock->data + min<int> (m_FileSize - block * GetCacheSize (), GetCacheSize ());
}

void FileCacherRead::UnlockCacheBlock (int block)
{
	AssertIf (!m_CacheBlocks.count (block));
	CacheBlock& cacheBlock = m_CacheBlocks.find (block)->second;
	cacheBlock.lockCount--;
	if (cacheBlock.lockCount == 0)
	{
		// LockCacheBlock sometimes locks more caches than m_MaxCacheCount
		// Thus we should Free them as soon as they become unlocked
		if (m_CacheBlocks.size () > m_MaxCacheCount)
			FreeSingleCache ();
	}
}

std::string FileCacherRead::GetPathName() const
{
	return m_Path;
}

#if SUPPORT_SERIALIZE_WRITE
FileCacherWrite::FileCacherWrite ()
{
	m_Success = true;
	m_Block = -1;
	m_Locked = false;
	m_CacheSize = 0;
	m_DataCache = NULL;
}

void FileCacherWrite::InitWriteFile (const std::string& pathName, size_t cacheSize)
{
	m_Path = PathToAbsolutePath(pathName);
	m_Success = true;

	m_File.Open(m_Path, File::kWritePermission);
	// file we're writing to always is a temporary, non-indexable file
	SetFileFlags(m_Path, kAllFileFlags, kFileFlagDontIndex|kFileFlagTemporary);
	
	m_Block = -1;
	m_Locked = false;
	m_CacheSize = cacheSize;
	m_DataCache = (UInt8*)UNITY_MALLOC (kMemFile, m_CacheSize);
}

bool FileCacherWrite::CompleteWriting (size_t size)
{
	Assert(m_Block != -1);

	size_t remainingData = size - (m_Block * m_CacheSize);
	Assert(remainingData <= m_CacheSize);
	
	m_Success &= m_File.Write(m_Block * m_CacheSize, m_DataCache, remainingData);
	
	return m_Success;
}

bool FileCacherWrite::WriteHeaderAndCloseFile (void* data, size_t position, size_t size)
{	
	Assert(position == 0);
	if (size != 0)
		m_Success &= m_File.Write(position, data, size);
	m_Success &= m_File.Close();

	return m_Success;
}


FileCacherWrite::~FileCacherWrite()
{
	if (m_DataCache)
	{
		UNITY_FREE (kMemFile, m_DataCache);
		m_DataCache = NULL;
	}
	
	m_File.Close();
}

void FileCacherWrite::LockCacheBlock (int block, UInt8** startPos, UInt8** endPos)
{
	AssertIf (block == -1);
	Assert (block == m_Block || m_Block+1 == block );
	Assert (!m_Locked);
	
	if (m_Block != block)
	{
		AssertIf (m_Locked != 0);
		
		if (m_Block != -1)
			m_Success &= m_File.Write(m_DataCache, m_CacheSize);

		m_Block = block;
	}
	
	*startPos = m_DataCache;
	*endPos = m_DataCache + m_CacheSize;
	m_Locked++;
}

void FileCacherWrite::UnlockCacheBlock (int block)
{
	Assert (block == m_Block);
	Assert (m_Locked);
	
	m_Locked = false;
}


std::string FileCacherWrite::GetPathName() const
{
	return m_Path;
}

#endif // SUPPORT_SERIALIZE_WRITE


MemoryCacherReadBlocks::MemoryCacherReadBlocks (UInt8** blocks, int size, size_t cacheBlockSize)
:	m_CacheBlockSize(cacheBlockSize)
,	m_Memory(blocks)
,	m_FileSize(size)
{
}


MemoryCacherReadBlocks::~MemoryCacherReadBlocks ()
{
}

void MemoryCacherReadBlocks::LockCacheBlock (int block, UInt8** startPos, UInt8** endPos)
{
	/// VERIFY OUT OF BOUNDS!!! ???
	AssertIf(block > m_FileSize / m_CacheBlockSize);
	*startPos = m_Memory[block];
	*endPos = *startPos + min<int> (GetFileLength () - block * m_CacheBlockSize, m_CacheBlockSize);
}

void MemoryCacherReadBlocks::DirectRead (void* data, size_t position, size_t size)
{
	ReadFileCache(*this, data, position, size);
}

void ReadFileCache (CacheReaderBase& cacher, void* data, size_t position, size_t size)
{
	UInt8 *cacheStart, *cacheEnd; 
	UInt8 *from, *fromClamped, *to, *toClamped;	
	
	int block = position / cacher.GetCacheSize ();
	int lastBlock = (position + size - 1) / cacher.GetCacheSize ();
	
	while (block <= lastBlock)
	{
		cacher.LockCacheBlock (block, &cacheStart, &cacheEnd);
		
		// copy data from oldblock and unlock
		from = cacheStart + (position - block * cacher.GetCacheSize ());
		fromClamped = clamp (from, cacheStart, cacheEnd);
		to = cacheStart + (position + size - block * cacher.GetCacheSize ());
		toClamped = clamp<UInt8*> (to, cacheStart, cacheEnd);
		memcpy ((UInt8*)data + (fromClamped - from), fromClamped, toClamped - fromClamped);
		
		cacher.UnlockCacheBlock (block);
		block++;
	}
}
