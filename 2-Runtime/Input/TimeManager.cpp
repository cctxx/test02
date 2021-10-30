#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "TimeManager.h"
#include <limits>
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/ReproductionLog.h"
#include "Runtime/Threads/Thread.h"
#if SUPPORT_REPRODUCE_LOG
#include <fstream>
#endif

using namespace std;


#define DEBUG_TIME_MANAGER 0


const float kMaximumDeltaTime = 1.0F / 3.0F;
const float kStartupDeltaTime = 0.02F;
const float kNewDeltaTimeWeight = 0.2F; // for smoothing

float CalcInvDeltaTime (float dt)
{
	if (dt > kMinimumDeltaTime)
		return 1.0F / dt;
	else
		return 1.0F;
}

TimeManager::TimeHolder::TimeHolder()
:	m_CurFrameTime(0)
,	m_LastFrameTime(0)
,	m_DeltaTime(0)
,	m_SmoothDeltaTime(0)
,	m_SmoothingWeight(0)
,	m_InvDeltaTime(0)
{
}

TimeManager::TimeManager (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_CullFrameCount(0)
{
	m_SetTimeManually = false;
	m_UseFixedTimeStep = false;

	m_FixedTime.m_SmoothDeltaTime = m_FixedTime.m_DeltaTime;

	m_TimeScale = 1.0F;
	m_FixedTime.m_DeltaTime = 0.02F;
	m_MaximumTimestep = kMaximumDeltaTime;
	m_LastSyncEnd = 0;
	ResetTime ();
}

TimeManager::~TimeManager () {}

inline void CalcSmoothDeltaTime (TimeManager::TimeHolder& time)
{
	// If existing weight is zero, don't take existing value into account
	time.m_SmoothingWeight *= (1.0F - kNewDeltaTimeWeight);
	time.m_SmoothingWeight += kNewDeltaTimeWeight;
	// As confidence in smoothed value increases the divisor goes towards 1
	float normalized = kNewDeltaTimeWeight / time.m_SmoothingWeight;
	time.m_SmoothDeltaTime = Lerp (time.m_SmoothDeltaTime, time.m_DeltaTime, normalized);
}

void TimeManager::SetPause(bool pause)
{
	m_FirstFrameAfterPause = true;
}

void TimeManager::Sync (float framerate)
{
	double time = GetTimeSinceStartup ();
	// wait for enough time to pass for the requested framerate
	if (framerate > 0)
	{
		// Wait a bit less (0.1ms), to accomodate for small fluctuations (gives much better result in webplayers)
		double frameTime = 1.0/(double)framerate - 0.0001;
		if (!CompareApproximately(time, m_LastSyncEnd) && time - m_LastSyncEnd < frameTime)
		{
#if SUPPORT_THREADS
			Thread::Sleep(frameTime - (time-m_LastSyncEnd));
#endif
			int i = 0;
			double start = GetTimeSinceStartup();
			// do the last adjustment with a busy wait
			do
			{
				time = GetTimeSinceStartup();
				if (++i >=  1000)
				{
					// When using PerfHUD ES together with the NVIDIA time extension
					// the time might be stopped, thus causing a diff of 0.
					if ((time - start) == 0)
					{
						m_LastSyncEnd = GetTimeSinceStartup ();
						return;
					}
					// Need to reset "start" here just in case we started to
					// busy wait and then the time was stopped while busy waiting
					start = time;
					i = 0;
				}
			}
			while(time - m_LastSyncEnd < frameTime);
			m_LastSyncEnd += frameTime;
			return;
		}
	}
	m_LastSyncEnd = GetTimeSinceStartup ();
}

void TimeManager::Update ()
{
	AssertIf (m_UseFixedTimeStep);
	m_FrameCount++;
	m_RenderFrameCount++;
	if (m_SetTimeManually)
		return;

	// Capture framerate is always constant
	if (m_CaptureFramerate > 0)
	{
		#if DEBUG_TIME_MANAGER
		printf_console( "time: setting time using capture framerate of %i, timescale %f\n", m_CaptureFramerate, m_TimeScale );
		#endif
		SetTime (m_DynamicTime.m_CurFrameTime + 1.0F / (float)m_CaptureFramerate * m_TimeScale);
		return;
	}

	// Don't do anything to delta time the first frame!
	if (m_FirstFrameAfterReset)
	{
		m_FirstFrameAfterReset = false;
		return;
	}

	// When coming out of a pause / startup / level load we don't want to have a spike in delta time.
	// So just default to kStartupDeltaTime.
	if (m_FirstFrameAfterPause)
	{
		m_FirstFrameAfterPause = false;
		#if DEBUG_TIME_MANAGER
		printf_console( "time: setting time first frame after pause\n" );
		#endif
		SetTime (m_DynamicTime.m_CurFrameTime + kStartupDeltaTime * m_TimeScale);
		// This is not a real delta time so don't include in smoothed time
		m_ActiveTime.m_SmoothingWeight = 0.0f;
		m_DynamicTime.m_SmoothingWeight = 0.0f;
		return;
	}

	double time = GetTimeSinceStartup () - m_ZeroTime;
	m_RealtimeStartOfFrame = time;

	// clamp the delta time in case a frame takes too long.
	if (time - m_DynamicTime.m_CurFrameTime > m_MaximumTimestep)
	{
		#if DEBUG_TIME_MANAGER
		printf_console( "time: maximum dt (was %f)\n", time - m_DynamicTime.m_CurFrameTime );
		#endif
		SetTime (m_DynamicTime.m_CurFrameTime + m_MaximumTimestep * m_TimeScale);
		return;
	}

	// clamp the delta time in case a frame goes to fast! (prevent delta time being zero)
	if (time - m_DynamicTime.m_CurFrameTime < kMinimumDeltaTime)
	{
		#if DEBUG_TIME_MANAGER
		printf_console( "time: minimum dt (was %f)\n", time - m_DynamicTime.m_CurFrameTime );
		#endif
		SetTime (m_DynamicTime.m_CurFrameTime + kMinimumDeltaTime * m_TimeScale);
		return;
	}

	// Handle time scale
	if (!CompareApproximately (m_TimeScale, 1.0F))
	{
		#if DEBUG_TIME_MANAGER
		printf_console( "time: time scale path, delta %f\n", time - m_DynamicTime.m_CurFrameTime );
		#endif
		float deltaTime = time - m_DynamicTime.m_CurFrameTime;
		SetTime (m_DynamicTime.m_CurFrameTime + deltaTime * m_TimeScale);
		return;
	}

	#if DEBUG_TIME_MANAGER
	printf_console( "time: set to %f\n", time );
	#endif
	m_DynamicTime.m_LastFrameTime = m_DynamicTime.m_CurFrameTime;
	m_DynamicTime.m_CurFrameTime = time;
	m_DynamicTime.m_DeltaTime = m_DynamicTime.m_CurFrameTime - m_DynamicTime.m_LastFrameTime;
	m_DynamicTime.m_InvDeltaTime = CalcInvDeltaTime(m_DynamicTime.m_DeltaTime);
	CalcSmoothDeltaTime (m_DynamicTime);

	m_ActiveTime = m_DynamicTime;
}

void TimeManager::SetDeltaTimeHack (float dt)
{
	m_ActiveTime.m_DeltaTime = max(dt, kMinimumDeltaTime);
	m_ActiveTime.m_InvDeltaTime = CalcInvDeltaTime(m_ActiveTime.m_DeltaTime);
}

void TimeManager::SetTime (double time)
{
	AssertIf (m_UseFixedTimeStep);
//	AssertIf (time - m_DynamicTime.m_CurFrameTime < kMinimumDeltaTime * 0.1F);

	m_DynamicTime.m_LastFrameTime = m_DynamicTime.m_CurFrameTime;
	m_DynamicTime.m_CurFrameTime = time;
	m_DynamicTime.m_DeltaTime = m_DynamicTime.m_CurFrameTime - m_DynamicTime.m_LastFrameTime;

	m_DynamicTime.m_InvDeltaTime = CalcInvDeltaTime(m_DynamicTime.m_DeltaTime);
	CalcSmoothDeltaTime (m_DynamicTime);

	m_ActiveTime = m_DynamicTime;

	// Sync m_ZeroTime with timemanager time
	m_ZeroTime = GetTimeSinceStartup () - m_DynamicTime.m_CurFrameTime;
	#if DEBUG_TIME_MANAGER
	printf_console( "time: set to %f, sync zero to %f\n", time, m_ZeroTime );
	#endif
}

#if UNITY_EDITOR
void TimeManager::NextFrameEditor () {
	m_RenderFrameCount++;
}
#endif

bool TimeManager::StepFixedTime ()
{
	if (m_FixedTime.m_CurFrameTime + m_FixedTime.m_DeltaTime > m_DynamicTime.m_CurFrameTime && !m_FirstFixedFrameAfterReset)
	{
		m_ActiveTime = m_DynamicTime;
		m_UseFixedTimeStep = false;

		return false;
	}

	m_FixedTime.m_LastFrameTime = m_FixedTime.m_CurFrameTime;
	if (!m_FirstFixedFrameAfterReset)
		m_FixedTime.m_CurFrameTime += m_FixedTime.m_DeltaTime;

	m_ActiveTime = m_FixedTime;
	m_UseFixedTimeStep = true;
	m_FirstFixedFrameAfterReset = false;

	return true;
}

void TimeManager::ResetTime ()
{
	AssertIf (m_UseFixedTimeStep);
	m_DynamicTime.m_CurFrameTime = 0.0F;
	m_DynamicTime.m_LastFrameTime = 0.0F;
	if (IsWorldPlaying())
	{
		m_DynamicTime.m_DeltaTime = 0.02F;
		m_DynamicTime.m_InvDeltaTime = 1.0F / m_DynamicTime.m_DeltaTime;
	}
	else
	{
		m_DynamicTime.m_DeltaTime = 0.0F;
		m_DynamicTime.m_InvDeltaTime = 0.0F;
	}
	m_DynamicTime.m_SmoothDeltaTime = 0.0F;
	m_DynamicTime.m_SmoothingWeight = 0.0F;

	m_FixedTime.m_CurFrameTime = 0.0F;
	m_FixedTime.m_LastFrameTime = 0.0F;
	// Dont erase the fixed delta time
	m_FixedTime.m_InvDeltaTime = 1.0F / m_FixedTime.m_DeltaTime;

	m_ActiveTime = m_DynamicTime;

	m_FirstFrameAfterReset = true;
	m_FirstFrameAfterPause = true;
	m_FirstFixedFrameAfterReset = true;

	m_FrameCount = 0;
	m_RenderFrameCount = 0;
	m_ZeroTime = GetTimeSinceStartup ();
	m_RealZeroTime = m_ZeroTime;
	#if DEBUG_TIME_MANAGER
	printf_console( "time: startup, zero time %f\n", m_RealZeroTime );
	#endif
	m_LevelLoadOffset = 0.0F;
	m_CaptureFramerate = 0;
	m_RealtimeStartOfFrame = 0.0;
}

template<class TransferFunction>
void TimeManager::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.Transfer (m_FixedTime.m_DeltaTime, "Fixed Timestep", kSimpleEditorMask);
	transfer.Transfer (m_MaximumTimestep, "Maximum Allowed Timestep", kSimpleEditorMask);
	transfer.Transfer (m_TimeScale, "m_TimeScale", kSimpleEditorMask);
}

