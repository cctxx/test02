#include "UnityPrefix.h"
#include "DeleteChangeSetsTask.h"

DeleteChangeSetsTask::DeleteChangeSetsTask(const VCChangeSetIDs& ids) : m_ChangeSetIDs(ids)
{
}

void DeleteChangeSetsTask::Execute()
{
	if (m_ChangeSetIDs.empty())
	{
		m_Success = true;
		return;
	}

	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	p.SendCommand("deleteChanges");
	p << m_ChangeSetIDs;
	p >> m_Assetlist;
	bool res = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	m_Success = res;
}


