#include "UnityPrefix.h"
#include "BuildTargetPlatformSpecific.h"
#include "Editor/Src/LicenseInfo.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Video/MovieTexture.h"
#include "Runtime/Graphics/ProceduralMaterial.h"
#include "Runtime/Audio/AudioClip.h"
#include "Runtime/Serialize/BuildTargetVerification.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Network/NetworkView.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Dynamics/ClothRenderer.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Dynamics/Cloth.h"
#include "Runtime/Terrain/TerrainData.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Audio/AudioListener.h"
#include "Runtime/Audio/AudioSource.h"
#include "Runtime/Audio/AudioReverbZone.h"
#include "Runtime/Dynamics/RigidBody.h"
#include "Runtime/Serialize/SwapEndianArray.h"
#include "Runtime/Dynamics/Joint.h"
#include "Runtime/Camera/RenderLayers/GUILayer.h"
#include "Runtime/Camera/Flare.h"
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"
#include "Runtime/Utilities/Argv.h"

#include <algorithm>

using namespace std;
void CollectBaseClassIDs(int src, set<int>& baseClasses);
static void GetBuildTargetName (BuildTargetPlatform platform, string* playbackEngineName, string* playbackDevelopmentEngineName, BuildPlayerOptions buildOptions);
static void AddPlatformScriptDefineString (BuildTargetPlatform buildMode, vector<string>& outDefines);

extern const char* kMonoClasslibsProfile;
extern std::string kMonoDistroFolder;
const char* kCurrentSceneBackupPath = "Temp/__BuildPlayer Backupscene";

#define WEBPLAYER_BUILDING_ENABLED 1

bool BuildPlayerLicenseCheck (BuildTargetPlatform target)
{
	// Build target enum is out of range
	if (target < kBuildValidPlayer || target >= kBuildPlayerTypeCount)
		return false;

	// When creating preview builds we don't allow for building web players
	#if !WEBPLAYER_BUILDING_ENABLED
	if (target == kBuildWebPlayerLZMA || target == kBuildWebPlayerLZMAStreamed)
		return false;
	#endif
	
	// Require specific license to support some platforms
	if (target == kBuildWii)
		return LicenseInfo::Flag(lf_wii);

	if (target == kBuildXBOX360)
		return LicenseInfo::Flag(lf_xbox_360);

	if (target == kBuildPS3)
		return LicenseInfo::Flag(lf_ps3);

	if (target == kBuild_iPhone)
		return LicenseInfo::Flag(lf_iphone_basic) || LicenseInfo::Flag(lf_iphone_pro);

	if (target == kBuild_Android)
		return LicenseInfo::Flag(lf_android_basic) || LicenseInfo::Flag(lf_android_pro);

	if (target == kBuildFlash)
		return LicenseInfo::Flag(lf_flash_basic) || LicenseInfo::Flag(lf_flash_pro);

	if (target == kBuildMetroPlayerX86 || target == kBuildMetroPlayerX64 || target == kBuildMetroPlayerARM || target == kBuildWP8Player)
		return LicenseInfo::Flag(lf_winrt_basic) || LicenseInfo::Flag(lf_winrt_pro);

	if (target == kBuildBB10)
		return LicenseInfo::Flag(lf_bb10_basic) || LicenseInfo::Flag(lf_bb10_pro);

	if (target == kBuildTizen)
		return LicenseInfo::Flag(lf_tizen_basic) || LicenseInfo::Flag(lf_tizen_pro);

	if (target == kBuildWinGLESEmu)
		return IsDeveloperBuild() || !IsHumanControllingUs(); // GLESEmu only in dev & automated builds

	if (target == kBuildNaCl)
		return true;

	return true;
}


std::string GetBuildTargetAdvancedLicenseName (BuildTargetPlatform target)
{
	if (target == kBuild_iPhone)
		return "iOS Pro License";
	else if (target == kBuild_Android)
		return "Android Pro License";
	else if (target == kBuildFlash)
		return "Flash Pro License";
	else if (target == kBuildMetroPlayerX86 || target == kBuildMetroPlayerX64 || target == kBuildMetroPlayerARM || target == kBuildWP8Player)
		return "WinStore Pro License";
	else if (target == kBuildBB10)
		return "BlackBerry Pro License";
	else if (target == kBuildTizen)
		return "Tizen Pro License";
	else
		return "Pro License";
}


bool HasAdvancedLicenseOnBuildTarget (BuildTargetPlatform target)
{
	// On iPhone / Android advanced version means having iPhone Pro / Android Pro
	// On other platform it means having Unity Pro
	if (target == kBuild_iPhone)
		return LicenseInfo::Flag(lf_iphone_pro);
	else if (target == kBuild_Android)
		return LicenseInfo::Flag(lf_android_pro);
	else if (target == kBuildFlash)
		return LicenseInfo::Flag(lf_flash_pro);
	else if (target == kBuildMetroPlayerX86 || target == kBuildMetroPlayerX64 || target == kBuildMetroPlayerARM || target == kBuildWP8Player)
		return LicenseInfo::Flag(lf_winrt_pro);
	else if(target == kBuildBB10)
		return LicenseInfo::Flag(lf_bb10_pro);
	else if(target == kBuildTizen)
		return LicenseInfo::Flag(lf_tizen_pro);
	else
		return LicenseInfo::Flag(lf_pro_version);
}


void SyncBuildSettingsLicensingAndActivePlatform ()
{
	if (GetEditorUserBuildSettingsPtr() == NULL)
		return;
	
	BuildTargetPlatform targetPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget();
	PlayerSettings& playerSettings = GetPlayerSettings();
	BuildSettings& buildSettings = GetBuildSettings();

	bool staticBatching, dynamicBatching;
	playerSettings.GetPlatformBatching (targetPlatform, &staticBatching, &dynamicBatching);
	buildSettings.enableDynamicBatching = dynamicBatching;

	// Setup build settings from license flags
	buildSettings.hasPROVersion = LicenseInfo::Flag(lf_pro_version);
	buildSettings.isNoWatermarkBuild = LicenseInfo::Flag(lf_no_watermark);
	buildSettings.isPrototypingBuild = LicenseInfo::Flag(lf_prototyping_watermark);
	buildSettings.isEducationalBuild = LicenseInfo::Flag(lf_edu_watermark);
	buildSettings.hasPublishingRights = !LicenseInfo::Flag(lf_trial);
	buildSettings.hasShadows = true; // Hard directional light shadows free as of 4.2
	// Soft and local light shadows both come with Pro. Separate flags in case we change the split later.
	buildSettings.hasSoftShadows = LicenseInfo::Flag(lf_pro_version);
	buildSettings.hasLocalLightShadows = buildSettings.hasSoftShadows;
	buildSettings.hasAdvancedVersion = HasAdvancedLicenseOnBuildTarget(targetPlatform);
	buildSettings.hasRenderTexture = HasAdvancedLicenseOnBuildTarget(targetPlatform);
	buildSettings.m_AuthToken = LicenseInfo::Get()->GetAuthToken();
}

