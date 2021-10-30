#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class DeleteChangeSetsTask : public VCTask
{
public:
	DeleteChangeSetsTask(const VCChangeSetIDs& ids);

	void Execute();

private:

	VCChangeSetIDs m_ChangeSetIDs;
};
