#include "UnityPrefix.h"
#include "SubmitTask.h"
#include "MoveToChangeSetTask.h"
#include "Editor/Src/VersionControl/VCAsset.h"

SubmitTask::SubmitTask(const VCChangeSet* change, VCAssetList const& assets, std::string const& description, bool saveOnly, bool autoAddMissing)
	: m_inputAssets(assets), m_inputDescription(description), m_inputSaveOnly(saveOnly), m_AutoAddMissing(autoAddMissing)
{
	m_HasValidInputChangeSet = change != NULL;
	if (m_HasValidInputChangeSet)
		m_inputChangeSet = *change;
}

const string& SubmitTask::GetDescription() const
{
	static string desc = "submitting";
	static string saveDesc = "";
	return m_inputSaveOnly ? saveDesc : desc;
}

bool SubmitTask::EnsureStatusForUnknownAssets(VCPluginSession& p, VCAssetSet& assets)
{
	// All assets with a state of kNone will be fetched from backend and
	// have their state set.

	VCAssetList toQuery;
	for (VCAssetSet::const_iterator i = assets.begin(); i != assets.end(); ++i)
	{
		if (i->GetState() == kNone || i->GetState() == kLocal)
			toQuery.push_back(*i);
	}

	if (toQuery.empty())
		return true;

	p.SendCommand("status");
	p << toQuery;
	toQuery.clear();
	p >> toQuery;
	
	bool res = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	m_Success = res;

	if (!m_Success)
		return m_Success;

	for (VCAssetList::const_iterator i = toQuery.begin(); i != toQuery.end(); ++i)
	{
		VCAssetSet::iterator j = assets.find(*i);
		if (j != assets.end())
		{
			assets.erase(j);
			assets.insert(*i);
		}
	}
	return m_Success;
}

