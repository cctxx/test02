#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Editor/Src/Panels/HelpPanel.h"
#include "Editor/Platform/Interface/BugReportingTools.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Src/Utility/EditorUpdateCheck.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"

struct HelpMenu : public MenuInterface {
	virtual bool Validate (const MenuItem &menuItem) {
		return true;
	}

	virtual void Execute (const MenuItem &menuItem) {
		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx) {
		case 0:
			ShowNamedHelp ("file:///unity/Manual/index.html");
			break;
		case 1:
			ShowNamedHelp ("file:///unity/Components/index.html");
			break;
		case 2:
			ShowNamedHelp ("file:///unity/ScriptReference/index.html");
			break;
		case 3:
			CallStaticMonoMethod ("WelcomeScreen", "ShowWelcomeScreen");
			break;
		case 4:
			OpenURLInWebbrowser ("http://unity3d.com/whatsnew.html");
			break;
		case 5:
			OpenURLInWebbrowser ("http://forum.unity3d.com");
			break;
		case 6:
			LaunchBugReporter (kManualOpen);
			break;
		case 7:
			EditorUpdateCheck(kShowAlways, false);
			break;
		case 9:
			OpenURLInWebbrowser ("http://answers.unity3d.com");
			break;
		case 10:
			OpenURLInWebbrowser ("http://feedback.unity3d.com");
			break;
		// Wii, PS3, Xbox 360 docs
		case 11:
		case 12:
		case 13:
		case 14:
			{
				BuildTargetPlatform platforms[4] = {kBuildWii, kBuildPS3, kBuildXBOX360, kBuildMetroPlayerX86};
				ShowNamedHelp (std::string("file://" + GetBuildToolsDirectory(platforms[idx - 11]) + "/Documentation/Documentation/Manual/index.html").c_str());
			}
			break;
		}
	}
};

static HelpMenu *gHelpMenu;
void HelpMenuRegisterMenu ();
void HelpMenuRegisterMenu () {
	gHelpMenu = new HelpMenu;

	MenuController::AddMenuItem ("Help/Unity Manual", "0", gHelpMenu);

	BuildTargetPlatform platforms[3] = {kBuildWii, kBuildPS3, kBuildXBOX360};
	for (int i = 0; i < 3; i++)
	{
		if (IsBuildTargetSupported(platforms[i]))
		{
			std::string name = platforms[i] == kBuildMetroPlayerX86 ? "Metro" : GetBuildTargetShortName(platforms[i]);
			MenuController::AddMenuItem ("Help/Unity Manual (" + name + ")", Format("%d", 11 + i), gHelpMenu);
		}
	}

	MenuController::AddMenuItem ("Help/Reference Manual", "1", gHelpMenu);
	MenuController::AddMenuItem ("Help/Scripting Reference", "2", gHelpMenu);
	MenuController::AddSeparator ("Help/");
	MenuController::AddMenuItem ("Help/Unity Forum", "5", gHelpMenu);
	MenuController::AddMenuItem ("Help/Unity Answers", "9", gHelpMenu);
	MenuController::AddMenuItem ("Help/Unity Feedback", "10", gHelpMenu);
	MenuController::AddMenuItem ("Help/Welcome Screen", "3", gHelpMenu);
	MenuController::AddMenuItem ("Help/Check for Updates", "7", gHelpMenu);
	MenuController::AddSeparator ("Help/");
	MenuController::AddMenuItem ("Help/Release Notes", "4", gHelpMenu);
	MenuController::AddMenuItem ("Help/Report a Bug", "6", gHelpMenu);


}

void ShowWelcomeScreenAtStartup () {
	if( IsBatchmode() || HasARGV("rebuildlibrary"))
		return;
	CallStaticMonoMethod ("WelcomeScreen", "ShowWelcomeScreenAtStartup");
}

STARTUP (HelpMenuRegisterMenu)	// Call this on startup.
