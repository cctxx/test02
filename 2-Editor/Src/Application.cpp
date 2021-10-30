
#include "UnityPrefix.h"
#include "Application.h"

#include "SceneInspector.h"
#include "AsyncProgressBar.h"
#include "ProjectWizardUtility.h"
#include "EditorBuildSettings.h"
#include "EditorUserBuildSettings.h"
#include "Editor/Src/ParticleSystem/ParticleSystemEffect.h"
#include "Editor/Src/ParticleSystem/ParticleSystemEditor.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/Utilities/URLUtility.h"
#include "EditorWindowController.h"
#include "Editor/Platform/Interface/RepaintController.h"
#include "MacroClient.h"
#include "Runtime/Misc/SaveAndLoadHelper.h"
#include "BuildPipeline/BuildBuiltinAssetBundles.h"
#include "BuildPipeline/BuildSerialization.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h" // ParticleSystem::UpdateAll ()
#include "Runtime/Filters/Particles/ParticleEmitter.h"
#include "Editor/Src/LicenseInfo.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Editor/Platform/Interface/ProjectWizard.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Camera/GraphicsSettings.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/GfxDeviceRecreate.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Editor/Platform/Interface/BugReportingTools.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/IMGUI/TextMeshGenerator2.h"
#include "AutoDocumentation.h"
#include "Runtime/Utilities/File.h"
#include "GUIDPersistentManager.h"
#include "EditorHelper.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/AssetModificationCallbacks.h"
#include "Editor/Src/AssetPipeline/MonoCompilationPipeline.h"
#include "Editor/Src/AuxWindowManager.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Misc/DebugUtility.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Audio/AudioManager.h"
#include "MenuController.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Testing/HighLevelTest.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Configuration/UnityConfigureOther.h"
//#include "EditorHighLevelTest.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Utility/AssetPreviews.h"
#include "TooltipManager.h"
#include "Runtime/Video/BaseVideoTexture.h"
#include "Editor/Src/Utility/CustomLighting.h"
#include "Editor/Src/Utility/ActiveEditorTracker.h"
#include "Editor/Src/Commands/HelpMenu.h"
#include "Editor/Src/Undo/UndoManager.h"
#include "Editor/Src/Undo/PropertyDiffUndoRecorder.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"
#include "Editor/Src/AssetServer/ASMonoUtility.h"
#include "Editor/Src/Utility/BuildPlayerUtility.h"
#include "PackageUtility.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/Plugins.h"
#include "Runtime/Misc/Player.h"
#include "Runtime/Input/InputManager.h"
#include "HierarchyState.h"
#include "Selection.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/Camera.h"
#include "EditorExtensionImpl.h"
#include "EditorMonoConsole.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "EditorSettings.h"
#include "Runtime/Utilities/ReportHardware.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Input/GetInput.h"
#include "Editor/Src/AssetServer/ASController.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/GameCode/CallDelayed.h"
#include "Runtime/Testing/Testing.h"
#include "Runtime/Misc/Plugins.h"
#include "Runtime/Profiler/GPUProfiler.h"
#include "Editor/Src/AssetPipeline/ShaderImporter.h"
#include "Editor/Src/AssetPipeline/BumpMapSettings.h"
#include "Runtime/Misc/GraphicsDevicesDB.h"
#include "Editor/Src/Prefabs/PrefabBackwardsCompatibility.h"
#include "Runtime/BaseClasses/Cursor.h"
#if UNITY_WIN
#include "Editor/Platform/Windows/Utility/DirectoryChanges.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "PlatformDependent/Win/WinUtils.h"
#endif
#include "Editor/Src/AssetPipeline/AssetImporterUtil.h"
#include "Editor/Src/AssetPipeline/SubstanceImporter.h"
#include "Editor/Src/Utility/CurlRequest.h"
#include "Editor/Src/Utility/Analytics.h"
#include "Editor/Src/Utility/EditorUpdateCheck.h"
#include "Editor/Src/EditorModules.h"

#include "Runtime/Misc/PreloadManager.h"
#include "Runtime/Network/PlayerCommunicator/EditorConnection.h"
#include "Editor/Src/AssetPipeline/LogicGraphCompilationPipeline.h"
#include "Runtime/Graphics/SubstanceSystem.h"
#include "Runtime/BaseClasses/Tags.h"
#include "ValidateProjectStructure.h"
#include "ManagerBackup.h"
#include "Runtime/BaseClasses/CleanupManager.h"
#include "Editor/Src/VersionControl/VCProvider.h"
#include "Editor/Src/VersionControl/VCCache.h"
#include "Editor/Src/EditorAssetGarbageCollectManager.h"
#include "Editor/Src/SpritePacker/SpritePacker.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Mono/MonoAttributeHelpers.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"
#include "Runtime/Scripting/Scripting.h"

// For WebViewScripting maintenance routines
#include "Editor/Src/WebViewScripting.h"
#include "Editor/Src/ShaderMenu.h"

void SetStatusHint (const std::string& statusHint, float timeout = 0.5F);

using namespace std;

static const char* kLastOpenedScene = "LastOpenedScene";
static const char* kAutoRefresh = "kAutoRefresh";
static const char* kUnityPackageExtension = "unitypackage";
static const char* kEditModeScenePath = "Temp/__EditModeScene";
static const char* kTempFolder = "Temp";

static Application* gApp = NULL;
static HardwareInfoReporter g_HardwareReporter;

static void HandleTooLargeNew ();
bool SavePanelIsValidFileName (void* userData, const string& fileName);
bool OpenPanelIsValidFileName (void* userData, const string& fileName);
bool OpenPanelShouldShowFileName (void* userData, const string& fileName);
bool SavePanelShouldShowFileName (void* userData, const string& fileName);
static void ScriptsChangedCallback ();

Application::Application ()
:	m_InsideTickTimer(false)
,	m_IgnoreInspectorChanges(false)
,	m_URLToOpenAfterLaunch()
,	m_LaunchTime(0.0)
,	m_IsUpgradingProject(false)
,	m_ActiveGoMightHaveChanged(false)
,	m_RecreateGfxDevice(false)
,	m_RecreateSkinnedMeshResources(false)
#if UNITY_WIN
,	m_LockFile(INVALID_HANDLE_VALUE)
#endif
{
	m_MacroClient = NULL;
	m_DisallowReloadAssemblies = 0;
	m_SetIsPlayingDelayed = -1;
	m_LastUpdateTime = -1;
	m_DisallowUpdating = 0;
	m_DisallowAutoRefresh = 0;
	m_ChangeLayout = m_DidSceneChangeBeforePlaymode = m_RequestScriptReload = m_SceneRepaintDirty = m_CompilationDelayPlaymode = m_IsInitialized = m_DidCancelSave = m_DidSceneChange = m_DisableDrawHouseKeeping = m_DoOneStep = m_IsPaused = m_SwitchSkin = m_RepaintAllViews = false;
	m_OpenSceneChangedOnDisk = false;
	SceneTracker::Initialize();
	ParticleSystemEditor::Initialize ();
	gApp = this;

	#if !UNITY_RELEASE
	m_CurTickCheckAwakeFromLoad = 0;
	#endif
}

Application& GetApplication()
{
	return *gApp;
}

Application* GetApplicationPtr()
{
	return gApp;
}

/// When scripts have changed.
static void ScriptsChangedCallback ()
{
	// Ensure that master event is created and synchronized.
	// Otherwise
	ScriptingInvocation invocation;
	invocation.method = MONO_COMMON.makeMasterEventCurrent;
	invocation.Invoke();
	
	// Reimport assets if the post processors has changed
	AssetInterface::Get ().StartAssetEditing();
	AssetInterface::Get ().ImportAssetsWithMismatchingAssetImporterVersion();
	AssetInterface::Get ().StopAssetEditing();
	
	// Reloaded script notification to script code
	ScriptingArguments no_arguments;
	CallMethodsWithAttribute(MONO_COMMON.didReloadScripts, no_arguments, NULL);
	
	GetSceneTracker().ForceReloadInspector();
}

static void RecordMacroMenuItemAndAnalytics (MenuItem& item)
{
	string path = MenuController::ExtractMenuItemPath(item);
	// Add analytics pageview to non temporary menu items, this is only to test the analytics system  
	if ( item.m_Parent && (item.m_Parent->m_Name != "TEMPORARY-OBJECT-DISPLAY") ) 
	{
		AnalyticsTrackPageView("/Editor/" + path);
	}
}

void SetupDefaultPreferences ()
{
	// For integration tests, clear the preferences file before running.
	// Do it after setting up the project path since it is quite convenient
	// to have the last integration test automatically launch, when you run unity afterwards to debug.
	if (HasARGV ("cleanTestPrefs"))
	{
		EditorPrefs::UseCleanTestPrefs();
		DeleteFileOrDirectory(GetUnityPreferencesFolder());
	}

	if (!EditorPrefs::HasKey(kAutoRefresh))
		EditorPrefs::SetBool(kAutoRefresh, true);
	
	if (!EditorPrefs::HasKey(kProjectBasePath) && !EditorPrefs::HasKey("LastOpenedScene") && !EditorPrefs::HasKey(Format(kRecentlyUsedProjectPaths, 0)))
	{
		std::string projectPath = Format("/Users/Shared/Unity/%s", UNITY_EXAMPLE_PROJECT_NAME);
		EditorPrefs::SetString(kProjectBasePath, projectPath);
		EditorPrefs::SetString("LastOpenedScene", UNITY_EXAMPLE_PROJECT_SCENE);
		EditorPrefs::SetString(Format(kRecentlyUsedProjectPaths, 0), projectPath);
	}
}

void EnsureSuitableFileSystem ()
{
// Is the project on a case sensitive disk? We don't support that!
//
// Yet... On Linux we will figure out the proper way to play with
// file system case sensitivity
#if !UNITY_LINUX
	DeleteFileOrDirectory (AppendPathName (kTempFolder, "CaseSensitiveTest"));
	DeleteFileOrDirectory (AppendPathName (kTempFolder, "casesensitivetest"));
	if (CreateFile (AppendPathName (kTempFolder, "CaseSensitiveTest")) &&
	    CreateFile (AppendPathName (kTempFolder, "casesensitivetest")))
		FatalErrorStringDontReport ("The project is on case sensitive file system.\nCase sensitive file systems are not supported at the moment.\nPlease move the project folder to a case insensitive file system.");
#endif
}


static void EnsureSupportedGfxDevice()
{
	std::string msg = gGraphicsCaps.CheckGPUSupported();
	if (msg.empty())
		return;
	DisplayDialog("Graphics card not supported by Unity", msg, "OK");
	ExitDontLaunchBugReporter(1);
}


static void HandleProjectAlreadyOpenInAnotherInstance ()
{
	string error = "It looks like another Unity instance is running with this project open.\n\nMultiple Unity instances cannot open the same project.\n\nProject: " + GetProjectPath();

	if (IsBatchmode ())
	{
		// In batchmode, we have no choice but to terminate.
		FatalErrorStringDontReport (error);
	}
	else
	{
		// Otherwise we just display the error and bump the user
		// back to the project wizard.
		DisplayDialog ("Error!", error, "Ok", "");

		RunProjectWizard (true, false);
	}
}


