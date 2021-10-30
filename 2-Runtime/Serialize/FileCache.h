#ifndef FILECACHE_H
#define FILECACHE_H

#include "Runtime/Utilities/LogAssert.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Utilities/File.h"
#include <list>
#include <vector>
#include <map>
#include <deque>
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Utilities/dynamic_array.h"

#define DEBUG_LINEAR_FILE_ACCESS 0

using std::max;
using std::min;
using std::list;

class EXPORT_COREMODULE CacheReaderBase
{
public:

	enum
	{
		kImmediatePriority = 0,
		kPreloadPriority = 1,
		kLoadPriorityCount = 2
	};

	enum
	{
		kUnloadPriority = 0,
		kPreloadedPriority = 1,
		kRequiredPriority = 2,
		kUnloadPriorityCount = 3
	};

	virtual ~CacheReaderBase () = 0;

	virtual void DirectRead (void* data, size_t position, size_t size) = 0;
	virtual void LockCacheBlock (int block, UInt8** startPos, UInt8** endPos) = 0;
	virtual void UnlockCacheBlock (int block) = 0;

	virtual size_t GetCacheSize () const = 0;
	virtual std::string GetPathName() const = 0;
	virtual size_t GetFileLength () const = 0;
	virtual UInt8* GetAddressOfMemory() { ErrorString("GetAddressOfMemory called on CacheReaderBase which does not support it"); return NULL; }
};

class CacheWriterBase
{
public:
	virtual ~CacheWriterBase () = 0;

	virtual bool CompleteWriting (size_t size) = 0;
	virtual bool WriteHeaderAndCloseFile (void* /*data*/, size_t /*position*/, size_t /*size*/) { AssertString("Only used for writing serialized files"); return false; }

	virtual void LockCacheBlock (int block, UInt8** startPos, UInt8** endPos) = 0;
	virtual void UnlockCacheBlock (int block) = 0;

	virtual size_t GetCacheSize () const = 0;
	virtual std::string GetPathName() const = 0;
	virtual size_t GetFileLength () const = 0;
	virtual UInt8* GetAddressOfMemory() { ErrorString("GetAddressOfMemory called on CacheWriterBase which does not support it"); return NULL; }
};

class MemoryCacheReader : public CacheReaderBase
{
protected:
	enum
	{
		kCacheSize = 256
	};

	dynamic_array<UInt8>&	m_Memory;
	SInt32				m_LockCount;

public:
	MemoryCacheReader (dynamic_array<UInt8>& mem) : m_Memory (mem), m_LockCount(0) { }
	virtual ~MemoryCacheReader () { AssertIf (m_LockCount != 0); }

	virtual void LockCacheBlock (int block, UInt8** startPos, UInt8** endPos)
	{
		*startPos = m_Memory.size() > block * kCacheSize ? &m_Memory[block * kCacheSize] : NULL;
		*endPos = *startPos + min<int> (GetFileLength () - block * kCacheSize, kCacheSize);
		m_LockCount++;
	}

	virtual void DirectRead (void* data, size_t position, size_t size)
	{
		AssertIf (m_Memory.size () - position < size);
		memcpy (data, &m_Memory[position], size);
	}

	virtual void UnlockCacheBlock (int /*block*/)	{ m_LockCount--; }

	virtual size_t GetFileLength ()	const			{ return m_Memory.size (); }
	virtual size_t GetCacheSize () const			{ return kCacheSize; }
	virtual std::string GetPathName() const 		{ return "MemoryStream"; }
	virtual UInt8* GetAddressOfMemory() 			{ return &m_Memory[0]; }
};

class MemoryCacheWriter : public CacheWriterBase
{
protected:
	enum
	{
		kCacheSize = 256
	};

	dynamic_array<UInt8>& m_Memory;
	SInt32				  m_LockCount;

public:
	MemoryCacheWriter (dynamic_array<UInt8>& mem) : m_Memory (mem), m_LockCount(0) { }
	virtual ~MemoryCacheWriter () { AssertIf (m_LockCount != 0); }

