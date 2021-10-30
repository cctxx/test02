#ifndef THREADED_STREAM_BUFFER_H
#define THREADED_STREAM_BUFFER_H

#if SUPPORT_THREADS

#include "Runtime/Utilities/NonCopyable.h"
#include "Runtime/Utilities/Utility.h"
#include <new> // for placement new
#include "Runtime/Threads/Thread.h"

class Mutex;
class Semaphore;

/// A single producer, single consumer ringbuffer

/// Most read and write operations are done even without atomic operations and use no expensive synchronization primitives
/// Each thread owns a part of the buffer and only locks when reaching the end


/// *** Common usage *** 
/// * Create the ring buffer
/// ThreadedStreamBuffer buffer (kModeThreaded);

/// * Producer thread...
/// buffer.WriteValueType<int>(5);
/// buffer.WriteValueType<int>(7);
/// buffer.WriteSubmitData();

/// * ConsumerThread...
/// print(buffer.ReadValueType<int>());
/// print(buffer.ReadValueType<int>());
/// buffer.ReadReleaseData();

class ThreadedStreamBuffer : public NonCopyable
{
public:

	struct BufferState
	{
		// These should not be size_t, as the GfxDevice may run across processes of different
		// bitness, and the data serialized in the command buffer must match.
		void Reset() volatile;
		volatile UInt32 bufferPos;
		volatile UInt32 bufferEnd;
		volatile UInt32 bufferWraps;
		volatile UInt32 checkedPos;
		volatile UInt32 checkedWraps;
#if !UNITY_RELEASE
		volatile UInt32 totalBytes;
#endif
	};

	struct BufferHeader
	{
		BufferState reader;
		BufferState writer;
	};
	
	typedef unsigned size_t;

	enum Mode
	{
		// This is the most common usage. One producer, one consumer on different threads.
		kModeThreaded,

		// When in read mode, we pass a pointer to the external data which can then be read using ReadValueType and ReadReleaseData.
		kModeReadOnly,
		// When in growable you are only allowed to write into the ring buffer. Essentially like a std::vector. It will keep on growing as you write data.
		kModeGrowable,
		kModeCrossProcess,
	};

	ThreadedStreamBuffer(Mode mode, size_t size);
	ThreadedStreamBuffer();
	~ThreadedStreamBuffer();

	enum
	{
		kDefaultAlignment = 4,
		kDefaultStep = 4096
	};
	
	// Read data from the ringbuffer
	// This function blocks until data new data has arrived in the ringbuffer.
	// It uses semaphores to wait on the producer thread in a efficient way.
	template <class T> const T&	ReadValueType();
	template <class T> T*		ReadArrayType(int count);
	// ReadReleaseData should be called when the data has been read & used completely.
	// At this point the memory will become available to the producer to write into it again.
	void						ReadReleaseData();
	
	// Write data into the ringbuffer
	template <class T> void		WriteValueType(const T& val);
	template <class T> void		WriteArrayType(const T* vals, int count);
	template <class T> T*		GetWritePointer();
	// WriteSubmitData should be called after data has been completely written and should be made available to the consumer thread to read it.
	// Before WriteSubmitData is called, any data written with WriteValueType can not be read by the consumer.
	void						WriteSubmitData();

	// Ringbuffer Streaming support. This will automatically call WriteSubmitData & ReadReleaseData.
	// It splits the data into smaller chunks (step). So that the size of the ringbuffer can be smaller than the data size passed into this function.
	// The consumer thread will be reading the streaming data while WriteStreamingData is still called on the producer thread.
	void						ReadStreamingData(void* data, size_t size, size_t alignment = kDefaultAlignment, size_t step = kDefaultStep);
	void						WriteStreamingData(const void* data, size_t size, size_t alignment = kDefaultAlignment, size_t step = kDefaultStep);

	
	// Utility functions
	void*	GetReadDataPointer(size_t size, size_t alignment);
	void*	GetWriteDataPointer(size_t size, size_t alignment);

	size_t	GetDebugReadPosition() const { return m_Reader->bufferPos; }
	size_t	GetDebugWritePosition() const { return m_Writer->bufferPos; }

	double	GetReadWaitTime() const { return m_ReadWaitTime; }
	double	GetWriteWaitTime() const { return m_WriteWaitTime; }
	void	ResetReadWaitTime() { m_ReadWaitTime = 0.0; }
	void	ResetWriteWaitTime() { m_WriteWaitTime = 0.0; }

	
	// Creation methods
	void	Create(Mode mode, size_t size);
	void	CreateReadOnly(const void* buffer, size_t size);
	void	CreateFromMemory(Mode mode, size_t size, void *buffer);
	void	ResetGrowable();
	void	Destroy();
	
