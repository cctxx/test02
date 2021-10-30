#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Player.h"
#include "QualitySettings.h"
#include "PreloadManager.h"
#if SUPPORT_REPRODUCE_LOG
#include "ReproductionLog.h"
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#endif
#include "SaveAndLoadHelper.h"
#include "Runtime/BaseClasses/ManagerContextLoading.h"
#include "SceneUnloading.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "PreloadManager.h"
#include "Runtime/Input/InputManager.h"
#include "PlayerSettings.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Serialize/PathNamePersistentManager.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Camera/UnityScene.h"
#include "GameObjectUtility.h"
#include "Runtime/IMGUI/TextMeshGenerator2.h"
#include "Runtime/Utilities/UserAuthorizationManager.h"
#if ENABLE_UNITYGUI
#include "Runtime/IMGUI/GUIManager.h"
#include "Runtime/IMGUI/GUIStyle.h"
#endif
#if ENABLE_WWW
#include "PlatformDependent/CommonWebPlugin/UnityWebStream.h"
#include "PlatformDependent/CommonWebPlugin/CompressedFileStreamMemory.h"
#include "Runtime/Utilities/ReportHardware.h"
#endif
#if WEBPLUG
#include "PlatformDependent/CommonWebPlugin/WebScripting.h"
#include "PlatformDependent/CommonWebPlugin/CompressedFileStreamMemory.h"
#endif
#if UNITY_EDITOR
#include "Editor/Src/Application.h"
#endif // UNITY_EDITOR
#include "Runtime/Graphics/ScreenManager.h"
#include "BuildSettings.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/GameCode/CallDelayed.h"
#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Camera/LODGroupManager.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "DebugUtility.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "Player.h"
#include "CaptureScreenshot.h"
#include "Runtime/Input/GetInput.h"
#include "ResourceManager.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Camera/RenderLayers/GUITexture.h"
#include "Runtime/Profiler/ProfilerHistory.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/GPUProfiler.h"
#include "Runtime/Profiler/MemoryProfilerStats.h"
#include "Runtime/Profiler/ProfilerConnection.h"
#include "Runtime/Profiler/MemoryProfiler.h"
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/BaseClasses/Cursor.h"
#include "Runtime/Allocator/MemoryManager.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"


#if ENABLE_CACHING
#include "CachingManager.h"
#endif
#include "Runtime/Utilities/RecursionLimit.h"
#if ENABLE_MOVIES || ENABLE_WEBCAM
#include "Runtime/Video/BaseVideoTexture.h"
#endif
#include "Runtime/IMGUI/TextMeshGenerator2.h"
#include "Runtime/Camera/IntermediateRenderer.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h" // ParticleSystem::UpdateAll ()
#include "Runtime/Filters/Particles/ParticleEmitter.h"
#include "InputEvent.h"
#include "SystemInfo.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/SubstanceSystem.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Profiler/DeprecatedFrameStatsProfiler.cpp" // hack until jamplus is finished
#include "Runtime/Graphics/DrawSplashScreenAndWatermarks.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"
#include "Runtime/Core/Callbacks/PlayerLoopCallbacks.h"

#if UNITY_WII
#include "PlatformDependent/Wii/WiiMoviePlayer.h"
#include "PlatformDependent/Wii/WiiLoadingScreen.h"
#elif UNITY_XENON
#include "PlatformDependent/Xbox360/Source/Kinect/Kinect.h"
#elif UNITY_WIN
#include "PlatformDependent/Win/WinUtils.h"
#elif UNITY_ANDROID
#include "PlatformDependent/AndroidPlayer/EntryPoint.h"
#elif UNITY_IPHONE
#include "PlatformDependent/iPhonePlayer/iphoneNativeEvents.h"
#endif

#if UNITY_IPHONE || UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
#include "PlatformDependent/CommonWebPlugin/WebScripting.h"
#endif

#if UNITY_IPHONE || UNITY_ANDROID || UNITY_BB10 || UNITY_WP8 || UNITY_TIZEN
#include "Runtime/Input/OnScreenKeyboard.h"
#endif

#if ENABLE_CLUSTER_SYNC
#include "Runtime/Interfaces/IClusterRenderer.h"
#endif

#include <ctype.h>
#include "Runtime/Interfaces/IAudio.h"


static bool PlayerInitEngineLoadData ();
static bool LoadQueuedWebPlayerData ();
static int gPluginVersion=0;
static bool s_InsidePlayerLoop = false;

void (*g_PresentCallback)(bool before);

using namespace std;

static const char* kMainDataSharedAssets = "sharedassets0.assets";
static const char* kExtraResourcesPath = "Resources/unity_builtin_extra";

#if UNITY_WIN && WEBPLUG
	static const char* kDefaultResourcePath = "Data/unity default resources";
#elif UNITY_WIN || UNITY_XENON || UNITY_PS3
	static const char* kDefaultResourcePath = "Resources/unity default resources";
#elif UNITY_OSX
	static const char* kDefaultResourcePath = "Resources/unity default resources";
#elif UNITY_WII
	static const char* kDefaultResourcePath = "Resources/unity default resources";
#elif UNITY_IPHONE
	static const char* kDefaultResourcePath = "Data/unity default resources";
#elif UNITY_ANDROID
	static const char* kDefaultResourcePath = "Data/unity default resources";
#elif UNITY_PEPPER
	static const char* kDefaultResourcePath = "Resources/unity default resources";
#elif UNITY_BB10
	static const char* kDefaultResourcePath = "unity default resources";
#elif UNITY_TIZEN
	static const char* kDefaultResourcePath = "unity default resources";
#elif UNITY_LINUX
	static const char* kDefaultResourcePath = "Resources/unity default resources";
#elif UNITY_FLASH
	static const char* kDefaultResourcePath = "Resources/unity default resources";
#elif UNITY_WEBGL
	static const char* kDefaultResourcePath = "Resources/unity_default_resources";
#else
	static const char* kDefaultResourcePath = "Resources/unity default resources";
	#error "Unknown platform"
#endif

#if WEBPLUG
#if UNITY_WIN
static const char* kDefaultOldResourcePath = "Data/unity_web_old";
#else
static const char* kDefaultOldResourcePath = "Resources/unity_web_old";
#endif
#endif // #if WEBPLUG

#if ENABLE_HARDWARE_INFO_REPORTER
static HardwareInfoReporter s_HardwareInfoReport;
#endif

#if ENABLE_GAMECENTER
namespace GameCenter
{
	void ExecuteGameCenterCallbacks();
}
#endif

#if ENABLE_MEMORY_TRACKING
std::auto_ptr<ScopedMemLeakDetector> m_MemLeakDetector = new ScopedMemLeakDetector();
#endif

struct LevelLoading
{
	int m_ActiveLoadedLevel;

	typedef dynamic_array<PPtr<Object> > DontDestroyOnLoadSet;
	DontDestroyOnLoadSet m_DontDestroyOnLoad;

	public:

	void LoadLevel (int index, const std::string& path, AwakeFromLoadQueue& awakeFromLoadQueue);

	LevelLoading () { ResetLoadLevel (-1); }

	void ResetLoadLevel (int activeLoadedLevel);

	int GetLoadedLevelIndex () { return m_ActiveLoadedLevel; }

	void DontDestroyOnLoad (Object& object)
	{
		m_DontDestroyOnLoad.push_back (&object);
	}

	bool m_hasLateBoundLevel;
	UnityStr m_lateBoundLevelName;
	UnityStr m_tempLateBoundLevelName;
};


void LevelLoading::ResetLoadLevel (int activeLoadedLevel)
{
	m_DontDestroyOnLoad.clear();
	m_ActiveLoadedLevel = activeLoadedLevel;
	m_hasLateBoundLevel = false;
	m_lateBoundLevelName.clear();
	m_tempLateBoundLevelName.clear();
}

static LevelLoading gLoadLevel;

bool GetHasLateBoundLevelFromAssetBundle (const string& name)
{
	string levelPath = Format("BuildPlayer-%s", name.c_str());
	bool exists = GetPersistentManager().HasMemoryOrCachedSerializedFile(levelPath);
	return exists;
}

