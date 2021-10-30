#include "UnityPrefix.h"
#include "Editor/Platform/Interface/BugReportingTools.h"
#include <stdlib.h>
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"

#include "Runtime/Utilities/Stacktrace.h"

static void PrintStackTrace ();

static int gLaunchedBugReporter = 0;
static char* gReporter = "UnityBugReporter.exe";

void LaunchBugReporter (BugReportMode mode)
{
	if (mode == kCrashbug)
	{
		if (gLaunchedBugReporter != 0)
			return;
		gLaunchedBugReporter = 1;
	}

	if (mode == kCrashbug)
		printf_console("\n*** Launching bug reporter due to crashbug.\n");
	else if (mode == kFatalError)
		printf_console("\n*** Launching bug reporter due to fatal error!\n");
	else if (mode == kCocoaExceptionOrAssertion)
		printf_console("\n*** Launching bug reporter due to windows exception or assert.\n");

	// do not actually launch bug reporter when in batch mode!
	if (!IsBatchmode())
	{
		PROCESS_INFORMATION pi;
		std::string output;
		std::vector<std::string> arguments;

		if (mode != kManualSimple)
		{
			#if UNITY_EDITOR
			//add project folder to the arguments, if the bug reporter is launched from the editor
			std::string projectPath;
			arguments.push_back("-ProjectFolder");
			projectPath = EditorPrefs::GetString("kProjectBasePath");
			#if UNITY_WIN
			ConvertSeparatorsToWindows( projectPath );
			#endif
			arguments.push_back(projectPath);

			// add buildsettings platform here, since it can change during session
			arguments.push_back("-BuildPlatform");
			std::string appName = GetBuildTargetName(GetEditorUserBuildSettings().GetActiveBuildTarget());
			arguments.push_back(appName);

			#endif
		}

		switch (mode)
		{
		case kCrashbug:	//TODO: it's yet unclear if any code path at all will reach these on windows editor (all except kManualOpen)
		case kCocoaExceptionOrAssertion:
		case kFatalError:
		case kManualOpen:
		case kManualSimple:
			LaunchTaskArrayOptions( gReporter, arguments, kLaunchQuoteArgs, "", pi );
			break;
		}
	}
	
	if (mode == kFatalError || mode == kCocoaExceptionOrAssertion)
	{
		PrintStackTrace ();
		abort();
		exit (1); // Just in case abort returns
	}
}

void ExitDontLaunchBugReporter (int exitValue)
{
	exit (exitValue);
}

void PrintStackTrace ()
{
#if UNITY_WIN
	printf_console("Full stack trace:\n");
	printf_console(GetStacktrace(true).c_str());
#else
	#pragma message("Check if this PrintStackTrace works on OSX")
#endif
}
