#include "UnityPrefix.h"

#if ENABLE_LIGHTMAPPER

#include "LightmapperBeast.h"

#include "Editor/Platform/Interface/EditorUtility.h"
#include "AsyncProgressBar.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"

using namespace BeastUtils;

bool BeastUtils::GetJobResult(ILBJobHandle job)
{
	ILBJobStatus status;

	// Note:
	// it's too early to check the job result when the job is completed,
	// it has to be not running.

	if (ILBGetJobResult(job, &status) != ILB_ST_SUCCESS)
	{
		printf_console("User canceled rendering.\n" );
		return false;
	}

	if (status != ILB_JS_SUCCESS)
	{
		switch(status)
		{
			case ILB_JS_CANCELLED:
				printf_console("User canceled rendering.\n" );
				break;
			case ILB_JS_INVALID_LICENSE:
				printf_console("Problem with the Beast License!\n");
				break;
			case ILB_JS_CMDLINE_ERROR:
				printf_console("Error parsing Beast command line!\n");
				break;
			case ILB_JS_CONFIG_ERROR:
				printf_console("Error parsing Beast config files!\n");
				break;
			case ILB_JS_CRASH:
				printf_console("Error: Beast crashed!\n");
				break;
			case ILB_JS_OTHER_ERROR:
				printf_console("Other error running Beast.\n");
				break;
		}
		return false;
	}
	return true;
}

void LightmapperBeastShared::TickInspectorBackground()
{
	LightmappingProgress progressState = JobProgress.GetProgressState();

	switch(progressState)
	{
		case kLightmappingInProgress:
			break;

		case kLightmappingDone:		
			if (GetJobResult(Job))
			{
				try
				{
					// process the result
					LightmapperBeastResults br(this);
					br.Retrieve();
				}
				catch (BeastException& be)
				{
					WarningString(be.What());
				}
			}

			ScriptingInvocation(MONO_COMMON.lightmappingDone).Invoke();
			
			GetSceneTracker().RemoveSceneInspector(this);

			ProgressThread.WaitForExit();

			// No code referencing member variables after this
			DeleteGlobalLightmapperBeastShared();
			break;

		case kLightmappingCancelled:
			std::string error = JobProgress.GetError();
			if (error != "")
				WarningString(error);

			ScriptingInvocation(MONO_COMMON.lightmappingDone).Invoke();

			GetSceneTracker().RemoveSceneInspector(this);

			ProgressThread.WaitForExit();
			
			// No code referencing member variables after this
			DeleteGlobalLightmapperBeastShared();
			break;
	}
}

bool LightmapperBeastShared::CanOpenScene()
{
	if (DisplayDialog("Baking Lightmap", "Opening a scene will cancel the Lightmap bake.", "Continue Baking", "Open Scene"))
		return false;

	return true;
}

void LightmapperBeastShared::DidOpenScene()
{
	CallStaticMonoMethod("LightmappingWindow", "DidOpenScene");
	Cancel();
}

bool LightmapperBeastShared::CanEnterPlaymode()
{
	if (DisplayDialog("Baking Lightmap", "Entering play mode will cancel the Lightmap bake.", "Continue Baking", "Play"))
		return false;

	Cancel();
	return true;
}

bool LightmapperBeastShared::CanTerminate()
{
	if (DisplayDialog("Baking Lightmap", "Quitting Unity will cancel the Lightmap bake.", "Continue Baking", "Quit"))
		return false;

	ProgressThread.WaitForExit();
	return true;
}

void LightmapperBeastShared::Compute()
{
	ProgressThread.Run(PollBeastForProgress, this);

	while (ProgressThread.IsRunning())
		Thread::Sleep(0.01);

	TickInspectorBackground();
}

void LightmapperBeastShared::ComputeAsync()
{
	ProgressThread.Run(PollBeastForProgress, this);
	GetSceneTracker().AddSceneInspector(this);
}

void LightmapperBeastShared::Cancel()
{
	if (!ProgressThread.IsRunning())
	{
		// Lightmapping finished when we were blocking the editor with a dialog and
		// now we don't want to use the lightmapping results
		JobProgress.SetProgressState(kLightmappingCancelled);
	}
	else
	{
		ProgressThread.SignalQuit();
	}
}

