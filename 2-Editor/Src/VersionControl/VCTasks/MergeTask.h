#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class MergeTask : public VCTask
{
public:
	MergeTask(VCAssetList const& assetlist, MergeMethod method);
	MergeTask();
	
	void Execute();
	
	// In main thread
	virtual void Done ();
	
private:
	bool MergeSingleFile(const VCAsset& outputAsset,
						 const VCAsset& mineAsset,
						 const VCAsset& conflictingAsset,
						 const VCAsset& baseAsset);
	VCAssetList m_InputList;
	MergeMethod m_MergeMethod;
	std::string m_DestTmpDir;
};
