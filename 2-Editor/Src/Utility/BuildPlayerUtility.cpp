#include "UnityPrefix.h"
#include "BuildPlayerUtility.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"
#include "Editor/Src/AssetPipeline/AudioImporter.h"
#include "Runtime/Misc/SaveAndLoadHelper.h"
#include "Editor/Src/BuildPipeline/BuildSerialization.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Misc/PreloadManager.h"
#include "Editor/Src/EditorBuildSettings.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/AssetPipeline/MonoCompilationPipeline.h"
#include "Editor/Src/Utility/RuntimeClassHashing.h"
#include "Runtime/Serialize/SwapEndianBytes.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Misc/AssetBundleUtility.h"
#include "PlatformDependent/CommonWebPlugin/UncompressedFileStreamMemory.h"
#include "PlatformDependent/CommonWebPlugin/CompressedFileStreamMemory.h"
#include "PlatformDependent/CommonWebPlugin/Verification.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoAttributeHelpers.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Editor/Src/Utility/FilesizeInfo.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Editor/Src/Application.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Editor/Src/AssetPipeline/ImageConverter.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Serialize/BuildTargetVerification.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/LicenseInfo.h"
#include "Configuration/UnityConfigureOther.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Editor/Src/AssetPipeline/AssetImporterUtil.h"
#include "Editor/Src/Utility/Analytics.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Editor/Src/AssetPipeline/MonoCompile.h"
#include "Runtime/Network/NetworkUtility.h"
#include "Runtime/Network/PlayerCommunicator/EditorConnection.h"
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"
#include "Editor/Src/AssetPipeline/LogicGraphCompilationPipeline.h"
#include "Runtime/Profiler/ProfilerConnection.h"
#include "Editor/Src/UploadingBuildsManager.h"
#include "Editor/Src/ColorSpaceLiveSwitch.h"
#include "Editor/Src/ManagerBackup.h"
#include "Editor/Src/Graphs/GraphUtils.h"
#include "Editor/Src/BuildPipeline/SerializationWeaver.h"
#include "Editor/Src/BuildPipeline/BuildBuiltinAssetBundles.h"
#include "Editor/Src/EditorModules.h"

#include "Runtime/Graphics/SpriteFrame.h"
#include "Editor/Src/SpritePacker/SpritePacker.h"

/// Set this to 1 to force type trees into the serialized files of player builds.
/// This is useful for inspecting the output of player builds.
#define DEBUG_FORCE_ALWAYS_WRITE_TYPETREES 0


static const char* kTempFolder = "Temp";

void SwitchGraphicsEmulationBuildTarget(BuildTargetPlatformGroup targetPlatform); // EditSumulationCaps.cpp


using namespace std;

enum BuildCopyOptions
{
	kDependenciesAndBuild = 0,
	kDependenciesOnly = 1,
	kBuildOnly = 2
};

static std::string BuildPlayerData (BuildTargetPlatform platform, BuildPlayerOptions options, std::vector<UnityStr> scenePathnames, const std::string& inCurrentScene, MonoObject*& usedClasses, Vector2f progressBarInterval);
MonoObject* CreateUsedClassesRegistry(BuildTargetPlatform platform, set<int>& usedClassIds);
bool BuildPlayerScriptDLLs (BuildTargetPlatform platform, FileSizeInfo& info, MonoObject* classDeps, int buildOptions, vector<UnityStr>& assemblyNames);
static std::string PostprocessPlayer (const std::string& installPath, int buildMode, int options, MonoObject* usedClasses);
static void AddScreenSelector (const string& playerApp, int platform);
static bool AddDataFolderToStream (CompressedFiles& files, int level);
static void AddDebugBuildFiles(string dataFolder, BuildTargetPlatform target, int options);
static void VerifyAllBuildFiles(string dataFolder);
string GetDeprecatedPlatformString (BuildTargetPlatform buildMode);
static vector<UnityStr> GetLevelPathNames ();
void AddIcon (const string& playerApp, BuildTargetPlatform platform);
void CollectBaseClassIDs(int src, set<int>& baseClasses);
static TransferInstructionFlags CalculatePlatformOptimizationFlags (BuildTargetPlatform platform, bool isCompressedLZMAStreamFile);


const char* kBuildingPlayerProgressTitle = "Building Player";
const char* kBuildingAssetBundleProgressTitle = "Building Asset Bundle";
const char* kDeployingPlayerProgressTitle = "Deploying Player";
static const char* kScreenSelectorImageOSX = "Contents/Resources/ScreenSelector.tif";
static const char* kScreenSelectorImageWin = "ScreenSelector.bmp";
static const char* kScreenSelectorImageLinux = "ScreenSelector.png";
static const char* kStagingArea = "Temp/StagingArea";
static const char* kTemporaryDataFolder = "Temp/StagingArea/Data";
static const char* kManagedDllsFolder = "Temp/StagingArea/Data/Managed";
static const char* kLogicGraphCodeFile = "Temp/StagingArea/LogicGraphs.cs";
static const char* kResourcesAssetPath = "resources.assets";
extern const char* kPlayerConnectionConfigFile;
static const char* kStagingAreaUnityWebStream = "Temp/unitystream.unity3d";

namespace 
{
	#define BUILDSTEPS\
		/*			enum_name,				display text,				mobile progress		progress for other platforms */ \
		BS_TUPLE(	kCompilingScripts,		"Compiling Scripts...",		0.1f,				0.1f) \
		BS_TUPLE(	kBuildPlayer,			"Build Player",				0.2f,				0.2f) \
		BS_TUPLE(	kPostprocessing,		"Postprocessing Player",	0.5f,				0.9f) \
		BS_TUPLE(	kDone,					"Done",						1.0f,				1.0f) \

	// enum
	enum BuildStepType
	{
		#define BS_TUPLE(enum_name, displayText, mobile, others) enum_name,
		BUILDSTEPS
		#undef BS_TUPLE
		kNumBuildSteps
	}; 

	// Build step texts
	#define BS_TUPLE(enum_name, displayText, mobile, others) displayText,
	const char* sBuildProgressTexts[kNumBuildSteps]= {BUILDSTEPS};
	#undef BS_TUPLE

	// Progress values for mobile devices
	#define BS_TUPLE(enum_name, displayText, mobile, others) mobile,
	float sMobileBuildProgress[kNumBuildSteps]= {BUILDSTEPS};
	#undef BS_TUPLE

	// Progress values for other platforms
	#define BS_TUPLE(enum_name, displayText, mobile, others) others,
	float sOtherPlatformsBuildProgress[kNumBuildSteps]= {BUILDSTEPS};
	#undef BS_TUPLE

	const char* GetBuildProgressText (BuildStepType buildStep)
	{
		return sBuildProgressTexts[buildStep];
	}

	Vector2f GetBuildProgressInterval (BuildStepType buildStep, BuildTargetPlatform platform)
	{
		if (buildStep == kDone)
			return Vector2f(1.0f, 1.0f);

		if (platform == kBuild_Android || platform == kBuild_iPhone || platform == kBuildBB10 || platform == kBuildTizen)
			return Vector2f(sMobileBuildProgress[buildStep], sMobileBuildProgress[buildStep+1]);
		else
			return Vector2f(sOtherPlatformsBuildProgress[buildStep], sOtherPlatformsBuildProgress[buildStep+1]);
	}
}

void LogBuildError (const string& message)
{
	DebugStringToFile (message, 0, "", -1, kDontExtractStacktrace | kError, 0, GetBuildErrorIdentifier());
}

static stack<InstanceIDToBuildAsset> gAssetDependenciesStack;
static int gBuildPackageSortIndex = 0;

void UpdateRuntimeHashes ()
{
	// If building a player, calculate TypeTree hashes for all runtime classes and store values
	// in BuildSettings. Use those to test if data layout matches that of objects stored in an AssetBundle
	// when loading it during the runtime.
	CalculateHashForRuntimeClasses (GetBuildSettings ().runtimeClassHashes, CalculatePlatformOptimizationFlags(kBuildWebPlayerLZMA, true));
}

// Check for assemblies that have file name different to internal name, and show warnings.
// This can cause build issues on some platforms, namely those doing AOT.
static void CheckForAssemblyFileNameMismatches()
{
	for (int i=MonoManager::kScriptAssemblies;i<GetMonoManager().GetAssemblyCount();i++)
	{
		string assemblyPath = GetMonoManager ().GetAssemblyPath (i);
		if (!IsEditorOnlyAssembly (i) && IsFileCreated(assemblyPath))
		{
			void* params[] = { MonoStringNew(assemblyPath) };
			CallStaticMonoMethod("AssemblyHelper", "CheckForAssemblyFileNameMismatch", params);
		}
	}
}

static void GenerateAssemblyTypeInfos (cil::TypeDB& db)
{
	for (int i=MonoManager::kScriptAssemblies;i<GetMonoManager().GetAssemblyCount();i++)
	{
		string assemblyPath = GetMonoManager ().GetAssemblyPath (i);
		if (!IsEditorOnlyAssembly (i) && IsFileCreated(assemblyPath))
		{
			void* params[] = { MonoStringNew(assemblyPath) };
			MonoArray* classInfos = (MonoArray*)CallStaticMonoMethod("AssemblyHelper", "ExtractAssemblyTypeInfo", params);
			db.AddInfo (classInfos);
		}
	}
}

// Microsoft C# compiler compiles multidimentional arrays to System.Byte[0..., 0...] in IL, while Mono compiles them to System.Byte[, ]
// Therefore, we have to delete all '0...' on platforms that use Microsoft compiler to compile their scripts,
// as we will be comparing field names and types with ones compiled on the editor (which uses Mono)
static void PostProcessAssemblyTypeInfos (cil::TypeDB& db)
{
	for (cil::TypeDB::class_iterator i = db.class_begin (); i != db.class_end (); i++)
	{
		cil::TypeDB::field_cont_t& fields = i->second.fields;

		for (cil::TypeDB::field_cont_t::iterator j = fields.begin (); j != fields.end (); j++)
		{
			replace_string(j->typeName, "0...", "");
		}
	}
}

static bool SetActiveBuildTargetAndSyncState (BuildTargetPlatform targetPlatform)
{
	if (!GetEditorUserBuildSettings().SetActiveBuildTarget(targetPlatform))
		return false;
	
	UnloadCurrentPlatformSupportModule();
	LoadPlatformSupportModule(targetPlatform);
	
	GetApplication().UpdateMainWindowTitle();
	SwitchGraphicsEmulationBuildTarget(GetBuildTargetGroup(targetPlatform));
	SyncBuildSettingsLicensingAndActivePlatform();
	
	return true;
}

static bool ApplyBuildTargetSwitchInternal(BuildTargetPlatform targetPlatform)
{
	BuildTargetPlatform oldTargetPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget();
	
	if (!SetActiveBuildTargetAndSyncState (targetPlatform))
		return false;
	
	try
	{
		VerifyAssetsForBuildTarget(false, AssetInterface::kAllowCancel | AssetInterface::kClearQueueOnCancel);
	}
	catch (ProgressBarCancelled)
	{
		SetActiveBuildTargetAndSyncState (oldTargetPlatform);
		VerifyAssetsForBuildTarget (oldTargetPlatform, AssetInterface::kNoCancel);
	}
	
	ProfilerConnection::Get().SetupTargetSpecificConnection(targetPlatform);
	ColorSpaceLiveSwitch ();

	//switching buildtarget can cause a different UnityEngine/UnityEditor.dll to be loaded, so make sure we do not cache the current one.
	GetMonoManager().UnloadAllAssembliesOnNextDomainReload();
	return true;
}

static void TriggerEditorUserBuildSettingsActiveBuildTargetChangedEvent()
{
	CallStaticMonoMethod("EditorUserBuildSettings", "Internal_ActiveBuildTargetChanged");
}

bool SwitchActiveBuildTargetForEmulation (BuildTargetPlatform targetPlatform)
{
	AnalyticsProcessTracker analyticsTracker("Build", "SwitchBuildTargetEmulation", GetBuildTargetName(targetPlatform));
	
	if (!IsBuildTargetSupported(targetPlatform))
	{
		LogBuildError("Switching to " + GetBuildTargetName(targetPlatform) + " is disabled");
		return false;
	}
	
	if (!ApplyBuildTargetSwitchInternal(targetPlatform))
		return false;
	
	TriggerEditorUserBuildSettingsActiveBuildTargetChangedEvent();
	
	// recompile scripts for new platform target
	DirtyAllScriptCompilers();
	RecompileScripts(0, true, targetPlatform);
	
	analyticsTracker.Succeeded();

	return true;
}

void SwitchActiveBuildTargetForBuild (BuildTargetPlatform targetPlatform, bool checkSupport)
{
	if (checkSupport)
	{
		Assert(IsBuildTargetSupported(targetPlatform));
		
		if (!IsFileCreated(GetUnityEngineDllForBuildTarget(targetPlatform)))
		{
			LogBuildError("UnityEngine.dll has not been built for the target platform. Please make sure to build the target platform. " + GetBuildTargetName(targetPlatform));
		}
	}
	
	ApplyBuildTargetSwitchInternal(targetPlatform);

	// VerifyAssetsForBuildTarget is performed when switching the build target, 
	// but we have to do call it even if the build target has not changed to handle the case where the project contains "not yet compressed textures"
	VerifyAssetsForBuildTarget(true, AssetInterface::kNoCancel);
}

static void PrepareQualitySettingsForBuild (BuildTargetPlatform targetPlatform)
{
	SaveUserEditableManagers();
	PrepareQualitySettingsForPlatform(targetPlatform, false);
}

static void CleanupAssetBundleBuild ()
{
	ClearProgressbar();
	ResetUserEditableManagers();
	UnloadUnusedAssetsImmediate(true);
}

