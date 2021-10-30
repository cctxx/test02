#ifndef BUILDPLAYERSERIALIZATION_H
#define BUILDPLAYERSERIALIZATION_H

#include "Configuration/UnityConfigure.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Editor/Src/Utility/FilesizeInfo.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Serialize/TransferFunctions/RemapPPtrTransfer.h"


class PreloadData;
class AssetBundle;

/// Temporary information about an asset object that is being included
/// in a specific build.
struct BuildAsset
{
	int  instanceID;
	int  classID;

	// Index of the file. When we have a dependency from one file to another shared asset or asset bundle, we have to still sort assets in the right preload order
	// For this we first sort by file (Order in which scenes / asset files are being built), then by fileID in that file.
	int                        buildPackageSortIndex;

	// Sort index inside of each serialized file to ensure assets load without dereferencing each other recursively.
	UInt64                     preloadSortIndex; 
	LocalIdentifierInFileType  originalLocalIdentifierInFile;
	UInt32                     originalDirtyIndex;
	
	mutable std::string                temporaryPathName;
	mutable SerializedObjectIdentifier temporaryObjectIdentifier;
	mutable BuildUsageTag              buildUsage;
};

typedef std::map<int, BuildAsset> InstanceIDToBuildAsset;
typedef vector_map<int, SerializedObjectIdentifier> InstanceIDBuildRemap;
typedef std::vector<WriteData> WriteDataArray;


/// GenerateIDFunctor that collects all instance IDs referenced by an object
/// graph when serialized in kSerializeGameRelease mode.
class GameReleaseCollector : public GenerateIDFunctor
{
	set<SInt32>* m_IDs;
	int          m_ExtraTransferInstructionFlags;

public:

	/// Construct collector that emits all collected IDs
	/// to the given set.
	GameReleaseCollector (set<SInt32>* ptrs, int extraTransferInstructionFlags = 0)
	{
		m_IDs = ptrs;
		m_ExtraTransferInstructionFlags = extraTransferInstructionFlags;
	}

	virtual ~GameReleaseCollector () {}

	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag = kNoTransferFlags);
};

/// GenerateIDFunctor that collects instanceIDs and GUIDs of all objects that
/// are contained in the same asset when serialized in kSerializeGameRelease mode.
class GameReleaseDependenciesSameAsset : public GenerateIDFunctor
{
	set<SInt32>* m_IDs;
	set<SInt32> m_SameAssetIDs;
	UnityGUID m_Asset;

public:

	GameReleaseDependenciesSameAsset (set<SInt32>* ptrs, UnityGUID guid)
	{
		m_IDs = ptrs;
		m_Asset = guid;
	}

	virtual ~GameReleaseDependenciesSameAsset () {}

	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag = kNoTransferFlags);
};

int GetClassIDWithoutLoadingObject (int instanceID);

/// options: kSaveGlobalManager, kSwapEndianess
bool CompileGameScene (const std::string& pathName, InstanceIDToBuildAsset& assets, std::set<int>& usedClassIDs, BuildTargetPlatform platform, int options, const std::vector<UnityStr>& playerAssemblyNames);
bool CompileGameSceneDependencies (const std::string& pathName, const std::string& assetPathName, int buildPackageSortIndex, InstanceIDToBuildAsset& assets, std::set<int>& usedClassIDs, PreloadData* preload, BuildTargetPlatform platform, int options);
bool CompileGameResourceManagerDependencies (const std::string& assetPathName, int buildPackageSortIndex, BuildTargetPlatform platform, InstanceIDToBuildAsset& assets, std::set<int>& usedClassIds, std::set<std::string>& outFilenames, bool splitResources);

BuildAsset& AddBuildAssetInfo (SInt32 instanceID, const string& temporaryPath, int buildPackageSortIndex, InstanceIDToBuildAsset& assets);
BuildAsset& AddBuildAssetInfoWithLocalIdentifier (SInt32 instanceID, const string& temporaryPath, int buildPackageSortIndex, LocalIdentifierInFileType fileID, InstanceIDToBuildAsset& assets);

/// Add BuildInfo to @a assets for the object identified by @a instanceID but first verify
/// that the given object is indeed an asset (i.e. a persistent object) and that
void AddBuildAssetInfoChecked (SInt32 instanceID, const string& temporaryPath, int buildPackageSortIndex, InstanceIDToBuildAsset& restore, LocalIdentifierInFileType fileID, bool allowBuiltinsResources);

bool CompileSharedAssetsFile (const std::string& assetPath, const InstanceIDToBuildAsset& assets, BuildTargetPlatform platform, int options);
std::string BuildCustomAssetBundle (PPtr<Object> mainAsset, const std::vector<PPtr<Object> >& objects, const std::vector<std::string>* overridePaths, const std::string& targetPath, int buildPackageSortIndex, InstanceIDToBuildAsset& assets, BuildTargetPlatform platform, TransferInstructionFlags flags, BuildAssetBundleOptions assetBundleOptions );
void CreateScriptCompatibilityInfo (AssetBundle& assetBundle, InstanceIDToBuildAsset& assets, std::string const& targetPath);

void AssignTemporaryLocalIdentifierInFileForAssets (const std::string& path, const InstanceIDToBuildAsset& assets);
void VerifyAllAssetsHaveAssignedTemporaryLocalIdentifierInFile (const InstanceIDToBuildAsset& assets);

// Assign temporary local identifiers to assets that were appended to the asset list after the main build process.
void AssignMissingTemporaryLocalIdentifierInFileForAssets (const std::string& path, InstanceIDToBuildAsset& assets);

void SortPreloadAssetsByFileID (std::vector<PPtr<Object> >& preload, InstanceIDToBuildAsset& assets);
void SortPreloadAssetsByFileID (std::vector<PPtr<Object> >& preload, int start, int size, InstanceIDToBuildAsset& assets);

TemporaryAssetLookup TemporaryFileToAssetPathMap (const InstanceIDToBuildAsset& assets);

void ClearEditorBuildAssetUsage ();
void ComputeObjectUsage (const std::set<SInt32>& objects, std::set<int>& classIDs, InstanceIDToBuildAsset& assets);
bool VerifyDeployment (Object* obj, BuildTargetPlatform buildTarget);

typedef UNITY_SET(kMemTempAlloc, Object*) TempSelectionSet;
void CollectAllDependencies (const TempSelectionSet& selection, TempSelectionSet& output );
std::set<UnityGUID> CollectAllDependencies (const std::set<UnityGUID>& selection);

bool ResetSerializedFileAtPath (const std::string& assetPath);

bool SaveScene (const std::string& pathName, std::map<LocalIdentifierInFileType, SInt32>* backupFileIDToHeapID, TransferInstructionFlags options);

bool ResolveInstanceIDMapping (SInt32 id, LocalSerializedObjectIdentifier& localIdentifier, const InstanceIDBuildRemap& assets);
void BuildingPlayerOrAssetBundleInstanceIDResolveCallback (SInt32 id, LocalSerializedObjectIdentifier& localIdentifier, void* context);

bool IsDefaultResourcesObject (int instanceID);
bool IsExtraResourcesObject (int instanceID);
bool IsAnyDefaultResourcesObject (int instanceID);
bool IsAlwaysIncludedShaderOrDependency (int instanceID);
bool IsClassIDSupportedInBuild (int classID);

bool WriteSharedAssetFile (const std::string& targetPath, const InstanceIDToBuildAsset& assets, BuildTargetSelection target, InstanceIDResolveCallback* resolveCallback, int flags);

#endif
