#include "UnityPrefix.h"
#include "GetOutgoingTask.h"

GetOutgoingTask::GetOutgoingTask()
{
}

GetOutgoingTask::~GetOutgoingTask()
{
}

const string& GetOutgoingTask::GetDescription() const
{
	static string desc = "resolving outgoing";
	return desc;
}

void GetOutgoingTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	p.SendCommand("changes");
	p >> m_ChangeSets;
	m_Success = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
}

void GetOutgoingTask::Done()
{
	VCTask::Done(false);
}
