#include "UnityPrefix.h"
#include "Profiler.h"
#include "Runtime/BaseClasses/BaseObject.h"

#include <string.h>
#if ENABLE_PROFILER

#include "ProfilerImpl.h"
#include "GPUProfiler.h"
#include "ProfilerHistory.h"
#include "ProfilerFrameData.h"
#include "CollectProfilerStats.h"
#include "ProfilerConnection.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Serialize/SwapEndianBytes.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Threads/JobScheduler.h"
#include "IntelGPAProfiler.h"
#include "Runtime/Scripting/ScriptingManager.h"

#if UNITY_PS3
#include "External/libsntuner.h"
#define PS3_TUNER_SAMPLE_BEGIN(s) snPushMarker((s)->name)
#define PS3_TUNER_SAMPLE_END() snPopMarker()
#else
#define PS3_TUNER_SAMPLE_BEGIN(s)
#define PS3_TUNER_SAMPLE_END()
#endif


#define DEBUG_AUTO_PROFILER_LOG 0
#if DEBUG_AUTO_PROFILER_LOG && !UNITY_BUILD_COPY_PROTECTED
#error "Must disable DEBUG_AUTO_PROFILER_LOG in deployed build"
#endif

#if UNITY_EDITOR
#include "Editor/Src/EditorHelper.h"
#endif


#if ENABLE_MONO
#include "Runtime/Scripting/CommonScriptingClasses.h"
const int kMonoProfilerDefaultFlags = MONO_PROFILE_GC | MONO_PROFILE_ALLOCATIONS | MONO_PROFILE_EXCEPTIONS;
#endif // #if ENABLE_MONO

const int kProfilerDataStreamVersion = 0x20122123;

void profiler_begin_frame()
{
	if (UnityProfiler::GetPtr())
		UnityProfiler::Get().BeginFrame();
}

void profiler_end_frame()
{
	if (UnityProfiler::GetPtr())
		UnityProfiler::Get().EndFrame();
}

void profiler_start_mode(ProfilerMode flags)
{
	if (UnityProfiler::GetPtr())
		UnityProfiler::Get().StartProfilingMode(flags);
}

void profiler_end_mode(ProfilerMode flags)
{
	if (UnityProfiler::GetPtr())
		UnityProfiler::Get().EndProfilingMode(flags);
}

void profiler_begin_thread_safe(ProfilerInformation* info, const Object* obj)
{
	PS3_TUNER_SAMPLE_BEGIN(info);
	
	UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
	if (prof && prof->m_ProfilerAllowSampling)
	{
		SET_ALLOC_OWNER(NULL);
		prof->BeginSample(info, obj);
	}
}

void profiler_end_thread_safe()
{
	PS3_TUNER_SAMPLE_END();
	
	UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
	if (prof && prof->m_ProfilerAllowSampling)
		prof->EndSample(START_TIME);
}


void profiler_begin(ProfilerInformation* info, const Object* obj)
{
	INTEL_GPA_SAMPLE_BEGIN(info);
	PS3_TUNER_SAMPLE_BEGIN(info);

	UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
	if (prof && prof->m_ProfilerAllowSampling)
	{
		SET_ALLOC_OWNER(NULL);
		prof->BeginSample(info, obj);
	}
}

void profiler_end()
{
	INTEL_GPA_SAMPLE_END();
	PS3_TUNER_SAMPLE_END();

	UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
	if (prof && prof->m_ProfilerAllowSampling)
		prof->EndSample(START_TIME);
}

GpuSection g_CurrentGPUSection = kGPUSectionOther;

void gpu_time_sample()
{
	GPUProfiler::GPUTimeSample();
}

void profiler_initialize_thread (const char* name, bool separateBeginEnd)
{
#if UNITY_EDITOR
	if(IsDeveloperBuild())
		UnityProfilerPerThread::Initialize(name, true);
#endif
}

void profiler_cleanup_thread ()
{
	UnityProfilerPerThread::Cleanup();
}

void profiler_set_active_seperate_thread (bool enabled)
{
	UnityProfiler* prof = UnityProfiler::GetPtr();
	if (prof)
		prof->SetActiveSeparateThread(enabled);
}

void profiler_begin_frame_seperate_thread (ProfilerMode mode)
{
	UnityProfiler* prof = UnityProfiler::GetPtr();
	if (prof)
		prof->BeginFrameSeparateThread(mode);
	
}

void profiler_disable_sampling_seperate_thread ()
{
	UnityProfiler* prof = UnityProfiler::GetPtr();
	if (prof)
		prof->DisableSamplingSeparateThread();
	
}

void profiler_end_frame_seperate_thread (int frameIDAndValid)
{
	UnityProfiler* prof = UnityProfiler::GetPtr();
	if (prof)
		prof->EndFrameSeparateThread(frameIDAndValid);
}
 

// -------------------------------------------------------------------------



ProfilerInformation::ProfilerInformation (const char* const functionName, ProfilerGroup grp, bool warn)
: name(functionName)
, group(grp)
, flags(kDefault)
, isWarning(warn)
{
	INTEL_GPA_INFORMATION_INITIALIZE();
}



// -------------------------------------------------------------------------
// Per-thread profiler class

const int kDeepProfilingMaxSamples = 1024 * 1024 * 4; // 80 MB on 32bit
const int kNormalProfilingMaxSamples = 1024 * 512;    // 10 MB on 32bit


UNITY_TLS_VALUE(UnityProfilerPerThread*) UnityProfilerPerThread::ms_InstanceTLS;

void UnityProfilerPerThread::Initialize(const char* threadName, bool separateBeginEnd)
{
	SET_ALLOC_OWNER(UnityProfiler::GetPtr());
	Assert(ms_InstanceTLS == NULL);
	ms_InstanceTLS = UNITY_NEW(UnityProfilerPerThread, kMemProfiler)(threadName, separateBeginEnd);
	
	UnityProfiler::ms_Instance->AddPerThreadProfiler(ms_InstanceTLS);
}

void UnityProfilerPerThread::Cleanup()
{
	if(ms_InstanceTLS == NULL)
		return;
	
	UnityProfiler::ms_Instance->RemovePerThreadProfiler(ms_InstanceTLS);
	
	UnityProfilerPerThread* instance = ms_InstanceTLS;
	UNITY_DELETE(instance, kMemProfiler);
	ms_InstanceTLS = NULL;
}

UnityProfilerPerThread::UnityProfilerPerThread(const char* threadName, bool separateBeginEnd)
: m_ProfilerAllowSampling(false)
, m_ActiveGlobalAllocator(1024 * 128, kMemProfiler)
, m_OutOfSampleMemory(false)
, m_ErrorDurringFrame(false)
, m_ActiveSamples(kMemProfiler)
, m_SampleStack(kMemProfiler)
, m_SampleTimeBeginStack(kMemProfiler)
, m_GPUTimeSamples(kMemProfiler)
, m_InstanceIDSamples(kMemProfiler)
, m_AllocatedGCMemorySamples(kMemProfiler)
, m_WarningSamples(kMemProfiler)
, m_ProfilersListNode(this)
, m_GCCollectTime(0)
, m_ThreadName(threadName)
, m_ThreadIndex(0)
, m_SeparateBeginEnd(separateBeginEnd)
{
	#if ENABLE_THREAD_CHECK_IN_ALLOCS
	Thread::ThreadID tid = Thread::GetCurrentThreadID();
	m_ActiveGlobalAllocator.SetThreadIDs(tid, tid);
	#endif
}

