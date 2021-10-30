#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class StatusTask : public VCTask
{
public:
	StatusTask(VCAssetList assetList, bool recursive);

	void Execute();
	virtual const std::string& GetDescription() const;

private:
	VCAssetList m_inputList;
	bool m_recursive;
};
