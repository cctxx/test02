#ifndef TIMERQUERYD3D9_H
#define TIMERQUERYD3D9_H

#if ENABLE_PROFILER

#include "Runtime/GfxDevice/GfxTimerQuery.h"

class TimerQueriesD3D9;

class TimerQueryD3D9 : public GfxTimerQuery
{
public:
	~TimerQueryD3D9();

	virtual void				Measure();
	virtual ProfileTimeFormat	GetElapsed(UInt32 flags);

	bool	PollResult(UInt64& prevTime, bool wait);
	void	SetTimeMultiplier(float tm) { m_TimeMultiplier = tm; }

private:
	friend TimerQueriesD3D9;
	TimerQueryD3D9();

	IDirect3DQuery9* m_Query;
	ProfileTimeFormat m_Time;
	float m_TimeMultiplier;
	bool m_Active;
};

class TimerQueriesD3D9
{
public:
	TimerQueriesD3D9();

	void		ReleaseAllQueries();
	void		RecreateAllQueries();

	void		BeginTimerQueries();
	void		EndTimerQueries();

	TimerQueryD3D9* CreateTimerQuery();

	void		AddActiveTimerQuery(TimerQueryD3D9* query);
	void		PollTimerQueries();
	bool		PollNextTimerQuery(bool wait);

	bool		HasFrequencyQuery() const { return m_FrequencyQuery != NULL; }

private:
	enum
	{
		kStartTimeQueryCount = 3
	};

	UInt64					m_LastQueryTime;
	IDirect3DQuery9*		m_FrequencyQuery;
	TimerQueryD3D9*			m_StartTimeQueries[kStartTimeQueryCount];
	int						m_StartTimeQueryIndex;
	typedef List<TimerQueryD3D9> TimerQueryList;
	TimerQueryList			m_InactiveTimerQueries;
	TimerQueryList			m_ActiveTimerQueries;
	TimerQueryList			m_PolledTimerQueries;
};

#endif
#endif
