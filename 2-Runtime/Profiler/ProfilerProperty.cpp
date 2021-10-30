#include "UnityPrefix.h"
#include "ProfilerProperty.h"

#if ENABLE_PROFILER && UNITY_EDITOR

#include "ProfilerImpl.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include <iostream>
#include <sstream>
#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/ArrayUtility.h"

/*
TODO:
* Sync time filter does not work
* non multiple selection in raw hierarch

* select object in double click
* Tooltip showing all names
 * deep profiler???
 */



// Constant and static variables initialization
static const char* const kRadixPoint = "0.";
static const char* const kPercentFmt = "%d.%d%%";
static const char* kNotAvailable = "N/A";


struct ProfilerHierarchy
{
	ProfilerHierarchy() : oneSample(~0), sampleIdx(kMemProfiler) {}

	// Can correspond to one or more individual samples. Optimize common use case (especially in timeline view)
	// when there is exactly one sample: that is stored in 'oneSample'. If there are more samples, they
	// are put into an array.
	UInt32					oneSample;
	dynamic_array<UInt32>	sampleIdx;

	UNITY_VECTOR(kMemProfiler,ProfilerHierarchy) children;
	ProfilerHierarchy*                           parent;

	ProfilerSampleData                           data;
};

// Implement swap to avoid massive amounts of copying during the sort function
namespace std
{
	template<>
	inline void swap(ProfilerHierarchy& __a, ProfilerHierarchy& __b)
	{
		std::swap(__a.oneSample, __b.oneSample);
		__a.sampleIdx.swap(__b.sampleIdx);
		__a.children.swap(__b.children);
		std::swap(__a.parent, __b.parent);
		std::swap(__a.data, __b.data);
	}
}


static std::string GetFormattedPercent(ProfileTimeFormat inTime, ProfileTimeFormat totalTime)
{
	if (totalTime == 0)
		return "0.0%";
	
	// Calculate percentage, format obtained value and convert into string 
	UInt64 time = inTime * (UInt64) 1000;
	UInt64 integer = time / totalTime;
	
	return Format(kPercentFmt, (int) integer / 10, (int) integer % 10);	
}

static std::string GetFormattedTime(ProfileTimeFormat time, int decimals = 2)
{
	time /= (1000000/Pow(10,decimals));
	
	std::string value = Format("%u", time);
	
	int length = value.length();
	
	if (length > decimals)
	{
		value.insert(length - decimals, 1, '.');
	}
	else
	{
		std::string tmp;
		tmp.assign(decimals, '0');
		
		value.copy((char*) tmp.c_str() + (decimals - length), length);
		
		return kRadixPoint + tmp;
	}
	
	return value;
}

static const char* GetObjectNameFromInstanceID (SInt32 instanceID)
{
	Object* obj = dynamic_instanceID_cast<Object*>(instanceID);
	if (obj)
		return obj->GetName();
	else
		return kNotAvailable;
}

static const char* GetFunctionName(const ProfilerSampleData& input)
{
	if (input.information)
		return input.information->name;
	else
		return "Invalid";
}

static bool LargerFunctionName(const ProfilerSampleData& lhs, const ProfilerSampleData& rhs)
{
	
	if (!lhs.information || !rhs.information)
		return &lhs < &rhs;
	
	return StrICmp(lhs.information->name, rhs.information->name) > 0;
}


static void GetSortedInstanceIDs (const dynamic_array<AdditionalProfilerSampleData*>& additionalData, UInt32 oneSample, const dynamic_array<UInt32>& samples, dynamic_array<SInt32>& outputInstanceIDs)
{
	// Get sorted list of instanceID's
	outputInstanceIDs.push_back(additionalData[oneSample]?additionalData[oneSample]->instanceID:0);
	for (int i=0;i<samples.size();i++)
		outputInstanceIDs.push_back(additionalData[samples[i]]?additionalData[samples[i]]->instanceID:0);
	std::sort (outputInstanceIDs.begin(), outputInstanceIDs.end());
}

