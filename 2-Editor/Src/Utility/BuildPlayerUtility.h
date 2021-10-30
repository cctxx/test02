#pragma once

#include "Runtime/Utilities/GUID.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"

template<class T> class PPtr;
class Object;

// Data necessary for performing a player build.
struct BuildPlayerSetup
{
	std::string                 path;				// Where to put the generated player.
	std::vector<UnityStr>		levels;				// Levels to include in the build.
	BuildTargetPlatform         platform;			// Platform to build for.
	BuildPlayerOptions          options;			// Various options (like e.g. whether to run the build afterwards).
	UInt32						*outCRC;
	BuildPlayerSetup() : outCRC(NULL) {}
};

std::string BuildPlayer (BuildPlayerSetup& setup);
bool IsBuildingPlayer ();

std::string BuildWebAssetBundle (PPtr<Object> mainAsset, std::vector<PPtr<Object> >& theAssets, std::vector<std::string>* assetPaths, std::string target, BuildTargetPlatform targetPlatform, BuildAssetBundleOptions buildOptions, UInt32* outCRC, bool performLicenseCheck = true);

bool IsPlatformOSXStandalone (int platform);

void PushAssetDependencies ();
void PopAssetDependencies ();

/// Internal functionality follows

struct CompressedFile
{
	int level;
	std::string path;
	size_t datasize;
	std::string originalPath;
	
	friend bool operator < (const CompressedFile& lhs, const CompressedFile& rhs);
};

bool BuildPlayerLicenseCheck (BuildTargetPlatform target);

typedef std::list<CompressedFile> CompressedFiles;

std::string BuildUncompressedUnityWebStream (const std::string& path, CompressedFiles& files, const std::vector<UnityStr>& outputPaths, const char* progressTitlebar, int numberOfLevelsToDownloadBeforeStreaming, UInt32 *outCRC);
std::string BuildUnityWebStream (const std::string& path, CompressedFiles& files, const std::vector<UnityStr>& outputPaths, const char* progressTitlebar, int numberOfLevelsToDownloadBeforeStreaming, UInt32 *outCRC);

void UpdateBuildSettingsWithPlatformSpecificData(BuildTargetPlatform platform);
int UpdateBuildSettings (const std::vector<UnityStr>& levels, const std::string& remapOpenScene, bool forceIncludeOpenScene, int options);

int UpdateBuildSettingsWithSelectedLevelsForPlaymode (const std::string& remapOpenScene);
std::string BuildPlayerWithSelectedLevels (const std::string& playerPath, BuildTargetPlatform platform, BuildPlayerOptions options);
bool BuildNaClResourcesWebStream ();
bool BuildNaClWebPlayerResourcesWebStream();

extern const char* kBuildingPlayerProgressTitle;

bool SwitchActiveBuildTargetForEmulation (BuildTargetPlatform targetPlatform);
void SwitchActiveBuildTargetForBuild (BuildTargetPlatform targetPlatform, bool checkSupport = true);

void UpdateRuntimeHashes ();