void Application::InitializeProject ()
{
	// initialize curl here (needs to be on the main thread!)
	CurlRequestInitialize ();

	// Editor ping back
	if ( IsTimeToRunUpdateCheck() )
	{		
		bool showAtStartUp = EditorPrefs::GetBool("EditorUpdateShowAtStartup", true);
		bool batchmode = IsBatchmode();
		EditorUpdateCheck((showAtStartUp && !batchmode) ? kShowIfNewerVersionExists : kDoNotShow, true);
	}	
	// Analytics 
	AnalyticsTrackPageView("/Application/Launch");
	
	string lic = "Unity";
	if ( LicenseInfo::Flag (lf_pro_version) )
	{
		lic = "Pro";
	}
	if ( LicenseInfo::Flag (lf_trial) )
	{
		lic += "Trial";
	}
	AnalyticsTrackEvent("License", lic, "", 1);
	
	LicenseInfo::QueryLicenseUpdate();

	// Cache the MAC address early, as fetching it after allocating a lot of memory fails. (case 551060)
	systeminfo::GetMacAddressForBeast ();
		
	m_LaunchTime = GetTimeSinceStartup();
	
	Thread::mainThreadId = Thread::GetCurrentThreadID ();
/*	#if ENABLE_THREAD_CHECK_IN_ALLOCS
	g_ForwardFrameAllocator.SetThreadIDs (Thread::GetCurrentThreadID (), Thread::GetCurrentThreadID ());
	#endif
*/
	GetSceneTracker().AddSceneInspector(this);
	
	ActiveEditorTracker::InitializeSharedTracker();

	// Load Mono console after mono is loaded
	RegisterLogToConsole (LogToConsoleImplementation);
	RegisterRemoveImportErrorFromConsole (RemoveLogImplementation);
	RegisterShowErrorWithMode (ShowFirstErrorWithMode);
	
	// Initialize mono utility functions
	AssetServer::ASMonoUtility::Get();
	
	// Insert license information into the About Dialog
 	LicenseInfo::Get();

	
	#if defined(TIME_LIMIT_EDITOR)
	if (!CheckDate(TIME_LIMIT_EDITOR))
	{
		exit(1);
	}
	#endif
	
	// Determine project path.
	if (IsBatchmode())
	{
		printf_console ("\n BATCHMODE ARGUMENTS:\n");
		PrintARGV ();	
	}

	if (HasARGV ("connectToMacroClient"))
	{
		vector<string> values = GetValuesForARGV ("connectToMacroClient");
		if (values.size() == 1)
		{
			int port = StringToInt(values[0]);
			ConnectMacroClient(port);
			printf_console("Connecting to macroclient!");
		}
		else
			CheckBatchModeErrorString ("connectToMacroServer has wrong amount of parameters");
	}
	
	
	bool hasProjectArguments = false;
	bool forgetProjectPath = HasARGV("forgetProjectPath");

	// Create a new project on startup (-createproject "projectpath" "package1.unity" "package2.unity" ...)
	if (HasARGV ("createProject"))
	{
		string projectPath = GetFirstValueForARGV ("createProject");
		if (!projectPath.empty () && CreateDirectory (projectPath))
		{
			SetProjectPath (projectPath, forgetProjectPath);
			vector<string> packages = GetValuesForARGV ("createProject");
			packages.erase (packages.begin (), packages.begin () + 1);
			
			ProjectDefaultTemplate templateOption = (ProjectDefaultTemplate)0;
			if (HasARGV("projectTemplate"))
				templateOption = (ProjectDefaultTemplate)StringToInt(GetFirstValueForARGV("projectTemplate"));
			
			SetIsCreatingProject (packages, templateOption);
			hasProjectArguments = true;
		}
		else
		{
			CheckBatchModeErrorString ("Creating project folder: " + projectPath + " failed.");
		
			DisplayDialog ("Creating project folder failed!", "Creating Project folder failed!", "Continue");
			SetProjectPath ("", forgetProjectPath);
		}
		
		if (!IsBatchmode())
			ActivateApplication ();
	}
	// setup project path through commandline
	else if (HasARGV ("projectpath"))
	{
		string projectPath = GetFirstValueForARGV ("projectpath");
		if (!IsProjectFolder (projectPath))
		{
			// When doing asset server update we create the project if it does not exist already
			if (HasARGV ("assetServerUpdate"))
			{
				string parentDirPath = PlatformDeleteLastPathNameComponent(projectPath);
				if (!IsDirectoryCreated(projectPath) && IsDirectoryCreated(parentDirPath))
				{
					if (!CreateDirectory(projectPath))
					{
						CheckBatchModeErrorString("Failed to create project directory\n");
					}
				}
				else
				{
					CheckBatchModeErrorString ("Parent dir does not exist. Couldn't set project path to: " + projectPath);
				}
			}
			else
			{
				CheckBatchModeErrorString ("Couldn't set project path to: " + projectPath);
			}
		}

		printf_console ("Successfully changed project path to: %s\n", projectPath.c_str ());
		SetProjectPath (projectPath, forgetProjectPath);
		hasProjectArguments = true;
		
		if (!IsBatchmode())
			ActivateApplication ();
	}
	// Open with arbitrary file (e.g. double click on package or scene)
	else if (HasARGV ("openfile"))
	{
		string filePath = GetFirstValueForARGV ("openfile");
		ConvertSeparatorsToUnity(filePath);
		if( SetupProjectPathWithOpenFile(filePath) ) 
		{
			printf_console("Opening project via file: %s\n", filePath.c_str());
			if (!IsBatchmode())
				ActivateApplication();
			hasProjectArguments = true;
		}
	}
	
	// Automatically open the scene!
	if (HasARGV ("openscene"))
	{
		printf_console ("Open scene: %s\n", GetFirstValueForARGV ("openscene").c_str ());
		EditorPrefs::SetString(kLastOpenedScene, GetFirstValueForARGV ("openscene"));
	}
	// Get the project path from the prefs.
	// If  the project path can't be opened or the user pressed the alt key.
	// - Display the quick project setup panel
	if (!IsCreatingProject () && !IsBatchmode())
	{
		if (!IsProjectFolder (GetProjectPath ()) || IsOptionKeyDown() || (!hasProjectArguments && EditorPrefs::GetBool("AlwaysShowProjectWizard") && !HasARGV("rebuildlibrary")))
		{
			RunProjectWizard(true, false);
		}
	}
	
	PersistentManager::RegisterSafeBinaryReadCallback (&RemapOldPrefabOverrideFromLoading);

	UNITY_NEW_AS_ROOT (GUIDPersistentManager (0), kMemManager, "GUIDManager", "");

	// Go into a loop until we have successfully opened a project.
	// In non-batch mode, we bring the user back to the project wizard
	// for as long as that is not the case.  In batch-mode, we terminate
	// here if we fail.
	while (true)
	{	
		// Get the real pathname from the file system (case sensitive)
		SetProjectPath (GetActualAbsolutePathSlow (GetProjectPath ()), forgetProjectPath);
	
		// Set read&write permissions
		#if !UNITY_WIN
		// on OSX (don't do it in the debugger since it doesn't work there)
		if (!AmIBeingDebugged ())
		#endif

		if (!SetPermissionsForProjectFolder (GetProjectPath ()))
		{
			CheckBatchModeErrorString ("Couldn't change permissions\n");
			ExitDontLaunchBugReporter (1);
		}
	
		printf_console("%s\n", GetProjectPath ().c_str());

		// Make the project folder our current directory.
		SetCocoaCurrentDirectory (GetProjectPath ());
		File::SetCurrentDirectory (GetProjectPath ());

		EnsureSuitableFileSystem ();

		// If the temp folder already exists, could be there's another
		// Unity instance that already has the project open.
		if (IsDirectoryCreated (kTempFolder))
		{
			// Try to get a lock.
			if (!AcquireProjectLock ())
			{
				HandleProjectAlreadyOpenInAnotherInstance ();
				continue;
			}

			// We're going to blast the Temp folder, so temporarily
			// release the lock/
			ReleaseProjectLock ();
		}

		// Make sure we have Temp folder; asset refreshing might need it
		DeleteFileOrDirectory (kTempFolder);
		CreateDirectorySafe (kTempFolder);
		SetFileFlags (kTempFolder, kAllFileFlags, kFileFlagDontIndex); // don't index Temp folder
	
		// (Re-)lock the project.
		if (!AcquireProjectLock ())
		{
			HandleProjectAlreadyOpenInAnotherInstance ();
			continue;
		}

		// We've successfully opened the project.
		break;
	}	

	// Setup new handler to Fatal Error
	set_new_handler (HandleTooLargeNew);

	// Initialize Engine (eg. Setup RTTI, Call Static Initialize functions, Setup messages and Tags)
	if (!InitializeEngineNoGraphics ())
		FatalErrorString ("Failed to initialize unity.");

	// Initialize asset system
	int prevForwardCompatibleVersion;
	bool saveAssets = AssetInterface::Get ().Initialize (HasARGV ("rebuildlibrary") || IsCreatingProject (), &prevForwardCompatibleVersion);

	if (!InitializeEngineGraphics ())
		FatalErrorString ("Failed to initialize unity graphics.");

	EnsureSupportedGfxDevice();

	// Disallow loading builtin resources when building them.
	// This is to ensure that there are no dependencies on the builtin resources when building them.
	BuiltinResourceManager::SetAllowLoadingBuiltinResources(!HasARGV ("buildBuiltinUnityResources") && !HasARGV("buildBuiltinOldWebResources"));
	
	// after initializing graphics, submit analytics event for unknown-yet GPUs, so we can add them
	if ((GetGraphicsPixelFillrate(gGraphicsCaps.vendorID, gGraphicsCaps.rendererID) < 0) && !gGraphicsCaps.rendererString.empty() && gGraphicsCaps.rendererID != 0 && gGraphicsCaps.vendorID != 0)
	{
		std::string str = Format("0x%04X 0x%04X %s", gGraphicsCaps.vendorID, gGraphicsCaps.rendererID, gGraphicsCaps.rendererString.c_str());
		AnalyticsTrackEvent("Graphics", std::string("UnknownDevice-") + UNITY_VERSION_DIGITS, str, 1);
	}
	
	InitializeAutoDocumentation ();

	CustomLighting::Initialize ();
		
	if (HasARGV ("enableMetaData"))
	{
		printf_console ("enabling meta data external version control support!\n");
		GetEditorSettings().SetExternalVersionControlSupport(ExternalVersionControlVisibleMetaFiles);
	}

	CreateWorldEditor ();
	LoadMonoAssembliesOrRecompile ();
	CleanupAfterLoad();

	ValidateProjectStructureAndAbort (GetProjectPath());
	
	// Check if open scene name changed after moving/renaming open scene and update window title accordingly
	AssetDatabase::RegisterPostprocessCallback (WindowTitlePostprocess);

	BuiltinResourceManager::LoadAllDefaultResourcesFromEditor ();

	CallStaticMonoMethod("PreferencesWindow", "SetupDefaultPreferences");
	
	// AutoRefresh lets us disable automatic refreshing of assets. To be on the safe side, always refresh in batchmode and when using the macro client
	// in order to get consistent behaviour there and make sure that all assets have been imported prior to -executeMethod being called.
	bool performRefresh = EditorPrefs::GetBool(kAutoRefresh) || saveAssets;
	if (IsBatchmode() || HasARGV ("connectToMacroClient") || HasARGV ("executeMethod"))
		performRefresh = true;

	if (prevForwardCompatibleVersion < 30)
	{
		performRefresh = true;
		ScriptingInvocation("UnityEditor.Scripting", "PragmaFixing30", "FixJavaScriptPragmas").Invoke();
	}

	ReadLicenseInfo();
	// report hardware after reading license info, to get correct Pro/Trial/etc. flags
	g_HardwareReporter.ReportHardwareInfo();
	
	GlobalCallbacks::Get().didReloadMonoDomain.Register(ScriptsChangedCallback);
	
	if (HasARGV ("buildTarget"))
	{
		string targetName = GetFirstValueForARGV ("buildTarget");

		// Translate name to target platform ID.
		BuildTargetPlatform target = GetBuildTargetByName (targetName);
		if (target == kBuildNoTargetPlatform || !IsBuildTargetSupported (target))
		{
			FatalErrorStringDontReport (Format ("Build target platform '%s' is not a supported build target.", targetName.c_str ()));
		}

		printf_console ("Targeting platform: %s\n", targetName.c_str ());

		// Switch platform.
		SwitchActiveBuildTargetForEmulation (target);
	}

	// Upgrade shaders if needed
	bool upgradedShaders = UpgradeShadersIfNeeded (saveAssets);
	if (upgradedShaders)
		performRefresh = true;

	// Write the project settings file here, so that when the project settings later get reloaded from disk they will have the changes from above.
	// Eg. shader upgrade or unityForwardCompatibleVersion = UNITY_FORWARD_COMPATIBLE_VERSION;
	// We do this after all upgrade operations have been performed
	if (GetPersistentManager().TestNeedWriteFile(kProjectSettingsPath))
		GetPersistentManager().WriteFile(kProjectSettingsPath);

	// Fill default set of always included shaders (can't do in initial Reset of render manager since resources aren't loaded yet)
	if (GetGraphicsSettings().DoesNeedToInitializeDefaultShaders())
	{
		GetGraphicsSettings().SetDefaultAlwaysIncludedShaders();
		GetPersistentManager().WriteFile(kGraphicsSettingsPath);
	}
	
	// All managers and default resources are loaded now.
	// We can now start importing assets
	// Import synchronously so that new editor scripts are applied to imported assets for example.
	if (performRefresh)
		AssetInterface::Get ().Refresh (kAllowForceSynchronousImport);
	
	AssetInterface::Get ().ImportAssetsWithMismatchingAssetImporterVersion();
	
	saveAssets |= UpgradeStandardAssets ();
	ImportProjectWizardPackagesAndTemplate ();

	UpdateRuntimeHashes ();
	// TODO: allow cancel here?
	VerifyAssetsForBuildTarget(false, AssetInterface::kNoCancel);

	// Synchronize Visual Studio Project folder
	CallStaticMonoMethod ("SyncVS", "SyncVisualStudioProjectIfItAlreadyExists");
	
	// We try to call SaveAssets as little as possible because it is slow
	// (Writes the entire assetdatabase and all serialized assets)
	// But after a rebuild we have spent so much time anyway that 
	// it doesn't matter and its annoying if the app crashes afterwards
	if (saveAssets)
		AssetInterface::Get ().SaveAssets ();
		
	// Don't do it if Unity is launched unlicensed.
	if( LicenseInfo::Get()->GetRawFlags() != 0 )
	{
		// Setup menus
		ExecuteStartups();
	
		// Fill in all menu items created by subsystems
		MenuController::RegisterDidExecuteMenuItem(&RecordMacroMenuItemAndAnalytics);
		MenuController::UpdateAllMenus ();
		RebuildMenu();
	}
	
	#if ENABLE_UNIT_TESTS
	// hack for the time being to reduce time to run all integration tests
	// Highlevel tests depends on builtin resources, which can make the tests fail if we want to build them
	bool runHighLevelTest = !HasARGV("connectToMacroClient") && !HasARGV("buildBuiltinUnityResources") && !HasARGV("buildBuiltinOldWebResources") && !HasARGV("buildBuiltinPreviews");
	// Only run high level tests in developer builds
	if (!IsDeveloperBuild())
		runHighLevelTest = false;
	if (runHighLevelTest)
	{
		RunHighLevelTest ();
		//RunEditorHighLevelTest ();
	}
	#endif
}

