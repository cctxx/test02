#include "UnityPrefix.h"
#include "Configuration/UnityConfigureOther.h"
#include "EditorWindowController.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoScript.h"
#include "MenuController.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/LicenseInfo.h"
#include "EditorHelper.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Editor/Src/EditorSettings.h"
#include "Editor/Src/Graphs/GraphUtils.h"
#include "Editor/Src/ProjectWizardUtility.h"
#if UNITY_WIN
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include <ShlObj.h>
#endif
#include "Editor/Src/VersionControl/VCProvider.h"

static void OpenProject (const std::string& name, vector<string>& additionalArgs)
{
	string projectFolder = GetBaseUnityDeveloperFolder () + "/" + name;
	
	vector<string> args;
	args.push_back ("-projectpath");
	args.push_back (projectFolder);
	
	for (int i=0; i!=additionalArgs.size(); i++)
		args.push_back(additionalArgs[i]);
	
	RelaunchWithArguments(args);
}

struct WindowMenu : public MenuInterface
{
	virtual bool Validate (const MenuItem &menuItem) {
		if (menuItem.m_Command == "7")
			return LicenseInfo::Flag (lf_pro_version);
		if (menuItem.m_Command == "10")
			return LicenseInfo::Flag (lf_pro_version);
		if (menuItem.m_Command == "0")
		{
			string vc = GetEditorSettings ().GetExternalVersionControlSupport ();

			if (vc == ExternalVersionControlHiddenMetaFiles)
				// Asset Server
				return LicenseInfo::Flag (lf_maint_client);
			else if (vc == ExternalVersionControlVisibleMetaFiles)
				// Meta
				return false;
			else
				// Subversion, Perforce or other plugins
				return LicenseInfo::Flag (lf_maint_client);
		}
		return true;
	}

	virtual void Execute (const MenuItem &menuItem)
	{ 
		if (menuItem.m_Command == "7")
			CallStaticMonoMethod("CreateBuiltinWindows", "ShowProfilerWindow");

		if (menuItem.m_Command == "8")
			ScriptingInvocation(kGraphsEditorBaseNamespace, "FlowWindow", "ShowFlowWindow").Invoke();

	#if ENABLE_ASSET_STORE
		if (menuItem.m_Command == "9")
			CallStaticMonoMethod("AssetStoreWindow", "Init");
	#endif
		if (menuItem.m_Command == "10")
			CallStaticMonoMethod("HeapshotWindow", "Init");
		if (menuItem.m_Command == "0")
			CallStaticMonoMethod("CreateBuiltinWindows", "ShowVersionControl");
		
		vector<string> args;
		if (menuItem.m_Command == "DocBrowser")
		{
			OpenProject("Tools/UnityTxtParser/UnityDocBrowser", args);
		}
	}

};

void RegisterWindowMenu () 
{
	WindowMenu* menu = new WindowMenu ();
	MenuController::AddMenuItem ("Window/Profiler %7", "7", menu, 2007);

	#if UNITY_LOGIC_GRAPH
	MenuController::AddMenuItem ("Window/Flow %8", "8", menu, 2008);
	#endif

	MenuController::AddMenuItem ("Window/Asset Store %9", "9", menu, 2009);
	MenuController::AddMenuItem ("Window/Version Control %0", "0", menu, 2010);

#if ENABLE_MONO_HEAPSHOT_GUI
	MenuController::AddMenuItem ("Window/Heapshot %10", "10", menu, 2011);
#endif

	if (IsDeveloperBuild())
	{
		if (GetProjectPath().find("UnityDocBrowser") == string::npos)
			MenuController::AddMenuItem ("Window/Doc Browser Project", "DocBrowser", menu, 1000);
	}
}

struct WindowLayoutMenu : public MenuInterface
{
	bool needsRebuild;
	virtual void Update ()
	{
		if (needsRebuild)
		{
			RebuildMenu ();
			needsRebuild = false;
		}
	}
	
	virtual bool Validate (const MenuItem &menuItem) {
		return true;
	}

	virtual void Execute (const MenuItem &menuItem) {
		int idx = atoi (menuItem.m_Command.c_str());

			switch (idx)
			{
			// Save
			case -1:
				CallStaticMonoMethod ("WindowLayout", "SaveGUI");	
			break;
			// Delete
			case -2:
				CallStaticMonoMethod ("WindowLayout", "DeleteGUI");	
			break;
			// Revert
			case -3:
				RevertFactorySettings (false);
			break;
			default:
				bool isLoadingNewProject = false;
				void* params[] = {  MonoStringNew(menuItem.m_Command), &isLoadingNewProject };
				CallStaticMonoMethod ("WindowLayout", "LoadWindowLayout", params);
				break;
			}
	}
};

static WindowLayoutMenu* gWindowLayoutMenu = NULL;
static const char* kCurrentLayoutPath = "Library/CurrentLayout.dwlt";
static const char* kDefaultLayoutName = "Default.wlt";
static const char* kLastLayoutName = "LastLayout.dwlt";