bool GetLevelAndAssetPath (const std::string& levelName, int levelIndex, std::string* outLevelPath, std::string* outAssetPath, int* outIndex)
{
	*outLevelPath = "";
	*outAssetPath = "";
	*outIndex = -1;

	string levelPath;
	if (levelIndex != -1)
	{
		levelPath = GetBuildSettings().GetLevelPathName (levelIndex);
	}
	else
	{
		// Late bound level (via asset bundle)
		if (GetHasLateBoundLevelFromAssetBundle(levelName))
		{
			*outLevelPath = Format("BuildPlayer-%s", levelName.c_str());
			*outAssetPath = Format("BuildPlayer-%s.sharedAssets", levelName.c_str());
			*outIndex = -1;

			gLoadLevel.m_tempLateBoundLevelName = levelName;

			return true;
		}
		// Level included via BuildSettings
		else
		{
			levelIndex = GetBuildSettings().GetLevelIndex(levelName);
			levelPath = GetBuildSettings().GetLevelPathName (levelIndex);
		}
	}

	const char* cantLoadLevelErrorMessage = "Level '%s' (%d) couldn't be loaded because it has not been added to the build settings.\nTo add a level to the build settings use the menu File->Build Settings...";
	// Verify that the level is accessable
	#if WEBPLUG
	const char* cantStreamLoadLevelErrorMessage = "Level '%s' (%d) couldn't be loaded because it has not been streamed in yet. You need to ensure that levels which have not been streamed in yet are not loaded.";
	if (CompressedFileStream::Get().Find(levelPath) == NULL && !IsFileCreated (levelPath))
	{
		if (levelPath.empty ())
		{
			ErrorString (Format (cantLoadLevelErrorMessage, levelName.c_str(), levelIndex));
		}
		else
		{
			ErrorString (Format (cantStreamLoadLevelErrorMessage, levelName.c_str(), levelIndex));
		}
		return false;
	}
	#else
	if (!IsFileCreated (levelPath))
	{
		ErrorString (Format (cantLoadLevelErrorMessage, levelName.c_str(), levelIndex));
		return false;
	}
	#endif

	// Extract
	*outLevelPath = levelPath;
	*outIndex = levelIndex;
	#if UNITY_EDITOR
	*outAssetPath = "";
	#else
	*outAssetPath = Format("sharedassets%d.assets", levelIndex);
	#endif

	return true;
}

int GetPluginVersion()
{
	return gPluginVersion;
}

static int gTargetFrameRate = -1;

static PlayerPause gPlayerPause = kPlayerRunning;

#if !UNITY_EDITOR
static bool gIsFirstFrame = true;
static bool gHasFrameToPresent = false;
#endif

#if WEBPLUG
static UnityWebStream* gLoadUnityWebData = NULL;
#endif

#if WEBPLUG || UNITY_EDITOR
void ResetPlayerInWebPlayer (int level)
{
	gLoadLevel.ResetLoadLevel(level);
	if (GetLODGroupManagerPtr())
		GetLODGroupManager().ResetLODBias();


	StopPreloadManager();

	GetUserAuthorizationManager ().Reset();

	#if WEBPLUG
	gDisplayFullscreenEscapeTimeout = -1000.0;
	gTargetFrameRate = -1;
	AssertIf(gLoadUnityWebData != NULL);
	#endif
}
#endif

#if UNITY_EDITOR
void ResetPlayerInEditor (int level)
{
	ResetPlayerInWebPlayer(level);

	IAudio* audioModule = GetIAudio();
	if (audioModule)
		audioModule->AudioManagerAwakeFromLoad(kDefaultAwakeFromLoad);
	
	#if ENABLE_CACHING
	GetCachingManager().Reset();
	#endif
	StopPreloadManager();
}
#endif

void DontDestroyOnLoad (Object& object)
{
	gLoadLevel.DontDestroyOnLoad(object);
}

///// @TODO CLEANUP THIS SHIT
void PlayerLoadLevelFromThread (int levelIndex, const std::string& name, AwakeFromLoadQueue& loadQueue)
{
	gLoadLevel.LoadLevel(levelIndex, name, loadQueue);
}

bool IsLoadingLevel ()
{
	return GetPreloadManager().IsLoadingOrQueued();
}

void SetPlayerPause (PlayerPause pause)
{
	if (gPlayerPause == pause)
		return;
	if ((kPlayerPaused == gPlayerPause) && (kPlayerPausing == pause))
		return;

	// Pausing is sometimes called when unity is not yet initialized
	// Happens on windows gles20 emulator. It seems like the WM_ACTIVATE function can be called
	// Prior to the context having been created.
	if (GetBuildSettingsPtr() == NULL)
		return;

#if ENABLE_AUDIO
	bool pauseAudio = ( pause != kPlayerRunning );
#if UNITY_EDITOR
	 pauseAudio = pauseAudio || GetApplication().IsPaused();
#endif // UNITY_EDITOR

	IAudio* audioModule = GetIAudio();
	if (audioModule)
	 	audioModule->SetPause( pauseAudio );
#endif // ENABLE_AUDIO

	GetTimeManager().SetPause( pause == kPlayerPaused );
	if (kPlayerPaused == pause)
	{
		GetScreenManager().SetCursorInsideWindow(false);
		GetScreenManager().SetAllowCursorLock(false);
	}

	if (kPlayerRunning == pause)
		ResetInputAfterPause();

	gPlayerPause = pause;

	if ((kPlayerRunning == pause) || (kPlayerPaused == pause))
	{
		MessageData data;
		data.SetData((kPlayerPaused == pause), ClassID (bool));
		SendMessageToEveryone (kPlayerPause, data);
	}
}

PlayerPause GetPlayerPause(void)
{
	return gPlayerPause;
}

void SetPlayerFocus(bool focus)
{
	if (GetBuildSettingsPtr() == NULL)
		return;

#if !WEBPLUG
	// Reset input state if focus is being lost as we don't want such state if we resume focus.
	if (!focus)
		ResetInput();
#endif

	static bool focusEventSupported = (GetBuildSettings().GetIntVersion() > GetNumericVersion("2.6.1f3"));

	if (focusEventSupported)
	{
		MessageData data;
		data.SetData(focus, ClassID(bool));
		SendMessageToEveryone(kPlayerFocus, data);
	}
}

bool NotifyPlayerQuit(bool forceQuit)
{
	// We unloaded the data file in the web player already. Dont use any managers at this point
	if (GetManagerPtrFromContext (ManagerContext::kPlayerSettings) == NULL)
		return true;

	GetInputManager().QuitApplication();

	SendMessageToEveryone (kPlayerQuit, MessageData());

	// During SendMessageToEveryone (kPlayerQuit, MessageData()); scripts can set m_ShouldQuit to false.
	// This cancels the quit operation
	Assert(forceQuit || (!WEBPLUG && !UNITY_EDITOR && !UNITY_IPHONE && !UNITY_WINRT));
	if (!forceQuit && !GetInputManager().ShouldQuit())
		return false;

	#if ENABLE_MOVIES || ENABLE_WEBCAM
	IAudio* audioModule = GetIAudio();
	if (audioModule)
		audioModule->StopVideoTextures ();
	#endif

	#if WEBPLUG
	// This will unfortunately kill all external calls issued from OnApplicationQuit.
	// Not doing this would make them execute on the next player launch though.
	WebScripting::Get().ClearExternalCalls();
	#endif

	#if ENABLE_NETWORK
	CALL_MANAGER_IF_EXISTS(ManagerContext::kNetworkManager, NetworkOnApplicationQuit());
	CALL_MANAGER_IF_EXISTS(ManagerContext::kMasterServerInterface, NetworkOnApplicationQuit());
	#endif

	// When exiting player. Reset show cursor
	GetScreenManager().SetShowCursor(true);
	GetScreenManager().SetCursorInsideWindow(false);
	GetScreenManager().SetLockCursor(false);
	GetScreenManager().SetAllowCursorLock(false);

	// Stop any tasks on the preload manager
	StopPreloadManager();

	#if WEBPLUG
	if (gLoadUnityWebData)
	{
		gLoadUnityWebData->Release();
		gLoadUnityWebData = NULL;
	}
	#endif

	return true;
}

void ProcessMouseInWindow()
{
	// In Core Animation plugin, rely on MouseEntered/MouseExited events instead.
	// Otherwise we hide the cursor when our window is not in front, as we cannot find out when that is the case.
	#if WEBPLUG && UNITY_OSX && !UNITY_PEPPER
	if (GetScreenManager().IsUsingCoreAnimation())
		return;
	#endif

	#if UNITY_WIN && !UNITY_WINRT

	bool inside = false;
	POINT cursorPosition;

	const BOOL getCursorPositionResult = GetCursorPos(&cursorPosition);
	//Assert(FALSE != getCursorPositionResult);	// happens when windows is locked

	if (FALSE != getCursorPositionResult)
	{
		const HWND window = WindowFromPoint(cursorPosition);

		if (NULL != window)
		{
			ScreenManagerPlatform& screen = GetScreenManager();
			if (screen.GetWindow() == window)
			{
				POINT pt = cursorPosition;
				ScreenToClient (window, &pt);
				inside = (pt.x >= 0 && pt.y >= 0 && pt.x < screen.GetWidth() && pt.y < screen.GetHeight());
			}
		}
	}

	#else

	bool inside = true;
	Vector2f pos = GetInputManager().GetMousePosition();
	if (pos.x < 0.0F || pos.x > GetScreenManager().GetWidth())
		inside = false;
	if (pos.y < 0.0F || pos.y > GetScreenManager().GetHeight())
		inside = false;

	#endif

	GetScreenManager().SetCursorInsideWindow(inside);
}