void TimeManager::SetFixedDeltaTime (float fixedStep)
{
	fixedStep = clamp<float>(fixedStep, 0.0001F, 10.0F);
	m_FixedTime.m_DeltaTime = fixedStep;
	m_FixedTime.m_InvDeltaTime = 1.0F / m_FixedTime.m_DeltaTime;
	m_FixedTime.m_SmoothDeltaTime = m_FixedTime.m_DeltaTime;

	SetMaximumDeltaTime(m_MaximumTimestep);
}

void TimeManager::SetMaximumDeltaTime (float maxStep)
{
	m_MaximumTimestep = max<float>(maxStep, m_FixedTime.m_DeltaTime);
}

void TimeManager::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad(awakeMode);

	m_FixedTime.m_InvDeltaTime = 1.0F / m_FixedTime.m_DeltaTime;
	m_FixedTime.m_SmoothDeltaTime = m_FixedTime.m_DeltaTime;
}

void TimeManager::CheckConsistency ()
{
	Super::CheckConsistency ();

	m_FixedTime.m_DeltaTime = clamp<float>(m_FixedTime.m_DeltaTime, 0.0001F, 10.0F);
	m_MaximumTimestep = max<float>(m_MaximumTimestep, m_FixedTime.m_DeltaTime);
}

