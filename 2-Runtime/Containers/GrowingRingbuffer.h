#ifndef RUNTIME_CONTAINERS_GROWINGRINGBUFFER_H
#define RUNTIME_CONTAINERS_GROWINGRINGBUFFER_H

#include "Ringbuffer.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Threads/AtomicOps.h"

// --------------------------------------------------------------------
// A Ringbuffer with the ability to grow
// Concurrently supports one consumer and one producer
//
// The functions in this class should be synchronized with
// Ringbuffer to fit Ringbuffer template functions.
// --------------------------------------------------------------------
class GrowingRingbuffer
{
public:
	GrowingRingbuffer(MemLabelId label, UInt32 maxSize, UInt32 initialSize = 1024)
	: m_Label(label)
	, m_MinSize(initialSize)
	, m_MaxSize(maxSize)
	{
		InitializeBuffers();
	}

	~GrowingRingbuffer() { DeleteBuffers(); }

	void* WritePtr(UInt32* nBytes) const;
	void WritePtrUpdate(void* writePtr, UInt32 nBytesWritten);

	const void* ReadPtr(UInt32* nBytes) const;
	void ReadPtrUpdate(const void* readPtr, UInt32 nBytesRead);

	UInt32 GetAllocatedSize() const { return m_AllocatedSize; }
	UInt32 GetAvailableSize() const { return m_AvailableSize; }
	UInt32 GetFreeSize() const { return m_MaxSize - m_AvailableSize; }
	UInt32 GetSize() const { return m_MaxSize; }

	void Reset();

private:
	void InitializeBuffers();
	void DeleteBuffers();

	struct RingbufferLink:public Ringbuffer
	{
		RingbufferLink(MemLabelId label, UInt32 size) : Ringbuffer(label, size)	, next(NULL) { }
		RingbufferLink* volatile 	next;
	};

	UInt32						m_MaxSize;
	UInt32						m_MinSize;
	MemLabelId					m_Label;
	volatile UInt32				m_AllocatedSize;
	volatile UInt32				m_AvailableSize;
	RingbufferLink* volatile	m_ReadBuffer;
	RingbufferLink* volatile	m_WriteBuffer;
};

inline void GrowingRingbuffer::InitializeBuffers()
{
	m_WriteBuffer	= m_ReadBuffer = new RingbufferLink(m_Label, m_MinSize);
	m_AllocatedSize	= m_ReadBuffer->GetSize();
	m_AvailableSize = 0;
}

inline void GrowingRingbuffer::DeleteBuffers()
{
	RingbufferLink* buffer = m_ReadBuffer;
	while (buffer)
	{
		RingbufferLink* current = buffer;
		buffer = current->next;
		delete current;
	}
}

inline void GrowingRingbuffer::Reset()
{
	DeleteBuffers();
	InitializeBuffers();
}

FORCE_INLINE void* GrowingRingbuffer::WritePtr(UInt32* nBytes) const
{
	*nBytes = std::min(*nBytes, GetFreeSize());
	return m_WriteBuffer->WritePtr(nBytes);
}

FORCE_INLINE const void* GrowingRingbuffer::ReadPtr(UInt32* nBytes) const
{
	return m_ReadBuffer->ReadPtr(nBytes);
}

FORCE_INLINE void GrowingRingbuffer::WritePtrUpdate(void* writePtr, UInt32 nBytesWritten)
{
	m_WriteBuffer->WritePtrUpdate(writePtr, nBytesWritten);
	AtomicAdd((volatile int*) &m_AvailableSize, nBytesWritten);
	if (m_WriteBuffer->GetFreeSize() == 0 && GetFreeSize() > 0)
	{
		RingbufferLink* emptyBuffer = new RingbufferLink(m_Label, m_AllocatedSize);
		m_WriteBuffer->next = emptyBuffer;
		m_WriteBuffer = emptyBuffer;
		AtomicAdd((volatile int*) &m_AllocatedSize, emptyBuffer->GetSize());
	}
}

FORCE_INLINE void GrowingRingbuffer::ReadPtrUpdate(const void* readPtr, UInt32 nBytesRead)
{
	m_ReadBuffer->ReadPtrUpdate(readPtr, nBytesRead);
	AtomicSub((volatile int*) &m_AvailableSize, nBytesRead);
	if (m_ReadBuffer->next && m_ReadBuffer->GetAvailableSize() == 0)
	{
		AtomicSub((volatile int*) &m_AllocatedSize, m_ReadBuffer->GetSize());
		RingbufferLink* emptyBuffer = m_ReadBuffer;
		m_ReadBuffer = m_ReadBuffer->next;
		delete emptyBuffer;
	}
}

#endif // RUNTIME_CONTAINERS_GROWINGRINGBUFFER_H
