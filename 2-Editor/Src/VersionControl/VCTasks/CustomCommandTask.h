#pragma once
#include "Editor/Src/VersionControl/VCTask.h"
#include "Editor/Src/VersionControl/VCPlugin.h"

class CustomCommandTask : public VCTask
{
public:
	CustomCommandTask(const VCCustomCommand& command);

	void Execute();

private:
	VCCustomCommand m_Command;
};
