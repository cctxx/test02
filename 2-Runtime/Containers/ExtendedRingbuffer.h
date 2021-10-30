#ifndef RUNTIME_CONTAINERS_RINGBUFFERS_H
#define RUNTIME_CONTAINERS_RINGBUFFERS_H

#include "GrowingRingbuffer.h"
#include "Ringbuffer.h"
#include "TransactionalRingbuffer.h"
#include "Runtime/Threads/Semaphore.h"
#include "Runtime/Utilities/NonCopyable.h"

#if SUPPORT_THREADS
namespace RingbufferTemplates
{
	// ------------------------------------------------------------------------
	// Support base classes used to avoid casting to 'normal' ringbuffer impl
	// ------------------------------------------------------------------------
	template <class Ringbuffer>
	class NonCastable : protected Ringbuffer
	{
	public:
		using Ringbuffer::WritePtr;
		using Ringbuffer::WritePtrUpdate;
		using Ringbuffer::ReadPtr;
		using Ringbuffer::ReadPtrUpdate;
		using Ringbuffer::GetSize;
		using Ringbuffer::GetFreeSize;
		using Ringbuffer::GetAllocatedSize;
		using Ringbuffer::GetAvailableSize;
		using Ringbuffer::Reset;

	protected:
		NonCastable(MemLabelId label, UInt32 size) : Ringbuffer(label, size) {}
	};

	// ------------------------------------------------------------------------
	// Adds support for notifications whenever data or free space is available
	// ------------------------------------------------------------------------
	template <class Ringbuffer>
	class AbstractNotificationSupport : public NonCastable<Ringbuffer>, public NonCopyable
	{
	public:
		AbstractNotificationSupport(MemLabelId label, UInt32 size) : NonCastable<Ringbuffer>(label, size) {}

		void ReleaseBlockedThreads(bool indefinitely = false)
		{
			m_FreeSemaphore.Suspend(indefinitely);
			m_AvailableSemaphore.Suspend(indefinitely);
		}

		void BlockUntilFree()
		{
			if (Ringbuffer::GetFreeSize())
				return;

			m_FreeSemaphore.Resume(true);
			if (Ringbuffer::GetFreeSize() == 0)
				m_FreeSemaphore.WaitForSignal();
			m_FreeSemaphore.Suspend();
		}

		void BlockUntilAvailable()
		{
			if (Ringbuffer::GetAvailableSize())
				return;

			m_AvailableSemaphore.Resume(true);
			if (Ringbuffer::GetAvailableSize() == 0)
				m_AvailableSemaphore.WaitForSignal();
			m_AvailableSemaphore.Suspend();
		}

	protected:
		SuspendableSemaphore 	m_AvailableSemaphore;
		SuspendableSemaphore 	m_FreeSemaphore;
	};

	template <class Ringbuffer>
	class RNotificationSupport : public AbstractNotificationSupport<Ringbuffer>
	{
	public:
		RNotificationSupport(MemLabelId label, UInt32 size) : AbstractNotificationSupport<Ringbuffer>(label, size) {}

		void WritePtrUpdate(void* writePtr, UInt32 nBytesWritten)
		{
			Ringbuffer::WritePtrUpdate(writePtr, nBytesWritten);
			AbstractNotificationSupport<Ringbuffer>::m_AvailableSemaphore.Signal();
		}

		void ReadPtrUpdate(const void* readPtr, UInt32 nBytesRead)
		{
			Ringbuffer::ReadPtrUpdate(readPtr, nBytesRead);
			AbstractNotificationSupport<Ringbuffer>::m_FreeSemaphore.Signal();
		}
	};


	template <class TransactionalRingbuffer>
	class TRNotificationSupport	: public AbstractNotificationSupport<TransactionalRingbuffer>
	{
	public:
		TRNotificationSupport(MemLabelId label, UInt32 size) : AbstractNotificationSupport<TransactionalRingbuffer>(label, size) {}

		using AbstractNotificationSupport<TransactionalRingbuffer>::ReadPtrReset;
		using AbstractNotificationSupport<TransactionalRingbuffer>::WritePtrReset;

		void WritePtrCommit()
		{
			AbstractNotificationSupport<TransactionalRingbuffer>::WritePtrCommit();
			AbstractNotificationSupport<TransactionalRingbuffer>::m_AvailableSemaphore.Signal();
		}

		void ReadPtrCommit()
		{
			AbstractNotificationSupport<TransactionalRingbuffer>::ReadPtrCommit();
			AbstractNotificationSupport<TransactionalRingbuffer>::m_FreeSemaphore.Signal();
		}
	};

}

typedef RingbufferTemplates::RNotificationSupport<Ringbuffer>					ExtendedRingbuffer;
typedef RingbufferTemplates::RNotificationSupport<GrowingRingbuffer>			ExtendedGrowingRingbuffer;
typedef RingbufferTemplates::TRNotificationSupport<TransactionalRingbuffer> 	ExtendedTransactionalRingbuffer;

#else // no thread support

namespace RingbufferTemplates
{
	template <class Ringbuffer>
	class NotificationSupport : public Ringbuffer, public NonCopyable
	{
	public:
		NotificationSupport(MemLabelId label, UInt32 size) : Ringbuffer(label, size) {}

		void ReleaseBlockedThreads(bool indefinitely = false) {}
		void BlockUntilFree() {}
		void BlockUntilAvailable() {}
	};
}

typedef RingbufferTemplates::NotificationSupport<Ringbuffer>				ExtendedRingbuffer;
typedef RingbufferTemplates::NotificationSupport<GrowingRingbuffer>			ExtendedGrowingRingbuffer;
typedef RingbufferTemplates::NotificationSupport<TransactionalRingbuffer> 	ExtendedTransactionalRingbuffer;

#endif

#endif //RUNTIME_CONTAINERS_RINGBUFFERS_H