bool GetPlayerRunInBackground()
{
	if(GetPlayerSettingsPtr() == NULL)
		return false;
	#if SUPPORT_REPRODUCE_LOG
	return GetPlayerSettings().runInBackground || RunningReproduction();
	#endif
	return GetPlayerSettings().runInBackground;
}

void SetPlayerRunInBackground(bool runInBackground)
{
	if (runInBackground != GetPlayerSettings().runInBackground)
	{
		GetPlayerSettings().runInBackground = runInBackground;
		GetPlayerSettings().SetDirty();

		if (runInBackground)
			SetPlayerPause(kPlayerRunning);
	}
}

#if !WEBPLUG && !UNITY_EDITOR
static string CalculateStandaloneDataFolder ()
{
	#if UNITY_OSX

	// OS X:
	// - Try AppPath/Contents/Data folder
	// - Try AppPath/../Data folder
	string appContentsFolder = AppendPathName (GetApplicationPath (), "Contents/Data");
	string appDataFolder = AppendPathName (GetApplicationFolder (), "Data");
	if (IsFileCreated (AppendPathName (appContentsFolder, kMainData)))
		return appContentsFolder;
	else if (IsFileCreated (AppendPathName (appDataFolder, kMainData)))
		return appDataFolder;

	#elif UNITY_WINRT

	return AppendPathName(GetApplicationFolder(), "Data");

	#elif UNITY_WIN || UNITY_LINUX

	// - Try ExeName_Data
	// - Try Data folder
	string appPath = GetApplicationPath();
	appPath = DeletePathNameExtension(appPath);
	string path = appPath + "_Data";
	if (IsFileCreated (AppendPathName (path, kMainData)))
		return path;
	string appDataFolder = AppendPathName (GetApplicationFolder (), "Data");
	if (IsFileCreated (AppendPathName (appDataFolder, kMainData)))
		return appDataFolder;

	#elif UNITY_XENON

		return "game:\\Media";

	#elif UNITY_PS3

		return AppendPathName (GetApplicationFolder (), "Media");

	#elif UNITY_IPHONE

		return AppendPathName(GetApplicationPath(), "Data");
	#elif UNITY_ANDROID
		return AppendPathName(GetApplicationPath(), "assets/bin/Data");
	#elif UNITY_BB10
		return AppendPathName(GetApplicationFolder(), "app/native/Data");
	#elif UNITY_TIZEN
		return AppendPathName(GetApplicationPath(), "data");
	#elif UNITY_WII
		return "unity/Data";
	#elif UNITY_FLASH || UNITY_WEBGL
		return "";
	#else
	#error "Unknown platform"
	#endif

	return "";
}

string SelectDataFolder ()
{
	static bool s_DataFolderDone = false;
	static StaticString s_DataFolder;
	if (!s_DataFolderDone)
	{
		s_DataFolder = CalculateStandaloneDataFolder ().c_str ();
		s_DataFolderDone = true;
	}
	return s_DataFolder.c_str ();
}
#endif

#if UNITY_STANDALONE
string GetApplicationNativeLibsPath()
{
#	if UNITY_OSX
		return AppendPathName (GetApplicationPath (), "Contents/Frameworks/MonoEmbedRuntime/osx");
#	else
		string monoBaseFolder = AppendPathName (SelectDataFolder (), "Mono");
		string x86_64Folder = AppendPathName (monoBaseFolder, "x86_64");
		string x86Folder = AppendPathName (monoBaseFolder, "x86");

		if (IsDirectoryCreated (x86_64Folder))
			return x86_64Folder;
		if (IsDirectoryCreated (x86Folder))
			return x86Folder;
		return monoBaseFolder;
#	endif
}
#endif

#if UNITY_EDITOR
string GetApplicationNativeLibsPath()
{
#	if UNITY_OSX
		return AppendPathName (GetApplicationPath (), "Contents/Frameworks/Mono/lib");
#	else
		return AppendPathName (GetApplicationContentsPath (), "Mono/lib");
#	endif
}
#endif

static void AddPathRemapsForBuiltinResources (const string& applicationContentsFolderPath)
{
	GetPersistentManager ().SetPathRemap ("library/unity default resources", AppendPathName (applicationContentsFolderPath, kDefaultResourcePath));

#if !WEBPLUG
	// Remap "resources/unity_builtin_extra" to "Resources/unity_builtin_extra" to deal
	// with case-sensitive file systems.
	//
	// In the webplayer, the file is contained in the webstream rather than being bundled
	// with the player, so no remapping necessary there.
	std::string extraResourcesLower = kExtraResourcesPath;
	ToLowerInplace(extraResourcesLower);
	GetPersistentManager ().SetPathRemap (extraResourcesLower, kExtraResourcesPath);
#endif

#if WEBPLUG
	GetPersistentManager ().SetPathRemap ("library/unity_web_old", AppendPathName (applicationContentsFolderPath, kDefaultOldResourcePath));
#endif
}

bool PlayerInitEngineNoGraphics (const string& dataFolder, const string& applicationContentsFolderPath)
{
/*
#if ENABLE_THREAD_CHECK_IN_ALLOCS
	g_ForwardFrameAllocator.SetThreadIDs (Thread::GetCurrentThreadID (), Thread::GetCurrentThreadID ());
#endif
*/
	/// Beta running out!
	#ifdef TIME_LIMIT
	if (CheckDate (TIME_LIMIT, "Unity Player beta has run out!"))
	{
		printf_console ("Unity Player beta has run out!\n");
		return false;
	}
	#endif

	#if SUPPORT_REPRODUCE_LOG
	BatchInitializeReproductionLog();
	#endif

	if (!IsFileCreated (AppendPathName (dataFolder, kMainData)))
	{
		printf_console ("No mainData file was found, quitting player!\n");
		return false;
	}

	InitPathNamePersistentManager();

	File::SetCurrentDirectory (dataFolder);

	AddPathRemapsForBuiltinResources (applicationContentsFolderPath);

	// Initialize Engine
	if (!InitializeEngineNoGraphics ())
	{
		printf_console( "PlayerInitEngineNoGraphics: InitializeEngine failed\n" );
		return false;
	}

	string error = PlayerLoadSettingsAndInput(kMainData);
	if (!error.empty())
	{
		#if UNITY_WIN
		winutils::AddErrorMessage( error.c_str() );
		#endif
		printf_console( "PlayerInitEngineNoGraphics settings: %s\n", error.c_str() );
		return false;
	}

	return true;
}

bool PlayerInitEngineGraphics (bool batchmode)
{
	// Initialize Engine (eg. Mastercontext, Shaderlab, Setup RTTI, Call Static Initialize functions, Setup messages and Tags)
	if (!InitializeEngineGraphics (batchmode))
	{
		#if UNITY_WIN
		winutils::AddErrorMessage( "InitializeEngineGraphics failed" );
		#endif
		printf_console( "PlayerInitEngineGraphics: InitializeEngineGraphics failed\n" );
		return false;
	}

	// Check if GPU is supported
	std::string gpuMsg = gGraphicsCaps.CheckGPUSupported();
	if (!gpuMsg.empty())
	{
		#if UNITY_WIN
		winutils::AddErrorMessage (gpuMsg.c_str());
		#endif
		printf_console ("PlayerInitEngineGraphics: GPU not supported; %s\n", gpuMsg.c_str());
		return false;
	}

	#if ENABLE_HARDWARE_INFO_REPORTER
	s_HardwareInfoReport.ReportHardwareInfo();
	#endif

	// Load all game managers! All global game managers are coming first in the main data
	// (Global game managers have to be loaded before selecting the screen resolution.
	// ProjectSettings i used by screen selector and RenderManager, InputManager by screen switching)
	string error = PlayerLoadGlobalManagers(kMainData);
	if (!error.empty())
	{
		printf_console( "PlayerInitEngineGraphics: %s\n", error.c_str() );
		return false;
	}

#if !UNITY_EDITOR
	// We can't do this in the editor, because this will get the default assets.
	// When the editor is running, the default assets are often being built, and that
	// will cause exceptions.
	
	// By Ian's request moving this and : IAN FIXME YESTERDAY.
	GUIStyle::SetDefaultFont(NULL);
#endif

	return true;
}

bool PlayerInitEngineWebNoGraphics (const string& applicationContentsFolderPath, int pluginversion)
{


	#if SUPPORT_REPRODUCE_LOG && UNITY_OSX
	BatchInitializeReproductionLog();
	#endif
	gPluginVersion = pluginversion;

	InitPathNamePersistentManager();

#if !UNITY_FLASH
	File::SetCurrentDirectory (applicationContentsFolderPath);
#endif

	AddPathRemapsForBuiltinResources (applicationContentsFolderPath);

	// Initialize base classes (things that are not dependent on graphics)
	if (!InitializeEngineNoGraphics ())
	{
		printf_console( "PlayerInitEngineNoGraphics: InitializeEngine failed\n" );
		return false;
	}
	return true;
}

