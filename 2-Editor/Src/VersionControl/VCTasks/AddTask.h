#pragma once
#include "Editor/Src/VersionControl/VCTask.h"
#include "Editor/src/VersionControl/VCAsset.h"
class AddTask : public VCTask
{
public:
	AddTask(VCAssetList assetList, bool recursive = true);

	void Execute();

private:
	
	VCAssetList m_inputList;
	bool m_Recursive;
};
