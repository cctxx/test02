#include "UnityPrefix.h"
#include "DiffTask.h"
#include "DownloadRevisionUtility.h"
#include "Editor/Src/VersionControl/VCAsset.h"

DiffTask::DiffTask(VCAssetList const& assetlist, bool includingMetaFiles,
				   const VCChangeSetID& id)
	: m_InputList(assetlist), m_IncludingMetaFiles(includingMetaFiles), m_ChangeSetID(id)
{
}

void DiffTask::Execute()
{
	// Fetch contents of files from (remote) repository and put into a folder named
	// by guid
	m_DestTmpDir = PathToAbsolutePath(GetUniqueTempPathInProject());

	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	if (m_ChangeSetID == kDefaultChangeSetID)
		m_ChangeSetID = "head";
	
	VCChangeSetIDs revs;
	revs.push_back(m_ChangeSetID);
	
	if (m_IncludingMetaFiles)
		m_InputList.IncludeMeta();
	else
		m_InputList.RemoveMeta();

	m_Success = DownloadRevisions(p, m_Messages, m_Assetlist, m_InputList, revs, m_DestTmpDir);
}

void DiffTask::Done()
{
	if (!m_Success)
	{
		DeleteFileOrDirectory(m_DestTmpDir);
		VCTask::Done();
		return;
	}

	// TODO: Folders in the inputlist may result in a larger asset list
	if (m_Assetlist.size() != m_InputList.size())
	{
		DeleteFileOrDirectory(m_DestTmpDir);
		ErrorString(Format("Received mismatching number of diff files from VCS plugin (%i - %i)", m_Assetlist.size(), m_InputList.size()));
		VCTask::Done();
		return;
	}

	// Compare the fetched content agains the working directory
	int idx = 0;
	
	VCAssetList::const_iterator i = m_InputList.begin();
	for (VCAssetList::const_iterator j = m_Assetlist.begin(); j != m_Assetlist.end(); i++, j++, idx++)
	{
		DisplayProgressbar("Compare", "Initiating compare...", (float)(idx+1)/m_Assetlist.size());
		
		if(!CompareSingleFile(*i, *j))
		{
			break;
		}
		Thread::Sleep(0.001f); // To avoid launching more than one instance of FileMerge
	}
	
	ClearProgressbar();
	
	// TODO: Monitor handles and delete or something to clean up
	// DeleteFileOrDirectory(m_DestTmpDir);
	VCTask::Done(false);
}

bool DiffTask::CompareSingleFile(const VCAsset& fromAsset, const VCAsset& toAsset)
{
	std::string fppath = AppendPathName(File::GetCurrentDirectory(), fromAsset.GetPath());
	std::string tppath = AppendPathName(File::GetCurrentDirectory(), toAsset.GetPath());
	std::string errorText = InvokeDiffTool(
										   Format("%s (mine)", fppath.c_str()), fromAsset.GetPath(),
										   Format("%s (%s)", tppath.c_str(), m_ChangeSetID.c_str()), toAsset.GetPath(),
										   "", "" // no ancestors
										   );
	if( !errorText.empty() ) 
	{
		ErrorString(errorText);
		return false;
	}
	return true;
}
