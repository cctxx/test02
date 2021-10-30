#include "UnityPrefix.h"
#include "GetLatestTask.h"
#include "Editor/Src/VersionControl/VCProvider.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Runtime/Utilities/File.h"

GetLatestTask::GetLatestTask(VCAssetList assetList) : m_inputList(assetList)
{
}

void GetLatestTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	const bool includeFolders = true;
	VCAssetList filteredAssets;
	m_inputList.Filter (filteredAssets, includeFolders, kAny, kAddedLocal);

	filteredAssets.IncludeMeta();

	if (filteredAssets.empty())
	{
		m_Success = true;
		return;
	}

	double origReadTimeout = p.GetReadTimeout();
	bool res = false;
	try
	{
		p.SendCommand("getLatest");
		p << filteredAssets;
		p.SetReadTimeout(60 * 30); // max transfer time in seconds per file.
		p >> m_Assetlist;
		res = VCProvider::ReadPluginStatus();
		p.SetReadTimeout(origReadTimeout);
	}
	catch (...)
	{
		p.SetReadTimeout(origReadTimeout);
		throw;
	}
	m_Messages = p.GetMessages();
	m_Success = res;
}

void GetLatestTask::Done()
{
	CreateFoldersFromMetaFiles();

	VCTask::Done(true);
	
	// Enable auto refreshing again
	GetApplication().AllowAutoRefresh();

	// Refresh any asset not already added to the refresh queue during syncing.
	AssetInterface::Get().Refresh(kAllowForceSynchronousImport);
}