static bool HasTargetPlatform(const QualitySettings::QualitySetting& settings, const std::string& buildTargetGroup)
{
	for (int i=0;i<settings.excludedTargetPlatforms.size();i++)
	{
		if (settings.excludedTargetPlatforms[i] == buildTargetGroup)
			return false;
	}
	return true;
}

static bool StripQualitySettingsForPlatform (BuildTargetPlatform platform, bool isEnteringPlaymode)
{
	string group = GetBuildTargetGroupName(platform);
	
	// Create a list of stripped quality settings
	vector<QualitySettings::QualitySetting> stripped;
	for (int i=0;i<GetQualitySettings().GetQualitySettingsCount();i++)
	{
		if (HasTargetPlatform(GetQualitySettings().GetQualityForIndex(i), group))
			stripped.push_back(GetQualitySettings().GetQualityForIndex(i));
	}

	
	// In playmode we will keep the current quality level the active one
	UnityStr selectedName;
	if (isEnteringPlaymode)
		selectedName = GetQualitySettings().GetCurrent().name;
	// When building a player we will use the default quality for platform settings
	else
	{	
		int platformQuality = GetQualitySettings().GetDefaultQualityForPlatform (group);
		if (platformQuality >= 0 && platformQuality < GetQualitySettings().GetQualitySettingsCount())
			selectedName = GetQualitySettings().GetQualityForIndex(platformQuality).name;
	}

	int currentQuality = -1;
	for (int i=0;i<stripped.size();i++)
	{
		if (selectedName == stripped[i].name)
		{
			currentQuality = i;
			break;
		}
	}
	if (currentQuality == -1)
		currentQuality = 0;
	
	GetQualitySettings().DeletePerPlatformDefaultQuality();
	GetQualitySettings().SetQualitySettings(&stripped[0], stripped.size(), currentQuality);
	
	return !stripped.empty();
}

static void ComputeStrippedMaximumLODLevel(BuildTargetPlatform platform)
{
	int maximumLODLevel = 255;
	for (int i=0;i<GetQualitySettings().GetQualitySettingsCount();i++)
		maximumLODLevel = min(GetQualitySettings().GetQualityForIndex(i).maximumLODLevel, maximumLODLevel);
	if (maximumLODLevel == 255)
		maximumLODLevel = 0;
	GetQualitySettings().SetStrippedMaximumLODLevel( GetBuildSettings ().hasPROVersion ? maximumLODLevel : 0);
}

bool PrepareQualitySettingsForPlatform (BuildTargetPlatform platform, bool isEnteringPlaymode)
{
	bool success = StripQualitySettingsForPlatform (platform, isEnteringPlaymode);
	ComputeStrippedMaximumLODLevel (platform);
	if (!isEnteringPlaymode)
		HintRecommendedQualitySettings (platform);
	
	return success;
}

// This function must be called after stripping quality settings, since it depends on the selected quality being setup for it.
void HintRecommendedQualitySettings(BuildTargetPlatform platform)
{
	// Hint the user if his/her's quality settings are too high for the specific target platform.
	if ( (platform == kBuild_iPhone) || (platform == kBuild_Android) || (platform == kBuildBB10) || (platform == kBuildTizen) )
	{
		const QualitySettings::QualitySetting& setting = GetQualitySettings().GetCurrent();
		if (setting.antiAliasing != 0)
		{
			WarningStringWithoutStacktrace( "Warning! Using antialiasing on a mobile device may decrease performance severely. You can change the mobile quality settings in 'Project Settings -> Quality Settings'." );
		}
		if (setting.pixelLightCount > 1)
		{
			WarningStringWithoutStacktrace( "Warning! Using more than 1 pixel lights on a mobile device may decrease performance severely. You can change the mobile quality settings in 'Project Settings -> Quality Settings'." );
		}
	}
}

std::string GetBuildTargetName (BuildTargetPlatform platform, BuildPlayerOptions buildOptions)
{
	string name;
	string devName;
	GetBuildTargetName (platform, &name, &devName, buildOptions);
	return name;
}

string GetBuildTargetGroupName (BuildTargetPlatform target, bool assertUnsupportedPlatforms)
{
	// "GroupName" is not the same as "GroupDisplayName", GroupNames are used to e.g. retrieve group icon from resources
	// these should never be localized!
	switch (target)
	{
		case kBuildWebPlayerLZMA:
		case kBuildWebPlayerLZMAStreamed:
		case kBuildNaCl:
			return "Web";

		case kBuildStandaloneOSXIntel:
		case kBuildStandaloneOSXIntel64:
		case kBuildStandaloneOSXUniversal:
		case kBuildStandaloneWinPlayer:
		case kBuildStandaloneWin64Player:
		case kBuildStandaloneLinux:
		case kBuildStandaloneLinux64:
		case kBuildStandaloneLinuxUniversal:
			return "Standalone";

		case kBuild_iPhone:      return "iPhone";
		case kBuildWii:          return "Wii";
		case kBuild_Android:     return "Android";
		case kBuildPS3:	         return "PS3";
		case kBuildXBOX360:	     return "XBOX360";
		case kBuildWinGLESEmu:   return "GLES Emulation";
		case kBuildFlash:		 return "FlashPlayer";
		case kBuildMetroPlayerX86:
		case kBuildMetroPlayerX64:
		case kBuildMetroPlayerARM:
			return "Windows Store Apps";
#if INCLUDE_WP8SUPPORT
		case kBuildWP8Player:	return "WP8";
#endif
		case kBuildBB10:	return "BlackBerry";
		case kBuildTizen:	return "Tizen";
		case kBuildNoTargetPlatform: return "Editor - No build target";

		default:	
			if (assertUnsupportedPlatforms)
			AssertString("unsupported platform");
			return "";
	}
}

