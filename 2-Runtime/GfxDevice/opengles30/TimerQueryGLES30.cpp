#include "UnityPrefix.h"
#if ENABLE_PROFILER && GFX_SUPPORTS_OPENGLES30
#include "TimerQueryGLES30.h"
#include "AssertGLES30.h"
#include "UnityGLES30Ext.h"
#include "Runtime/Shaders/GraphicsCaps.h"

bool TimerQueriesGLES30::Init()
{
	Assert(!gGraphicsCaps.hasTimerQuery || QueryExtension("GL_NV_timer_query"));

	if (gGraphicsCaps.hasTimerQuery)
		GLES_CHK(glGenQueries(kQueriesCount, timestamp_gl));

	nextIndex = 0;
	return true;
}

void TimerQueriesGLES30::BeginTimerQueries()
{
	GLES_CHK(glFlush());
	SetTimestamp();
}

void TimerQueriesGLES30::EndTimerQueries()
{
	GLES_CHK(glFlush());
}

unsigned TimerQueriesGLES30::SetTimestamp()
{
	// \todo [2013-04-17 pyry] glQueryCounter is not in ES3
#if 0
	GLES_CHK(gGles3ExtFunc.glQueryCounterNV(timestamp_gl[nextIndex], GL_TIMESTAMP_NV));
#endif
	unsigned ret = nextIndex;

	++nextIndex;
	if(nextIndex == kQueriesCount)
		nextIndex = 0;

	return ret;
}

UInt64 TimerQueriesGLES30::GetElapsedTime(unsigned idx, bool wait)
{
	GLuint available = 0;
	GLES_CHK(glGetQueryObjectuiv(timestamp_gl[idx], GL_QUERY_RESULT_AVAILABLE, &available));
	// sometimes timestamp might be not ready (we still dont know why)
	// the only workaround would be to add glFlush into SetTimestamp
	// but then some timings will be a bit off
	if(wait)
	{
		for(unsigned i = 0 ; i < 100 && !available ; ++i)
		{
			GLES_CHK(glGetQueryObjectuiv(timestamp_gl[idx], GL_QUERY_RESULT_AVAILABLE, &available));
		}
	}

	if(available)
	{
		unsigned prev_idx = idx > 0 ? idx-1 : kQueriesCount-1;

		// \todo [2013-04-17 pyry] i64 variant?
		GLuint time1, time2;
		GLES_CHK(glGetQueryObjectuiv(timestamp_gl[prev_idx], GL_QUERY_RESULT, &time1));
		GLES_CHK(glGetQueryObjectuiv(timestamp_gl[idx], GL_QUERY_RESULT, &time2));

		return time2-time1;
	}

	return kInvalidProfileTime;

}


TimerQueryGLES30::TimerQueryGLES30()
  : m_Index(0),
  	m_Time(kInvalidProfileTime)
{
}

void TimerQueryGLES30::Measure()
{
	m_Index = g_TimerQueriesGLES30.SetTimestamp();
	m_Time  = kInvalidProfileTime;
}

ProfileTimeFormat TimerQueryGLES30::GetElapsed(UInt32 flags)
{
	if(m_Time == kInvalidProfileTime)
		m_Time = g_TimerQueriesGLES30.GetElapsedTime(m_Index, (flags & kWaitRenderThread) != 0);

	return m_Time;
}

TimerQueriesGLES30 g_TimerQueriesGLES30;


#endif
