#include "UnityPrefix.h"
#include "CacheWrap.h"
#include "FileCache.h"
#include "Runtime/Utilities/Utility.h"
#include <algorithm>
#if UNITY_EDITOR
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Utilities/FileUtilities.h"
#endif

#include "Runtime/Scripting/ScriptingUtility.h"

using namespace std;

#if UNITY_WINRT
#define OUTPUT_FIELDS_OF_LAST_SERIALIZABLE OutputFieldsOfLastSerializableObject();
#else
#define OUTPUT_FIELDS_OF_LAST_SERIALIZABLE
#endif

CachedReader::CachedReader ()
{
	m_Cacher = 0;
	m_Block = -1;
	m_OutOfBoundsRead = false;
	m_ActiveResourceImage = NULL;
	#if CHECK_SERIALIZE_ALIGNMENT
	m_CheckSerializeAlignment = true;
	#endif
}

void CachedReader::InitRead (CacheReaderBase& cacher, size_t position, size_t readSize)
{
	AssertIf (m_Block != -1);
	m_Cacher = &cacher;
	AssertIf (m_Cacher == NULL);
	m_CacheSize = m_Cacher->GetCacheSize ();
	m_Block = position / m_CacheSize;
	m_MaximumPosition = position + readSize;
	m_MinimumPosition = position;

	LockCacheBlockBounded ();

	SetPosition (position);
}

void CachedReader::InitResourceImages (ResourceImageGroup& resourceImageGroup)
{
	Assert(m_ActiveResourceImage == NULL);
	m_ResourceImageGroup = resourceImageGroup;
}


void CachedReader::LockCacheBlockBounded ()
{
	m_Cacher->LockCacheBlock (m_Block, &m_CacheStart, &m_CacheEnd);
	UInt8* maxPos = m_MaximumPosition - m_Block * m_CacheSize + m_CacheStart;
	m_CacheEnd = min(m_CacheEnd, maxPos);
}

size_t CachedReader::End ()
{
	AssertIf (m_Block == -1);
	size_t position = GetPosition ();
	OutOfBoundsError (position, 0);

	m_Cacher->UnlockCacheBlock (m_Block);
	m_Block = -1;
	return position;
}

void CachedReader::GetStreamingInfo (size_t offset, size_t size, StreamingInfo* streamingInfo)
{
	Assert (m_ActiveResourceImage != NULL || m_ActiveResourceImage == m_ResourceImageGroup.resourceImages[kStreamingResourceImage]);

	streamingInfo->offset = offset;
	streamingInfo->size = size;
	streamingInfo->path = m_ActiveResourceImage->GetStreamingPath();
}

CachedReader::~CachedReader ()
{
	AssertIf (m_Block != -1);
}

UInt8* CachedReader::FetchResourceImageData (size_t offset, size_t size)
{
	if (m_ActiveResourceImage == NULL)
	{
		ErrorString("Resource image for '" + m_Cacher->GetPathName() + "' couldn't be loaded!");
		return NULL;
	}

	return m_ActiveResourceImage->Fetch (offset, size);
}

ResourceImage::ResourceImage (const std::string& path, bool streaming)
{
	if (!streaming)
	{
		m_Size = GetFileLength(path);
		m_Data = static_cast<UInt8*> (UNITY_MALLOC(kMemResource, m_Size));

		if (!ReadFromFile(path, m_Data, 0, m_Size))
		{
			ErrorString("Resource image couldn't be loaded completely");
		}
	}
	else
	{
		m_StreamingPath = path;
	}
}

ResourceImage::~ResourceImage ()
{
	if (m_Data)
		UNITY_FREE(kMemResource, m_Data);
}