static std::string GetSearchFiltersPath ()
{
	return AppendPathName(GetUnityPreferencesFolder (), "SearchFilters");
} 

static void CopyDefaultSearchFiltersIfNeeded ()
{
	if (!IsFileCreated ( GetSearchFiltersPath ()))
		CopyFileOrDirectory (AppendPathName(GetApplicationContentsPath(), "Resources/SearchFilters"), GetSearchFiltersPath ());
}

void Application::FinishLoadingProject ()
{
	if (HasARGV ("buildBuiltinUnityResources") || HasARGV("buildBuiltinOldWebResources"))
		ParseARGVCommands ();
	
	if (!IsBatchmode () && LicenseInfo::Get()->GetRawFlags() != 0 )
	{
		CopyDefaultSearchFiltersIfNeeded ();
		LoadDefaultWindowPreferences();
	#if UNITY_EDITOR
		// we change emulation to default for selected platform prior to creating GUI
		gGraphicsCaps.ApplyEmulationSettingsAffectingGUI();
	#endif
	}

    // Don't do it if Unity is launched unlicensed.
    if( LicenseInfo::Get()->GetRawFlags() != 0 )
    {
        // Open last opened scene
        if (!OpenScene (EditorPrefs::GetString(kLastOpenedScene)))
        {
            NewScene();
        }
    }

	m_IsInitialized = true;

	ParseARGVCommands ();
		
	// Don't show if Unity is launched unlicensed.
	if( LicenseInfo::Get()->GetRawFlags() != 0 )
	{
		// Display the welcome screen...
		ShowWelcomeScreenAtStartup ();
		
		OpenAssetStoreURLAfterLaunch();
	}
}

void Application::OpenAssetStoreURL(const string& url)
{
	if ( m_IsInitialized ) {
		void* arg[] = { MonoStringNew(url.c_str()) };
		CallStaticMonoMethod("AssetStoreWindow", "OpenURL", arg);
	}
	else 
		m_URLToOpenAfterLaunch=url;

}

void Application::OpenAssetStoreURLAfterLaunch()
{
	Assert ( m_IsInitialized );
	if (! m_URLToOpenAfterLaunch.empty() )
	{
		OpenAssetStoreURL(m_URLToOpenAfterLaunch);
		m_URLToOpenAfterLaunch="";
	}
}

bool Application::IsInitializingPlayModeLayout ()
{
	return GUIView::GetStartView () != NULL;
}


void Application::AfterEverythingLoaded()
{
	// perform bumpmap fixing when all project is finished loading and main window is activated
	BumpMapSettings::Get().PerformUnmarkedBumpMapTexturesFixing();
	
	string vc = GetEditorSettings().GetExternalVersionControlSupport();
	
	std::string license;
	if (LicenseInfo::Flag (lf_maint_client))
		license = "Team";
	else
		license = "NonTeam";
	
	AnalyticsTrackEvent("VersionControl", license, vc, 1);

	AnalyticsTrackEvent("ProjectBrowser", " AssetStore", EditorPrefs::GetBool("ShowAssetStoreSearchHits") ? "Show search hits" : "Hide search hits", 1);

	LoadPlatformSupportModule(GetEditorUserBuildSettings().GetActiveBuildTarget());
}

void Application::ConnectMacroClient (int port)
{
	printf_console("Connecting macro client ...\n");
	delete m_MacroClient;
	m_MacroClient = new MacroClient(port);
	if (GetMonoManagerPtr())
		GetMonoManager().SetLogAssemblyReload(true);
}

////@TODO: MOve this somewhere else when cleaning up shutdown in general
void Application::CoreShutdown ()
{
	PluginsSetGraphicsDevice (NULL, GetGfxDevice().GetRenderer(), kGfxDeviceEventShutdown);
	
	// We want to destroy any player-created objects as there might be some that prevent mono from shutting down later on.
	DestroyWorld (true);
	CreateWorldEditor ();
	CleanupAfterLoad();

	DisableUpdating ();
	MenuController::CleanupClass();

	AssetInterface::Get ().Shutdown ();

	InputShutdown();

	LicenseInfo::Cleanup();

	CleanupAutoDocumentation();
	ShaderNameManager::StaticDestroy();
}

struct PlayerBuildInfo {
	const char          *argument;
	const char          *os_display;
	BuildTargetPlatform buildTarget;
	BuildPlayerOptions  buildOptions;
};

static PlayerBuildInfo sPlayerBuildTargets [] = {
		{ "buildOSXPlayer", "x86 os x", kBuildStandaloneOSXIntel, kBuildPlayerOptionsNone },
		{ "buildOSX64Player", "x86_64 os x", kBuildStandaloneOSXIntel64, kBuildPlayerOptionsNone },
		{ "buildOSXUniversalPlayer", "universal os x", kBuildStandaloneOSXUniversal, kBuildPlayerOptionsNone },
		{ "buildWindowsPlayer", "x86 windows", kBuildStandaloneWinPlayer, kBuildPlayerOptionsNone },
		{ "buildWindows64Player", "x86_64 windows", kBuildStandaloneWin64Player, kBuildPlayerOptionsNone },
		{ "buildLinux32Player", "x86 linux", kBuildStandaloneLinux, kBuildPlayerOptionsNone },
		{ "buildLinux64Player", "x86_64 linux", kBuildStandaloneLinux64, kBuildPlayerOptionsNone },
		{ "buildLinuxUniversalPlayer", "universal linux", kBuildStandaloneLinuxUniversal, kBuildPlayerOptionsNone },
		{ "buildWebPlayer", "web", kBuildWebPlayerLZMA, kBuildPlayerOptionsNone },
		{ "buildWebPlayerStreamed", "streamed web", kBuildWebPlayerLZMAStreamed, kBuildPlayerOptionsNone }
};

static void TryBuildPlayers ()
{
	int possiblePlayers = sizeof (sPlayerBuildTargets) / sizeof (PlayerBuildInfo);
	PlayerBuildInfo player;

	for (int i=0; i<possiblePlayers; ++i) {
		player = sPlayerBuildTargets[i];
		if (HasARGV (player.argument)) {
			printf_console ("building %s player!\n", player.os_display);
			CheckBatchModeErrorString (BuildPlayerWithSelectedLevels (GetFirstValueForARGV (player.argument), player.buildTarget, player.buildOptions));
		}
	}
}