bool PlayerInitEngineWebGraphics ()
{
	// Initialize ShaderLab, default resources etc. (things that are dependent on graphics)
	if (!InitializeEngineGraphics (false))
	{
		printf_console( "PlayerInitEngineGraphics: InitializeEngineGraphics failed\n" );
		return false;
	}

	// Check if GPU is supported
	std::string gpuMsg = gGraphicsCaps.CheckGPUSupported();
	if (!gpuMsg.empty())
	{
		printf_console ("PlayerInitEngineGraphics: GPU not supported; %s\n", gpuMsg.c_str());
		return false;
	}

	return true;
}


bool PlayerInitEngineWeb (const string& applicationContentsFolderPath, int pluginversion)
{
	if (!PlayerInitEngineWebNoGraphics(applicationContentsFolderPath, pluginversion))
		return false;
	if (!PlayerInitEngineWebGraphics())
		return false;
	return true;
}


static bool PlayerLoadNewSettings ()
{
	ASSERT_RUNNING_ON_MAIN_THREAD

	std::string error = PlayerLoadSettingsAndInput(kMainData);
	if (!error.empty())
	{
		printf_console( "PlayerInitEngineReload settings: %s\n", error.c_str() );
		return false;
	}

	// Print version number that our content was built with.
	printf_console( "Loading webdata version: %s\n", GetBuildSettings().GetVersion().c_str() );

	return true;
}
#if WEBPLUG

static bool PlayerInitEngineLoadData ()
{
	#if UNITY_WIN
	// Windows web player only calls PlayerInitEngineWebNoGraphics at initialization time;
	// graphisc part is deferred until here, when we have player settings loaded (to be
	// able to choose between DX9 & DX11).
	if (!PlayerInitEngineWebGraphics ())
	{
		printf_console("web: failed to initialize graphics\n");
		return false;
	}

	#endif


	#if ENABLE_HARDWARE_INFO_REPORTER
	// In web player, report hardware info after loading BuildSettings
	s_HardwareInfoReport.ReportHardwareInfo();
	#endif


	// Load all game managers! All global game managers are coming first in the main data
	// (Global game managers have to be loaded before selecting the screen resolution.
	// ProjectSettings is used by screen selector and RenderManager, InputManager by screen switching)
	std::string error = PlayerLoadGlobalManagers(kMainData);
	if (!error.empty())
	{
		printf_console( "PlayerInitEngineReload: %s\n", error.c_str() );
		return false;
	}

	// Some graphics caps need to be adjusted based on content build version,
	// which is only available now.
	gGraphicsCaps.InitWithBuildVersion();

	return true;
}

bool PlayerResetPreferences(UnityWebStream& stream, const std::string absoluteURL, bool clearPlayerPrefs /*= true*/)
{
	ResetPlayerInWebPlayer(0);
	gPlayerPause = kPlayerRunning;
	if (clearPlayerPrefs)
		PlayerPrefs::Init(absoluteURL);

	#if SUPPORT_REPRODUCE_LOG
	if (RunningReproduction())
		PlayerPrefs::DeleteAll();
	#endif

	FileStream::SetDataFile(stream.GetFileStream());
	stream.SetAddStreamedFilesToPersistentManager(true);

	return PlayerLoadNewSettings();
}

bool PlayerLoadEngineData (UnityWebStream& stream, const std::string srcValue, const std::string absoluteURL)
{
	// Load data
	if (!PlayerInitEngineLoadData())
	{
		return false;
	}

	GetPlayerSettings().srcValue = srcValue;
	GetPlayerSettings().absoluteURL = absoluteURL;
	GetUserAuthorizationManager ().Reset();
	#if SUPPORT_REPRODUCE_LOG
	ReadWriteAbsoluteUrl(GetPlayerSettings().srcValue, GetPlayerSettings().absoluteURL);

	// Extract dll's to disk when creating reproduction log
	if (GetReproduceMode() == kGenerateReproduceLog || GetReproduceMode() == kGenerateReproduceLogAndRemapWWW || GetReproduceMode() == kPlaybackReproduceLog)
	{
		AssertIf(stream.GetFileStream()->GetType() != kCompressedFileStreamMemoryType);

		for (CompressedFileStreamMemory::iterator i=stream.GetFileStream()->begin();i != stream.GetFileStream()->end();i++)
		{
			if (!StrICmp(GetPathNameExtension(i->name), "dll"))
			{
				CompressedFileStream::Data& data = *i;

				string path = AppendPathName(GetReproductionDirectory(), data.name);

				MemoryCacherReadBlocks cacher (data.blocks, data.end, kCacheBlockSize);
				UInt8* dataCpy = (UInt8*)UNITY_MALLOC(kMemTempAlloc, data.GetSize()*sizeof(UInt8));
				cacher.DirectRead(dataCpy, data.offset, data.GetSize());
				WriteBytesToFile(dataCpy, data.GetSize(), path);
				UNITY_FREE(kMemTempAlloc, dataCpy);
			}
		}
	}

	#endif

	IAudio* audioModule = GetIAudio();
	if (audioModule)
		audioModule->SetPause(kPlayerRunning);

	return true;
}

bool PlayerLoadWebData (UnityWebStream& data, const std::string srcValue, const std::string absoluteURL, bool clearPlayerPrefs /*= true*/)
{
	return PlayerResetPreferences(data, absoluteURL, clearPlayerPrefs) &&
		PlayerLoadEngineData(data, srcValue, absoluteURL);
}


bool QueuePlayerLoadWebData (UnityWebStream* stream)
{
	if (stream != NULL)
	{
		gLoadUnityWebData = stream;
		gLoadUnityWebData->Retain();
		return true;
	}
	else
	{
		ErrorString("No Valid Unity Web Stream was found in the download.");
		return false;
	}
}

static bool LoadQueuedWebPlayerData ()
{
	if (gLoadUnityWebData)
	{
		UnityWebStream* stream = gLoadUnityWebData;
		gLoadUnityWebData = NULL;
		if (!stream->IsReadyToPlay())
		{
			stream->Release();
			return false;
		}

		string srcValue = GetPlayerSettings().srcValue;
		string absoluteURL = GetPlayerSettings().absoluteURL;

		if (!PlayerWebUnloadReloadable ())
		{
			stream->Release();
			return false;
		}

		SetUnityWebStream(stream);

		if (!PlayerLoadWebData (*stream, srcValue, absoluteURL, false))
		{
			return false;
		}

		PlayerLoadFirstLevel();

		GetScreenManager().ReapplyWindowRect();
	}

	return true;
}

bool PlayerWebUnloadReloadable ()
{
	PlayerPrefs::Sync();
	

	IAudio* audioModule = GetIAudio();
	if (audioModule)
	{
       // Pause audio (Handle the case where no data file has been loaded yet)
       if (GetManagerPtrFromContext (ManagerContext::kPlayerSettings))
			audioModule->SetPause ( true );
	}

	gPlayerPause = kPlayerPaused;///WHY????

	NotifyPlayerQuit (true);

	ResetInput();

	StopPreloadManager();

	CleanupAllObjects(true);

	#if ENABLE_MONO && !UNITY_PEPPER
	if (!CleanupMonoReloadable())
		return false;
	#endif

	GetPersistentManager().UnloadMemoryStreams();

	GetUnityWebStream ().Release();

	#if ENABLE_HARDWARE_INFO_REPORTER
	GetCachingManager ().Reset();
	s_HardwareInfoReport.Shutdown();
	#endif

	SetUnityWebStream(NULL);
	CompressedFileStream::SetDataFile(NULL);

	return true;
}

#endif


void PlayerLoadFirstLevel ()
{
#if UNITY_WII
	wii::StartLoadingScreen();
#elif UNITY_ANDROID
	StartActivityIndicator();
#endif

	gLoadLevel.ResetLoadLevel(0);

	/////@TODO: Clear preloadmanager queue
	// Create an empty set of level managers

#if DEBUGMODE	
	// (GlobalGameManagers are already loaded at this point)
	for (int i=0;i<ManagerContext::kGlobalManagerCount;i++)
	{
		if (GetManagerPtrFromContext(i) == NULL)
			printf_console ("GlobalManager '%s' is not included.\n", GetManagerContext().m_ManagerNames[i]);
	}
#		endif

	AwakeFromLoadQueue emptyQueue (kMemTempAlloc);
	LoadManagers(emptyQueue);

	// Load level async
	PreloadLevelOperation* op = PreloadLevelOperation::LoadLevel (kMainData, kMainDataSharedAssets, 0, PreloadLevelOperation::kLoadMainData, true);
	GetPreloadManager().WaitForAllAsyncOperationsToComplete();
	op->Release();

	GetTimeManager().ResetTime ();

#if UNITY_WII
	wii::EndLoadingScreen();
#elif UNITY_ANDROID
	StopActivityIndicator();
#endif
	//?@TODO MAKE WORK
}

void PlayerInitState(void)
{
	MessageData pauseData;
	pauseData.SetData((kPlayerPaused == gPlayerPause), ClassID(bool));
	SendMessageToEveryone(kPlayerPause, pauseData);

	SetPlayerFocus(GetScreenManager().GetIsFocused());

#if ENABLE_CLUSTER_SYNC
	if(GetIClusterRenderer())
		GetIClusterRenderer()->InitCluster();
#endif
}

