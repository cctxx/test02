#include "UnityPrefix.h"
#include "ProfilerFrameData.h"
#include "Runtime/GfxDevice/GfxDevice.h"

#if ENABLE_PROFILER



// -------------------------------------------------------------------


dynamic_array<GfxTimerQuery*> ProfilerFrameData::m_UnusedQueries;

ProfilerFrameData::ProfilerFrameData(int threadCount, int frameID)
: m_FrameID(frameID)
{
	Assert(threadCount > 0);
	m_ThreadData = new ThreadData[threadCount];
	m_ThreadCount = threadCount;
}


ProfilerFrameData::~ProfilerFrameData()
{
	for (int t = 0; t < m_ThreadCount; ++t)
	{
		ThreadData& td = m_ThreadData[t];
		for (int i = 0; i < td.m_GPUTimeSamples.size(); i++)
		{
			GfxTimerQuery* query = td.m_GPUTimeSamples[i].timerQuery;
			if (query)
				m_UnusedQueries.push_back(query);
		}
	}
	delete[] m_ThreadData;
}

GfxTimerQuery* ProfilerFrameData::AllocTimerQuery()
{
	if (!m_UnusedQueries.empty())
	{
		GfxTimerQuery* query = m_UnusedQueries.back();
		m_UnusedQueries.pop_back();
		return query;
	}
	else
		return GetGfxDevice().CreateTimerQuery();
}

void ProfilerFrameData::ReleaseTimerQuery(GfxTimerQuery* query)
{
	m_UnusedQueries.push_back(query);
}

void ProfilerFrameData::FreeAllTimerQueries()
{
	for (int i = 0; i < m_UnusedQueries.size(); i++)
		GetGfxDevice().DeleteTimerQuery(m_UnusedQueries[i]);
	m_UnusedQueries.clear();
}

void ProfilerFrameData::ThreadData::ExtractAllChildSamples (UInt32 index, dynamic_array<UInt32>& allChildren) const
{
	const ProfilerSample* sample = &m_AllSamples[index];
	const ProfilerSample* currentSample = sample + 1;
	for (int i=0;i<sample->nbChildren;i++)
	{
		UInt32 currentIndex = currentSample - m_AllSamples.begin();
		allChildren.push_back(currentIndex);
		currentSample = SkipSampleRecurse(currentSample);
	}	
}



// -------------------------------------------------------------------



#if UNITY_EDITOR

#include "ProfilerHistory.h"

ProfilerFrameDataIterator::ProfilerFrameDataIterator()
:	m_ThreadIdx(0)
,	m_FrameData(NULL)
,	m_CurrIndex(0)
{
}

const ProfilerSample& ProfilerFrameDataIterator::GetSample(UInt32 index) const
{
	DebugAssert(m_FrameData);
	const ProfilerFrameData::ThreadData& tdata = m_FrameData->m_ThreadData[m_ThreadIdx];
	Assert(index < tdata.m_AllSamples.size());

	return tdata.m_AllSamples[index];
}

int ProfilerFrameDataIterator::GetGroup() const
{
	const ProfilerSample& s = GetSample(m_CurrIndex);
	return s.information ? s.information->group : kProfilerOther;
}

float ProfilerFrameDataIterator::GetStartTimeMS () const
{
	const ProfilerSample& s = GetSample(m_CurrIndex);
	return (s.startTimeUS - m_FrameData->m_StartTimeUS) / 1000.0;
}

float ProfilerFrameDataIterator::GetDurationMS () const
{
	const ProfilerSample& s = GetSample(m_CurrIndex);
	return s.timeUS / 1000.0;
}


int ProfilerFrameDataIterator::GetThreadCount(int frame) const
{
	ProfilerFrameData* frameData = ProfilerHistory::Get().GetFrameData(frame);
	if (frameData == NULL)
		return 0;
	return frameData->m_ThreadCount;
}

double ProfilerFrameDataIterator::GetFrameStartS(int frame) const
{
	ProfilerFrameData* frameData = ProfilerHistory::Get().GetFrameData(frame);
	if (frameData == NULL)
		return 0.0;
	return frameData->m_StartTimeUS / 1000000.0;
}

const std::string* ProfilerFrameDataIterator::GetThreadName () const
{
	if (!m_FrameData)
		return NULL;
	const ProfilerFrameData::ThreadData& tdata = m_FrameData->m_ThreadData[m_ThreadIdx];
	return &tdata.m_ThreadName;
}


void ProfilerFrameDataIterator::SetRoot(int frame, int threadIdx)
{
	m_Stack.clear();
	m_CurrIndex = 0;
	m_FrameData = NULL;

	ProfilerFrameData* frameData = ProfilerHistory::Get().GetFrameData(frame);
	if (frameData == NULL)
		return;

	if (threadIdx < 0 || threadIdx >= frameData->m_ThreadCount)
		return;

	m_FrameData = frameData;
	m_ThreadIdx = threadIdx;

	m_CurrIndex = 0;
}	

float ProfilerFrameDataIterator::GetFrameTimeMS() const
{
	if (m_FrameData)
		return m_FrameData->m_TotalCPUTimeInMicroSec/1000.0;
	else
		return 0.0f;
}

bool ProfilerFrameDataIterator::GetNext(bool expanded)
{
	if (m_FrameData == NULL)
		return false;

	const ProfilerFrameData::ThreadData& tdata = m_FrameData->m_ThreadData[m_ThreadIdx];
	const UInt32 nSamples = tdata.m_AllSamples.size();
	if (m_CurrIndex >= nSamples)
	{
		DebugAssert(nSamples == 0);
		return false;
	}

	const ProfilerSample* s = tdata.GetSample (m_CurrIndex);
	const ProfilerSample* nextSameLevel = SkipSampleRecurse (s);
	const UInt32 idxNextSameLevel = nextSameLevel - tdata.m_AllSamples.data();

	const bool hasChildren = s->nbChildren != 0;
	if (hasChildren && expanded)
	{
		// entering into children, push our range into stack
		StackInfo info;
		if (!m_Stack.empty())
			info.path = m_Stack.back().path;
		const ProfilerSample* ps = tdata.GetSample(m_CurrIndex);
		if (ps && ps->information)
		{
			info.path += (ps && ps->information) ? ps->information->name : "?";
			info.path += '/';
		}

		info.sampleBegin = m_CurrIndex;
		info.sampleEnd = idxNextSameLevel;
		m_Stack.push_back(info);
		++m_CurrIndex;
	}
	else
	{
		// We'll go to sample idxNextSameLevel. But if we've reached end of our level,
		// that sample might be at parent level already. Pop scope until we're at
		// the right level.
		m_CurrIndex = idxNextSameLevel;
		while (!m_Stack.empty() && idxNextSameLevel >= m_Stack.back().sampleEnd)
			m_Stack.pop_back();
		if (m_Stack.empty())
			return false; // reached very end
	}

	s = tdata.GetSample (m_CurrIndex);

	m_FunctionName = s->information ? s->information->name : "?";
	m_FunctionPath = m_Stack.back().path + m_FunctionName;

	return true;
}


#endif // #if UNITY_EDITOR

#endif // #if ENABLE_PROFILER
