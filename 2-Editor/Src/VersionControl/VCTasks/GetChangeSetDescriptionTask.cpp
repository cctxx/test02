#include "UnityPrefix.h"
#include "GetChangeSetDescriptionTask.h"

GetChangeSetDescriptionTask::GetChangeSetDescriptionTask(const VCChangeSetID& id) : m_InputChangeSetID(id)
{
}

GetChangeSetDescriptionTask::~GetChangeSetDescriptionTask()
{
}

void GetChangeSetDescriptionTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	p.SendCommand("changeDescription");
	p << m_InputChangeSetID;
	p >> m_Text;
	m_Success = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
}

void GetChangeSetDescriptionTask::Done()
{
	VCTask::Done(false);
}
