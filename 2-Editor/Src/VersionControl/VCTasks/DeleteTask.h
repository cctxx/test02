#pragma once
#include "Editor/Src/VersionControl/VCTask.h"
class DeleteTask : public VCTask
{
public:
	DeleteTask(VCAssetList assetList);

	void Execute();

private:
	VCAssetList m_inputList;
};