std::string BuildWebAssetBundle (PPtr<Object> mainAsset, std::vector<PPtr<Object> >& theAssets, std::vector<std::string>* assetPaths, std::string target, BuildTargetPlatform targetPlatform, BuildAssetBundleOptions assetBundleOptions, UInt32* outCRC, bool performLicenseCheck)
{
	string error;

	ConvertSeparatorsToUnity(target);
	
	AnalyticsProcessTracker analyticsTracker("Build", "WebAssetBundle", GetBuildTargetName(targetPlatform));

	if (performLicenseCheck)
	{
		if (!LicenseInfo::Flag(lf_pro_version))
			return "Building Asset Bundles requires Unity PRO";
		if (targetPlatform == kBuild_iPhone && !LicenseInfo::Flag(lf_iphone_pro))
			return "Building Asset Bundles for iPhone requires Unity iPhone Pro";
		if (targetPlatform == kBuild_Android && !LicenseInfo::Flag(lf_android_pro))
			return "Building Asset Bundles for Android requires Unity Android Pro";
		if (targetPlatform == kBuildBB10 && !LicenseInfo::Flag(lf_bb10_pro))
			return "Building Asset Bundles for BlackBerry requires Unity BlackBerry Pro";
		if (targetPlatform == kBuildTizen && !LicenseInfo::Flag(lf_tizen_pro))
			return "Building Asset Bundles for Tizen requires Unity Tizen Pro";
	}
	
	// Do we have a playback engine directory we can use for building the player?
	if (GetPlaybackEngineDirectory (targetPlatform, 0, false).empty())
		return "Building an asset bundle for the target is not supported in this Unity build.";
	
	// Verify license
	if (!BuildPlayerLicenseCheck (targetPlatform))
		return "Building an asset bundle for the target is not allowed because your License does not include it.";
	
	AssetInterface::Get ().SaveAssets ();
	
	try
	{
		// Switch build target (compress textures that aren't compressed in the right format yet)
		SwitchActiveBuildTargetForBuild(targetPlatform);
		PrepareQualitySettingsForBuild(targetPlatform);
		
		UnityGUID guid;
		guid.Init();
		
		string internalName = "CAB-" + GUIDToString(guid);
		if (assetBundleOptions & kAssetBundleDeterministic)
			internalName = "CAB-" + DeletePathNameExtension(GetLastPathNameComponent(target));
		
		string tempPath = AppendPathName("Temp", internalName);
		
		DisplayProgressbarThrowOnCancel (kBuildingAssetBundleProgressTitle, "Building Asset Bundle.", 0.1F) ;
		
		InstanceIDToBuildAsset assets;
		if (!gAssetDependenciesStack.empty())
			assets = gAssetDependenciesStack.top();
		
		TransferInstructionFlags internalMask = CalculatePlatformOptimizationFlags(targetPlatform, true);
		if (assetBundleOptions & kAssetBundleDisableWriteTypeTree)
			internalMask |= kDisableWriteTypeTree;
		
		if (internalMask & kDisableWriteTypeTree)
			internalMask |= kSerializedAssetBundleVersion;

		if ((internalMask & kDisableWriteTypeTree) && IsWebPlayerTargetPlatform (targetPlatform))
		{
			CleanupAssetBundleBuild ();
			return "An asset bundle targeting Web platform must embed type information";
		}

		gBuildPackageSortIndex++;
		
		GetPersistentManager().SetPathRemap (internalName, tempPath);
		error = BuildCustomAssetBundle (mainAsset, theAssets, assetPaths, internalName, gBuildPackageSortIndex, assets, targetPlatform, internalMask, assetBundleOptions);
		
		GetPersistentManager().SetPathRemap (internalName, "");
		
		if (!gAssetDependenciesStack.empty())
			gAssetDependenciesStack.top() = assets;
		
		if (!error.empty())
		{	
			CleanupAssetBundleBuild ();
			return error;
		}
		CompressedFiles files;
		CompressedFile file;
		file.level = 0;
		file.path = internalName;
		file.originalPath = tempPath;
		file.datasize = GetFileLength(tempPath);

		files.push_back(file);
		
		vector<UnityStr> outputPath;
		outputPath.push_back(target);
		
		if (assetBundleOptions & kAssetBundleUncompressed)
		{
			error = BuildUncompressedUnityWebStream(kStagingAreaUnityWebStream, files, outputPath, kBuildingAssetBundleProgressTitle, 1, outCRC);
		}
		else
		{
			error = BuildUnityWebStream(kStagingAreaUnityWebStream, files, outputPath, kBuildingAssetBundleProgressTitle, 1, outCRC);
		}
		DeleteFile(file.originalPath);

	} catch (ProgressBarCancelled)
	{
		error = "Building asset bundle was cancelled";
	}
	
	CleanupAssetBundleBuild();
	
	if (!error.empty())
		return error;
	
	if (MoveReplaceFile(kStagingAreaUnityWebStream, target))
	{
		analyticsTracker.Succeeded();
		return "";
	}
	else
	{
		return "Failed to replace file " + target;
	}
}

void WriteBigEndian (vector<UInt8>& buffer, SInt32 data)
{
#if UNITY_LITTLE_ENDIAN
	SwapEndianBytes(data);	
#endif
	
	buffer.insert(buffer.end(), reinterpret_cast<UInt8*> (&data), reinterpret_cast<UInt8*> (&data) + sizeof(SInt32));
}

static void WriteString (vector<UInt8>& buffer, const string& data)
{
	buffer.insert(buffer.end(), data.begin(), data.end());
	buffer.push_back(0);
}

const int kCompressAlign = 4;
inline int AlignBytesCompress (int pos)
{
	int newpos =  ((pos + kCompressAlign - 1) / kCompressAlign) * kCompressAlign;
	AssertIf (newpos % kCompressAlign != 0);
	AssertIf (newpos < pos);
	AssertIf (newpos >= pos+kCompressAlign);
	return newpos;
}

static void BuildUnityWebStreamFinal (const std::string& path, const std::vector<pair<int, int> >& levels, const InputString& compressedData, int numberOfLevelsToDownloadBeforeStreaming, int fileInfoHeaderSize)
{
	// Build header
	vector<UInt8> header;
	std::vector<pair<int, int> >::const_iterator i;
	
	// Build header for plugin to load	
	WriteString(header, "UnityWeb");
	WriteBigEndian(header, 3);
	WriteString(header, UNITY_WEB_BUNDLE_VERSION);
	WriteString(header, UNITY_WEB_MINIMUM_REVISION);
	
	// Write the header data for the player
	int headerSize = header.size() + sizeof(SInt32) * 4 + levels.size() * 2 * sizeof(SInt32) + sizeof(SInt32) * 2;
	headerSize = AlignBytesCompress(headerSize);
	AssertIf(numberOfLevelsToDownloadBeforeStreaming > levels.size());
	int byteStart = headerSize + levels[numberOfLevelsToDownloadBeforeStreaming - 1].first;
	
	WriteBigEndian(header, byteStart);
	WriteBigEndian(header, headerSize);
	WriteBigEndian(header, numberOfLevelsToDownloadBeforeStreaming);
	WriteBigEndian(header, levels.size());
	
	byteStart = headerSize;
	
	for (i=levels.begin();i != levels.end();i++)
	{
		WriteBigEndian(header, i->first);
		WriteBigEndian(header, i->second);
	}
	
	WriteBigEndian(header, compressedData.size() + headerSize);
	WriteBigEndian(header, fileInfoHeaderSize);
	
	AssertIf (AlignBytesCompress(header.size()) != headerSize);
	
	header.resize(headerSize, 0);
	
	// Create file
	File file;
	file.Open(path, File::kWritePermission);
	
	file.Write(&header[0], header.size());
	file.Write(compressedData.data(), compressedData.size());
	
	file.Close();
}


static bool AddToStreamFile (const std::string& pathName, int level, list<CompressedFile>& files, const char *name = NULL)
{
	CompressedFile file;
	files.push_back(file);
	if (name != NULL)
		files.back().path = name;
	else
		files.back().path = GetLastPathNameComponent(pathName);	
		
	files.back().level = level;
	files.back().datasize = GetFileLength(pathName);
	files.back().originalPath = pathName;

	return true;
}

static bool AddToStreamFileIfExists (const std::string& pathName, int level, list<CompressedFile>& files, const char* name = NULL)
{
	if (!IsFileCreated(pathName))
		return true;

	return AddToStreamFile (pathName, level, files, name);
}

bool operator < (const CompressedFile& lhs, const CompressedFile& rhs)
{
	// Sort by level
	if (lhs.level != rhs.level)
		return lhs.level < rhs.level;
	else
	{
		// Sort dlls to be close together (better for lzma compression)
		int leftPriority = 0;
		if (!StrICmp(GetPathNameExtension(lhs.path), "dll"))
			leftPriority = -1;
		int rightPriority = 0;
		if (!StrICmp(GetPathNameExtension(rhs.path), "dll"))
			rightPriority = -1;
		
		// Otherwise just sort by pathname
		// We sort reversed because we want asset files to come first in a compressed stream
		if (rightPriority == leftPriority)
			return rhs.path < lhs.path;
		else
			return leftPriority < rightPriority;
	}
}

static void WriteStreamFile (list<CompressedFile>& files, const std::string& dstPath, int& outHeaderSize, UInt32 *outCRC)
{
	list<CompressedFile>::iterator i;
	// Probe the size of the header
	int headerSize = sizeof(SInt32);
	for (i=files.begin();i!=files.end();i++)
	{
		headerSize += i->path.size() + 1;
		headerSize += 2 * sizeof(SInt32);
	}
	headerSize = AlignBytesCompress(headerSize);
	outHeaderSize = headerSize;
	
	// Build header
	vector<UInt8> header;
	int byteStart = headerSize;
	WriteBigEndian(header, files.size());
	for (i=files.begin();i!=files.end();i++)
	{
		WriteString(header, i->path);
		WriteBigEndian(header, byteStart);
		WriteBigEndian(header, i->datasize);
		byteStart += AlignBytesCompress(i->datasize);
	}
	AssertIf (AlignBytesCompress(header.size()) != headerSize);
	header.resize(headerSize, 0);
	
	File file;
	file.Open(dstPath, File::kWritePermission);
	file.SetFileLength(byteStart);
	
	// Write header
	byteStart = 0;
	file.Write(byteStart,&header[0], header.size());
	byteStart += header.size();
	
	UInt8 paddingbuffer[kCompressAlign];
	memset(paddingbuffer, 0, sizeof(UInt8) * kCompressAlign);
	
	if (outCRC)
		*outCRC = CRCBegin();
	// Write files
	size_t buffersize = 32*1024;
	char* buffer = (char*)UNITY_MALLOC(kMemTempAlloc, buffersize);
	for (i=files.begin();i != files.end();i++)
	{
		AssertIf (AlignBytesCompress(byteStart) != byteStart);
		
		File inputfile;
		inputfile.Open(i->originalPath,File::kReadPermission);
		int readpos = 0;

		while(readpos < i->datasize)
		{
			int bytesread = inputfile.Read(readpos, buffer, buffersize);
			file.Write(byteStart, buffer, bytesread);
			if (outCRC)
				*outCRC = CRCFeed (*outCRC, (const UInt8*)buffer, bytesread);
			readpos += bytesread;
			byteStart += bytesread;
		}

		inputfile.Close();

		file.Write(byteStart, paddingbuffer, AlignBytesCompress(byteStart) - byteStart);
		byteStart = AlignBytesCompress(byteStart);
	}
	UNITY_FREE(kMemTempAlloc, buffer);
	if (outCRC)
		*outCRC = CRCDone(*outCRC);

	file.Close();
}

#if UNITY_WIN
const char* kLZMATool = "Tools/lzma.exe";
#elif UNITY_OSX
const char* kLZMATool = "Tools/lzma";
#elif UNITY_LINUX
const char* kLZMATool = "Tools/lzma-linux32";
#else
#error "Unknown platform";
#endif

static bool CompressLZMA (const std::string& src, const std::string& dst)
{
	DeleteFile(dst);
	string output;
#if UNITY_LINUX
	// lzma-linux32 doesn't take an output filename so we have to rename the compressed into place... For now at least...
	// option -3 corresponds to a dictionary size of 512K or the -d19 flag used for other platforms
	string realDst = src + ".lzma";
	return (LaunchTask(ResolveSymlinks(AppendPathName(GetApplicationContentsPath(), kLZMATool)), &output, "-z", "-3", PathToAbsolutePath(src).c_str(), NULL)
			&& MoveFileOrDirectory(realDst, dst));
#else
	// -d19 results in a decompression dictionary of 512K instead of the default 8MB
	return LaunchTask (ResolveSymlinks(AppendPathName(GetApplicationContentsPath(), kLZMATool)), NULL, "e", PathToAbsolutePath(src).c_str(), PathToAbsolutePath(dst).c_str(), "-fb273", "-d19", NULL);
#endif
}

static void BuildUncompressedUnityWebStreamFinal (const std::string& path, const std::vector<pair<int, int> >& levels, const InputString& uncompressedData, int numberOfLevelsToDownloadBeforeStreaming, int fileInfoHeaderSize)
{
	// Build header
	vector<UInt8> header;
	std::vector<pair<int, int> >::const_iterator i;
	
	// Build header for plugin to load	
	WriteString(header, "UnityRaw");
	WriteBigEndian(header, 3);
	WriteString(header, UNITY_WEB_BUNDLE_VERSION);
	WriteString(header, UNITY_WEB_MINIMUM_REVISION);
	
	// Write the header data for the player
	int headerSize = header.size() + sizeof(SInt32) * 4 + levels.size() * 2 * sizeof(SInt32) + sizeof(SInt32) * 2;
	headerSize = AlignBytesCompress(headerSize);
	AssertIf(numberOfLevelsToDownloadBeforeStreaming > levels.size());
	int byteStart = headerSize + levels[numberOfLevelsToDownloadBeforeStreaming - 1].first;
	
	WriteBigEndian(header, byteStart);
	WriteBigEndian(header, headerSize);
	WriteBigEndian(header, numberOfLevelsToDownloadBeforeStreaming);
	WriteBigEndian(header, levels.size());
	
	byteStart = headerSize;
	
	for (i=levels.begin();i != levels.end();i++)
	{
		WriteBigEndian(header, i->first);
		WriteBigEndian(header, i->second);
	}
	
	WriteBigEndian(header, uncompressedData.size() + headerSize);
	WriteBigEndian(header, fileInfoHeaderSize);
	AssertIf (AlignBytesCompress(header.size()) != headerSize);
	
	header.resize(headerSize, 0);
	
	// Create file
	File file;
	file.Open(path, File::kWritePermission);
	file.Write(&header[0], header.size());
	file.Write(uncompressedData.data(), uncompressedData.size());
	file.Close();
}


