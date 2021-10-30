#include "UnityPrefix.h"
#if ENABLE_PROFILER
#include "ThreadedTimerQuery.h"
#include "GfxDeviceWorker.h"
#include "Runtime/Threads/ThreadUtility.h"
#include "Runtime/Threads/ThreadedStreamBuffer.h"
#include "Runtime/GfxDevice/threaded/GfxDeviceClient.h"
#include "Runtime/GfxDevice/threaded/GfxCommands.h"

ThreadedTimerQuery::ThreadedTimerQuery(GfxDeviceClient& device)
:	m_ClientDevice(device)
{
	m_ClientQuery = new ClientDeviceTimerQuery;
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		ThreadedStreamBuffer& stream = *m_ClientDevice.GetCommandQueue();
		stream.WriteValueType<GfxCommand>(kGfxCmd_TimerQuery_Constructor);
		stream.WriteValueType<ClientDeviceTimerQuery*>(m_ClientQuery);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_ClientQuery->internalQuery = GetRealGfxDevice().CreateTimerQuery();
}

ThreadedTimerQuery::~ThreadedTimerQuery()
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		ThreadedStreamBuffer& stream = *m_ClientDevice.GetCommandQueue();
		stream.WriteValueType<GfxCommand>(kGfxCmd_TimerQuery_Destructor);
		stream.WriteValueType<ClientDeviceTimerQuery*>(m_ClientQuery);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		Assert(m_ClientQuery);
		GetRealGfxDevice().DeleteTimerQuery(m_ClientQuery->GetInternal());
		delete m_ClientQuery;
	}
	m_ClientQuery = NULL;
}

void ThreadedTimerQuery::Measure()
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		ThreadedStreamBuffer& stream = *m_ClientDevice.GetCommandQueue();
		stream.WriteValueType<GfxCommand>(kGfxCmd_TimerQuery_Measure);
		stream.WriteValueType<ClientDeviceTimerQuery*>(m_ClientQuery);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_ClientQuery->GetInternal()->Measure();
}

ProfileTimeFormat ThreadedTimerQuery::GetElapsed(UInt32 flags)
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		// See if we have the result already
		ProfileTimeFormat time = GetElapsedIfReady();
		if (time != kInvalidProfileTime)
			return time;

		// Request result from worker thread
		ThreadedStreamBuffer& stream = *m_ClientDevice.GetCommandQueue();
		stream.WriteValueType<GfxCommand>(kGfxCmd_TimerQuery_GetElapsed);
		stream.WriteValueType<ClientDeviceTimerQuery*>(m_ClientQuery);
		stream.WriteValueType<UInt32>(flags);
		if (flags & GfxTimerQuery::kWaitClientThread)
		{
			m_ClientDevice.SubmitCommands();
			m_ClientDevice.GetGfxDeviceWorker()->WaitForSignal();
		}
		GFXDEVICE_LOCKSTEP_CLIENT();
		return GetElapsedIfReady();
	}
	else
		return m_ClientQuery->GetInternal()->GetElapsed(flags);
}

ProfileTimeFormat ThreadedTimerQuery::GetElapsedIfReady()
{
	if (!m_ClientQuery->pending)
	{
		// Be careful since UInt64 isn't guaranteed atomic
		UnityMemoryBarrier();
		return m_ClientQuery->elapsed;
	}
	return kInvalidProfileTime;
}

void ThreadedTimerQuery::DoLockstep()
{
	m_ClientDevice.DoLockstep();
}

#endif
