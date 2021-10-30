#ifndef _PLAYER_H
#define _PLAYER_H

#include <string>
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Misc/PlayerSettings.h"

// To avoid frozen window because of GPU workload
class IHookEvent
{
public:
	virtual void Execute() = 0;
};

class Object;
class DecompressedDataFile;
class UnityWebStream;
class AwakeFromLoadQueue;

// ** Initializing unity
// 1) InitializeMonoFromMain ("Mono dll path");
// 2) Optional: SelectDataFolder ();
// 2) PlayerInitEngine ("Data folder", "app path/Contents");
// 3) Optional: Display screen selector.
//    The engine is initialized so you can access the input manager at this point.
// 4) PlayerInitEngineGraphics ();
// 5) if (!SwitchToStandaloneDefaultSettings())
//       couldnt switch resolution
// 6) PlayerLoadFirstLevel ();
bool PlayerInitEngineNoGraphics (const std::string& dataFolder, const std::string& applicationContentsFolderPath);
bool PlayerInitEngineGraphics (bool batchmode = false);
std::string SelectDataFolder ();
void PlayerLoadFirstLevel ();
void PlayerInitState(void);

/// For reloading a unity player from the web player
bool PlayerInitEngineWeb (const std::string& applicationContentsFolderPath, int pluginversion);
bool PlayerInitEngineWebNoGraphics (const string& applicationContentsFolderPath, int pluginversion);
bool PlayerInitEngineWebGraphics ();

bool PlayerLoadWebData (UnityWebStream& data, const std::string srcValue, const std::string absoluteURL, bool resetPrefs = true);
bool PlayerResetPreferences(UnityWebStream& stream, const std::string absoluteURL, bool clearPlayerPrefs = true);
bool PlayerLoadEngineData (UnityWebStream& data, const std::string srcValue, const std::string absoluteURL);
bool PlayerWebUnloadReloadable ();

/// Force loading a new unityweb data file. Used for streaming unityweb files with the WWW class.
bool QueuePlayerLoadWebData (UnityWebStream* stream);

/// Render scene without doing anything else
void PlayerRender(bool present);

/// Running unity every frame
/// 1) Read input in platform specific way.
///    GetInput.h on os x. This sets appropriate variables inside InputManager.
/// 2) PlayerLoop ();
/// Don't call this is if the player is paused (kPlayerPaused == GetPlayerPause)
bool PlayerLoop (bool batchmode = false, bool performRendering = true, IHookEvent* pHookEvt=NULL);

/// Cleanup the player (Calls NotifyPlayerQuit)
/// 1) optional PlayerPrefs::Sync (); 
/// 2) PlayerCleanup ();
/// 3) ExitToShell();
/// Returns true if cleanup succeeded.
/// If forceQuit is true it will always cleanup. (Force quit should be enabled in the web player)
/// If cleanupEverything is true, it will also delete all objects. This should be enabled
bool PlayerCleanup(bool cleanupEverything, bool forceQuit);

/// Initialize and destroy the runtime. This has to be called as the first thing on player/engine startup
/// and as the very last thing before exiting.
/// All globals and statics should be initialized and cleaned up from these functions
void RuntimeInitialize();
void RuntimeCleanup();

struct AutoInitializeAndCleanupRuntime
{
	AutoInitializeAndCleanupRuntime(){RuntimeInitialize();}
	~AutoInitializeAndCleanupRuntime(){RuntimeCleanup();}
};

int GetPluginVersion();

/// Sends a script message that the player is about to quit
bool NotifyPlayerQuit(bool forceQuit);

void ProcessMouseInWindow();

enum PlayerPause
{
	kPlayerRunning,
	kPlayerPausing,
	kPlayerPaused,
};

// To pause unity: Call PlayerPause. It notifies all appropriate managers to pause or unpause
// Also you shouldn't call PlayerLoop until the player is not paused anymoyre
void SetPlayerPause (PlayerPause pause);
// Is the player currently paused?
PlayerPause GetPlayerPause ();

// Notify scripts about lost or received focus
void SetPlayerFocus(bool focus);

// Sends "frame complete" to scripts
void PlayerSendFrameComplete();

// Reloads quality settings from preferences
// Resets resolution
#if !UNITY_EDITOR && !WEBPLUG
bool SwitchToStandaloneDefaultSettings();
#endif

#if UNITY_OSX
bool SwitchToBatchmode();
#endif

/// Should the player run in the background?
bool GetPlayerRunInBackground();
void SetPlayerRunInBackground(bool runInBackground);


// For editor. Are we currently inside of the PlayerLoop?
// Can be used to detect if we are currently executing game code or editor code in the editor..
bool IsInsidePlayerLoop ();

/// Makes an object survive when loading a new scene.
void DontDestroyOnLoad (Object& object);

int PlayerGetLoadedLevel ();
std::string PlayerGetLoadedLevelName ();
int PlayerGetLevelCount ();
bool GetLevelAndAssetPath (const std::string& levelName, int levelIndex, std::string* levelPath, std::string* assetPath, int* index);
bool GetHasLateBoundLevelFromAssetBundle (const std::string& name);

void ResetPlayerInEditor (int level);
bool IfWillLoadLevel ();
bool IsLoadingLevel ();

/// Called from PreloadManager::WaitForAllAsyncOperationsToComplete
/// while the loading queue is being processed, and the main thread is stalled.
void LevelLoadingLoop();

float GetActualTargetFrameRate();
float GetActualTargetFrameRate(int vSyncCount);
int GetTargetFrameRate();
int GetTargetFrameRateFromScripting();
void SetTargetFrameRate(int target);

//// HACK
void PlayerLoadLevelFromThread (int levelIndex, const std::string& name, AwakeFromLoadQueue& awakeFromLoadQueue);

#endif
