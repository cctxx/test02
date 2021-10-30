#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

enum SubmitResult
{
	kSubmitRunning = 0,
	kSubmitOk = 1,
	kSubmitError = 2,
	kSubmitConflictingFiles = 4,
	kSubmitUnaddedFiles = 8,
};

class SubmitTask : public VCTask
{
public:
	SubmitTask(const VCChangeSet* change, VCAssetList const& assets, std::string const& description, bool saveOnly, bool autoAddMissing);

	void Execute();

	const string& GetDescription() const;

private:
	bool EnsureStatusForUnknownAssets(VCPluginSession& p, VCAssetSet& assets);

	bool        m_HasValidInputChangeSet;
	VCChangeSet m_inputChangeSet;
	VCAssetList m_inputAssets;
	std::string m_inputDescription;
	bool m_inputSaveOnly;
	bool m_AutoAddMissing;
};
