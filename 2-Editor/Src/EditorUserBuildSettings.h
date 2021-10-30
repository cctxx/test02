#pragma once

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"

struct MonoString;

/// This needs to be in Sync with FlashBuildSubtarget in C#
enum FlashBuildSubtarget
{
	Flash11dot2 = 0,
	Flash11dot3 = 1,
	Flash11dot4 = 2,
	Flash11dot5 = 3,
	Flash11dot6 = 4,
	Flash11dot7 = 5,
	Flash11dot8 = 6,
};

/// This needs to be in Sync with PS3BuildSubtarget in C#
enum PS3BuildSubtarget
{
	PCHosted = 0,
	HddTitle = 1,
	BluRayTitle = 2,
};


/// This needs to be in Sync with WiiBuildSubtarget in C#
enum WiiBuildSubtarget
{
	kWiiBuildSubtarget_DVD = 1,
	kWiiBuildSubtarget_WiiWare = 2,
	kWiiBuildSubtarget_DVDLibrary = 3,
	kWiiBuildSubtarget_WiiWareLibrary = 4
};

enum MetroBuildType
{
	kMetroBuildAppX = 1,
	kMetroBuildVS
};

enum MetroSDK
{
	kMetroSDK80 = 0,
	kMetroSDK81
};

class EditorUserBuildSettings : public Object
{
	std::vector<UnityStr>    m_BuildLocation;
	int                      m_ActiveBuildTarget;
	int                      m_SelectedBuildTargetGroup;
	int                      m_SelectedStandaloneTarget;
	int                      m_SelectedPS3Subtarget;
	int                      m_SelectedWiiSubtarget;
	int						 m_SelectedWiiDebugLevel;
	int                      m_SelectedXboxSubtarget;
	int                      m_SelectedXboxRunMethod;
	int                      m_SelectedAndroidSubtarget;
	int						 m_SelectedFlashSubtarget;
	int						 m_SelectedMetroTarget;
	int						 m_SelectedMetroBuildType;
	int						 m_SelectedMetroSDK;
	int						 m_SelectedBlackBerrySubtarget;
	int						 m_SelectedBlackBerryBuildType;
	bool                     m_Development;
	bool                     m_ConnectProfiler;
	bool                     m_AllowDebugging;
	bool                     m_InstallInBuildFolder;
	bool					 m_WebPlayerDeployOnline;
	bool                     m_WebPlayerStreamed;
	bool                     m_WebPlayerOfflineDeployment;
	bool					 m_AppendProject;
	bool					 m_SymlinkLibraries;
	int						 m_ArchitectureFlags;
	bool					 m_ExplicitNullChecks;
	bool                     m_EnableHeadlessMode;
	bool					 m_StripPhysics;
	
public:	
	
	REGISTER_DERIVED_CLASS (EditorUserBuildSettings, Object)
	DECLARE_OBJECT_SERIALIZE(EditorUserBuildSettings)
	
	EditorUserBuildSettings (MemLabelId label, ObjectCreationMode mode);
	// ~EditorUserBuildSettings (); declared-by-macro

	// The active target used for simulation / currently being built build target.
	// If you are querying the current target, this is most likely what you are looking for.
	BuildTargetPlatform GetActiveBuildTarget () const;
	bool SetActiveBuildTarget (BuildTargetPlatform target);
	
	BuildTargetSelection GetActiveBuildTargetSelection ();
	
	// The group selected in the build window. This is not necessarily the currently active build target / switched editor.
	BuildTargetPlatformGroup GetSelectedBuildTargetGroup () const { return (BuildTargetPlatformGroup)m_SelectedBuildTargetGroup; }
	void SetSelectedBuildTargetGroup (BuildTargetPlatformGroup targetGroup);

	// The selected standalone target in the standalone player group. This is not necessarily the currently active build target / switched editor.
	BuildTargetPlatform GetSelectedStandaloneTarget () const { return (BuildTargetPlatform)m_SelectedStandaloneTarget; }
	void SetSelectedStandaloneTarget (BuildTargetPlatform target);
	
	FlashBuildSubtarget GetSelectedFlashBuildSubtarget() const {return (FlashBuildSubtarget)m_SelectedFlashSubtarget; }
	void SetSelectedFlashBuildSubtarget (FlashBuildSubtarget target);

	// PS3 Player subtarget
	PS3BuildSubtarget GetSelectedPS3BuildSubtarget () const { return (PS3BuildSubtarget)m_SelectedPS3Subtarget; }
	void SetSelectedPS3BuildSubtarget (PS3BuildSubtarget target);

	// Wii Player subtarget
	WiiBuildSubtarget GetSelectedWiiBuildSubtarget () const { return (WiiBuildSubtarget)m_SelectedWiiSubtarget; }
	void SetSelectedWiiBuildSubtarget (WiiBuildSubtarget target);