string BuildUncompressedUnityWebStream (const std::string& path, CompressedFiles& files, const vector<UnityStr>& paths, const char* progressTitlebar, int numberOfLevelsToDownloadBeforeStreaming, UInt32 *outCRC)
{
	// Sort the files by levels and eg. pack code close together
	files.sort();
	int fileInfoHeaderSize;
	// Write them into one uncompressed file
	WriteStreamFile (files, "Temp/uncompressedData", fileInfoHeaderSize, outCRC);
	
	DisplayProgressbarThrowOnCancel (progressTitlebar, "Postprocessing uncompressed web stream.", 0.85F);
	
	// We now decompress the file in small chunks and determine when a level can completely be loaded.
	// (Given in compressed size)
	// We then store those level complete markers in the header which is not being compressed.
	InputString uncompressed;
	if (!ReadStringFromFile(&uncompressed, "Temp/uncompressedData"))
		return "Reading uncompressedData file failed";
	
	UncompressedFileStream file;
	
	vector<pair<int,  int> > uncompressedByteEnd;
	uncompressedByteEnd.resize(files.back().level + 1, make_pair(-1, -1));
	
	int probe = 0;
	int totalSize = uncompressed.size();
	file.SetTotalSize(totalSize);
	while (probe != totalSize)
	{
		int chunk = std::min (512, totalSize - probe);
		
		file.Feed((UInt8*)uncompressed.data() + probe, chunk);
		probe += chunk;
		
		if (!file.LoadFiles (true))
			continue;
		
		// For each level check if we can now load the level completely!
		for (int i = 0; i < uncompressedByteEnd.size(); i++)
		{
			// Can already be loaded
			if (uncompressedByteEnd[i].first != -1)	continue;
			
			bool failed = false;
			// Make sure all files in that level can already be loaded!
			for (CompressedFiles::iterator f=files.begin();f != files.end();f++)
			{
				if (f->level == i && file.Find(f->path) == NULL)
				{
					failed = true;
					break;
				}
			}
			if (failed)	break;
			uncompressedByteEnd[i].first = probe;
			uncompressedByteEnd[i].second = file.GetProcessedBytes();
		}
	}
	
	// Ensure all levels were found
	for (int i = 0; i < uncompressedByteEnd.size(); i++)
	{
		AssertIf(uncompressedByteEnd[i].first == -1);
		AssertIf(uncompressedByteEnd[i].second == -1);
	}		
	
	AssertIf(uncompressedByteEnd.back().first != probe);
	AssertIf(uncompressedByteEnd.back().second != file.GetProcessedBytes());
	

	/// Print stats
	printf_console ("\n***Player size statistics***\n");
	for (int i=0;i<uncompressedByteEnd.size();i++)
	{
		int curCompressedSize = 0;
		if (i != 0)
			curCompressedSize = uncompressedByteEnd[i].first - uncompressedByteEnd[i-1].first;
		else
			curCompressedSize = uncompressedByteEnd[i].first;
		
		int curDecompressedSize = 0;
		if (i != 0)
			curDecompressedSize = uncompressedByteEnd[i].second - uncompressedByteEnd[i-1].second;
		else
			curDecompressedSize = uncompressedByteEnd[i].second;
		
		printf_console("Level %d '%s' uses %s uncompressed / %s uncompressed.\n", i, paths[i].c_str(), FormatBytes(curCompressedSize).c_str(), FormatBytes(curDecompressedSize).c_str());
	}
	printf_console("Total uncompressed size %s. Total uncompressed size %s.\n", FormatBytes(uncompressedByteEnd.back().first).c_str(), FormatBytes(uncompressedByteEnd.back().second).c_str());
	
	BuildUncompressedUnityWebStreamFinal(path, uncompressedByteEnd, uncompressed, numberOfLevelsToDownloadBeforeStreaming, fileInfoHeaderSize);
	return "";
}


string BuildUnityWebStream (const std::string& path, CompressedFiles& files, const vector<UnityStr>& paths, const char* progressTitlebar, int numberOfLevelsToDownloadBeforeStreaming, UInt32 *outCRC)
{
	// Sort the files by levels and eg. pack code close together
	files.sort();
	int fileInfoHeaderSize;
	// Write them into one uncompressed file
	WriteStreamFile (files, "Temp/uncompressedData", fileInfoHeaderSize, outCRC);
	
	DisplayProgressbarThrowOnCancel (progressTitlebar, "Performing maximum LZMA compression. This will take a while.", 0.6F);
	
	// Compress that file
	if (!CompressLZMA("Temp/uncompressedData", "Temp/compressedData"))
		return "LZMA Compression failed";
	
	DisplayProgressbarThrowOnCancel (progressTitlebar, "Postprocessing compressed web stream.", 0.85F);
	
	// We now decompress the file in small chunks and determine when a level can completely be loaded.
	// (Given in compressed size)
	// We then store those level complete markers in the header which is not being compressed.
	InputString compressed;
	if (!ReadStringFromFile(&compressed, "Temp/compressedData"))
		return "Reading compressed file failed";
	
	CompressedFileStreamPeek file;
	
	vector<pair<int,  int> > compressedByteEnd;
	compressedByteEnd.resize(files.back().level + 1, make_pair(-1, -1));
	
	int probe = 0;
	int totalSize = compressed.size();
	while (probe != totalSize)
	{
		int chunk = std::min (512, totalSize-probe);
		
		file.Feed((UInt8*)compressed.data() + probe, chunk);
		probe += chunk;
		
		if (!file.LoadFiles (true))
			continue;
		
		// For each level check if we can now load the level completely!
		for (int i=0;i<compressedByteEnd.size();i++)
		{
			// Can already be loaded
			if (compressedByteEnd[i].first != -1)
				continue;
			
			bool failed = false;
			// Make sure all files in that level can already be loaded!
			for (CompressedFiles::iterator f=files.begin();f != files.end();f++)
			{
				if (f->level == i)
				{
					if (file.Find(f->path) == NULL)
					{
						failed = true;
						break;
					}
				}
			}
			
			if (failed)
				break;
			
			compressedByteEnd[i].first = probe;
			compressedByteEnd[i].second = file.GetProcessedBytes();
		}
		
	}
	
	// Ensure all levels were found
	for (int i=0;i<compressedByteEnd.size();i++)
	{
		AssertIf(compressedByteEnd[i].first == -1);
		AssertIf(compressedByteEnd[i].second == -1);
	}		
	
	AssertIf(compressedByteEnd.back().first != probe);
	AssertIf(compressedByteEnd.back().second != file.GetProcessedBytes());
	
	/// Print stats
	printf_console ("\n***Player size statistics***\n");
	for (int i=0;i<compressedByteEnd.size();i++)
	{
		int curCompressedSize = 0;
		if (i != 0)
			curCompressedSize = compressedByteEnd[i].first - compressedByteEnd[i-1].first;
		else
			curCompressedSize = compressedByteEnd[i].first;
		
		int curDecompressedSize = 0;
		if (i != 0)
			curDecompressedSize = compressedByteEnd[i].second - compressedByteEnd[i-1].second;
		else
			curDecompressedSize = compressedByteEnd[i].second;
		
		printf_console("Level %d '%s' uses %s compressed / %s uncompressed.\n", i, paths[i].c_str(), FormatBytes(curCompressedSize).c_str(), FormatBytes(curDecompressedSize).c_str());
	}
	printf_console("Total compressed size %s. Total uncompressed size %s.\n", FormatBytes(compressedByteEnd.back().first).c_str(), FormatBytes(compressedByteEnd.back().second).c_str());
	
	BuildUnityWebStreamFinal(path, compressedByteEnd, compressed, numberOfLevelsToDownloadBeforeStreaming, fileInfoHeaderSize);
	
#if 0
	struct mstats stats2;	
	stats2 = mstats();
	printf_console("BuildUnityWebStream: Memory usage %s (%d) --- Used: %s Free: %s (Chunks used:%d Free: %d)\n", FormattedByteSize(stats2.bytes_total).c_str(), stats2.bytes_total, FormattedByteSize(stats2.bytes_used).c_str(), FormattedByteSize(stats2.bytes_free).c_str(), stats2.chunks_used, stats2.chunks_free);	
#endif
	return "";
}

static std::string CallMono (const char* className, const char* methodName, void** parameters)
{
	MonoClass* klass = GetMonoManager ().GetMonoClass (className, "UnityEditor");
	if (!klass)
		return "Internal error (PostprocessBuildPlayer not found)";
	
	MonoMethod* method = mono_class_get_method_from_name (klass, methodName, -1);
	if (!method)
		return "Internal error (PostprocessBuildPlayer.Postprocess not found)";
	
	MonoException* exception;
	mono_runtime_invoke_profiled (method, NULL, parameters, &exception);
	if (exception)
	{
		if (IsDeveloperBuild())
			Scripting::LogException(exception, 0);
		
		// If postprocess player raised an exception, do not log it; report it as build error instead.
		MonoException* tempException = NULL;
		MonoString* monoStringMessage = NULL;
		MonoString* monoStringTrace = NULL;
		void* args[] = { exception, &monoStringMessage, &monoStringTrace };
		if (GetMonoManagerPtr () && GetMonoManager ().GetCommonClasses ().extractStringFromException)
		{
			mono_runtime_invoke_profiled (GetMonoManager ().GetCommonClasses ().extractStringFromException->monoMethod, (MonoObject*)exception, args, &tempException);
		}
		
		if (tempException)
			return "Internal error (exception raised, but we failed to get exception string)";
		
		
		// Extract exception string & trace
		string message;
		if (monoStringMessage)
		{
			char* extractedMessage = mono_string_to_utf8 (monoStringMessage);
			message = extractedMessage;
			g_free (extractedMessage);
		}
		return message;
	}
	
	return "";
}

static std::string CallMonoPostprocessPlayer (void** parameters)
{
	const char* kClassName = "PostprocessBuildPlayer";
	const char* kMethodName = "Postprocess";
	
	return CallMono(kClassName, kMethodName, parameters);
}

static std::string CallMonoLaunchPlayer (BuildTargetPlatform buildTarget, string const& installPath, string const& productName, int options)
{
	const char* kClassName = "PostprocessBuildPlayer";
	const char* kMethodName = "Launch";
	void* parameters[] = { &buildTarget, MonoStringNew(installPath), MonoStringNew(productName), &options};
	
	return CallMono(kClassName, kMethodName, parameters);
}


static string PostprocessPlayer (const string& installPath, BuildTargetPlatform buildTarget, int options, MonoObject* usedClasses)
{	
	if (options & kBuildAdditionalStreamedScenes)
	{
		DeleteFile(installPath);
		if (MoveFileOrDirectory (kStagingAreaUnityWebStream, installPath))
			return string();
		else
			return Format("Failed to move streamed scene file to '%s'.", installPath.c_str());
	}
	
	bool installInBuildsFolder = options & kInstallInBuildsFolder;
	if (installInBuildsFolder)
	{
		if (!IsDeveloperBuild(!IsMetroTargetPlatform(buildTarget))
			#if INCLUDE_WP8SUPPORT
			&& (buildTarget != kBuildWP8Player)
			#endif
			)
			return "Install in builds folder can only be used from a source code build";
	}
	else
	{
		if (installPath.empty ())
			return "Build player path is not set";
	}
	
	// We need to generate the AndroidPlayer icons _before_ creating the APK package (which is done in MonoPostprocessPlayer)
	// iPhone icons also should be generated before calling postprocess script
	if (kBuild_Android == buildTarget || kBuild_iPhone == buildTarget || kBuildBB10 == buildTarget || kBuildTizen == buildTarget)
	{
		AddIcon (installPath, buildTarget);
	}
	
	// Player default width
	int width = GetPlayerSettings ().defaultScreenWidth;
	int height = GetPlayerSettings ().defaultScreenHeight;
	if (IsWebPlayerTargetPlatform(buildTarget) || buildTarget == kBuildFlash || buildTarget == kBuildWebGL)
	{
		width = GetPlayerSettings ().defaultWebScreenWidth;
		height = GetPlayerSettings ().defaultWebScreenHeight;	
	}
	
	const string companyName = StripInvalidIdentifierCharacters(GetPlayerSettings ().companyName);
	const string productName = StripInvalidIdentifierCharacters(GetPlayerSettings ().productName);

	string niceInstallPath = installPath;
	ConvertSeparatorsToUnity(niceInstallPath);
	
	void* params[] = { &buildTarget, MonoStringNew(niceInstallPath), MonoStringNew(companyName), MonoStringNew(productName), &width, &height,
		MonoStringNew(UNITY_WEB_DOWNLOAD_URL), MonoStringNew(UNITY_WEB_MANUAL_DOWNLOAD_URL), &options, usedClasses };
	
	std::string err = CallMonoPostprocessPlayer (params);
	if (!err.empty())
		return err;
	
#if !UNITY_WIN
	
	/// Run user provided commandline postprocess script
	string userPostprocessScript = PathToAbsolutePath("Assets/Editor/PostprocessBuildPlayer");
	if (IsFileCreated(userPostprocessScript))
	{
		string optimize;
		if ((options & kDevelopmentBuild) != 0)
			optimize = "strip";
		
		//TODO: !!!
		printf_console("Executing PostprocessBuildPlayer...\n");
		
		LaunchTask ("/bin/chmod", NULL, "u+x", userPostprocessScript.c_str (), NULL);
		
		string target = GetDeprecatedPlatformString (buildTarget);
		
		DisplayProgressbarThrowOnCancel (kBuildingPlayerProgressTitle, "Executing 'Assets/Editor/PostprocessBuildPlayer'", 0.99F);
		
		LaunchTask (userPostprocessScript, NULL,
					installPath.c_str(),
					target.c_str (),
					optimize.c_str (),
					companyName.c_str (),
					productName.c_str (),
					IntToString(width).c_str (),
					IntToString(height).c_str (),
					NULL);
	}
#endif
	
	// Add screen selector
	AddScreenSelector (installPath, buildTarget);

	// This is already done for AndroidPlayer
	if (kBuild_Android != buildTarget && kBuild_iPhone != buildTarget && kBuildBB10 != buildTarget && kBuildTizen != buildTarget && !installInBuildsFolder)
	{
		AddIcon (installPath, buildTarget);
	}

	// Find and execute all correctly formed [Callbacks.PostProcessBuild] attributed methods
	ScriptingArguments arguments;
	arguments.AddInt(buildTarget);
	arguments.AddString(installPath.c_str());

	ScriptingMethodPtr cmpParamMethod = GetScriptingManager().GetScriptingMethodRegistry().GetMethod("UnityEditor", "Empty", "OnPostprocessBuild");
	CallMethodsWithAttribute(MONO_COMMON.postProcessBuildAttribute, arguments, cmpParamMethod->monoMethod);

	return "";
}

