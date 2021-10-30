#include "UnityPrefix.h"
#include "ResolveTask.h"

ResolveTask::ResolveTask(VCAssetList const& assetlist, ResolveMethod resolveMethod) : m_InputList(assetlist), m_ResolveMethod(resolveMethod)
{

}

void ResolveTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	VCAssetList filesRecursive;

	for (VCAssetList::const_iterator i = m_InputList.begin(); i != m_InputList.end(); ++i)
	{
		// Check each folders meta data as well as recursing their contents
		filesRecursive.AddRecursive(*i);
	}

	filesRecursive.IncludeMeta();

	// Cannot download a folder but only files so we strip folder assets.
	// Note that folder meta files are included never the less.
	const bool excludeFolders = false;
	VCAssetList filteredAssets;
	filesRecursive.Filter(filteredAssets, excludeFolders, kConflicted);

	if (filteredAssets.empty())
	{
		m_Success = true;
		return;
	}

	string m;
	switch (m_ResolveMethod)
	{
		case kUseMine:
			m = "mine";
			break;
		case kUseTheirs:
			m = "theirs";
			break;
		case kUseMerged:
			m = "merged";
			break;
		default:
			m = "mine";
			break;
	}
	p.SendCommand("resolve", m);
	p << filteredAssets;
	p >> m_Assetlist;

	m_Success = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
}

void ResolveTask::Done()
{
	// When done resolving make sure to reload assets, as there might be changes in memory that should be reloaded
	GUIDPersistentManager& gpm = GetGUIDPersistentManager();
	vector<UnityGUID> guids;
	for (int a = 0; a < m_InputList.size(); ++a)
	{
		VCAsset& vcAsset = m_InputList[a];
		if (!vcAsset.IsMeta())
		{
			string assetPath = Trim(m_InputList[a].GetPath(), "/");
			UnityGUID guid;
			gpm.PathNameToGUID(assetPath, &guid);
			guids.push_back(guid);
		}
	}
	AssetInterface::Get().ImportAssets(guids, kAssetWasModifiedOnDisk);

	VCTask::Done();
}
