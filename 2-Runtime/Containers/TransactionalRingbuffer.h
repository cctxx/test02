#ifndef RUNTIME_CONTAINERS_TRANSACTIONALRINGBUFFER_H
#define RUNTIME_CONTAINERS_TRANSACTIONALRINGBUFFER_H

#include "Ringbuffer.h"

// --------------------------------------------------------------------
// Transactional Ringbuffer (commit, reset)
// Concurrently supports one consumer and one producer
// --------------------------------------------------------------------
class TransactionalRingbuffer : private Ringbuffer
{
public:
	TransactionalRingbuffer(void* memory, UInt32 size) 		: Ringbuffer(memory, size), m_GetBarrier(m_Put), m_PutBarrier(m_Get) {}
	TransactionalRingbuffer(MemLabelId label, UInt32 size)	: Ringbuffer(label, size) , m_GetBarrier(m_Put), m_PutBarrier(m_Get) {}

	using Ringbuffer::WritePtrUpdate;
	using Ringbuffer::ReadPtrUpdate;
	using Ringbuffer::GetSize;

	void* WritePtr(UInt32* nBytes) const { return Ptr(m_Put, GetFreeSize(), nBytes); }
	void WritePtrCommit() { m_GetBarrier = m_Put; }
	void WritePtrReset() { m_Put = m_GetBarrier; }

	const void* ReadPtr(UInt32* nBytes) const { return Ptr(m_Get, GetAvailableSize(), nBytes); }
	void ReadPtrCommit() { m_PutBarrier = m_Get; }
	void ReadPtrReset() { m_Get = m_PutBarrier; }

	UInt32 GetFreeSize() const	{ return m_Size - (m_Put - m_PutBarrier); }
	UInt32 GetAvailableSize() const { return m_GetBarrier - m_Get; }

	void Reset() { Ringbuffer::Reset(); m_GetBarrier = m_PutBarrier = 0; }

private:
	volatile UInt32	m_GetBarrier;
	volatile UInt32	m_PutBarrier;
};
#endif // RUNTIME_CONTAINERS_RINGBUFFER_H