void CachedReader::Read (void* data, size_t size)
{
	if (m_CachePosition + size <= m_CacheEnd)
	{
		memcpy (data, m_CachePosition, size);
		m_CachePosition += size;
	}
	else
	{
		// Read some data directly if it is coming in big chunks and we are not hitting the end of the file!
		size_t position = GetPosition ();
		OutOfBoundsError (position, size);

		if (m_OutOfBoundsRead)
		{
			memset(data, 0, size);
			return;
		}

		// Read enough bytes from the cache to align the position with the cache size
		if (position % m_CacheSize != 0)
		{
			int blockEnd = ((position / m_CacheSize) + 1) * m_CacheSize;
			int curReadSize = min<int> (size, blockEnd - position);
			UpdateReadCache (data, curReadSize);
			(UInt8*&)data += curReadSize;
			size -= curReadSize;
			position += curReadSize;
		}

		// If we have a big block of data read directly without a cache, all aligned reads
		int physicallyLimitedSize = min ((position + size), m_Cacher->GetFileLength ()) - position;
		int blocksToRead = physicallyLimitedSize / m_CacheSize;
		if (blocksToRead > 0)
		{
			int curReadSize = blocksToRead * m_CacheSize;
			m_Cacher->DirectRead ((UInt8*)data, position, curReadSize);
			m_CachePosition += curReadSize;
			(UInt8*&)data += curReadSize;
			size -= curReadSize;
		}

		// Read the rest of the data from the cache!
		while (size != 0)
		{
			int curReadSize = min<int> (size, m_CacheSize);
			UpdateReadCache (data, curReadSize);
			(UInt8*&)data += curReadSize;
			size -= curReadSize;
		}
	}
}

void CachedReader::Skip (int size)
{
	if (m_CachePosition + size <= m_CacheEnd)
	{
		m_CachePosition += size;
	}
	else
	{
		int position = GetPosition ();
		SetPosition(position + size);
	}
}

void memcpy_constrained_src (void* dst, const void* src, int size, const void* srcFrom, void* srcTo);
void memcpy_constrained_dst (void* dst, const void* src, int size, const void* dstFrom, void* dstTo);

void memcpy_constrained_src (void* dst, const void* src, int size, const void* srcFrom, void* srcTo)
{
	UInt8* fromClamped = clamp ((UInt8*)src, (UInt8*)srcFrom, (UInt8*)srcTo);
	UInt8* toClamped = clamp ((UInt8*)src + size, (UInt8*)srcFrom, (UInt8*)srcTo);

	int offset = fromClamped - (UInt8*)src;
	size = toClamped - fromClamped;
	memcpy ((UInt8*)dst + offset, (UInt8*)src + offset, size);
}

void memcpy_constrained_dst (void* dst, const void* src, int size, const void* dstFrom, void* dstTo)
{
	UInt8* fromClamped = clamp ((UInt8*)dst, (UInt8*)dstFrom, (UInt8*)dstTo);
	UInt8* toClamped = clamp ((UInt8*)dst + size, (UInt8*)dstFrom, (UInt8*)dstTo);

	int offset = fromClamped - (UInt8*)dst;
	size = toClamped - fromClamped;
	memcpy ((UInt8*)dst + offset, (UInt8*)src + offset, size);
}

void CachedReader::UpdateReadCache (void* data, size_t size)
{
	AssertIf (m_Cacher == NULL);
	AssertIf (size > m_CacheSize);

	size_t position = GetPosition ();
	OutOfBoundsError(position, size);

	if (m_OutOfBoundsRead)
	{
		memset(data, 0, size);
		return;
	}

	// copy data oldblock
	SetPosition (position);
	memcpy_constrained_src (data, m_CachePosition, size, m_CacheStart, m_CacheEnd);

	// Read next cache block only if we actually need it.
	if (m_CachePosition + size > m_CacheEnd)
	{
		// Check if the cache block
		// copy data new block
		SetPosition (position + size);
		UInt8* cachePosition = position - m_Block * m_CacheSize + m_CacheStart;
		memcpy_constrained_src (data, cachePosition, size, m_CacheStart, m_CacheEnd);
	}
	else
	{
		m_CachePosition += size;
	}
}

