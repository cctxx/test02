#include "UnityPrefix.h"
#include "ProfilerHistory.h"

#if ENABLE_PROFILER && UNITY_EDITOR

#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"
#include "Runtime/Network/PlayerCommunicator/EditorConnection.h"
#include "ProfilerStats.h"
#include "Profiler.h"
#include "GPUProfiler.h"
#include "Runtime/Utilities/Word.h"
#include "ProfilerImpl.h"
#include "ProfilerProperty.h"
#include "ProfilerConnection.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Shaders/GraphicsCaps.h"

#include <fstream>
#include <sstream>
#include "Runtime/Misc/SystemInfo.h"

static const ProfilerSample* CalculateSampleAtPath (const ProfilerSample* sample, const char* path, dynamic_array<ProfilerSample*>& outputSamples);

ProfilerHistory* ProfilerHistory::ms_Instance = NULL;

// ProfilerHistory class implementation
//

void ProfilerHistory::Initialize()
{
	Assert(ms_Instance == NULL);
	ms_Instance = new ProfilerHistory();
}

void ProfilerHistory::Cleanup()
{
	Assert(ms_Instance != NULL);
	delete ms_Instance;
	ms_Instance = NULL;
}

ProfilerHistory::ProfilerHistory() 
	: m_MaxFrameHistoryLength(300)
	, m_FrameCounter(0)
	, m_BytesUsedLastFrame(0u)
	, m_HistoryPosition(-1)
	, m_Frames(kMemProfiler)
	, m_Properties(kMemProfiler)
{	
	InitializeStatisticsProperties(m_Properties);

	m_FramesWithGPUData = 0;
#if SUPPORT_THREADS
	m_MainThreadID = Thread::GetCurrentThreadID();
#endif
}

ProfilerHistory::~ProfilerHistory() 
{
	CleanupFrameHistory();
}


static inline int FindSeperator (const char* in)
{
	const char* c = in;
	while (*c != '/' && *c != '\0')
		c++;
	return c - in;
}

static const ProfilerSample* CalculateSampleAtPath (const ProfilerSample* sample, const char* path, dynamic_array<ProfilerSample*>& outputSamples)
{
	int seperatorIndex = FindSeperator(path);

	const ProfilerSample* child = sample + 1;
	for (int i=0;i<sample->nbChildren;i++)
	{
		if (strncmp(child->information->name, path, seperatorIndex) == 0)
		{	
			if (seperatorIndex == strlen (path))
			{	
				outputSamples.push_back(const_cast<ProfilerSample*>(child));
				child = SkipSampleRecurse(child);
			}
			else
			{
				child = CalculateSampleAtPath(child, path + seperatorIndex + 1, outputSamples);
			}
		}
		else
		{
			child = SkipSampleRecurse(child);
		}
	}

	return child;
}

static void AddToChart(ChartSample& data, int group, int sampleTime, int direction)
{
	// Add times to their groups
	if (group == kProfilerRender)
		data.rendering += sampleTime * direction;
	else if (group == kProfilerScripts)
		data.scripts += sampleTime * direction;
	else if (group == kProfilerPhysics)
		data.physics += sampleTime * direction;
	else if (group == kProfilerGC)
		data.gc += sampleTime * direction;
	else if (group == kProfilerVSync)
		data.vsync += sampleTime * direction;
	else
		data.others += sampleTime * direction;
}




// Adds the self time of each sample into the group.
// Expects that ChartSample has all elements set to zero, except the other time which needs to be the root time 
// of the root sample.
static const ProfilerSample* RecursiveAdjustChartForGroupChange(ChartSample& data, const ProfilerSample* root, int direction)
{
	Assert(root != NULL);

	const ProfilerSample* childSample = root + 1;

	// Walk through the children and collect chart information recursively
	for (int i=0;i<root->nbChildren;i++)
	{
		//@TODO: Make this cleaner
		if (root->information == NULL || root->information->group != childSample->information->group)
		{
			int childSampleTime = (int) childSample->timeUS*1000;
			
			// Add times to their groups
			AddToChart(data, childSample->information->group, childSampleTime, direction);
			
			// Remove this sample time from the group the parent is in, thus we end up with only the self time of the parent
			if (root->information != NULL)
				AddToChart(data, root->information->group, childSampleTime, -direction);
		}

		childSample = RecursiveAdjustChartForGroupChange(data, childSample, direction);
	}
	
	return childSample;
}

