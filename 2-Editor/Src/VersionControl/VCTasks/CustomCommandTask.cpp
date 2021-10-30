#include "UnityPrefix.h"
#include "CustomCommandTask.h"

CustomCommandTask::CustomCommandTask(const VCCustomCommand& command) : m_Command(command)
{
}

void CustomCommandTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	p.SendCommand("customCommand", m_Command.name);
	bool res = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	m_Success = res;
}