std::string GetNicePath (const CacheReaderBase& cacher)
{
	#if UNITY_EDITOR
	string path = cacher.GetPathName();
	string assetPath = AssetPathNameFromAnySerializedPath(GetProjectRelativePath(cacher.GetPathName()));

	if (!assetPath.empty())
		return cacher.GetPathName() + "\' - \'" +  assetPath;
	else
		return cacher.GetPathName();
	#else
	return cacher.GetPathName();
	#endif
}

void CachedReader::OutOfBoundsError (size_t position, size_t size)
{
	if (m_OutOfBoundsRead)
		return;

	#define ERROR_FMT	"The file \'%s\' is corrupted! Remove it and launch unity again!\n" 			\
						"[Position out of bounds! %" PRINTF_SIZET_FORMAT " %s %" PRINTF_SIZET_FORMAT "]"

	if (position + size > m_Cacher->GetFileLength ())
	{
		OUTPUT_FIELDS_OF_LAST_SERIALIZABLE;
		FatalErrorMsg (ERROR_FMT, GetNicePath(*m_Cacher).c_str(), position + size, ">", m_Cacher->GetFileLength ());
		m_OutOfBoundsRead = true;
	}

	if (position + size > m_MaximumPosition)
	{
		OUTPUT_FIELDS_OF_LAST_SERIALIZABLE;
		FatalErrorMsg (ERROR_FMT, GetNicePath(*m_Cacher).c_str(), position + size, ">", m_MaximumPosition);
		m_OutOfBoundsRead = true;
	}

	if (position < m_MinimumPosition)
	{
		OUTPUT_FIELDS_OF_LAST_SERIALIZABLE;
		FatalErrorMsg (ERROR_FMT, GetNicePath(*m_Cacher).c_str(), position + size, "<", m_MinimumPosition);
		m_OutOfBoundsRead = true;
	}
}

void CachedReader::SetPosition (size_t position)
{
	OutOfBoundsError(position, 0);
	if (m_OutOfBoundsRead)
		return;

	if (position / m_CacheSize != m_Block)
	{
		m_Cacher->UnlockCacheBlock (m_Block);
		m_Block = position / m_CacheSize;
		m_Cacher->LockCacheBlock (m_Block, &m_CacheStart, &m_CacheEnd);
	}
	m_CachePosition = position - m_Block * m_CacheSize + m_CacheStart;
}

void CachedReader::Align4Read ()
{
	UInt32 offset = m_CachePosition - m_CacheStart;
	offset = ((offset + 3) >> 2) << 2;
	m_CachePosition = m_CacheStart + offset;
}


//////

void CachedWriter::InitActiveWriter (ActiveWriter& activeWriter, CacheWriterBase& cacher)
{
	Assert (activeWriter.block == -1);
	Assert (&cacher != NULL);

	activeWriter.cacheBase = &cacher;
	activeWriter.block = 0;
	activeWriter.cacheBase->LockCacheBlock (activeWriter.block, &activeWriter.cacheStart, &activeWriter.cacheEnd);
	activeWriter.cachePosition = activeWriter.cacheStart;
}

void CachedWriter::Align4Write ()
{
	UInt32 leftOver = Align4LeftOver (m_ActiveWriter.cachePosition - m_ActiveWriter.cacheStart);
	UInt8 value = 0;
	for (UInt32 i=0;i<leftOver;i++)
		Write(value);
}

void CachedWriter::Write (const void* data, size_t size)
{
	if (m_ActiveWriter.cachePosition + size < m_ActiveWriter.cacheEnd)
	{
		memcpy (m_ActiveWriter.cachePosition, data, size);
		m_ActiveWriter.cachePosition += size;
	}
	else
	{
		while (size != 0)
		{
			size_t curWriteSize = min (size, m_ActiveWriter.cacheBase->GetCacheSize());
			UpdateWriteCache (data, curWriteSize);
			(UInt8*&)data += curWriteSize;
			size -= curWriteSize;
		}
	}
}