static void GatherChartInformation (ChartSample& data, const ProfilerSample* sample, int size)
{
	const ProfilerSample* end = RecursiveAdjustChartForGroupChange(data, sample, 1);
	Assert(end - sample == size || size == -1);
}

static void GatherGPUChartInformation (ChartSample& data, const ProfilerData::GPUTime* sample, int size)
{
	data.hasGPUProfiler = size != 0;
	for(int i = 0; i < size; i++)
	{
		int sampletime = sample[i].gpuTimeInMicroSec*1000;
		switch(sample[i].gpuSection)
		{
		case kGPUSectionShadowPass:
			data.gpuShadows += sampletime;
			break;
		case kGPUSectionOpaquePass:
			data.gpuOpaque += sampletime;
			break;
		case kGPUSectionTransparentPass:
			data.gpuTransparent += sampletime;
			break;
		case kGPUSectionPostProcess:
			data.gpuPostProcess += sampletime;
			break;
		case kGPUSectionDeferedPrePass:
			data.gpuDeferredPrePass += sampletime;
			break;
		case kGPUSectionDeferedLighting:
			data.gpuDeferredLighting += sampletime;
			break;
		default:
			data.gpuOther += sampletime;
			break;
		};
	}
}

void ProfilerHistory::AddFrameDataAndTransferOwnership (ProfilerFrameData* frame, int guid)
{
	Assert(UNITY_EDITOR);
	
	if(!AddFrame(frame, guid))
		UNITY_DELETE(frame, kMemProfiler);
}

bool ProfilerHistory::AddFrame (ProfilerFrameData* frame, int identifier)
{
	if (ProfilerConnection::Get().GetConnectedProfiler() != identifier)
		return false;

	// Fill in group timing information for the charts			
	ChartSample& activeChartSample = frame->allStats.chartSample;
	activeChartSample = ChartSample();
	
	const ProfilerFrameData::ThreadData& tdata = frame->m_ThreadData[0];
	GatherChartInformation(activeChartSample, tdata.m_AllSamples.begin(), tdata.m_AllSamples.size());
	GatherGPUChartInformation(activeChartSample, tdata.m_GPUTimeSamples.begin(), tdata.m_GPUTimeSamples.size());
	
	CalculateSelectedTimeAndChart(*frame);

#if SUPPORT_THREADS
	Assert (Thread::EqualsCurrentThreadID(m_MainThreadID));
#endif

	if (m_Frames.size() >= m_MaxFrameHistoryLength)
		KillOneFrame();

	frame->frameIndex = m_FrameCounter++;

	// We count frames with GPU data to detect if there is any GPU profiling data
	if (frame->allStats.chartSample.hasGPUProfiler)
		m_FramesWithGPUData++;
	
	m_Frames.push_back(frame);
	return true;
}


void ProfilerHistory::CalculateSelectedTimeAndChart (ProfilerFrameData& frame)
{
	frame.selectedTime = 0;
	frame.allStats.chartSampleSelected = ChartSample();
	if (!m_SelectedPropertyPath.empty())
	{
		const ProfilerFrameData::ThreadData& tdata = frame.m_ThreadData[0];
		dynamic_array<ProfilerSample*> selectedSamples;
		const ProfilerSample* endSample = CalculateSampleAtPath(tdata.GetRoot(), m_SelectedPropertyPath.c_str(), selectedSamples);
		Assert(endSample - tdata.GetRoot() == tdata.m_AllSamples.size());
		
		for (int i=0;i<selectedSamples.size();i++)
		{	
			ProfilerSample* sample = selectedSamples[i];
			
			AddToChart(frame.allStats.chartSampleSelected, sample->information->group, (int)sample->timeUS*1000, 1);
			GatherChartInformation(frame.allStats.chartSampleSelected, sample, -1);
			frame.selectedTime += sample->timeUS*1000;
		}
	}
}

void ProfilerHistory::SetSelectedPropertyPath (const std::string& samplePath)
{
	if (m_SelectedPropertyPath != samplePath)
	{
		m_SelectedPropertyPath = samplePath;
		
		// Recompute cached selected frame time from new path
		for (int i=0;i<m_Frames.size();i++)
			CalculateSelectedTimeAndChart(*m_Frames[i]);
	}
}