static const char* GetObjectNameSummary (UInt32 oneSample, const dynamic_array<UInt32>& samples, const dynamic_array<AdditionalProfilerSampleData*>& additionalData)
{
	int instanceID = additionalData[oneSample] ? additionalData[oneSample]->instanceID : 0;
	const char* name = GetObjectNameFromInstanceID(instanceID);
	
	for (int i=0;i<samples.size();i++)
	{	
		int instanceID = additionalData[samples[i]]?additionalData[samples[i]]->instanceID:0;
		const char* tempName = GetObjectNameFromInstanceID(instanceID);
		if (name != tempName) //@TODO: this is just comparing pointers?!
			return "multiple";
	}

	return name;
}

static std::string GetProfilerColumn (const ProfilerSampleData& data, UInt32 oneSample, const dynamic_array<UInt32>& samples, const dynamic_array<AdditionalProfilerSampleData*>& additionalData, bool supportsGPUProfiler, ProfilerColumn column)
{
	switch (column)
	{
		case kFunctionNameColumn:
			return GetFunctionName(data);
			
		case kTotalPercentColumn:
			return GetFormattedPercent(data.time, data.rootTime);
			
		case kSelfPercentColumn:
			if (data.time > data.childrenTime)
				return GetFormattedPercent(data.time - data.childrenTime, data.rootTime);
			else
				return GetFormattedPercent(0u, 1u);

		case kTotalGPUPercentColumn:
			if (!supportsGPUProfiler)
				return kNotAvailable;
			else if (supportsGPUProfiler)
				return GetFormattedPercent(data.gpuTime, data.rootGPUTime);
			else
				return kNotAvailable;
			
		case kSelfGPUPercentColumn:
			if (!supportsGPUProfiler)
				return kNotAvailable;
			else if (data.gpuTime > data.childrenGPUTime)
				return GetFormattedPercent(data.gpuTime - data.childrenGPUTime, data.rootGPUTime);
			else
				return GetFormattedPercent(0u, 1u);
			
		case kCallsColumn:
			return Format("%u", data.numberOfCalls);

		case kWarningColumn:
			if(data.warningCount == 0)
				return "";
			return Format("%u", data.warningCount);

		case kGCMemory:
			return FormatBytes(data.allocatedGCMemory);
			
		case kTotalTimeColumn: 
			return GetFormattedTime(data.time);
			
		case kSelfTimeColumn:
			if (data.time > data.childrenTime)
				return GetFormattedTime(data.time - data.childrenTime);
			else
				return GetFormattedTime(0u);
			
		case kDrawCallsColumn:
			if (supportsGPUProfiler)
				return Format("%u", data.gpuSamplesCount);
			else
				return kNotAvailable;
			
		case kTotalGPUTimeColumn:
			if (supportsGPUProfiler)
				return GetFormattedTime(data.gpuTime,3);
			else
				return kNotAvailable;

		case kSelfGPUTimeColumn:
			if (!supportsGPUProfiler)
				return kNotAvailable;
			else if (data.gpuTime > data.childrenGPUTime)
				return GetFormattedTime(data.gpuTime - data.childrenGPUTime);
			else
				return GetFormattedTime(0u);
			
		case kObjectNameColumn:
			
			return GetObjectNameSummary(oneSample, samples, additionalData);
			
		default:
			AssertString("Unimplemented GetProfilerColumn ");
			return "";
	}
}


