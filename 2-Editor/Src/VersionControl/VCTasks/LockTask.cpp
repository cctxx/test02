#include "UnityPrefix.h"
#include "LockTask.h"

LockTask::LockTask(VCAssetList const& assetlist, bool inputLock) : m_inputList(assetlist), m_inputLock(inputLock)
{
}
void LockTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	if (m_inputList.empty())
	{
		m_Success = true;
		return;
	}

	const bool includeFolders = true;
	VCAssetList filteredAssets;
	int excl = m_inputLock ? kLockedLocal | kLockedRemote : 0;
	m_inputList.Filter(filteredAssets, includeFolders, kAny, excl);
	filteredAssets.IncludeMeta();

	if (filteredAssets.empty())
	{
		m_Success = true;
		return;
	}

	p.SendCommand(m_inputLock ? "lock" : "unlock");
	p << filteredAssets;
	p >> m_Assetlist;
	bool res = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	m_Success = res;
}
