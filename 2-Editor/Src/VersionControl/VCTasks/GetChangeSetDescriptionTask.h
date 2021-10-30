#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class GetChangeSetDescriptionTask : public VCTask
{
public:
	GetChangeSetDescriptionTask(const VCChangeSetID& id);

	~GetChangeSetDescriptionTask();

	void Execute();

	// In main thread
	virtual void Done ();

private:
	VCChangeSetID m_InputChangeSetID;
};
