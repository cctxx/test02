#include "UnityPrefix.h"
#include "BuildPlayerHelpers.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"
#include "Editor/Src/AssetPipeline/MonoCompilationPipeline.h"

static bool gDelayedPlayerBuildWaiting = false;
static bool gHaveRegisteredCallbacks = false;
static BuildPlayerSetup gDelayedBuildPlayerSetup;

/// Called when the Mono compiler finishes compilation of scripts.
/// This is where we choose whether we can go forward with a delayed
/// player build.
static void OnScriptCompilationFinished (bool success, const MonoCompileErrors& diagnostics)
{
	// If the scripts failed compiling, abort our delayed
	// build.
	if (!success)
	{
		gDelayedPlayerBuildWaiting = false;
		gDelayedBuildPlayerSetup = BuildPlayerSetup ();
	}
}

/// Called when the freshly recompiled scripts have finished reloading.
/// This is where we pick up on a delayed player build and actually execute it.
static void OnScriptsHaveReloaded ()
{
	// Ignore if no delayed build waiting.
	if (!gDelayedPlayerBuildWaiting)
		return;

	// Build.
	BuildPlayer (gDelayedBuildPlayerSetup);

	// Clear out state.
	gDelayedBuildPlayerSetup = BuildPlayerSetup ();
	gDelayedPlayerBuildWaiting = false;
}

void BuildPlayerAfterNextScriptReload (BuildPlayerSetup& setup)
{
	// Make sure there isn't a build already waiting.  We
	// can only have one in the pipeline.
	Assert (!gDelayedPlayerBuildWaiting);

	// If this is the first time we're triggering a delayed build,
	// register our callbacks.
	if (!gHaveRegisteredCallbacks)
	{
		RegisterScriptCompilationFinishedCallback (&OnScriptCompilationFinished);
		
		GlobalCallbacks::Get().didReloadMonoDomain.Register(OnScriptsHaveReloaded);
		gHaveRegisteredCallbacks = true;
	}

	// Save state.
	gDelayedPlayerBuildWaiting = true;
	gDelayedBuildPlayerSetup = setup;
}

void ShowBuildPlayerWindow()
{
	CallStaticMonoMethod("BuildPlayerWindow", "ShowBuildPlayerWindow");
}

void BuildPlayerWithLastSettings()
{
	CallStaticMonoMethod("BuildPlayerWindow", "BuildPlayerAndRun");
}
