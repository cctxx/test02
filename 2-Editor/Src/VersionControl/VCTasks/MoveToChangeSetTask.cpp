#include "UnityPrefix.h"
#include "MoveToChangeSetTask.h"

MoveToChangeSetTask::MoveToChangeSetTask(VCAssetList const& assets, const VCChangeSetID& id) : m_inputList(assets), m_ChangeSetID(id)
{
}

const string& MoveToChangeSetTask::GetDescription() const
{
	static string desc = "moving change";
	return desc;
}

void MoveToChangeSetTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	m_Success = MoveToChangeSet(p, m_Messages, m_Assetlist, m_inputList, m_ChangeSetID);
}

bool MoveToChangeSetTask::MoveToChangeSet(VCPluginSession& p, VCMessages& msgs, VCAssetList& result, VCAssetList const& assets, const VCChangeSetID& id)
{
	const bool includeFolders = true;
	VCAssetList filteredAssets;
	assets.Filter(filteredAssets, includeFolders, kCheckedOutLocal | kDeletedLocal | kAddedLocal | kLockedLocal);
	filteredAssets.IncludeMeta();

	if (filteredAssets.empty())
		return true;

	p.SendCommand("changeMove");
	p << id;
	p << filteredAssets;
	p >> result;
	bool res = GetVCProvider().ReadPluginStatus();
	msgs = p.GetMessages();
	return res;
}