UnityProfilerPerThread::~UnityProfilerPerThread()
{
	m_ActiveGlobalAllocator.purge (true);
	m_ActiveMethodCache.clear();
	m_DynamicMethodCache.clear();
}

void UnityProfilerPerThread::BeginFrame(ProfilerMode mode)
{
	if (mode & kProfilerDeepScripts)
		m_ActiveSamples.resize_uninitialized(kDeepProfilingMaxSamples);
	else
		m_ActiveSamples.resize_uninitialized(kNormalProfilingMaxSamples);
	
	m_ActiveSamples[0] = ProfilerSample();
	m_ActiveSamples[0].startTimeUS = GetProfileTime(START_TIME)/1000;
	m_NextSampleIndex = 1;
	
	m_AllocatedGCMemorySamples.resize_uninitialized(0);
	m_GPUTimeSamples.resize_uninitialized(0);
	m_InstanceIDSamples.resize_uninitialized(0);
	m_WarningSamples.resize_uninitialized(0);

	m_SampleStack.resize_uninitialized(0);
	m_SampleStack.push_back(0);
	
	m_SampleTimeBeginStack.push_back(START_TIME);
	
	m_GCCollectTime = 0;
}


bool UnityProfilerPerThread::EndFrame()
{	
	Assert (!GetIsActive());
	
	ProfilerSample* rootSample = GetRoot();
	if (m_SampleStack.size()>1)
	{
		if (!m_ErrorDurringFrame)
			ErrorString("Too many Profiler.BeginSample (BeginSample and EndSample count must match)");
		m_ErrorDurringFrame = true;
	}
	
	bool ok = (rootSample->nbChildren != 0 && !m_OutOfSampleMemory && !m_ErrorDurringFrame);
	return ok;
}


void UnityProfilerPerThread::ClearFrame () 
{ 
	m_NextSampleIndex = 0;
	
	m_SampleStack.resize_uninitialized(0);
	m_SampleTimeBeginStack.resize_uninitialized(0);	
	m_GPUTimeSamples.resize_uninitialized(0);
	m_InstanceIDSamples.resize_uninitialized(0);
	m_AllocatedGCMemorySamples.resize_uninitialized(0);
	m_WarningSamples.resize_uninitialized(0);

	m_ProfilerAllowSampling = false;
	m_OutOfSampleMemory = false;
	m_ErrorDurringFrame = false;
}


void UnityProfilerPerThread::SetIsActive (bool enabled)
{
	if (!enabled && m_ProfilerAllowSampling)
	{
		if (m_GCCollectTime != 0)
			InjectGCCollectSample();
	}
	m_ProfilerAllowSampling = enabled && !m_SampleStack.empty();
}

void UnityProfilerPerThread::AddMiscSamplesAfterFrame(ProfileTimeFormat frameDuration, bool addOverhead)
{
	// Insert GC.Collect sample if we had one
	if (m_GCCollectTime != 0)
		InjectGCCollectSample();

	ProfilerSample* rootSample = GetRoot();
	if (rootSample)
		rootSample->timeUS = frameDuration / 1000;

	if (addOverhead)
		CreateOverheadSample();
}


ProfilerSample* UnityProfilerPerThread::BeginSample(ProfilerInformation* info, const Object* obj)
{
	DebugAssert(this);
	
	DebugAssert(m_ProfilerAllowSampling);
	DebugAssert(!m_SampleStack.empty() == m_ProfilerAllowSampling);
	
	// Insert GC.Collect sample if we had one
	if (m_GCCollectTime != 0)
		InjectGCCollectSample();
	
	// Add child to children
	m_ActiveSamples[m_SampleStack.back()].nbChildren++;
	
	// Add new sample and insert into stack
	m_SampleStack.push_back(m_NextSampleIndex);
	ProfilerSample* sample = &m_ActiveSamples[m_NextSampleIndex];
	m_NextSampleIndex++;
	if(m_NextSampleIndex > ((int)m_ActiveSamples.size()-1))
	{
		m_NextSampleIndex = m_ActiveSamples.size()-1;
		if(!m_OutOfSampleMemory)
		{
			m_OutOfSampleMemory = true;
			ErrorString("The profiler has run out of samples for this frame. This frame will be skipped.");	
		}		
	}
	
	// Initialize sample object 
	sample->information = info;
	
	if(info->isWarning)
		m_WarningSamples.push_back(GetActiveSampleIndex ());

	sample->nbChildren = 0;
	sample->startTimeUS = 0;
	
	if(obj != NULL)
	{
		ProfilerData::InstanceID instanceSample = {GetActiveSampleIndex(), obj->GetInstanceID()};
		m_InstanceIDSamples.push_back(instanceSample);
	}
	
	// Get current processor time and store it in the sample
	m_SampleTimeBeginStack.push_back(START_TIME);
	return sample;
}


void UnityProfilerPerThread::EndSample(ABSOLUTE_TIME time)
{
	DebugAssert(this);
	
	DebugAssert(m_ProfilerAllowSampling);	
	if(m_SampleStack.size() <= 1)
	{
		if(!m_ErrorDurringFrame)
			ErrorString("Non matching Profiler.EndSample (BeginSample and EndSample count must match)");
		m_ErrorDurringFrame = true;
		return;
	}
	DebugAssert(m_SampleStack.empty() != m_ProfilerAllowSampling);
	
	ProfilerSample* sample = &m_ActiveSamples[m_SampleStack.back()];
	// Set duration and start time
	sample->startTimeUS = GetProfileTime(m_SampleTimeBeginStack.back())/1000;	
	sample->timeUS = GetProfileTime(SUBTRACTED_TIME(time, m_SampleTimeBeginStack.back()))/1000;
	
	// Insert GC.Collect sample if we had one
	if (m_GCCollectTime != 0)
		InjectGCCollectSample();
	
	m_SampleStack.pop_back();
	m_SampleTimeBeginStack.pop_back();	
}


void UnityProfilerPerThread::BeginSampleDynamic(const std::string& name, const Object* obj)
{
	DebugAssert(this);
	if (!m_ProfilerAllowSampling)
		return;
	
	DynamicMethodCache::iterator found = m_DynamicMethodCache.find(name);
	if (found != m_DynamicMethodCache.end())
	{
		BeginSample(&found->second, obj);
	}
	else
	{
		found = m_DynamicMethodCache.insert(make_pair(name, ProfilerInformation(NULL, kProfilerScripts))).first;
		found->second.name = found->first.c_str();
		found->second.group = kProfilerScripts;
		BeginSample(&found->second, obj);
	}
}


void UnityProfilerPerThread::SaveToFrameData (ProfilerFrameData& dst) const
{
	ProfilerFrameData::ThreadData& tdata = dst.m_ThreadData[m_ThreadIndex];
	tdata.m_ThreadName = m_ThreadName;
	tdata.m_AllSamples.assign(m_ActiveSamples.begin(), &m_ActiveSamples[m_NextSampleIndex]);
	tdata.m_GPUTimeSamples.assign(m_GPUTimeSamples.begin(), m_GPUTimeSamples.end());
	tdata.m_InstanceIDSamples.assign(m_InstanceIDSamples.begin(), m_InstanceIDSamples.end());
	tdata.m_AllocatedGCMemorySamples.assign(m_AllocatedGCMemorySamples.begin(), m_AllocatedGCMemorySamples.end());
	tdata.m_WarningSamples.assign(m_WarningSamples.begin(), m_WarningSamples.end());
}



