#include "UnityPrefix.h"
#if ENABLE_PROFILER
#include "UnityGL.h"
#include "TimerQueryGL.h"
#include "Runtime/Shaders/GraphicsCaps.h"

TimerQueryGL::TimerQueryGL()
{
	m_Query = g_TimerQueriesGL.AllocateQuery();
}

TimerQueryGL::~TimerQueryGL()
{
	g_TimerQueriesGL.ReleaseQuery(m_Query);
}

void TimerQueryGL::Measure()
{
	// Finish previous timer query
	OGL_CALL(glEndQueryARB(GL_TIME_ELAPSED_EXT));

	MeasureBegin();
}

void TimerQueryGL::MeasureBegin()
{
	// Flush previous result
	GetElapsed(kWaitRenderThread);

	OGL_CALL(glBeginQueryARB(GL_TIME_ELAPSED_EXT, m_Query));

	g_TimerQueriesGL.AddActiveTimerQuery(this);
	m_Time = kInvalidProfileTime;
}

ProfileTimeFormat TimerQueryGL::GetElapsed(UInt32 flags)
{
	bool wait = (flags & kWaitRenderThread) != 0;
	// We need to return a valid time if waiting
	if (wait && m_Time == kInvalidProfileTime)
		m_Time = 0;
	while (IsInList())
	{
		if (!g_TimerQueriesGL.PollNextTimerQuery(wait))
			break;
	}
	return m_Time;
}

bool TimerQueryGL::PollResult(UInt64& prevTime, bool wait)
{
	for (;;)
	{
		// Currently we always wait on result
		//GLint available = 0;
		//OGL_CALL(glGetQueryObjectivARB(m_Query, GL_QUERY_RESULT_AVAILABLE, &available));
		//if  (available)
		{
			GLuint64EXT time;
			OGL_CALL(glGetQueryObjectui64vEXT(m_Query, GL_QUERY_RESULT, &time));
			// Some Nvidia cards return invalid results, sanity check!
			if (time > GLuint64EXT(0xffffffff))
				gGraphicsCaps.buggyTimerQuery = true;
			// We actually want previous query's time elapsed
			// Save current returned result for next query
			m_Time = prevTime;
			prevTime = time;
			return true;
		}
		if (!wait)
			break;
	}
	return false;
}

TimerQueriesGL::TimerQueriesGL()
{
	memset(m_FreeQueries, 0, sizeof(m_FreeQueries));
	m_NumFreeQueries = 0;
	m_LastQueryTime = 0;
	memset(m_StartTimeQueries, 0, sizeof(m_StartTimeQueries));
	m_StartTimeQueryIndex = 0;
	m_Active = false;
}

GLuint TimerQueriesGL::AllocateQuery()
{
	if (m_NumFreeQueries == 0)
	{
		OGL_CALL(glGenQueriesARB(kMaxFreeQueries, m_FreeQueries));
		m_NumFreeQueries = kMaxFreeQueries;
	}
	return m_FreeQueries[--m_NumFreeQueries];
}

void TimerQueriesGL::ReleaseQuery(GLuint query)
{
	if (m_NumFreeQueries == kMaxFreeQueries)
	{
		OGL_CALL(glDeleteQueriesARB(kMaxFreeQueries, m_FreeQueries));
		m_NumFreeQueries = 0;
	}
	m_FreeQueries[m_NumFreeQueries++] = query;
}

void TimerQueriesGL::AddActiveTimerQuery(TimerQueryGL* query)
{
	m_ActiveTimerQueries.push_back(*query);
}

void TimerQueriesGL::BeginTimerQueries()
{
	Assert(!m_Active);
	int& index = m_StartTimeQueryIndex;
	if(m_StartTimeQueries[index] == NULL)
	{
		m_StartTimeQueries[index] = new TimerQueryGL;
	}
	m_StartTimeQueries[index]->MeasureBegin();
	index = (index + 1) % kStartTimeQueryCount;
	m_Active = true;
}

void TimerQueriesGL::EndTimerQueries()
{
	Assert(m_Active);
	OGL_CALL(glEndQueryARB(GL_TIME_ELAPSED_EXT));
	OGL_CALL(glFlush());

	// Move queries from active to polled list
	m_PolledTimerQueries.append(m_ActiveTimerQueries);

	g_TimerQueriesGL.PollTimerQueries(true);
	m_Active = false;
}

void TimerQueriesGL::PollTimerQueries(bool wait)
{
	for (;;)
	{
		if (!PollNextTimerQuery(wait))
			break;
	}
}

bool TimerQueriesGL::PollNextTimerQuery(bool wait)
{
	if (m_PolledTimerQueries.empty())
		return false;

	TimerQueryGL& query = m_PolledTimerQueries.front();
	if (query.PollResult(m_LastQueryTime, wait))
	{
		m_PolledTimerQueries.pop_front();
		return true;
	}
	return false;
}

TimerQueriesGL g_TimerQueriesGL;

#endif
