#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class MoveToChangeSetTask : public VCTask
{
public:
	MoveToChangeSetTask(VCAssetList const& assets, const VCChangeSetID& id);

	void Execute();

	static bool MoveToChangeSet(VCPluginSession& p, VCMessages& msgs, VCAssetList& result, VCAssetList const& assets, const VCChangeSetID& id);

	
	const string& GetDescription() const;

private:
	VCAssetList m_inputList;
	VCChangeSetID m_ChangeSetID;
};
