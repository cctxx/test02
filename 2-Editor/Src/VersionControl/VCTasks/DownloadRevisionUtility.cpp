#include "UnityPrefix.h"
#include "DownloadRevisionUtility.h"
#include "Editor/Src/VersionControl/VCPluginProtocol.h"
#include "Editor/Src/VersionControl/VCProvider.h"

bool DownloadRevisions(VCPluginSession& p, VCMessages& msgs, VCAssetList& result, const VCAssetList& assets, const VCChangeSetIDs& changeSetIDs, const std::string& targetDir)
{
	VCAssetList filesRecursive;
	if (changeSetIDs.empty())
	{
		msgs.push_back(VCMessage(kSevWarning, "No revision specified for downloading from version control", kMASystem));
		return false;
	}

	for (VCAssetList::const_iterator i = assets.begin(); i != assets.end(); ++i)
	{
		// Check each folders meta data as well as recursing their contents
		filesRecursive.AddRecursive(*i);
	}

	// Cannot download a folder but only files so we strip folder assets.
	// Note that folder meta files are included never the less.
	const bool excludeFolders = false;
	VCAssetList filteredAssets;
	filesRecursive.Filter(filteredAssets, excludeFolders, kAny);

	if (filteredAssets.empty())
		return true;

	p.SendCommand("download");
	p.WriteLine(targetDir);
	
	p << changeSetIDs;
	p << filteredAssets;
	p >> result;
	
	bool res = GetVCProvider().ReadPluginStatus();
	msgs = p.GetMessages();

	return res;
}