void Application::ParseARGVCommands ()
{
	if (HasARGV ("connectToMacroClient"))
	{
		GetMonoManager().SetLogAssemblyReload(true);
	}	

	if (HasARGV ("assetServerUpdate"))
	{
		vector<string> values = GetValuesForARGV ("assetServerUpdate");
		bool status = AssetServer::Controller::Get().InitializeFromCommandLine(values);
		if (status && !AssetServer::Controller::Get().AssetServerUpdateCommandLine (values))
			CheckBatchModeErrorString ("assetServerUpdate failed");
	}
	else if (HasARGV("assetServerCommit"))
	{
		vector<string> values = GetValuesForARGV ("assetServerCommit");
		bool status = AssetServer::Controller::Get().InitializeFromCommandLine(values);
		if (status && !AssetServer::Controller::Get().AssetServerCommitCommandLine (values))
			CheckBatchModeErrorString ("assetServerCommit failed");
	}

	bool ignoreCompilerErrors = HasARGV("ignoreCompilerErrors");
	if (!ignoreCompilerErrors && IsBatchmode () && GetMonoManager().HasCompileErrors())
	{
		CheckBatchModeErrorString ("Scripts have compiler errors.");
	}
	
	if (HasARGV ("executeMethod"))
	{
		string param = GetFirstValueForARGV ("executeMethod");
		size_t index = param.rfind ('.');
		if (index != string::npos)
		{
			string className = string(param.begin(), param.begin() + index);
			string classNameAndNamespace = className;
			string methodName = string(param.begin() + index + 1, param.end());

			string nspace;
			index = className.rfind ('.');
			if (index != string::npos)
			{
				nspace = string(className.begin(), className.begin() + index);
				className = string(className.begin() + index + 1, className.end());
			}
			
			MonoClass* klass = GetMonoManager().GetMonoClass (className.c_str(), nspace.c_str());
			if (klass)
			{
				ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(klass,methodName.c_str(), ScriptingMethodRegistry::kStaticOnly);
				if (method != NULL)
				{
					MonoException* exception;
					mono_runtime_invoke_profiled (method->monoMethod, NULL, NULL, &exception);
					if (exception)
					{
						Scripting::LogException(exception, 0);
						CheckBatchModeErrorString ("executeMethod method " + param + " threw exception.");
					}
				}
				else
					CheckBatchModeErrorString (Format("executeMethod method '%s' in class '%s' could not be found.\nArgument was -executeMethod %s", methodName.c_str(), classNameAndNamespace.c_str(), param.c_str()));
			}
			else
				CheckBatchModeErrorString (Format("executeMethod class '%s' could not be found.\nArgument was -executeMethod %s", className.c_str(), param.c_str()));
		}
		else
		{
			CheckBatchModeErrorString ("executeMethod must have format ClassName.MethodName");
		}
	}

	TryBuildPlayers ();

	if (HasARGV ("buildBuiltinUnityResources"))
	{
		printf_console ("building builtin unity resources!\n");
		if (!BuildBuiltinAssetBundles ())
			CheckBatchModeErrorString("Failed building builtin resources");
	}
	if (HasARGV ("buildBuiltinOldWebResources"))
	{
		printf_console ("building builtin old web playerresources!\n");
		if (!BuildBuiltinOldWebAssetBundles ())
			CheckBatchModeErrorString("Failed building old web player resources");
	}
	if (HasARGV ("buildBuiltinPreviews"))
	{
		printf_console ("building builtin previews!\n");
		if (!GenerateBuiltinAssetPreviews ())
			CheckBatchModeErrorString("Failed building builtin previews");
	}
	
	if (HasARGV ("buildEditorUnityResources"))
	{
		printf_console ("building editor unity resources!\n");
		if (!BuildEditorAssetBundles ())
			CheckBatchModeErrorString("Failed building editor resources");
	}
	
	if (HasARGV ("buildNaClResourcesWebStream"))
	{
		printf_console ("building nacl resources web stream!\n");
		if (!BuildNaClResourcesWebStream ())
			CheckBatchModeErrorString("Failed nacl resources web stream");
	}

	if (HasARGV ("buildNaClWebPlayerResourcesWebStream"))
	{
		printf_console ("building nacl resources web stream!\n");
		if (!BuildNaClWebPlayerResourcesWebStream ())
			CheckBatchModeErrorString("Failed nacl resources web stream");
	}

	if (HasARGV ("importPackage"))
	{
		printf_console ("Import package from :%s !\n", GetFirstValueForARGV ("importPackage").c_str ());
		if (!ImportPackageNoGUI (GetFirstValueForARGV ("importPackage")))
			CheckBatchModeErrorString ("Failed importing package");
	}

	if (HasARGV ("exportPackage"))
	{
		vector<string> values = GetValuesForARGV("exportPackage");
		vector<string> exportAssetPaths;
		for (int i = 0; i < (values.size() - 1); i++)
		{
			exportAssetPaths.push_back (values[i]);
		} 
		string exportFileName = values[values.size() - 1];
		bool allAssetsFound = true;
		set<UnityGUID> cguids;
		for (int i = 0; i < exportAssetPaths.size(); i++)
		{
			string exportAssetPath = exportAssetPaths[i];
			UnityGUID folderGUID;
			bool includeLibraryAssets = false;
			
			if (exportAssetPath == "/")
			{
				exportAssetPath = "Assets";
				includeLibraryAssets = true;
			}
			
			if (!GetGUIDPersistentManager ().PathNameToGUID (exportAssetPath, &folderGUID))
			{
				allAssetsFound = false;
				CheckBatchModeErrorString (Format ("Asset not found: %s", exportAssetPath.c_str()));
			}
			else
			{
				printf_console ("Export package %s to %s\n", exportAssetPath.c_str(), exportFileName.c_str ());
				if (includeLibraryAssets)
				{
					cguids = AssetDatabase::Get().GetAllRootGUIDs();
				}		
				cguids.insert(folderGUID);
				
				AssetDatabase::Get().CollectAllChildren(folderGUID, &cguids);
			}
		}
		if (allAssetsFound)
		{
			ExportPackage( cguids, exportFileName );
		}
	}
	
	// Package can also be imported by doubleclicking on it
	if( HasARGV("openfile") )
	{
		std::string pathName = GetFirstValueForARGV ("openfile");
		if (StrICmp(GetPathNameExtension (pathName), kUnityPackageExtension) == 0)
		{
			if (!ImportPackageNoGUI (pathName))
				CheckBatchModeErrorString ("Failed importing package");
		}
	}
	
	// Assetstore urls can be passed in on the command line
	if( HasARGV("openurl") )
	{
		std::string url = GetFirstValueForARGV ("openurl");
		if ( IsAssetStoreURL( url ) )
		{
			GetApplication().OpenAssetStoreURL( url );
		}
	}

	if (HasARGV ("quit"))
	{
		printf_console ("Batchmode quit successfully invoked - shutting down!\n");

		CoreShutdown();

		#if UNITY_WIN
		CloseHandle(m_LockFile);
		#endif
		DeleteFileOrDirectory(kTempFolder);

		printf_console ("Exiting batchmode successfully now!\n");
		
		
		ExitDontLaunchBugReporter (0);
	}
}

void Application::ReadLicenseInfo ()
{
	SyncBuildSettingsLicensingAndActivePlatform ();
	
	SetAllowPlugins (LicenseInfo::Flag(lf_pro_version));
	m_LicenseInfoText = LicenseInfo::Get()->GetLicenseString();
}

void Application::GetLicenseFlags( vector<int>& outFlags )
{
	 outFlags.clear();
	 UInt64 flags = LicenseInfo::Get()->GetRawFlags();
	 int i=1;
	 while (flags > 0) 
	 {
		if ( (flags & 1) > 0 )
			outFlags.push_back(i);
		i++;
		flags >>= 1;
	 }
	 
}

void Application::EnterSerialNumber ()
{
	LicenseInfo::Get()->Reauthorize();
}

//
//
//
void Application::ReturnLicense ()
{
    LicenseInfo::Get()->ReturnLicense();
}

bool Application::MayUpdate ()
{
	return m_DisallowUpdating == 0;
}

void Application::HandleOpenSceneChangeOnDisk()
{
	if (DisplayDialog ("The open scene has been modified externally", Format("The open scene '%s' has been changed on disk.\nDo you want to reload the scene?", GetCurrentScene().c_str()), "Reload", "Ignore"))
		OpenScene (GetCurrentScene());

	m_OpenSceneChangedOnDisk = false;
}

//#define ENABLE_DEEP_PROFILER

// Code to enable function profiler. 
// Requires FunctionProfiler.dll to be present next to the executable
void DeepProfilerHeartbeat(){
#ifdef ENABLE_DEEP_PROFILER
	typedef void (*LPHEARTBEAT)();

	static HINSTANCE hDLL = NULL;
	static LPHEARTBEAT lpHeartBeat = NULL;
	if( !hDLL )
		hDLL = LoadLibrary("FunctionProfiler.dll");

	if( hDLL && !lpHeartBeat )
		lpHeartBeat = (LPHEARTBEAT)GetProcAddress(hDLL, "HeartBeat");

	if( lpHeartBeat )
		lpHeartBeat();
#endif
}


