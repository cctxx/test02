#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class GetIncomingTask : public VCTask
{
public:
	GetIncomingTask();
	virtual const std::string& GetDescription() const;

	void Execute();

	// In main thread
	virtual void Done ();
};