BuildTargetPlatform GetBuildTargetByName (const string& name)
{
	// Convert name to lowercase so we can easily
	// compare without having to watch out for casing.
	string lowerCaseName = ToLower (name);

	if (lowerCaseName == "standalone")
	{
		#if UNITY_WIN
			#if UNITY_64
			return kBuildStandaloneWin64Player;
			#else
			return kBuildStandaloneWinPlayer;
			#endif
		#elif UNITY_OSX
			return kBuildStandaloneOSXIntel;
		#elif UNITY_LINUX
			#if UNITY_64
			return kBuildStandaloneLinux64;
			#else
			return kBuildStandaloneLinux;
			#endif
		#endif
	}
	else if (lowerCaseName == "win" || lowerCaseName == "pc" || lowerCaseName == "win32")
		return kBuildStandaloneWinPlayer;
	else if (lowerCaseName == "win64")
		return kBuildStandaloneWin64Player;
	else if (lowerCaseName == "mac" || lowerCaseName == "osx")
		return kBuildStandaloneOSXIntel;
	else if (lowerCaseName == "linux" || lowerCaseName == "linux32")
		return kBuildStandaloneLinux;
	else if (lowerCaseName == "linux64")
		return kBuildStandaloneLinux64;
	else if (lowerCaseName == "iphone" || lowerCaseName == "ios")
		return kBuild_iPhone;
	else if (lowerCaseName == "android")
		return kBuild_Android;
	else if (lowerCaseName == "web" || lowerCaseName == "webplayer")
		return kBuildWebPlayerLZMA;
	else if (lowerCaseName == "webgl")
		return kBuildWebGL;
	else if (lowerCaseName == "flash")
		return kBuildFlash;
	else if (lowerCaseName == "nacl")
		return kBuildNaCl;
	else if (lowerCaseName == "wii")
		return kBuildWii;
	else if (lowerCaseName == "xbox360")
		return kBuildXBOX360;
	else if (lowerCaseName == "ps3")
		return kBuildPS3;
	else if (lowerCaseName == "metro" || lowerCaseName == "metro32" || lowerCaseName == "metrox86")
		return kBuildMetroPlayerX86;
	else if (lowerCaseName == "metro64" || lowerCaseName == "metrox64")
		return kBuildMetroPlayerX64;
	else if (lowerCaseName == "metroarm")
		return kBuildMetroPlayerARM;
	else if (lowerCaseName == "winglesemu")
		return kBuildWinGLESEmu;
#if INCLUDE_WP8SUPPORT
	else if (lowerCaseName == "wp8" || lowerCaseName == "windowsphone")
		return kBuildWP8Player;
#endif
	else if (lowerCaseName == "bb10" || name == "blackberry")
		return kBuildBB10;
	else if (lowerCaseName == "tizen")
		return kBuildTizen;

	return kBuildNoTargetPlatform;
}

string GetBuildTargetGroupDisplayName (BuildTargetPlatformGroup targetPlatformGroup)
{
	// TODO: localize strings in this function once localization system is accessible from C++
	switch (targetPlatformGroup)
	{
		case kPlatformStandalone:	return "PC, Mac & Linux Standalone";			// "BuildSettings.Standalone"
		case kPlatformWebPlayer:	return "Web Player";					// "BuildSettings.Web"
		case kPlatformWii:			return "Wii";							// "BuildSettings.Wii"
		case kPlatform_iPhone:		return "iPhone, iPod Touch and iPad";	// "BuildSettings.iPhone"
		case kPlatformPS3:			return "PS3";							// "BuildSettings.PS3"
		case kPlatformXBOX360:		return "Xbox 360";						// "BuildSettings.XBox360"
		case kPlatformAndroid:		return "Android";						// "BuildSettings.Android"
		case kPlatformFlash:		return "FlashPlayer";						// "BuildSettings.FlashPlayer"
		case kPlatformNaCl:			return "Native Client";					
		case kPlatformMetro:		return "Windows Store Apps";
		case kPlatformBB10:			return "BlackBerry";					// "BuildSettings.BlackBerry"
		case kPlatformTizen:		return "Tizen";							// "BuildSettings.Tizen"
#if INCLUDE_WEBGLSUPPORT				
		case kPlatformWebGL:		return "WebGL";
#endif
		default: return string();											// Smth new or f*ed up
	}
}


string GetBuildTargetShortName (BuildTargetPlatform platform)
{
	switch (platform)
	{
		case kBuildStandaloneOSXUniversal:
		case kBuildStandaloneOSXIntel:
		case kBuildStandaloneOSXIntel64:			return "Mac";
		case kBuildMetroPlayerX86:
		case kBuildMetroPlayerX64:
		case kBuildMetroPlayerARM:
		case kBuildStandaloneWin64Player:
		case kBuildStandaloneWinPlayer:			return "Win";
		case kBuildStandaloneLinuxUniversal:
		case kBuildStandaloneLinux64:
		case kBuildStandaloneLinux:				return "Linux";
		case kBuildWebPlayerLZMA: 
		case kBuildWebPlayerLZMAStreamed:		return "Web";
		case kBuildWii:							return "Wii";
		case kBuild_iPhone:						return "iPhone";
		case kBuildPS3:							return "PS3";
		case kBuildXBOX360:						return "Xbox360";
		case kBuild_Android:					return "Android";
		case kBuildWinGLESEmu:					return "GLESEmu";
		case kBuildNaCl:						return "NaCl";
		case kBuildFlash:						return "FlashPlayer";
		case kBuildBB10:						return "BlackBerry";
		case kBuildTizen:						return "Tizen";
#if INCLUDE_WP8SUPPORT
		case kBuildWP8Player:					return "WP8";
#endif
		default: return string();
	}
}