// Terminate player app.
bool PlayerCleanup(bool cleanupEverything, bool forceQuit)
{
	gPlayerPause = kPlayerPaused;///WHY????

	if (!NotifyPlayerQuit (forceQuit))
	{
		Assert(!forceQuit);
		gPlayerPause = kPlayerRunning;
		return false;
	}

#if ENABLE_CLUSTER_SYNC
	if(GetIClusterRenderer())
		GetIClusterRenderer()->ShutdownCluster();
#endif

	ReleasePreloadManager();

	PlayerPrefs::Sync();

#if ENABLE_PROFILER
	UnityProfiler::CleanupGfx();
#endif

	InputShutdown();

	if (cleanupEverything)
	{
		CleanupEngine();
		#if ENABLE_MONO
		CleanupMono();
		#endif
	}

	CleanupPersistentManager();

	#if ENABLE_PROFILER

	#if UNITY_EDITOR
	ProfilerHistory::Cleanup();
	#endif

	ProfilerConnection::Cleanup();
	UnityProfiler::Cleanup();
	#endif

	#if ENABLE_PLAYERCONNECTION && !UNITY_EDITOR
	// Send last bits (required for automated tests)
	PlayerConnection::Get().SendMessage(ANY_PLAYERCONNECTION, GeneralConnection::kApplicationQuitMessage, NULL, 0);
	PlayerConnection::Get().WaitForFinish();
	PlayerConnection::Get().DisconnectAll();
	PlayerConnection::Cleanup();
	#endif

	#if ENABLE_HARDWARE_INFO_REPORTER
	s_HardwareInfoReport.Shutdown();
	#endif

	return true;
}

bool g_RuntimeInitialized = false;
void RuntimeInitialize()
{
	if(g_RuntimeInitialized)
		return;
#if SUPPORT_THREADS
	Thread::mainThreadId = Thread::GetCurrentThreadID ();
#endif
	g_RuntimeInitialized = true;
	#if ENABLE_MEMORY_MANAGER
	MemoryManager::StaticInitialize();
	#endif

	RegisterRuntimeInitializeAndCleanup::ExecuteInitializations();

#if !UNITY_ANDROID
	// atexit functions are called at process termination - _not_ when dlclose is called and the dynamic library is unloaded.
	// Also, it seems plenty of platforms call RuntimeCleanup in a controlled fashion, instead of relying on the libc cleanup...
	atexit(RuntimeCleanup);
#endif
}

void RuntimeCleanup()
{
	if(!g_RuntimeInitialized)
		return;
	g_RuntimeInitialized = false;

	RegisterRuntimeInitializeAndCleanup::ExecuteCleanup();

	File::CleanupClass();

#if ENABLE_MEMORY_MANAGER
#if !UNITY_WP8	// don't touch memory managers because managed finalizers can try to release memory after cleanup. todo: check if metro relates to this issue
	GetMemoryManager().FrameMaintenance(true);
//	size_t remainingMemory = GetMemoryManager().GetTotalAllocatedMemory() - GetMemoryManager().GetTotalProfilerMemory();
//	Assert(remainingMemory < 100*1024);
#if ENABLE_STACKS_ON_ALL_ALLOCS
	ProfilerString leaks = GetMemoryProfiler()->GetOverview();
	printf("%s",leaks.c_str());
#endif
	MemoryManager::StaticDestroy();
#endif

#endif // #if ENABLE_MEMORY_MANAGER
}

static void ResetRandomsAfterLevelLoad ()
{
	// reset all internal Random number generators after level load.
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Invoke ();
}

void LevelLoading::LoadLevel (int index, const string& path, AwakeFromLoadQueue& loadQueue)
{
#if UNITY_WII
	wii::StartLoadingScreen();
#endif
	set<SInt32> temporaryObjects;

#if UNITY_IPHONE || UNITY_ANDROID || UNITY_BB10 || UNITY_WP8 || UNITY_TIZEN
	KeyboardOnScreen::Hide();
#endif

	// Collect dont destroy on loading objects!
	DontDestroyOnLoadSet::iterator it=m_DontDestroyOnLoad.begin ();
	while (it != m_DontDestroyOnLoad.end ())
	{
		// This is a better way of doing it.
		// Object* object = Object::IDToPoint(i->GetInstanceID());
		Object* object = *it;
		if (object)
		{
			CollectPPtrs (*object, &temporaryObjects);
			++it;
		}
		else
		{
			(*it) = m_DontDestroyOnLoad.back();
			m_DontDestroyOnLoad.pop_back();
		}
	}

	// - Deactivate all game objects that are marked not to be destroyed in scene load
	///  They are deactivate with kDeactivateToggleForLevelLoad. Some components handle this,
	///  eg. audio will ignore the deactivate completely.
	// - Make them all temp so they dont get killed
	typedef set<PPtr<GameObject> > NeedActivation;
	NeedActivation needActivation;
	typedef set<PPtr<Object> > NeedsIsTemporaryReset;
	NeedsIsTemporaryReset needsIsTemporaryReset;

	for (set<SInt32>::iterator i=temporaryObjects.begin ();i != temporaryObjects.end ();i++)
	{
		Object* object = PPtr<Object> (*i);
		if (object)
		{
			#if UNITY_EDITOR
			// In the editor we never unload assets anyway. Thus we shouldn't mess with assets in that case
			bool needsDontSaveFlag = !object->TestHideFlag (Object::kDontSave) && !object->IsPersistent();
			#else
			// Only mark objects that are set to DontSave
			bool needsDontSaveFlag = !object->TestHideFlag (Object::kDontSave);
			#endif

			if (needsDontSaveFlag)
			{
				needsIsTemporaryReset.insert (object);
				// game objects don't apply the same to all components
				object->SetHideFlagsObjectOnly (object->GetHideFlags() | Object::kDontSave);
			}

			// We used to Deactivate / Activate game objects between level reloads but now we instead keep all managers that track objects always around.
			if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
			{
				// Deactivate the game!
				GameObject* go = dynamic_pptr_cast<GameObject*> (object);
				if (go && go->IsActive ())
				{
					needActivation.insert (go);
					go->Deactivate (kDeprecatedDeactivateToggleForLevelLoad);
				}
			}
		}
	}

	// Movie textures are assets thus might not get unloaded after a level load.
	// However since the playback code is on the asset, we should just unpause all videos after loading as a convenience to the user.
	// (Optimally a movie playback component would be what drives the movie playback code instead, but we are not doing that...)
	#if ENABLE_MOVIES || ENABLE_WEBCAM
	IAudio* audioModule = GetIAudio();
	if (audioModule)
		audioModule->PauseVideoTextures ();
	#endif

	// Load level
	UnloadGameScene ();

	// Reset the temporary state
	for (NeedsIsTemporaryReset::iterator i=needsIsTemporaryReset.begin ();i != needsIsTemporaryReset.end ();i++)
	{
		Object* object = *i;
		if (object)
		{
			int flags = object->GetHideFlags();
			flags &= ~Object::kDontSave;
			// Don't apply the same to all components
			object->SetHideFlagsObjectOnly (flags);
		}
	}

	m_ActiveLoadedLevel = index;
	m_hasLateBoundLevel = false;

	if (m_ActiveLoadedLevel < 0)
	{
		// If the m_ActiveLoadedLevel is -1, this means we're loading from an asset bundle.
		m_hasLateBoundLevel = true;

		m_lateBoundLevelName = m_tempLateBoundLevelName;
		m_tempLateBoundLevelName.clear();
	}

	CompletePreloadManagerLoadLevel (path, loadQueue);

	if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
	{
	// Activate the game objects that were marked not to be destroyed on scene load
	for (NeedActivation::iterator i=needActivation.begin ();i != needActivation.end ();i++)
	{
		GameObject* go = *i;
		if (go != NULL && !go->IsActive ())
		{
			go->Activate ();
		}
		else
		{
			#if UNITY_EDITOR
			ErrorString("Some objects were marked as DontDestroyOnLoad, but since their parent game objects were not marked as DontDestroyOnLoad they have been destroyed anyway.\nIf you want to keep them around, please ensure their parent is marked as DontDestroyOnLoad or make sure they are unparented before loading a new level.");
			#endif
		}
	}
	}

	// Send all active gameobjects a kLevelWasLoaded message
	MessageData msgData;
	msgData.SetData (index, ClassID (int));
	SendMessageToEveryone (kLevelWasLoaded, msgData);

	ResetRandomsAfterLevelLoad ();
	GetTimeManager ().DidFinishLoadingLevel ();

#if UNITY_WII
	wii::EndLoadingScreen();
#endif
}

int PlayerGetLoadedLevel ()
{
	return gLoadLevel.GetLoadedLevelIndex();
}

