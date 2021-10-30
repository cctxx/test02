#pragma once
#include <map>
#include <string>
#include "SceneInspector.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/GfxDevice/GfxDeviceStats.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/PlayerPrefs.h"

class HierarchyState;
class MacroClient;
struct InputEvent;

#if UNITY_WIN
namespace dirchanges { class DirectoryWatcher; }
#endif

class Application : public ISceneInspector
{
	double       m_LaunchTime;
	int          m_DisallowUpdating;
	int          m_DisallowReloadAssemblies;
	int          m_DisallowAutoRefresh;
	UnityGUID    m_OpenScene;
	std::string  m_URLToOpenAfterLaunch;
	std::string  m_CurrentAssetServerProject;
	bool         m_IsInitialized;
	bool		 m_ChangeLayout;
	bool         m_DidSceneChange;
	bool         m_DidSceneChangeBeforePlaymode;
	bool         m_DidCancelSave;
	bool         m_RequestScriptReload;
	bool		 m_SwitchSkin;
	bool		 m_RepaintAllViews;
	bool		 m_RecreateGfxDevice;
	bool		 m_RecreateSkinnedMeshResources;
	bool         m_CompilationDelayPlaymode;
	int          m_SetIsPlayingDelayed;
	std::map<LocalIdentifierInFileType, SInt32> m_PlaymodeFileIDToHeapID;
	double       m_LastUpdateTime;
	bool         m_DoOneStep;
	bool         m_DisableDrawHouseKeeping;
	bool         m_SceneRepaintDirty;
	bool         m_IsPaused; 
	bool         m_ActiveGoMightHaveChanged;
	bool		 m_InsideTickTimer;
	bool		 m_IgnoreInspectorChanges;
	bool		 m_OpenSceneChangedOnDisk;
	bool		 m_IsUpgradingProject;

	std::string	m_LicenseInfoText;
	std::string m_ProjectToLoadOnNextTick;
	std::vector<std::string> m_ProjectToLoadArgs;
	MacroClient* m_MacroClient;
#if UNITY_WIN
	HANDLE		m_LockFile;
#elif UNITY_OSX
	File		m_LockFile;
#endif

	#if !UNITY_RELEASE
	UInt32			m_CurTickCheckAwakeFromLoad;
	#endif
	
	void CoreShutdown ();

public:
	
	Application ();
	
	void InitializeProject ();
	void FinishLoadingProject ();
	void AfterEverythingLoaded ();
	bool IsInitializingPlayModeLayout ();

	/// Creates a new empty scene
	void NewScene ();

	/// Open the scene without asking any questions or saving the active scene.
	bool OpenScene (string pathName);

	/// Save scene at path
	bool SaveScene (string path, bool explicitSceneSave = false);
	
	// Save scene from script with optional parameters
	bool SaveSceneFromScript (string path = "", bool saveAsCopy = false);

	// Ask user if he wants to save? (Eg. before opening a new scene)
	bool SaveCurrentSceneIfUserWantsTo ();
	bool SaveCurrentSceneIfUserWantsToForce ();
	bool SaveCurrentSceneDontAsk ();

	// Ask user if he wants to save? (Eg. before running lightmap baking which otherwise will cancel the bake...)
	bool EnsureSceneHasBeenSaved(const std::string& operation);
	
	/// Open any supported file type (.unity scene files or .unityPackage)
	/// (Finder event tells us to open a file)
	static bool OpenFileGeneric (const string& filename);

	/// Creates a new empty scene
	/// (File -> New Scene)
	void FileMenuNewScene ();

	/// Display the Open Scene Panel and load the scene if not cancelled.
	/// (File -> Open)
	void FileMenuOpen ();

	/// Save scene based on open scene name or bring up SaveAs dialog and save.
	/// (File -> Save)
	void FileMenuSave (bool explicitlySaveScene = false);

	/// Bring up save as dialog and save if not cancelled.
	/// (File -> Save As)
	void FileMenuSaveAs ();

	/// Saves all assets (eg. Materials, prefabs) & the asset database
	void SaveAssets ();
	
	// Should be called 100 times per second. Updates various managers, that need to be ticked
	void TickTimer();

	// Disable Updating of views while for example importing assets etc.
	void DisableUpdating ();
	void EnableUpdating (bool forceSceneUpdate);
	bool MayUpdate ();

	void DisallowAutoRefresh();
	void AllowAutoRefresh();

	void LockReloadAssemblies ();
	void UnlockReloadAssemblies ();
	void ResetReloadAssemblies ();

	// Reads License info
	void ReadLicenseInfo ();
	void EnterSerialNumber ();
    void ReturnLicense();
	const std::string& GetLicenseInfoText() const { return m_LicenseInfoText; }
	void GetLicenseFlags( std::vector<int>& outFlags );

	// What is shown in the window title bar as file name
	string GetDisplayOpenPath ();
	
	// The path to the current opened scene (empty for untitled scene)
	string GetCurrentScene ();
	UnityGUID GetCurrentSceneGUID ();
	
