#include "UnityPrefix.h"
#include "Runtime/Utilities/File.h"
#include "RevertTask.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/MetaFileUtility.h"

RevertTask::RevertTask(VCAssetList const& assetlist, RevertMode revertMode) : m_inputList(assetlist), m_inputRevertMode(revertMode)
{
}

void RevertTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	const bool includeFolders = true;
	VCAssetList filteredAssets;
	m_inputList.Filter (filteredAssets, includeFolders, kCheckedOutLocal | kLockedLocal | kDeletedLocal | kAddedLocal);

	filteredAssets.IncludeMeta();

	// Force revert of folder if its associated .meta is in the list.
	filteredAssets.IncludeFolders();
	
	if (filteredAssets.empty())
	{
		m_Success = true;
		return;
	}

	string revertMode;
	if (m_inputRevertMode == kRevertUnchanged)
		revertMode = "unchangedOnly";
	else if (m_inputRevertMode == kRevertKeepModifications)
		revertMode = "keepLocalModifications";
	
	p.SendCommand("revert", revertMode);
	p << filteredAssets;
	p >> m_RevertResult;
	bool res = GetVCProvider().ReadPluginStatus();

	filteredAssets.clear();
	
	m_RevertResult.Filter (filteredAssets, includeFolders, kAny, kDeletedLocal); // only want anything that is not deleted local
	p.SendCommand("status");
	p << filteredAssets;
	p >> m_Assetlist;

	res &= GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	m_Success = res;
}

void RevertTask::Done()
{
	GUIDPersistentManager& gpm = GetGUIDPersistentManager();

	vector<UnityGUID> guidsToReimport;
	vector<UnityGUID> RemovedGUIDs;
	string reloadScene;

	AssetInterface::Get().StartAssetEditing();

	// Step 1: make sure to remove any deleted assets
	// Assets can be deleted during revert if a revert after a rename is being performed 
	for (int a = 0; a < m_RevertResult.size(); ++a)
	{
		VCAsset& vcAsset = m_RevertResult[a];

		if (!vcAsset.IsMeta() )
		{
			string assetPath = Trim(vcAsset.GetPath(), "/");
			UnityGUID guid;
			gpm.PathNameToGUID(assetPath, &guid);

			if (vcAsset.HasState(kDeletedLocal))
			{
				AssetDatabase::Get().RemoveAsset(guid, AssetDatabase::kRemoveCacheOnly, NULL);
				RemovedGUIDs.push_back(guid);
			}
		}
	}

	// Step 2: Find the guid of all assets to re-import
	for (int a = 0; a < m_RevertResult.size(); ++a)
	{
		VCAsset& vcAsset = m_RevertResult[a];

		if (!vcAsset.IsMeta() )
		{
			if (vcAsset.HasState(kSynced))
			{
				string assetPath = Trim(vcAsset.GetPath(), "/");
				UnityGUID guid;
				gpm.PathNameToGUID(assetPath, &guid);

				// If we are reverting the scene we need to reload it later
				if (assetPath == GetApplication().GetCurrentScene())
				{
					reloadScene = assetPath;
				}

				// If the GUID for the path name is not know it means we have reverted after renaming
				// We read the guid from the meta file in that case and tell the GUIDPersistentManager to assign the GUID to the asset
				if (!guid.IsValid())
				{
					guid = ReadGUIDFromTextMetaData(assetPath);
					Assert (std::find(RemovedGUIDs.begin(), RemovedGUIDs.end(), guid) != RemovedGUIDs.end());
					gpm.MoveAsset(guid, assetPath);
				}

				Assert(guid.IsValid());
				guidsToReimport.push_back(guid);
			}
		}
	}
	// Step 3: Re-import all reverted asset
	AssetInterface::Get().ImportAssets(guidsToReimport, kAssetWasModifiedOnDisk);
	AssetInterface::Get().StopAssetEditing();

	// Reload the scene if is was one of the assets being reverted
	if (!reloadScene.empty())
	{
		GetApplication().OpenScene(reloadScene);
	}

	// Enable auto refreshing again
	GetApplication().AllowAutoRefresh();

	VCTask::Done();
}
