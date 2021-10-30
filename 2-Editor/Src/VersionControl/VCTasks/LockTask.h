#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class LockTask : public VCTask
{
public:
	LockTask(VCAssetList const& assetlist, bool inputLock);

	void Execute();

private:
	bool m_inputLock;
	VCAssetList m_inputList;
};
