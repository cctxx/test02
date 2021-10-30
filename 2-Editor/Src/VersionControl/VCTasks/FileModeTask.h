#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class FileModeTask : public VCTask
{
public:
	FileModeTask(VCAssetList assetList, FileMode mode);

	void Execute();

private:
	VCAssetList m_InputList;
	FileMode m_Mode;
};
