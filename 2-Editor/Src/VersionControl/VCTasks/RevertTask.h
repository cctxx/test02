#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class RevertTask : public VCTask
{
public:
	RevertTask(VCAssetList const& assetlist, RevertMode revertMode);

	void Execute();

	void Done();

private:
	RevertMode m_inputRevertMode;
	VCAssetList m_inputList;
	VCAssetList m_RevertResult;
};