void CachedWriter::UpdateWriteCache (const void* data, size_t size)
{
	Assert (m_ActiveWriter.cacheBase != NULL);
	AssertIf (size > m_ActiveWriter.cacheBase->GetCacheSize());

	size_t position = GetPosition ();
	size_t cacheSize = m_ActiveWriter.cacheBase->GetCacheSize();
	// copy data from oldblock
	memcpy_constrained_dst (m_ActiveWriter.cachePosition, data, size, m_ActiveWriter.cacheStart, m_ActiveWriter.cacheEnd);

	SetPosition (position + size);

	// copy data new block
	UInt8* cachePosition = position - m_ActiveWriter.block * cacheSize + m_ActiveWriter.cacheStart;
	memcpy_constrained_dst (cachePosition, data, size, m_ActiveWriter.cacheStart, m_ActiveWriter.cacheEnd);
}

void CachedWriter::SetPosition (size_t position)
{
	size_t cacheSize = m_ActiveWriter.cacheBase->GetCacheSize();
	int newBlock = position / cacheSize;
	if (newBlock != m_ActiveWriter.block)
	{
		Assert(newBlock == m_ActiveWriter.block + 1);

		m_ActiveWriter.cacheBase->UnlockCacheBlock (m_ActiveWriter.block);
		m_ActiveWriter.block = newBlock;
		m_ActiveWriter.cacheBase->LockCacheBlock (m_ActiveWriter.block, &m_ActiveWriter.cacheStart, &m_ActiveWriter.cacheEnd);
	}
	m_ActiveWriter.cachePosition = position - m_ActiveWriter.block * cacheSize + m_ActiveWriter.cacheStart;
}

size_t CachedWriter::GetPosition () const
{
	return m_ActiveWriter.GetPosition();
}


bool CachedWriter::CompleteWriting ()
{
	m_ActiveWriter.cacheBase->UnlockCacheBlock (m_ActiveWriter.block);

	bool success = m_ActiveWriter.cacheBase->CompleteWriting (m_ActiveWriter.GetPosition());

	#if UNITY_EDITOR
	if (m_ActiveResourceImageMode != kResourceImageNotSupported)
	{
		for (int i=0;i<kNbResourceImages;i++)
		{
			success &= m_ResourceImageWriters[i].cacheBase->CompleteWriting (m_ResourceImageWriters[i].GetPosition());
			success &= m_ResourceImageWriters[i].cacheBase->WriteHeaderAndCloseFile (NULL, 0, 0);
		}
	}
	#endif

	return success;
}

size_t CachedWriter::ActiveWriter::GetPosition () const
{
	return cachePosition - cacheStart + block * cacheBase->GetCacheSize();
}

#if UNITY_EDITOR

void CachedWriter::InitWrite (CacheWriterBase& cacher)
{
	InitActiveWriter(m_ActiveWriter, cacher);
	m_DefaultWriter = m_ActiveWriter;
	m_ActiveResourceImageMode = kResourceImageNotSupported;
}

void CachedWriter::InitResourceImage (ActiveResourceImage index, CacheWriterBase& resourceImage)
{
	m_ActiveResourceImageMode = kResourceImageInactive;
	InitActiveWriter(m_ResourceImageWriters[index], resourceImage);
}

void CachedWriter::BeginResourceImage (ActiveResourceImage resourceImage)
{
	if (m_ActiveResourceImageMode == kResourceImageNotSupported)
		return;

	Assert(m_ActiveResourceImageMode == kResourceImageInactive);
	Assert(resourceImage > kResourceImageInactive);

	m_ActiveResourceImageMode = resourceImage;

	m_DefaultWriter = m_ActiveWriter;
	m_ActiveWriter = m_ResourceImageWriters[m_ActiveResourceImageMode];
	Assert(m_ActiveWriter.cacheBase != NULL);
}

void CachedWriter::EndResourceImage ()
{
	Assert(IsWritingResourceImage());

	m_ResourceImageWriters[m_ActiveResourceImageMode] = m_ActiveWriter;
	m_ActiveWriter = m_DefaultWriter;

	m_ActiveResourceImageMode = kResourceImageInactive;
}

#else

void CachedWriter::InitWrite (CacheWriterBase& cacher)
{
	InitActiveWriter(m_ActiveWriter, cacher);
}

#endif
