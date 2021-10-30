#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class RevertChangeSetsTask : public VCTask
{
public:
	RevertChangeSetsTask(const VCChangeSetIDs& ids, bool unchangedOnly);

	void Execute();

	void Done();

private:
	bool m_unchangedOnly;
	VCChangeSetIDs m_ChangeSetIDs;
};
