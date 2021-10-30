#include "UnityPrefix.h"
#include "CheckoutTask.h"

CheckoutTask::CheckoutTask(VCAssetList assetList, CheckoutMode mode) : m_inputList(assetList), m_mode(mode)
{
}

void CheckoutTask::Execute()
{
	// Checkout is recursive
	const bool includeFolders = true;
	VCAssetList filteredAssets;
	m_inputList.Filter(filteredAssets, includeFolders, kAny, kCheckedOutLocal);

	// Only one mode supported for now
	Assert(m_mode == kBoth);

	switch (m_mode)
	{
	case kBoth:
		filteredAssets.IncludeMeta();
		break;
	case kAsset:
		;
		break;
	case kMeta:
		filteredAssets.ReplaceWithMeta();
		break;
	}

	if (filteredAssets.empty())
	{
		m_Success = true;
		return;
	}

	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	p.SendCommand("checkout");
	p << filteredAssets;
	p >> m_Assetlist;
	bool res = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	m_Success = res;
}