// -------------------------------------------------------------------------
// Global Profiler class



UnityProfiler* UnityProfiler::ms_Instance = NULL;

PROFILER_INFORMATION(gGCCollect, "GC.Collect", kProfilerGC)
PROFILER_INFORMATION(gOverheadProfile, "Overhead", kProfilerOverhead)


#if SUPPORT_THREADS
#define IS_MAIN_THREAD(p) Thread::EqualsCurrentThreadID(p->m_MainThreadID)
#else
#define IS_MAIN_THREAD(p) (true)
#endif


void UnityProfiler::Initialize ()
{
	Assert(ms_Instance == NULL);
	ms_Instance = UNITY_NEW_AS_ROOT(UnityProfiler,kMemProfiler, "Profiler", "");
	UnityProfilerPerThread::Initialize("Main Thread");
}

void UnityProfiler::CleanupGfx ()
{
	Assert(ms_Instance != NULL);
	for (int i = 0; i < kFrameCount; i++)
	{
		if (ms_Instance->m_PreviousFrames[i] != NULL)
			GPUProfiler::ClearTimerQueries(ms_Instance->m_PreviousFrames[i]->m_ThreadData[0].m_GPUTimeSamples);
	}
	ProfilerFrameData::FreeAllTimerQueries();
}

void UnityProfiler::Cleanup ()
{
	UnityProfilerPerThread::Cleanup();
	
	Assert(ms_Instance != NULL);
	UNITY_DELETE(ms_Instance, kMemProfiler);
	ms_Instance = NULL;
}



UnityProfiler::UnityProfiler() 
: m_ProfilerEnabledThisFrame(false)
, m_ProfilerEnabledLastFrame(false)
, m_ProfilerAllowSamplingGlobal(false)
, m_EnabledCount(0)
, m_FramesLogged(0)
, m_TextFile(NULL)
, m_DataFile(NULL)
, m_BinaryLogEnabled(false)
, m_ProfilerCount(0)
, m_FrameIDCounter(1)
{
#if SUPPORT_THREADS
	m_MainThreadID = Thread::GetCurrentThreadID();
#endif

	m_PendingProfilerMode = kProfilerGame;
	m_ProfilerMode = m_PendingProfilerMode;
	ABSOLUTE_TIME_INIT(m_LastEnabledTime);

	memset(m_PreviousFrames, 0, sizeof(m_PreviousFrames));

	#if DEBUG_AUTO_PROFILER_LOG
#if UNITY_OSX
	SetEnabled(true);
	string result = getenv ("HOME");
	SetLogPath(AppendPathName( result, "Library/Logs/Unity/Profiler.log"));
#elif UNITY_WIN
	SetEnabled(true);
	const char* tempPath = ::getenv("TEMP");
	if (!tempPath)
		tempPath = "C:";
	SetLogPath(AppendPathName (tempPath, "UnityProfiler.log"));
#endif
	#endif // #if DEBUG_AUTO_PROFILER_LOG
}


UnityProfiler::~UnityProfiler()
{
	SetLogPath("");
	UNITY_DELETE(m_TextFile, kMemProfiler);
	UNITY_DELETE(m_DataFile, kMemProfiler);
}

void UnityProfiler::AddPerThreadProfiler (UnityProfilerPerThread* prof)
{
	Mutex::AutoLock lock(m_ProfilersMutex);
	m_Profilers.push_back(prof->GetProfilersListNode());
	++m_ProfilerCount;
}

void UnityProfiler::RemovePerThreadProfiler (UnityProfilerPerThread* prof)
{
	if (!this)
		return;
	Mutex::AutoLock lock(m_ProfilersMutex);
	prof->GetProfilersListNode().RemoveFromList();
	--m_ProfilerCount;
}


void UnityProfiler::CheckPro()
{
	if (GetEnabled())
	{
		BuildSettings* buildSettings = GetBuildSettingsPtr();
#if UNITY_EDITOR
		if (buildSettings && !buildSettings->hasPROVersion)
#else
		if (buildSettings && !buildSettings->hasAdvancedVersion)
#endif
		{
			ErrorString("Profiler is only supported in Unity Pro.");
			SetEnabled(false);
		}
	}
}

void UnityProfiler::SetEnabled (bool val) 
{
	BuildSettings* buildSettings = GetBuildSettingsPtr();
#if UNITY_EDITOR
	if (buildSettings && !buildSettings->hasPROVersion)
		return;
#else
	if (buildSettings && !buildSettings->hasAdvancedVersion)
		return;
#endif
	
	if(val)
		m_PendingProfilerMode |= kProfilerEnabled;
	else
		m_PendingProfilerMode &= ~kProfilerEnabled;
}

void UnityProfiler::RecordPreviousFrame(ProfilerMode mode)
{
	UnityProfiler* profiler = UnityProfiler::GetPtr();
	if(!profiler)
		return;

	if(profiler->m_ProfilerEnabledLastFrame)
	{
		GPUProfiler::EndFrame();
		profiler->EndProfilingMode(mode);
		profiler->EndFrame();
		profiler->m_ProfilerEnabledLastFrame = false;
	}
}

bool UnityProfiler::StartNewFrame(ProfilerMode mode)
{
	UnityProfiler* profiler = UnityProfiler::GetPtr();
	if(!profiler)
		return false;

	UnityProfiler::Get().UpdateEnabled();

	if (UnityProfiler::Get().GetEnabled())
	{
		profiler->BeginFrame();
		profiler->StartProfilingMode(mode);
		GPUProfiler::BeginFrame();
		profiler->m_ProfilerEnabledLastFrame = true;
	}

	return profiler->m_ProfilerEnabledLastFrame;
}

void UnityProfiler::BeginFrame () 
{ 
	DebugAssert(IS_MAIN_THREAD(this));
	
	CheckPro();
	
	GfxDevice& device = GetGfxDevice();
	device.ProfileControl(GfxDevice::kGfxProfDisableSampling, 0);
	
	m_ProfilerMode = m_PendingProfilerMode;
	m_ProfilerEnabledThisFrame = m_ProfilerMode & kProfilerEnabled;
	
	{
		Mutex::AutoLock lock(m_ProfilersMutex);
		for(ProfilersList::iterator it = m_Profilers.begin(); it != m_Profilers.end(); ++it)
		{
			UnityProfilerPerThread& prof = **it;
			if (!prof.IsSeparateBeginEnd())
				prof.m_ProfilerAllowSampling = false;
		}
	}
	m_ProfilerAllowSamplingGlobal = false;
	
	if(!m_ProfilerEnabledThisFrame)
		return;
	
	device.ProfileControl(GfxDevice::kGfxProfBeginFrame, m_ProfilerMode);
	{
		Mutex::AutoLock lock(m_ProfilersMutex);
		for(ProfilersList::iterator it = m_Profilers.begin(); it != m_Profilers.end(); ++it)
		{
			UnityProfilerPerThread& prof = **it;
			if (!prof.IsSeparateBeginEnd())
				prof.BeginFrame(m_ProfilerMode);
		}
	}

	
	m_TotalProfilerFrameDuration = START_TIME;
	// accumulates all time from startFrame(N) to startFrame(N+1)
	m_ProfilerEnabledDuration = 0;
}

	
void UnityProfiler::BeginFrameSeparateThread(ProfilerMode mode)
{
	Mutex::AutoLock lock(m_ProfilersMutex);
	UnityProfilerPerThread* profTLS = UnityProfilerPerThread::ms_InstanceTLS;
	if (!profTLS)
		return;
	Assert(profTLS->IsSeparateBeginEnd());

	profTLS->BeginFrame(mode);
}