void CollectBaseClassIDs(int src, set<int>& baseClasses)
{
	int res = Object::GetSuperClassID(src);
	
	if (ClassID(Object) != res)
	{
		baseClasses.insert(res);
		CollectBaseClassIDs(res, baseClasses);
	}
}

static void AddNativeClassesToUsedClassesRegistry(MonoObject* registry, const set<int>& usedClassIDs)
{
	if (!registry)
		return;
	
	MonoClass* klass = mono_object_get_class(registry);
	if (!klass)
		LogBuildError("Can't find RuntimeClassRegistry class!");
	
	MonoMethod* method = mono_class_get_method_from_name (klass, "AddNativeClassID", 1);
	if (!method)
		LogBuildError("Can't find RuntimeClassRegistry::AddNativeClassID method!");
	
	void* argumentList[1] = { NULL };
	MonoException* exception = NULL;
	
	for (set<int>::const_iterator it = usedClassIDs.begin(); it != usedClassIDs.end(); ++it)
	{
		int arg = *it;
		argumentList[0] = &arg;
		mono_runtime_invoke_profiled(method, registry, argumentList, &exception);
		
		if (exception)
		{
			::Scripting::LogException(exception, 0);
			return;
		}
	}
}

string GetReadableScenePath (const std::string& scenePath)
{
	if (!scenePath.empty())
		return scenePath;
	else
		return "Currently opened scene";
}

struct BuildCopyGameSceneInput {
	string originalScenePath;
	string editorScenePath;
	string destinationPath;
	string assetsPath;
	int buildPackageSortIndex;
	BuildTargetPlatform platform;
	int options;
	BuildCopyOptions buildOption;
	vector<UnityStr> playerAssemblyNames;
};

static void BuildCopyGameScene (const BuildCopyGameSceneInput& buildCopyGameSceneInput, InstanceIDToBuildAsset& assets, set<int>& usedClassIDs, PreloadData* preload)
{
	// Load scene
	GetApplication().OpenScene(buildCopyGameSceneInput.editorScenePath);
	
	// Delete game objects with kEditorOnlyTag tag
	vector<GameObject*> temp;
	FindGameObjectsWithTag (kEditorOnlyTag, temp);
	vector<PPtr<GameObject> > gos;
	for (int i=0;i<temp.size ();i++)
		gos.push_back (PPtr<GameObject> (temp[i]));
	for (int i=0;i<gos.size ();i++)
		DestroyObjectHighLevel (gos[i]);
	
	PostprocessScene ();

	BuildCopyOptions buildOption = buildCopyGameSceneInput.buildOption;
	const string& originalScenePath = buildCopyGameSceneInput.originalScenePath;
	const string& destinationPath = buildCopyGameSceneInput.destinationPath;
	const string& assetsPath = buildCopyGameSceneInput.assetsPath;
	int buildPackageSortIndex = buildCopyGameSceneInput.buildPackageSortIndex;
	BuildTargetPlatform platform = buildCopyGameSceneInput.platform;
	int options = buildCopyGameSceneInput.options;

#if UNITY_LOGIC_GRAPH
	GenerateSceneLogicGraphs(buildOption == kDependenciesOnly || buildOption == kDependenciesAndBuild, buildOption == kDependenciesAndBuild || buildOption == kBuildOnly);
#endif
	if (buildOption == kDependenciesOnly || buildOption == kDependenciesAndBuild)
	{
		CompileGameSceneDependencies (destinationPath, assetsPath, buildPackageSortIndex, assets, usedClassIDs, preload, platform, options);
		AssignTemporaryLocalIdentifierInFileForAssets (assetsPath, assets);
	}
	
	
	if (buildOption == kBuildOnly || buildOption == kDependenciesAndBuild)
	{
		GetPersistentManager().SetPathRemap(destinationPath, AppendPathName(kTemporaryDataFolder, destinationPath));
		
		if (!CompileGameScene (destinationPath, assets, usedClassIDs, platform, options, buildCopyGameSceneInput.playerAssemblyNames))
			throw Format("Failed to build scene: '%s'", GetReadableScenePath(originalScenePath).c_str());
		
		GetPersistentManager().UnloadStream(destinationPath);
		
		GetPersistentManager().SetPathRemap(destinationPath, "");
		
		// Copy written scene into player project folder
		if (!IsFileCreated (AppendPathName(kTemporaryDataFolder, destinationPath)))
			throw Format("Couldn't copy player scene into data folder: '%s'", GetReadableScenePath(originalScenePath).c_str());
	}
}

static void BuildSharedAssetsFile (const std::string& assetPath, const InstanceIDToBuildAsset& assets, BuildTargetPlatform platform, int options)
{
	GetPersistentManager().SetPathRemap (assetPath, AppendPathName (kTemporaryDataFolder, assetPath));
	bool result = CompileSharedAssetsFile (assetPath, assets, platform, options);
	GetPersistentManager().UnloadStream(assetPath);
	GetPersistentManager().SetPathRemap (assetPath, "");
	
	if (!result)
		throw string ("Couldn't build player because of unsupported data on target platform.");
}

static void AddScreenSelector (const string& playerApp, int platform)
{
	Texture2D* texture = GetPlayerSettings ().GetEditorOnly().resolutionDialogBanner;
	if (!texture)
		return;
	
	if (IsPlatformOSXStandalone (platform) )
	{
		Image image (texture->GetDataWidth (), texture->GetDataHeight (), kTexFormatRGB24);
		texture->ExtractImage (&image);
		
		string path = AppendPathName (playerApp, kScreenSelectorImageOSX);
		SaveImageToFile (image.GetImageData (), image.GetWidth (), image.GetHeight (), image.GetFormat(), path, 'TIFF');
	}
	else if (platform == kBuildStandaloneWinPlayer || platform == kBuildStandaloneWin64Player)
	{
		const string playerFolder = DeleteLastPathNameComponent (playerApp);	
		const string playerData = DeletePathNameExtension (GetLastPathNameComponent (playerApp)) + "_Data";
		
		Image image (texture->GetDataWidth (), texture->GetDataHeight (), kTexFormatRGB24);
		texture->ExtractImage (&image);
		
		const string path = AppendPathName (AppendPathName (playerFolder, playerData), kScreenSelectorImageWin);
		SaveImageToFile (image.GetImageData (), image.GetWidth (), image.GetHeight (), image.GetFormat(), path, 'BMPf');
	}
	else if (platform == kBuildStandaloneLinux || platform == kBuildStandaloneLinux64 || platform == kBuildStandaloneLinuxUniversal)
	{
		const string playerFolder = DeleteLastPathNameComponent (playerApp);
		const string playerData = DeletePathNameExtension (GetLastPathNameComponent (playerApp)) + "_Data";

		Image image (texture->GetDataWidth (), texture->GetDataHeight (), kTexFormatRGBA32);
		texture->ExtractImage (&image);

		const string path = AppendPathName (AppendPathName (playerFolder, playerData), kScreenSelectorImageLinux);
		SaveImageToFile (image.GetImageData (), image.GetWidth (), image.GetHeight (), kTexFormatRGBA32, path, 'PNGf');
	}
}

void VerifyDisallowedDlls (set<std::string>& dlls, BuildTargetPlatform buildTarget)
{
	for (set<string>::iterator i=dlls.begin();i!=dlls.end();i++)
	{
		string name = *i;
		if (name == "UnityEditor.dll")
			throw Format("'%s' is included in the build due to a dependency from one of the dll's placed in your project folder. This is not allowed.\nPlease make sure that any dll's using UnityEditor.dll are placed in an 'Editor' folder.", name.c_str());
	}
	
	// Verify managed references to the assemblies & types if they are supported on target platform
#if UNITY_LINUX
	// FIXME, XXX, TODO, HACK: Player building currently breaks on Linux without this,
	// presuambly CWD is different somehow, but I don't know where... PathToAbsolutePath()
	// shouldn't hurt or other platforms, I guess, but I'm slightly paraniod...
	void* params[] = { &buildTarget, MonoStringNew(PathToAbsolutePath(kManagedDllsFolder))};
#else
	void* params[] = { &buildTarget, MonoStringNew(kManagedDllsFolder)};
#endif
	MonoException* exception = NULL;
	CallStaticMonoMethod ("BuildVerifier", "VerifyBuild", params, &exception);
	if (exception)
		throw string("Build verification failed.");
}


static vector<string> GetAssembliesThatShipWithWebPlayer ()
{
	MonoArray* array = (MonoArray*)CallStaticMonoMethod ("AssembliesShippedWithWebPlayerProvider", "ProvideAsArray");
	if (array == NULL)
		throw string("AssembliesShippedWithWebPlayerProvider.ProvideAsArray failed.");
	
	vector<string> result;
	StringMonoArrayToVector(array,result);
	
	return result;
}
static set<string> GetReferencedAssemblies (const vector<std::string>& dllPaths, BuildTargetPlatform targetPlatform)
{
	vector<string> refdirs =  GetDirectoriesGameCodeCanReferenceFromForBuildTarget(targetPlatform);

	void* parameters[] = { Scripting::StringVectorToMono(dllPaths), Scripting::StringVectorToMono(refdirs), &targetPlatform };
	
	MonoArray* array = (MonoArray*)CallStaticMonoMethod ("AssemblyHelper", "FindAssembliesReferencedBy", parameters);
	if (array == NULL)
		throw string("Extracting referenced dlls failed.");
	
	set<string> referencedAssemblies;
	for (int i=0;i<mono_array_length_safe (array);i++)
	{
		string path = MonoStringToCpp (GetMonoArrayElement<MonoString*> (array, i));

		ConvertSeparatorsToUnity(path);
		referencedAssemblies.insert (path);
	}
	
	return referencedAssemblies;
}


bool RemoveAssembly(set<string>& assemblies, string name)
{
	for (set<string>::iterator a=assemblies.begin();a!=assemblies.end();a++)
	{
		if (a->find(name) != string::npos)
		{
			assemblies.erase(a);
			return true;
		}
	}
	return false;
}

bool BuildPlayerScriptDLLs (BuildTargetPlatform platform, FileSizeInfo& info, MonoObject* classDeps, int buildOptions, vector<UnityStr>& assemblyNames)
{
	Assert(!IsDirectoryCreated(kManagedDllsFolder));
		
	// Copy UnityEngine.dll / mono config files
	if (IsWebPlayerTargetPlatform(platform) || IsMetroTargetPlatform(platform)
		#if INCLUDE_WP8SUPPORT
		|| (kBuildWP8Player == platform)
		#endif
		)
		CreateDirectory(kManagedDllsFolder);
	else
		CopyFileOrDirectory(AppendPathName(GetPlaybackEngineDirectory (platform, 0), "Managed"), kManagedDllsFolder);

#if UNITY_LOGIC_GRAPH
	BuildLogicGraphsDll();
#endif
	set<string> allCopiedAssemblies;
	vector<string> assemblyPaths;
	vector<string> userAssemblyNames;

	assemblyPaths.push_back(GetUnityEngineDllForBuildTarget(platform));
	assemblyNames.push_back(GetLastPathNameComponent(assemblyPaths.back()));
	
	// MonoManager expects the second dll to be UnityEditor.dll
	// and all following ones to be script assemblies. So just add an empty string here.
	assemblyNames.push_back(""); 
	
	// Extract all source assemblies which we need in the player
	for (int i=MonoManager::kScriptAssemblies;i<GetMonoManager().GetAssemblyCount();i++)
	{
		string assemblyPath = GetMonoManager ().GetAssemblyPath (i);
		string assemblyFileName = GetLastPathNameComponent(assemblyPath);
		
		if (IsFileCreated(assemblyPath) && !IsEditorOnlyAssembly (i))
		{
			assemblyPaths.push_back(assemblyPath);
			assemblyNames.push_back(GetLastPathNameComponent(assemblyPath));
			
			string dest = AppendPathName (kManagedDllsFolder, assemblyFileName);
			if (!CopyReplaceFile (assemblyPath, dest))
			  throw string ("Failed copying dll into build " + assemblyPath);
			
			if (buildOptions & kDevelopmentBuild || IsMetroTargetPlatform(platform)
				#if INCLUDE_WP8SUPPORT
				|| platform == kBuildWP8Player
				#endif
				)
			{
				CopyReplaceFile (MdbFile(assemblyPath), MdbFile(dest));
				CopyReplaceFile (PdbFile(assemblyPath), PdbFile(dest));
			}
			
			info.AddScriptDll(dest);
			
			allCopiedAssemblies.insert(assemblyPath);
			userAssemblyNames.push_back(assemblyFileName);
		}
	}
	
	// Extract dependencies
	set<string> dependencies;
	dependencies = GetReferencedAssemblies (assemblyPaths, platform);
	
	// Boo compiler (JavaScript eval) requires Mono.CompilerServices.SymbolWriter.dll.
	// This gets linked in dynamically it seems. So the dependency is necessary
	// but not explicitly finding through the generic dependency tracker
	string monolibdir = GetMonoLibDirectory(platform);
	if (dependencies.count (AppendPathName(monolibdir, "Boo.Lang.Compiler.dll")))
		dependencies.insert(AppendPathName(monolibdir, "Mono.CompilerServices.SymbolWriter.dll"));
	
	// Check if the web player requires any external assemblies
	if (IsWebPlayerTargetPlatform(platform))
	{
		vector<string> assembliesShippingWithWebplayer = GetAssembliesThatShipWithWebPlayer();
		for (vector<string>::iterator i=assembliesShippingWithWebplayer.begin();i != assembliesShippingWithWebplayer.end();i++)
		{
			RemoveAssembly(dependencies, *i);
		}

		// All GAC dependencies need to be loaded manually in the web player, so add them to the assembly names
		for (set<string>::iterator i=dependencies.begin();i != dependencies.end();i++)
		{
			Assert(find(assemblyNames.begin(), assemblyNames.end(), *i) == assemblyNames.end());
			assemblyNames.push_back(GetLastPathNameComponent(*i));
		}
	}
	
	// Copy all dependencies
	for (set<string>::iterator i=dependencies.begin();i != dependencies.end();i++)
	{
		string dllPath = *i;
		string dllTargetPath = AppendPathName (kManagedDllsFolder, GetLastPathNameComponent(dllPath));
		
		if (!CopyReplaceFile (dllPath, dllTargetPath))
			throw string ("Failed copying dll from into the build " + dllPath);
		if (buildOptions & kDevelopmentBuild)
		{
			CopyReplaceFile (MdbFile(dllPath), MdbFile(dllTargetPath));
		}
		
		allCopiedAssemblies.insert(dllPath);
		info.AddDependencyDll(dllPath);
	}
	
	// Print what mono dependencies we included
	printf_console ("\n\n");
	
	printf_console ("Mono dependencies included in the build\n");
	for (set<string>::iterator i=allCopiedAssemblies.begin ();i != allCopiedAssemblies.end ();i++)
		printf_console ("Dependency assembly - %s\n", GetLastPathNameComponent(*i).c_str ());
	printf_console ("\n");
	
	// Verify that we didn't include anything bad
	VerifyDisallowedDlls(allCopiedAssemblies, platform);
	
	CopyPlatformSpecificFiles(platform, kStagingArea, kManagedDllsFolder);
	
	// Strip / Link assemblies if stripping is enabled
	if (GetStrippingLevelForTarget(platform) > 0)
	{
		std::vector<std::string> scriptAssemblies;
		std::string prefix("Assembly-");
		// Only script assemblies need to be used as stripping roots
		for (std::vector<std::string>::const_iterator it = userAssemblyNames.begin(); it != userAssemblyNames.end(); it++)
			if (it->compare(0, prefix.size(), prefix) == 0)
				scriptAssemblies.push_back(*it);
		
		void* userAssemblyNamesArgs[] = { &platform, MonoStringNew(kManagedDllsFolder), Scripting::StringVectorToMono(scriptAssemblies), Scripting::StringVectorToMono(userAssemblyNames), classDeps};
		MonoException* exception = NULL;
		CallStaticMonoMethod ("MonoAssemblyStripping", "MonoLink", userAssemblyNamesArgs, &exception);
		if (exception)
			return false;
	}

	return true;
}