	// 
	// Is there data available to be read
	// typicall this is not used
	bool	HasData() const;
	bool	HasDataToRead() const;
	
	size_t	GetAllocatedSize() const { return m_BufferSize; }
	size_t	GetCurrentSize() const;
	const void*	GetBuffer() const;
	
	
	////@TODO: Remove this
	typedef void (*DataConsumer)(const void* buffer, size_t bufferSize, void* userData);
	size_t						ReadStreamingData(DataConsumer consumer, void* userData, size_t alignment = kDefaultAlignment, size_t step = kDefaultStep);
	
	typedef bool (*DataProvider)(void* dest, size_t bufferSize, size_t& bytesWritten, void* userData);
	void						WriteStreamingData(DataProvider provider, void* userData, size_t alignment = kDefaultAlignment, size_t step = kDefaultStep);
	
	
private:
	FORCE_INLINE size_t Align(size_t pos, size_t alignment) const { return (pos+alignment-1)&~(alignment-1); }

	void	SetDefaults();

	void	HandleReadOverflow(size_t& dataPos, size_t& dataEnd);
	void	HandleWriteOverflow(size_t& dataPos, size_t& dataEnd);

	void	SendReadSignal();
	void	SendWriteSignal();

	Mode m_Mode;
	char* m_Buffer;
	size_t m_BufferSize;
	size_t m_GrowStepSize;
	BufferState *m_Reader;
	BufferState *m_Writer;
	BufferHeader m_Header;
	Mutex* m_Mutex;
	Semaphore* m_ReadSemaphore;
	Semaphore* m_WriteSemaphore;
	volatile int m_NeedsReadSignal;
	volatile int m_NeedsWriteSignal;
	double m_ReadWaitTime;
	double m_WriteWaitTime;
};

FORCE_INLINE bool ThreadedStreamBuffer::HasData() const
{
	return (m_Reader->bufferPos != m_Writer->checkedPos);
}

FORCE_INLINE void* ThreadedStreamBuffer::GetReadDataPointer(size_t size, size_t alignment)
{
	size = Align(size, alignment);
	size_t dataPos = Align(m_Reader->bufferPos, alignment);
	size_t dataEnd = dataPos + size;
	if (dataEnd > m_Reader->bufferEnd)
	{
		HandleReadOverflow(dataPos, dataEnd);
	}
	m_Reader->bufferPos = dataEnd;
#if !UNITY_RELEASE
	m_Reader->totalBytes += size;
#endif
	return &m_Buffer[dataPos];
}

FORCE_INLINE void* ThreadedStreamBuffer::GetWriteDataPointer(size_t size, size_t alignment)
{
	size = Align(size, alignment);
	Assert(size*2 <= m_BufferSize || m_Mode == kModeGrowable);
	size_t dataPos = Align(m_Writer->bufferPos, alignment);
	size_t dataEnd = dataPos + size;
	if (dataEnd > m_Writer->bufferEnd)
	{
		HandleWriteOverflow(dataPos, dataEnd);
	}
	m_Writer->bufferPos = dataEnd;
#if !UNITY_RELEASE
	m_Writer->totalBytes += size;
#endif
	return &m_Buffer[dataPos];
}

template <class T> FORCE_INLINE const T& ThreadedStreamBuffer::ReadValueType()
{
	// Read simple data type from queue
	const void* pdata = GetReadDataPointer(sizeof(T), ALIGN_OF(T));
	const T& src = *reinterpret_cast<const T*>(pdata);
	return src;
}

template <class T> FORCE_INLINE T* ThreadedStreamBuffer::ReadArrayType(int count)
{
	// Read array of data from queue-
	void* pdata = GetReadDataPointer(count * sizeof(T), ALIGN_OF(T));
	T* src = reinterpret_cast<T*>(pdata);
	return src;
}

template <class T> FORCE_INLINE void ThreadedStreamBuffer::WriteValueType(const T& val)
{
	// Write simple data type to queue
	void* pdata = GetWriteDataPointer(sizeof(T), ALIGN_OF(T));
	new (pdata) T(val);
}

template <class T> FORCE_INLINE void ThreadedStreamBuffer::WriteArrayType(const T* vals, int count)
{
	// Write array of data to queue
	T* pdata = (T*)GetWriteDataPointer(count * sizeof(T), ALIGN_OF(T));
	for (int i = 0; i < count; i++)
		new (&pdata[i]) T(vals[i]);
}

template <class T> FORCE_INLINE T* ThreadedStreamBuffer::GetWritePointer()
{
	// Write simple data type to queue
	void* pdata = GetWriteDataPointer(sizeof(T), ALIGN_OF(T));
	return static_cast<T*>(pdata);
}

#endif
#endif