	// The selected metro target.
	BuildTargetPlatform GetSelectedMetroTarget () const { return (BuildTargetPlatform)m_SelectedMetroTarget; }
	void SetSelectedMetroTarget (BuildTargetPlatform target);

	// Metro Player build type
	MetroBuildType GetSelectedMetroBuildType () const { return (MetroBuildType)m_SelectedMetroBuildType; }
	void SetSelectedMetroBuildType (MetroBuildType type);

	// Metro Player Target SDK
	MetroSDK GetSelectedMetroSDK () const { return (MetroSDK)m_SelectedMetroSDK; }
	void SetSelectedMetroSDK (MetroSDK type);

	// Wii Player debug level
	WiiBuildDebugLevel GetSelectedWiiBuildDebugLevel () const { return (WiiBuildDebugLevel)m_SelectedWiiDebugLevel; }
	void SetSelectedWiiBuildDebugLevel (WiiBuildDebugLevel level);

	/// Xbox 360 subtarget
	XboxBuildSubtarget GetSelectedXboxBuildSubtarget () const { return (XboxBuildSubtarget)m_SelectedXboxSubtarget; }
	void SetSelectedXboxBuildSubtarget (XboxBuildSubtarget method);

	/// Xbox 360 run method
	XboxRunMethod GetSelectedXboxRunMethod () const { return (XboxRunMethod)m_SelectedXboxRunMethod; }
	void SetSelectedXboxRunMethod (XboxRunMethod method);

	// Android Player subtarget
	AndroidBuildSubtarget GetSelectedAndroidBuildSubtarget () const { return (AndroidBuildSubtarget)m_SelectedAndroidSubtarget; }
	void SetSelectedAndroidBuildSubtarget (AndroidBuildSubtarget target);

	// BlackBerry Player subtarget
	BlackBerryBuildSubtarget GetSelectedBlackBerryBuildSubtarget () const { return (BlackBerryBuildSubtarget)m_SelectedBlackBerrySubtarget; }
	void SetSelectedBlackBerryBuildSubtarget (BlackBerryBuildSubtarget target);

	// BlackBerry Player build type
	BlackBerryBuildType GetSelectedBlackBerryBuildType () const { return (BlackBerryBuildType) m_SelectedBlackBerryBuildType; }
	void SetSelectedBlackBerryBuildType (BlackBerryBuildType type);

	// When building a web player, should it be deployed online?
	bool GetWebPlayerDeployOnline () const { return m_WebPlayerDeployOnline; }
	void SetWebPlayerDeployOnline (bool deployOnline);

	// When building a web player, should it be using streamed mode?
	bool GetWebPlayerStreamed () const { return m_WebPlayerStreamed; }
	void SetWebPlayerStreamed (bool streamed);

	// Should web players include the unityobject.js file, so they can run without requiring the unity web site.
	bool GetWebPlayerOfflineDeployment () const { return m_WebPlayerOfflineDeployment; }
	void SetWebPlayerOfflineDeployment (bool offlineDeployment);
	
	// A map of locations for the different build targets
	std::string GetBuildLocation (BuildTargetPlatform target) const;
	void SetBuildLocation (BuildTargetPlatform target, std::string path);
	
	// Are we building a development player (Good for profiling / debugging)
	bool GetDevelopment () const { return m_Development; }
	void SetDevelopment (bool value);

	// Are we connecting the profiler to the player
	bool GetConnectProfiler () const { return m_ConnectProfiler; }
	void SetConnectProfiler (bool value);
	
	// Are we allowing debugger attaching
	bool GetAllowDebugging () const { return m_AllowDebugging; }
	void SetAllowDebugging (bool value);
		
	// Install in build folder. Useful for source code customers for debugging players workflow
	bool GetInstallInBuildFolder () const { return m_InstallInBuildFolder; }
	void SetInstallInBuildFolder (bool value);
	
	// Append to xcode project file (used on iphone)
	bool GetAppendProject() const { return m_AppendProject; }
	void SetAppendProject(bool value);
	
	// Symlink libraries where possible (used on iphone)
	bool GetSymlinkLibraries() const { return m_SymlinkLibraries; }
	void SetSymlinkLibraries(bool value);

	int GetExplicitNullChecks() const { return m_ExplicitNullChecks; }
	void SetExplicitNullChecks(bool value);

	int GetEnableHeadlessMode() const { return m_EnableHeadlessMode; }
	void SetEnableHeadlessMode(bool value);

	int GetStripPhysics() const { return m_StripPhysics; }
	void SetStripPhysics(bool value);

	// The effective script compilation defines for the current settings.
	void GetActiveScriptCompilationDefines(std::vector<std::string>& outDefines);
	
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
};

EditorUserBuildSettings& GetEditorUserBuildSettings();
EditorUserBuildSettings* GetEditorUserBuildSettingsPtr();
void SetEditorUserBuildSettings(EditorUserBuildSettings* settings);
BuildTargetPlatform FindMostSuitableSupportedTargetPlatform(BuildTargetPlatform p);
