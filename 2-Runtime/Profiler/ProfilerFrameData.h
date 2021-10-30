#ifndef _PROFILERFRAMEDATA_H_
#define _PROFILERFRAMEDATA_H_


#include "ProfilerImpl.h"

#if ENABLE_PROFILER


// -------------------------------------------------------------------


// Profiling data stored for one frame
class ProfilerFrameData
{
public:
	struct ThreadData
	{
		ThreadData()
		: m_AllSamples(kMemProfiler)
		, m_GPUTimeSamples(kMemProfiler)
		, m_InstanceIDSamples(kMemProfiler)
		, m_AllocatedGCMemorySamples(kMemProfiler)
		, m_WarningSamples(kMemProfiler)
		, m_ThreadName("<no data this frame>")
		{
		}
		
		const ProfilerSample* GetRoot () const { return &m_AllSamples[0]; }
		ProfilerSample* GetRoot () { return &m_AllSamples[0]; }
		const ProfilerSample* GetSample (int sample) const { return &m_AllSamples[sample]; }
		
		void ExtractAllChildSamples (UInt32 index, dynamic_array<UInt32>& allChildren) const;
		
		dynamic_array<ProfilerSample>                  m_AllSamples;
		dynamic_array<ProfilerData::GPUTime>           m_GPUTimeSamples;
		dynamic_array<ProfilerData::InstanceID>        m_InstanceIDSamples;
		dynamic_array<ProfilerData::AllocatedGCMemory> m_AllocatedGCMemorySamples;
		dynamic_array<UInt32>                          m_WarningSamples;

		std::string m_ThreadName;
	};
	
	ProfilerFrameData(int threadCount, int frameID);
	~ProfilerFrameData();

	int m_FrameID;
	int frameIndex;
	int realFrame;
	
	// Automatic statistic value extraction can extract any int value from here
	AllProfilerStats allStats;
	int selectedTime;
	// Until here
	
	ThreadData* m_ThreadData;
	int m_ThreadCount;
	
	ProfileTimeFormat m_StartTimeUS;
	int m_TotalCPUTimeInMicroSec;
	int m_TotalGPUTimeInMicroSec;
	
#if ENABLE_PLAYERCONNECTION 
	void Serialize(dynamic_array<int>& bs);
	void Deserialize(int** bs, bool swapdata);
	
	static ProfilerInformation* DeserializeProfilerInformation( int** bitstream, bool swapdata );
	static void SerializeProfilerInformation( const ProfilerInformation& info, dynamic_array<int>& bitstream );

#endif

	static GfxTimerQuery* AllocTimerQuery();
	static void ReleaseTimerQuery(GfxTimerQuery* query);
	static void FreeAllTimerQueries();

private:
	static dynamic_array<GfxTimerQuery*> m_UnusedQueries;
};


// -------------------------------------------------------------------


#if UNITY_EDITOR

// Hierarchical iterator over raw samples in one profiler frame
class ProfilerFrameDataIterator
{
public:
	ProfilerFrameDataIterator();

	int GetThreadCount(int frame) const;
	double GetFrameStartS(int frame) const;
	const std::string* GetThreadName () const;
	float GetFrameTimeMS() const;
	float GetStartTimeMS() const;

	float GetDurationMS() const;
	int GetGroup() const;
	int GetID() const { return m_CurrIndex; }
	int GetDepth() const { return m_Stack.size(); }
	const std::string& GetFunctionName() const { return m_FunctionName; }
	const std::string& GetFunctionPath() const { return m_FunctionPath; }

	void SetRoot(int frame, int threadIdx);
	bool GetNext(bool expanded);
	bool IsFrameValid() const { return m_FrameData != NULL; }

private:	
	const ProfilerSample& GetSample(UInt32 index) const;

private:
	struct StackInfo
	{
		std::string path;
		UInt32 sampleBegin;
		UInt32 sampleEnd;
	};

	ProfilerFrameData* m_FrameData;
	int m_ThreadIdx;

	UNITY_VECTOR(kMemProfiler,StackInfo) m_Stack;
	UInt32	m_CurrIndex;

	std::string m_FunctionName; // name (e.g. Camera.Render)
	std::string m_FunctionPath; // hierarchical name, e.g. PlayerLoop/RenderCameras/Camera.Render
};

#endif // #if UNITY_EDITOR

#endif // #if ENABLE_PROFILER

#endif