void TimeManager::DidFinishLoadingLevel ()
{
	m_LevelLoadOffset = -m_DynamicTime.m_CurFrameTime;
	// Trying to reconstruct what was intended here, this seems plausible:
	m_FirstFrameAfterPause = m_FirstFrameAfterReset = true;
}

void TimeManager::SetTimeScale (float scale) {
	bool outOfRange = scale <= 100.0f && scale >= 0.0f;
	if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1))
		outOfRange = scale < 100;
	
	if (outOfRange)
	{
		m_TimeScale = scale;
		SetDirty ();
	}
	else
	{
		ErrorString ("Time.timeScale is out of range. Needs to be between 0 and 100.");
	}
}

#if SUPPORT_REPRODUCE_LOG

void TimeManager::ReadLog (std::ifstream& in)
{
	if (!CheckReproduceTag("Time", in))
	{
		FailReproduction("Error reading reproduce log");
		return;
	}

	ReadFloat(in, m_DynamicTime.m_CurFrameTime);
	ReadFloat(in, m_DynamicTime.m_LastFrameTime);
	ReadFloat(in, m_DynamicTime.m_DeltaTime);
	ReadFloat(in, m_DynamicTime.m_SmoothDeltaTime);
	ReadFloat(in, m_DynamicTime.m_InvDeltaTime);
	ReadFloat(in, m_RealtimeStartOfFrame);

	m_ActiveTime = m_DynamicTime;
}

