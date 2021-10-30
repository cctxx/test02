#include "UnityPrefix.h"
#include "RevertChangeSetsTask.h"

RevertChangeSetsTask::RevertChangeSetsTask(const VCChangeSetIDs& ids, bool unchangedOnly) : m_ChangeSetIDs(ids), m_unchangedOnly(unchangedOnly)
{
}

void RevertChangeSetsTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	if (m_ChangeSetIDs.empty())
	{
		m_Success = true;
		return;
	}

	p.SendCommand("revertChanges", string(m_unchangedOnly ? "unchangedOnly" : ""));
	p << m_ChangeSetIDs;
	p >> m_Assetlist;
	bool res = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	m_Success = res;
}

void RevertChangeSetsTask::Done()
{
	GetApplication().AllowAutoRefresh();
	VCTask::Done();
}
