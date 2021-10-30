#pragma once
#include "Editor/Src/VersionControl/VCTask.h"
class GetLatestTask : public VCTask
{
public:
	GetLatestTask(VCAssetList assetList);

	void Execute();
	virtual void Done();

private:
	VCAssetList m_inputList;
};
