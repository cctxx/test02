#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class DiffTask : public VCTask
{
public:
	DiffTask(VCAssetList const& assetlist, bool includingMetaFiles, const VCChangeSetID& id);

	void Execute();

	// In main thread
	virtual void Done ();

	bool CompareSingleFile(const VCAsset& fromAsset, const VCAsset& toAsset);

private:
	VCAssetList m_InputList;
	bool m_IncludingMetaFiles;
	VCChangeSetID m_ChangeSetID;
	std::string m_DestTmpDir;
};