static void GetBuildTargetName (BuildTargetPlatform platform, string* playbackEngineName, string* playbackDevelopmentEngineName, BuildPlayerOptions buildOptions)
{
	if (platform == kBuildStandaloneOSXUniversal || platform == kBuildStandaloneOSXIntel || platform == kBuildStandaloneOSXIntel64)
	{
		*playbackEngineName = "MacStandalonePlayer";
		*playbackDevelopmentEngineName = "MacDevelopmentStandalonePlayer";
	}
	else if (platform == kBuildStandaloneWinPlayer || platform == kBuildWinGLESEmu)
	{
		*playbackEngineName = "WindowsStandalonePlayer";
		*playbackDevelopmentEngineName = "WindowsDevelopmentStandalonePlayer";
	}
	else if (platform == kBuildStandaloneWin64Player)
	{
		*playbackEngineName = "Windows64StandalonePlayer";
		*playbackDevelopmentEngineName = "Windows64DevelopmentStandalonePlayer";
	}
	else if (platform == kBuildMetroPlayerX86 || platform == kBuildMetroPlayerX64 || platform == kBuildMetroPlayerARM)
	{
		*playbackEngineName = "MetroSupport";
		*playbackDevelopmentEngineName = "MetroDevelopmentSupport";
	}
	#if INCLUDE_WP8SUPPORT
	else if (platform == kBuildWP8Player)
	{
		*playbackEngineName = "WP8Support";
		*playbackDevelopmentEngineName = "WP8DevelopmentSupport";
	}
	#endif
	else if (platform == kBuildWii)
	{
		*playbackEngineName = "WiiPlayer";
		*playbackDevelopmentEngineName = "WiiDevelopmentPlayer";
	}
	else if (platform == kBuild_iPhone)
	{
		*playbackEngineName = "iPhonePlayer";
		*playbackDevelopmentEngineName = "iPhoneDevelopmentPlayer";
	}
	else if (platform == kBuildXBOX360)
	{
		*playbackEngineName = "XenonPlayer";
		*playbackDevelopmentEngineName = "XenonDevelopmentPlayer";
	}	
	else if (platform == kBuildPS3)
	{
		*playbackEngineName = "PS3Player";
		*playbackDevelopmentEngineName = "PS3DevelopmentPlayer";
	}
	else if (platform == kBuildWebPlayerLZMA || platform == kBuildWebPlayerLZMAStreamed)
	{
		*playbackEngineName = "WebPlayer";
		*playbackDevelopmentEngineName = "WebPlayer";
	}
	else if (platform == kBuild_Android)
	{
		*playbackEngineName = "AndroidPlayer";
		*playbackDevelopmentEngineName = "AndroidDevelopmentPlayer";
	}
	else if (platform == kBuildBB10)
	{
		*playbackEngineName = "BB10Player";
		*playbackDevelopmentEngineName = "BB10DevelopmentPlayer";
	}
	else if (platform == kBuildTizen)
	{
		*playbackEngineName = "TizenPlayer";
		*playbackDevelopmentEngineName = "TizenDevelopmentPlayer";
	}
	else if (platform == kBuildNaCl)
	{
		*playbackEngineName = "NaClPlayer";
		*playbackDevelopmentEngineName = "NaClDevelopmentPlayer";
	}
	else if ((platform == kBuildStandaloneLinux) || (platform == kBuildStandaloneLinuxUniversal))
	{
		if (kHeadlessModeEnabled & buildOptions)
			*playbackEngineName = "Linux32HeadlessStandalonePlayer";
		else
			*playbackEngineName = "LinuxStandalonePlayer";
		// Headless mode isn't supported in development players
		*playbackDevelopmentEngineName = "LinuxDevelopmentStandalonePlayer";
	}
	else if (platform == kBuildStandaloneLinux64)
	{
		if (kHeadlessModeEnabled & buildOptions)
			*playbackEngineName = "Linux64HeadlessStandalonePlayer";
		else
			*playbackEngineName = "Linux64StandalonePlayer";
		// Headless mode isn't supported in development players
		*playbackDevelopmentEngineName = "Linux64DevelopmentStandalonePlayer";
	}
	else if (platform == kBuildFlash)
	{
		*playbackEngineName = "FlashSupport";
		*playbackDevelopmentEngineName = "FlashSupport";
	}
#if INCLUDE_WEBGLSUPPORT				
	else if (platform == kBuildWebGL)
	{
		*playbackEngineName = "WebGLSupport";
		*playbackDevelopmentEngineName = "WebGLSupport";
	}
#endif
	else
	{
		*playbackEngineName = "";
		*playbackDevelopmentEngineName = "";
	}
}


std::string GetBuildToolsDirectory (BuildTargetPlatform platform, bool assertMissingDirectory)
{
	// for NaCl and Flash, we have tools and playback directories the same.
	if (platform == kBuildNaCl || platform == kBuildFlash || platform == kBuildWebGL || platform == kBuildBB10 || platform == kBuildTizen)
		return GetPlaybackEngineDirectory(platform, 0, false);

	string targetFolder;

	// ToDo: not sure what to do with BuildTargetTools for other platforms than these 3
	//       if everything is ok, remove else path
	if (platform == kBuildWii || 
		platform == kBuildPS3 || 
		platform == kBuildXBOX360 || 
		platform == kBuildMetroPlayerX86 ||
		platform == kBuildMetroPlayerX64 ||
		platform == kBuildMetroPlayerARM
		#if INCLUDE_WP8SUPPORT
		|| platform == kBuildWP8Player
		#endif
		)
	{
		targetFolder = AppendPathName(GetPlaybackEngineDirectory (platform, 0, false), "Tools");
	}
	else
	{
		string playbackEngineName, playbackDevelopmentEngineName;
		GetBuildTargetName(platform, &playbackEngineName, &playbackDevelopmentEngineName, kBuildPlayerOptionsNone);

		targetFolder = AppendPathName(GetApplicationContentsPath(),
			AppendPathName("BuildTargetTools", playbackEngineName));
	}


	if (assertMissingDirectory)
	{
		ErrorIf(!IsDirectoryCreated(targetFolder));
	}

	return targetFolder;
}


bool IsBuildTargetSupported (BuildTargetPlatform platform)
{
	return !GetPlaybackEngineDirectory (platform, 0, false).empty() &&
		BuildPlayerLicenseCheck(platform);
}


string GetPlaybackEngineDirectory (BuildTargetPlatform platform, int options,
	bool assertUnsupportedPlatforms)
{
	string playbackEngineName, playbackDevelopmentEngineName;
	GetBuildTargetName(platform, &playbackEngineName, &playbackDevelopmentEngineName, (BuildPlayerOptions)options);

	if (playbackEngineName.empty())
	{
		if (assertUnsupportedPlatforms)
			ErrorString("Unsupported platform");
		return "";
	}

	// TODO
	// Query the package manager (or utility assembly) for the location to the updatable playback engines
	// if none exist, do the fallback of checking inside unity, etc

	for (int i = 0; i < 2; i++)
	{
		// * First we try in the Unity.app/Contents/PlaybackEngines
		// * if that fails we probably have a source code setup, thus we use the playback engines folder next to Unity
		string baseDir;
		if (i == 0)
			baseDir = AppendPathName(GetApplicationContentsPath(), "PlaybackEngines");
		else
			baseDir = GetTargetsBuildFolder();

		// If we should use development builds, try that
		string targetFolder;

		if ((options & kDevelopmentBuild) != 0)
		{
			targetFolder = AppendPathName(baseDir, playbackDevelopmentEngineName);

			if (IsDirectoryCreated(targetFolder))
				return targetFolder;
		}

		// Otherwise fallback to non-development build folder
		targetFolder = AppendPathName(baseDir, playbackEngineName);

		if (IsDirectoryCreated(targetFolder))
			return targetFolder;
	}

	if (assertUnsupportedPlatforms)
	{
		ErrorString("Build Target directory for does not exist!" + playbackEngineName);
	}

	return "";
}


string GetUnityEngineDllForBuildTarget (BuildTargetPlatform platform)
{
	// okay this is tricky. We want to find a UnityEngine.dll to compile against,
	// however, often we might actually not have that dll.  (if active buildtarget
	// is a windows standalone player,  and you're on the mac, you likely don't
	// have a windowsstandaloneplayer laying around).

	platform = FindMostSuitableSupportedTargetPlatform(platform);

	if (GetEditorUserBuildSettings().GetStripPhysics())
	{
		string path = AppendPathName(GetPlaybackEngineDirectory (platform, 0), "Managed_nophysx/UnityEngine.dll");
		if (IsFileCreated(path))
			return path;
	}

	string path = AppendPathName(GetPlaybackEngineDirectory (platform, 0), "Managed/UnityEngine.dll");

	return path;
}