static bool IsSmaller (const ProfilerSampleData& lhs, const ProfilerSampleData& rhs, ProfilerColumn criteria)
{
	if (criteria == kTotalTimeColumn || criteria == kTotalPercentColumn)
	{
		if (lhs.time != rhs.time)
			return lhs.time < rhs.time;
	}
	else if (criteria == kTotalGPUTimeColumn || criteria == kTotalGPUPercentColumn)
	{
		if (lhs.gpuTime != rhs.gpuTime)
			return lhs.gpuTime < rhs.gpuTime;
	}
	else if (criteria == kSelfTimeColumn || criteria == kSelfPercentColumn)
	{	
		if ((lhs.time - lhs.childrenTime) != (rhs.time - rhs.childrenTime))
			return (lhs.time - lhs.childrenTime)  < (rhs.time - rhs.childrenTime);
	}
	else if (criteria == kSelfGPUTimeColumn || criteria == kSelfGPUPercentColumn)
	{
		if ((lhs.gpuTime - lhs.childrenGPUTime) != (rhs.gpuTime - rhs.childrenGPUTime))
			return (lhs.gpuTime - lhs.childrenGPUTime)  < (rhs.gpuTime - rhs.childrenGPUTime);
	}
	else if (criteria == kGCMemory)
	{
		if (lhs.allocatedGCMemory != rhs.allocatedGCMemory)
			return lhs.allocatedGCMemory < rhs.allocatedGCMemory;
	}
	else if (criteria == kDrawCallsColumn)
	{
		if (lhs.gpuSamplesCount != rhs.gpuSamplesCount)
			return lhs.gpuSamplesCount < rhs.gpuSamplesCount;
	}
	else if (criteria == kFunctionNameColumn)
	{
		return LargerFunctionName(lhs, rhs);
	}
	else if (criteria == kWarningColumn)
	{
		return lhs.warningCount <  rhs.warningCount;
	}
	else if (criteria == kCallsColumn)
	{
		if (lhs.numberOfCalls != rhs.numberOfCalls)
			return lhs.numberOfCalls < rhs.numberOfCalls;
	}
	else if (criteria == kObjectNameColumn)
	{	
		// Not supported. Screws with the data flow and not worth it...
		return LargerFunctionName(lhs, rhs);
	}
	else
	{
		AssertString("Unsupported sortmode");
		return false;
	}
	
	// As a fallback if the results are the same. Sort by name
	return LargerFunctionName(lhs, rhs);
}


// ProfilerProperty class implementation
//

MemoryPool ProfilerProperty::s_AdditionalDataPool(false, "AdditionalData Pool", sizeof (AdditionalProfilerSampleData), 16 * 1024, kMemProfiler);

ProfilerProperty::ProfilerProperty()
	:m_Root(NULL)
	,m_FrameData(NULL)
	,m_ProfilerViewType(kViewHierarchy)
	,m_ProfilerSortColumn(kTotalTimeColumn)
	,m_Depth(0)
	,m_ActiveHierarchy(NULL)
	,m_OnlyShowGPUSamples(false)
	,m_AdditionalData(kMemProfiler)
	,m_ThreadIdx(0)
{
}

ProfilerProperty::~ProfilerProperty() 
{
	CleanupProperty();
}

struct SortByColumn
{
	ProfilerColumn column;
	
	SortByColumn (ProfilerColumn c) : column (c) { } 
	
	bool operator () (const ProfilerHierarchy& lhs, const ProfilerHierarchy& rhs) const
	{
		return !IsSmaller(lhs.data, rhs.data, column);
	}
};

struct SortByFunctionName
{
	const ProfilerFrameData& frameData;
	int threadIdx;
	
	SortByFunctionName (const ProfilerFrameData& f, int t) : frameData (f), threadIdx(t) { } 
	
	bool operator () (const UInt32 lhs, const UInt32 rhs) const
	{
		const char* lhsName = frameData.m_ThreadData[threadIdx].GetSample(lhs)->information->name;
		const char* rhsName = frameData.m_ThreadData[threadIdx].GetSample(rhs)->information->name;
		return strcmp(lhsName, rhsName) < 0;
	}
};

static AdditionalProfilerSampleData* AllocateAdditionalData()
{
	void* ptr = ProfilerProperty::s_AdditionalDataPool.Allocate();
	memset(ptr, 0, sizeof(AdditionalProfilerSampleData));
	return (AdditionalProfilerSampleData*) ptr;
}

