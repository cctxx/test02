#include "UnityPrefix.h"

#if SUPPORT_THREADS

#include "ThreadedStreamBuffer.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Semaphore.h"
#include "Runtime/Threads/ThreadUtility.h"
#include "Runtime/Threads/AtomicOps.h"

#if ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
#include <ppapi/cpp/instance.h>
#include "External/NPAPI2NaCl/Common/UnityInterfaces.h"
#include "PlatformDependent/PepperPlugin/UnityInstance.h"
#endif

double GetTimeSinceStartup();

ThreadedStreamBuffer::ThreadedStreamBuffer()
{
	SetDefaults();
}

ThreadedStreamBuffer::ThreadedStreamBuffer(Mode mode, size_t size)
{
	SetDefaults();
	Create(mode, size);
}

ThreadedStreamBuffer::~ThreadedStreamBuffer()
{
	Destroy();
}

void ThreadedStreamBuffer::CreateFromMemory(Mode mode, size_t size, void *buffer)
{
	Assert(mode != kModeReadOnly);
	Assert(m_Buffer == NULL);
	m_Mode = mode;
	BufferHeader *header = (BufferHeader*)buffer;
	m_Reader = &header->reader;
	m_Writer = &header->writer;
	m_Buffer = (char*)buffer + sizeof(BufferHeader);
	m_BufferSize = size - sizeof(BufferHeader);
	m_Reader->Reset();
	m_Writer->Reset();
	m_Writer->bufferEnd = size;
	if (m_Mode == kModeThreaded)
	{
		m_Mutex = new Mutex;
		m_ReadSemaphore = new Semaphore;
		m_WriteSemaphore = new Semaphore;
	}
}

void ThreadedStreamBuffer::Create(Mode mode, size_t size)
{
	m_Reader = &m_Header.reader;
	m_Writer = &m_Header.writer;
	Assert(mode != kModeReadOnly);
	Assert(m_Buffer == NULL);
	m_Mode = mode;
	if (size != 0)
		m_Buffer = (char*)UNITY_MALLOC(kMemUtility, size);
	m_BufferSize = size;
	m_Reader->Reset();
	m_Writer->Reset();
	m_Writer->bufferEnd = size;
	if (m_Mode == kModeThreaded)
	{
		m_Mutex = new Mutex;
		m_ReadSemaphore = new Semaphore;
		m_WriteSemaphore = new Semaphore;
	}
}

void ThreadedStreamBuffer::CreateReadOnly(const void* buffer, size_t size)
{
	m_Reader = &m_Header.reader;
	m_Writer = &m_Header.writer;
	m_Mode = kModeReadOnly;
	m_Buffer = (char*)buffer;
	m_BufferSize = size;
	m_Reader->Reset();
	m_Writer->Reset();
	m_Reader->bufferEnd = size;
}

void ThreadedStreamBuffer::ResetGrowable()
{
	Assert(m_Mode == kModeGrowable);
	m_Reader->Reset();
	m_Writer->Reset();
	m_Writer->bufferEnd = m_BufferSize;
}

void ThreadedStreamBuffer::Destroy()
{
	if (m_Buffer == NULL) return;
	if (m_Mode != kModeReadOnly)
		UNITY_FREE(kMemUtility, m_Buffer);
	m_Reader->Reset();
	m_Writer->Reset();
	if (m_Mode == kModeThreaded)
	{
		delete m_Mutex;
		delete m_ReadSemaphore;
		delete m_WriteSemaphore;
	}
	SetDefaults();
}

ThreadedStreamBuffer::size_t ThreadedStreamBuffer::GetCurrentSize() const
{
	Assert(m_Mode != kModeThreaded);
	if (m_Mode == kModeGrowable)
	{
		return m_Writer->bufferPos;
	}
	return m_BufferSize;
}

const void*	ThreadedStreamBuffer::GetBuffer() const
{
	//Assert(m_Mode != kModeThreaded);
	return m_Buffer;
}

void ThreadedStreamBuffer::ReadStreamingData(void* data, size_t size, size_t alignment, size_t step)
{
	Assert((step % alignment) == 0);

	// This should not be size_t, as the GfxDevice may run across processes of different
	// bitness, and the data serialized in the command buffer must match.
	size_t sz = ReadValueType<UInt32>();
	Assert(sz == size);

	char* dest = (char*)data;
	for (size_t offset = 0; offset < size; offset += step)
	{
		size_t bytes = std::min(size - offset, step);
		const void* src = GetReadDataPointer(bytes, alignment);
		if (data)
			UNITY_MEMCPY(dest, src, bytes);

		int magic = ReadValueType<int>();
		Assert(magic == 1234);

		ReadReleaseData();
		dest += step;
	}
}