LightmappingProgress LightmapperJobProgress::GetProgressState()
{
	m_Mutex.Lock();
	LightmappingProgress progressState = m_ProgressState;
	m_Mutex.Unlock();
	return progressState;
}

void LightmapperJobProgress::SetProgressState(LightmappingProgress progressState)
{
	m_Mutex.Lock();
	m_ProgressState = progressState;
	m_Mutex.Unlock();
}

float LightmapperJobProgress::GetProgress()
{
	m_Mutex.Lock();
	float progress = m_Progress;
	m_Mutex.Unlock();
	return progress;
}

void LightmapperJobProgress::SetProgress(float progress)
{
	m_Mutex.Lock();
	m_Progress = progress;
	m_Mutex.Unlock();
}

std::string LightmapperJobProgress::GetActivity()
{
	m_Mutex.Lock();
	string activity = m_Activity;
	m_Mutex.Unlock();
	return activity;
}

void LightmapperJobProgress::SetActivity(const string& activity)
{
	m_Mutex.Lock();
	m_Activity = activity;
	m_Mutex.Unlock();
}

std::string LightmapperJobProgress::GetError()
{
	m_Mutex.Lock();
	string error = m_Error;
	m_Mutex.Unlock();
	return error;
}

void LightmapperJobProgress::SetError(const string& error)
{
	m_Mutex.Lock();
	m_Error = error;
	m_Mutex.Unlock();
}

void* LightmapperBeastShared::PollBeastForProgress(void *data)
{
	// DO NOT USE Unity API's here

	const int kWaitJobDoneInterval = 100;

	LightmapperBeastShared& lbs = *(LightmapperBeastShared*)data;

	ILBJobHandle job = lbs.Job;
	LightmapperJobProgress& jobProgress = lbs.JobProgress;
	ILBBool isRunning = true;

	try
	{
		// return immediately with timeout set to 0
		BeastCall(ILBWaitJobDone(job, 0));
		BeastCall(ILBIsJobRunning(job, &isRunning));

		jobProgress.SetProgressState(kLightmappingInProgress);

		while(isRunning && !lbs.ProgressThread.IsQuitSignaled())
		{
			ILBBool newProgress;
			ILBBool newActivity;
			BeastCall(ILBJobHasNewProgress(job, &newActivity, &newProgress));

			ILBStringHandle taskName;
			int32 progressInt = 0;
			BeastCall(ILBGetJobProgress(job, &taskName, &progressInt));
			std::string jobNameString = ConvertStringHandle(taskName);

			if (newActivity)
			{
				std::string kPrefix = "Title: ";
				if (jobNameString.substr(0, kPrefix.length()) == kPrefix)
					jobNameString = jobNameString.substr(kPrefix.length());
				jobProgress.SetActivity(jobNameString);
			}
			if (newProgress)
			{
				int globalProgress;
				BeastCall(ILBGetGlobalJobProgress(job, &globalProgress));
				float globalProgress01 = (float)globalProgress / 100.0f;
				jobProgress.SetProgress(globalProgress01);
			}

			if (newActivity || newProgress)
				GetAsyncProgressBar().Display(jobProgress.GetActivity(), jobProgress.GetProgress());

			BeastCall(ILBWaitJobDone(job, kWaitJobDoneInterval));
			BeastCall(ILBIsJobRunning(job, &isRunning));
		}

		GetAsyncProgressBar().Clear();

		if (lbs.ProgressThread.IsQuitSignaled())
		{
			BeastCall(ILBCancelJob(job));
			printf_console("Lightmapping cancelled\n");
			jobProgress.SetProgressState(kLightmappingCancelled);
		}
		else
		{
			printf_console("Lightmapping done\n");
			jobProgress.SetProgressState(kLightmappingDone);
		}
	}
	catch (BeastException& be)
	{
		GetAsyncProgressBar().Clear();
		// TODO: notify the user on what went wrong
		jobProgress.SetProgressState(kLightmappingCancelled);
	}

	return NULL;
}

#endif // #if ENABLE_LIGHTMAPPER