static void InitializeAdditionalProfilerData(const ProfilerFrameData& frameData, int threadIdx, dynamic_array<AdditionalProfilerSampleData*>& additionalData)
{
	const ProfilerFrameData::ThreadData& tdata = frameData.m_ThreadData[threadIdx];
	UInt32 sampleCount = tdata.m_AllSamples.size();
	
	for(int i = 0; i < additionalData.size(); i++)
		ProfilerProperty::s_AdditionalDataPool.Deallocate(additionalData[i]);

	additionalData.resize_uninitialized(sampleCount);
	if (sampleCount > 0)
		memset(&additionalData[0], 0, sampleCount*sizeof(AdditionalProfilerSampleData*));

	dynamic_array<ProfilerData::AllocatedGCMemory>::const_iterator itGCMem = tdata.m_AllocatedGCMemorySamples.begin();
	for(; itGCMem != tdata.m_AllocatedGCMemorySamples.end(); ++itGCMem)
	{
		if(additionalData[itGCMem->relatedSampleIndex] == 0)
			additionalData[itGCMem->relatedSampleIndex] = AllocateAdditionalData();
		additionalData[itGCMem->relatedSampleIndex]->allocatedGCMemory += itGCMem->allocatedGCMemory;
	}

	dynamic_array<ProfilerData::InstanceID>::const_iterator itID = tdata.m_InstanceIDSamples.begin();
	for(; itID != tdata.m_InstanceIDSamples.end(); ++itID)
	{
		if(additionalData[itID->relatedSampleIndex] == 0)
			additionalData[itID->relatedSampleIndex] = AllocateAdditionalData();
		additionalData[itID->relatedSampleIndex]->instanceID = itID->instanceID;
	}
	dynamic_array<ProfilerData::GPUTime>::const_iterator itGPUTime = tdata.m_GPUTimeSamples.begin();
	for(; itGPUTime != tdata.m_GPUTimeSamples.end(); ++itGPUTime)
	{
		if(additionalData[itGPUTime->relatedSampleIndex] == 0)
			additionalData[itGPUTime->relatedSampleIndex] = AllocateAdditionalData();
		additionalData[itGPUTime->relatedSampleIndex]->gpuTime += itGPUTime->gpuTimeInMicroSec*1000;
		additionalData[itGPUTime->relatedSampleIndex]->drawCalls++;
		additionalData[itGPUTime->relatedSampleIndex]->gpuSection = itGPUTime->gpuSection; 
	}

	dynamic_array<UInt32>::const_iterator itWarningSample = tdata.m_WarningSamples.begin();
	for(; itWarningSample != tdata.m_WarningSamples.end(); ++itWarningSample)
	{
		if(additionalData[*itWarningSample] == 0)
			additionalData[*itWarningSample] = AllocateAdditionalData();
		additionalData[*itWarningSample]->warningCount += 1;
	}
}


static UInt32 FillAdditionalProfilerData(const ProfilerFrameData& frameData, int threadIdx, dynamic_array<AdditionalProfilerSampleData*>& additionalData, UInt32 index = 0)
{
	const ProfilerFrameData::ThreadData& tdata = frameData.m_ThreadData[threadIdx];
	if (tdata.m_AllSamples.empty())
		return 0;
	
	int childIndex = index+1;
	for(int i = 0; i < tdata.GetSample(index)->nbChildren; i++)
	{
		int nextChild = FillAdditionalProfilerData(frameData, threadIdx, additionalData, childIndex);
		if(additionalData[childIndex])
		{
			if(additionalData[index] == NULL)
				additionalData[index] = AllocateAdditionalData();
			additionalData[index]->gpuTime += additionalData[childIndex]->gpuTime;
			additionalData[index]->drawCalls += additionalData[childIndex]->drawCalls;
			additionalData[index]->allocatedGCMemory += additionalData[childIndex]->allocatedGCMemory;
			additionalData[index]->warningCount += additionalData[childIndex]->warningCount;
		}
		childIndex = nextChild;
	}
	return childIndex;
}

