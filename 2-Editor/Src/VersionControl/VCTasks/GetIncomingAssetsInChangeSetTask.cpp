#include "UnityPrefix.h"
#include "GetIncomingAssetsInChangeSetTask.h"

GetIncomingAssetsInChangeSetTask::GetIncomingAssetsInChangeSetTask(const VCChangeSetID& id) : m_ChangeSetID(id)
{
}

GetIncomingAssetsInChangeSetTask::~GetIncomingAssetsInChangeSetTask()
{
}

void GetIncomingAssetsInChangeSetTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	p.SendCommand("incomingChangeAssets");
	p << m_ChangeSetID;
	p >> m_Assetlist;
	m_Assetlist.SortByPath();	
	m_Success = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
}

void GetIncomingAssetsInChangeSetTask::Done()
{
	VCTask::Done(false);
}
