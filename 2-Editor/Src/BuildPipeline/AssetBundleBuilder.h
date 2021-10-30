#pragma once

#include "Editor/Src/BuildPipeline/BuildSerialization.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/BaseClasses/GameObject.h"


/// Helper that encapsulates the build process for custom asset bundles.
class AssetBundleBuilder
{
public:

	AssetBundleBuilder (InstanceIDToBuildAsset& buildAssets)
		: m_BuildAssets (buildAssets) {}

	std::string BuildCustomAssetBundle (PPtr<Object> mainAsset,
										const vector<PPtr<Object> >& objects,
										const vector<string>* overridePaths,
										const string& targetPath,
										int buildPackageSortIndex,
										BuildTargetPlatform platform,
										TransferInstructionFlags transferFlags,
										BuildAssetBundleOptions assetBundleOptions);

private:

	/// GUIDs of all assets included in the bundle.
	///
	/// NOTE: This may include GUIDs for the default and extra resource "assets".
	std::set<UnityGUID> m_IncludedAssets;

	////FIXME: is it really necessary to have all objects loaded at the same time?

	/// Set of objects to persist in the bundle.
	std::set<Object*> m_IncludedObjects;

	/// Same as #m_IncludedObjects but restricted to just instance IDs.  Allows
	/// for fast queries on whether a particular ID
	std::set<SInt32> m_IncludedObjectIDs;

	/// Records for all the objects that we are writing to the final bundle.
	InstanceIDToBuildAsset& m_BuildAssets;

	/// @name Builds Steps
	/// @{

	void BuildInitialObjectSet (PPtr<Object> mainAsset, const vector<PPtr<Object> >& objects, bool buildDeterministicAssetBundle);
	void CollectDependencies ();
	void CollectInstanceIDsAndAssets ();
	void AddInstanceIDsOfCompleteAssets ();
	void VerifyComponentsAndChildTransformsArePresent ();
	void AddBuildAssets (const string& targetPath, int buildPackageSortIndex, bool buildDeterministicAssetBundle);
	void CreateAllAssetInfos (AssetBundle* assetBundle, PPtr<Object> mainAsset, const vector<PPtr<Object> >& objects, const vector<string>* overridePaths);

	/// @}

	/// @name Helper Methods
	/// @{

	BuildAsset& AddAssetToBuild (SInt32 instanceID, const string& temporaryPath, int buildPackageSortIndex)
	{
		return AddBuildAssetInfo (instanceID, temporaryPath, buildPackageSortIndex, m_BuildAssets);
	}

	void VerifyComponentsArePresent (GameObject* obj);
	void VerifyChildTransformsArePresent (GameObject* obj);

	void AddAssetInfoForAssetFromDatabase (AssetBundle* assetBundle, const UnityGUID& guid);
	void AddPreloadTableEntries (AssetBundle* assetBundle, AssetBundle::AssetInfo** assetInfos, int numAssetInfos);
	static AssetBundle::AssetInfo& AddAssetInfoToContainer (AssetBundle* assetBundle, PPtr<Object> object, const string& path);

	void AddPreloadTableEntries (AssetBundle* assetBundle, AssetBundle::AssetInfo& assetInfo)
	{
		AssetBundle::AssetInfo* assetInfoPtr = &assetInfo;
		AddPreloadTableEntries (assetBundle, &assetInfoPtr, 1);
	}

	/// @}
};
