#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class GetOutgoingTask : public VCTask
{
public:
	GetOutgoingTask();
	~GetOutgoingTask();
	virtual const std::string& GetDescription() const;

	void Execute();

	// In main thread
	virtual void Done ();
};
