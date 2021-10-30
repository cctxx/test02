#include "UnityPrefix.h"
#if ENABLE_PROFILER && GFX_SUPPORTS_OPENGLES20
#include "TimerQueryGLES20.h"
#include "AssertGLES20.h"
#include "UnityGLES20Ext.h"
#include "Runtime/Shaders/GraphicsCaps.h"

bool TimerQueriesGLES::Init()
{
	bool hasHWSupport = false;
	if( QueryExtension("GL_NV_timer_query") )
	{
		hasHWSupport = gGlesExtFunc.glGetQueryObjectuivEXT && gGlesExtFunc.glGenQueriesEXT;

	#if UNITY_ANDROID
		hasHWSupport = hasHWSupport && gGlesExtFunc.glQueryCounterNV && gGlesExtFunc.glGetQueryObjectui64vNV;
	#endif
	}

	gGraphicsCaps.hasTimerQuery = hasHWSupport;
	if(hasHWSupport)
		GLES_CHK(gGlesExtFunc.glGenQueriesEXT(kQueriesCount, timestamp_gl));

	nextIndex = 0;
	return true;
}

void TimerQueriesGLES::BeginTimerQueries()
{
	GLES_CHK(glFlush());
	SetTimestamp();
}

void TimerQueriesGLES::EndTimerQueries()
{
	GLES_CHK(glFlush());
}

unsigned TimerQueriesGLES::SetTimestamp()
{
#if UNITY_ANDROID
	GLES_CHK(gGlesExtFunc.glQueryCounterNV(timestamp_gl[nextIndex], GL_TIMESTAMP_NV));
#endif
	unsigned ret = nextIndex;

	++nextIndex;
	if(nextIndex == kQueriesCount)
		nextIndex = 0;

	return ret;
}

UInt64 TimerQueriesGLES::GetElapsedTime(unsigned idx, bool wait)
{
#if UNITY_ANDROID
	GLuint available = 0;
	GLES_CHK(gGlesExtFunc.glGetQueryObjectuivEXT(timestamp_gl[idx], GL_QUERY_RESULT_AVAILABLE_EXT, &available));
	// sometimes timestamp might be not ready (we still dont know why)
	// the only workaround would be to add glFlush into SetTimestamp
	// but then some timings will be a bit off
	if(wait)
	{
		for(unsigned i = 0 ; i < 100 && !available ; ++i)
		{
			GLES_CHK(gGlesExtFunc.glGetQueryObjectuivEXT(timestamp_gl[idx], GL_QUERY_RESULT_AVAILABLE_EXT, &available));
		}
	}

	if(available)
	{
		unsigned prev_idx = idx > 0 ? idx-1 : kQueriesCount-1;

		EGLuint64NV time1, time2;
		GLES_CHK(gGlesExtFunc.glGetQueryObjectui64vNV(timestamp_gl[prev_idx], GL_QUERY_RESULT_EXT, &time1));
		GLES_CHK(gGlesExtFunc.glGetQueryObjectui64vNV(timestamp_gl[idx], GL_QUERY_RESULT_EXT, &time2));

		return time2-time1;
	}
#endif

	return kInvalidProfileTime;

}


TimerQueryGLES::TimerQueryGLES()
  : m_Index(0),
  	m_Time(kInvalidProfileTime)
{
}

void TimerQueryGLES::Measure()
{
	m_Index = g_TimerQueriesGLES.SetTimestamp();
	m_Time  = kInvalidProfileTime;
}

ProfileTimeFormat TimerQueryGLES::GetElapsed(UInt32 flags)
{
	if(m_Time == kInvalidProfileTime)
		m_Time = g_TimerQueriesGLES.GetElapsedTime(m_Index, (flags & kWaitRenderThread) != 0);

	return m_Time;
}

TimerQueriesGLES g_TimerQueriesGLES;


#endif
