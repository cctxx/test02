#ifndef TIMERQUERYGLES_H
#define TIMERQUERYGLES_H

#if ENABLE_PROFILER && GFX_SUPPORTS_OPENGLES30

#include "IncludesGLES30.h"
#include "Runtime/GfxDevice/GfxTimerQuery.h"

struct TimerQueriesGLES30
{
	enum
	{
		kQueriesCount = 128,
	};

	GLuint		timestamp_gl[kQueriesCount];
	unsigned	nextIndex;

	bool		Init();

	void		BeginTimerQueries();
	void		EndTimerQueries();

	unsigned	SetTimestamp();
	UInt64		GetElapsedTime(unsigned idx, bool wait);
};
extern TimerQueriesGLES30 g_TimerQueriesGLES30;


class TimerQueryGLES30
  : public GfxTimerQuery
{
public:

	TimerQueryGLES30();

	virtual void				Measure();
	virtual ProfileTimeFormat	GetElapsed(UInt32 flags);

private:

	unsigned			m_Index;
	ProfileTimeFormat	m_Time;
};

#endif
#endif
