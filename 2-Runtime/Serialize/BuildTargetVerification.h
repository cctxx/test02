#ifndef BUILDTARGETVERIFICATION_H
#define BUILDTARGETVERIFICATION_H

#include "SerializationMetaFlags.h"
#include "Configuration/UnityConfigure.h"

inline bool IsPCStandaloneTargetPlatform (BuildTargetPlatform targetPlatform)
{
	return
	// We don't support building for these any more, but we still need the constants for asset bundle
	// backwards compatibility.
	targetPlatform == kBuildStandaloneOSXUniversal ||
	targetPlatform == kBuildStandaloneOSXPPC ||
	
	targetPlatform == kBuildStandaloneOSXIntel ||
	targetPlatform == kBuildStandaloneOSXIntel64 ||
	targetPlatform == kBuildStandaloneWinPlayer ||
	targetPlatform == kBuildStandaloneWin64Player ||
	targetPlatform == kBuildMetroPlayerX86 ||
	targetPlatform == kBuildMetroPlayerX64 ||
	targetPlatform == kBuildMetroPlayerARM ||
	targetPlatform == kBuildStandaloneLinux ||
	targetPlatform == kBuildStandaloneLinux64 ||
	targetPlatform == kBuildStandaloneLinuxUniversal ||
	targetPlatform == kBuildWinGLESEmu;
}

inline bool IsWebPlayerTargetPlatform (BuildTargetPlatform targetPlatform)
{
	return targetPlatform == kBuildWebPlayerLZMA || targetPlatform == kBuildWebPlayerLZMAStreamed || targetPlatform == kBuildNaCl;
}

inline bool IsMetroTargetPlatform (BuildTargetPlatform targetPlatform)
{
	return targetPlatform == kBuildMetroPlayerX86 || targetPlatform == kBuildMetroPlayerX64 || targetPlatform == kBuildMetroPlayerARM;
}

inline bool CanLoadFileBuiltForTargetPlatform(BuildTargetPlatform targetPlatform)
{
	// Editor and BinaryToTextFile can load anything
#if UNITY_EDITOR || UNITY_EXTERNAL_TOOL
	return true;
	// Web players can handle all web formats
#elif WEBPLUG
	return IsWebPlayerTargetPlatform(targetPlatform) || IsPCStandaloneTargetPlatform(targetPlatform);
#elif UNITY_METRO
	// !! this code should be before #elif UNITY_WIN, because on Metro UNITY_WIN is defined as well !!
	return IsMetroTargetPlatform(targetPlatform);
#elif UNITY_WP8
	// !! this code should be before #elif UNITY_WIN, because on WP8 UNITY_WIN is defined as well !!
	return targetPlatform == kBuildWP8Player;
#elif UNITY_OSX || (UNITY_WIN && !UNITY_WINRT) || UNITY_LINUX
	// Standalone can handle all web and standalone formats
	return IsPCStandaloneTargetPlatform(targetPlatform) || IsWebPlayerTargetPlatform(targetPlatform);
#elif UNITY_WII
	return targetPlatform == kBuildWii;
#elif UNITY_XENON
	return targetPlatform == kBuildXBOX360;
#elif UNITY_PS3
	return targetPlatform == kBuildPS3;
#elif UNITY_IPHONE
	return targetPlatform == kBuild_iPhone;
#elif UNITY_ANDROID
	return targetPlatform == kBuild_Android;
#elif UNITY_FLASH
	return targetPlatform == kBuildFlash;
#elif UNITY_WEBGL
	return targetPlatform == kBuildWebGL;
#elif UNITY_BB10
	return targetPlatform == kBuildBB10;
#elif UNITY_TIZEN
	return targetPlatform == kBuildTizen;
#else
#error Unknown platform
#endif
}

#endif