void RevertFactorySettings (bool quitOnCancel)
{
	if (!DisplayDialog("Revert All Window Layouts", "Unity is about to delete all window layout and restore them to the default settings", "Continue", quitOnCancel ? "Quit" : "Cancel"))
	{
		if (quitOnCancel)
			exit (0);
		else
			return;
	}
		
	DeleteFileOrDirectory(GetUnityLayoutsPreferencesFolder ());
	DeleteFileOrDirectory(kCurrentLayoutPath);

	LoadDefaultWindowPreferences ();
}

string GetDefaultLayoutPath ()
{
	return AppendPathName (GetUnityLayoutsPreferencesFolder (), kDefaultLayoutName);
}

void InitializeLayoutPreferencesFolder ()
{
	string defaultLayoutPath = GetDefaultLayoutPath ();

	string layoutResourcesPath = AppendPathName (GetApplicationContentsPath(), "Resources/Layouts");

	// Make sure we have a window layouts preferences folder
	if (!IsDirectoryCreated (GetUnityLayoutsPreferencesFolder ()))
	{
		// If not copy the standard set of window layouts to the preferences folder
		CopyFileOrDirectory (layoutResourcesPath, GetUnityLayoutsPreferencesFolder ());
	}
	// Make sure we have the default layout file in the preferences folder
	else if (!IsFileCreated (defaultLayoutPath))
	{
		// If not copy our default file to the preferences folder
		CopyFileOrDirectory (AppendPathName(layoutResourcesPath, kDefaultLayoutName), defaultLayoutPath);
	}
	Assert(IsFileCreated(defaultLayoutPath));
	ReloadWindowLayoutMenu ();
}

void LoadCurrentLayout (bool newProjectLayout)
{
	string layoutPath = AppendPathName(GetProjectPath(), kCurrentLayoutPath);
	void* params[] = {  MonoStringNew(layoutPath), &newProjectLayout };
	CallStaticMonoMethod ("WindowLayout", "LoadWindowLayout", params);
}

void LoadDefaultLayout ()
{
	InitializeLayoutPreferencesFolder ();

	DeleteFileOrDirectory(kCurrentLayoutPath);
	CopyFileOrDirectory (GetDefaultLayoutPath (), kCurrentLayoutPath);
	Assert(IsFileCreated(kCurrentLayoutPath));

	LoadCurrentLayout (true);
}

void LoadDefaultWindowPreferences ()
{
	string lastLayoutPath = AppendPathName (GetUnityLayoutsPreferencesFolder (), kLastLayoutName);

	InitializeLayoutPreferencesFolder ();

	// Upgrade path from global layout state to per-project layout state
	string oldCurrentLayoutPath = AppendPathName (GetUnityLayoutsPreferencesFolder (), "__Current__.dwlt");
	if (IsFileCreated (oldCurrentLayoutPath) && !IsFileCreated (lastLayoutPath))
	{
		// For projects that were using the global layout state, we'll use the last layout state used.
		// In the case of Unity's first run, the last layout state will be the old global layout state.
		CopyFileOrDirectory (oldCurrentLayoutPath, lastLayoutPath);
	}

	bool newProjectLayout = !IsFileCreated (kCurrentLayoutPath);

	// Make sure we have a current layout file created
	if (newProjectLayout)
	{
		if (IsFileCreated (lastLayoutPath))
			// First we try to load the last layout
			CopyFileOrDirectory (lastLayoutPath, kCurrentLayoutPath);
		else
			// Otherwise we load the default layout that the user could've modified
			CopyFileOrDirectory (GetDefaultLayoutPath (), kCurrentLayoutPath);
	}
	Assert(IsFileCreated(kCurrentLayoutPath));

	// Load the current project layout
	LoadCurrentLayout (newProjectLayout);
}

void ReloadWindowLayoutMenu ()
{
	if (gWindowLayoutMenu == NULL)
	{
		RegisterWindowMenu();
		gWindowLayoutMenu = new WindowLayoutMenu ();
	}
	
	MenuController::RemoveMenuItem ("Window/Layouts");
	
	set<string> layouts;
	GetFolderContentsAtPath(GetUnityLayoutsPreferencesFolder(), layouts);
	for (set<string>::iterator i=layouts.begin();i != layouts.end();i++)
	{
		string name = GetLastPathNameComponent(*i);
		if (GetPathNameExtension(name) == "wlt") // we don't show dwlt layouts
		{
			name = DeletePathNameExtension(name);
			MenuController::AddMenuItem ("Window/Layouts/" + name, *i, gWindowLayoutMenu);
		}
	}
	
	MenuController::AddSeparator ("Window/Layouts/");
	MenuController::AddMenuItem ("Window/Layouts/Save Layout...", "-1", gWindowLayoutMenu);
	MenuController::AddMenuItem ("Window/Layouts/Delete Layout...", "-2", gWindowLayoutMenu);
	MenuController::AddMenuItem ("Window/Layouts/Revert Factory Settings...", "-3", gWindowLayoutMenu);
	gWindowLayoutMenu->needsRebuild = true;
}

