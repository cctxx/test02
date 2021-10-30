#include "UnityPrefix.h"
#include "FileModeTask.h"

FileModeTask::FileModeTask(VCAssetList assetList, FileMode mode) : m_InputList(assetList), m_Mode(mode)
{
}

void FileModeTask::Execute()
{
	if (m_InputList.empty())
	{
		m_Success = true;
		return;
	}

	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	p.SendCommand("filemode", "set", m_Mode == kFMText ? "text" : "binary");
	p << m_InputList;
	p >> m_Assetlist;
	m_Assetlist.clear(); // ignored for now
	bool res = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	m_Success = res;
}
