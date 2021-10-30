#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_PROFILER

#include "Runtime/Utilities/LogAssert.h"
#include "TimeHelper.h"
#include "Runtime/Allocator/LinearAllocator.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/ThreadSpecificValue.h"
#include "Profiler.h"
#include "ProfilerStats.h"
#include "TimeHelper.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/LinkedList.h"
#include <map>
#include "Runtime/Scripting/ScriptingUtility.h"


class GfxTimerQuery;
class ProfilerFrameData;
class File;


// Additional data possibly "attached" to base ProfilerSamples.
// GPU times, object IDs etc. Separated out since only a few
// samples need it.
namespace ProfilerData
{
	// GPU time per draw call or other interesting GPU event.
	struct GPUTime
	{
		UInt32 relatedSampleIndex;
		GfxTimerQuery* timerQuery;
		int gpuTimeInMicroSec;
		GpuSection gpuSection;
	};

	// Instance ID of object that the source of profiled event.
	struct InstanceID 
	{
		UInt32 relatedSampleIndex;
		SInt32 instanceID;
	};

	// Managed GC memory allocated during the call.
	struct AllocatedGCMemory
	{
		UInt32 relatedSampleIndex;
		UInt32 allocatedGCMemory;
	};
};

// Base profiler sample with CPU times;
// minimal amount of actual per-sample data.
struct ProfilerSample
{
	ProfilerSample()
		: information(NULL)
		, startTimeUS(0)
		, timeUS(0)
		, nbChildren(0)
	{
	}
	
	UInt64 startTimeUS; // start time in microsecs
	UInt32 timeUS; // duration in microsecs
	
	ProfilerInformation* information; // actual information
	
	int nbChildren;
};

const ProfilerSample* SkipSampleRecurse (const ProfilerSample* sample);



class UnityProfilerPerThread
{
public:
	static UNITY_TLS_VALUE(UnityProfilerPerThread*) ms_InstanceTLS;
	bool m_ProfilerAllowSampling; // public for performance in free sampling functions

public:
	static void Initialize(const char* threadName, bool separateBeginEnd = false);
	static void Cleanup();
	
	~UnityProfilerPerThread();
	
	bool GetIsActive () const { return m_ProfilerAllowSampling; }
	void SetIsActive (bool enabled);
	
	void BeginFrame(ProfilerMode mode);
	bool EndFrame();
	void ClearFrame();
	void AddMiscSamplesAfterFrame(ProfileTimeFormat frameDuration, bool addOverhead);
	void SaveToFrameData (ProfilerFrameData& dst) const;
	
	ProfilerSample* BeginSample(ProfilerInformation* info, const Object* obj);
	void EndSample(ABSOLUTE_TIME time);
	
	void BeginSampleDynamic(const std::string& name, const Object* obj);
	void EndSampleDynamic() { if (m_ProfilerAllowSampling) EndSample(START_TIME); }
	
	void AddGPUSample (const ProfilerData::GPUTime& sample) { m_GPUTimeSamples.push_back(sample); }
	
	
	// Clear the mono method cache, but do not clear the memory used to store info/names.
	// We still need that memory to display profiler information.
	void CleanupMonoMethodCache() { m_ActiveMethodCache.clear(); }
	
	ProfilerSample* GetRoot () { return m_NextSampleIndex == 0 ? NULL : &m_ActiveSamples[0]; }
	UInt32 GetActiveSampleIndex () { return m_SampleStack.back(); }	
	ProfilerSample* GetActiveSample ()
	{
		// @TODO out of memory?
		return &m_ActiveSamples[ GetActiveSampleIndex () ];
	}
	const ProfilerSample* GetActiveSample(int parentLevel) const;
	
	ListNode<UnityProfilerPerThread>& GetProfilersListNode() { return m_ProfilersListNode; }
	
	ProfilerInformation* CreateProfilerInformationForMethod(ScriptingObjectPtr object, ScriptingMethodPtr method, const char* methodName, ScriptingTypePtr profileKlass, int flags);
	ProfilerInformation* GetProfilerInformation( const std::string& name, UInt16 group, UInt16 flags, bool isWarning);
	
	void EnterMonoMethod (MonoMethod *method);
	void LeaveMonoMethod (MonoMethod *method);
	void SampleGCAllocation (MonoObject *obj, MonoClass *klass);
	static void SampleGCMonoCallback (void *prof, int event, int generation);
	void ClearGCCollectTime() { m_GCCollectTime = 0; }
	
	size_t GetAllocatedBytes() const { return m_ActiveGlobalAllocator.GetAllocatedBytes(); }
	void SetThreadIndex(int idx) { m_ThreadIndex = idx; }
	bool IsSeparateBeginEnd() const { return m_SeparateBeginEnd; }
	
private:
	UnityProfilerPerThread(const char* threadName, bool separateBeginEnd); // prevent public creation
	
	void InjectGCCollectSample();	
	void CreateOverheadSample();
	
private:
	int                            m_NextSampleIndex;
	dynamic_array<ProfilerSample>  m_ActiveSamples;
	dynamic_array<UInt32>          m_SampleStack;
	dynamic_array<ABSOLUTE_TIME>   m_SampleTimeBeginStack;
	dynamic_array<ProfilerData::GPUTime> m_GPUTimeSamples;
	dynamic_array<ProfilerData::InstanceID> m_InstanceIDSamples;
	dynamic_array<ProfilerData::AllocatedGCMemory> m_AllocatedGCMemorySamples;
	dynamic_array<UInt32>          m_WarningSamples;
	bool                           m_OutOfSampleMemory;
	bool                           m_ErrorDurringFrame;
	
	ProfileTimeFormat m_GCCollectTime;
	