MonoObject* CreateUsedClassesRegistry(BuildTargetPlatform platform, set<int>& usedClassIds)
{
	MonoObject* res = NULL;
	
	if (GetStrippingLevelForTarget(platform) > 0)
	{
		set<int> allClasses;
		for (std::set<int>::const_iterator i=usedClassIds.begin ();i != usedClassIds.end ();i++)
		{
			CollectBaseClassIDs(*i, allClasses);
		}
		
		allClasses.insert(usedClassIds.begin(), usedClassIds.end());
		
		MonoArray *arr = mono_array_new (mono_domain_get (), mono_get_int32_class(), allClasses.size());
		int idx = 0;
		for (std::set<int>::const_iterator i=allClasses.begin ();i != allClasses.end ();i++)
		{
			Scripting::SetScriptingArrayElement<int> (arr, idx, *i);
			idx++;
		}
		
		
		void* args[] = { arr};
		MonoException* exception = NULL;
		res = CallStaticMonoMethod ("RuntimeClassRegistry", "Produce", args, &exception);
		if (exception)
			return NULL;
		
	}
	
	return res;
}

inline string BuildLevelPath (int index, string name, int options)
{
	name = DeletePathNameExtension(GetLastPathNameComponent(name));
	if (options & kBuildAdditionalStreamedScenes)
	{
		return Format("BuildPlayer-%s", name.c_str());
	}
	else
	{
		const char* kMainData = "mainData";
		const char* kLevel = "level";
		
		if (index == 0)
			return kMainData;
		else
			return kLevel + IntToString (index - 1);
	}
}

inline string BuildAssetsFilePath (int index, string name, int options)
{
	name = DeletePathNameExtension(GetLastPathNameComponent(name));
	if (options & kBuildAdditionalStreamedScenes)
	{
		return Format("BuildPlayer-%s.sharedAssets", name.c_str());
	}
	else
	{
		const char* kAssetsFileStreamed = "sharedassets%d.assets";
		return Format(kAssetsFileStreamed, index);
	}
}
void PushAssetDependencies ()
{
	if (gAssetDependenciesStack.empty())
		gAssetDependenciesStack.push(InstanceIDToBuildAsset());
	else
		gAssetDependenciesStack.push(gAssetDependenciesStack.top());
}

void PopAssetDependencies ()
{
	if (gAssetDependenciesStack.empty())
	{
		LogBuildError("Begin and End Asset Dependencies does not match!");
	}
	else
		gAssetDependenciesStack.pop();
}

string GetDeprecatedPlatformString (BuildTargetPlatform buildMode)
{
	// Convert target enum to string	
	if (buildMode == kBuildStandaloneOSXIntel)
		return "standaloneOSXIntel";
	else if (buildMode == kBuildStandaloneWinPlayer)
		return "standaloneWin32";
	else if (buildMode == kBuildStandaloneWin64Player)
		return "standaloneWin64";
	else if (buildMode == kBuildMetroPlayerX86)
		return "metroPlayerX86";
	else if (buildMode == kBuildMetroPlayerX64)
		return "metroPlayerX64";
	else if (buildMode == kBuildMetroPlayerARM)
		return "metroPlayerARM";
	#if INCLUDE_WP8SUPPORT
	else if (buildMode == kBuildWP8Player)
		return "windowsPhone8Player";
	#endif
	else if (buildMode == kBuildWebPlayerLZMAStreamed || buildMode == kBuildWebPlayerLZMA)
		return "webplayer";
	else if (buildMode == kBuildWii)
		return "wii";
	else if (buildMode == kBuild_iPhone)
		return "iPhone";
	else if (buildMode == kBuild_Android)
		return "android";
	else
		return "Unsupported build target!";
}

static TransferInstructionFlags CalculatePlatformOptimizationFlags (BuildTargetPlatform platform, bool isCompressedLZMAStreamFile)
{
	TransferInstructionFlags flags = CalculateEndianessOptions(platform);
	flags |= kSerializeGameRelease | kBuildPlayerOnlySerializeBuildProperties;
	
	// TypeTree is supported on the web player & for asset bundles for standalone
	bool writeTypeTrees = false;
	if (IsWebPlayerTargetPlatform(platform))
		writeTypeTrees = true;

#if DEBUG_FORCE_ALWAYS_WRITE_TYPETREES
	writeTypeTrees = true;
#endif

	// For the time being asset bundles contain typetrees to make it easier to switch between standalone / web player
	if (isCompressedLZMAStreamFile && IsPCStandaloneTargetPlatform(platform) && !IsMetroTargetPlatform(platform))
		writeTypeTrees = true;

	if (!writeTypeTrees)
		flags |= kDisableWriteTypeTree;
	
	// Compressed lzma stream files don't support streaming resources.
	if (!isCompressedLZMAStreamFile)
		flags |= kBuildResourceImage;
	
	AssertIf((flags & kBuildResourceImage) && IsWebPlayerTargetPlatform(platform));
	
	return flags;
}

void BuildPlayerDataUnityWebStream (const vector<UnityStr>& scenePathNames,
									const std::set<std::string>& resourceFiles,
									BuildTargetPlatform platform,
									BuildPlayerOptions options,
									int finalStreamedLevelWithResources,
									const Vector2f& progressInterval,
									bool isStreamedAdditionScene,
									UInt32 *outCRC
									)
{
	CompressedFiles assetBundleFiles;

	// Put assets & level data files into unitywebstream
	for (int i=0;i<scenePathNames.size ();i++)
	{
		string destinationPath = BuildLevelPath(i, scenePathNames[i], options);
		string assetPath = BuildAssetsFilePath(i,scenePathNames[i], options);
		
		// Make sure we add the first scene's assets file as the first in the container
		if (!AddToStreamFileIfExists(AppendPathName (kTemporaryDataFolder, assetPath), i, assetBundleFiles))
			throw string ("Building stream file failed.");
		
		// Also, if we have unity_builtin_extra resources for this player,
		// put them before the first scene itself since it will contain resources
		// shared by the whole player.
		//
		// Don't put extra resources in streamed additional scenes, though.
		// Must only be included on player webstream itself.
		if (!isStreamedAdditionScene && i == 0)
		{
			string extraResourcesPathLowerCase = ToLower (string (kDefaultExtraResourcesPath));
			AddToStreamFileIfExists (
				AppendPathName (kTemporaryDataFolder, kDefaultExtraResourcesPath),
				0,
				assetBundleFiles,
				extraResourcesPathLowerCase.c_str ());
		}

		if (!AddToStreamFileIfExists(AppendPathName (kTemporaryDataFolder, destinationPath), i, assetBundleFiles))
			throw string ("Building stream file failed.");
	}

	// Put resource files into unitywebstream
	for (std::set<std::string>::const_iterator it = resourceFiles.begin(); it != resourceFiles.end(); ++it)
	{
		if (!AddToStreamFileIfExists(AppendPathName (kTemporaryDataFolder, *it), finalStreamedLevelWithResources, assetBundleFiles))
			throw string ("Building stream file failed.");
	}
	
	// Inject all dll's
	set<string> paths;
	GetFolderContentsAtPath(kManagedDllsFolder, paths);
	for (set<string>::iterator i=paths.begin();i != paths.end();i++)
	{
		string path = *i;
		string ext = GetPathNameExtension(path);
		if (StrICmp (ext, "dll") == 0 || StrICmp (ext, "mdb") == 0)
		{
			AssertIf(StrICmp (ext, "mdb") == 0 && (options & kDevelopmentBuild) == 0);
			if (!AddToStreamFile(path, 0, assetBundleFiles))
				throw string ("Building stream file failed.");
		}
	}
	
	// Verify that we included all necessary files in the build
	GetFolderContentsAtPath(kTemporaryDataFolder, paths);
	paths.erase(kManagedDllsFolder);
	
	//Assert(assetBundleFiles.size() == paths.size());
	
	int numberOfLevelsToLoadFirst = platform == kBuildWebPlayerLZMAStreamed ? 1 : scenePathNames.size();
	
	DisplayBuildProgressbarThrowOnCancel (kBuildingPlayerProgressTitle, "Combining Unityweb stream", progressInterval);
	
	string buildError;
	if (options & kBuildPlayerUncompressed)
	{	
		// Note: The files which are in 'assetBundleFiles' list won't be compressed
		buildError = BuildUncompressedUnityWebStream("Temp/unitystream.unity3d", assetBundleFiles, scenePathNames, kBuildingPlayerProgressTitle, numberOfLevelsToLoadFirst, outCRC);
	}
	else
	{
		buildError = BuildUnityWebStream("Temp/unitystream.unity3d", assetBundleFiles, scenePathNames, kBuildingPlayerProgressTitle, numberOfLevelsToLoadFirst, outCRC);
	}
	if (!buildError.empty())
		throw buildError;
}

static Vector2f GetProgressInterval(int &step, float start, float increment)
{
	float v1 = step*increment + start;
	step++;
	float v2 = step*increment + start;
	return Vector2f (v1, v2);

}

static void CalculatePlayerDataSizeInfo (const vector<UnityStr>& scenePathNames, const std::set<std::string>& resourceFiles, const InstanceIDToBuildAsset& assets, int options, FileSizeInfo& output)
{
	// Apply sizeInfo on all assets
	TemporaryAssetLookup assetLookup = TemporaryFileToAssetPathMap(assets);
	for (int i=0;i<scenePathNames.size ();i++)
	{
		string assetPath = BuildAssetsFilePath(i, scenePathNames[i], options);
		GetPersistentManager().SetPathRemap (assetPath, AppendPathName (kTemporaryDataFolder, assetPath));
		output.AddAssetFileSize  (assetPath, assetLookup);
		GetPersistentManager().SetPathRemap (assetPath, "");
		
		string levelPath = BuildLevelPath(i, scenePathNames[i], options);
		GetPersistentManager().SetPathRemap (levelPath, AppendPathName (kTemporaryDataFolder, levelPath));
		output.AddLevelFileSize  (levelPath);
		GetPersistentManager().SetPathRemap (levelPath, "");
	}

	for (std::set<std::string>::const_iterator it = resourceFiles.begin(); it != resourceFiles.end(); ++it)
	{
		GetPersistentManager().SetPathRemap (*it, AppendPathName (kTemporaryDataFolder, *it));
		output.AddAssetFileSize  (*it, assetLookup);
		GetPersistentManager().SetPathRemap (*it, "");
	}
}

