#ifndef _PROFILERPROPERTY_H_
#define _PROFILERPROPERTY_H_

#include "Configuration/UnityConfigure.h"

enum ProfilerViewType
{
	kViewHierarchy = 0,	// Functions merged and sorted
	kViewTimeline,		// same as kViewRawHierarchy, only different in the UI
	kViewRawHierarchy,	// Unmerged and unsorted
	kViewDetailFlat,	// Unmerged and unsorted and no children
};

enum ProfilerColumn
{
	kDontSortProfilerColumn = -1,
	kFunctionNameColumn = 0,

	kTotalPercentColumn,
	kSelfPercentColumn,
	kCallsColumn,
	kGCMemory,
	kTotalTimeColumn,
	kSelfTimeColumn,

	kDrawCallsColumn,
	kTotalGPUTimeColumn,
	kSelfGPUTimeColumn,
	kTotalGPUPercentColumn,
	kSelfGPUPercentColumn,
	
	kWarningColumn,

	kObjectNameColumn,
	
	kProfilerColumnCount
};

#if ENABLE_PROFILER && UNITY_EDITOR

#include "Profiler.h"
#include "ProfilerHistory.h"
#include "Runtime/Utilities/MemoryPool.h"
#include <iosfwd>

class Object;
struct ProfilerHierarchy;

struct AdditionalProfilerSampleData
{
	ProfileTimeFormat gpuTime;
	UInt32            drawCalls;
	GpuSection        gpuSection;
	SInt32            instanceID;
	UInt32            allocatedGCMemory;
	UInt32            warningCount;
};

// ProfilerProperty class for navigation through profiling information map in Profiler
class ProfilerProperty
{
public:
	ProfilerProperty();
	~ProfilerProperty();

	std::string GetProfilerColumn (ProfilerColumn column) const;
	
	std::string GetFrameTime() const;
	std::string GetFrameGpuTime() const;
	std::string GetFrameFPS() const;
	
	void InitializeDetailProperty (const ProfilerProperty& sourceProperty);
	
	const std::string& GetFunctionName() const { return m_FunctionName; }
	const std::string& GetFunctionPath() const { return m_FunctionPath; }
	
	// Returns call stack depth (profiled function nested level)
	int GetDepth() const { return m_Depth; }

	// Checks if the property has children (other profiled functions called within)
	bool HasChildren() const;
	
	// Sets profile sample list root
	void SetRoot(int frame, ProfilerColumn sortColumn, ProfilerViewType viewType);
	
	// Retrieves next profile sample
	bool GetNext(bool expanded);

	void GetInstanceIDs(dynamic_array<SInt32>& instanceIDs);
	
	// Returns current frame data
	ProfilerFrameData* GetFrameData() { return m_FrameData; }
	
	// Clean up property parameters (such as depth, ect)
	void CleanupProperty();

	std::string GetTooltip (ProfilerColumn column);

	bool GetOnlyShowGPUSamples () { return m_OnlyShowGPUSamples; }
	void SetOnlyShowGPUSamples (bool value) { m_OnlyShowGPUSamples = value; }

	static MemoryPool                           s_AdditionalDataPool;

private:
	
	bool SupportsGPUProfiler () const 	{ return !m_FrameData->m_ThreadData[m_ThreadIdx].m_GPUTimeSamples.empty(); }
	bool GetNextInternal(bool expanded);

	std::string m_FunctionName; // name (e.g. Camera.Render)
	std::string m_FunctionPath; // hierarchical name, e.g. PlayerLoop/RenderCameras/Camera.Render

	// Call stack depth (profiled function nested level)
	int m_Depth;

	bool m_OnlyShowGPUSamples;
	
	// Requested profile data view type
	ProfilerViewType                            m_ProfilerViewType;
	ProfilerColumn                              m_ProfilerSortColumn;

	ProfilerHierarchy*                          m_Root;
	ProfilerFrameData*                          m_FrameData;
	ProfilerHierarchy*                          m_ActiveHierarchy;
	dynamic_array<AdditionalProfilerSampleData*>m_AdditionalData;
	int m_ThreadIdx;
};

struct ProfilerSampleData
{
	// constant across all samples
	ProfileTimeFormat rootTime;
	ProfileTimeFormat rootGPUTime;

	ProfileTimeFormat time;
	
	ProfileTimeFormat gpuTime;
	
	ProfileTimeFormat startTime;
	
	UInt32            gpuSamplesCount;
	// Stores execution time per frame of all function called within this one
	ProfileTimeFormat childrenTime;
	ProfileTimeFormat childrenGPUTime;

	UInt32			  childrenGPUSamplesCount;

	// Stores amount of allocated GC memory
	UInt32            allocatedGCMemory;
	
	// Stores number of calls made to the function in one frame
	UInt32            numberOfCalls;
	
	UInt32            warningCount;

	// Stores profile information
	ProfilerInformation* information;
};


#endif // #if ENABLE_PROFILER && UNITY_EDITOR

#endif /*_PROFILERPROPERTY_H_*/
