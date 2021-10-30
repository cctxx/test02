#ifndef RUNTIME_CONTAINERS_RINGBUFFER_H
#define RUNTIME_CONTAINERS_RINGBUFFER_H

#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Allocator/tlsf/tlsfbits.h"
#include "Runtime/Threads/AtomicOps.h"

// --------------------------------------------------------------------
// Ringbuffer
// Concurrently supports one consumer and one producer
// --------------------------------------------------------------------
class Ringbuffer
{
public:
	Ringbuffer(void* memory, UInt32 size);
	Ringbuffer(MemLabelId label, UInt32 size);

	~Ringbuffer();

	void* WritePtr(UInt32* nBytes) const { return Ptr(m_Put, GetFreeSize(), nBytes); }
	void WritePtrUpdate(void* writePtr, UInt32 nBytesWritten);

	const void* ReadPtr(UInt32* nBytes) const { return Ptr(m_Get, GetAvailableSize(), nBytes); }
	void ReadPtrUpdate(const void* readPtr, UInt32 nBytesRead);

	UInt32 GetSize() const { return m_Size; }
	UInt32 GetFreeSize() const	{ return m_Size - GetAvailableSize(); }
	UInt32 GetAvailableSize() const { return m_Put - m_Get; }
	UInt32 GetAllocatedSize() const { return GetAvailableSize(); }

	void Reset() { m_Get = m_Put = 0; }

protected:
	void* Ptr(UInt32 position, UInt32 availableBytes, UInt32* nBytes) const;

protected:
	char*			m_Buffer;
	bool			m_OwnMemory;
	MemLabelId		m_OwnMemoryLabel;
	UInt32			m_Size;
	volatile UInt32	m_Get;
	volatile UInt32	m_Put;
};

// ---------------------------------------------------------------------------
inline Ringbuffer::Ringbuffer(void* memory, UInt32 size)
:	m_Get(0)
,	m_Put(0)
{
	m_Size = 1 << tlsf_fls(size);		// use the biggest 2^n size that fits
	m_Buffer = reinterpret_cast<char*>(memory);
	m_OwnMemory = false;
}

inline Ringbuffer::Ringbuffer(MemLabelId label, UInt32 size)
:	m_Get(0)
,	m_Put(0)
{
	AssertBreak(size >> 31 == 0);
	m_Size = 1 << tlsf_fls(size*2 - 1);	// make sure we have _at least_ 'size' bytes
	m_Buffer = reinterpret_cast<char*>(UNITY_MALLOC(label, m_Size));
	m_OwnMemory = true;
	m_OwnMemoryLabel = label;
}

inline Ringbuffer::~Ringbuffer()
{
	if (m_OwnMemory)
		UNITY_FREE(m_OwnMemoryLabel, m_Buffer);
}

FORCE_INLINE void Ringbuffer::WritePtrUpdate(void* writePtr, UInt32 nBytesWritten)
{
	int result = AtomicAdd((volatile int*)&m_Put, nBytesWritten);
	AssertBreak(writePtr == &m_Buffer[(result - nBytesWritten) & (m_Size - 1)]);
}

FORCE_INLINE void Ringbuffer::ReadPtrUpdate(const void* readPtr, UInt32 nBytesRead)
{
	int result = AtomicAdd((volatile int*)&m_Get, nBytesRead);
	AssertBreak(readPtr == &m_Buffer[(result - nBytesRead) & (m_Size - 1)]);
}

FORCE_INLINE void* Ringbuffer::Ptr(UInt32 position, UInt32 bytesAvailable, UInt32* nBytesPtr) const
{
	UInt32& nBytes = *nBytesPtr;
	UInt32 index = position & (m_Size - 1);
	UInt32 spaceUntilEnd = m_Size - index;
	UInt32 continousBytesAvailable = std::min(bytesAvailable, spaceUntilEnd);
	nBytes = std::min(nBytes, continousBytesAvailable);
	return &m_Buffer[index];
}

#endif // RUNTIME_CONTAINERS_RINGBUFFER_H
