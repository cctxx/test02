#include "UnityPrefix.h"
#include "SceneBackgroundTask.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "AsyncProgressBar.h"

SceneBackgroundTask* gBackgroundTask[kNumberOfSceneBuildingTasks] = { NULL };


SceneBackgroundTask::~SceneBackgroundTask ()
{
	
}

void SceneBackgroundTask::CleanupSingletonBackgroundTask (SceneBackgroundTaskType taskID)
{
	SceneBackgroundTask* task = gBackgroundTask[taskID];
	if (task)
	{
		////@TODO: Handle async progress bar case of multiple tasks running at the same time
		task->BackgroundStatusChanged();
		GetSceneTracker().RemoveSceneInspector (task);
		GetAsyncProgressBar().Clear();
		gBackgroundTask[taskID] = NULL;
		delete task;
	}
}

bool SceneBackgroundTask::CanOpenScene()
{
	if (DisplayDialog(GetDialogTitle(), "Opening a new Scene will cancel the computation.", GetDialogTitle(), "Open Scene"))
		return false;
	
	return true;
}

void SceneBackgroundTask::DidOpenScene()
{
	CleanupSingletonBackgroundTask (GetTaskType());
}

bool SceneBackgroundTask::CanEnterPlaymode()
{
	if (DisplayDialog(GetDialogTitle(), "Entering play mode will cancel the computation.", "Continue baking", "Play"))
		return false;
	
	CleanupSingletonBackgroundTask (GetTaskType());
	return true;
}

bool SceneBackgroundTask::CanTerminate()
{
	if (DisplayDialog(GetDialogTitle(), "Quitting Unity will cancel the computation.", "Continue baking", "Quit"))
		return false;
	
	CleanupSingletonBackgroundTask (GetTaskType());
	return true;
}

void SceneBackgroundTask::ProgressBarTick ()
{
	for (int i=0;i<kNumberOfSceneBuildingTasks;i++)
	{
		SceneBackgroundTask* thisTask = gBackgroundTask[i];
		if (thisTask == NULL)
			continue;
		
		///@TODO: Make the status text better
		/// @TODO: make status bar be sum of all background tasks
		GetAsyncProgressBar().Display(thisTask->GetStatusText(), thisTask->GetProgress());
		
		// Integrate 
		if (thisTask->IsFinished())
		{
			thisTask->Complete ();
			CleanupSingletonBackgroundTask ((SceneBackgroundTaskType)i);
		}
	}
}

void SceneBackgroundTask::CreateTask (SceneBackgroundTask& task)
{
	CleanupSingletonBackgroundTask (task.GetTaskType());
	
	/////@TODO: Replace for example lightmap pass? Give warning when replacing different type of computation?
	
	gBackgroundTask[task.GetTaskType()] = &task;
	if (gBackgroundTask)
	{
		GetSceneTracker().AddSceneInspector (&task);
		GetAsyncProgressBar().SetTickerFunction(ProgressBarTick);
	}
	task.BackgroundStatusChanged();
}

bool SceneBackgroundTask::IsTaskRunning(SceneBackgroundTaskType task)
{
	return gBackgroundTask[task] != NULL;
}

SceneBackgroundTask* SceneBackgroundTask::GetBackgroundTask(SceneBackgroundTaskType taskType)
{
	return gBackgroundTask[taskType];
}