static void SaveWindowLayout (const std::string& path)
{
	void* params[] = {  MonoStringNew(path) };
	CallStaticMonoMethod ("WindowLayout", "SaveWindowLayout", params);
}

void SaveDefaultWindowPreferences ()
{
	// Save Project Current Layout
	SaveWindowLayout (AppendPathName (GetProjectPath (), kCurrentLayoutPath));

	// Save Global Last Layout
	SaveWindowLayout (AppendPathName (GetUnityLayoutsPreferencesFolder (), kLastLayoutName));
}

static string GenerateUniqueInternalPath ()
{
	UnityGUID guid; guid.Init();
	return "Library/Unused/" + GUIDToString(guid);
}


bool SaveToSerializedFileAndForget(const std::string& dstPath, const std::vector<Object*>& objects, bool allowTextSerialization /*= false*/)
{
	string internalPath = GenerateUniqueInternalPath();
	
	for (int i=0;i<objects.size();i++)
	{
		if (objects[i] == NULL)
		{
			ErrorString("You may not pass null objects");
			return false;
		}
		
		if (objects[i]->IsPersistent())
		{
			ErrorString("You may not pass in objects that are already persistent");
			return false;
		}
	}

	GetPersistentManager().SetPathRemap (internalPath, dstPath);

	// Reset file
	GetPersistentManager ().DeleteFile (internalPath, PersistentManager::kDontDeleteLoadedObjects);
	GetPersistentManager ().ResetHighestFileIDAtPath (internalPath);
	
	// Persist objects
	for (int i=0;i<objects.size();i++)
	{
		int heapdID = objects[i]->GetInstanceID();
		LocalIdentifierInFileType fileID = 0;
		GetPersistentManager().MakeObjectsPersistent(&heapdID, &fileID, 1, internalPath, PersistentManager::kAllowDontSaveObjectsToBePersistent);
		
		// MonoBehaviour* beh = dynamic_instanceID_cast<MonoBehaviour*> (heapdID);
		// if (beh)
		// 	LogString(beh->GetScriptClassName());
	}
	
	/////@TODO: Disable links to external objects???
	GetPersistentManager().WriteFile (internalPath, BuildTargetSelection::NoTarget (), allowTextSerialization ? kAllowTextSerialization : 0);
	
	for (int i=0;i<objects.size();i++)
	{
		GetPersistentManager().MakeObjectUnpersistent(objects[i]->GetInstanceID(), kDontDestroyFromFile);
	}
	
	GetPersistentManager().SetPathRemap (internalPath, "");


	return true;
}


void LoadSerializedFileAndForget (const std::string& absolutePath, std::vector<Object*>& objects)
{
	string path = GenerateUniqueInternalPath();
	GetPersistentManager().LoadExternalStream (path, absolutePath, kAutoreplaceEditorWindow);
	GetPersistentManager().LoadFileCompletely(path);

	vector<LocalIdentifierInFileType> fileIDs;
	GetPersistentManager().GetAllFileIDs (path, &fileIDs);

	objects.clear();
	for (int i=0;i<fileIDs.size();i++)
	{
		int instanceID = GetPersistentManager().GetInstanceIDFromPathAndFileID(path, fileIDs[i]);
		GetPersistentManager().MakeObjectUnpersistent(instanceID, kDontDestroyFromFile);
		Object* obj = dynamic_instanceID_cast<Object*>(instanceID);
		if (obj)
		{
			objects.push_back(obj);
		}
	}
	
	GetPersistentManager().UnloadStream(path);
}

std::string GetUnityLayoutsPreferencesFolder ()
{
	return AppendPathName(GetUnityPreferencesFolder (), "Layouts");
}

std::string GetUnityPreferencesFolder ()
{
	const char* testFolderName = "Editor-4.x";
	if (HasARGV ("cleanTestPrefs"))
		testFolderName = "Editor-4.x - UnityAutomatedTest";

	#if UNITY_OSX || UNITY_LINUX

	#if UNITY_OSX
	string path = "Library/Preferences/Unity";
	#else
	string path = ".config/unity3d/Preferences";
	#endif

	string result = getenv ("HOME");
	result = AppendPathName(result, path);
	result = AppendPathName(result, testFolderName);

	if (!IsDirectoryCreated(result))
	{
		std::string prevDir = DeleteLastPathNameComponent(result);
		if (!IsDirectoryCreated(prevDir))
			CreateDirectory(prevDir);
		CreateDirectory(result);
	}
	
	return result;
	#elif UNITY_WIN

	// Path is %APPDATA%/Unity/Editor/Preferences

	wchar_t widePath[MAX_PATH];
	if( SUCCEEDED(SHGetFolderPathW( NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, widePath )) )
	{
		std::string path;
		ConvertWindowsPathName( widePath, path );
		path = AppendPathName( path, "Unity" );
		CreateDirectory( path );
		path = AppendPathName( path, testFolderName );
		CreateDirectory( path );
		path = AppendPathName( path, "Preferences" );
		CreateDirectory( path );
		return path;
	}
	else
	{
		return "Library/Preferences"; // if the above fails, just store locally to project
	}
	
	#else
	#error "Unknown platform"
	#endif
}