static void ProcessProfilerOneSample (const ProfilerFrameData::ThreadData& tdata, UInt32 sampleIndex, ProfilerSampleData& data, const dynamic_array<AdditionalProfilerSampleData*>& additionalData)
{
	const ProfilerSample* sample = tdata.GetSample(sampleIndex);
	data.time += sample->timeUS*1000;
	if(additionalData[sampleIndex])
	{
		data.allocatedGCMemory += additionalData[sampleIndex]->allocatedGCMemory;
		data.gpuTime += additionalData[sampleIndex]->gpuTime;
		data.gpuSamplesCount += additionalData[sampleIndex]->drawCalls;
		data.warningCount += additionalData[sampleIndex]->warningCount;
	}
	///@TODO: maybe not depend on data structure of samples here...
	const ProfilerSample* child = sample + 1;
	for (int c=0;c<sample->nbChildren;c++)
	{
		UInt32 childSampleIndex = child - tdata.GetRoot();
		data.childrenTime += child->timeUS*1000;

		if(additionalData[childSampleIndex])
		{
			data.childrenGPUTime += additionalData[childSampleIndex]->gpuTime;
			data.childrenGPUSamplesCount += additionalData[childSampleIndex]->drawCalls;
		}
		child = SkipSampleRecurse(child);
	}
}

static void ProcessProfilerSampleData (const ProfilerFrameData& frameData, int threadIdx, ProfilerHierarchy& hierarchy, const dynamic_array<AdditionalProfilerSampleData*>& additionalData)
{
	const ProfilerFrameData::ThreadData& tdata = frameData.m_ThreadData[threadIdx];
	Assert(hierarchy.oneSample < tdata.m_AllSamples.size());

	ProfilerSampleData& data = hierarchy.data;
	data.time = 0;
	data.childrenTime = 0;
	data.childrenGPUTime = 0;
	data.childrenGPUSamplesCount = 0;
	data.allocatedGCMemory = 0;
	data.gpuTime = 0;
	data.gpuSamplesCount = 0;
	data.startTime = 0;
	data.numberOfCalls = 1 + hierarchy.sampleIdx.size();
	data.warningCount = 0;

	ProcessProfilerOneSample (tdata, hierarchy.oneSample, data, additionalData);
	for (int i=0;i<hierarchy.sampleIdx.size();i++)
		ProcessProfilerOneSample (tdata, hierarchy.sampleIdx[i], data, additionalData);

	UInt32 firstIndex = hierarchy.oneSample;
	data.rootTime = tdata.GetRoot()->timeUS*1000;
	if(additionalData[0])
		data.rootGPUTime = additionalData[0]->gpuTime;
	data.information = tdata.GetSample(firstIndex)->information;
	data.startTime = tdata.GetSample(firstIndex)->startTimeUS;
}

std::string ProfilerProperty::GetProfilerColumn (ProfilerColumn column) const
{
	return ::GetProfilerColumn(m_ActiveHierarchy->data, m_ActiveHierarchy->oneSample, m_ActiveHierarchy->sampleIdx, m_AdditionalData, SupportsGPUProfiler(), column);
}