void UnityProfiler::DisableSamplingSeparateThread()
{
	Mutex::AutoLock lock(m_ProfilersMutex);	
	UnityProfilerPerThread* profTLS = UnityProfilerPerThread::ms_InstanceTLS;
	if (!profTLS)
		return;
	Assert(profTLS->IsSeparateBeginEnd());
	
	profTLS->m_ProfilerAllowSampling = false;
}

void UnityProfiler::SetActiveSeparateThread(bool enabled)
{
	Mutex::AutoLock lock(m_ProfilersMutex);	
	UnityProfilerPerThread* profTLS = UnityProfilerPerThread::ms_InstanceTLS;
	if (!profTLS)
		return;
	Assert(profTLS->IsSeparateBeginEnd());
	
	profTLS->SetIsActive(enabled);
}


void UnityProfiler::EndFrame () 
{
	Assert (m_EnabledCount == 0);

	if (!m_ProfilerEnabledThisFrame)
		return;

	bool profileFrameValid = true;
	int threadIdx = 0;
	{
		Mutex::AutoLock lock(m_ProfilersMutex);
		for(ProfilersList::iterator it = m_Profilers.begin(); it != m_Profilers.end(); ++it)
		{
			UnityProfilerPerThread& prof = **it;
			prof.SetThreadIndex(threadIdx);
			if (!prof.IsSeparateBeginEnd())
			{
				bool threadValid = prof.EndFrame();
				if (threadIdx == 0 && !threadValid)
					profileFrameValid = false;
			}
			++threadIdx;
		}
	}

	int curFrameID = 0;
	if (profileFrameValid)
	{
		StartProfilingMode(kProfilerGame);

		threadIdx = 0;
		{
			Mutex::AutoLock lock(m_ProfilersMutex);
			for(ProfilersList::iterator it = m_Profilers.begin(); it != m_Profilers.end(); ++it)
			{
				UnityProfilerPerThread& prof = **it;
				if (!prof.IsSeparateBeginEnd())
					prof.AddMiscSamplesAfterFrame(m_ProfilerEnabledDuration, threadIdx==0);
				++threadIdx;
			}
		}

		EndProfilingMode(kProfilerGame);
		

		// Collect GPU timing for recent frames (non blocking)
		for (int i = kFrameCount - 2; i >= 0; i--)
		{
			if (m_PreviousFrames[i] != NULL)
				GPUProfiler::CollectGPUTime(m_PreviousFrames[i]->m_ThreadData[0].m_GPUTimeSamples, false);
		}

		// Compute GPU timing for oldest frames (blocking)
		ProfilerFrameData* oldestFrame = m_PreviousFrames[kFrameCount-1];
		if(oldestFrame)
		{
			oldestFrame->m_TotalGPUTimeInMicroSec = GPUProfiler::ComputeGPUTime(oldestFrame->m_ThreadData[0].m_GPUTimeSamples);

			Mutex::AutoLock prevFramesLock(m_PrevFramesMutex);

			LogFrame(oldestFrame);
#if UNITY_EDITOR
			// profilerhistory takes ownership and pushes pointer on a vector;
			ProfilerHistory::Get().AddFrameDataAndTransferOwnership (oldestFrame, ProfilerConnection::GetEditorGuid());
			// allocate new frame
			oldestFrame = UNITY_NEW(ProfilerFrameData, kMemProfiler) (m_ProfilerCount, ++m_FrameIDCounter);
#elif ENABLE_PLAYERCONNECTION
			ProfilerConnection::Get().SendFrameDataToEditor (*oldestFrame); // reuse allocated memory
			// GPUProfiler::ExtractGPUTime(m_PreviousGPUTimeSamples); // TODO use this sceme instead to reduce memory
			// ProfilerConnection::Get().TransferPartialData(...)
			// m_PreviousGPUTimeSamples.swap(m_GPUTimeSamples);
#endif
		}

		ProfilerFrameData* curFrame = NULL;
		{
			Mutex::AutoLock prevFramesLock(m_PrevFramesMutex);
			for (int i = 1; i < kFrameCount; i++)
				m_PreviousFrames[i] = m_PreviousFrames[i - 1];

			m_PreviousFrames[0] = oldestFrame;

			if (m_PreviousFrames[0] == NULL)
			{
				m_PreviousFrames[0] = UNITY_NEW(ProfilerFrameData,kMemProfiler) (m_ProfilerCount, ++m_FrameIDCounter);
			}
			
			curFrame = m_PreviousFrames[0];
		}

		curFrameID = curFrame->m_FrameID;
		{
			Mutex::AutoLock lock(m_ProfilersMutex);
			for(ProfilersList::iterator it = m_Profilers.begin(); it != m_Profilers.end(); ++it)
			{
				UnityProfilerPerThread& prof = **it;
				if (!prof.IsSeparateBeginEnd())
					prof.SaveToFrameData(*curFrame);
			}
		}

		CollectProfilerStats(curFrame->allStats);
	}

	const unsigned frameIDAndValidFlag = curFrameID | (profileFrameValid ? 0x80000000 : 0);
	GetGfxDevice().ProfileControl(GfxDevice::kGfxProfEndFrame, frameIDAndValidFlag);
	#if ENABLE_JOB_SCHEDULER
	GetJobScheduler().EndProfilerFrame (frameIDAndValidFlag);
	#endif

	ABSOLUTE_TIME frameStartTime = m_TotalProfilerFrameDuration;
	m_TotalProfilerFrameDuration = SUBTRACTED_TIME(START_TIME, frameStartTime);
	if (profileFrameValid)
	{
			m_PreviousFrames[0]->m_StartTimeUS = GetProfileTime(frameStartTime) / 1000;
			m_PreviousFrames[0]->m_TotalCPUTimeInMicroSec = GetProfileTime(m_TotalProfilerFrameDuration) / 1000;
	}

	{
		Mutex::AutoLock lock(m_ProfilersMutex);
		for(ProfilersList::iterator it = m_Profilers.begin(); it != m_Profilers.end(); ++it)
		{
			UnityProfilerPerThread& prof = **it;
			if (!prof.IsSeparateBeginEnd())
				prof.ClearFrame();
		}
	}
	m_ProfilerEnabledThisFrame = false;
	m_ProfilerAllowSamplingGlobal = false;
}



void UnityProfiler::EndFrameSeparateThread(unsigned frameIDAndValid)
{
	Mutex::AutoLock lock(m_ProfilersMutex);
	UnityProfilerPerThread* profTLS = UnityProfilerPerThread::ms_InstanceTLS;
	if (!profTLS)
		return;
	Assert(profTLS->IsSeparateBeginEnd());

	profTLS->EndFrame();

	const bool frameValid = (frameIDAndValid & 0x80000000);
	if (frameValid)
	{
		Mutex::AutoLock prevFramesLock(m_PrevFramesMutex);
		int frameID = frameIDAndValid & 0x7fffffff;
		for (int i = 0; i < kFrameCount; ++i)
		{
			ProfilerFrameData* frame = m_PreviousFrames[i];
			if (!frame)
				continue;
			if (frame->m_FrameID != frameID)
				continue;
			profTLS->SaveToFrameData(*frame);
		}
	}

	profTLS->ClearFrame();
}