string BuildPlayerData (BuildTargetPlatform platform, BuildPlayerOptions options, vector<UnityStr> scenePathNames, const string& inCurrentScene, MonoObject*& usedClasses, Vector2f progressInterval, UInt32 *outCRC)
{
	FileSizeInfo sizeInfo;
	
	// Rebuild resources
	GetResourceManager().RebuildResources();
	
	// Backup the current scene
	string userOpenScene = inCurrentScene;

	string returnError;
	
	// All used class Ids
	set<int> usedClassIds;
	
	// Startup building game scenes
	
	InstanceIDToBuildAsset assets;
	if (!gAssetDependenciesStack.empty())
		assets = gAssetDependenciesStack.top();
	
	map<string, int> includedAssetSizes;
	
	string editorScenePath, destinationPath;
	vector<UnityStr> oldAssemblyNames;
	oldAssemblyNames = GetMonoManager().GetRawAssemblyNames();
	bool wasCancelled = false;
	
	// Compile all scenes for the player, copy them into the applications package
	// The first scene in the list to be named kMainData
	// All others are named level0, level1, level2...
	try
	{
#if ENABLE_SPRITES
		if (!SpritePacker::RebuildAtlasCacheIfNeeded(platform, true, SpritePacker::kSPE_Normal, false))
			throw ProgressBarCancelled();
#endif

		bool isStreamedAdditionalScene = options & kBuildAdditionalStreamedScenes;
		bool includeResourcesDependencies = !isStreamedAdditionalScene;
		bool isCompressedLZMAStreamFile = IsWebPlayerTargetPlatform(platform) || isStreamedAdditionalScene;
		///@TODO: Move kSerializeGameRelease | kBuildPlayerOnlySerializeBuildProperties out of buildCopyGameScene and drive it from here!
		TransferInstructionFlags endianAndOptimizationFlag = CalculatePlatformOptimizationFlags(platform, isCompressedLZMAStreamFile);
		bool needScriptCompatibilityInfo = isStreamedAdditionalScene && (endianAndOptimizationFlag & kDisableWriteTypeTree) != 0;
		if (needScriptCompatibilityInfo)
			endianAndOptimizationFlag |= kSerializedAssetBundleVersion;

		// Progress bar setup
		int numProgressSteps = scenePathNames.size ()*2 + 2 + (isCompressedLZMAStreamFile ? 1 : 0);
		float incrementProgress = (progressInterval.y - progressInterval.x) / numProgressSteps;
		int step = 0;

		// Use the backup scene instead of the current open scene.
		// This way we get the latest changes if the user hasn't saved yet.
		vector<UnityStr> actualScenePathNames = scenePathNames;
		for (int i=0;i<actualScenePathNames.size ();i++)
		{
			if (StrICmp (inCurrentScene.c_str(), actualScenePathNames[i]) == 0)
				actualScenePathNames[i] = kCurrentSceneBackupPath;
		}

		AssetBundle* assetBundle = NULL;
			
		TransferInstructionFlags firstSceneFlags = endianAndOptimizationFlag;
		if (!isStreamedAdditionalScene)
			firstSceneFlags |= kSaveGlobalManagers;
		
		string assetPath, destinationPath;
		
		// Create array of PreloadData objects that gets filled by BuildCopyGameScene
		vector<PreloadData*> preloadDataArray;
		preloadDataArray.resize(scenePathNames.size());
		for (int i=0;i<preloadDataArray.size();i++)
		{
			string assetPath = BuildAssetsFilePath(i, scenePathNames[i], options);
		
			PreloadData* preloadData = CreateObjectFromCode<PreloadData>();
			preloadData->SetHideFlags(Object::kHideAndDontSave);
			preloadDataArray[i] = preloadData;
			AddBuildAssetInfo (preloadData->GetInstanceID(), assetPath, gBuildPackageSortIndex, assets);
		}
		
		std::set<std::string> resourceFiles;
		bool splitResourceFiles = platform == kBuild_Android;

		// Only the web player uses "First Streamed Level" - all other targets should use the last scene.
		// This prevents assets from being sorted to the resources.assets file, when they are actually referenced implicitly by a scene.
		int firstStreamedLevel = IsWebPlayerTargetPlatform(platform) ? GetPlayerSettings().firstStreamedLevelWithResources : scenePathNames.size() - 1;
		int finalStreamedLevelWithResources = std::min<int>( std::max<int>(0, firstStreamedLevel), scenePathNames.size() - 1);
		
		// Calculate dependencies & build scene files
		for (int i=0;i<scenePathNames.size ();i++)
		{
			DisplayBuildProgressbarThrowOnCancel (kBuildingPlayerProgressTitle, Format("Building level %d", i), GetProgressInterval(step, progressInterval.x, incrementProgress));
		
			destinationPath = BuildLevelPath(i, scenePathNames[i], options);
			assetPath = BuildAssetsFilePath(i,scenePathNames[i], options);
			
			ResetSerializedFileAtPath (assetPath);
			
			gBuildPackageSortIndex++;
			
			TransferInstructionFlags optimizationFlag = endianAndOptimizationFlag;
			BuildCopyOptions buildOption = kDependenciesAndBuild;
			
			// * The first level needs to include Global managers in the scene
			// * We can't write the first level immediately since the managers might have references to assets included by later levels (For example the Shader ScriptMapper) 
			if (i == 0)
			{
				optimizationFlag = firstSceneFlags;
				buildOption = kDependenciesOnly;
			}

			// Inject an AssetBundle object if we're building streaming levels.
			// We add the object to the first .shaderAssets file only and store hashes for scripts
			// and runtime classes that are used in all scenes being written.
			if (isStreamedAdditionalScene && !assetBundle)
			{
				assetBundle = CreateObjectFromCode<AssetBundle>();
				assetBundle->SetHideFlags (Object::kHideAndDontSave);
			
				AddBuildAssetInfo(assetBundle->GetInstanceID(), assetPath, gBuildPackageSortIndex, assets);
			}

			BuildCopyGameSceneInput input;
			input.originalScenePath = scenePathNames[i];
			input.editorScenePath = actualScenePathNames[i];
			input.destinationPath = destinationPath;
			input.assetsPath = assetPath;
			input.buildPackageSortIndex = gBuildPackageSortIndex;
			input.platform = platform;
			input.options = optimizationFlag;
			input.buildOption = buildOption;
			BuildCopyGameScene (input, assets, usedClassIds, preloadDataArray[i]);

			if (finalStreamedLevelWithResources == i && includeResourcesDependencies)
			{
				AssertIf((options & kBuildAdditionalStreamedScenes));
				if (!CompileGameResourceManagerDependencies(kResourcesAssetPath, gBuildPackageSortIndex, platform, assets, usedClassIds, resourceFiles, splitResourceFiles))
					throw string ("Couldn't build player because of unsupported data on target platform");
			}
		}
		
		DisplayBuildProgressbarThrowOnCancel (kBuildingPlayerProgressTitle, "Building Resources folder", GetProgressInterval(step, progressInterval.x, incrementProgress));
		
		// Copy script dll's and modify custom dll names
		vector<UnityStr> playerScriptDLLs;
		if ((options & kBuildAdditionalStreamedScenes) == 0)
		{
			usedClasses = CreateUsedClassesRegistry(platform, usedClassIds);
			if (!BuildPlayerScriptDLLs (platform, sizeInfo, usedClasses, options, playerScriptDLLs))
				throw string ("Building player scripts failed.");
		}
		
		UpdateBuildSettingsWithPlatformSpecificData(platform);
		
		// Make sure there is nothing loaded (Reduces the number of assets that need to be in memory while writing assets)
		GetApplication().NewScene();

		for (std::set<std::string>::const_iterator it = resourceFiles.begin(); it != resourceFiles.end(); ++it)
			BuildSharedAssetsFile ((*it), assets, platform, endianAndOptimizationFlag);

		UnloadUnusedAssetsImmediate(true);

		DisplayBuildProgressbarThrowOnCancel (kBuildingPlayerProgressTitle, Format("Building scene %d", 0), GetProgressInterval(step, progressInterval.x, incrementProgress));
		
		// Build mainData
		// Have to build it last since it has the script mapper which
		// requires that all the scripts that will be needed are assembled when writing
		BuildCopyGameSceneInput input;
		input.originalScenePath = scenePathNames[0];
		input.editorScenePath = actualScenePathNames[0];
		input.destinationPath = BuildLevelPath(0, scenePathNames[0], options);
		input.assetsPath = BuildAssetsFilePath(0, scenePathNames[0], options);
		input.buildPackageSortIndex = gBuildPackageSortIndex;
		input.platform = platform;
		input.options = firstSceneFlags;
		input.buildOption = kBuildOnly;
		input.playerAssemblyNames = playerScriptDLLs;
		BuildCopyGameScene ( input, assets, usedClassIds, NULL);

		if (needScriptCompatibilityInfo)
		{
			CreateScriptCompatibilityInfo (*assetBundle, assets, "");
			
			std::vector<SInt32> used (usedClassIds.begin (), usedClassIds.end ());
			used.push_back (ClassID (PreloadData));
			assetBundle->FillHashTableForRuntimeClasses (used, endianAndOptimizationFlag);
		}
		
		// Make sure there is nothing loaded (Reduces the number of assets that need to be in memory while writing assets)
		GetApplication().NewScene();
		
		// Write asset files
		for (int i=0;i<scenePathNames.size ();i++)
		{
			DisplayBuildProgressbarThrowOnCancel (kBuildingPlayerProgressTitle, Format("Building assets for scene %d", i), GetProgressInterval(step, progressInterval.x, incrementProgress));
		
			assetPath = BuildAssetsFilePath(i,scenePathNames[i], options);
		
			// Prepare preload data
			PreloadData* preloadData = preloadDataArray[i];
			preloadData->SetHideFlags(0);
			SortPreloadAssetsByFileID(preloadData->m_Assets, assets);
		
			// Build shared assets file
			BuildSharedAssetsFile(assetPath, assets, platform, endianAndOptimizationFlag);

			// Cleanup PreloadData
			assets.erase(preloadData->GetInstanceID());
			DestroySingleObject(preloadData);
			
			UnloadUnusedAssetsImmediate(true);
		}	
		
		if (assetBundle)
		{
			assets.erase(assetBundle->GetInstanceID());
			DestroySingleObject(assetBundle);
		}
		
		AddNativeClassesToUsedClassesRegistry(usedClasses, usedClassIds);
		
		// Verify that no scene files / asset files stay loaded
		for (int i=0;i<scenePathNames.size ();i++)
		{
			Assert(!GetPersistentManager().IsStreamLoaded(BuildLevelPath(i, scenePathNames[i], options)));
			Assert(!GetPersistentManager().IsStreamLoaded(BuildAssetsFilePath(i, scenePathNames[i], options)));
		}
		Assert(!GetPersistentManager().IsStreamLoaded(kResourcesAssetPath));
		for (std::set<std::string>::const_iterator it = resourceFiles.begin(); it != resourceFiles.end(); ++it)
		{
			Assert(!GetPersistentManager().IsStreamLoaded(*it));
		}
		
		CalculatePlayerDataSizeInfo (scenePathNames, resourceFiles, assets, options, sizeInfo);

		// Write always included shaders to Resources/unity_builtin_extra.  These are accessible
		// from there by both the player's own data as well as by bundles built for the player.
		// Need to do this before building the webstream as it will pick up the generated file.
		BuildExtraResourcesBundleForPlayer (platform, kTemporaryDataFolder);

		// Write folder contents for this level to stream

		// Build one compressed lzma stream file out of the generated files.
		if (isCompressedLZMAStreamFile)
			BuildPlayerDataUnityWebStream(
				scenePathNames,
				resourceFiles,
				platform,
				options,
				finalStreamedLevelWithResources,
				GetProgressInterval(step, progressInterval.x, incrementProgress),
				isStreamedAdditionalScene,
				outCRC);
		// ToDo: Someone delete uncompressed asset bundle loading, restore it !!!
			
		AddDebugBuildFiles(kTemporaryDataFolder, platform, options);
		
		///@TODO: This triggers for player building asset bundles. The "BuildPlayer-mySceneName.sharedAssets" (For now disabled everywhere except 360)
		///@TODO: Tomas said he will make asset bundle building use shorts file names.
		if (platform == kBuildXBOX360)
			VerifyAllBuildFiles(kTemporaryDataFolder);
			
		VerifyAllAssetsHaveAssignedTemporaryLocalIdentifierInFile(assets);
		
		// Print useful information on what was included in the build
		// and what assets take up space
		sizeInfo.Print ();

		// This catches if anyone add/removes a call to DisplayBuildProgressbar without updating the numProgressSteps counter.
		if (step != numProgressSteps)
			LogBuildError(Format("BuildPlayerData progress mismatch - Ensure that check that 'numProgressSteps' (%d) matches with the number of calls to 'DisplayBuildProgressbar' (%d)", step, numProgressSteps));
	}
	catch (ProgressBarCancelled) {
		wasCancelled = true;
	}
	catch (const string& error)
	{
		returnError = error;
	}
	
	GetResourceManager ().ClearDependencyInfo ();
	GetMonoManager().GetRawAssemblyNames() = oldAssemblyNames;
	
	if (!gAssetDependenciesStack.empty())
		gAssetDependenciesStack.top() = assets;
	
	if (wasCancelled)
		throw ProgressBarCancelled ();
	
	return returnError;
}

bool DoBuildNaClResourcesWebStream (bool nophysx)
{			
	try
	{
		string suffix = nophysx ? "_nophysx" : "";
		CompressedFiles compressedFiles;
												
		AddToStreamFile(AppendPathName(GetPlaybackEngineDirectory(kBuildNaCl, 0), "Managed"+suffix+"/UnityEngine.dll"), 0, compressedFiles, "UnityEngine.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/MonoNaCl/builds/monodistribution/lib/mono/2.0/mscorlib.dll"), 0, compressedFiles, "mscorlib.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/MonoNaCl/builds/monodistribution/lib/mono/2.0/System.dll"), 0, compressedFiles, "System.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/MonoNaCl/builds/monodistribution/lib/mono/2.0/System.Core.dll"), 0, compressedFiles, "System.Core.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/MonoNaCl/builds/monodistribution/lib/mono/2.0/Mono.Security.dll"), 0, compressedFiles, "Mono.Security.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/Resources/builtin_resources/builds/unity default resources nacl"), 0, compressedFiles, kResourcePath);
								
		vector<UnityStr> outputPath;
		outputPath.push_back("nacl_resources");
		string buildError = BuildUnityWebStream(AppendPathName(GetPlaybackEngineDirectory(kBuildNaCl, 0), "unity_nacl_files/nacl_resources"+suffix+".unity3d"), compressedFiles, outputPath, kBuildingPlayerProgressTitle, 1, NULL);
		
		if (!buildError.empty())
			throw buildError;
	}
	catch (const string& error)
	{
		LogBuildError(error);
		return false;
	}
	
	return true;
}

bool BuildNaClWebPlayerResourcesWebStream ()
{
	try
	{
		CompressedFiles compressedFiles;
		string naclWebPlayerPath = AppendPathName(GetTargetsBuildFolder(), "NaclWebPlayer");
		AddToStreamFile(AppendPathName(naclWebPlayerPath, "Managed/UnityEngine.dll"), 0, compressedFiles, "UnityEngine.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/MonoBleedingEdge/builds/monodistribution/lib/mono/2.0/mscorlib.dll"), 0, compressedFiles, "mscorlib.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/Mono/builds/monodistribution/lib/mono/2.0/System.dll"), 0, compressedFiles, "System.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/Mono/builds/monodistribution/lib/mono/2.0/System.Core.dll"), 0, compressedFiles, "System.Core.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/Mono/builds/monodistribution/lib/mono/2.0/Mono.Security.dll"), 0, compressedFiles, "Mono.Security.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "build/MacEditor/Unity.app/Contents/Frameworks/Managed/CrossDomainPolicyParser.dll"), 0, compressedFiles, "CrossDomainPolicyParser.dll");
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/Resources/builtin_resources/builds/unity default resources nacl"), 0, compressedFiles, kResourcePath);
		AddToStreamFile(AppendPathName(GetBaseUnityDeveloperFolder(), "External/Resources/builtin_web_old/builds/unity_web_gl"), 0, compressedFiles, kOldWebResourcePath);
		
		vector<UnityStr> outputPath;
		outputPath.push_back("nacl_resources");
		string buildError = BuildUnityWebStream(AppendPathName(naclWebPlayerPath, "unity_nacl_files/nacl_resources.unity3d"), compressedFiles, outputPath, kBuildingPlayerProgressTitle, 1, NULL);
		
		if (!buildError.empty())
			throw buildError;
		
	}
	catch (const string& error)
	{
		LogBuildError(error);
		return false;
	}
	
	return true;
}