void SubmitTask::Execute()
{
	// This function can be called without any assets when only updating the submit
	// change description ie. saveOnly is true
	// Otherwise assets must be provided.

	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	// Are there files passed.  can be an empty list or null
	bool hasFiles = !m_inputAssets.empty();

	// Throw error when we try to do the submit without any files
	if (!hasFiles && !m_inputSaveOnly)
	{
		m_Messages.push_back(VCMessage(kSevInfo, "Change list contains no files", kMASystem));
		m_ResultCode = kSubmitError;
		m_Success = false;
		return;
	}

	VCAssetSet inputSet(m_inputAssets.begin(), m_inputAssets.end());
	if (!EnsureStatusForUnknownAssets(p, inputSet))
	{
		m_Messages.push_back(VCMessage(kSevError, "Cannot get status for files to submit", kMASystem));
		m_ResultCode = kSubmitError;
		m_Success = false;
		return;
	}

	// Make sure that there are no conflicting or local only files
	VCAssetList assetsToAdd;
	for (VCAssetSet::const_iterator i = inputSet.begin(); i != inputSet.end(); ++i)
	{
		if (i->HasState(kLocal) && !i->IsUnderVersionControl())
		{
			m_ResultCode = m_ResultCode | kSubmitUnaddedFiles;
			assetsToAdd.push_back(*i);
		}
		else if (i->GetState() & kConflicted)
		{
			m_ResultCode = m_ResultCode | kSubmitConflictingFiles | kSubmitError;
		}
	}

	if (m_ResultCode & kSubmitConflictingFiles)
	{
		m_Messages.push_back(VCMessage(kSevError, "Please resolve conflicting files before submitting.", kMASystem));
		m_Assetlist.insert(m_Assetlist.end(), inputSet.begin(), inputSet.end());
		m_Success = false;
		return;
	}
	else if ((m_ResultCode & kSubmitUnaddedFiles) && !m_AutoAddMissing)
	{
			m_Messages.push_back(VCMessage(kSevError, "Files must be added before they can be submitted", kMASystem));
			m_Assetlist.insert(m_Assetlist.end(), inputSet.begin(), inputSet.end());
			m_ResultCode = m_ResultCode | kSubmitError;
			m_Success = false;
			return;
	}
	else if (m_ResultCode & kSubmitUnaddedFiles)
	{
		p.SendCommand("add");
		p << assetsToAdd;
		assetsToAdd.clear();
		p >> assetsToAdd;

		bool res = GetVCProvider().ReadPluginStatus();
		m_Success = res;

		if (!m_Success)
		{
			m_Messages.push_back(VCMessage(kSevError, "Error auto adding files for submit", kMASystem));
			const VCMessages& msgs = p.GetMessages();
			m_Messages.insert(m_Messages.end(), msgs.begin(), msgs.end());
			m_Assetlist.insert(m_Assetlist.end(), inputSet.begin(), inputSet.end());
			m_ResultCode = m_ResultCode | kSubmitError;
			return;
		}

		m_Messages = p.GetMessages();
		
		for (VCAssetList::const_iterator i = assetsToAdd.begin(); i != assetsToAdd.end(); ++i)
		{
			VCAssetSet::iterator j = inputSet.find(*i);
			if (j != inputSet.end())
			{
				// Update with new state
				inputSet.erase(j);
				inputSet.insert(*i);
			}
		}

		m_ResultCode = m_ResultCode & ~kSubmitUnaddedFiles;
	}
	
	AssertMsg(m_ResultCode == 0, "Submit result code not zero before actual submit it to be performed");

	m_inputAssets.clear();
	m_inputAssets.insert(m_inputAssets.end(), inputSet.begin(), inputSet.end());

	// If the change list is null then create a new changeset for the occasion
	// The default revision is used for the default changeset but also 
	// here to signal that a new changeset should be created for the assets
	if (!m_HasValidInputChangeSet)
	{
		m_inputChangeSet.SetDescription(kDefaultChangeSetDescription);
		m_inputChangeSet.SetID(kNewChangeSetID);

		// Ensure the files aren't attached to other change lists
		// Essentially we have to move them back to the default 1st.
		// Otherwise, if they are in other change lists then the 
		// operation will fail.
		VCAssetList dummy; // do not need updated states since we get them in the end of this method anyway
		VCMessages moveMsgs;
		if (!MoveToChangeSetTask::MoveToChangeSet(p, moveMsgs, dummy, m_inputAssets, kDefaultChangeSetID))
		{
			m_Messages.insert(m_Messages.end(), moveMsgs.begin(), moveMsgs.end());
			m_Messages.push_back(VCMessage(kSevError, "Cannot move assets to default changeset", kMAGeneral));
			m_ResultCode = kSubmitError;
			m_Success = false;
			return;
		}
		m_Messages.insert(m_Messages.end(), moveMsgs.begin(), moveMsgs.end());
	}

	// If no description is given then just use a default
	if (m_inputDescription.empty())
	{
		if (m_inputChangeSet.GetDescription().empty())
			m_inputChangeSet.SetDescription("No description provided");
	} 
	else
	{
		m_inputChangeSet.SetDescription(m_inputDescription);
	}

	const bool includeFolders = true;

	VCAssetList filteredAssets;
	//m_inputAssets.IncludeMeta();
	m_inputAssets.Filter(filteredAssets, includeFolders,
						 kCheckedOutLocal | kDeletedLocal | kAddedLocal | kLockedLocal);

	bool res = false;
	double origReadTimeout = p.GetReadTimeout();
	try
	{
		p.SendCommand("submit", m_inputSaveOnly ? "saveOnly" : "");
		p << m_inputChangeSet;
		p.SetReadTimeout(60 * 30); // max transfer time in seconds per file.
		p << filteredAssets;

		// @TODO P4SubmitCommand does not handle if no files are submitted and will not return a status in that case
		if (hasFiles)
			p >> m_Assetlist;

		res = GetVCProvider().ReadPluginStatus();
		p.SetReadTimeout(origReadTimeout);
	}
	catch (...)
	{
		p.SetReadTimeout(origReadTimeout);
		throw;
	}
	
	VCMessages pmsgs = p.GetMessages();
	m_Messages.insert(m_Messages.end(), pmsgs.begin(), pmsgs.end());
	m_Success = res;
	m_ResultCode = m_Success ? kSubmitOk :  kSubmitError;
		
	for (VCAssetList::const_iterator i = m_Assetlist.begin(); i != m_Assetlist.end(); ++i)
	{
		if (i->GetState() & kConflicted)
		{
			m_ResultCode = kSubmitError | kSubmitConflictingFiles;
			break;
		}
	}
}
