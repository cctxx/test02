#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class GetIncomingAssetsInChangeSetTask : public VCTask
{
public:
	GetIncomingAssetsInChangeSetTask(const VCChangeSetID& id);

	~GetIncomingAssetsInChangeSetTask();

	void Execute();

	// In main thread
	virtual void Done ();

private:
	VCChangeSetID m_ChangeSetID;
};