BuildTargetPlatformGroup GetBuildTargetGroup(BuildTargetPlatform platform)
{
	switch (platform)
	{
		case kBuildStandaloneOSXIntel:
		case kBuildStandaloneOSXIntel64:
		case kBuildStandaloneOSXUniversal:
		case kBuildStandaloneWinPlayer:
		case kBuildStandaloneWin64Player:
		case kBuildStandaloneLinux:
		case kBuildStandaloneLinux64:
		case kBuildStandaloneLinuxUniversal:
		case kBuildWinGLESEmu:
			return kPlatformStandalone;
		case kBuildWebPlayerLZMA:
		case kBuildWebPlayerLZMAStreamed:
			return kPlatformWebPlayer;
		case kBuildWii:
			return kPlatformWii;
		case kBuild_iPhone:
			return kPlatform_iPhone;
		case kBuildPS3:
			return kPlatformPS3;
		case kBuildXBOX360:
			return kPlatformXBOX360;
		case kBuild_Android:
			return kPlatformAndroid;
		case kBuildFlash:
			return kPlatformFlash;
		case kBuildWebGL:
			return kPlatformWebGL;
		case kBuildNaCl:
			return kPlatformNaCl;
		case kBuildMetroPlayerX86:
		case kBuildMetroPlayerX64:
		case kBuildMetroPlayerARM:
			return kPlatformMetro;
		#if INCLUDE_WP8SUPPORT
		case kBuildWP8Player:
			return kPlatformWP8;
		#endif
		case kBuildBB10:
			return kPlatformBB10;
		case kBuildTizen:
			return kPlatformTizen;
		default:
			return kPlatformUnknown;
	}
}


static bool IsConsolePlatform (BuildTargetPlatform platform)
{
	return platform == kBuildPS3 || platform == kBuildXBOX360 ||
		platform == kBuildWii;
}


static bool IsMobilePlatform (BuildTargetPlatform platform)
{
	return platform == kBuild_iPhone || platform == kBuild_Android || platform == kBuildBB10 || platform == kBuildTizen;
}

static bool SupportsReflectionEmit(BuildTargetPlatform platform)
{
	return
		platform != kBuild_iPhone &&
		platform != kBuildPS3 && platform != kBuildXBOX360 && platform != kBuildWii &&
		platform != kBuildFlash && platform != kBuildWebGL &&
		platform != kBuildWP8Player && !IsMetroTargetPlatform(platform);
}

bool HasBuildTargetOSFonts (BuildTargetPlatform targetPlatform)
{
	switch (targetPlatform)
	{
		case kBuildNoTargetPlatform:
		case kBuildAnyPlayerData:
		case kBuildStandaloneOSXIntel:
		case kBuildStandaloneOSXIntel64:
		case kBuildStandaloneOSXUniversal:
		case kBuildStandaloneWinPlayer:
		case kBuildStandaloneWin64Player:
		case kBuildWebPlayerLZMA:
		case kBuildWebPlayerLZMAStreamed:
		case kBuildWinGLESEmu:
		case kBuildWP8Player:
		case kBuild_iPhone:
		case kBuild_Android:
		case kBuildTizen:
			return true;
		default:
			return false;
	}
}

bool HasBuildTargetDefaultUnityFonts (BuildTargetPlatform targetPlatform)
{
	switch (targetPlatform)
	{
	case kBuild_Android:
    case kBuildTizen:
		// Android has very few fonts, and does not have our default (Arial) one.
		return false;
	default:
		return HasBuildTargetOSFonts (targetPlatform);
	}
}



// Should endianess be swapped when building the player?
TransferInstructionFlags CalculateEndianessOptions (BuildTargetPlatform targetPlatform)
{
	bool bigEndianTarget = 
		targetPlatform == kBuildWii ||
		targetPlatform == kBuildPS3 ||
		targetPlatform == kBuildXBOX360;

	if (bigEndianTarget == IsBigEndian ())
		return kNoTransferInstructionFlags;
	else
		return kSwapEndianess;
}



static std::vector<std::string> GetPlatformDefines(const char* platformDefine)
{
	std::vector<std::string> result;
	
#include "Configuration/PerPlatformDefinesForEditor.include"

	if (GetEditorUserBuildSettings().GetStripPhysics() && (!strcmp("UNITY_NACL", platformDefine) || !strcmp("UNITY_FLASH", platformDefine)))
{
		for (std::vector<std::string>::iterator i=result.begin(); i!=result.end(); i++)
{
			if (*i == "ENABLE_PHYSICS" || *i == "ENABLE_CLOTH")
				result.erase(i);
}
}

	return result;
}

static const char* PlatformDefineFor(BuildTargetPlatform platform)
{
	switch (platform)
	{
		case kBuildStandaloneOSXIntel:
		case kBuildStandaloneOSXIntel64:
		case kBuildStandaloneOSXUniversal:
			return "UNITY_STANDALONE_OSX";
		case kBuildStandaloneWinPlayer:
		case kBuildWinGLESEmu:
		case kBuildStandaloneWin64Player:
			return "UNITY_STANDALONE_WIN";
		case kBuildStandaloneLinux:
		case kBuildStandaloneLinux64:
		case kBuildStandaloneLinuxUniversal:
			return "UNITY_STANDALONE_LINUX";
		case kBuildWebPlayerLZMA:
		case kBuildWebPlayerLZMAStreamed:
			return "UNITY_WEBPLAYER";
		case kBuild_iPhone:
			return "UNITY_IPHONE";
		case kBuildPS3:
			return "UNITY_PS3";
		case kBuildXBOX360:
			return "UNITY_XENON";
		case kBuild_Android:
			return "UNITY_ANDROID";
		case kBuildNaCl:
			return "UNITY_NACL_CHROME";
		case kBuildFlash:
			return "UNITY_FLASH";
		#if INCLUDE_WEBGLSUPPORT
		case kBuildWebGL:
			return "UNITY_WEBGL";
		#endif
		case kBuildMetroPlayerARM:
		case kBuildMetroPlayerX64:
		case kBuildMetroPlayerX86:
			return "UNITY_METRO";
		case kBuildBB10:
			return "UNITY_BB10";
		case kBuildTizen:
			return "UNITY_TIZEN";
		#if INCLUDE_WP8SUPPORT
		case kBuildWP8Player:
			return "UNITY_WP8";
		#endif
		default:
			return "UNITY_EDITOR";
	}
}

static bool IsFeatureSupported(const char* featureDefine, BuildTargetPlatform platform)
{
	std::vector<std::string> defines = GetPlatformDefines(PlatformDefineFor(platform));
	return std::find(defines.begin(), defines.end(), featureDefine) != defines.end();
}

bool IsUnityScriptEvalSupported (BuildTargetPlatform target)
{
	return SupportsReflectionEmit(target);
}

static bool AreAnimationEventsSupported (BuildTargetPlatform target)
{
	return true;
}