void UnityProfiler::ClearPendingFrames()
{
	Mutex::AutoLock prevFramesLock(m_PrevFramesMutex);
	for (int i = 0; i < kFrameCount; i++)
	{
		UNITY_DELETE(m_PreviousFrames[i],kMemProfiler);
		m_PreviousFrames[i] = NULL;
	}
}


const ProfilerSample* UnityProfilerPerThread::GetActiveSample(int parentLevel) const
{
	const int indexInStack = m_SampleStack.size()-1 - parentLevel;
	if (indexInStack < 0 || indexInStack >= m_SampleStack.size())
		return NULL;
	const int sampleIndex = m_SampleStack[indexInStack];
	const ProfilerSample* sample = &m_ActiveSamples[sampleIndex];
	return sample;
}


void UnityProfilerPerThread::InjectGCCollectSample() 
{
	ProfileTimeFormat collectTime = m_GCCollectTime;
	m_GCCollectTime = 0;

	BeginSample(&gGCCollect, NULL);
	ProfilerSample* gcSample = GetActiveSample();
	EndSample(START_TIME);
	gcSample->timeUS = collectTime/1000;
}


void UnityProfilerPerThread::CreateOverheadSample()
	{
	BeginSample(&gOverheadProfile, NULL);
	ProfilerSample* overheadSample = GetActiveSample();
	EndSample(START_TIME);

	// Calculate overhead time be taking the root time and subtracting all children.
	ProfilerSample* root = GetRoot();
	ProfileTimeFormat overheadTime = root->timeUS * 1000;
	const ProfilerSample* sample = root + 1;
	for (int i=0;i<root->nbChildren;i++)
	{
		overheadTime -= sample->timeUS*1000;
		sample = SkipSampleRecurse(sample);
	}
	overheadSample->timeUS += overheadTime/1000;
}


const ProfilerSample* SkipSampleRecurse (const ProfilerSample* sample)
{
	const ProfilerSample* child = sample + 1;
	for (int i=0;i<sample->nbChildren;i++)
		child = SkipSampleRecurse(child);

	return child;
}


static void UpdateWithSmallestTime (ABSOLUTE_TIME& val, ABSOLUTE_TIME newval, int iteration)
{
	if (iteration == 0 || IsSmallerAbsoluteTime (newval, val))
		val = newval;
}


void UnityProfiler::StartProfilingMode (ProfilerMode mode)
{
	if(m_ProfilerMode & mode)
	{
		SetIsActive(true);
	}
}

void UnityProfiler::EndProfilingMode (ProfilerMode mode)
{
	if(m_ProfilerMode & mode)
		SetIsActive(false);
}


void UnityProfiler::SetIsActive (bool enabled)
{
	if (enabled)
	{
		Mutex::AutoLock lock(m_ProfilersMutex);
		for(ProfilersList::iterator it = m_Profilers.begin(); it != m_Profilers.end(); ++it)
		{
			UnityProfilerPerThread& prof = **it;
			if (!prof.IsSeparateBeginEnd())
				prof.ClearGCCollectTime();
		}
	}
		
	if(!m_ProfilerEnabledThisFrame)
		return;
	// m_EnabledCount can go below 0 (double disable), but will not start before enableCount is 1
	m_EnabledCount += (enabled?1:-1);
	if ( enabled && (m_EnabledCount != 1))
		return;

	if (!enabled && (m_EnabledCount != 0))
		return;
	
	
	if (!enabled && m_ProfilerAllowSamplingGlobal)
	{
		m_ProfilerEnabledDuration += GetProfileTime(SUBTRACTED_TIME(START_TIME, m_LastEnabledTime));
		ABSOLUTE_TIME_INIT(m_LastEnabledTime);
	}
	
	m_ProfilerAllowSamplingGlobal = enabled;
	{
		Mutex::AutoLock lock(m_ProfilersMutex);
		for(ProfilersList::iterator it = m_Profilers.begin(); it != m_Profilers.end(); ++it)
		{
			UnityProfilerPerThread& prof = **it;
			if (!prof.IsSeparateBeginEnd())
				prof.SetIsActive (enabled);
		}
	}
	GetGfxDevice().ProfileControl(GfxDevice::kGfxProfSetActive, enabled ? 1 : 0);
	
	if (enabled && m_ProfilerAllowSamplingGlobal)
	{
		Assert(GetProfileTime (m_LastEnabledTime) == 0);
		m_LastEnabledTime = START_TIME;
	}
}


void UnityProfiler::GetDebugStats (DebugStats& debugStats)
{
	Mutex::AutoLock lock(m_ProfilersMutex);	
	debugStats.m_ProfilerMemoryUsage = 0;
	debugStats.m_AllocatedProfileSamples = 0;

	debugStats.m_ProfilerMemoryUsageOthers = 0;
	for(ProfilersList::iterator it = m_Profilers.begin(); it != m_Profilers.end(); ++it)
	{
		debugStats.m_ProfilerMemoryUsageOthers += (*it)->GetAllocatedBytes();
	}
}

void UnityProfiler::CleanupMonoMethodCaches()
{
	Mutex::AutoLock lock(m_ProfilersMutex);	
	for(ProfilersList::iterator it = m_Profilers.begin(); it != m_Profilers.end(); ++it)
	{
		(*it)->CleanupMonoMethodCache();
	}
}


#if ENABLE_MONO || UNITY_WINRT

ProfilerSample* mono_profiler_begin(ScriptingMethodPtr method, ScriptingClassPtr profileKlass, ScriptingObjectPtr instance)
{
	UnityProfilerPerThread* profTLS = UnityProfilerPerThread::ms_InstanceTLS;
	if (!profTLS || !profTLS->m_ProfilerAllowSampling)
		return NULL;
	UnityProfiler* profiler = UnityProfiler::ms_Instance;
	
	if (!IS_MAIN_THREAD(profiler))
		return NULL;
	
	// If deep profiling, we return the current sample, which is the one we have to get back to when this call exits
	if ((profiler->m_ProfilerMode & kProfilerDeepScripts) != 0)
		return profTLS->GetActiveSample();
	
	DebugAssert(profTLS->m_SampleStack.empty() != profTLS->m_ProfilerAllowSampling);

	// Extract Object ptr for profiler
	Object* objPtr = NULL;

#if ENABLE_MONO
	if (instance)
	{
		MonoClass* instanceClass = mono_object_get_class(instance);
		if (mono_class_is_subclass_of(instanceClass, MONO_COMMON.unityEngineObject, false))
			objPtr = ScriptingObjectOfType<Object>(instance).GetPtr();
	}
#endif
	
	// Do we have this method's info in the cache?
	ProfilerInformation* information;
	UnityProfilerPerThread::MethodInfoCache::iterator it = profTLS->m_ActiveMethodCache.find(method);
	if (it != profTLS->m_ActiveMethodCache.end())
		information = it->second;
	else
		information = profTLS->CreateProfilerInformationForMethod(instance, method, scripting_method_get_name(method), profileKlass, ProfilerInformation::kScriptMonoRuntimeInvoke);
	
	// Begin Sample
	return profTLS->BeginSample(information, objPtr);
}

