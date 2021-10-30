#ifndef TIMERQUERYGLES_H
#define TIMERQUERYGLES_H

#if ENABLE_PROFILER && GFX_SUPPORTS_OPENGLES20

#include "IncludesGLES20.h"
#include "Runtime/GfxDevice/GfxTimerQuery.h"

struct TimerQueriesGLES
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
extern TimerQueriesGLES g_TimerQueriesGLES;


class TimerQueryGLES
  : public GfxTimerQuery
{
public:

	TimerQueryGLES();

	virtual void				Measure();
	virtual ProfileTimeFormat	GetElapsed(UInt32 flags);

private:

	unsigned			m_Index;
	ProfileTimeFormat	m_Time;
};

#endif
#endif