bool BuildNaClResourcesWebStream ()
{
	if (!DoBuildNaClResourcesWebStream (false))
		return false;
	if (IsFileCreated(AppendPathName(GetPlaybackEngineDirectory(kBuildNaCl, 0), "Managed/UnityEngine_nophysx.dll")))
		return DoBuildNaClResourcesWebStream (true);
	return true;
}

static void VerifyAllBuildFiles(string dataFolder)
{
	std::set<string> paths;
	GetDeepFolderContentsAtPath(dataFolder, paths);

	for (std::set<string>::iterator it = paths.begin(), eit = paths.end(); it != eit; ++it)
	{
		const string& path = *it;
		string fileName = GetLastPathNameComponent(path);
		AssertMsg(fileName.length() <= UNITY_RESOURCE_FILE_NAME_MAX_LEN, "File name too long (%s). Max length is %d characters", fileName.c_str(), (int)UNITY_RESOURCE_FILE_NAME_MAX_LEN);
	}
}

static std::string ChooseNetworkAdapter(BuildTargetPlatform target)
{
	if(target == kBuildFlash)
		return "127.0.0.1";

	char ips[10][16];
	int ipCount = GetIPs(ips);
	if (ipCount == 0)
		throw string("Could not GetIP.");

	// Compile a complete list of IPs since there is no way to know which 
	// NIC the connection is on for a machine with multiple networks
	std::string ipList;
	for (int i = 0; i < ipCount; ++i)
	{
		ipList += ips[i];
		ipList += " ";
	}

	return ipList;
}

static bool GetPlayerConnectionConfigurationToUse(BuildTargetPlatform target,int options, string& output)
{
	if (! options & kDevelopmentBuild) 
		return false;
	
	if (options & kBuildAdditionalStreamedScenes)
		return false;

	if (options & kConnectToHost)
	{
		std::string ip = ChooseNetworkAdapter(target);
		output = Format(PLAYER_CONNECTION_CONFIG_DATA_FORMAT_CONNECT, ip.c_str());
		return true;
	}

	if (!DoesBuildTargetSupportPlayerConnectionListening(target))
		return false;

	bool allowDebugging = options & kAllowDebugging;
	bool connectWithProfiler = options & kConnectWithProfiler;	

	//in the case of the user wanting to debug or profile the built player, force the player to wait on startup for a while, so we can profile/debug as early as possible.
	bool forcePlayerToWaitForConnectionOnStartup = allowDebugging || connectWithProfiler;

	// always write the playerconnection config file if in development mode
	output = Format(PLAYER_CONNECTION_CONFIG_DATA_FORMAT_LISTEN, (unsigned int)EditorConnection::Get().GetLocalGuid(), allowDebugging, forcePlayerToWaitForConnectionOnStartup, connectWithProfiler);
	return true;	
}

static void AddDebugBuildFiles(string dataFolder, BuildTargetPlatform target, int options)
{
	string playerConnectionConfigFile = AppendPathName(dataFolder, kPlayerConnectionConfigFile);
	
	string content;
	if (GetPlayerConnectionConfigurationToUse(target,options,content))
	{
		if (!WriteStringToFile(content, playerConnectionConfigFile, kNotAtomic, 0))
			throw Format("Unable to write to file %s", playerConnectionConfigFile.c_str());
	} else
	{
		DeleteFileOrDirectory(playerConnectionConfigFile);
	}

	if (options & kAutoRun)
		EditorConnection::Get().ResetLastPlayer();
}

bool IsPlatformOSXStandalone (int platform)
{
	return platform == kBuildStandaloneOSXIntel || platform == kBuildStandaloneOSXIntel64 || platform ==kBuildStandaloneOSXUniversal;
}

void CleanupBuildPlayer ()
{
	DeleteFileOrDirectory (kEditorAssembliesPath);
	DeleteFileOrDirectory (kLogicGraphCodeFile);
	CreateDirectorySafe(kEditorAssembliesPath);
	
	ResetUserEditableManagers();
	ForceRecompileAllScriptsAndDlls();

	DisplayBuildProgressbar ("", "", Vector2f(-1.0, -1.0f));
	ClearProgressbar();

	GetPreloadManager().LockPreloading();
	GarbageCollectSharedAssets(true);
	GetPreloadManager().UnlockPreloading();
}

inline string FailWithError (const string& error)
{
	LogBuildError(error);
	UnityBeep ();
	return error;
}

#if UNITY_WIN
static HANDLE s_XbEmulator = NULL;
string RunPlayerOnXbox360(const string& playerPath, int buildOptions)
{
	string xdkPath = getenv("XEDK");
	if(xdkPath == "")
		return FailWithError("No XDK installed. Deployment will not function!");

	string xProjectName = GetLastPathNameComponent(playerPath);
	string xdkBinPath = xdkPath + "\\bin\\win32\\";
	string cmdXbResetPath = xdkBinPath + "xbreboot.exe";
	string cmdXbCopyPath = xdkBinPath + "xbcp.exe";
	string cmdXbEmulatePath = xdkBinPath + "xbemulate.exe";
	string cmdXbDeletePath = xdkBinPath + "xbdel.exe";
	string cmdLaunchPath = cmdXbResetPath;

	const XboxRunMethod method = GetEditorUserBuildSettings().GetSelectedXboxRunMethod();
	switch (method)
	{
	case kXboxRunMethodDiscEmuFast:
	case kXboxRunMethodDiscEmuAccurate:
		{
			string xgdPath = AppendPathName(playerPath, "_AutoLayout.XGD");

			DisplayProgressbar(kDeployingPlayerProgressTitle, "Generating disc layout.", 0.1f);
			{	
				string mediaPath = AppendPathName(playerPath, "Media/");

				string content = "<LAYOUT MAJORVERSION=\"2\" MINORVERSION=\"2\">\n";
				content += "    <AVATARASSETPACK INCLUDE=\"YES\"/>\n";
				content += "    <DISC NAME=\"\" LayoutType=\"XGD2\">\n";

				// Add root files
				std::set<string> filePaths;
				GetFolderContentsAtPath(playerPath, filePaths);
				for (std::set<string>::iterator it = filePaths.begin(); it != filePaths.end(); ++it)
				{
					string filePath = *it;
					if (IsDirectoryCreated(filePath))
						continue; // Skip folders

					string fileName = GetLastPathNameComponent(filePath);
					content += "        <ADD NAME=\"\" SOURCE=\"" + playerPath + "\" DEST=\"\\\" FILESPEC=\"" + fileName + "\" RECURSE=\"NO\" LAYER=\"ANY\" ALIGN=\"1\"/>\n";
				}

				// Add Media folder
				content += "        <ADD NAME=\"\" SOURCE=\"" + mediaPath + "\" DEST=\"\\Media\\\" FILESPEC=\"*.*\" RECURSE=\"YES\" LAYER=\"ANY\" ALIGN=\"1\"/>\n";

				content += "\n";
				content += "\n";
				content += "    </DISC>\n";
				content += "</LAYOUT>\n";

				File csFile;
				if (!csFile.Open(xgdPath, File::kWritePermission))
					return FailWithError("Could not open " + xgdPath + " for writing!");
				csFile.Write(content.c_str(), content.length());
				csFile.Close();
			}

			DisplayProgressbar(kDeployingPlayerProgressTitle, "Launching the emulator.", 0.3f);
			{
				string timingMode = (method == kXboxRunMethodDiscEmuFast) ? "none" : "accurate";

				std::string copyCmdArgs = "/media \"" + xgdPath + "\" /timingmode " + timingMode + " /emulate start /power on";
				vector<string> paramVec;
				paramVec.push_back(copyCmdArgs);

				DWORD exitCode = 0;
				LaunchTaskArray(cmdXbEmulatePath, 0, paramVec, false, std::string(), (UInt32*) &exitCode);

				if (exitCode > 0)
				{
					s_XbEmulator = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, exitCode);
				}
				else
				{
					// xbEmulate returns -1 on failure
					return FailWithError("Failed to launch the Xbox 360 Game Disc Emulator.");
				}
			}

			DisplayProgressbar(kDeployingPlayerProgressTitle, "Launching the game.", 0.9f);
			{
				std::string runCmdArgs = "\"dvd:\\" + xProjectName + ".xex\"";
				vector<string> paramVec;
				paramVec.push_back(runCmdArgs);
				if (!LaunchTaskArray(cmdXbResetPath, 0, paramVec, false))
					return FailWithError("Failed to launch the game.");
			}

			DisplayProgressbar(kDeployingPlayerProgressTitle, "Done.", 1.0f);
		}
		break;

	case kXboxRunMethodHDD:
		{
			// Reset the kit
			DisplayProgressbar(kDeployingPlayerProgressTitle, "Resetting the development kit.", 0.1f);
			{
				vector<string> paramVec;
				if (!LaunchTaskArray(cmdXbResetPath, 0, paramVec, false))
					return FailWithError("Failed to reset the Xbox 360 development kit.");
			}

			// Sleep (kit needs to reset)
			Sleep(2000);

			// Delete existing files
			DisplayProgressbar(kDeployingPlayerProgressTitle, "Deleting existing project files.", 0.28f);
			{
				std::string delCmdArgs = "/R /H /S /F \"Devkit:\\" + xProjectName;
				vector<string> paramVec;
				paramVec.push_back(delCmdArgs);
				LaunchTaskArray(cmdXbDeletePath, 0, paramVec, false);
			}

			// Copy the project
			DisplayProgressbar(kDeployingPlayerProgressTitle, "Deploying project files.", 0.3f);
			{
				std::string copyCmdArgs = "/R /H /Y /C /T /F \"" + playerPath + "\" \"Devkit:\\" + xProjectName + "\"";
				vector<string> paramVec;
				paramVec.push_back(copyCmdArgs);
				if (!LaunchTaskArray(cmdXbCopyPath, 0, paramVec, false))
					return FailWithError("Failed to deploy project files on to the the Xbox 360 development kit.");
			}

			// Run it
			DisplayProgressbar(kDeployingPlayerProgressTitle, "Launching the game.", 0.9f);
			{
				std::string runCmdArgs = "\"Devkit:\\" + xProjectName + "\\" + xProjectName + ".xex\"";
				vector<string> paramVec;
				paramVec.push_back(runCmdArgs);
				if (!LaunchTaskArray(cmdXbResetPath, 0, paramVec, false))
					return FailWithError("Failed to launch the game.");
			}

			DisplayProgressbar(kDeployingPlayerProgressTitle, "Done.", 1.0f);
		}
		break;

	default:
		Assert(false);
	};

	return string();
}

void CloseXbox360Emulator()
{
	if (s_XbEmulator)
	{
		DWORD exitCode = 0;
		if (GetExitCodeProcess(s_XbEmulator, &exitCode) == TRUE)
		{
			if (exitCode == STILL_ACTIVE)
			{
				// Still running
				string cmdXbEmulatePath = std::string(getenv("XEDK")) + "\\bin\\win32\\xbemulate.exe";

				DisplayProgressbar(kBuildingPlayerProgressTitle, "Shutting down emulator.", 0.15f);
				{
					vector<string> paramVec;
					paramVec.push_back("/Process " + IntToString(GetProcessId(s_XbEmulator)) + " /Quit");
					LaunchTaskArray(cmdXbEmulatePath, 0, paramVec, false);
				}
			}
		}
		
		CloseHandle(s_XbEmulator);
		s_XbEmulator = 0;
	}
}

#endif // UNITY_WIN
 
void CleanupBeforePlayerBuild()
{
#if UNITY_WIN
	CloseXbox360Emulator();
#endif
}



static void ReportAnalyticsAtBuildPlayerTime(const std::string& platformName)
{
	static const char* kRenderingPathNames[kRenderPathCount] = {
		"VertexLit",
		"Forward",
		"DeferredLighting",
	};
	static const char* kColorSpaceNames[kMaxColorSpace] = {
		"Gamma",
		"Linear",
	};
	static const char* kGfxDeviceNames[kGfxRendererCount] = {
		"OpenGL",
		"Direct3D 9",
		"Direct3D 11",
		"PS3 GCM",
		"Null",
		"Wii",
		"Xbox 360",
		"OpenGL ES 1.x Deprecated",
		"OpenGL ES 2.x Mobile",
		"Flash Stage3D",
		"OpenGL ES 2.x Desktop",
		"OpenGL ES 3.x",
	};
									   
	PlayerSettings& playerSettings = GetPlayerSettings();
	AnalyticsTrackEvent("Graphics", "RenderingPath-"+platformName, kRenderingPathNames[playerSettings.GetRenderingPath()], 1);
	AnalyticsTrackEvent("Graphics", "MobileRenderingPath-"+platformName, kRenderingPathNames[playerSettings.GetMobileRenderingPath()], 1);
	AnalyticsTrackEvent("Graphics", "ColorSpace-"+platformName, kColorSpaceNames[playerSettings.GetValidatedColorSpace()], 1);
	AnalyticsTrackEvent("Graphics", "UseDX11-"+platformName, playerSettings.GetUseDX11() ? "DX11 enabled" : "DX11 disabled", 1);
	AnalyticsTrackEvent("Graphics", "GfxDevice-"+platformName, kGfxDeviceNames[GetGfxDevice().GetRenderer()], 1);
}

