#include "UnityPrefix.h"
#include "StatusTask.h"
#include "Editor/Src/VersionControl/VCAsset.h"

StatusTask::StatusTask(VCAssetList assetList, bool recursive) : m_inputList(assetList), m_recursive(recursive)
{
}

const string& StatusTask::GetDescription() const
{
	static string desc = "assets status";
	return desc;
}

void StatusTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);
	
	VCAssetList listWithMeta;
	m_inputList.CopyWithMeta(listWithMeta);

	if (listWithMeta.empty())
	{
		m_Success = true;
		return;
	}

	p.SendCommand("status", m_recursive ? "recursive" : "");

	// We want only one entry per asset hence the set
	VCAssetSet result;

	p << listWithMeta;
	p >> result;
	
	m_Assetlist.insert(m_Assetlist.end(), result.begin(), result.end());

	bool res = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	m_Success = res;
}
