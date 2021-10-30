#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class GetAssetsStatusInChangeSetTask : public VCTask
{
public:
	GetAssetsStatusInChangeSetTask(const VCChangeSetID& id, const VCAssetList& dirtyAssets);

	void Execute();

	// In main thread
	virtual void Done ();

private:
	VCChangeSetID m_ChangeSetID;
	VCAssetList m_DirtyAssets;
};