static string DoBuildPlayer (BuildPlayerSetup& setup)
{
	string error;
	
	std::string buildTargetPlatformName = GetBuildTargetName(setup.platform, setup.options);
	BuildPlayerOptions buildOptions = setup.options;
	BuildTargetPlatform platform = setup.platform;
	const std::string& playerPath = setup.path;
	std::vector<UnityStr>& levels = setup.levels;
	for( size_t i = 0; i < levels.size(); ++i)
		ConvertSeparatorsToUnity (levels[i]);
	
	cil::TypeDB scriptTypesInEditor;
	GenerateAssemblyTypeInfos (scriptTypesInEditor);
	
	if (IsWebPlayerTargetPlatform (platform) && (buildOptions & kDeployOnline) && !GetUploadingBuildsManager ().ValidateSession ())
		return FailWithError ("Must be logged into UDN in order to publish to it.");
	
	// Verify install in builds folder
	if ((buildOptions & kInstallInBuildsFolder) != 0 && !IsDeveloperBuild(!IsMetroTargetPlatform(platform)
		#if INCLUDE_WP8SUPPORT
		&& (platform != kBuildWP8Player)
		#endif
		))
		return FailWithError ("Install In Builds Folder can only be used when building from source");
	
	// Do we have a playback engine directory we can use for building the player?
	if (!IsBuildTargetSupported(platform))
		return FailWithError("Building a player for the target is not supported in this Unity build.");
	
	// Verify license
	if (!BuildPlayerLicenseCheck (platform))
		return FailWithError("Building a player for the target is not allowed because your License does not include it.");
	
	try
	{
		// Prevent building over current project
		string playerPathUnity = playerPath;
		string cwdUnity = File::GetCurrentDirectory ();
		ConvertSeparatorsToUnity (playerPathUnity);
		ConvertSeparatorsToUnity (cwdUnity);
		if (playerPathUnity.compare (cwdUnity) == 0)
			return Format ("Can not build to %s (Build Path same as Project Path)", playerPath.c_str());
		
		for (int i=0;i<levels.size();i++)
		{
			if (!IsFileCreated (levels[i]) || !IsUnitySceneFile(levels[i]))
				return FailWithError(Format("'%s' is an incorrect path for a scene file. BuildPlayer expects paths relative to the project folder.", levels[i].c_str()));
		}
		
		if (levels.empty ())
			levels.push_back (GetApplication().GetCurrentScene());

		// Begin building...
		DisplayBuildProgressbarThrowOnCancel (kBuildingPlayerProgressTitle, GetBuildProgressText (kCompilingScripts), GetBuildProgressInterval (kCompilingScripts, platform) );

		// Cleanup before build, report analytics
		CleanupBeforePlayerBuild();
		ReportAnalyticsAtBuildPlayerTime(buildTargetPlatformName);

		// Copy old script assemblies back
		DeleteFileOrDirectory(kEditorAssembliesPath);
		CreateDirectorySafe (kEditorAssembliesPath);
		
		// Recompile 
		DirtyAllScriptCompilers();
		int compileFlags = kCompileSynchronous | kDontLoadAssemblies;
		if (buildOptions & kDevelopmentBuild)
			compileFlags |= kCompileDevelopmentBuild;
		
		printf_console("building target %d\n", platform);

		// Try to load platform extensions, because they might be not loaded, if user builds
		// a player via script, for ex., using -executeMethod command line argument (see Case 570115)
		LoadPlatformSupportModule(platform);

		RecompileScripts(compileFlags, false, platform, false);
		if (GetMonoManager().HasCompileErrors())
			return FailWithError ("Error building Player because scripts had compiler errors");
		
		// Use Cecil to fetch type information from the compiled dlls
		cil::TypeDB scriptTypesOnTargetPlatform;

		GenerateAssemblyTypeInfos (scriptTypesOnTargetPlatform);

		if (IsMetroTargetPlatform(platform))
		{
			PostProcessAssemblyTypeInfos (scriptTypesOnTargetPlatform);
		}

		//	printf_console ("SOURCE TYPE DB:\n"); scriptTypesInEditor.Dump ();
		//	printf_console ("DESTINATION TYPE DB:\n"); scriptTypesOnTargetPlatform.Dump ();

		// Check that the target doesn't have more properties than the editor as that is not supported
		cil::ExtraFieldTester extraFieldTester (scriptTypesInEditor, scriptTypesOnTargetPlatform);
		if (extraFieldTester.HasExtra ())
		{
			extraFieldTester.PrintErrorsToConsole ();
			return FailWithError ("Error building player because script class layout is incompatible between the editor and the player.");
		}


		if (DoesBuildTargetWantCodeGeneratedSerialization(platform))
		{
			WeaveSerializationCodeIntoAssemblies();
		}
		
		// Switch build target (compress textures that aren't compressed in the right format yet)
		SwitchActiveBuildTargetForBuild(platform);
		PrepareQualitySettingsForBuild(platform);
		
		// Get the scene path names
		UpdateBuildSettings (levels, "", false, buildOptions);
		
		// All used classes registry
		MonoObject* usedClasses = NULL;
		
		// Build player
		{
			cil::AutoResetTargetTypeDB autoResetTargetTypeDB (&scriptTypesOnTargetPlatform);
			error = BuildPlayerData (platform, buildOptions, levels, GetApplication().GetCurrentScene(), usedClasses, GetBuildProgressInterval (kBuildPlayer, platform), setup.outCRC);
		}
		
		if (!error.empty ())
			return FailWithError("Error building Player: " + error);
		
		// Postprocess player data
		DisplayBuildProgressbarThrowOnCancel (kBuildingPlayerProgressTitle, GetBuildProgressText (kPostprocessing), GetBuildProgressInterval (kPostprocessing, platform) );

		error = PostprocessPlayer (playerPath, platform, buildOptions, usedClasses);
		if (!error.empty ())
			return FailWithError("Error building Player: " + error);

		DisplayBuildProgressbarThrowOnCancel (kBuildingPlayerProgressTitle, GetBuildProgressText (kDone), GetBuildProgressInterval (kDone, platform) );
		
		// Uploading of online deployment of webplayer	
		if (IsWebPlayerTargetPlatform (platform) && (buildOptions & kDeployOnline))
		{
			GetUploadingBuildsManager ().BeginUploadBuild (playerPath, buildOptions & kAutoRun);
		}

		if((buildOptions & kConnectWithProfiler) && (buildOptions & kAutoRun) && (buildOptions & kInstallInBuildsFolder) == 0)
		{
			CallStaticMonoMethod("CreateBuiltinWindows", "ShowProfilerWindow");
		}

		// Auto run (Dont do it when installing in build folder for debugging)
		if ((buildOptions & kAutoRun) && (buildOptions & kInstallInBuildsFolder) == 0)
		{
			if (platform == kBuildWebPlayerLZMA || platform == kBuildWebPlayerLZMAStreamed )
			{
				string htmlFile;
				if (!(buildOptions & kDeployOnline))
				{
					htmlFile = AppendPathName (playerPath, GetLastPathNameComponent (playerPath));
					htmlFile = AppendPathNameExtension (htmlFile, "html");
					OpenWithDefaultApp (htmlFile);
				}
			}
	#if UNITY_WIN
			else if (platform == kBuildXBOX360)
			{
				string err = RunPlayerOnXbox360(playerPath, buildOptions);
				if (!err.empty())
					return err;
			}
			else if (platform == kBuildPS3)
			{
				string snPath = getenv("SN_PS3_PATH");
				if(snPath == "")
					return FailWithError("No ProDG installed. Deployment will not function!");

				string xProjectName = GetLastPathNameComponent(playerPath);
				string selfFile = playerPath + "\\PS3_GAME\\USRDIR\\EBOOT.BIN";
				string ps3runPath = "\"" + snPath + "\\bin\\ps3run.exe" + "\"";
				string ps3runArgs = " /f\""+playerPath+"\" " + " \"" + selfFile + "\"";
				string ps3runCmd = "cmd.exe /C \"" + ps3runPath + ps3runArgs + "\"";
				system(ps3runCmd.c_str());
			}
	#endif // UNITY_WIN
			else if (platform == kBuild_iPhone || platform == kBuildNaCl
				#if INCLUDE_WP8SUPPORT
				|| platform == kBuildWP8Player
				#endif
				)
			{
				DisplayProgressbar(kDeployingPlayerProgressTitle, "Deploying Player", 0.1f);

				if (platform == kBuildWP8Player)
				{
					vector<UInt32> players;
					EditorConnection::Get().GetAvailablePlayers(WP8Player, players);

					for (vector<UInt32>::const_iterator it = players.begin(); it != players.end(); ++it)
						EditorConnection::Get().EnablePlayer(*it, false);
				}

				string const productName = StripInvalidIdentifierCharacters(GetPlayerSettings().productName);

				error = CallMonoLaunchPlayer (platform, playerPath, productName, buildOptions);
				if (!error.empty())
				{
					LogBuildError(error);
					UnityBeep ();
				}

				DisplayProgressbar(kDeployingPlayerProgressTitle, "Done.", 1.0f);
			}
			else if ((platform == kBuild_Android) || (platform == kBuildFlash) || (platform == kBuildBB10))
			{
				; // Build processing is done in postprocessing script for android, flash and BB10
			}
			else
			{
				LaunchApplication(playerPath);
			}
		}
		
		// Select build (Dont do it when installing in build folder for debugging or when deploying online)
		if ((buildOptions & kSelectBuiltPlayer) && (buildOptions & kInstallInBuildsFolder) == 0 && (buildOptions & kDeployOnline) == 0)
		{
			// Select the generated player in the finder
			SelectFileWithFinder (playerPath);
		}
	} catch (ProgressBarCancelled)
	{
		error = "Building Player was cancelled";
		WarningString (error);
	}
	
	return error;
}


static bool s_IsBuildingPlayer = false;
string BuildPlayer (BuildPlayerSetup& setup)
{
	//--------------------------------------------------
	// Build setup
	//--------------------------------------------------
	s_IsBuildingPlayer = true;
	string originalOpenScene = GetApplication().GetCurrentScene();
	bool isSceneDirty = GetApplication().IsSceneDirty();
	
	RemoveErrorWithIdentifierFromConsole(GetBuildErrorIdentifier());
	GetApplication().SetIsPlaying(false);
	AnalyticsProcessTracker analyticsTracker("Build", "Player", GetBuildTargetName(setup.platform, setup.options));

	PlayerPrefs::Sync(); // Sync player prefs -- this way we ensure that any PlayerPrefs set in the editor will be visible when/if the player is launched
	StopAllCompilation();
	CheckForAssemblyFileNameMismatches();
	
	DeleteFileOrDirectory (kTempFolder);
	CreateDirectoryRecursive (kTemporaryDataFolder);
	
	// Save assets / save open scene in temp file (We don't want to auto save the scene)
	CreateDirectory (kTempFolder);
	AssetInterface::Get ().SaveAssets ();
	GetApplication().SaveSceneInternal(kCurrentSceneBackupPath, kNoTransferInstructionFlags);

	//--------------------------------------------------
	// Do the actual build (might fail or be cancelled)
	//--------------------------------------------------
	string result = DoBuildPlayer (setup);

	//--------------------------------------------------
	// Cleanup after build
	//--------------------------------------------------
	
	// Open backup scene
	GetApplication().LoadSceneInternal(kCurrentSceneBackupPath, 0);
	// Perform Update scene immediately.
	// This is to prevent Repaint Events happening before the PlayerLoop has been invoked at least once.
	// (Cocoa does this when you hit cmd-p to enter playmode)
	GetApplication().UpdateScene ();
	GetApplication().SetOpenPath (originalOpenScene);
	GetApplication().SetSceneDirty (isSceneDirty);

	UnloadUnusedAssetsImmediate(true);
	CleanupBuildPlayer();
	
	if (result.empty ())
		analyticsTracker.Succeeded();
	
	s_IsBuildingPlayer = false;
	return result;
}

bool IsBuildingPlayer ()
{
	return s_IsBuildingPlayer;
}

bool GetScriptsHaveMouseEvents(BuildTargetPlatform platform)
{
	if (platform == kBuild_Android || 
		platform == kBuild_iPhone ||
		platform == kBuildMetroPlayerX86 || 
		platform == kBuildMetroPlayerARM ||
		platform == kBuildMetroPlayerX64 ||
		platform == kBuildWP8Player ||
		platform == kBuildBB10 ||
		platform == kBuildTizen)
	{
		void* params[] = {scripting_string_new(kManagedDllsFolder)};
		bool res = MonoObjectToBool(CallStaticMonoMethod("AssemblyReferenceChecker", "GetScriptsHaveMouseEvents", params));
		
		if (res)
			WarningString("Game scripts or other custom code contains OnMouse_ event handlers. Presence of such handlers might impact performance on handheld devices.");
		
		return res;
	}
	
	return true;
}

void UpdateBuildSettingsWithPlatformSpecificData(BuildTargetPlatform platform)
{
	BuildSettings& settings = GetBuildSettings();
	settings.usesOnMouseEvents = GetScriptsHaveMouseEvents(platform);
}

int UpdateBuildSettings (const vector<UnityStr>& inLevels, const string& remapOpenScene, bool forceIncludeOpenScene, int options)
{
	vector<UnityStr> levels = inLevels;
	vector<UnityStr> remapped = inLevels;
	int openScene = -1;
	for (int i=0;i<remapped.size ();i++)
	{
		if (StrICmp (GetApplication().GetCurrentScene().c_str(), remapped[i]) == 0)
		{
			openScene = i;
			remapped[i] = remapOpenScene;
		}
	}
	
	if (openScene == -1 && forceIncludeOpenScene)
	{
		levels.insert(levels.begin(), GetApplication().GetCurrentScene());
		remapped.insert(remapped.begin(), remapOpenScene);
		openScene = 0;
	}
	
	BuildSettings& settings = GetBuildSettings();
	settings.levels = levels;
	settings.remappedLevels = remapped;
	settings.isDebugBuild = options & kDevelopmentBuild;
	return openScene;
}

std::string BuildPlayerWithSelectedLevels (const std::string& playerPath, BuildTargetPlatform platform, BuildPlayerOptions options)
{
	BuildPlayerSetup setup;
	
	setup.levels = GetLevelPathNames ();
	setup.platform = platform;
	setup.options = options;
	setup.path = playerPath;

	return BuildPlayer (setup);
}

int UpdateBuildSettingsWithSelectedLevelsForPlaymode (const string& remapOpenScene)
{
	vector<UnityStr> levels = GetLevelPathNames ();
	int openScene = UpdateBuildSettings (levels, remapOpenScene, true, 0);
	GetBuildSettings().isDebugBuild = true;
	return openScene;
}

static vector<UnityStr> GetLevelPathNames ()
{
	EditorBuildSettings::Scenes scenes = GetEditorBuildSettings().GetScenes();
	vector<UnityStr> outPaths;
	for (int i=0;i<scenes.size();i++)
	{
		if (IsFileCreated (scenes[i].path) && scenes[i].enabled)
			outPaths.push_back (scenes[i].path);
	}
	
	if (outPaths.empty ())
		outPaths.push_back (GetApplication().GetCurrentScene());
	
	return outPaths;
}