ThreadedStreamBuffer::size_t ThreadedStreamBuffer::ReadStreamingData(DataConsumer consumer, void* userData, size_t alignment, size_t step)
{
	Assert((step % alignment) == 0);
	Assert(consumer != NULL);

	size_t totalBytesRead = 0;

	bool moreData = false;
	do
	{
		const void* src = GetReadDataPointer(step + sizeof(size_t), alignment);
		const size_t bytesInBuffer = *(static_cast<const size_t*>(src));
		consumer(static_cast<const size_t*>(src) + 1, bytesInBuffer, userData);
		totalBytesRead += bytesInBuffer;
		int magic = ReadValueType<int>();
		Assert(magic == 1234);
		moreData = ReadValueType<bool>();
		ReadReleaseData();
	} while (moreData);

	return totalBytesRead;
}

void ThreadedStreamBuffer::ReadReleaseData()
{
	UnityMemoryBarrier();
	if (m_Reader->checkedWraps == m_Reader->bufferWraps)
	{
		// We only update the position
		m_Reader->checkedPos = m_Reader->bufferPos;
	}
	else
	{
		// Both values need to be set atomically
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		Mutex::AutoLock lock(*m_Mutex);
#endif
		m_Reader->checkedPos = m_Reader->bufferPos;
		m_Reader->checkedWraps = m_Reader->bufferWraps;
	}
	UnityMemoryBarrier();
	SendReadSignal();
}

void ThreadedStreamBuffer::WriteStreamingData(const void* data, size_t size, size_t alignment, size_t step)
{	
	// This should not be size_t, as the GfxDevice may run across processes of different
	// bitness, and the data serialized in the command buffer must match.
	WriteValueType<UInt32>(size);
	Assert((step % alignment) == 0);

	const char* src = (const char*)data;
	for (size_t offset = 0; offset < size; offset += step)
	{
		size_t bytes = std::min(size - offset, step);
		void* dest = GetWriteDataPointer(bytes, alignment);
		UNITY_MEMCPY(dest, src, bytes);
		WriteValueType<int>(1234);
		
		// In the NaCl Web Player, make sure that only complete commands are submitted, as we are not truely
		// asynchronous.
		#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		WriteSubmitData();
		#endif
		
		src += step;
	}
	WriteSubmitData();
}

void ThreadedStreamBuffer::WriteStreamingData(DataProvider provider, void* userData, size_t alignment, size_t step)
{
	Assert((step % alignment) == 0);
	Assert(provider != NULL);

	bool moreData = false;
	do
	{
		void* dest = GetWriteDataPointer(step + sizeof(size_t), alignment);
		size_t outBytesWritten = 0;
		moreData = provider(static_cast<size_t*>(dest) + 1, step, outBytesWritten, userData);
		*((size_t*)dest) = outBytesWritten;
		WriteValueType<int>(1234);
		WriteValueType(moreData);
		
		// In the NaCl Web Player, make sure that only complete commands are submitted, as we are not truely
		// asynchronous.
		#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		WriteSubmitData();
		#endif
	} while (moreData);
	
	WriteSubmitData();
}

void ThreadedStreamBuffer::WriteSubmitData()
{
	UnityMemoryBarrier();
	if (m_Writer->checkedWraps == m_Writer->bufferWraps)
	{
		// We only update the position
		m_Writer->checkedPos = m_Writer->bufferPos;
	}
	else
	{
		// Both values need to be set atomically
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		Mutex::AutoLock lock(*m_Mutex);
#endif
		m_Writer->checkedPos = m_Writer->bufferPos;
		m_Writer->checkedWraps = m_Writer->bufferWraps;
	}
	UnityMemoryBarrier();
	SendWriteSignal();
}

void ThreadedStreamBuffer::SetDefaults()
{
	m_Mode = kModeReadOnly;
	m_Buffer = NULL;
	m_BufferSize = 0;
	m_GrowStepSize = 8*1024;
	m_Mutex = NULL;
	m_ReadSemaphore = NULL;
	m_WriteSemaphore = NULL;
	m_NeedsReadSignal = 0;
	m_NeedsWriteSignal = 0;
	m_ReadWaitTime = 0.0;
	m_WriteWaitTime = 0.0;
};

bool ThreadedStreamBuffer::HasDataToRead() const
{
	if (m_Reader->bufferWraps == m_Writer->checkedWraps)
	{
		return (m_Reader->bufferPos < m_Writer->checkedPos) || (m_Reader->bufferPos < m_Reader->bufferEnd);
	}
	else
		return true;
}

void ThreadedStreamBuffer::HandleReadOverflow(size_t& dataPos, size_t& dataEnd)
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	Assert(m_Mode == kModeThreaded);
	Assert(m_Mutex != NULL);
	Mutex::AutoLock lock(*m_Mutex);