void mono_profiler_end(ProfilerSample* beginsample)
{
	UnityProfilerPerThread* profTLS = UnityProfilerPerThread::ms_InstanceTLS;
	if (!profTLS || !profTLS->m_ProfilerAllowSampling)
		return;
	UnityProfiler* profiler = UnityProfiler::ms_Instance;
	
	if (!IS_MAIN_THREAD(profiler))
		return;

	// roll back the stack if there has been an exception, and some end samples have been skipped
	while(profTLS->GetActiveSample() != beginsample)
		profTLS->EndSample(START_TIME);		

	// if not deep profiling, end the current sample
	if ((profiler->m_ProfilerMode & kProfilerDeepScripts) == 0)
		profTLS->EndSample(START_TIME);
}

#endif // #if ENABLE_MONO || UNITY_WINRT


ProfilerInformation* UnityProfilerPerThread::CreateProfilerInformationForMethod(ScriptingObjectPtr object, ScriptingMethodPtr method, const char* methodName, ScriptingTypePtr profileKlass, int flags)
{
#if !ENABLE_MONO && !UNITY_WINRT
	return 0;
#else // ENABLE_MONO || UNITY_WINRT

	ProfilerInformation* information = static_cast<ProfilerInformation*> (m_ActiveGlobalAllocator.allocate(sizeof(ProfilerInformation)));
	information->group = kProfilerScripts;
	information->flags = flags;
	information->isWarning = false;

#if ENABLE_MONO
	const char* klassName = mono_class_get_name(mono_method_get_class(method->monoMethod));
#else	// UNITY_WINRT
	const char* klassName = "";
	if (object != SCRIPTING_NULL)
	{
		ScriptingTypePtr klass = scripting_object_get_class(object, GetScriptingTypeRegistry());
		if (klass != NULL)
			klassName = scripting_class_get_name(klass);
	}
#endif

	if (profileKlass == NULL)
	{
		// Optimized snprintf(buffer, kNameBufferSize, "%s.%s()", klassName, methodName); to minimize profiler overhead
		int size = 4;
		for(int i=0;i<klassName[i] != 0;i++) { size++; }
		for(int i=0;i<methodName[i] != 0;i++) { size++; }
		
		char* allocatedBuffer = static_cast<char*> (m_ActiveGlobalAllocator.allocate(size));
		information->name = allocatedBuffer;
		char* c = allocatedBuffer;
		for(int i=0;i<klassName[i] != 0;i++)
		{
			*c = klassName[i]; c++;
		}
		
		*c = '.'; c++;
		for(int i=0;i<methodName[i] != 0;i++)
		{
			*c = methodName[i]; c++;
		}
		
		c[0] = '(';
		c[1] = ')';
		c[2] = 0;

		// Put into method cache
		m_ActiveMethodCache.insert (std::make_pair(method, information));
		
		return information;
	}
	else
	{
		enum { kNameBufferSize = 256 };
		char buffer[kNameBufferSize];
		char coroutineMethodName[kNameBufferSize] = { 0 };
		
		// The generated class for a coroutine is called "<Start>Iterator_1"
		// We want to extract "Start" and use that as the method name.
		const char* coroutineMethodNameEnd = NULL;
		if (klassName[0] == '<')
			coroutineMethodNameEnd = strchr(klassName, '>');
		
		if (coroutineMethodNameEnd != NULL)
			strncpy(coroutineMethodName, klassName + 1, std::min((int)(coroutineMethodNameEnd - (klassName + 1)), (int)kNameBufferSize));
		else
			strncpy(coroutineMethodName, klassName, kNameBufferSize);
			
		snprintf(buffer, kNameBufferSize, "%s.%s() [Coroutine: %s]", scripting_class_get_name(profileKlass), coroutineMethodName, methodName);
		
		int size = strlen(buffer)+1;
		char* copyBuffer = static_cast<char*> (m_ActiveGlobalAllocator.allocate(size));
		memcpy(copyBuffer, buffer, size);
		information->name = copyBuffer;

		// Put into method cache
		m_ActiveMethodCache.insert (std::make_pair(method, information));
		
		return information;
	}
#endif // ENABLE_MONO || UNITY_WINRT
}	



ProfilerInformation* UnityProfilerPerThread::GetProfilerInformation (const std::string& name, UInt16 group, UInt16 flags, bool isWarning)
{
	DynamicMethodCache::iterator found = m_DynamicMethodCache.find(name);
	if (found != m_DynamicMethodCache.end())
		return &found->second;
	else
	{
		// Put into method cache
		DynamicMethodCache::iterator found = m_DynamicMethodCache.insert (std::make_pair(name, ProfilerInformation(NULL, (ProfilerGroup) group))).first;
		found->second.name = found->first.c_str();
		found->second.flags = flags;
		found->second.isWarning = isWarning;
		return &found->second;
	}
}
	

void UnityProfilerPerThread::EnterMonoMethod(MonoMethod *method)
{
#if ENABLE_MONO
	if (!m_ProfilerAllowSampling)
		return;
	const char* methodName = mono_method_get_name(method);
	if (strncmp (methodName, "runtime_invoke", 14) == 0)
		return;

	// Do we have this method's info in the cache?
	ScriptingMethodPtr scriptingMethod = GetScriptingMethodRegistry().GetMethod(method);
	MethodInfoCache::iterator it = m_ActiveMethodCache.find(scriptingMethod);
	if (it != m_ActiveMethodCache.end())
	{
		BeginSample (it->second, NULL);
		return;
	}

	// Method info not in the cache; create and cache it
	ProfilerInformation* information = CreateProfilerInformationForMethod(NULL, scriptingMethod, methodName, NULL, ProfilerInformation::kScriptEnterLeave);
	
	// Begin Sample
	BeginSample(information, NULL);
	#endif // #if ENABLE_MONO
} 


void UnityProfilerPerThread::LeaveMonoMethod(MonoMethod *method)
{
#if ENABLE_MONO
	if (!m_ProfilerAllowSampling)
		return;
	const char* methodName = mono_method_get_name(method);
	if (strncmp (methodName, "runtime_invoke", 14) == 0)
		return;
	
	ABSOLUTE_TIME time = START_TIME;
	EndSample(time);
	#endif // #if ENABLE_MONO
}


void UnityProfilerPerThread::SampleGCAllocation (MonoObject *obj, MonoClass *klass)
{
#if ENABLE_MONO
	if (!m_ProfilerAllowSampling)
		return;
	
#if 0
	// We can extract the name of the class being allocated here.
	string info = Format("\n\t\t%s size: %d", mono_class_get_name(klass), size);
#endif	
	
	int size = mono_object_get_size(obj);
	ProfilerData::AllocatedGCMemory allocSample = {GetActiveSampleIndex(), size};
	m_AllocatedGCMemorySamples.push_back(allocSample);
	#endif // #if ENABLE_MONO
}


static void enter_mono_sample(void* pr, MonoMethod* method)
{
	UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
	if (prof)
		prof->EnterMonoMethod(method);
}

static void leave_mono_sample(void* pr, MonoMethod* method)
{
	UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
	if (prof)
		prof->LeaveMonoMethod(method);
}


