#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Src/ProjectWizardUtility.h"
#include "Editor/Src/EditorSettings.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"

using namespace std;

static void OpenTestProject (const std::string& name, vector<string>& additionalArgs)
{
	string testFolder = GetBaseUnityDeveloperFolder () + "/Tests/" + name;

	/* ask joachim why he's deleting these all the time. doesn't seem to make much sense.
	DeleteFileOrDirectory(testFolder + "/Library");
	DeleteFileOrDirectory(testFolder + "/Temp");
	DeleteFileOrDirectory(testFolder + "/results.xml");
	 */
	
	vector<string> args;
	args.push_back ("-projectpath");
	args.push_back (GetBaseUnityDeveloperFolder () + "/Tests/" + name);
	
	for (int i=0; i!=additionalArgs.size(); i++)
		args.push_back(additionalArgs[i]);
	
	RelaunchWithArguments(args);
}

static void CreateSceneBasedTest()
{
	string assetsFolder = GetProjectPath() + "/Assets";
	string editorSubfolder = assetsFolder + "/Editor";
	string testingFunctionsFolder = GetBaseUnityDeveloperFolder() + "/Tools/UnityRuntimeTestFunctions";
	string testingFunctionsPath = testingFunctionsFolder + "/TestingFunctions.cs";
	string projectTestSettingsPath = testingFunctionsFolder + "/ProjectTestSettings.xml";
	//CreateDirectory(editorSubfolder);
	CopyReplaceFile(testingFunctionsPath, assetsFolder + "/TestingFunctions.cs");
	CopyReplaceFile(projectTestSettingsPath, assetsFolder + "/ProjectTestSettings.xml");

	AssetInterface::Get().Refresh();

	GetEditorSettings().SetSerializationMode(EditorSettings::kForceText);
	GetEditorSettings().SetExternalVersionControlSupport(ExternalVersionControlVisibleMetaFiles);
}

struct TestMenu : public MenuInterface
{
	virtual bool Validate (const MenuItem &menuItem)
	{
		return StrICmp(PathToAbsolutePath (""), GetBaseUnityDeveloperFolder () + "/Tests/" + menuItem.m_Command) != 0;
	}

	virtual void Execute (const MenuItem &menuItem)
	{
		vector<string> args;
		if (menuItem.m_Command == "OpenEditorFunctionalTest")
		{
			OpenTestProject("EditorFunctionalTest", args);
		}
		if(menuItem.m_Command == "CreateSceneBasedTest")
		{
			CreateSceneBasedTest();
		}
		else if (menuItem.m_Command == "OpenFunctionalTests")
		{
			args.push_back ("-executeMethod");
			args.push_back ("FunctionalTestWindow.OpenWindow");
			OpenTestProject("FunctionalTests", args);
		} 
		else if (menuItem.m_Command == "OpenAssetServerTests")
		{
			args.push_back ("-executeMethod");
			args.push_back ("TestWindow.OpenWindow");
			args.push_back ("-logfile");
			args.push_back (GetBaseUnityDeveloperFolder () + "/Tests/AssetServerTest/Results/Editor.log");
			OpenTestProject("AssetServerTest", args);
		}
	}
};

void RunTestMenu ()
{
	if (IsDeveloperBuild())
	{
		TestMenu* test = new TestMenu();
		//MenuController::AddMenuItem ("Tests/Editor Functional/Open", "OpenEditorFunctionalTest", test, 150);

		//if (GetProjectPath().find("FunctionalTests") == string::npos)
		//	MenuController::AddMenuItem ("Tests/Functional/Open", "OpenFunctionalTests", test, 150);

		MenuController::AddMenuItem ("Tests/Asset Server/Open", "OpenAssetServerTests", test, 150);
		MenuController::AddMenuItem ("Tests/Scene-based RuntimeTest/Create", "CreateSceneBasedTest", test, 150);
	}
}

STARTUP (RunTestMenu)	// Call this on startup.
