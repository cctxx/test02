#ifndef THREADEDTIMERQUERY_H
#define THREADEDTIMERQUERY_H

#if ENABLE_PROFILER

#include "Runtime/GfxDevice/GfxTimerQuery.h"

class GfxDeviceClient;
struct ClientDeviceTimerQuery;

class ThreadedTimerQuery : public GfxTimerQuery
{
public:
	ThreadedTimerQuery(GfxDeviceClient& device);
	~ThreadedTimerQuery();

	virtual void				Measure();
	virtual ProfileTimeFormat	GetElapsed(UInt32 flags);

private:
	ProfileTimeFormat GetElapsedIfReady();
	void DoLockstep();

	GfxDeviceClient&		m_ClientDevice;
	ClientDeviceTimerQuery* m_ClientQuery;
};

#endif
#endif
