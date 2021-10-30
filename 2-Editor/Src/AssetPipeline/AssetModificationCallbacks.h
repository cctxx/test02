#pragma once
#include "Runtime/Utilities/GUID.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/VersionControl/VCProvider.h"

class AssetModificationCallbacks
{	
public:
	enum AssetMoveResult {DidNotMove = 0,  FailedMove = 1, DidMove = 2};
	enum AssetDeleteResult {DidNotDelete = 0,  FailedDelete = 1, DidDelete = 2};

	// Return true if file is actually checked out. If promptIfCheckoutNeeded is empty a checkout is done
	// without prompting.
	static bool CheckoutIfRequired(const std::set<UnityGUID>& assets, const std::string& promptIfCheckoutNeeded);

	static void FileModeChanged(const std::set<UnityGUID>& assets, FileMode mode);
	
	static void WillCreateAssetFile (const std::string &path);
	static bool ShouldSaveSingleFile (const std::string &path);
	static void ShouldSaveAssets (const std::set<UnityGUID>& assets, std::set<UnityGUID>& outAssetsThatShouldBeSaved, std::set<UnityGUID>& outAssetsThatShouldBeReverted, bool explicitlySaveScene);
	static int WillMoveAsset( std::string const& fromPath, std::string const& toPath);
	static int WillDeleteAsset( std::string const& asset, AssetDatabase::RemoveAssetOptions options );
	static void OnStatusUpdated();

};
