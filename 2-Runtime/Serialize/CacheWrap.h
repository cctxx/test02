#ifndef CACHEWRAP_H
#define CACHEWRAP_H

#include "Configuration/UnityConfigure.h"
#include "Runtime/Serialize/FileCache.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"

class CachedWriter
{
	struct ActiveWriter
	{
		UInt8*		cachePosition;
		UInt8*		cacheStart;
		UInt8*		cacheEnd;
		SInt32		block;
		CacheWriterBase* cacheBase;
		
		ActiveWriter () { cachePosition = NULL; cacheStart = NULL; cacheEnd = NULL; block = -1; cacheBase = NULL; }
		size_t GetPosition () const;
	};

	ActiveWriter  m_ActiveWriter;
	
	#if UNITY_EDITOR

	ActiveResourceImage m_ActiveResourceImageMode;

	ActiveWriter        m_DefaultWriter;
	ActiveWriter        m_ResourceImageWriters[kNbResourceImages];

	#endif
	
	static void InitActiveWriter (CachedWriter::ActiveWriter& activeWriter, CacheWriterBase& cacher);
	void SetPosition (size_t position);
	void EXPORT_COREMODULE UpdateWriteCache (const void* data, size_t size);

	public:
	
	void InitWrite (CacheWriterBase& cacher);

	bool CompleteWriting ();
	

#if UNITY_EDITOR
	void InitResourceImage (ActiveResourceImage index, CacheWriterBase& resourceImage);
	
	void BeginResourceImage (ActiveResourceImage resourceImageType);
	void EndResourceImage ();
	bool IsWritingResourceImage () { return m_ActiveResourceImageMode > kResourceImageInactive; }
	
	CacheWriterBase& GetCacheBase () { return *m_ActiveWriter.cacheBase; }
	
#endif
	
	template<class T>
	void Write (const T& data)
	{
#if CHECK_SERIALIZE_ALIGNMENT
		if (m_CheckSerializeAlignment)
		{
			SInt32 position = reinterpret_cast<SInt32>(m_ActiveWriter.cachePosition);
			SInt32 size = sizeof(T);
			SInt32 align = position % size;
			if (align != 0)
			{
				ErrorString("Alignment error ");
			}
		}
#endif
		
		if (m_ActiveWriter.cachePosition + sizeof (T) < m_ActiveWriter.cacheEnd)
		{
			*reinterpret_cast<T*> (m_ActiveWriter.cachePosition) = data;
			m_ActiveWriter.cachePosition += sizeof (T);
		}
		else
			UpdateWriteCache (&data, sizeof (data));
	}
	
	void Align4Write ();
	
	void Write (const void* data, size_t size);
	
	size_t GetPosition () const;
};

struct StreamingInfo
{
	size_t    offset;
	size_t    size;
	std::string path;
	
	bool IsValid () const { return !path.empty(); }
	
	StreamingInfo () { offset = 0; size = 0; }
};


struct ResourceImage
{
	UInt8*      m_Data;
	UInt32      m_Size;
	std::string m_StreamingPath;
	
	public:
	
	ResourceImage (const std::string& path, bool stream);
	~ResourceImage ();
	
	UInt8* Fetch (size_t offset, size_t size)
	{
		Assert(m_Data != NULL);
		Assert(size + offset <= m_Size);
		return m_Data + offset;
	}
	
	const std::string& GetStreamingPath () { Assert(!m_StreamingPath.empty()); return m_StreamingPath; }
};

struct EXPORT_COREMODULE ResourceImageGroup
{
	ResourceImage* resourceImages[kNbResourceImages];
	
	ResourceImageGroup () { memset(this, 0, sizeof(ResourceImageGroup)); }
};

class EXPORT_COREMODULE CachedReader
{
	private:
	
	UInt8*		m_CachePosition;
	UInt8*		m_CacheStart;
	UInt8*		m_CacheEnd;
	CacheReaderBase* m_Cacher;
	SInt32		m_Block;
	size_t	m_CacheSize;
	size_t	m_MinimumPosition;
	size_t	m_MaximumPosition;
	bool        m_OutOfBoundsRead;
	
	ResourceImage* m_ActiveResourceImage;
	ResourceImageGroup m_ResourceImageGroup;
	
	void UpdateReadCache (void* data, size_t size);

	CachedReader (const  CachedReader& c);// undefined
	CachedReader& operator = (const  CachedReader& c);// undefined

	void OutOfBoundsError (size_t position, size_t size);
	void LockCacheBlockBounded();

	public:
	
	CachedReader ();
	~CachedReader ();
				
	void InitRead (CacheReaderBase& cacher, size_t position, size_t size);
	void InitResourceImages (ResourceImageGroup& resourceImage);
	
	size_t GetEndPosition () { return m_MaximumPosition; }
	
	size_t End ();
	
	template<class T>
	void Skip ()
	{
		m_CachePosition += sizeof (T);
	}	
	
	void Skip (int size);
	
	UInt8* FetchResourceImageData (size_t offset, size_t size);
	
	void GetStreamingInfo (size_t offset, size_t size, StreamingInfo* streamingInfo);
	
	void BeginResourceImage (ActiveResourceImage index) { m_ActiveResourceImage = m_ResourceImageGroup.resourceImages[index]; }
	void EndResourceImage () { m_ActiveResourceImage = NULL; }
	bool IsReadingResourceImage () { return m_ActiveResourceImage != NULL; }
	const char* GetSerializedFilePathName() { return m_Cacher->GetPathName().c_str(); }
	
	template<class T>
	void Read (T& data, size_t position)
	{
		m_CachePosition = m_CacheStart + position - m_Block * m_CacheSize;
		if (m_CachePosition >= m_CacheStart && m_CachePosition + sizeof (data) <= m_CacheEnd)
		{
			data = *reinterpret_cast<T*> (m_CachePosition);
			m_CachePosition += sizeof (T);
		}
		else
			UpdateReadCache (&data, sizeof (data));
	}

	template<class T>	
	void Read (T& data)
	{
		if (m_CachePosition + sizeof (T) <= m_CacheEnd)
		{
			data = *reinterpret_cast<T*> (m_CachePosition);
			m_CachePosition += sizeof (T);
		}
		else
			UpdateReadCache (&data, sizeof (data));
 	}
	
	void Align4Read ();
	
	size_t GetPosition ()	const							{ return m_CachePosition - m_CacheStart + m_Block * m_CacheSize; }
	void SetPosition (size_t position);
	void SetAbsoluteMemoryPosition(UInt8* position) { m_CachePosition = position; }
	UInt8* GetAbsoluteMemoryPosition() { return m_CachePosition; }

	void Read (void* data, size_t size);
	
	CacheReaderBase*	GetCacher () const { return m_Cacher; }
};

inline UInt32 Align4 (UInt32 size)
{
	UInt32 value = ((size + 3) >> 2) << 2;
	return value;
}

inline UInt32 Align4LeftOver (UInt32 size)
{
	return Align4(size) - size;
}

#endif
