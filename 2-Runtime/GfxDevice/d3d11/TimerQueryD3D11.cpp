#include "UnityPrefix.h"
#if ENABLE_PROFILER
#include "GfxDeviceD3D11.h"
#include "D3D11Context.h"
#include "TimerQueryD3D11.h"


TimerQueryD3D11::TimerQueryD3D11()
	: m_Query(NULL), m_Time(0), m_Active(false)
{
	D3D11_QUERY_DESC QueryDesc;
	QueryDesc.Query = D3D11_QUERY_TIMESTAMP;
	QueryDesc.MiscFlags = 0;
	GetD3D11Device()->CreateQuery(&QueryDesc, &m_Query);
	m_TimeMultiplier = 0.0f;
}

TimerQueryD3D11::~TimerQueryD3D11()
{
	SAFE_RELEASE(m_Query);
}

void TimerQueryD3D11::Measure()
{
	// Flush previous result
	GetElapsed(kWaitRenderThread);

	if (m_Query)
	{
		g_TimerQueriesD3D11.AddActiveTimerQuery(this);
		GetD3D11Context()->End(m_Query);
		m_Active = true;
		m_Time = kInvalidProfileTime;
	}
	else
		m_Time = 0;
	m_TimeMultiplier = 0.0f;
}

ProfileTimeFormat TimerQueryD3D11::GetElapsed(UInt32 flags)
{
	while (m_Active)
	{
		bool wait = (flags & kWaitRenderThread) != 0;
		if (!g_TimerQueriesD3D11.PollNextTimerQuery(wait))
			break;
	}
	return m_Time;
}

bool TimerQueryD3D11::PollResult(UInt64& prevTime, bool wait)
{
	for (;;)
	{
		UINT64 time;
		UINT flags = wait ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH;
		HRESULT hr = GetD3D11Context()->GetData(m_Query, &time, sizeof(time), flags);
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

TimerQueriesD3D11::TimerQueriesD3D11()
{
	m_LastQueryTime = 0;
	m_DisjointQuery = NULL;
	memset(m_StartTimeQueries, 0, sizeof(m_StartTimeQueries));
	m_StartTimeQueryIndex = 0;
}

void TimerQueriesD3D11::ReleaseAllQueries()
{
	SAFE_RELEASE(m_DisjointQuery);
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
		TimerQueryD3D11& query = *it;
		query.m_Active = false;
		query.m_Time = 0;
		SAFE_RELEASE(query.m_Query);
	}
}

void TimerQueriesD3D11::RecreateAllQueries()
{
	Assert(m_ActiveTimerQueries.empty());
	Assert(m_PolledTimerQueries.empty());
	TimerQueryList& queries = m_InactiveTimerQueries;
	for (TimerQueryList::iterator it = queries.begin(); it != queries.end(); ++it)
	{
		TimerQueryD3D11& query = *it;

		D3D11_QUERY_DESC QueryDesc;
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP;
		QueryDesc.MiscFlags = 0;
		GetD3D11Device()->CreateQuery(&QueryDesc, &query.m_Query);
	}
}

void TimerQueriesD3D11::BeginTimerQueries()
{
	// Poll queries from previous frames
	PollTimerQueries();

	if (m_DisjointQuery == NULL)
	{
		D3D11_QUERY_DESC QueryDesc;
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		QueryDesc.MiscFlags = 0;
		GetD3D11Device()->CreateQuery(&QueryDesc, &m_DisjointQuery);
	}
	GetD3D11Context()->Begin(m_DisjointQuery);

	int& index = m_StartTimeQueryIndex;
	if (m_StartTimeQueries[index] == NULL)
	{
		m_StartTimeQueries[index] = new TimerQueryD3D11;
	}
	m_StartTimeQueries[index]->Measure();
	index = (index + 1) % kStartTimeQueryCount;
}

void TimerQueriesD3D11::EndTimerQueries()
{
	if(m_DisjointQuery == NULL)
		return;

	GetD3D11Context()->End(m_DisjointQuery);

	HRESULT hr;
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointQueryResult;
	do
	{
		hr = GetD3D11Context()->GetData(m_DisjointQuery, &disjointQueryResult, sizeof(disjointQueryResult), 0);
	} while (hr == S_FALSE);
	if (hr == S_OK && !disjointQueryResult.Disjoint)
	{
		float timeMult = float(1000000000.0 / (double)disjointQueryResult.Frequency);
		TimerQueryList::iterator query, queryEnd = m_ActiveTimerQueries.end();
		for (query = m_ActiveTimerQueries.begin(); query != queryEnd; ++query)
			query->SetTimeMultiplier(timeMult);
	}
	// Move queries from active to polled list
	m_PolledTimerQueries.append(m_ActiveTimerQueries);
}

TimerQueryD3D11* TimerQueriesD3D11::CreateTimerQuery()
{
	TimerQueryD3D11* query = new TimerQueryD3D11;
	m_InactiveTimerQueries.push_back(*query);	
	return query;
}

void TimerQueriesD3D11::AddActiveTimerQuery(TimerQueryD3D11* query)
{
	query->RemoveFromList();
	m_ActiveTimerQueries.push_back(*query);
}

void TimerQueriesD3D11::PollTimerQueries()
{
	for (;;)
	{
		if (!PollNextTimerQuery(false))
			break;
	}
}

bool TimerQueriesD3D11::PollNextTimerQuery(bool wait)
{
	if (m_PolledTimerQueries.empty())
		return false;

	TimerQueryD3D11& query = m_PolledTimerQueries.front();
	if (query.PollResult(m_LastQueryTime, wait))
	{
		query.m_Active = false;
		query.RemoveFromList();
		m_InactiveTimerQueries.push_back(query);
		return true;
	}
	return false;
}

TimerQueriesD3D11 g_TimerQueriesD3D11;

#endif
