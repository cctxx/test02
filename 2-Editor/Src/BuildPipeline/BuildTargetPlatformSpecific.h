#pragma once

#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"

/// The purpose of this file is to consolidate all target specific code in here. Functions are not supposed to have actual code, but mainly just if statements
/// detecting if a specific format can be used on the target platform or getting target specific folder names.

enum BuildTargetPlatformGroup
{
	kPlatformUnknown = 0,
	kPlatformStandalone = 1,
	kPlatformWebPlayer = 2,
	kPlatformWii = 3,
	kPlatform_iPhone = 4,
	kPlatformPS3 = 5,
	kPlatformXBOX360 = 6,
	kPlatformAndroid = 7,
	// was kPlatformBroadcom = 8,
	kPlatformGLESEmu = 9,
	kPlatformNaCl = 11,
	kPlatformFlash = 12,
	kPlatformWebGL = 13,
	kPlatformMetro = 14,
	kPlatformWP8 = 15,
	kPlatformBB10 = 16,
	kPlatformTizen = 17,
	kPlatformCount
};

enum StrippingLevel
{
	kStripDisabled = 0,
	kStripAssemblies = 1,
	kStripByteCode = 2,
	kStripMicroCorlib = 3
};

std::string GetPlaybackEngineDirectory (BuildTargetPlatform platform, int options, bool assertUnsupportedPlatforms = true);
std::string GetBuildToolsDirectory (BuildTargetPlatform platform, bool assertMissingDirectory = true);
std::string GetUnityEngineDllForBuildTarget (BuildTargetPlatform platform);
bool BuildPlayerLicenseCheck (BuildTargetPlatform target);
bool IsBuildTargetSupported (BuildTargetPlatform platform);
std::string GetBuildTargetShortName (BuildTargetPlatform platform);
std::string GetBuildTargetName (BuildTargetPlatform platform, BuildPlayerOptions buildOptions = kBuildPlayerOptionsNone);
BuildTargetPlatform GetBuildTargetByName (const std::string& name);
std::string GetBuildTargetGroupName (BuildTargetPlatform target, bool assertUnsupportedPlatforms = true);
BuildTargetPlatformGroup GetBuildTargetGroup(BuildTargetPlatform platform);
std::string GetBuildTargetGroupDisplayName (BuildTargetPlatformGroup targetPlatformGroup);
void GetScriptCompilationDefines (bool developmentBuild, bool buildingForEditor, bool editorOnlyAssembly, BuildTargetPlatform simulationPlatform, std::vector<std::string>& outDefines);
bool IsBuildTargetETC (BuildTargetPlatform targetPlatform);
bool IsBuildTargetPVRTC (BuildTargetPlatform target);
bool IsBuildTargetATC (BuildTargetPlatform target);
bool IsBuildTargetDXT (BuildTargetPlatform targetPlatform);
bool IsBuildTargetFlashATF (BuildTargetPlatform targetPlatform);
bool CanBuildTargetHandle16Bit (BuildTargetPlatform platform);
bool DoesTargetPlatformSupportRGBM (BuildTargetPlatform targetPlatform);
bool DoesTargetPlatformUseDXT5nm (BuildTargetPlatform targetPlatform);
std::string GetMonoLibDirectory (BuildTargetPlatform targetPlatform, const std::string& profile = "");
void CopyPlatformSpecificFiles(BuildTargetPlatform platform, std::string const& stagingArea, std::string const& stagingAreaDataManaged);
bool HasBuildTargetOSFonts (BuildTargetPlatform targetPlatform);
bool HasBuildTargetDefaultUnityFonts (BuildTargetPlatform targetPlatform);
void GetTargetPlatformBatchingDefaults (BuildTargetPlatform platform, bool* outStaticBatching, bool* outDynamicBatching);
bool DoesBuildTargetUseSecuritySandbox(BuildTargetPlatform platform);
bool DoesBuildTargetSupportPlayerConnectionListening(BuildTargetPlatform platform);
bool DoesBuildTargetWantCodeGeneratedSerialization(BuildTargetPlatform platform);
TransferInstructionFlags CalculateEndianessOptions (BuildTargetPlatform targetPlatform);
bool HasAdvancedLicenseOnBuildTarget (BuildTargetPlatform target);
void SyncBuildSettingsLicensingAndActivePlatform ();
std::string GetBuildTargetAdvancedLicenseName (BuildTargetPlatform target);
int GetStrippingLevelForTarget (BuildTargetPlatform platform);
void SetStrippingLevelForTarget (BuildTargetPlatform platform, int strippingLevel);
bool IsUnityScriptEvalSupported (BuildTargetPlatform target);
bool IsClassSupportedOnBuildTarget(int classID, BuildTargetPlatform buildTarget);
bool IsClassSupportedOnSelectedBuildTarget(int classID);
bool AreClassesSupportedOnSelectedBuildTarget(const std::vector<const char *> classes,
std::string &unsupportedComponent);
void HintRecommendedQualitySettings(BuildTargetPlatform platform);
bool PrepareQualitySettingsForPlatform (BuildTargetPlatform platform, bool isEnteringPlaymode);
class Texture2D;
bool Xbox360SaveSplashScreenToFile(Texture2D* image, const std::string& path);
extern const char* kCurrentSceneBackupPath;