static bool IsScriptAnimationSupported (BuildTargetPlatform target)
{
	return target != kBuildFlash && target != kBuildWebGL && target != kBuildMetroPlayerX86 && target != kBuildMetroPlayerARM && target != kBuildMetroPlayerX64;
}

bool IsBuildTargetETC (BuildTargetPlatform targetPlatform)
{
	return targetPlatform == kBuild_Android || targetPlatform == kBuildWinGLESEmu || targetPlatform == kBuildBB10 || targetPlatform == kBuildTizen ;
}

bool IsBuildTargetPVRTC (BuildTargetPlatform targetPlatform)
{
	return targetPlatform == kBuild_iPhone || targetPlatform == kBuild_Android ||
		targetPlatform == kBuildWinGLESEmu || targetPlatform == kBuildBB10;
}

bool IsBuildTargetATC (BuildTargetPlatform targetPlatform)
{
	return targetPlatform == kBuild_Android || targetPlatform == kBuildBB10;
}

bool IsBuildTargetDXT (BuildTargetPlatform targetPlatform)
{
	return targetPlatform != kBuildFlash && targetPlatform != kBuild_iPhone && targetPlatform != kBuildWii && targetPlatform != kBuildWebGL && targetPlatform != kBuildBB10 && targetPlatform != kBuildTizen;
}

bool IsBuildTargetFlashATF (BuildTargetPlatform targetPlatform)
{
	return targetPlatform == kBuildFlash;
}


static bool IsTargetPlatformOpenGLES (BuildTargetPlatform targetPlatform)
{
	return targetPlatform == kBuild_iPhone || targetPlatform == kBuild_Android ||
		targetPlatform == kBuildWinGLESEmu ||
		targetPlatform == kBuildNaCl ||
		targetPlatform == kBuildBB10 ||
		targetPlatform == kBuildTizen;
}

bool CanBuildTargetHandle16Bit (BuildTargetPlatform platform)
{
	return platform != kBuildFlash ;
}

static bool IsMobileShaderPlatform (BuildTargetPlatform plat)
{
	return IsMobilePlatform (plat) || plat == kBuildWinGLESEmu;
}


bool DoesTargetPlatformUseDXT5nm (BuildTargetPlatform targetPlatform)
{
	return !(IsTargetPlatformOpenGLES (targetPlatform) && IsMobileShaderPlatform(targetPlatform));
}

bool DoesTargetPlatformSupportRGBM (BuildTargetPlatform targetPlatform)
{
	return !(IsTargetPlatformOpenGLES (targetPlatform) && IsMobileShaderPlatform(targetPlatform));
}


static void AddPlatformScriptDefineString (BuildTargetPlatform buildMode,
										   std::vector<std::string>& outDefines)
{
	if (buildMode == kBuildStandaloneWin64Player ||
	    buildMode == kBuildStandaloneOSXIntel64 ||
	    buildMode == kBuildStandaloneLinux64)
		outDefines.push_back ("UNITY_64");
}

static void AddIfNotAlreadyPresent(std::vector<std::string>& defines, const char* define)
{
	if (std::find(defines.begin(), defines.end(), define) != defines.end())
		return;

	defines.push_back(define);
}

void GetScriptCompilationDefines (bool developmentBuild, bool buildingForEditor, bool editorOnlyAssembly, BuildTargetPlatform platform, std::vector<std::string>& outDefines)
{
	outDefines.push_back(Format("UNITY_%d_%d_%d", UNITY_VERSION_VER, UNITY_VERSION_MAJ, UNITY_VERSION_MIN));
	outDefines.push_back(Format("UNITY_%d_%d", UNITY_VERSION_VER, UNITY_VERSION_MAJ));

	AssertIf(platform == kBuildAnyPlayerData);

	AddPlatformScriptDefineString(platform, outDefines);

	std::vector<std::string> perPlatformDefines = GetPlatformDefines(PlatformDefineFor(platform));
	outDefines.insert(outDefines.begin(), perPlatformDefines.begin(), perPlatformDefines.end());

	if (developmentBuild)
		outDefines.push_back("DEVELOPMENT_BUILD");

	if (buildingForEditor || developmentBuild)
		outDefines.push_back("ENABLE_PROFILER");

	if (buildingForEditor)
	{
		AddIfNotAlreadyPresent(outDefines,"UNITY_EDITOR");

#if defined(_WIN32) || defined(__WIN32__)
		outDefines.push_back("UNITY_EDITOR_WIN");
#elif defined(__APPLE__)
		outDefines.push_back("UNITY_EDITOR_OSX");
#elif defined(linux) || defined(__linux__)
		outDefines.push_back("UNITY_EDITOR_LINUX");
#endif

		if (LicenseInfo::Flag (lf_maint_client))
			AddIfNotAlreadyPresent(outDefines,"UNITY_TEAM_LICENSE");
	}

	if (editorOnlyAssembly)
		AddIfNotAlreadyPresent(outDefines,"ENABLE_DUCK_TYPING");

	if (kBuildNaCl == platform)
		AddIfNotAlreadyPresent (outDefines, "UNITY_WEBPLAYER");
	
	// Append user-specified defines
	std::string userDefines = GetPlayerSettings ().GetEditorOnly ().GetUserScriptingDefineSymbolsForGroup (GetBuildTargetGroup (platform));
	if (!userDefines.empty ())
		Split (userDefines, ';', outDefines);
}


std::string GetAssetAndSceneInfo (Object *obj)
{
	string assetPath = GetAssetPathFromObject (obj);
	string result;
	
	if (!assetPath.empty ())
		result = Format("Asset: '%s'", assetPath.c_str());

	string sceneInfo = GetApplication ().GetCurrentScene ();
	if (!sceneInfo.empty ())
 	{
		if (!result.empty())
			result += '\n';
		
		if (sceneInfo == kCurrentSceneBackupPath)
			result += "Included from currently open scene";
		else
			result += Format("Included from scene: '%s'", sceneInfo.c_str());
		
	}
	return result;
}