	typedef UNITY_MAP(kMemProfiler, ScriptingMethodPtr, ProfilerInformation*) MethodInfoCache;
	typedef UNITY_MAP(kMemProfiler, std::string, ProfilerInformation) DynamicMethodCache;
	
	ForwardLinearAllocator m_ActiveGlobalAllocator;
	MethodInfoCache m_ActiveMethodCache;
	DynamicMethodCache m_DynamicMethodCache;
	
	ListNode<UnityProfilerPerThread> m_ProfilersListNode;
	
	ABSOLUTE_TIME m_GCStartTime;
	
	const char* m_ThreadName;
	int m_ThreadIndex;
	bool m_SeparateBeginEnd;
	
	friend ProfilerSample* mono_profiler_begin(ScriptingMethodPtr method, ScriptingClassPtr profileKlass, ScriptingObjectPtr instance);
	friend void mono_profiler_end(ProfilerSample* beginsample);
};



class UnityProfiler
{
public:
	static UnityProfiler* ms_Instance; // singleton
	
public:
	// Destructor
	~UnityProfiler();
	
	static void Initialize ();
	static void CleanupGfx ();
	static void Cleanup ();
	
	// Singleton accessor for profiler
	static UnityProfiler& Get() { return *ms_Instance; }
	static UnityProfiler* GetPtr() { return ms_Instance; }

	void SetEnabled (bool val);
	void UpdateEnabled() { m_ProfilerMode = m_PendingProfilerMode; }
	bool GetEnabled () { return (m_ProfilerMode & kProfilerEnabled) != 0; }

	void SetProfileEditor (bool val) { val ? m_PendingProfilerMode |= kProfilerEditor : m_PendingProfilerMode &= ~kProfilerEditor ; }
	bool GetProfileEditor () { return (m_PendingProfilerMode & kProfilerEditor) != 0; }

	void SetDeepProfiling (bool val) { val ? m_PendingProfilerMode |= kProfilerDeepScripts : m_PendingProfilerMode &= ~kProfilerDeepScripts ; }
	bool GetDeepProfiling () { return (m_PendingProfilerMode & kProfilerDeepScripts) != 0; }

	void StartProfilingMode (ProfilerMode mode);
	void EndProfilingMode (ProfilerMode mode);
	
	void BeginFrame ();
	void EndFrame ();
	void BeginFrameSeparateThread(ProfilerMode mode);
	void EndFrameSeparateThread(unsigned frameIDAndValid);
	void DisableSamplingSeparateThread();
	void SetActiveSeparateThread(bool enabled);
	
	void ClearPendingFrames();
				
			
	void SetupProfilerEvents ();
	ProfilerSample* AllocateSample();
	
	void GetDebugStats (DebugStats& debugStats);
	
	void CleanupMonoMethodCaches();
	
	
	void SetLogPath (std::string logPath);
	std::string GetLogPath () { return m_LogFile; }

	void EnableBinaryLog(bool val) { m_BinaryLogEnabled = val; }
	bool BinaryLogEnabled() { return m_BinaryLogEnabled; }

	static void AddFramesFromFile(std::string path);

	static void SerializeFrameData(ProfilerFrameData& frame, dynamic_array<int>& buffer);
	static bool DeserializeFrameData(ProfilerFrameData* frame, const void* data, int size);
	
	void Serialize( dynamic_array<int>& bs );
	static ProfilerInformation* Deserialize( int** bs, bool swapdata );
	
	void AddPerThreadProfiler (UnityProfilerPerThread* prof);
	void RemovePerThreadProfiler (UnityProfilerPerThread* prof);
	
	static void RecordPreviousFrame(ProfilerMode mode);
	static bool StartNewFrame(ProfilerMode mode);

private:
	UnityProfiler(); // prevent accidental creation
	
	void SetIsActive (bool enabled);
	
	void CheckPro();

	void LogFrame (ProfilerFrameData* frame);

private:	
	bool m_ProfilerEnabledThisFrame;
	bool m_ProfilerEnabledLastFrame;
	bool m_ProfilerAllowSamplingGlobal;
	int  m_EnabledCount;	
	

	// hold framedata for sevral frames to wait for GPU samples
	enum  { kFrameCount = 2	};
	ProfilerFrameData* m_PreviousFrames[kFrameCount];

	// indicates Disabled or what elements we are profiling
	ProfilerMode m_ProfilerMode;
	ProfilerMode m_PendingProfilerMode;

	#if SUPPORT_THREADS
	Thread::ThreadID m_MainThreadID;
	#endif
		
	// accumulates time inside profiler start/stop (multiple start/stop pair are allowed during the frame)
	ABSOLUTE_TIME m_TotalProfilerFrameDuration;
	// accumulates all time from startFrame(N) to startFrame(N+1)
	ProfileTimeFormat m_ProfilerEnabledDuration;
	ABSOLUTE_TIME m_LastEnabledTime;
	
	typedef List< ListNode<UnityProfilerPerThread> > ProfilersList;
	ProfilersList m_Profilers;
	Mutex m_ProfilersMutex;
	int m_ProfilerCount;
	
	Mutex m_PrevFramesMutex;
	int m_FrameIDCounter;
	

	std::string m_LogFile;
	int m_FramesLogged;
	bool m_BinaryLogEnabled;
	File* m_TextFile;
	File* m_DataFile;

	
	friend class ProfilerHistory;
	friend ProfilerSample* mono_profiler_begin(ScriptingMethodPtr method, ScriptingClassPtr profileKlass, ScriptingObjectPtr instance);
	friend void mono_profiler_end(ProfilerSample* beginsample);
};



#endif // #if ENABLE_PROFILER