string PlayerGetLoadedLevelName ()
{
	int loadedLevel = PlayerGetLoadedLevel ();
	// If loadedLevel is invalid, try to check the late bound level.
	if (loadedLevel >= 0)
		return GetBuildSettings ().GetLevelName(loadedLevel);
	else if (gLoadLevel.m_hasLateBoundLevel)
		return gLoadLevel.m_lateBoundLevelName;

	return string ();
}

int PlayerGetLevelCount ()
{
	return GetBuildSettings ().levels.size ();
}

void PlayerSendFrameComplete()
{
	GetDelayedCallManager ().Update (DelayedCallManager::kEndOfFrame);
}

void PlayerTakeScreenShots()
{
// for now only on iphone we have explicit msaa resolve so we have a place where we can get proper screenshot
// TODO: proper define?
#if GAMERELEASE && CAPTURE_SCREENSHOT_AVAILABLE && !UNITY_IPHONE
	UpdateCaptureScreenshot ();
	#if SUPPORT_REPRODUCE_LOG
		CaptureScreenshotReproduction(false);
	#endif
#endif
}

PROFILER_INFORMATION(gRemoteInputProfile, "ProcessRemoteInput", kProfilerOther);
static void ProcessRemoteInput ()
{
	PROFILER_AUTO(gRemoteInputProfile, NULL);


#if SUPPORT_IPHONE_REMOTE && UNITY_EDITOR
	void RemoteInputUpdate();
	RemoteInputUpdate();
#endif

}

bool IsInsidePlayerLoop ()
{
	return s_InsidePlayerLoop;
}

PROFILER_INFORMATION(gGraphicsPresent, "Device.Present", kProfilerRender);
PROFILER_INFORMATION(gFramerateSync, "WaitForTargetFPS", kProfilerVSync);
PROFILER_INFORMATION(gSubstanceUpdate, "Substance.Update", kProfilerRender)
PROFILER_INFORMATION(gPlayerLoopPresent, "Graphics.PresentAndSync", kProfilerRender)
PROFILER_INFORMATION(gPlayerLoopBeginFrame, "Graphics.BeginFrame", kProfilerRender)
PROFILER_INFORMATION(gUpdatePreloading, "Loading.UpdatePreloading", kProfilerPlayerLoop)
PROFILER_INFORMATION(gUpdateWebStream, "Loading.UpdateWebStream", kProfilerPlayerLoop)
PROFILER_INFORMATION(gPlayerCleanupCachedData, "Cleanup Unused Cached Data", kProfilerPlayerLoop)

#if !UNITY_EDITOR

void PresentFrame()
{
	PROFILER_AUTO(gGraphicsPresent, NULL);
#if ((UNITY_LINUX && SUPPORT_X11) || UNITY_OSX) && !UNITY_EDITOR && !WEBPLUG
	GetScreenManager().SetBlitMaterial();
#endif
	GfxDevice& device = GetGfxDevice();
	device.PresentFrame();
	GPU_TIMESTAMP();
	gHasFrameToPresent = false;
}

void PresentAndSync()
{
#if GAMERELEASE
	#if UNITY_WIN && !UNITY_WINRT
	if (!IsWindowVisible(GetScreenManager().GetWindow()))
		return;
	#endif

	PROFILER_AUTO (gPlayerLoopPresent, NULL)

	if (g_PresentCallback) g_PresentCallback(true);

	PresentFrame();

	if (g_PresentCallback) g_PresentCallback(false);
#endif
	;;gFrameStats.reset();
}

void PresentBeforeUpdate(GfxDevice::PresentMode presentMode)
{
	if (presentMode != GfxDevice::kPresentBeforeUpdate)
		return;

	// NOTE: updating TimeManager after VBlank potentially leads to stable frame delta times
	// since there is no OS event pump between them in such case
	if (gHasFrameToPresent)
	{
		#if UNITY_XENON
		xenon::Kinect::Update(xenon::Kinect::kUT_BeforePresent);
		#endif
		PresentAndSync();
	}
}

void PresentAfterDraw(GfxDevice::PresentMode presentMode)
{
	switch (presentMode)
	{
	case GfxDevice::kPresentAfterDraw:
#if UNITY_XENON
		xenon::Kinect::Update(xenon::Kinect::kUT_BeforePresent);
#endif
		PresentAndSync();
		break;

	case GfxDevice::kPresentBeforeUpdate:
		// Present first frame immediately
		if (gIsFirstFrame)
			PresentAndSync();
		break;
	}
	gIsFirstFrame = false;
};

static bool PlayerBeginFrame()
{
	GfxDevice& device = GetGfxDevice();
	for (;;)
	{
		// We might already be inside a frame if something was rendered from script during update
		if (!device.IsInsideFrame())
			device.BeginFrame();

		// Check if GfxDevice is ready to render
		if (device.IsValidState())
			return true;

		// Handle Direct3D lost device
		if (!device.HandleInvalidState())
		{
			LogString ("Skipped rendering frame because GfxDevice is in invalid state (device lost)");
			return false;
		}
		// Loop back to BeginFrame() since we might be outside a frame if the device was lost
	}
}

static void PlayerEndFrame(bool present)
{
	gHasFrameToPresent = true;
	if (present)
	{
		GetGfxDevice().EndFrame();
		PresentFrame();
	}
}

void PlayerRender(bool present)
{
	if (!PlayerBeginFrame())
		return;

	GetRenderManager().RenderOffscreenCameras();
	GetRenderManager().RenderCameras();

#if ENABLE_UNITYGUI
	GetGUIManager().Repaint ();
#endif
#if UNITY_WII
	wii::DrawMovieFrame();
#endif

	DrawSplashAndWatermarks();

#if RENDER_SOFTWARE_CURSOR
	Cursors::RenderSoftwareCursor();
#endif

#if WEBPLUG
	RenderFullscreenEscapeWarning();
#endif

	PlayerEndFrame(present);
}

#endif // !UNITY_EDITOR


int GetWantedVSyncCount()
{
	// What the user has requested, no clamping to actual caps
	QualitySettings* qualitySettings = GetQualitySettingsPtr();
	if (qualitySettings)
		return qualitySettings->GetCurrent().vSyncCount;
	return 0;
}

int GetMaxSupportedVSyncCount()
{
	int vSync = gGraphicsCaps.maxVSyncInterval;
#if UNITY_WIN
	if (!GetScreenManager().IsFullScreen() &&
		GetGfxDevice().GetRenderer() == kGfxRendererD3D9)
	{
		// D3D9 does not support intervals >1 in windowed mode (case 497116)
		vSync = std::min(vSync, 1);
	}
#endif
	return vSync;
}

void PlayerUpdateTime()
{
	// Xbox is bound to VBlank and does its own delicate Kinect synchronization.
	// iOS uses its own VBlank sync
#if !UNITY_XENON && !UNITY_FLASH && !UNITY_IPHONE && !UNITY_PS3 && !UNITY_WEBGL
	{
		int vSyncWanted = GetWantedVSyncCount();
		int vSyncSupported = GetMaxSupportedVSyncCount();
		// If interval is non-zero and supported by GfxDevice, don't sync on CPU
		if (UNITY_EDITOR || vSyncWanted == 0 || vSyncWanted > vSyncSupported)
		{
			PROFILER_AUTO(gFramerateSync, NULL);
			float actualTargetFPS = GetActualTargetFrameRate(vSyncWanted);
			GetTimeManager().Sync(actualTargetFPS);
		}
	}
#endif

	// Update time, input
	GetTimeManager().Update();
}

PROFILER_INFORMATION(gOnMouseHandlers, "Monobehaviour.OnMouse_", kProfilerScripts);

