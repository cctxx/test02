#pragma once

#include "Editor/Src/SceneInspector.h"

enum SceneBackgroundTaskType
{
	kNavMeshBuildingTask = 0,
	kOcclusionBuildingTask = 1,
	kNumberOfSceneBuildingTasks = 2
};

class SceneBackgroundTask : ISceneInspector
{
public:
	
	// Overridable interface
	virtual             ~SceneBackgroundTask ();
	virtual float       GetProgress () = 0;
	virtual std::string GetStatusText () = 0;
	virtual std::string GetDialogTitle () = 0;

	virtual void        BackgroundStatusChanged () = 0; // CallStaticMonoMethod("OcclusionCullingWindow", "BackgroundTaskStatusChanged");
	virtual bool        IsFinished () = 0; // Determines if all data has been computed and is ready for integration.
	virtual bool        Complete () = 0; // Gives the background task a chance to integrate the data on the main thread.
	virtual SceneBackgroundTaskType GetTaskType () = 0; // The task type

	// Callbacks to prevent opening scenes while baking Tome
	virtual bool        CanOpenScene();
	virtual void        DidOpenScene();
	virtual bool        CanEnterPlaymode();
	virtual bool        CanTerminate();
	
	/// Update progress bar and perform integration when the task is complete
	static void         ProgressBarTick ();
	
	static void         CreateTask (SceneBackgroundTask& task);
	static void         CleanupSingletonBackgroundTask(SceneBackgroundTaskType taskType);
	static bool         IsTaskRunning(SceneBackgroundTaskType taskType);

	static SceneBackgroundTask* GetBackgroundTask(SceneBackgroundTaskType taskType);
};