bool IsClassSupportedOnBuildTarget(int classID, BuildTargetPlatform buildTarget)
{	
	#define VERIFY(x) if (Object::IsDerivedFromClassID(classID, x)) return false;
	
	if (!IsFeatureSupported("ENABLE_TERRAIN", buildTarget))
	{
		VERIFY(ClassID(TerrainData));
		VERIFY(ClassID(TerrainCollider));
	}

#if ENABLE_MOVIES
	if (!IsFeatureSupported("ENABLE_MOVIES", buildTarget))
	{
		VERIFY(ClassID(MovieTexture));
	}
#endif

	if (!IsFeatureSupported("ENABLE_CLOTH",buildTarget))
	{
		VERIFY(ClassID(Cloth));
		VERIFY(ClassID(ClothRenderer));
	}

	if (!IsFeatureSupported("ENABLE_NETWORK",buildTarget))
	{
		VERIFY(ClassID(NetworkView));
	}
	
	if (!IsFeatureSupported("ENABLE_AUDIO",buildTarget))
	{
		VERIFY(ClassID(AudioSource));
		VERIFY(ClassID(AudioClip));
		VERIFY(ClassID(AudioListener));
	}

	if (!IsFeatureSupported("ENABLE_AUDIO_FMOD",buildTarget))
	{
		VERIFY(ClassID(AudioFilter));
		VERIFY(ClassID(AudioReverbZone));
	}

	if (!IsFeatureSupported("ENABLE_PHYSICS",buildTarget))
	{
		VERIFY(ClassID(Joint));
		VERIFY(ClassID(Rigidbody));
		VERIFY(ClassID(Collider));
	}
	
#if ENABLE_2D_PHYSICS
	if (!IsFeatureSupported("ENABLE_2D_PHYSICS",buildTarget))
	{
		VERIFY(ClassID(Joint2D));
		VERIFY(ClassID(Rigidbody2D));
		VERIFY(ClassID(Collider2D));
	}
#endif

	return true;
}


bool IsClassSupportedOnSelectedBuildTarget(int classID)
{
	BuildTargetPlatform buildTarget =
		GetEditorUserBuildSettings().GetActiveBuildTarget();

	return IsClassSupportedOnBuildTarget(classID, buildTarget);
}


bool AreClassesSupportedOnSelectedBuildTarget(const std::vector<const char *> classes,
	std::string &unsupportedComponent)
{
	BuildTargetPlatform buildTarget = GetEditorUserBuildSettings().GetActiveBuildTarget();

	for (std::vector<const char *>::const_iterator it = classes.begin(); it != classes.end(); ++it)
	{
		if (!IsClassSupportedOnBuildTarget(Object::StringToClassID(*it), buildTarget))
		{
			unsupportedComponent = (*it);
			return false;
		}
	}

	return true;
}

string DeploymentErrorText (Object* obj, BuildTargetPlatform buildTarget)
{
	return 
	GetBuildTargetGroupName(buildTarget) + "\n" + 
	GetAssetPathFromObject(obj) + "\nIncluded from scene: " + GetApplication().GetCurrentScene();
}

bool HasScriptAnimationCurves (AnimationClip& clip)
{
	AnimationClip::FloatCurves& curves = clip.GetFloatCurves();
	for (int i=0;i<curves.size();i++)
	{
		if (curves[i].classID == ClassID(MonoBehaviour))
			return true;
	}
	return false;
}

bool VerifyFeatureDeployment(Object* obj, BuildTargetPlatform buildTarget)
{
	if (!IsClassSupportedOnBuildTarget(obj->GetClassID(), buildTarget))
	{
		bool treatAsError = (buildTarget != kBuildFlash && buildTarget != kBuildNaCl && buildTarget != kBuildWebGL && !IsMetroTargetPlatform(buildTarget) && buildTarget != kBuildXBOX360);

		char const fmt[] = "'%s' is not supported when building for %s.\n%s";
		string msg = Format (fmt, 
								   obj->GetClassName ().c_str (),
								   GetBuildTargetShortName(buildTarget).c_str(),
		                           GetAssetAndSceneInfo (obj).c_str ());
		if (treatAsError)
		{
			ErrorStringObject (msg, obj);
			return false;
		}
		WarningStringObject(msg,obj);
	}

	// Check objects

	if (buildTarget == kBuild_iPhone)
	{
		AudioClip* clip = dynamic_pptr_cast<AudioClip*> (obj);

		if (clip)
		{
			if (clip->GetType() == FMOD_SOUND_TYPE_OGGVORBIS)
			{
				ErrorStringObject("Ogg vorbis compressed audio is not supported when publishing to the iPhone. Please reimport the clip.\n" + 
								  GetAssetPathFromObject(obj) + "\nIncluded from scene: " + GetApplication().GetCurrentScene(), obj);
				return false;
			}
		}
	}

	if (!AreAnimationEventsSupported(buildTarget))
	{
		AnimationClip* clip = dynamic_pptr_cast<AnimationClip*> (obj);
		if (clip != NULL)
		{
			if (!clip->GetEvents().empty())
				WarningStringObject("AnimationEvents are currently not supported on " + DeploymentErrorText(obj, buildTarget), obj);
		}
	}
	
	if (!IsScriptAnimationSupported (buildTarget))
	{
		AnimationClip* clip = dynamic_pptr_cast<AnimationClip*> (obj);
		if (clip != NULL)
		{
			if (HasScriptAnimationCurves(*clip))
				WarningStringObject("AnimationCurves driving script properties are currently not supported on " + DeploymentErrorText(obj, buildTarget), obj);
		}
	}
	

	Font* font = dynamic_pptr_cast<Font*> (obj);
	if (font && font->GetConvertCase() == Font::kDynamicFont && font->GetFontData().empty() && !HasBuildTargetOSFonts(buildTarget))
	{
		ErrorStringObject("Need to include font data on " + DeploymentErrorText(obj, buildTarget), obj);
		return false;
	}
	
	Texture2D* tex = dynamic_pptr_cast<Texture2D*> (obj);

	if (tex)
	{
		TextureFormat format = tex->GetTextureFormat ();
		bool unsupported = false;
		if (IsCompressedDXTTextureFormat (format) && !IsBuildTargetDXT (buildTarget))
			unsupported = true;
		else if (IsCompressedETCTextureFormat (format) && !IsBuildTargetETC (buildTarget))
			unsupported = true;
		else if (IsCompressedPVRTCTextureFormat (format) && !IsBuildTargetPVRTC (buildTarget))
			unsupported = true;
		else if (IsCompressedATCTextureFormat (format) && !IsBuildTargetATC (buildTarget))
			unsupported = true;
		else if (IsCompressedFlashATFTextureFormat (format) && !IsBuildTargetFlashATF (buildTarget))
			unsupported = true;

		if (unsupported)
		{
//			const char* infoText = ". Please switch to a supported texture compression type in the import settings.\n";
			string compressionType = GetCompressionTypeString(tex->GetTextureFormat ());

			ErrorStringObject(compressionType + " compressed textures are not supported when publishing to " + 
							  DeploymentErrorText(obj, buildTarget), obj);
			return false;
		}
	}

	return true;
}


bool VerifyDeployment (Object* obj, BuildTargetPlatform buildTarget)
{
	if (obj == NULL)
	{
		ErrorString("Failed verifying dependent asset because it has been deleted:\n" + GetAssetAndSceneInfo(obj));
		return false;
	}
	
	if ((obj->GetHideFlags() & Object::kDontSave) && obj->GetClassID() != ClassID(AssetBundle))
	{
		ErrorString("An asset is marked as dont save, but is included in the build:\n" + GetAssetAndSceneInfo(obj));
		return false;
	}

	// Ignore default resources file since different build targets have differnet builtin resources
	// and the editor one might not be compatible with the target
	std::string path = GetPersistentManager().GetPathName(obj->GetInstanceID());

	if (StrICmp(path, kResourcePath) == 0)
		return true;

	if (obj->GetClassID () >= 1000 && buildTarget != kBuildNoTargetPlatform)
	{
		ErrorString(Format("Internal Error: Including Editor Object in Asset Bundle %s %s", obj->GetClassName().c_str(), GetAssetPathFromObject(obj).c_str()));
		return false;
	}

	if (!VerifyFeatureDeployment(obj, buildTarget))
		return false;

	return true;
}