bool ProfilerHistory::IsGPUProfilerBuggyOnDriver ()
{
	return gGraphicsCaps.buggyTimerQuery;
}

bool ProfilerHistory::IsGPUProfilerSupportedByOS ()
{
#if UNITY_OSX
	return systeminfo::GetOperatingSystemNumeric() >= 1070;
#else
	return true;
#endif
}

bool ProfilerHistory::IsGPUProfilerSupported ()
{
	if (m_Frames.empty())
		return true;
	else
		return m_FramesWithGPUData != 0;
}


void ProfilerHistory::KillOneFrame()
{
	if (m_Frames.size() < 3)
		return;
		
	// Kill first or second frame. But always keep the frame the user is currently viewing (m_HistoryPosition)
	int killIndex = 0;
	if (m_Frames[0]->frameIndex == m_HistoryPosition)
		killIndex = 1;

	// We count frames with GPU data to detect if there is any GPU profiling data
	if (m_Frames[killIndex]->allStats.chartSample.hasGPUProfiler)
		m_FramesWithGPUData--;
	
	UNITY_DELETE(m_Frames[killIndex], kMemProfiler);
	m_Frames.erase(&m_Frames[killIndex] , &m_Frames[killIndex+1]);
	
}

const ProfilerFrameData* ProfilerHistory::GetLastFrameData() const
{
	if (!m_Frames.empty())
		return m_Frames.back();
	else
		return NULL;
}

ProfilerFrameData* ProfilerHistory::GetLastFrameData()
{
	if (!m_Frames.empty())
		return m_Frames.back();
	else
		return NULL;
}

const ProfilerFrameData* ProfilerHistory::GetFrameData(int frame) const
{
	if (frame == -1)
		return GetLastFrameData();
	else
	{
		if (m_Frames.size () > 1)
		{
			int index = m_Frames.size() - (m_Frames.back()->frameIndex - frame) - 1;
			if (index >= 0 && index < m_Frames.size() && m_Frames[index]->frameIndex == frame)
				return m_Frames[index];
		}

		int size = m_Frames.size();
		for (int i=0;i<size;i++)
		{
			if (frame == m_Frames[i]->frameIndex)
				return m_Frames[i];
		}
		return NULL;
	}	
}

ProfilerFrameData* ProfilerHistory::GetFrameData(int frame)
{
	if (frame == -1)
		return GetLastFrameData();
	else
	{
		if (m_Frames.size () > 1)
		{
			int index = m_Frames.size() - (m_Frames.back()->frameIndex - frame) - 1;
			if (index >= 0 && index < m_Frames.size() && m_Frames[index]->frameIndex == frame)
				return m_Frames[index];
		}
		
		int size = m_Frames.size();
		for (int i=0;i<size;i++)
		{
			if (frame == m_Frames[i]->frameIndex)
				return m_Frames[i];
		}
		return NULL;
	}	
}

int ProfilerHistory::GetFirstFrameIndex ()
{
	if (m_Frames.size() >= 1)
		return m_Frames[0]->frameIndex;
	else
		return -1;
}

int ProfilerHistory::GetLastFrameIndex ()
{
	if (!m_Frames.empty())
		return m_Frames.back()->frameIndex;
	else
		return -1;
}

int ProfilerHistory::GetNextFrameIndex (int frame)
{
	if (frame == -1)
		return -1;
	
	int size = m_Frames.size();
	for (int i=0;i<size;i++)
	{
		if (m_Frames[i]->frameIndex > frame)
			return m_Frames[i]->frameIndex;
	}
	return -1;
}

int ProfilerHistory::GetPreviousFrameIndex (int frame)
{
	if (frame == -1)
		frame = std::numeric_limits<int>::max();
	
	int size = m_Frames.size();
	for (int i=size-1;i>=0;i--)
	{
		if (m_Frames[i]->frameIndex < frame)
			return m_Frames[i]->frameIndex;
	}
	return -1;
}

void ProfilerHistory::CleanupFrameHistory()
{
	for (int i = 0; i < m_Frames.size(); ++i)
		UNITY_DELETE(m_Frames[i], kMemProfiler);
	m_Frames.clear();
	
	m_FrameCounter = 0;	
	m_HistoryPosition = -1;
	m_BytesUsedLastFrame = 0u;

	UnityProfiler::Get().ClearPendingFrames();
}