void Application::TickTimer()
{
	//if (m_IsInitialized)
	//	LicenseInfo::Get()->Tick();
	
	GetAuxWindowManager().Update ();

	if( m_DisallowUpdating )
		return;
	if( m_InsideTickTimer )
		return;
	m_InsideTickTimer = true;

	VCProvider* vcProvider = GetVCProviderPtr();
	if (vcProvider)
		vcProvider->Tick();

	DeepProfilerHeartbeat();
	if (!GetSceneTracker().IsLocked())
	{
		#if ENABLE_PROFILER		
		UnityProfiler::RecordPreviousFrame(kProfilerEditor);
		#endif

		// Recreate GfxDevice if requested.
		// Must be done between ending last profiler frame and starting the next (case 563311).
		if (m_RecreateGfxDevice)
		{
			RecreateGfxDevice();
			m_RecreateGfxDevice = false;
		}

		if (m_RecreateSkinnedMeshResources)
		{
			RecreateSkinnedMeshResources();
			m_RecreateSkinnedMeshResources = false;
		}

		#if ENABLE_PROFILER		
		UnityProfiler::StartNewFrame(kProfilerEditor);
		#endif

//		static Ticker shouldUpdateScene (.01F);
		static Ticker shouldUpdateConsoleLog (.2F);
		static Ticker shouldUpdateProfiler (0.5F);
		#if WEBVIEW_IMPL_WEBKIT
		static Ticker shouldUpdateJSCore (0.5F);
		#endif
		static Ticker shouldUpdateInspectorMonoBehaviours (0.15F);
		static Ticker shouldUpdateInspectorMonoBehavioursBackground (0.15F);
		static Ticker shouldUpdateMonoCompiler (.2F);
		static Ticker shouldUpdatePackageImport (.2F);
		static Ticker shouldUpdateTooltip(0.05F);
		static Ticker hierarchyWindowTick (.15F);
		static Ticker asyncProgressBarTick (.15F);
		static Ticker shouldCheckCurlRequest (.1F);
		static Ticker shouldHandlePlayerUpdates (.05F);
		static Ticker shouldPingMacroClient (0.5F);
		
		bool active = IsApplicationActive() || GetPlayerRunInBackground () || GetPlayerPause() != kPlayerPaused;

		if (m_OpenSceneChangedOnDisk)
		{
			HandleOpenSceneChangeOnDisk();
		}

		// This is used to make AS Mono GUI not crash after update/commit
		AssetServer::Controller::Get().Tick();

		// Perform delayed asset database/interface actions
		AssetInterface::Get().Tick();

		if (!IsCompiling() && m_ProjectToLoadOnNextTick.size() != 0)
		{
			DoOpenProject(m_ProjectToLoadOnNextTick, string(), m_ProjectToLoadArgs);
		}

		if (shouldUpdateMonoCompiler.Tick())
		{
			bool performReload = false;
			if (IsCompiling())
			{
				// Poll compilers here, and reload assemblies when done compiling
				if (m_DisallowReloadAssemblies <= 0)
					performReload = UpdateMonoCompileTasks();
				
				m_RequestScriptReload = false;
			}
			else if (m_RequestScriptReload && m_DisallowReloadAssemblies <= 0)
			{
				m_RequestScriptReload = false;
				printf_console ("Reloading assemblies due to reload request.\n");
				ReloadAllUsedAssemblies();
			}
			
			// Enter play mode delayed when compilation stops
			if (!IsCompiling() && m_CompilationDelayPlaymode && !IsWorldPlaying())
			{
				SetIsPlaying (true);
				
				// Entering playmode implicitly reloads the domain already. No need to reload it again.
				if (IsWorldPlaying())
					performReload = false;
			}
			
			if (performReload)
			{
				printf_console ("Reloading assemblies after finishing script compilation.\n");
				ReloadAllUsedAssemblies();
			}
		}

		if (m_RepaintAllViews && !IsCompiling() && m_DisallowReloadAssemblies <= 0)
		{	
			m_RepaintAllViews = false;
			if (m_SwitchSkin)
			{	
				CallStaticMonoMethod("EditorApplication", "Internal_SwitchSkin");
				printf_console ("Reloading assemblies due to switching skins.\n");
				ReloadAllUsedAssemblies();
			}
			
			CallStaticMonoMethod("EditorApplication", "Internal_RepaintAllViews");			
			m_SwitchSkin = false;
		}
		
		#if WEBVIEW_IMPL_WEBKIT
		if (shouldUpdateJSCore.Tick())
		{
			WebViewScriptingCheck();
		}
		#endif
		if (shouldCheckCurlRequest.Tick())
		{
			CurlRequestCheck();
		}
		
		if (asyncProgressBarTick.Tick())
		{
			AsyncProgressBar::InvokeTickerFunctionIfActive();
		}

#if ENABLE_PROFILER
		if(active || EditorConnection::Get().IsConnected())
		{
			if (shouldUpdateProfiler.Tick())
			{
				// this only repaints using profiler history, not currently recorded UnityProfiler data
				ScriptingInvocation(MONO_COMMON.repaintAllProfilerWindows).Invoke();
			}
			if (shouldHandlePlayerUpdates.Tick())
			{
				GeneralConnection::MulticastInfo multiCastInfo = EditorConnection::Get().PollWithCustomMessage();
				if (multiCastInfo.HasCustomMessage() && multiCastInfo.GetIdentifier() == "ConnectMacroClient")
					ConnectMacroClient (multiCastInfo.GetPort());
			}
		}
#endif

		if (active)
		{
			GetSceneTracker().Update();

			if (shouldUpdateConsoleLog.Tick())
				TickConsoleAndStatusBar ();
			
			if (shouldUpdatePackageImport.Tick())
				TickPackageImport ();

			if (hierarchyWindowTick.Tick())
				GetSceneTracker().TickHierarchyWindowHasChanged();

			if (shouldUpdateInspectorMonoBehaviours.Tick())
			{
				GetSceneTracker().TickInspector();
				CallStaticMonoMethod("EditorApplication", "Internal_CallDelayFunctions");
			}
			
			#if ENABLE_SUBSTANCE
			GetSubstanceSystem().Update();
			#endif

			if (shouldUpdateTooltip.Tick())
				GetTooltipManager().Update();
			
			if (!UpdateSceneIfNeeded())
				GUIView::RepaintAll(false);

		#if ENABLE_MOVIES
			//For Movie Preview
			if(!IsWorldPlaying())
				BaseVideoTexture::UpdateVideoTextures();
		#endif
		}

		if (shouldUpdateInspectorMonoBehavioursBackground.Tick())
		{
			GetSceneTracker().TickInspectorBackground();
		}
		
		// make sure compile indicator updates in background
		if (!active)
		{
			if (shouldUpdateConsoleLog.Tick())
				TickConsoleAndStatusBar ();
			
			GUIView::RepaintAll(false);
		}
		
		// Poll macro client for instructing Unity to do something from outside
		if (m_MacroClient)
			m_MacroClient->Poll();
		
		
		///@TODO: Temporary test for debugging test suite flipflopping...
		if (shouldPingMacroClient.Tick() && m_MacroClient)
		{
			printf_console("MacroClient: Unity editor is still alive... %f\n", (float)GetTimeSinceStartup());
		}
		
		// Enter / Exit playmode delayed because it reloads assemblies
		if (m_SetIsPlayingDelayed != -1)
		{
			bool isPlaying = m_SetIsPlayingDelayed;
			m_SetIsPlayingDelayed = -1;
			SetIsPlaying(isPlaying);
		}

		// Delay any ui layout changes to happen after UpdateScene and PlayerLoop
		ChangeLayoutIfNeeded ();
		
		/// Currently Unity 
		if (!GetPreloadManager().IsLoading())
			ForceCloseAllOpenFileCaches ();
		
		#if !UNITY_RELEASE
		{
			enum { kCheckTickCount = 100 };

			if( ++m_CurTickCheckAwakeFromLoad == kCheckTickCount )
			{
				CheckCorrectAwakeUsage();
				m_CurTickCheckAwakeFromLoad = 0;
			}
		}
		#endif

		if(!IsWorldPlaying())
			EditorAssetGarbageCollectManager::Get()->GarbageCollectIfHighMemoryUsage();

		GetPropertyDiffUndoRecorder().Flush();

	}
	m_InsideTickTimer = false;
}

void Application::ChangeLayoutIfNeeded ()
{
	if (m_ChangeLayout)
	{
		m_ChangeLayout = false;

		if (IsWorldPlaying())
		{
			CallStaticMonoMethod("EditorApplicationLayout", m_IsPaused ? "SetPausemodeLayout" : "SetPlaymodeLayout");
		}
	}
}

void Application::LockReloadAssemblies ()
{
	if (m_DisallowReloadAssemblies < 0)
		m_DisallowReloadAssemblies = 0;

	m_DisallowReloadAssemblies++;
		
//	LogString("Lock" + IntToString(m_DisallowReloadAssemblies));
}

void Application::UnlockReloadAssemblies ()
{
	m_DisallowReloadAssemblies--;

//	LogString("Unlock" + IntToString(m_DisallowReloadAssemblies));
}

void Application::ResetReloadAssemblies ()
{
	m_DisallowReloadAssemblies = -1;
//	LogString("ResetLock" + IntToString(m_DisallowReloadAssemblies));
}


void Application::SaveSceneInternal (string path, TransferInstructionFlags options, std::map<LocalIdentifierInFileType, SInt32>*fileIDToHeapID)
{
	GlobalCallbacks::Get().willSaveScene.Invoke();

	// Make sure that any delayed changes (eg. prefab recording are applied)
	GetSceneTracker().FlushDirty();
	
	int didSceneChange = m_DidSceneChange;

	if (!::SaveScene (path, fileIDToHeapID, options))
	{
		ErrorString (Format("Failed to save scene %s.", path.c_str()));
	}

	GetSceneTracker().FlushDirty();
	
	m_DidSceneChange = didSceneChange;
	UpdateDocumentEdited ();
}

void Application::FileMenuSaveAs ()
{
	m_DidCancelSave = true;
	if (IsWorldPlaying ())
		return UnityBeep ();
	
	string path = RunComplexSavePanel ("Save Scene", "", "", PathToAbsolutePath("Assets"), "", "unity", &SavePanelIsValidFileName, &SavePanelShouldShowFileName, NULL);
	
	if (path.empty())
		return;
	
	m_DidCancelSave = false;
	
	string relativePath = GetProjectRelativePath (path);
	AssertIf (relativePath.empty ());

	SaveScene(relativePath, true);
}

void Application::FileMenuSave (bool explicitlySaveScene)
{
	m_DidCancelSave = true;
	if (IsWorldPlaying ()) {
		CallStaticMonoMethod ("SceneView", "ShowSceneViewPlayModeSaveWarning");
		return UnityBeep ();
	}
	
	// Before saving with last pathname check if its empty and if the pathname can be accessed
	if (m_OpenScene != UnityGUID () && IsDirectoryCreated (DeleteLastPathNameComponent(GetCurrentScene ())))
	{
		SaveScene(GetCurrentScene (), explicitlySaveScene);

		m_DidCancelSave = false;
	}
	else
	{
		FileMenuSaveAs ();
	}
}

bool Application::EnsureSceneHasBeenSaved(const std::string& operation)
{
	if (GetApplication().GetCurrentScene() != "")
		return true;
	
	if (!DisplayDialog("Scene needs saving", Format("You need to save the scene before baking %s.", operation.c_str()), "Save Scene", "Cancel"))
		return false;
	
	GetApplication().FileMenuSave();
	
	return !GetApplication().GetCurrentScene().empty();
}

bool Application::SaveCurrentSceneIfUserWantsTo ()
{
	GetSceneTracker().FlushDirty();
	if (m_DidSceneChange == 0)
		return true;
	
	return SaveCurrentSceneIfUserWantsToForce ();
}

bool Application::SaveCurrentSceneIfUserWantsToForce ()
{
	GetSceneTracker().FlushDirty();
	
	int returnValue;
	
	if (EditorPrefs::GetBool("VerifySavingAssets", false) && m_OpenScene != UnityGUID () && IsDirectoryCreated (DeleteLastPathNameComponent(GetCurrentScene ())))
		returnValue = 0;
	else
	{
		string title = Format("Do you want to save the changes you made in the scene %s?", GetDisplayOpenPath().c_str());
		returnValue = DisplayDialogComplex (title, "Your changes will be lost if you don't save them", "Save", "Don't Save", "Cancel");
	}
	
	// Save
	if (returnValue == 0)
	{
		SetIsPlaying(false);
		FileMenuSave();
		return !m_DidCancelSave;
	}
	// Dont save
	else if (returnValue == 1)
		return true;
		
	// Cancel
	else
		return false;
}

bool Application::SaveCurrentSceneDontAsk ()
{
	GetSceneTracker().FlushDirty();
	SetIsPlaying(false);
	FileMenuSave();
	return !m_DidCancelSave;
}

void Application::LoadSceneInternal (const string& pathname, int options, std::map<LocalIdentifierInFileType, SInt32>* fileIDToHeapIDHint)
{
	AssertIf (pathname.empty ());

	int oldActiveObject = Selection::GetActiveID();
	set<int> oldSelection = Selection::GetSelectionID();

	Selection::SetActiveID(0);
	GetSceneTracker().FlushDirty();
	
	// Load the world, if any datatemplate merges took place set scene dirty
	LoadSceneEditor (pathname, fileIDToHeapIDHint, options);

	GetSceneTracker().FlushDirty();
	
	// Flush any object that has been marked for deletion
	GetCleanupManager ().Flush ();
	

	// Load old selection
	Selection::SetActiveID (oldActiveObject);
	Selection::SetSelectionID (oldSelection);
	GetSceneTracker().FlushDirty();
}

bool Application::OpenScene (string pathName)
{	
	SetIsPlaying(false);
	ConvertSeparatorsToUnity(pathName);

	if (IsFileCreated (pathName) && IsAbsoluteFilePath(pathName))
	{
		string relativePath = GetProjectRelativePath (pathName);
		if (relativePath.empty ())
		{
			string project;
			string scene;
			if (ExtractProjectAndScenePath (pathName, &project, &scene))
			{
				GetSceneTracker().FlushDirty();

				string msg = Format ("The scene you want to open is in another project\n%s\nDo you want to open the scene in that project?", project.c_str ());
				if (!DisplayDialog ("About to open another project!", msg, "Open", "Cancel"))
					return true;
				
				m_DidSceneChange = 0; // user already answered if he wants to save current scene, don't ask again
				DoOpenProject(project, scene, vector<string>());
			}
			else
				pathName = "";
		}
		// We are still in the same project.
		// Load the project relative path!
		else
			pathName = relativePath;
	}
	
	// Load scene
	if (IsFileCreated (pathName))
	{
		SetOpenPath (pathName);
		LoadSceneInternal (pathName, 0, NULL);
		GetSceneTracker().DidOpenScene();

		// Reset scene dirtyness
		m_DidSceneChange = 0;
		m_OpenSceneChangedOnDisk = false;
		UpdateDocumentEdited ();
		GetUndoManager().ClearAll();
		
		return true;
	}
	else
	{
		return false;
	}
	
}