static void BuildHierarchyLevel (const ProfilerFrameData& frameData, int threadIdx, dynamic_array<UInt32>& allSamplesThisLevel, ProfilerHierarchy* parent, ProfilerColumn sortColumn, ProfilerViewType viewType, const dynamic_array<AdditionalProfilerSampleData*>& additionData)
{
	/////@TODO: Dont do this.
	if (!parent->children.empty())
		return;
	
	Assert(parent->children.empty()); // should only be called once to build child list
	
	const ProfilerFrameData::ThreadData& tdata = frameData.m_ThreadData[threadIdx];	
	
	// hierarchy merging
	if (viewType == kViewHierarchy)
	{
		std::sort (allSamplesThisLevel.begin(), allSamplesThisLevel.end(), SortByFunctionName(frameData,threadIdx));

		const char * lastUniqueName = NULL;
		int count = 0;
		for (int i=0;i<allSamplesThisLevel.size();i++)
		{
			const char* name = tdata.GetSample(allSamplesThisLevel[i])->information->name;
			if (lastUniqueName == NULL || strcmp(name, lastUniqueName) != 0)
				count++;
		}
		parent->children.reserve(count);
		
		lastUniqueName = NULL;
		for (int i=0;i<allSamplesThisLevel.size();i++)
		{
			const char* name = tdata.GetSample(allSamplesThisLevel[i])->information->name;
			if (lastUniqueName == NULL || strcmp(name, lastUniqueName) != 0)
			{
				lastUniqueName = name;
				parent->children.push_back(ProfilerHierarchy());
				parent->children.back().oneSample = allSamplesThisLevel[i];
				continue;
			}

			ProfilerHierarchy& element = parent->children.back();
			element.sampleIdx.push_back(allSamplesThisLevel[i]);
		}
		
	}
	// raw data. no merging of data is necessary
	else
	{
		const size_t size = allSamplesThisLevel.size();
		parent->children.resize(size, ProfilerHierarchy());
		for (size_t i = 0; i < size; ++i)
		{
			ProfilerHierarchy& element = parent->children[i];
			element.oneSample = allSamplesThisLevel[i];
		}
	}
	
	// Calculate profiler display data for all generated children
	for (int i=0;i<parent->children.size();i++)
	{
		ProfilerHierarchy& element = parent->children[i];
		element.parent = parent;
		ProcessProfilerSampleData (frameData, threadIdx, element, additionData);
	}
	
	// Sort by selected column
	// IMPORTANT: stable_sort is necessary here. std::sort crashes since it doesn't take advantage of the std::swap of ProfilerHierarchy.
	if (sortColumn != kDontSortProfilerColumn)
		std::stable_sort (parent->children.begin(), parent->children.end(), SortByColumn(sortColumn));
}

static void BuildHierarchyLevel (ProfilerFrameData& frameData, int threadIdx, ProfilerHierarchy* parent, ProfilerColumn sortColumn, ProfilerViewType viewType, const dynamic_array<AdditionalProfilerSampleData*>& additionData)
{
	const ProfilerFrameData::ThreadData& tdata = frameData.m_ThreadData[threadIdx];	
	
	int childCount = tdata.GetSample(parent->oneSample)->nbChildren;
	for(int i = 0; i < parent->sampleIdx.size(); i++)
		childCount += tdata.GetSample(parent->sampleIdx[i])->nbChildren;

	dynamic_array<UInt32> allSamplesThisLevel(kMemTempAlloc);
	allSamplesThisLevel.reserve(childCount);
	tdata.ExtractAllChildSamples(parent->oneSample, allSamplesThisLevel);
	for(int i = 0; i < parent->sampleIdx.size(); i++)
		tdata.ExtractAllChildSamples(parent->sampleIdx[i], allSamplesThisLevel);
	
	BuildHierarchyLevel(frameData, threadIdx, allSamplesThisLevel, parent, sortColumn, viewType, additionData);
}


void ProfilerProperty::GetInstanceIDs(dynamic_array<SInt32>& instanceIDs)
{
	GetSortedInstanceIDs (m_AdditionalData, m_ActiveHierarchy->oneSample, m_ActiveHierarchy->sampleIdx, instanceIDs);
}

std::string ProfilerProperty::GetTooltip (ProfilerColumn column)
{
	if (column == kTotalGPUTimeColumn)
	{
		if (!SupportsGPUProfiler ())
			return "The platform you are profiling does not support GPU profiling\nMac OS X 10.7 and higher supports profiling, many mobile platforms have no builtin GPU profiling capabilities.";
	}
	
	return "";
}



