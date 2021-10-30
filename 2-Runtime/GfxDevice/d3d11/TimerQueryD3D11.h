#ifndef TIMERQUERYD3D11_H
#define TIMERQUERYD3D11_H

#if ENABLE_PROFILER

#include "Runtime/GfxDevice/GfxTimerQuery.h"

class TimerQueriesD3D11;

class TimerQueryD3D11 : public GfxTimerQuery
{
public:
	~TimerQueryD3D11();

	virtual void				Measure();
	virtual ProfileTimeFormat	GetElapsed(UInt32 flags);

	bool	PollResult(UInt64& prevTime, bool wait);
	void	SetTimeMultiplier(float tm) { m_TimeMultiplier = tm; }

private:
	friend TimerQueriesD3D11;
	TimerQueryD3D11();

	ID3D11Query* m_Query;
	ProfileTimeFormat m_Time;
	float m_TimeMultiplier;
	bool m_Active;
};

class TimerQueriesD3D11
{
public:
	TimerQueriesD3D11();

	void		ReleaseAllQueries();
	void		RecreateAllQueries();

	void		BeginTimerQueries();
	void		EndTimerQueries();

	TimerQueryD3D11* CreateTimerQuery();

	void		AddActiveTimerQuery(TimerQueryD3D11* query);
	void		PollTimerQueries();
	bool		PollNextTimerQuery(bool wait);

private:
	enum
	{
		kStartTimeQueryCount = 3
	};

	UInt64					m_LastQueryTime;
	ID3D11Query*			m_DisjointQuery;
	TimerQueryD3D11*		m_StartTimeQueries[kStartTimeQueryCount];
	int						m_StartTimeQueryIndex;
	typedef List<TimerQueryD3D11> TimerQueryList;
	TimerQueryList			m_InactiveTimerQueries;
	TimerQueryList			m_ActiveTimerQueries;
	TimerQueryList			m_PolledTimerQueries;
};

extern TimerQueriesD3D11 g_TimerQueriesD3D11;

#endif
#endif