bool Application::SaveSceneFromScript (string path, bool saveAsCopy)
{
	bool didSave = false;
	if (path == "")
	{
		// Before saving with last pathname check if its empty and if the pathname can be accessed
		m_DidCancelSave = true;
		if (m_OpenScene != UnityGUID () && IsDirectoryCreated (DeleteLastPathNameComponent (GetCurrentScene ())))
		{
			didSave = SaveScene (GetCurrentScene ());
			m_DidCancelSave = false;
		}
		else
		{
			FileMenuSaveAs ();
		}
		didSave = !m_DidCancelSave;
	}
	else
	{
		UnityGUID oldOpenScene = m_OpenScene;
		bool didSceneChangeOld = m_DidSceneChange;
		
		didSave = SaveScene (path);
		
		// If saving as copy, current open scene and scene changed state
		// should be the same as before saving.
		if (saveAsCopy)
		{
			m_OpenScene = oldOpenScene;
			UpdateMainWindowTitle ();
			m_DidSceneChange = didSceneChangeOld;
			UpdateDocumentEdited ();
		}
	}
	return didSave;
}

bool Application::SaveScene (string path, bool explicitlySaveScene)
{
	ConvertSeparatorsToUnity(path);
	
	UnityGUID guid;
	GetGUIDPersistentManager().PathNameToGUID (path, &guid);

	bool doSave = true;
	bool doRevert = false;
	// If we are editing an existing scene, and the user did not explicitly save that, 
	// add the scene to the list of files to confirm saving of.
	if (guid.IsValid())
	{
		set<UnityGUID> dirtyAssets;
		AssetInterface::Get ().GetDirtyAssets (dirtyAssets);
		
		dirtyAssets.insert(guid);
		
		set<UnityGUID> saveAssets;
		set<UnityGUID> revertAssets;
		AssetModificationCallbacks::ShouldSaveAssets (dirtyAssets, saveAssets, revertAssets, explicitlySaveScene);

		AssetInterface::Get ().WriteRevertAssets (saveAssets, revertAssets);
	
		if (saveAssets.find(guid) == saveAssets.end())
			doSave = false;
		if (revertAssets.find(guid) != revertAssets.end())
			doRevert = true;
	}
	else
		AssetInterface::Get ().SaveAssets ();
	
	if (doSave)
	{
		TransferInstructionFlags options = kWarnAboutLeakedObjects;
		if (GetEditorSettings().GetSerializationMode() == EditorSettings::kForceText)
			options |= kAllowTextSerialization;
		SaveSceneInternal (path, options);
		AssetInterface::Get ().ImportAtPath (path);

		SetOpenPath (path);
		
		m_DidSceneChange = 0;
		UpdateDocumentEdited ();

		//@TODO: ALWAYS RETURNS TRUE (THIS MIGHT FAIL)
		return true;
	}
	else if (doRevert)
	{
		// revert scene from disk.
		OpenScene (GetCurrentScene());
		m_OpenSceneChangedOnDisk = false;
	}
	return false;
}

void Application::NewScene ()
{
	SetIsPlaying(false);
	SetOpenPath ("");
	
	Selection::SetActiveID(0);
	GetSceneTracker().FlushDirty();
	
	///////////////@TODO CLEANUP OR WHATEVE!!!!!!!!!!!
	
/////////////////	GetPreloadManager().LockPreloading();
	DestroyWorld (true);
	CreateWorldEditor ();
	CleanupAfterLoad();
///////////////	GetPreloadManager().UnlockPreloading();

	GameObject& camera = CreateGameObject ("Main Camera", "GUILayer", "FlareLayer", "AudioListener", NULL);
	camera.GetComponent(Camera).SetDepth(-1);
	camera.SetTag (kMainCameraTag);
#if ENABLE_SPRITES
	if (GetEditorSettings().GetDefaultBehaviorMode() == EditorSettings::kMode2D)
	{
		camera.GetComponent (Transform).SetPosition (Vector3f (0.0F, 0.0F, -10.0F));
		camera.GetComponent (Camera).SetOrthographic(true);
	}
	else
#endif
	camera.GetComponent (Transform).SetPosition (Vector3f (0.0F, 1.0F, -10.0F));

	// VERY IMPORTANT: must update before we get the repaint requests
	// (so pipelines can execute)
	GetSceneTracker().FlushDirty();

	GetSceneTracker().DidOpenScene();
	
	UpdateScene();

	GetSceneTracker().FlushDirty();
	m_DidSceneChange = 0;
	UpdateDocumentEdited ();
	GetUndoManager().ClearAll();
}

void Application::FileMenuOpen ()
{
	string path = RunComplexOpenPanel ("Load Scene", "", "", PathToAbsolutePath("Assets"), OpenPanelIsValidFileName, OpenPanelShouldShowFileName, NULL);
	if (path.empty())
		return;

	OpenFileGeneric(path);
}

bool Application::OpenFileGeneric (const string& filename)
{
	if (gApp && gApp->m_IsInitialized)
	{
		if (!OpenPanelIsValidFileName(NULL, filename))
			return false;

		if (StrICmp(GetPathNameExtension (filename), kUnityPackageExtension) == 0)
		{
			ImportPackageGUI (filename);
		}
		else
		{
			if (!GetSceneTracker().CanOpenScene())
				return false;

			if (!gApp->SaveCurrentSceneIfUserWantsTo())
				return false;
			
			gApp->OpenScene (filename);
		}
		
		return true;
	}
	else
	{
		// Application was launched by document double click. Setup project path accordingly
		return SetupProjectPathWithOpenFile (filename);
	}
}

void Application::FileMenuNewScene ()
{
	if (!GetSceneTracker().CanOpenScene())
		return;

	if (!GetApplication().SaveCurrentSceneIfUserWantsTo ())
		return;

	GetApplication().NewScene ();
}


bool Application::SetupProjectPathWithOpenFile (const string& fileName)
{
	string project, scene;
	if (IsUnitySceneFile (fileName) && ExtractProjectAndScenePath (fileName, &project, &scene))
	{
		SetProjectPath (project, false);
	
		EditorPrefs::SetString(kLastOpenedScene, scene);
		return true;
	}
	else
		return false;
}

void Application::SetOpenPath (const std::string& pathName)
{
	UnityGUID asset;
	GetGUIDPersistentManager().PathNameToGUID (GetActualPathSlow (pathName), &asset);
	m_OpenScene = asset;
	UpdateMainWindowTitle ();
}

void Application::SetAssetServerProjectName (const std::string& pathName)
{
	m_CurrentAssetServerProject = pathName;
	UpdateMainWindowTitle ();
}

void WindowTitlePostprocess (const std::set<UnityGUID>& imported, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved)
{	
	UnityGUID currentScene = GetApplication ().GetCurrentSceneGUID ();
	for (map<UnityGUID, string>::const_iterator i=moved.begin(); i!=moved.end(); i++)
		if (i->first == currentScene)
			GetApplication ().UpdateMainWindowTitle ();
}

void Application::UpdateMainWindowTitle ()
{
	string projectName = GetLastPathNameComponent(GetProjectPath());

	string title;
	if (m_CurrentAssetServerProject.empty())
		title = Format("%s - %s", GetDisplayOpenPath ().c_str(), projectName.c_str());
	else
		title = Format ("%s - %s [ %s ]", GetDisplayOpenPath ().c_str(), projectName.c_str(), m_CurrentAssetServerProject.c_str());

	string targetName = GetBuildTargetGroupDisplayName (GetBuildTargetGroup(GetEditorUserBuildSettings().GetActiveBuildTarget()));

	if (!targetName.empty())
		 title += " - " + targetName;

	string completePath;
	if (m_OpenScene != UnityGUID ())
		completePath = PathToAbsolutePath (GetCurrentScene ());

	SetMainWindowFileName (title, completePath);
}

string Application::GetDisplayOpenPath ()
{
	string title = DeletePathNameExtension (GetLastPathNameComponent (GetCurrentScene ()));
	if (title.empty ())
	{
		return "Untitled";
	}
	else
	{
		return AppendPathNameExtension (title, "unity");
	}
}

bool OpenPanelIsValidFileName (void* userData, const string& filename)
{
	string cPathName = ToLower (filename);
	return IsUnitySceneFile (cPathName) || StrICmp(GetPathNameExtension (cPathName), kUnityPackageExtension) == 0;
}

bool OpenPanelShouldShowFileName (void* userData, const string& fileName)
{
	string cPathName = ToLower (fileName);
	// Show folders that 
	// - are inside the project path
	// - lead to the project path	
	if (IsDirectoryCreated (cPathName))
	{
		string projectPath = ToLower (File::GetCurrentDirectory ());
		if (cPathName.find (projectPath) == 0)
			return true;
		else if (projectPath.find (cPathName) == 0)
			return true;
		else
			return true;
	}
	// Show files with unity extension if inside project path
	else
	{
		return IsUnitySceneFile(cPathName) || StrICmp(GetPathNameExtension (cPathName), kUnityPackageExtension) == 0;
	}
}

bool SavePanelIsValidFileName (void* userData, const string& fileName)
{
	string relativePathName = ToLower(GetProjectRelativePath (fileName));

	if (!StartsWithPath(relativePathName, "Assets"))
	{
		DisplayDialog("Save the scene in the Assets folder", "The scene needs to be saved inside the Assets folder of your project.", "Ok");
		return false;
	}

	if (!IsUnitySceneFile (relativePathName))
	{
		DisplayDialog("Use the .unity extension", "The scene needs to be saved with the .unity extension.", "Ok");
		return false;
	}
	
	if (!CheckValidFileName(GetLastPathNameComponent(relativePathName)))
	{
		DisplayDialog("Correct the file name", "Special characters and reserved names cannot be used for file names.", "Ok");
		return false;
	}
	
	return true;
}

bool SavePanelShouldShowFileName (void* userData, const string& fileName)
{
	string cPathName = ToLower (fileName);
	// Show folders that 
	// - are inside the project path
	// - lead to the project path	
	if (IsDirectoryCreated (cPathName))
	{
		string projectPath = ToLower (File::GetCurrentDirectory ());
		if (cPathName.find (projectPath) == 0)
			return true;
		else if (projectPath.find (cPathName) == 0)
			return true;
		else
			return false;
	}
	// Show files with unity extension if inside project path
	else
	{
		cPathName = GetProjectRelativePath (cPathName);
		return IsUnitySceneFile(cPathName);
	}
}

string Application::GetCurrentScene ()
{
	if (m_OpenScene == UnityGUID ())
		return "";
	return GetAssetPathFromGUID (m_OpenScene);
}

UnityGUID Application::GetCurrentSceneGUID ()
{
	return m_OpenScene;
}

void Application::SetSceneDirty ()
{
	if (m_DidSceneChange == 0)
		SetMainWindowDocumentEdited(true);
	m_DidSceneChange = true;
}

void Application::SetSceneDirty (bool dirty)
{
	if (m_DidSceneChange != dirty)
	{
		m_DidSceneChange = dirty;
		SetMainWindowDocumentEdited(m_DidSceneChange);
	}	
}

