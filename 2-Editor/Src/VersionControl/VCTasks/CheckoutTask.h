#pragma once
#include "Editor/Src/VersionControl/VCTask.h"
class CheckoutTask : public VCTask
{
public:
	CheckoutTask(VCAssetList assetList, CheckoutMode mode);

	void Execute();

private:
	VCAssetList m_inputList;
	CheckoutMode m_mode;
};