	virtual void LockCacheBlock (int block, UInt8** startPos, UInt8** endPos)
	{
		m_Memory.resize_uninitialized (max<int> ((block + 1) * kCacheSize, m_Memory.size ()), true);
		*startPos = &m_Memory[block * kCacheSize];
		*endPos = *startPos + kCacheSize;

		m_LockCount++;
	}

	virtual bool CompleteWriting (size_t size)		{ m_Memory.resize_uninitialized (size); m_Memory.shrink_to_fit(); return true; }

	virtual void UnlockCacheBlock (int /*block*/)	{ m_LockCount--; }

	virtual size_t GetFileLength ()	const			{ return m_Memory.size (); }
	virtual size_t GetCacheSize () const			{ return kCacheSize; }
	virtual std::string GetPathName() const			{ return "MemoryStream"; }
};

enum
{
	kBlockCacherCacheSize = 256
};

class BlockMemoryCacheWriter : public CacheWriterBase
{
protected:

	enum
	{
		kNumBlockReservations = 256
	};

	size_t		m_Size;
	SInt32		m_LockCount;
	MemLabelId	m_AllocLabel;

	//	It is possible to use the custom allocator for this index as well -- however,
	//	using the tracking linear tempory allocator is most efficient, when deallocating
	//	in the exact opposite order of allocating, which can only be guaranteed, when all allocations
	//	are in our control.
	typedef UNITY_VECTOR(kMemFile, UInt8*)	BlockVector;
	BlockVector					m_Blocks;

	public:

	BlockMemoryCacheWriter (MemLabelId label)
		: m_AllocLabel(label)
		, m_Blocks()
	{
		m_Blocks.reserve(kNumBlockReservations);
		m_Size = 0;
		m_LockCount = 0;
	}

	~BlockMemoryCacheWriter () {
		AssertIf (m_LockCount != 0);
		for(BlockVector::reverse_iterator i = m_Blocks.rbegin(); i != m_Blocks.rend(); i++)
			UNITY_FREE(m_AllocLabel, *i);
	}

	void ResizeBlocks (int newBlockSize)
	{
		int oldBlockSize = m_Blocks.size();

		for(int block = oldBlockSize-1; block>=newBlockSize; block--)
			UNITY_FREE(m_AllocLabel, m_Blocks[block]);

		if(m_Blocks.capacity() < newBlockSize)
			m_Blocks.reserve(m_Blocks.capacity() * 2);

		m_Blocks.resize(newBlockSize);

		for(int block = oldBlockSize; block<newBlockSize; block++)
			m_Blocks[block] = (UInt8*)UNITY_MALLOC(m_AllocLabel, kBlockCacherCacheSize);
	}

	virtual void LockCacheBlock (int block, UInt8** startPos, UInt8** endPos)
	{
		ResizeBlocks (max<int>(block+1, m_Blocks.size()));
		*startPos = m_Blocks[block];
		*endPos = *startPos + kBlockCacherCacheSize;
		m_LockCount++;
	}

	virtual bool CompleteWriting (size_t size)
	{
		m_Size = size;
		ResizeBlocks(m_Size/kBlockCacherCacheSize + 1);
		return true;
	}
	virtual void UnlockCacheBlock (int /*block*/) 	{ m_LockCount--; }
	virtual size_t GetFileLength ()	const			{ return m_Size; }
	virtual size_t GetCacheSize () const			{ return kBlockCacherCacheSize; }
	virtual std::string GetPathName() const			{ return "MemoryStream"; }

	// Expose, so the internal data can be used with MemoryCacherReadBlocks (see below)
	UInt8** GetCacheBlocks ()						{ return m_Blocks.empty () ? NULL : &*m_Blocks.begin (); }
};

#if SUPPORT_SERIALIZE_WRITE

/// Used by SerializedFile to write to disk.
/// Currently it doesn't allow any seeking that is, you can only write blocks in consecutive order
class FileCacherWrite : public CacheWriterBase
{
public:
	FileCacherWrite ();
	void InitWriteFile (const std::string& pathName, size_t cacheSize);