bool Application::IsSceneDirty ()
{
	return m_DidSceneChange;
}

void Application::UpdateDocumentEdited ()
{
	SetMainWindowDocumentEdited(m_DidSceneChange);
}

void Application::DisallowAutoRefresh()
{
	m_DisallowAutoRefresh++;
}

void Application::AllowAutoRefresh()
{
	m_DisallowAutoRefresh--;
	Assert (m_DisallowAutoRefresh >= 0);
}

void Application::DisableUpdating ()
{
	m_DisallowUpdating++;
}

void Application::EnableUpdating (bool forceSceneUpdate)
{
	m_DisallowUpdating--;
	AssertIf (m_DisallowUpdating < 0);

	/// This code was enabled, but it causes some strange corner cases when calling Refresh from inside the PlayerLoop
	/// which might happen in the functional tests
	/// I don't really understand why we must call repaint immediately after an asset import since this will happen anyway.
	/// Lets see if we run into any issues with this turned off.
	
	// if (m_DisallowUpdating == 0)
	//{
		/// It seems pointless to do this though since 
		// if (forceSceneUpdate)
		// {
		// 	// We always have to flush so that we can actually see if something is dirty
		// 	GetSceneTracker().FlushDirty();
		// 	if (IsUpdateNeeded ())
		// 		UpdateScene (false);
		// }
	//}
}

void Application::SaveAssets ()
{
	AssetInterface::Get ().SaveAssets ();
}

void Application::AutoRefresh ()
{
	if (m_IsInitialized)
	{
		if (EditorPrefs::GetBool(kAutoRefresh) && (m_DisallowUpdating == 0) && (m_DisallowAutoRefresh == 0))
		{
			AssetInterface::Get ().Refresh (kAllowForceSynchronousImport);
		}
		
		if (GetVCProvider().IsActive())
		{
			// Force refetch of version control state from backend when Unity gets focused
			GetVCCache().Invalidate();
		
			// Reflect new version control state in gui
			MonoException* exception = NULL;
			CallStaticMonoMethod("EditorApplication", "Internal_RepaintAllViews", NULL, &exception);
		}
	}
}

bool Application::Terminate ()
{
	m_IsInitialized = false;

	GetSceneTracker().FlushDirty();

	if (!GetSceneTracker().CanTerminate())
		return false;

	if (!SaveCurrentSceneIfUserWantsTo ())
		return false;

	// Perform Quit Analytics. Note that the forceRequest flag is set for Application Quit, we always want to try to get that so it matches the Application/Launch request
	AnalyticsTrackPageView("/Application/Quit", true);
	double time = m_LaunchTime = GetTimeSinceStartup();
	AnalyticsTrackEvent("Application", "Quit", "UpTime", RoundfToInt(time), true);
	
	// Do this as early as possible. Analytics events will block the return until they are completed.
	CurlRequestCleanup();
	
	Selection::SetActiveID(0);
	GetSceneTracker().FlushDirty();

	#if WEBVIEW_IMPL_WEBKIT
	WebViewScriptingCheck();
	#endif

	SaveDefaultWindowPreferences();

	// Save the open scene pathname
	EditorPrefs::SetString(kLastOpenedScene, GetCurrentScene ());

	CoreShutdown();

	EditorPrefs::Sync();

	#if UNITY_WIN
	CloseHandle(m_LockFile);
	#endif
	DeleteFileOrDirectory(kTempFolder);

	g_HardwareReporter.Shutdown();
	
	return true;
}

void Application::ObjectHasChanged (PPtr<Object> object)
{
	Object* o = object;
	AssertIf (o == NULL);
	
	if (o->TestHideFlag (Object::kDontSave))
		return;

	m_SceneRepaintDirty = true;
	
	// LogString("Causing dirty repaint : " + o->GetName() + " (" + o->GetClassName() + ")");

	// Mark scene dirty when object is modified that is not persistent
	if (!o->IsPersistent ())
	{
		SetSceneDirty ();
	}
}

void Application::GOWasDestroyed (GameObject* go)
{
	if (go->TestHideFlag (Object::kDontSave)) return;

	m_SceneRepaintDirty = true;

	// Mark scene dirty when object is modified that is not persistent
	if (!go->IsPersistent ())
	{
		SetSceneDirty ();
	}
}

bool Application::IsInitialized ()
{
	return gApp && gApp->m_IsInitialized;
}

float Application::CalculateDeltaTime ()
{
	if (m_LastUpdateTime < 0.0F)
		m_LastUpdateTime = GetTimeSinceStartup();
		
	double time = GetTimeSinceStartup();
	float deltaTime = time - m_LastUpdateTime;
	deltaTime = std::min (deltaTime, 0.5F);
	deltaTime = std::max(deltaTime, kMinimumDeltaTime);

	m_LastUpdateTime = time;
	return deltaTime;
}

void Application::PerformDrawHouseKeeping()
{
	if( m_DisableDrawHouseKeeping )
		return;
	
	GetTimeManager().NextFrameEditor();
	GetRenderBufferManager().GarbageCollect();
	TextMeshGenerator2::GarbageCollect();
}

bool Application::IsUpdateNeeded ()
{
	if (m_SceneRepaintDirty)
		return true;
	
	if (IsWorldPlaying())
	{
		if (!m_IsPaused)
			return true;
		else if (m_DoOneStep)
			return true;
	}
	else
	{
		bool isAnyParticleEmittersActive = ParticleSystemEffect::GetIsAnyParticleEmitterActive ();
		std::set<GameObject*> activeParticleSystems = ParticleSystemEffect::GetActiveParticleSystems ();
		if(!activeParticleSystems.empty() || isAnyParticleEmittersActive || ParticleSystemEditor::GetUpdateAll())
		{
			static Ticker particleTicker (1.0F / 60.0F);
			return particleTicker.Tick ();
		}
		else
		{
			ParticleSystemEditor::SetPlaybackIsPlaying(false);
		}
	} 
	return false;		
}

bool Application::UpdateSceneIfNeeded ()
{
	// We always have to flush so that we can actually see if something is dirty
	GetSceneTracker().FlushDirty();

	if (IsUpdateNeeded ())
	{
		if (kPlayerPausing == GetPlayerPause())
			SetPlayerPause(kPlayerPaused);
			
		// If we have a world update, render ALL views.
		UpdateScene ();
		return true;
	} 
	else
	{
		GetAudioManager().Update();
		return false;
	}
}

void Application::Step ()
{
	if (IsWorldPlaying())
	{
		SetPaused(true);
		m_DoOneStep = true;
		UpdateScene();
	}
}

void Application::UpdateScene (bool doRepaint)
{
	AssertIf (!GetApplication().MayUpdate());
	
	// Calculate Delta time
	float deltaTime = CalculateDeltaTime ();
	
	bool updateWorld = true;
	
	if (IsWorldPlaying ())
	{
		if (m_IsPaused)
		{
			GetTimeManager ().SetTimeManually (true);
			GetTimeManager ().SetTime (
				GetTimeManager ().GetCurTime () + 
				GetTimeManager ().GetFixedDeltaTime () * m_DoOneStep * GetTimeManager ().GetTimeScale () );
			// Update world only when stepping and update the first frame!
			updateWorld = m_DoOneStep || GetTimeManager().GetFrameCount () == 0;
			m_DoOneStep = false;
		}
		else
		{
			GetTimeManager ().SetTimeManually (false);
		}
	}
	else
	{
		GetTimeManager ().SetTimeManually (true);
		GetTimeManager ().NextFrameEditor ();
	}
	
	float originalDeltaTime = GetDeltaTime();

	// Reset frame rendering stats here. Gather stats for both Update and further
	// offscreen camera rendering.
	GfxDevice& device = GetGfxDevice();
	device.ResetFrameStats();
	device.BeginFrameStats();

	#pragma message ("Why do we update the world when in non-play mode? That doesn't make sense. We should only be rendering")
#if ENABLE_PROFILER
	bool profileGame = IsWorldPlaying() && updateWorld;
	if (profileGame)
		profiler_start_mode(kProfilerGame);
#endif

	if (updateWorld)
	{
		UpdateScreenManagerAndInput();
			
		PlayerLoop ();

		if (!IsWorldPlaying ())
		{
			GetTimeManager ().SetDeltaTimeHack (deltaTime);
			ParticleSystemEffect::UpdateActiveParticleSystems();
			RenderManager::UpdateAllRenderers();
		}
	}
	else
	{
		ParticleSystem::BeginUpdateAll ();
		ParticleSystem::EndUpdateAll ();
		ParticleEmitter::UpdateAllParticleSystems();
		RenderManager::UpdateAllRenderers();
	}

	if (!doRepaint)
	{
		GetTimeManager ().SetDeltaTimeHack (originalDeltaTime);
#if ENABLE_PROFILER
		if (profileGame)
			profiler_end_mode(kProfilerGame);
#endif
		return;
	}
	
	PerformDrawHouseKeeping();
	m_DisableDrawHouseKeeping = true;
	
	// Render all offscreen cameras before everything else
	{
		AutoGfxDeviceBeginEndFrame frame;
		if (frame.GetSuccess())
			GetRenderManager().RenderOffscreenCameras();
	}
	RenderTexture::SetActive(NULL); // required for RTs to be flushed correctly	
	
#if ENABLE_PROFILER
	if (profileGame)
		profiler_end_mode(kProfilerGame);
#endif

	// Now disable stats, and store what we have accumulated in this update
	device.EndFrameStats();
	device.SaveDrawStats();
	
	GUIView::RepaintAll(true);
	
	m_DisableDrawHouseKeeping = false;
	
	GetTimeManager ().SetDeltaTimeHack (originalDeltaTime);
	
	// Empty the dirty queue and clean isSceneDirty 
	// So that SetDirtys that were called from inside the renderer are not used to update
	GetSceneTracker().FlushDirty();
	m_SceneRepaintDirty = false;
}

void Application::TickConsoleAndStatusBar ()
{
	GetEditorMonoConsole().Tick();
	GetAsyncProgressBar().Tick();
}

