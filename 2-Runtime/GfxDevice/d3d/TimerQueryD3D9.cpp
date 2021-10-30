#include "UnityPrefix.h"
#if ENABLE_PROFILER
#include "GfxDeviceD3D9.h"
#include "TimerQueryD3D9.h"


TimerQueryD3D9::TimerQueryD3D9()
	: m_Query(NULL), m_Time(0), m_Active(false)
{
	GetD3DDevice()->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &m_Query);
	m_TimeMultiplier = 0.0f;
}

TimerQueryD3D9::~TimerQueryD3D9()
{
	SAFE_RELEASE(m_Query);
}

void TimerQueryD3D9::Measure()
{
	// Flush previous result
	GetElapsed(kWaitRenderThread);

	TimerQueriesD3D9& queries = GetD3D9GfxDevice().GetTimerQueries();
	if (m_Query && queries.HasFrequencyQuery())
	{
		queries.AddActiveTimerQuery(this);
		m_Query->Issue(D3DISSUE_END);
		m_Active = true;
		m_Time = kInvalidProfileTime;
	}
	else
		m_Time = 0;
	m_TimeMultiplier = 0.0f;
}

ProfileTimeFormat TimerQueryD3D9::GetElapsed(UInt32 flags)
{
	while (m_Active)
	{
		bool wait = (flags & kWaitRenderThread) != 0;
		if (!GetD3D9GfxDevice().GetTimerQueries().PollNextTimerQuery(wait))
			break;
	}
	return m_Time;
}

bool TimerQueryD3D9::PollResult(UInt64& prevTime, bool wait)
{
	for (;;)
	{
		UINT64 time;
		DWORD flags = wait ? D3DGETDATA_FLUSH : 0;
		HRESULT hr = m_Query->GetData(&time, sizeof(time), flags);
		if (hr == S_OK)
		{
			UInt64 elapsed = prevTime ? (time - prevTime) : 0;
			m_Time = ProfileTimeFormat(elapsed * m_TimeMultiplier);
			prevTime = time;
			return true;
		}
		// Stop polling on unknown result (e.g D3DERR_DEVICELOST)
		if (hr != S_FALSE)
		{
			m_Time = 0;
			prevTime = 0;
			return true;
		}
		if (!wait)
			break;
	}
	return false;
}

TimerQueriesD3D9::TimerQueriesD3D9()
{
	m_LastQueryTime = 0;
	m_FrequencyQuery = NULL;
	memset(m_StartTimeQueries, 0, sizeof(m_StartTimeQueries));
	m_StartTimeQueryIndex = 0;
}

void TimerQueriesD3D9::ReleaseAllQueries()
{
	SAFE_RELEASE(m_FrequencyQuery);
	for (int i = 0; i < kStartTimeQueryCount; i++)
	{
		delete m_StartTimeQueries[i];
		m_StartTimeQueries[i] = NULL;
	}
	m_InactiveTimerQueries.append(m_ActiveTimerQueries);
	m_InactiveTimerQueries.append(m_PolledTimerQueries);
	TimerQueryList& queries = m_InactiveTimerQueries;
	for (TimerQueryList::iterator it = queries.begin(); it != queries.end(); ++it)
	{
		TimerQueryD3D9& query = *it;
		query.m_Active = false;
		query.m_Time = 0;
		SAFE_RELEASE(query.m_Query);
	}
}

void TimerQueriesD3D9::RecreateAllQueries()
{
	Assert(m_ActiveTimerQueries.empty());
	Assert(m_PolledTimerQueries.empty());
	TimerQueryList& queries = m_InactiveTimerQueries;
	for (TimerQueryList::iterator it = queries.begin(); it != queries.end(); ++it)
	{
		TimerQueryD3D9& query = *it;
		GetD3DDevice()->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &query.m_Query);
	}
}

void TimerQueriesD3D9::BeginTimerQueries()
{
	// Poll queries from previous frames
	PollTimerQueries();

	if (m_FrequencyQuery == NULL)
	{
		GetD3DDevice()->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &m_FrequencyQuery);
	}
	if (m_FrequencyQuery)
		m_FrequencyQuery->Issue(D3DISSUE_END);

	int& index = m_StartTimeQueryIndex;
	if (m_StartTimeQueries[index] == NULL)
	{
		m_StartTimeQueries[index] = new TimerQueryD3D9;
	}
	m_StartTimeQueries[index]->Measure();
	index = (index + 1) % kStartTimeQueryCount;
}

void TimerQueriesD3D9::EndTimerQueries()
{
	if(m_FrequencyQuery == NULL)
		return;

	HRESULT hr;
	UINT64 freq;
	do
	{
		hr = m_FrequencyQuery->GetData(&freq, sizeof(freq), D3DGETDATA_FLUSH);
	} while (hr == S_FALSE);
	if (hr == S_OK)
	{
		float timeMult = float(1000000000.0 / (double)freq);
		TimerQueryList::iterator query, queryEnd = m_ActiveTimerQueries.end();
		for (query = m_ActiveTimerQueries.begin(); query != queryEnd; ++query)
			query->SetTimeMultiplier(timeMult);
	}
	// Move queries from active to polled list
	m_PolledTimerQueries.append(m_ActiveTimerQueries);
}

TimerQueryD3D9* TimerQueriesD3D9::CreateTimerQuery()
{
	TimerQueryD3D9* query = new TimerQueryD3D9;
	m_InactiveTimerQueries.push_back(*query);	
	return query;
}

void TimerQueriesD3D9::AddActiveTimerQuery(TimerQueryD3D9* query)
{
	query->RemoveFromList();
	m_ActiveTimerQueries.push_back(*query);
}

void TimerQueriesD3D9::PollTimerQueries()
{
	for (;;)
	{
		if (!PollNextTimerQuery(false))
			break;
	}
}

bool TimerQueriesD3D9::PollNextTimerQuery(bool wait)
{
	if (m_PolledTimerQueries.empty())
		return false;

	TimerQueryD3D9& query = m_PolledTimerQueries.front();
	if (query.PollResult(m_LastQueryTime, wait))
	{
		query.m_Active = false;
		query.RemoveFromList();
		m_InactiveTimerQueries.push_back(query);
		return true;
	}
	return false;
}

#endif