void ProfilerProperty::SetRoot(int frame, ProfilerColumn sortColumn, ProfilerViewType viewType)
{
	m_ProfilerViewType = viewType;
	m_ProfilerSortColumn = sortColumn;

	// @TODO: Dont cleanup here, but call it when done with the data
	if(m_Root)
		CleanupProperty();
	Assert(m_Root == NULL);
	Assert(m_FrameData == NULL);

	ProfilerFrameData* frameData = ProfilerHistory::Get().GetFrameData(frame);
	if (frameData == NULL)
		return;
	
	m_FrameData = frameData;
	m_ThreadIdx = 0;

	InitializeAdditionalProfilerData(*m_FrameData, m_ThreadIdx, m_AdditionalData);
	FillAdditionalProfilerData(*m_FrameData, m_ThreadIdx, m_AdditionalData);

	m_Root = new ProfilerHierarchy();
	m_Root->oneSample = 0;
	m_Root->parent = NULL;
	
	BuildHierarchyLevel(*m_FrameData, m_ThreadIdx, m_Root, m_ProfilerSortColumn, m_ProfilerViewType, m_AdditionalData);
	ProcessProfilerSampleData(*m_FrameData, m_ThreadIdx, *m_Root, m_AdditionalData);
	
	m_ActiveHierarchy = m_Root;
}	

void ProfilerProperty::InitializeDetailProperty (const ProfilerProperty& sourceProperty)
{
	m_FrameData = sourceProperty.m_FrameData;
	m_ThreadIdx = sourceProperty.m_ThreadIdx;
	m_ProfilerViewType = kViewDetailFlat;
	m_ProfilerSortColumn = sourceProperty.m_ProfilerSortColumn;

	InitializeAdditionalProfilerData(*m_FrameData, m_ThreadIdx, m_AdditionalData);
	FillAdditionalProfilerData(*m_FrameData, m_ThreadIdx, m_AdditionalData);
	
	m_Root = new ProfilerHierarchy();
	m_Root->oneSample = sourceProperty.m_ActiveHierarchy->parent->oneSample;
	m_Root->sampleIdx = sourceProperty.m_ActiveHierarchy->parent->sampleIdx;
	m_Root->parent = NULL;

	dynamic_array<UInt32> allSamples = sourceProperty.m_ActiveHierarchy->sampleIdx;
	allSamples.push_back (sourceProperty.m_ActiveHierarchy->oneSample);

	BuildHierarchyLevel(*m_FrameData, m_ThreadIdx, allSamples, m_Root, m_ProfilerSortColumn, m_ProfilerViewType, m_AdditionalData);
	ProcessProfilerSampleData(*m_FrameData, m_ThreadIdx, *m_Root, m_AdditionalData);
	
	m_ActiveHierarchy = m_Root;
}

std::string ProfilerProperty::GetFrameTime() const
{
	if (!m_FrameData)
		return "--";

	return GetFormattedTime(m_FrameData->m_ThreadData[m_ThreadIdx].GetRoot()->timeUS*1000);
}

std::string ProfilerProperty::GetFrameGpuTime() const
{
	if (m_FrameData){
		return GetFormattedTime(m_FrameData->m_TotalGPUTimeInMicroSec*1000);
	}
	return "--";
}

std::string ProfilerProperty::GetFrameFPS() const
{
	if (m_FrameData)
	{
		double frame = 1000000.0 / (double)m_FrameData->m_ThreadData[m_ThreadIdx].GetRoot()->timeUS;
		return Format("%.1f", (float)frame);
	}
	else
		return "--";
}