void Application::SetIsPlaying (bool isPlaying)
{
	// Stop status hint 
	if (m_CompilationDelayPlaymode)
	{
		SetStatusHint("", -1);
		m_CompilationDelayPlaymode = false;
	}
		
	if (IsWorldPlaying() == isPlaying)
		return;

	// When compiling scripts we want to delay play mode until compilation is finished
	if (isPlaying && IsCompiling())
	{
		SetStatusHint("Waiting for script compilation to finish...", 1000.0F);
		m_CompilationDelayPlaymode = true;
		return;		
	}

#if UNITY_LOGIC_GRAPH
	if (isPlaying)
	{
		SetStatusHint("Compiling Logic Graphs...", 1000.0F);
		CompileAllGraphsInTheScene(true);
	}
#endif

	// Don't enter playmode if we have compiler errors
#if UNITY_LOGIC_GRAPH
	if (isPlaying && (GetMonoManager().HasCompileErrors() || HasGraphCompilationErrors()))
#else
	if (isPlaying && GetMonoManager().HasCompileErrors())
#endif
	{
		CallStaticMonoMethod("Toolbar", "InternalWillTogglePlaymode");
		
		CallStaticMonoMethod("SceneView", "ShowCompileErrorNotification");
		
		return;		
	}
	
	if (!GetSceneTracker().CanEnterPlaymode())
	{
		CallStaticMonoMethod("Toolbar", "InternalWillTogglePlaymode");
		return;
	}
	
	GetPreviewTextureManagerCache ().ClearAllTemporaryPreviews ();
	
	PlayerPrefs::Sync();

	// Switch to play mode
	if (isPlaying)
	{
		Assert(!IsCompiling());
		
		SaveUserEditableManagers();

		#if ENABLE_MOVIES
		BaseVideoTexture::StopVideoTextures();
		#endif

		m_DidSceneChangeBeforePlaymode = m_DidSceneChange;
		ClearPlaymodeConsoleLog();
#if UNITY_LOGIC_GRAPH
		ClearGraphCompilationErrors();
#endif
		
#if ENABLE_SPRITES
		SpritePacker::RebuildAtlasCacheIfNeeded(GetEditorUserBuildSettings().GetActiveBuildTarget(), true, SpritePacker::kSPE_Normal, true);
		Sprite::OnEnterPlaymode();
#endif

		CreateDirectory (kTempFolder);
		
		m_PlaymodeFileIDToHeapID.clear();
		SaveSceneInternal (kEditModeScenePath, kNoTransferInstructionFlags, &m_PlaymodeFileIDToHeapID);
		GetDelayedCallManager ().ClearAll ();
		
		int openLevelIndex = UpdateBuildSettingsWithSelectedLevelsForPlaymode (kEditModeScenePath);
		
		printf_console ("Reloading assemblies for play mode.\n");
		ReloadAllUsedAssemblies();
		ResetPlayerInEditor (openLevelIndex);
		PrepareQualitySettingsForPlatform(GetEditorUserBuildSettings().GetActiveBuildTarget(), true);

		// In pause mode we do not allow to lock cursor (See also SetPaused())
		GetScreenManager().SetAllowCursorLock (!m_IsPaused);

		// Pause state can change during script init (e.g by script or if 'Error pause' is turned on in the console)
		bool isPaused = m_IsPaused;

		if (!IsBatchmode ())
		{
			// Let mono-land initialize gameview layout so that screensize is valid before scripts are awaked, case 373262.
			if (!isPaused)
				CallStaticMonoMethod("EditorApplicationLayout", "InitPlaymodeLayout");
			else
				CallStaticMonoMethod("EditorApplicationLayout", "SetPausemodeLayout");
		}

		// Update screenmanager (must be after gameview init).
		UpdateScreenManagerAndInput();

		SubstanceImporter::OnEnterPlaymode();

		// Set up the default cursor... do this whenever we enter play mode
		GetPlayerSettings().InitDefaultCursors();
		
		// Load scene. We could just work with the current scene however that might be problematic 
		// for example because Rigid bodies will be out of sync.
		// Note: Script 'Awake' and 'OnEnable' are called during LoadSceneEditor() in LoadSceneInternal().
		LoadSceneInternal (kEditModeScenePath, kEditorPlayMode, &m_PlaymodeFileIDToHeapID);

		// Perform first scene update WITHOUT repaint. 
		// Note: Script 'Start' and first 'Update are called here.
		UpdateScene (false);

		if (!IsBatchmode ())
		{	
			// Finalize and render maximized view after loading (if set)
			if (!isPaused)
				CallStaticMonoMethod("EditorApplicationLayout", "FinalizePlaymodeLayout");

			// Pause state can change during script init (e.g by script or if 'Error pause' is turned on in the console)
			ChangeLayoutIfNeeded ();
		}

		// Update scene WITH repaint to warm up renderer.
		UpdateScene (true);

		PopupCompilerErrors();
		GetSceneTracker().DidPlayScene ();
	}
	// Switch to edit mode 
	else 
	{
		ResetPlayerInEditor (0);
		GetDelayedCallManager ().ClearAll ();
		NotifyPlayerQuit(true);
		ClearAllLines ();
		GetUndoManager().ClearAllPlaymodeUndo ();

		AnimationClip::RevertAllPlaymodeAnimationEvents ();
		
		LoadSceneInternal(kEditModeScenePath, 0, &m_PlaymodeFileIDToHeapID);
		
		// Cleanup asset bundles that were not properly unloaded
		vector<AssetBundle*> files;
		Object::FindObjectsOfType(&files);
		for (int i=0;i<files.size();i++)
		{
			AssetBundle* bundle = files[i];
			if (bundle && bundle != GetEditorAssetBundle())
				UnloadAssetBundle(*bundle, true);
		}

		m_PlaymodeFileIDToHeapID.clear();

		m_IsPaused = false;
		
		GetAudioManager ().SetPause (m_IsPaused);
		GetAudioManager().StopSources();
		
        SubstanceImporter::OnLeavePlaymode();

		ResetUserEditableManagers();

		m_DidSceneChange = m_DidSceneChangeBeforePlaymode;
		UpdateDocumentEdited ();
		
		// Clear log callback so that it does not stick around after exiting playmode.
		ClearLogCallback ();

		CallStaticMonoMethod("EditorApplicationLayout", "SetStopmodeLayout");

		// Update scene and repaint after SetStopmodeLayout to prevent showing one frame with sceneview
		// before returning from maximized window.
		UpdateScene (true);
	}
	Cursors::SetCursor (NULL, Vector2f::zero, kAutoHardwareCursor);
	CallStaticMonoMethod("EditorApplication", "Internal_PlaymodeStateChanged");
	
#if UNITY_PERFORMANCETEST
	if(isPlaying)
	{
		LogString("Playmode entered");
	}
	else
	{
		LogString("Playmode stopped");
	}
#endif
}

bool Application::IsPlayingOrWillEnterExitPlaymode ()
{
	if (m_SetIsPlayingDelayed != -1)
		return m_SetIsPlayingDelayed;
	if (m_CompilationDelayPlaymode)
		return true;
	return IsWorldPlaying();
}

void Application::SetIsPlayingDelayed (bool isPlaying)
{
	if (m_SetIsPlayingDelayed != (isPlaying ? 1 : 0))
	{
		m_SetIsPlayingDelayed = isPlaying;
		CallStaticMonoMethod("Toolbar", "RepaintToolbar");
		CallStaticMonoMethod("EditorApplication", "Internal_PlaymodeStateChanged");
	}
}


void Application::SetPaused (bool pause)
{
	// When exiting pause, jump timer to avoid frame rate hiccup
	if (m_IsPaused && !pause)
		GetTimeManager ().SetTime (GetTimeManager ().GetCurTime ());
	
	// Un/Pause audio manager
	GetAudioManager().SetPause (pause);
		
	m_IsPaused = pause;

	if (IsWorldPlaying())
	{
		if (m_IsPaused)
		{
			GetScreenManager().SetAllowCursorLock(false);
			GetScreenManager().SetCursorInsideWindow(false);
#if UNITY_WIN
			// Can't do it in UpdateScreenManagerAndInput, because that is not called when
			// switching to paused mode.
			void ResetEditorMouseOffset ();
			ResetEditorMouseOffset ();
#endif
		}
	}

	m_ChangeLayout = true;

	CallStaticMonoMethod("EditorApplication", "Internal_PlaymodeStateChanged");
}

void Application::RequestScriptReload ()
{
	m_RequestScriptReload = true;
}

void Application::SwitchSkinAndRepaintAllViews ()
{
	m_SwitchSkin = true;
	m_RepaintAllViews = true;
}

void Application::RequestRepaintAllViews ()
{
	m_RepaintAllViews = true;
}

void Application::RequestRecreateGfxDevice ()
{
	m_RecreateGfxDevice = true;
}

void Application::RequestRecreateSkinnedMeshResources ()
{
	m_RecreateSkinnedMeshResources = true;
}


void Application::DoOpenProject(const string& project, const string& scene, vector<string> args)
{
	if (scene != string())
	{
		args.push_back ("-openscene");
		args.push_back (scene);
	}

	args.push_back ("-projectpath");
	args.push_back (project);

	RelaunchWithArguments (args);
}

bool Application::AcquireProjectLock ()
{
	string lockFilePath = AppendPathName (kTempFolder, "UnityLockfile");

#if UNITY_WIN

	m_LockFile = CreateFileA (lockFilePath.c_str(), FILE_GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, NULL, NULL);
	return (m_LockFile != INVALID_HANDLE_VALUE);

#else

	m_LockFile.Open (lockFilePath.c_str(), File::kAppendPermission);
	return m_LockFile.Lock (File::kExclusive, false);

#endif
}

void Application::ReleaseProjectLock ()
{
#if UNITY_WIN
	if (m_LockFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle (m_LockFile);
		m_LockFile = INVALID_HANDLE_VALUE;
	}
#else
	m_LockFile.Close ();
#endif
}

#if !UNITY_RELEASE
void Application::CheckCorrectAwakeUsage()
{
	dynamic_array<Object*> aliveObject(kMemTempAlloc);
	Object::FindObjectsOfType (ClassID(Object), &aliveObject);
	
	for( unsigned aliveI = 0 ; aliveI < aliveObject.size() ; ++aliveI )
		aliveObject[aliveI]->CheckCorrectAwakeUsage();
}
#endif

static void HandleTooLargeNew ()
{
	FatalErrorString ("Out of memory!");
}

bool IsAssetStoreURL( const std::string& url )
{
	return  (url.size() > 19 && url.compare(0, 19, "com.unity3d.kharma:") == 0) ;
}

void PauseEditor ()
{
	GetApplication().SetPaused (true);	
}

void FocusProjectView ()
{
	if (!ExecuteCommandOnKeyWindow ("FocusProjectWindow"))
	{
		ExecuteCommandOnAllWindows ("FocusProjectWindow");
	}
}

void ShowAboutDialog()
{
	CallStaticMonoMethod("AboutWindow", "ShowAboutWindow");
}

void ShowPreferencesDialog()
{
	CallStaticMonoMethod("PreferencesWindow", "ShowPreferencesWindow");
}

static bool ValidAutoUndoInput (const InputEvent& event)
{
	return event.type == InputEvent::kMouseDown || (event.type == InputEvent::kKeyDown && !IsEditingTextField());
}

static bool IsHotControlActive ()
{
	return GetEternalGUIState()->m_HotControl != 0;
}

void HandleAutomaticUndoGrouping (const InputEvent& event)
{
	// Increment event index for events that must be counted as a new, separate undo event.
	// Events with the same index are handled by the undo system as a single undo.
	if (ValidAutoUndoInput (event))
	{
		// While dragging etc we do not increment undo group to ensure actions done while dragging is counted as one undo group
		if (!IsHotControlActive())
		{
			GetUndoManager().IncrementCurrentGroup ();
		}
	}
}

// All gui controls (in C#) that keeps state that needs to be reset if hotcontrol is lost needs to handle the
// 'KeyCode.Escape' key down event and reset their state there! This could be tools like SceneView motion 
// or dragging of EditorWindows etc. 
// If the control just uses hotControl without storing additional state then it does not need to handle 
// the Escape event (Like buttons, sliders, toggles etc.)
void HandleAutomaticUndoOnEscapeKey (const InputEvent& event, bool eventWasUsed)
{
	if (ValidAutoUndoInput (event))
	{
		// Only auto revert current undo group on Esc if hotcontrol is activie (e.g is dragging) and the event was not handled by gui code
		if (IsHotControlActive() && !eventWasUsed &&  event.type == InputEvent::kKeyDown && event.keycode == SDLK_ESCAPE)
		{
			GetEternalGUIState()->m_HotControl = 0;
			GetUndoManager().RevertAllInCurrentGroup ();
		}
	}
}