#if ENABLE_MONO_MEMORY_PROFILER

void UnityProfiler::SetupProfilerEvents ()
{
	int flags = kMonoProfilerDefaultFlags;
	if (m_PendingProfilerMode & kProfilerDeepScripts)
		flags |= MONO_PROFILE_ENTER_LEAVE;
	
	mono_profiler_set_events (flags);
}

static void sample_mono_shutdown (void *prof)
{
}
	

void UnityProfilerPerThread::SampleGCMonoCallback (void* pr, int event, int generation)
{
	if (event == 1)
	{
		UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
		if (prof)
		{
			prof->m_GCStartTime = START_TIME;
		}
	}

	if (event == 4)
	{
		UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
		if (prof)
		{
			prof->m_GCCollectTime += GetProfileTime(ELAPSED_TIME(prof->m_GCStartTime));
		}
	}
}

static void sample_gc_resize (void *pr, SInt64 new_size)
{
	//	printf_console("--- GC resize %d\n", (int)new_size);
}

static void sample_allocation (void* pr, MonoObject *obj, MonoClass *klass)
{
	UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
	if (prof)
		prof->SampleGCAllocation(obj, klass);
}


void mono_profiler_startup ()
{
	mono_profiler_install (NULL, sample_mono_shutdown);
	
	mono_profiler_install_gc(UnityProfilerPerThread::SampleGCMonoCallback, sample_gc_resize);
	mono_profiler_install_allocation(sample_allocation);
#if UNITY_EDITOR
	// Deep profiling is only be available in editor
	mono_profiler_install_enter_leave (enter_mono_sample, leave_mono_sample);
#endif
	int flags = kMonoProfilerDefaultFlags;
	mono_profiler_set_events (flags);
}

#endif // ENABLE_MONO_MEMORY_PROFILER



void UnityProfiler::LogFrame(ProfilerFrameData* data)
{
#if !UNITY_PEPPER
	if (m_LogFile.empty())
		return;

	float fps = 1000000.0 / data->m_ThreadData[0].GetRoot()->timeUS;

#if DEBUG_AUTO_PROFILER_LOG
	const int kMinimumFramerate = 20;
	if (fps > kMinimumFramerate)
		return;
#endif

	{	
		string fpsCategory;
		if (fps < 10)
			fpsCategory = "Very Low";
		else if (fps < 20)
			fpsCategory = "Low";
		else if (fps < 25)
			fpsCategory = "Okay";
		else if (fps < 25)
			fpsCategory = "Average";
		else if (fps < 40)
			fpsCategory = "Good";
		else
			fpsCategory = "Very Good";
		std::string output = Format(" -- Frame %d Framerate: %.1f [%s Framerate]\n", ++m_FramesLogged, fps, fpsCategory.c_str());

		m_TextFile->Write(output.c_str(),output.length());
	}

	if(m_BinaryLogEnabled)
	{
#if ENABLE_PLAYERCONNECTION
		/// TODO: make async write. Causes stalls
		dynamic_array<int> buffer;

		SerializeFrameData(*data, buffer);
		int size = buffer.size()*sizeof(int);
		m_DataFile->Write(&size,sizeof(size));
		int threadCount = data->m_ThreadCount;
		m_DataFile->Write(&threadCount, sizeof(threadCount));
		m_DataFile->Write(buffer.begin(), size);
#endif
	}
#endif
}

void UnityProfiler::SetLogPath (std::string logPath)
{
#if WEBPLUG
	if(!logPath.empty())
	{
		std::string name(GetConsoleLogPath());
		ConvertSeparatorsToUnity(name);
		logPath = DeleteLastPathNameComponent(name)+"/profile.log";
	}
#endif
	if (m_LogFile != logPath)
	{
		m_LogFile = logPath;
#if !UNITY_PEPPER
		if (!logPath.empty())
		{
			m_FramesLogged = 0;
			if(!m_TextFile)
				m_TextFile = UNITY_NEW(File, kMemProfiler);
			if(!m_DataFile)
				m_DataFile = UNITY_NEW(File, kMemProfiler);
			m_TextFile->Open(m_LogFile, File::kWritePermission);
			m_DataFile->Open(m_LogFile+".data", File::kWritePermission);
		}
		else
		{
			if(m_TextFile)
				m_TextFile->Close();
			if(m_DataFile)
				m_DataFile->Close();
		}
#endif
	}
}
void UnityProfiler::AddFramesFromFile(string path)
{
#if UNITY_EDITOR
	File dataFile;
	dataFile.Open(path+".data", File::kReadPermission);
	int size;
	dynamic_array<int> buffer;
	while(dataFile.Read(&size,sizeof(size)))
	{
		int threadCount;
		dataFile.Read(&threadCount,sizeof(threadCount));
		buffer.resize_uninitialized(size/sizeof(int));
		dataFile.Read(buffer.begin(),size);
		ProfilerFrameData* frame = UNITY_NEW(ProfilerFrameData, kMemProfiler) (threadCount, 0);
	
		int fileguid = ProfilerConnection::Get().GetConnectedProfiler();
		if( UnityProfiler::DeserializeFrameData(frame,buffer.begin(),size) )
			ProfilerHistory::Get().AddFrameDataAndTransferOwnership(frame, fileguid);
		else
			UNITY_DELETE(frame, kMemProfiler);	
	}
	dataFile.Close();
#endif
}

void UnityProfiler::SerializeFrameData(ProfilerFrameData& frame, dynamic_array<int>& buffer)
{
#if ENABLE_PLAYERCONNECTION
	buffer.push_back(UNITY_LITTLE_ENDIAN);
	buffer.push_back(kProfilerDataStreamVersion);

	frame.Serialize(buffer);

	buffer.push_back(0xAFAFAFAF);
#endif
}

bool UnityProfiler::DeserializeFrameData(ProfilerFrameData* frame, const void* data, int size)
{		
#if ENABLE_PLAYERCONNECTION
	int* buffer = (int*)data;
	int wordsize = size/sizeof(int);
	int* endBuffer = buffer + wordsize;
	int** bitstream = &buffer;

	int dataIsLittleEndian = *((*bitstream)++);
	bool shouldswap = UNITY_LITTLE_ENDIAN ? dataIsLittleEndian == 0 : dataIsLittleEndian != 0;
	if(shouldswap)
	{
		int* ptr = *bitstream;
		while(ptr < endBuffer)
			SwapEndianBytes(*(ptr++));
	}

	int version = *((*bitstream)++);

	if(version != kProfilerDataStreamVersion)
		return false;

	frame->Deserialize(bitstream, shouldswap);
	Assert(**bitstream == 0xAFAFAFAF);
	return true;
#else
	return false;
#endif
}


// --------------------------------------------------------------------------
// Profiler serialization


#if ENABLE_PLAYERCONNECTION
template< class T >
void WriteArray(dynamic_array<int>& bitstream, const dynamic_array<T>& array)
{
	bitstream.push_back(array.size());
	if(array.size() > 0)
	{
		int startindex = bitstream.size();
		bitstream.resize_uninitialized( startindex + array.size() * sizeof(T) / sizeof(int) );
		memcpy( (char*)&bitstream[startindex], (char*)&array[0], sizeof(T) * array.size() );
	}
}