bool PlayerLoop (bool batchMode, bool performRendering, IHookEvent* pHookEvt)
{
#if !UNITY_FLASH
	ReentrancyChecker checker( &s_InsidePlayerLoop );
	if (!checker.IsOK())
	{
		ErrorString ("PlayerLoop called recursively!");
		return true;
	}
#endif

	#if SUPPORT_LOG_ORDER_TRACE
	if (RunningReproduction())
	{
		LogString(Format("Frame %d", GetTimeManager().GetFrameCount()));
	}
	#endif

#if ENABLE_CLUSTER_SYNC
	if(GetIClusterRenderer())
		GetIClusterRenderer()->SynchronizeCluster();
#endif

	#if ENABLE_PLAYERCONNECTION  && !UNITY_EDITOR
	PlayerConnection::Get().Poll();
	#endif

	#if ENABLE_PROFILER && GAMERELEASE
	
	UnityProfiler::RecordPreviousFrame(kProfilerGame);
	bool profilerEnabled = UnityProfiler::StartNewFrame(kProfilerGame);
	
	if (profilerEnabled)
	{	
		GfxDevice& device = GetGfxDevice();
		device.BeginFrameStats();
	}
	
	#endif // ENABLE_PROFILER && GAMERELEASE

	GPU_TIMESTAMP(); // Initial GPU time

	#if ENABLE_SUBSTANCE
	{
		SubstanceSystem* system = GetSubstanceSystemPtr ();
		if (system != NULL)
		{
		PROFILER_AUTO(gSubstanceUpdate, NULL)
			system->Update();
		}
	}
	#endif

	;;FrameStats::Timestamp ts = 0;

	#if SUPPORT_REPRODUCE_LOG
	static bool didSetWebplayerSize = false;
	if (!didSetWebplayerSize)
	{
		didSetWebplayerSize = true;
		WriteWebplayerSize(GetScreenManager().GetWidth(), GetScreenManager().GetHeight());
	}
	#endif // SUPPORT_REPRODUCE_LOG

	if (!batchMode)
		ProcessMouseInWindow();

	// In the editor, clear intermediate renderers before loop. So that in paused state or when resizing windows,
	// we can still draw the previous ones.
	#if UNITY_EDITOR
	GetScene().ClearIntermediateRenderers();
	#endif

	ClearLines();

	GfxDevice& device = GetGfxDevice();
	GfxDevice::PresentMode presentMode = device.GetPresentMode();
	bool playmode = IsWorldPlaying();

	#if !UNITY_EDITOR
	if (!batchMode)
		PresentBeforeUpdate(presentMode);
	#endif

	// Reset frame stats after present (case 496221)
	if (presentMode == GfxDevice::kPresentBeforeUpdate)
		device.ResetFrameStats();

	#if SUPPORT_REPRODUCE_LOG
	RepeatReproductionScreenshot();
	ReadWriteReproductionTime();
	#endif
#if ENABLE_CLUSTER_SYNC
	if(!GetIClusterRenderer() || GetIClusterRenderer()->IsMasterOfCluster())
#endif
	{
		PlayerUpdateTime();
	}

	#if ENABLE_WWW
	{
		PROFILER_AUTO(gUpdateWebStream, NULL);
		UnityWebStream::UpdateAllUnityWebStreams();
	}
	#endif


	////@TODO: CLeanup code where input is processed after level loading
	/// All input should be processed prior to level loading

	// In reproduce mode make sure to handle input postprocessor prior to loading a level.
	// This fixes potential issues where scripts use input in awake from load
	#if SUPPORT_REPRODUCE_LOG
	if (RunningReproduction())
	{
		if (playmode)
			GetInputManager ().ProcessInput ();
		ReadWriteReproductionInput();
	}
	#endif

	#if SUPPORT_REPRODUCE_LOG
	if (RunningReproduction())
		GetPreloadManager().WaitForAllAsyncOperationsToComplete();
	#endif

	{
		PROFILER_AUTO(gUpdatePreloading, NULL)
		GetPreloadManager().UpdatePreloading ();
	}

	if (performRendering)
		GetScene().NotifyInvisible();

	{
		PROFILER_AUTO(gPlayerCleanupCachedData, NULL)
		GetRenderBufferManager().GarbageCollect();
		TextMeshGenerator2::GarbageCollect();
	}

	////@TODO: CLeanup code where input is processed after level loading
	/// All input should be processed prior to level loading
	#if SUPPORT_REPRODUCE_LOG
	if (!RunningReproduction())
	#endif
	{
		if (playmode)
			GetInputManager ().ProcessInput ();
	}

	ProcessRemoteInput ();

	;;ts = GetTimestamp();
	GetDelayedCallManager ().Update (DelayedCallManager::kRunStartupFrame);
	;;gFrameStats.coroutineDt += GetTimestamp() - ts;

	/* This is what fixed time stepping is doing
	float time = GetCurrentTime ();
	while (fixedTime < time)
	{
		fixedTime += fixedDeltaTime;
		UpdateFixedBehaviours ();
		UdateDynamicsManager ();
	}
	Which means:
	  - fixed timestep is always larger than dynamic timestep
	  - fixed delta time is always the same
	*/

	int stepCounter = 0;

	// Make sure collider positions update when time scale is zero, so raycasts will still work as expected.
	if (CompareApproximately (GetTimeManager().GetTimeScale(), 0.0f))
	{
		CALL_UPDATE_MODULAR(PhysicsRefreshWhenPaused)
	}

	#if UNITY_XENON
	xenon::Kinect::Update(xenon::Kinect::kUT_BeforeUserScripts);
	#endif

#if UNITY_IPHONE
	iphone::DeliverPlatformEvents();
#endif

	// Fixed framerate loop (fixed behaviours, dynamics, delayed calling)
	while (GetTimeManager ().StepFixedTime ())
	{
		// Placed here so we ensure it is also called in edit-mode (fix for case 379024: pressing stop did not properly clear fixedStepLines)
		ClearLines ();

		if (playmode)
		{
			if (stepCounter == 0)
			{
				CALL_UPDATE_MODULAR(PhysicsResetInterpolatedTransformPosition)
				CALL_UPDATE_MODULAR(Physics2DResetInterpolatedTransformPosition)
			}

			#if ENABLE_AUDIO
			IAudio* audioModule = GetIAudio();
			if (audioModule)
				audioModule->FixedUpdate ();
			#endif

			// Script.FixedUpdate
			;;ts = GetTimestamp();
			GetFixedBehaviourManager ().Update ();
			;;gFrameStats.fixedBehaviourManagerDt += (GetTimestamp() - ts);

			// Animation (Root motion)
			;;ts = GetTimestamp();
			CALL_UPDATE_MODULAR(AnimatorFixedUpdateFKMove)
			CALL_UPDATE_MODULAR(LegacyAnimationUpdate)
			;;gFrameStats.animationUpdateDt += (GetTimestamp() - ts);

			// Physics
			;;ts = GetTimestamp();
			CALL_UPDATE_MODULAR(PhysicsFixedUpdate)
			CALL_UPDATE_MODULAR(Physics2DFixedUpdate)
			;;gFrameStats.fixedPhysicsManagerDt += GetTimestamp() - ts;

			// Animation IK and write bones
			CALL_UPDATE_MODULAR(AnimatorFixedUpdateRetargetIKWrite)

			// Script Coroutines
			;;ts = GetTimestamp();
			GetDelayedCallManager ().Update (DelayedCallManager::kRunFixedFrameRate);
			;;gFrameStats.coroutineDt += GetTimestamp() - ts;
		}
		stepCounter++;
	}
	gFrameStats.fixedUpdateCount = stepCounter;

	// Dynamics, animation, behaviours
	if (playmode)
	{
		CALL_UPDATE_MODULAR(PhysicsUpdate)
		CALL_UPDATE_MODULAR(Physics2DUpdate)
	}
	bool oldTextFocus = GetInputManager().GetTextFieldInput();

#if ENABLE_UNITYGUI
	GetGUIManager().SendQueuedEvents ();
#endif

#if ENABLE_NEW_EVENT_SYSTEM
#if ENABLE_UNITYGUI
	if (playmode && !GetGUIManager().GetMouseUsed())
#else
	if (playmode)
#endif
		GetInputManager().SendInputEvents();
#endif

#if ENABLE_RETAINEDGUI
	GetGUITracker().SendInputEvents(GetGUIManager().GetQueuedEvents());
#endif

#if ENABLE_SCRIPTING && SUPPORT_MOUSE
#if ENABLE_NEW_EVENT_SYSTEM
	// Deprecated in Unity 4.1. Mouse and touch events will now be sent via C++. Look inside InputManager.cpp.
	if (playmode && !IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a1))
#else
	if (playmode)
#endif
	{
		if (GetBuildSettings().usesOnMouseEvents)
		{
			PROFILER_AUTO(gOnMouseHandlers, NULL)
			int mouseUsed = GetGUIManager().GetMouseUsed () ? 1 : 0;
			int skipRTCameras = IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1);

			ScriptingInvocation invocation(MONO_COMMON.doSendMouseEvents);
			invocation.AddInt(mouseUsed);
			invocation.AddInt(skipRTCameras);
			invocation.Invoke();
		}
	}