#endif

	if (dataEnd > m_BufferSize)
	{
		dataEnd -= dataPos;
		dataPos = 0;
		m_Reader->bufferPos = 0;
		m_Reader->bufferWraps++;
	}

	for (;;)
	{
		// Get how many buffer lengths writer is ahead of reader
		// This may be -1 if we are waiting for the writer to wrap
		size_t comparedPos = m_Writer->checkedPos;
		size_t comparedWraps = m_Writer->checkedWraps;
		size_t wrapDist = comparedWraps - m_Reader->bufferWraps;
		m_Reader->bufferEnd = (wrapDist == 0) ? comparedPos : (wrapDist == 1) ? m_BufferSize : 0;

		if (dataEnd <= m_Reader->bufferEnd)
		{
			break;
		}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		AtomicIncrement(&m_NeedsWriteSignal);
		UnityMemoryBarrier();
		
		m_Mutex->Unlock();
		if (comparedPos != m_Writer->checkedPos || comparedWraps != m_Writer->checkedWraps)
		{
			// Writer position changed while we requested a signal
			// Request might be missed, so we signal ourselves to avoid deadlock
			SendWriteSignal();
		}
		SendReadSignal();
		// Wait for writer thread
		double startTime = GetTimeSinceStartup();
		m_WriteSemaphore->WaitForSignal();
		m_ReadWaitTime += GetTimeSinceStartup() - startTime;
		m_Mutex->Lock();
#endif
	}
}

void ThreadedStreamBuffer::HandleWriteOverflow(size_t& dataPos, size_t& dataEnd)
{
	if (m_Mode == kModeGrowable)
	{
		size_t dataSize = dataEnd - dataPos;
		size_t growSize = std::max(dataSize, m_GrowStepSize);
		m_BufferSize += growSize;
		m_Buffer = (char*)UNITY_REALLOC_(kMemUtility, m_Buffer, m_BufferSize);
		m_Writer->bufferEnd = m_BufferSize;
		return;
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	Assert(m_Mode == kModeThreaded);
	Assert(m_Mutex != NULL);

	Mutex::AutoLock lock(*m_Mutex);
#endif
	
	if (dataEnd > m_BufferSize)
	{
		dataEnd -= dataPos;
		dataPos = 0;
		m_Writer->bufferPos = 0;
		m_Writer->bufferWraps++;
	}

	for (;;)
	{
		UnityMemoryBarrier();

		// Get how many buffer lengths writer is ahead of reader
		// This may be 2 if we are waiting for the reader to wrap
		size_t comparedPos = m_Reader->checkedPos;
		size_t comparedWraps = m_Reader->checkedWraps;
		size_t wrapDist = m_Writer->bufferWraps - comparedWraps;
		m_Writer->bufferEnd = (wrapDist == 0) ? m_BufferSize : (wrapDist == 1) ? comparedPos : 0;

		if (dataEnd <= m_Writer->bufferEnd)
		{
			break;
		}
#if ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
		struct UNITY_GfxDevice_1_0 *gfxInterface = (UNITY_GfxDevice_1_0*)pp::Module::Get()->GetBrowserInterface(UNITY_GFXDEVICE_INTERFACE_1_0);
		gfxInterface->WaitForSignal(0);
#elif !ENABLE_GFXDEVICE_REMOTE_PROCESS
		AtomicIncrement(&m_NeedsReadSignal);
		UnityMemoryBarrier();
		
		m_Mutex->Unlock();
		if (comparedPos != m_Reader->checkedPos || comparedWraps != m_Reader->checkedWraps)
		{
			// Reader position changed while we requested a signal
			// Request might be missed, so we signal ourselves to avoid deadlock
			SendReadSignal();
		}
		SendWriteSignal();
		// Wait for reader thread
		double startTime = GetTimeSinceStartup();
		m_ReadSemaphore->WaitForSignal();
		m_WriteWaitTime += GetTimeSinceStartup() - startTime;
		m_Mutex->Lock();
#endif
	}
}

void ThreadedStreamBuffer::SendReadSignal()
{
	if (AtomicCompareExchange(&m_NeedsReadSignal, 0, 1) )
	{
		m_ReadSemaphore->Signal();
	}
}

void ThreadedStreamBuffer::SendWriteSignal()
{
	if (AtomicCompareExchange(&m_NeedsWriteSignal, 0, 1) )
	{
		m_WriteSemaphore->Signal();
	}
}
void ThreadedStreamBuffer::BufferState::Reset() volatile
{
	bufferPos = 0;
	bufferEnd = 0;
	bufferWraps = 0;
	checkedPos = 0;
	checkedWraps = 0;
#if !UNITY_RELEASE
	totalBytes = 0;
#endif
}

#endif