string GetMonoLibDirectory (BuildTargetPlatform targetPlatform, const string& profile)
{
	if (targetPlatform == kBuildXBOX360)
		return AppendPathName(GetBuildToolsDirectory(kBuildXBOX360), "MonoXenon");
	else if (targetPlatform == kBuildPS3)
		return AppendPathName(GetBuildToolsDirectory(kBuildPS3), "MonoPS3");
	else if (targetPlatform == kBuildWii)
		return AppendPathName(GetBuildToolsDirectory(kBuildWii), "MonoWii");
	else
	{
		const char* classlib_profile = kMonoClasslibsProfile;

		if (!profile.empty())
			classlib_profile = profile.c_str();

		std::string classlib = kMonoDistroFolder;
		classlib += "/lib/mono/";
		classlib += classlib_profile;
		return AppendPathName (GetApplicationContentsPath (), classlib);
	}
}


static string GetCorlibPath(BuildTargetPlatform platform)
{
	if (GetStrippingLevelForTarget(platform) == kStripMicroCorlib)
		return AppendPathName(GetMonoLibDirectory(platform, "micro"), "mscorlib.dll");
	else
		return AppendPathName(GetMonoLibDirectory(platform), "mscorlib.dll");
}


int GetStrippingLevelForTarget (BuildTargetPlatform platform)
{
	if (platform == kBuildFlash) 
		return GetPlayerSettings().GetEditorOnly().flashStrippingLevel;

	if (!HasAdvancedLicenseOnBuildTarget(platform))
		return 0;

	if (platform == kBuild_Android || platform == kBuild_iPhone || platform == kBuildBB10 || platform == kBuildTizen)
		return GetPlayerSettings().GetEditorOnly().iPhoneStrippingLevel;
	else if ((platform == kBuildXBOX360) || (platform == kBuildPS3))
	{
		// kStripMicroCorlib is not available for Xbox 360 (yet)
		return clamp<int>(GetPlayerSettings().GetEditorOnly().iPhoneStrippingLevel, 0, kStripByteCode);
	}
	return 0;
}

void SetStrippingLevelForTarget (BuildTargetPlatform platform, int strippingLevel)
{
	if (platform == kBuildFlash)
		GetPlayerSettings().GetEditorOnlyForUpdate().flashStrippingLevel = strippingLevel;
	else
		GetPlayerSettings().GetEditorOnlyForUpdate().iPhoneStrippingLevel = strippingLevel;
}

void CopyPlatformSpecificFiles(BuildTargetPlatform platform, string const& stagingArea, string const& stagingAreaDataManaged)
{
	if (platform == kBuild_iPhone)
	{
		// Place correct mscorlib
		string booLang = "Boo.Lang.dll";
		string srcCorlib = GetCorlibPath(platform);
		string targetCorlib = AppendPathName(stagingAreaDataManaged, "mscorlib.dll");
		string targetBooLang = AppendPathName(stagingAreaDataManaged, booLang);

		// Always copy iOS specific Boo.Lang.dll (if it is used), because it is implemented AOT compatible way
		if (IsFileCreated(targetBooLang))
		{
			DeleteFileOrDirectory(targetBooLang);
			CopyFileOrDirectory(AppendPathName(GetMonoLibDirectory(platform, "micro"), booLang), targetBooLang);
		}

		// Replace stock corlib only in case of micromscorlib
		if (GetStrippingLevelForTarget(platform) == kStripMicroCorlib)
		{
			DeleteFileOrDirectory(targetCorlib);
			CopyFileOrDirectory(srcCorlib, targetCorlib);
		}
	}
}


void GetTargetPlatformBatchingDefaults (BuildTargetPlatform platform, bool* outStaticBatching, bool* outDynamicBatching)
{
	// static batching: off by default in:
	// web player (to save space)
	// consoles (save space, & draw calls are not high cost)
	// flash (no good runtime support)
	*outStaticBatching =
		(platform != kBuildNaCl) && (platform != kBuildWebPlayerLZMA) && (platform != kBuildWebPlayerLZMAStreamed) &&
		(platform != kBuildXBOX360) && (platform != kBuildPS3) &&
		(platform != kBuildFlash);

	// dynamic batching: off by default in
	// NaCl (dynamic VBO performance is not good there)
	// consoles (not implemented)
	// Flash (no good runtime support)
	*outDynamicBatching =
		(platform != kBuildNaCl) &&
		(platform != kBuildXBOX360) && (platform != kBuildPS3) &&
		(platform != kBuildFlash);
}


bool DoesBuildTargetUseSecuritySandbox(BuildTargetPlatform platform)
{
	return (GetBuildTargetGroup(platform) == kPlatformWebPlayer);
}

bool DoesBuildTargetSupportPlayerConnectionListening(BuildTargetPlatform platform)
{
	return (platform != kBuildFlash);
}

bool DoesBuildTargetWantCodeGeneratedSerialization(BuildTargetPlatform platform)
{
	// Do not add metro and wp8 platforms here, because we're performing assembly weaving in PostProcess script
	return !IsMetroTargetPlatform(platform) && (platform != kBuildWP8Player) && IsFeatureSupported("ENABLE_SERIALIZATION_BY_CODEGENERATION", platform);
}

// Saves a raw big-endian RGBA image file for the Xbox 360
bool Xbox360SaveSplashScreenToFile(Texture2D* image, const std::string& path)
{
	if (!image)
		return true;

	int width = image->GetDataWidth();
	int height = image->GetDataHeight();
	const int pixelCount = width * height;
	
	ColorRGBA32* pixels = new ColorRGBA32[pixelCount];
	if (!image->GetPixels32(0, pixels))
	{
		delete[] pixels;
		return false;
	}

#if UNITY_LITTLE_ENDIAN
	SwapEndianBytes(width);
	SwapEndianBytes(height);
	SwapEndianArray(pixels, 4, pixelCount);
#endif

	File file;
	if (!file.Open(path, File::kWritePermission))
	{
		delete[] pixels;
		return false;
	}

	int zeroPadding = 0;
	file.Write(&width, 4);
	file.Write(&height, 4);
	file.Write(&zeroPadding, 4);
	file.Write(&zeroPadding, 4);
	file.Write(pixels, pixelCount * 4);
	file.Close();

	delete[] pixels;
	return true;
}