template< class T >
void ReadArray( int** bitstream, dynamic_array<T>& array)
{
	int size = *((*bitstream)++);
	array.resize_uninitialized(size);
	if(size > 0)
	{
		memcpy((char*)&array[0], (char*)*bitstream, sizeof(T) * size);
		*bitstream += sizeof(T) * size / sizeof(int);
	}
}

template< typename T >
void ReadArrayFixup( int** bitstream, dynamic_array<T>& array, bool swapdata)
{
	ReadArray<T>(bitstream, array);
	if (swapdata)
	{
		for (typename dynamic_array<T>::iterator it = array.begin(); it != array.end(); ++it)
		{
			(*it).Fixup();
		}
	}
}

void WriteConditionaly(dynamic_array<int>& bitstream, ProfilerInformation* object)
{
	if (object)
	{
		bitstream.push_back(1);
		ProfilerFrameData::SerializeProfilerInformation(*object, bitstream);
	}
	else
		bitstream.push_back(0);
}

void ReadConditionaly( int** bitstream, ProfilerInformation*& object, bool swapdata)
{
	int condition = *((*bitstream)++);
	if(condition)
		object = ProfilerFrameData::DeserializeProfilerInformation(bitstream, swapdata);
}

static void SerializeString (dynamic_array<int>& bitstream, int len, const char* str)
{
	int startindex = bitstream.size();
	bitstream.resize_initialized( startindex + len/4 + 1);
	memcpy((char*)&bitstream[startindex], str, len+1);
}

static std::string DeserializeString (int**& bitstream, bool swapdata)
{
	char* chars = (char*)*bitstream;
	if (swapdata)
	{
		int wordcount = strlen(chars)/4 + 1;
		for(int i = 0; i < wordcount; i++)
			SwapEndianBytes((*bitstream)[i]);
	}
	std::string name((char*)*bitstream);
	(*bitstream) += name.length()/4 + 1;
	return name;
}


#define HIPART(x) ((x>>32) & 0xFFFFFFFF)
#define LOPART(x) (x & 0xFFFFFFFF)

void ProfilerFrameData::Serialize( dynamic_array<int>& bitstream )
{
	bitstream.push_back(frameIndex);
	bitstream.push_back(realFrame);
	bitstream.push_back(m_StartTimeUS);
	bitstream.push_back(m_TotalCPUTimeInMicroSec);
	bitstream.push_back(m_TotalGPUTimeInMicroSec);
	allStats.Serialize(bitstream);

	bitstream.push_back(m_ThreadCount);
	for (int t = 0; t < m_ThreadCount; ++t)
	{
		const ThreadData& tdata = m_ThreadData[t];
		
		SerializeString(bitstream, tdata.m_ThreadName.size(), tdata.m_ThreadName.c_str());
		bitstream.push_back(tdata.m_AllSamples.size());
		for(int i = 0; i < tdata.m_AllSamples.size(); i++)
		{
			bitstream.push_back(tdata.m_AllSamples[i].timeUS);
			bitstream.push_back(tdata.m_AllSamples[i].startTimeUS);
			bitstream.push_back(tdata.m_AllSamples[i].nbChildren);
		}
		
		bitstream.push_back(tdata.m_GPUTimeSamples.size());
		for(int i = 0; i < tdata.m_GPUTimeSamples.size(); i++)
		{
				bitstream.push_back(tdata.m_GPUTimeSamples[i].gpuTimeInMicroSec);
				bitstream.push_back(tdata.m_GPUTimeSamples[i].relatedSampleIndex);
				bitstream.push_back(tdata.m_GPUTimeSamples[i].gpuSection);
		}
		
		// Don't write m_InstanceIDSamples, since the IDs are not portable
		WriteArray(bitstream, tdata.m_AllocatedGCMemorySamples);
		for(int i = 0; i < tdata.m_AllSamples.size(); i++)
			WriteConditionaly(bitstream,tdata.m_AllSamples[i].information);	

		WriteArray(bitstream, tdata.m_WarningSamples);
	}
}

void ProfilerFrameData::Deserialize( int** bitstream, bool swapdata )
{
	frameIndex = *((*bitstream)++);
	realFrame = *((*bitstream)++);
	m_StartTimeUS = *((*bitstream)++);
	m_TotalCPUTimeInMicroSec = *((*bitstream)++);
	m_TotalGPUTimeInMicroSec = *((*bitstream)++);
	allStats.Deserialize(bitstream, swapdata);

	int threadCount = *((*bitstream)++);
	if (threadCount != m_ThreadCount)
	{
		delete[] m_ThreadData;
		m_ThreadData = new ThreadData[threadCount];
		m_ThreadCount = threadCount;
	}

	for (int t = 0; t < m_ThreadCount; ++t)
	{
		ThreadData& tdata = m_ThreadData[t];

		tdata.m_ThreadName = DeserializeString (bitstream, swapdata);
		
		tdata.m_AllSamples.resize_uninitialized(*((*bitstream)++));
		
		for(int i = 0; i < tdata.m_AllSamples.size(); i++)
		{
			tdata.m_AllSamples[i].timeUS = *((*bitstream)++);
			tdata.m_AllSamples[i].startTimeUS = *((*bitstream)++);
			tdata.m_AllSamples[i].nbChildren = *((*bitstream)++);
			tdata.m_AllSamples[i].information = NULL;
		}

		tdata.m_GPUTimeSamples.resize_uninitialized(*((*bitstream)++));
		for(int i = 0; i < tdata.m_GPUTimeSamples.size(); i++)
		{
			tdata.m_GPUTimeSamples[i].gpuTimeInMicroSec = *((*bitstream)++);
			tdata.m_GPUTimeSamples[i].relatedSampleIndex = *((*bitstream)++);
			tdata.m_GPUTimeSamples[i].gpuSection = (GpuSection)(*((*bitstream)++));
			tdata.m_GPUTimeSamples[i].timerQuery = NULL;
		}
	
		// m_InstanceIDSamples are not written, since the IDs are not portable
		tdata.m_InstanceIDSamples.resize_uninitialized(0);
		ReadArray(bitstream, tdata.m_AllocatedGCMemorySamples);
		for(int i = 0; i < tdata.m_AllSamples.size(); i++)
			ReadConditionaly(bitstream, tdata.m_AllSamples[i].information, swapdata);

		ReadArray(bitstream, tdata.m_WarningSamples);
	}
}


void ProfilerFrameData::SerializeProfilerInformation( const ProfilerInformation& info, dynamic_array<int>& bitstream )
{
	SerializeString (bitstream, strlen(info.name), info.name);
	bitstream.push_back((info.group << 16) | (info.flags << 8) | info.isWarning);
}

ProfilerInformation* ProfilerFrameData::DeserializeProfilerInformation( int** bitstream, bool swapdata )
{
	std::string name = DeserializeString (bitstream, swapdata);

	int groupFlags = *((*bitstream)++);
	UInt16 group = groupFlags >> 16;
	UInt8 flags = (groupFlags & 0xFF00) >> 8;
	UInt8 warn = groupFlags & 0xFF;

	UnityProfilerPerThread* prof = UnityProfilerPerThread::ms_InstanceTLS;
	DebugAssert(prof);
	return prof->GetProfilerInformation(name, group, flags, warn);
}

#endif // #if ENABLE_PLAYERCONNECTION




#endif // #if ENABLE_PROFILER