static std::string FormatCount (int b)
{
	if (b < 1000)
		return Format ("%i", b);
	if (b < 1000000)
		return Format ("%01.1fk", b/1000.0);
	b /= 1000;
	return Format ("%01.1fM", b/1000.0);
}

std::string ProfilerHistory::GetFormattedStatisticsValue(ProfilerFrameData& frameData, int identifier)
{
	Assert (identifier >= 0 && identifier < m_Properties.size());
	
	int val = GetStatisticsValue(frameData,identifier);
	if (val == -1)
		return "";
	
	switch(m_Properties[identifier].format) {
		case kFormatTime: return Format("%i.%ims", val/1000000, val%1000000/100000);
		case kFormatCount: return FormatCount (val);
		case kFormatBytes: return FormatBytes(val);
		case kFormatPercentage: return Format("%i.%i %%", val / 10, val % 10);
		default: AssertString("unknown format"); return "";
	}
}

std::string ProfilerHistory::GetFormattedStatisticsValue(int frame, int identifier)
{
	Assert (identifier >= 0 && identifier < m_Properties.size());
	
	ProfilerFrameData* frameData = GetFrameData(frame);
	if (frameData == NULL)
		return "";
	return GetFormattedStatisticsValue (*frameData, identifier);
}

int ProfilerHistory::GetStatisticsValue(ProfilerFrameData& data, int identifier)
{
	if (identifier < 0 || identifier >= m_Properties.size())
		return -1;
	
	return ::GetStatisticsValue(m_Properties[identifier].offset, data.allStats);
}


void ProfilerHistory::GetStatisticsValuesBatch(int identifier, int firstValue, float scale, float* buffer, int size, float* outMaxValue)
{
	int offset = -1;
	if (identifier >= 0 && identifier < m_Properties.size())
		offset = m_Properties[identifier].offset;

	*outMaxValue = 0;

	for (int i=0;i<size;i++)
	{
		int frame = firstValue + i;
		int value = -1;
		
		///@TODO: Optimize GetFrameData
		if (frame >= 0 && offset != -1)
		{
			ProfilerFrameData* data = GetFrameData(frame);
			if (data)
				value = ::GetStatisticsValue(offset, data->allStats);
		}
		if (value >= 0)
		{
			float scaledValue = double(value) * double(scale);
			buffer[i] = scaledValue;
			*outMaxValue = std::max(*outMaxValue, scaledValue);
		}
		else
		{
			buffer[i] = -1;
		}
	}
}

int ProfilerHistory::GetStatisticsIdentifier(const std::string& statName)
{
	for (size_t i = 0; i < m_Properties.size(); ++i)
		if (statName == m_Properties[i].name)
			return i;
	return -1;
}

void ProfilerHistory::GetAllStatisticsProperties(std::vector<std::string>& all)
{
	all.reserve(m_Properties.size());
	for (size_t i = 0; i < m_Properties.size(); ++i)
		all.push_back(m_Properties[i].name);
}

void ProfilerHistory::GetGraphStatisticsPropertiesForArea(ProfilerArea area, std::vector<std::string>& all)
{
	all.reserve(m_Properties.size());
	for (size_t i = 0; i < m_Properties.size(); ++i)
	{
		if (area == m_Properties[i].area && m_Properties[i].showGraph)
			all.push_back(m_Properties[i].name);
	}
}

ProfilerString ProfilerHistory::GetOverviewTextForProfilerArea (int frame, ProfilerArea profilerArea)
{
	ProfilerFrameData* frameData = GetFrameData(frame);
	if (frameData == NULL)
		return "";
	
	/////@TODO: USE ONLY GENERIC CODE BELOW> STOP HARDCODING SHIT
	if (profilerArea == kProfilerAreaCPU)
	{
		
	}
	else if (profilerArea == kProfilerAreaRendering)
		return frameData->allStats.drawStats.ToString();
	else if (profilerArea == kProfilerAreaMemory)
		return frameData->allStats.memoryStats.ToString();
	
	TEMP_STRING overview;
	for (int i=0;i<m_Properties.size();i++)
	{
		StatisticsProperty& prop = m_Properties[i];
		if (prop.area != profilerArea)
			continue;
		
		overview += FormatString<TEMP_STRING>("%s: %s\n", prop.name.c_str(), GetFormattedStatisticsValue(*frameData, i).c_str());
	}
	
	return overview.c_str();
}

#endif // ENABLE_PROFILER 
