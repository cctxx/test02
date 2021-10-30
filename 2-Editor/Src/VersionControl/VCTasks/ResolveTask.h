#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class ResolveTask : public VCTask
{
public:
	ResolveTask(VCAssetList const& assetlist, ResolveMethod resolveMethod);

	void Execute();
	virtual void Done();

private:
	VCAssetList m_InputList;
	ResolveMethod m_ResolveMethod; 
};