bool ProfilerProperty::HasChildren() const
{
	// Detail view does not haven any children.
	if (m_ProfilerViewType == kViewDetailFlat && m_Root != m_ActiveHierarchy)
		return false;

	if (m_OnlyShowGPUSamples)
		return m_ActiveHierarchy->data.childrenGPUSamplesCount != 0;

	if (m_FrameData->m_ThreadData[m_ThreadIdx].GetSample(m_ActiveHierarchy->oneSample)->nbChildren != 0)
		return true;

	dynamic_array<UInt32>& samples = m_ActiveHierarchy->sampleIdx;
	for (int i=0;i<samples.size();i++)
	{
		if (m_FrameData->m_ThreadData[m_ThreadIdx].GetSample(samples[i])->nbChildren != 0)
			return true;
	}
	return false;
}

bool ProfilerProperty::GetNext(bool expanded)
{
	if (!GetNextInternal (expanded))
		return false;
	
	while (m_OnlyShowGPUSamples && m_ActiveHierarchy->data.gpuSamplesCount == 0)
	{
		if (!GetNextInternal (expanded))
			return false;
	}
	
	return true;
}

bool ProfilerProperty::GetNextInternal(bool expanded)
{
	if (m_ActiveHierarchy == NULL)
		return false;
	
	if (HasChildren() && expanded)
	{
		BuildHierarchyLevel(*m_FrameData, m_ThreadIdx, m_ActiveHierarchy, m_ProfilerSortColumn, m_ProfilerViewType, m_AdditionalData);

		// Step into children list
		m_ActiveHierarchy = &m_ActiveHierarchy->children[0];
			
		// Increase depth
		++m_Depth;
	}
	else
	{
		if (m_ActiveHierarchy->parent == 0)
			return false;

		int nextIndex = (m_ActiveHierarchy - &m_ActiveHierarchy->parent->children[0]) + 1;
		
		// Locating next profile sample object until one is found or end of samples is reached
		// step out of children list to its parent if end of list is reached
		while(nextIndex == m_ActiveHierarchy->parent->children.size())
		{
			// Go to parent
			m_ActiveHierarchy = m_ActiveHierarchy->parent;
			
			if (!m_ActiveHierarchy->parent)
			{
				// End of samples
				return false;
			}
			nextIndex = (m_ActiveHierarchy - &m_ActiveHierarchy->parent->children[0]) + 1;
			
			// Decrease depth
			--m_Depth;
		}
		
		m_ActiveHierarchy = &m_ActiveHierarchy->parent->children[nextIndex];
	}
	
	// Read profile sample name
	m_FunctionPath = m_FunctionName = m_ActiveHierarchy->data.information->name;
		
	// Assemble path name (used by profile property to handle fold in / fold out)
	ProfilerHierarchy* sample = m_ActiveHierarchy;
	sample = sample ? sample->parent : NULL;
	int depth = m_Depth - 1;
	
	// Construct path string of all parent names and name of this sample
	while (sample != NULL && sample->data.information)
	{
		// Using Format("%s/%s") takes 80ms to display profiler timeone on Windows, Core i7 2600K.
		// Replacing that with manual string operations gets time down to 20ms.

		//m_FunctionPath = Format("%s/%s", sample->data.information->name, m_FunctionPath.c_str());
		const size_t nameLen = strlen(sample->data.information->name);
		m_FunctionPath.reserve (m_FunctionPath.size() + nameLen + 1); // name length plus '/' char
		m_FunctionPath.insert (0, sample->data.information->name, nameLen+1); // insert whole name and trailing zero
		m_FunctionPath[nameLen] = '/'; // replace just inserted trailing zero with '/'

		sample = sample->parent;
		depth--;
	}
	
	// Found profile sample
	return true;
}

void ProfilerProperty::CleanupProperty()
{
	m_Depth = 0;
	delete m_Root; m_Root = NULL;
	m_FrameData = NULL;
	m_ActiveHierarchy = NULL;
	for(int i = 0; i < m_AdditionalData.size(); i++)
		ProfilerProperty::s_AdditionalDataPool.Deallocate(m_AdditionalData[i]);
	m_AdditionalData.clear();
}


#endif // #if ENABLE_PROFILER && UNITY_EDITOR