	virtual ~FileCacherWrite ();

	virtual void LockCacheBlock (int block, UInt8** startPos, UInt8** endPos);
	virtual void UnlockCacheBlock (int block);

	virtual bool WriteHeaderAndCloseFile (void* data, size_t position, size_t size);

	virtual bool CompleteWriting (size_t size);

	virtual size_t  GetCacheSize () const	{ return m_CacheSize; }
	virtual std::string GetPathName() const;
	virtual size_t  GetFileLength () const	{ AssertString("Dont use"); return 0; }

private:
	void WriteBlock (int block);

	int    m_Block;
	UInt8* m_DataCache;
	size_t    m_CacheSize;

	File          m_File;
	bool m_Success;
	bool m_Locked;
	std::string m_Path;
};

#endif // SUPPORT_SERIALIZE_WRITE


/// Used by SerializedFile to read from disk
class FileCacherRead : public CacheReaderBase
{
public:
	FileCacherRead (const std::string& pathName, size_t cacheSize, size_t cacheCount);
	~FileCacherRead ();

	virtual void LockCacheBlock (int block, UInt8** startPos, UInt8** endPos);
	virtual void UnlockCacheBlock (int block);

	virtual void DirectRead (void* data, size_t position, size_t size);

	virtual size_t GetFileLength () const   { return m_FileSize; }
	virtual size_t GetCacheSize () const    { return m_CacheSize; }
	virtual std::string GetPathName() const;

private:

	// Finishes all reading, deletes all caches
	void Flush ();

	struct CacheBlock
	{
		UInt8* data;
		int block;
		int lockCount;
		unsigned timeStamp;
	};

	// Reads the cacheBlock from disk.
	void ReadCacheBlock (CacheBlock& cacheBlock);

	/// Allocates an cache block at block
	CacheBlock& AllocateCacheBlock (int block);

	// Frees the cache block with the smallest timestamp that is not locked.
	// Returns whether or not a cache block could be freed.
	bool FreeSingleCache ();

	typedef UNITY_MAP(kMemFile, int, CacheBlock) CacheBlocks;
	CacheBlocks                     m_CacheBlocks;

	size_t                          m_CacheSize;
	size_t                          m_MaxCacheCount;
	size_t                          m_FileSize;
	UInt32                          m_TimeStamp;
	std::string                     m_Path;
	File                            m_File;
	ProfilerAllocationHeader*       m_RootHeader;
	friend struct OpenFilesCache;

	#if DEBUG_LINEAR_FILE_ACCESS
	int m_LastFileAccessPosition;
	#endif

};

enum
{
	// Evil: Must match the in CompressedFileStreamMemory.h, UncompressedFileStreamMemory.h"
	kCacheBlockSize = 1024 * 100
};

/// Used by SerializedFile to read from disk
class MemoryCacherReadBlocks : public CacheReaderBase
{
public:

	MemoryCacherReadBlocks (UInt8** blocks, int size, size_t cacheBlockSize);
	~MemoryCacherReadBlocks ();

	virtual void LockCacheBlock (int block, UInt8** startPos, UInt8** endPos);
	virtual void UnlockCacheBlock (int /*block*/) {}

	virtual void DirectRead (void* data, size_t position, size_t size);

	virtual size_t  GetFileLength () const      { return m_FileSize; }

	virtual size_t GetCacheSize () const        { return m_CacheBlockSize; }

	virtual std::string GetPathName() const { return "none"; }

	virtual UInt8* GetAddressOfMemory()
	{
		return m_Memory[0];
	}

private:

	// Finishes all reading, deletes all caches
	void Flush ();

	UInt8**		m_Memory;
	size_t		m_FileSize;
	size_t		m_CacheBlockSize;
};

void ReadFileCache (CacheReaderBase& cacher, void* data, size_t position, size_t size);
#if UNITY_EDITOR
void ForceCloseAllOpenFileCaches ();
#endif

#endif