#endif

	CALL_UPDATE_MODULAR(NavMeshUpdate)

	;;ts = GetTimestamp();
	GetBehaviourManager ().Update ();
	;;gFrameStats.dynamicBehaviourManagerDt += GetTimestamp() - ts;

	;;ts = GetTimestamp();
	GetDelayedCallManager ().Update (DelayedCallManager::kRunDynamicFrameRate);
	;;gFrameStats.coroutineDt += GetTimestamp() - ts;

	// Dynamic Step Animation Update
	if (playmode)
	{
		;;ts = GetTimestamp();

		CALL_UPDATE_MODULAR(AnimatorUpdateFKMove);
		CALL_UPDATE_MODULAR(LegacyAnimationUpdate);
		CALL_UPDATE_MODULAR(AnimatorUpdateRetargetIKWrite);

		;;gFrameStats.animationUpdateDt += (GetTimestamp() - ts);
	}

	#if ENABLE_NETWORK
	if (playmode)
	{
		CALL_MANAGER_IF_EXISTS(ManagerContext::kNetworkManager, NetworkUpdate());
		CALL_MANAGER_IF_EXISTS(ManagerContext::kMasterServerInterface, NetworkUpdate());
	}
	#endif

	ParticleSystem::BeginUpdateAll ();

	;;ts = GetTimestamp();
	GetLateBehaviourManager ().Update ();
	;;gFrameStats.dynamicBehaviourManagerDt += GetTimestamp() - ts;

	;;ts = GetTimestamp();
	GetDelayedCallManager ().Update (DelayedCallManager::kRunDynamicFrameRate);
	;;gFrameStats.coroutineDt += GetTimestamp() - ts;

	#if ENABLE_AUDIO
	IAudio* audioModule = GetIAudio();
	if (audioModule)
	 	audioModule->Update();
	#endif

	ParticleEmitter::UpdateAllParticleSystems();

	#if ENABLE_PHYSICS
	if (playmode && performRendering && device.IsValidState())
	{
		CALL_UPDATE_MODULAR(PhysicsSkinnedClothUpdate);
	}
	#endif

	// We need to sync particle systems here to make sure they update their renderers properly
	ParticleSystem::EndUpdateAll ();

	RenderManager::UpdateAllRenderers();

	if(pHookEvt)
		pHookEvt->Execute();

	;;ts = GetTimestamp();
	if (performRendering && device.IsValidState()) // skinning touches graphics resources; only do that when device is not lost
	{
		SkinnedMeshRenderer::UpdateAllSkinnedMeshes(SkinnedMeshRenderer::kUpdateNonCloth);
	}
	;;gFrameStats.skinMeshUpdateDt += GetTimestamp() - ts;

	#if UNITY_IPHONE || UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
	WebScripting::Get().ProcessSendMessages();
	#endif
	GetUpdateManager ().Update ();

	if(pHookEvt)
		pHookEvt->Execute();

	#if ENABLE_MOVIES || ENABLE_WEBCAM
	if (performRendering && device.IsValidState()) // movies touch graphics resources; only do that when device is not lost
	{
		IAudio* audioModule = GetIAudio();
		if (audioModule)
			audioModule->UpdateVideoTextures ();
	}
	#endif


#if UNITY_WII && !WIIWARE
	void ProcessDVDErrors();
	ProcessDVDErrors();
#endif

	// In d3d ref mode only render when taking screenshots
	#if SUPPORT_REPRODUCE_LOG && GFX_SUPPORTS_D3D9 && ENABLE_FORCE_GFX_RENDERER
	if (::g_ForceD3D9RefDevice && RunningReproduction() && !GetInputManager().GetKey(SDLK_F5))
		performRendering = false;
	#endif

	#if !UNITY_EDITOR
	if (performRendering)
	{
		;;ts = GetTimestamp();

		// Perform rendering
		if (!batchMode)
			PlayerRender(false);

		// We want to send 'end-of-frame' events even if running in batch mode
		PlayerSendFrameComplete();

		PlayerTakeScreenShots();

		if (!batchMode)
		{
			device.EndFrame();
			PresentAfterDraw(presentMode);
		}

		;;gFrameStats.renderDt += GetTimestamp() - ts;
	} // End rendering

	// In the player, clear intermediate renderers after loop.
	GetScene().ClearIntermediateRenderers();

	#endif // !UNITY_EDITOR

	#if SUPPORT_REPRODUCE_LOG
	ReadWriteReproductionEnd();
	#endif

	GetScreenManager().SetRequestedResolution();

	// Clear the input string and the keydown events at the end of the Loop.
	// This makes sure all input string is cleared
	GetInputManager ().InputEndFrame ();

	// Let Fonts uncache characters if they are not used in the next frame.
	Font::FrameComplete();

	// We entered Text Field input this frame, Game mode input is disable.
	// Reset axes, so they don't stick.
	if(GetInputManager().GetTextFieldInput() && !oldTextFocus)
		GetInputManager ().ResetInputAxes ();

	#if WEBPLUG
	if (!LoadQueuedWebPlayerData())
		return false;
	#endif

	#if THREADED_LOADING_DEBUG
	static double endFrameTime = 0.0F;
	float delta = GetTimeSinceStartup() - endFrameTime;
	endFrameTime = GetTimeSinceStartup();
	printf_console("FPS %f\n", 1.0F / delta);
	#endif

	#if ENABLE_PROFILER && GAMERELEASE
	if (profilerEnabled)
	{
		device.EndFrameStats();
		device.SynchronizeStats();
	}
	#endif

	#if ENABLE_MEMORY_MANAGER
	GetMemoryManager().FrameMaintenance();
	#endif

	#if ENABLE_GAMECENTER
	GameCenter::ExecuteGameCenterCallbacks();
	#endif

	// Reset frame stats after present and UnityProfiler EndFrame() (case 494732)
	if (presentMode == GfxDevice::kPresentAfterDraw)
		device.ResetFrameStats();

	return true;
}

void LevelLoadingLoop()
{
#if UNITY_ANDROID
	// make sure input is being processed to keep those ANR dialogs at bay..
	InputProcess();
#endif
#if UNITY_METRO
	void UpdateMetroEvents();
	UpdateMetroEvents();
#endif
	Thread::Sleep(0.020f);
}


#if !UNITY_EDITOR && !WEBPLUG
bool SwitchToStandaloneDefaultSettings()
{
	// Reload quality settings (Preferences might have changed)
	GetQualitySettings().AwakeFromLoad(kDefaultAwakeFromLoad);

#if UNITY_IPHONE
	// on ios we do not use player prefs for resolution
	bool success = true;
#else
	const PlayerSettings& settings = GetPlayerSettings();
	int width = PlayerPrefs::GetInt(kResolutionWidth, settings.defaultScreenWidth);
	int height = PlayerPrefs::GetInt(kResolutionHeight, settings.defaultScreenHeight);
	bool fullscreen = PlayerPrefs::GetInt(kIsFullScreen, settings.defaultIsFullScreen);

	if (fullscreen &&
	    !(PlayerPrefs::HasKey (kResolutionWidth) && PlayerPrefs::HasKey (kResolutionHeight)) &&
	    settings.defaultIsNativeResolution) {
		width = GetScreenManager ().GetCurrentResolution ().width;
		height = GetScreenManager ().GetCurrentResolution ().height;
	}

	bool success = GetScreenManager ().SetResolutionImmediate (width, height, fullscreen, 0);
#endif
	GetScreenManager().SetShowCursor (true);
	return success;
}
#endif

#if UNITY_OSX && !UNITY_EDITOR && !UNITY_PEPPER
bool SwitchToBatchmode()
{
	// Reload quality settings (Preferences might have changed)
	GetQualitySettings().AwakeFromLoad(kDefaultAwakeFromLoad);

	bool success = GetScreenManager().SwitchToBatchmode();
	return success;
}
#endif

int GetTargetFrameRateFromScripting()
{
	return gTargetFrameRate;
}

int GetTargetFrameRate()
{
#if	SUPPORT_REPRODUCE_LOG
	if (HasNormalPlaybackSpeed() || GetReproduceMode() == kGenerateReproduceLog || GetReproduceMode() == kGenerateReproduceLogAndRemapWWW)
		return 20;
	else if (GetReproduceMode() == kPlaybackReproduceLog)
		return 1000;
#endif

	return gTargetFrameRate;
}

#if UNITY_IPHONE
	extern "C" void NotifyFramerateChange(int);
#endif

void SetTargetFrameRate(int target)
{
	gTargetFrameRate = target;
#if UNITY_FLASH
	__asm __volatile__("stage.frameRate = %0" : : "r"(target));
#endif

#if UNITY_IPHONE
	NotifyFramerateChange(gTargetFrameRate);
#endif

#if UNITY_WP8
	extern BridgeInterface::IBridge^ s_WinRTBridge;
	Assert(s_WinRTBridge);
	s_WinRTBridge->WP8Utility->MaxFrameRate = gTargetFrameRate;
#endif
}

float GetActualTargetFrameRate()
{
	return GetActualTargetFrameRate(GetWantedVSyncCount());
}

float GetActualTargetFrameRate(int vSyncCount)
{
#if UNITY_FLASH
	int fr;
	__asm __volatile__("%0 = stage.frameRate;" : "=r"(fr));
	return fr;
#endif

#if UNITY_WIN && WEBPLUG
	const int kFullscreenTargetFPS = 200;
#else
	const int kFullscreenTargetFPS = 1000;
#endif

	float framerate = GetTargetFrameRate();

	// Can be a fraction of an odd refresh rate like 59 so we can't use int
	if (vSyncCount > 0)
	{
		int refreshRate = GetScreenManager().GetCurrentResolution().refreshRate;
		framerate = float(refreshRate > 0 ? refreshRate : 60) / vSyncCount;
	}

	if( framerate <= 0 )
	{
#if WEBPLUG && !UNITY_PEPPER
		return (GetScreenManagerPtr() && GetScreenManager().IsFullScreen()) ? kFullscreenTargetFPS : 60;
#else
		return kFullscreenTargetFPS;
#endif
	}
	else
	{
		return std::min<float>(framerate,kFullscreenTargetFPS);
	}
}
