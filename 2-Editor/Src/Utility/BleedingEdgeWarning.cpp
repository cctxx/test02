#include "UnityPrefix.h"

#include "Editor/Platform/Interface/BugReportingTools.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/Argv.h"

void DisplayBleedingEdgeWarningIfRequired()
{
#if UNITY_ISBLEEDINGEDGE_BUILD
	const char* text = "This is a bleeding edge Unity build.\n\nThis build has had no manual testing. It is highly recommended to use an official build from unity3d.com instead.\n\nIf you choose to use this version, please make sure you create a backup of your project before continuing.";
	if (IsHumanControllingUs() && !DisplayDialog ("WARNING!!", text, "I made a backup and want to continue", "Quit"))
			ExitDontLaunchBugReporter ();
#endif
}