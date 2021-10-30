#include "UnityPrefix.h"
#include "GetIncomingTask.h"

GetIncomingTask::GetIncomingTask()
{
}

const string& GetIncomingTask::GetDescription() const
{
	static string desc = "resolving incoming";
	return desc;
}

void GetIncomingTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	p.SendCommand("incoming");
	p >> m_ChangeSets;
	m_Success = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
}

void GetIncomingTask::Done()
{
	VCTask::Done(false);
}
