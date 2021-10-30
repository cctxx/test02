#ifndef TIMERQUERYGL_H
#define TIMERQUERYGL_H

#if ENABLE_PROFILER

#include "Runtime/GfxDevice/GfxTimerQuery.h"

class TimerQueryGL : public GfxTimerQuery
{
public:
	TimerQueryGL();
	~TimerQueryGL();

	virtual void				Measure();
	void						MeasureBegin();
	virtual ProfileTimeFormat	GetElapsed(UInt32 flags);

	bool	PollResult(UInt64& prevTime, bool wait);

private:
	GLuint m_Query;
	ProfileTimeFormat m_Time;
};

class TimerQueriesGL
{
public:
	TimerQueriesGL();

	GLuint		AllocateQuery();
	void		ReleaseQuery(GLuint query);

	void		BeginTimerQueries();
	void		EndTimerQueries();

	bool		IsActive() const { return m_Active; }

	void		AddActiveTimerQuery(TimerQueryGL* query);
	void		PollTimerQueries(bool wait);
	bool		PollNextTimerQuery(bool wait);

private:
	enum
	{
		kStartTimeQueryCount = 3,
		kMaxFreeQueries = 128
	};

	GLuint					m_FreeQueries[kMaxFreeQueries];
	int						m_NumFreeQueries;
	UInt64					m_LastQueryTime;
	TimerQueryGL*			m_StartTimeQueries[kStartTimeQueryCount];
	int						m_StartTimeQueryIndex;
	typedef List<TimerQueryGL> TimerQueryList;
	TimerQueryList			m_ActiveTimerQueries;
	TimerQueryList			m_PolledTimerQueries;
	bool					m_Active;
};

extern TimerQueriesGL g_TimerQueriesGL;

#endif
#endif