void TimeManager::WriteLog (std::ofstream& out)
{
	// In the web player WebScripting.cpp injects the first new line for the frame
	#if !WEBPLUG
	out << std::endl;
	#endif

	out << "Time" << std::endl;
	WriteFloat(out, m_DynamicTime.m_CurFrameTime); out << " ";
	WriteFloat(out, m_DynamicTime.m_LastFrameTime); out << " ";
	WriteFloat(out, m_DynamicTime.m_DeltaTime); out << " ";
	WriteFloat(out, m_DynamicTime.m_SmoothDeltaTime); out << " ";
	WriteFloat(out, m_DynamicTime.m_InvDeltaTime); out << " ";
	WriteFloat(out, m_RealtimeStartOfFrame); out << std::endl;

	m_ActiveTime = m_DynamicTime;
}

double TimeManager::GetRealtime()
{
	#if SUPPORT_REPRODUCE_LOG

	if (RunningReproduction() && (GetReproduceVersion() == 1 || GetReproduceVersion() == 2))
	{
		return m_RealtimeStartOfFrame;
	}

	if (GetReproduceMode() == kPlaybackReproduceLog)
	{
		std::ifstream& in = *GetReproduceInStream();

		if (!CheckReproduceTag("RealTime", in))
		{
			ErrorString("Grabbing realtime but there are no realtime calls recorded");
			return m_RealtimeStartOfFrame;
		}

		double realtime;
		ReadBigFloat(in, realtime);

		return realtime;
	}
	else if (RunningReproduction())
	{
		double realtime = GetTimeSinceStartup() - m_RealZeroTime;

		std::ofstream& out = *GetReproduceOutStream();
		out << "RealTime ";
		WriteBigFloat(out, realtime);
		out << std::endl;

		return realtime;
	}
	#endif
	double realtime = GetTimeSinceStartup() - m_RealZeroTime;
	return realtime;
}
#else
double TimeManager::GetRealtime()
{
	return GetTimeSinceStartup() - m_RealZeroTime;
}
#endif

#if ENABLE_CLUSTER_SYNC
template<class TransferFunc>
void TimeManager::ClusterTransfer (TransferFunc& transfer)
{
    TRANSFER(m_DynamicTime.m_CurFrameTime);
    TRANSFER(m_DynamicTime.m_LastFrameTime);
    TRANSFER(m_DynamicTime.m_DeltaTime);
    TRANSFER(m_DynamicTime.m_SmoothDeltaTime);
    TRANSFER(m_DynamicTime.m_InvDeltaTime);
    TRANSFER(m_RealtimeStartOfFrame);
    TRANSFER(m_TimeScale);
}
#endif



#if UNITY_ANDROID || UNITY_NACL
#include <sys/time.h>

inline double clock_gettime_to_double ()
{
	timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return time.tv_sec + (double)time.tv_nsec * 0.000000001;
}

double TimeSinceStartupImpl ()
{
	static double sStartTime = 0;

	if (sStartTime == 0)
		sStartTime = clock_gettime_to_double ();

	return clock_gettime_to_double () - sStartTime;
}
#endif


IMPLEMENT_CLASS (TimeManager)
IMPLEMENT_OBJECT_SERIALIZE (TimeManager)
IMPLEMENT_CLUSTER_SERIALIZE(TimeManager)
GET_MANAGER (TimeManager)