	// Internal Scene saving
	void SaveSceneInternal (string path, TransferInstructionFlags options, std::map<LocalIdentifierInFileType, SInt32>*fileIDToHeapID = NULL);
	void LoadSceneInternal (const string& pathname, int mode, std::map<LocalIdentifierInFileType, SInt32>* fileIDToHeapIDHint = NULL);

	// Terminates the application.
	// Returns false if the user cancelled quitting the application
	bool Terminate ();

	void SetSceneDirty ();
	void SetSceneDirty (bool dirty);
	bool IsSceneDirty ();
	void AutoRefresh ();
	static bool IsInitialized ();

	void SetOpenPath (const std::string& pathName);

	/// Enter/Exit Playmode
	void SetIsPlaying (bool isPlaying);
	/// Enter/Exit Playmode delayed. If we call SetIsPlaying directly from C#, we will crash due to reloading mono domains
	void SetIsPlayingDelayed (bool isPlaying);
	bool IsPlayingOrWillEnterExitPlaymode ();
	
	// Set if we are compiling or not
	// - Updates spinning compiler wheel
	// - delays entering playmode, if user hits play during compilation.
	void RequestScriptReload ();
	void SwitchSkinAndRepaintAllViews ();
	void RequestRepaintAllViews ();
	void RequestRecreateGfxDevice ();
	void RequestRecreateSkinnedMeshResources ();

	void SetIsUpgradingProject(bool upgrading) {m_IsUpgradingProject = upgrading;}
	bool GetIsUpgradingProject() {return m_IsUpgradingProject;}
	/// Step a single frame while in playmode
	void Step ();


	/// Update scene / Player Loop and repaint. 
	void UpdateScene (bool doRepaint = true);

	/// Should we be repainting all scene / game views because something has changed or we are in playmode?
	bool IsUpdateNeeded ();
	
	// Update scene / Player Loop and repaint. But only if something has changed.
	bool UpdateSceneIfNeeded ();

	// Change editor application layout if needed
	void ChangeLayoutIfNeeded ();

	// Update scene / Player Loop and repaint. But only if something has changed.
	void SetSceneRepaintDirty () { m_SceneRepaintDirty = true; }

	/// Set if we are in pause mode?
	void SetPaused (bool pause);

	/// Get if we are in pause mode?
	bool IsPaused () { return m_IsPaused; }

	// Calculates delta time (Takes pausing / stepping into account)
	float CalculateDeltaTime ();

	// This performs various per-frame cleanup tasks (cleans up unused render buffers, text meshes,
	// advances to next frame in editor etc.)
	void PerformDrawHouseKeeping();
	
	/// Sets the name of the asset server project. Used to display the main window title bar
	void SetAssetServerProjectName (const std::string& pathName);

	void UpdateMainWindowTitle ();
	void UpdateDocumentEdited ();

	bool GetIgnoreInspectorChanges() const { return m_IgnoreInspectorChanges; }
	void SetIgnoreInspectorChanges( bool v ) { m_IgnoreInspectorChanges = v; }

	void OpenSceneChangedOnDisk() {m_OpenSceneChangedOnDisk = true;}

	void OpenProjectOnNextTick (const std::string& project, std::vector<string> args)
	{
		m_ProjectToLoadOnNextTick = project;
		m_ProjectToLoadArgs = args;
	}

	void OpenAssetStoreURL(const std::string& url);

private:
	void OpenAssetStoreURLAfterLaunch();
	void TickConsoleAndStatusBar ();

	bool HasObjectChangedCallback () { return true; } 
	void ObjectHasChanged (PPtr<Object> object);
	void GOWasDestroyed (GameObject* go);
	
	void ConnectMacroClient (int port);

	
	static bool SetupProjectPathWithOpenFile (const string& path);

	void ParseARGVCommands ();

	void HandleOpenSceneChangeOnDisk();

	bool AcquireProjectLock ();
	void ReleaseProjectLock ();

	void DoOpenProject(const std::string& project, const std::string& scene, std::vector<std::string> args);
	
	#if !UNITY_RELEASE
	void CheckCorrectAwakeUsage();
	#endif
};

Application& GetApplication();
Application* GetApplicationPtr();

/// GetApplication().SetPaused (true);	
void PauseEditor ();
void FocusProjectView ();
void ShowAboutDialog();
void ShowPreferencesDialog();
bool IsAssetStoreURL( const std::string& url );
void SetupDefaultPreferences ();

/// Set up the gui on the main window to indicate that the file has been modified
void SetMainWindowDocumentEdited (bool edited);
/// Set up the window name to match the given title and file
void SetMainWindowFileName (const std::string& title, const std::string& file);
// Check if open scene name changed after moving/renaming open scene and update window title accordingly
void WindowTitlePostprocess (const std::set<UnityGUID>& imported, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved);

void HandleAutomaticUndoGrouping (const InputEvent& event);
void HandleAutomaticUndoOnEscapeKey (const InputEvent& event, bool eventWasUsed);
