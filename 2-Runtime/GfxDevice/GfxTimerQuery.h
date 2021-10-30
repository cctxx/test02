#ifndef GFXTIMERQUERY_H
#define GFXTIMERQUERY_H

#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Utilities/LinkedList.h"

class GfxTimerQuery : public ListElement
{
public:
	virtual ~GfxTimerQuery() {}

	enum
	{
		kWaitRenderThread = 1 << 0,
		kWaitClientThread = 1 << 1,
		kWaitAll = (kWaitClientThread | kWaitRenderThread)
	};

	virtual void				Measure() = 0;
	virtual ProfileTimeFormat	GetElapsed(UInt32 flags) = 0;
};

#endif
