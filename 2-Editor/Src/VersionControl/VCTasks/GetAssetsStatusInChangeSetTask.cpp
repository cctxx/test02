#include "UnityPrefix.h"
#include "GetAssetsStatusInChangeSetTask.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"

using namespace std;

GetAssetsStatusInChangeSetTask::GetAssetsStatusInChangeSetTask(const VCChangeSetID& id, const VCAssetList& dirtyAssets)
	: m_ChangeSetID(id), m_DirtyAssets(dirtyAssets)
{
}

void GetAssetsStatusInChangeSetTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	p.SendCommand("changeStatus");
	p << m_ChangeSetID;
	p >> m_Assetlist;

	m_Success = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	
	// The default changeset should always included non-versioned files
	// in the ProjectSettings folder.
	// It should also contain dirty assets for VCS that does not rely on
	// checking out assets before editing e.g. Subversion.
	if (m_Success && m_ChangeSetID == kDefaultChangeSetID)
	{		
		// Get most recent status for dirty assets.
		// This list is empty for vcplugin that requires checkouts.
		if (!m_DirtyAssets.empty())
		{
			VCAssetSet dirtyAssetsVCSStatus;

			p.SendCommand("status", "offline");
			p << m_DirtyAssets;
			p >> dirtyAssetsVCSStatus;
			
			// Mark changed even if the plugin doesn's say so because we know better
			for (VCAssetList::iterator i1 = m_DirtyAssets.begin(); i1 != m_DirtyAssets.end(); ++i1)
			{
				VCAssetSet::const_iterator i2 = dirtyAssetsVCSStatus.find(*i1);
				if (i2 != dirtyAssetsVCSStatus.end())
					*i1 = *i2;
				if (!(i1->GetState() & kAddedLocal))
					i1->SetState(static_cast<States>(i1->GetState() | kCheckedOutLocal));
			}
			
			GetVCProvider().ReadPluginStatus();
			const VCMessages& sm2 = p.GetMessages();
			m_Messages.insert(m_Messages.end(), sm2.begin(), sm2.end());
			m_Assetlist.insert(m_Assetlist.end(), m_DirtyAssets.begin(), m_DirtyAssets.end());
			m_Assetlist.RemoveDuplicates();
		}
	}
	
	m_Assetlist.SortByPath();
}

void GetAssetsStatusInChangeSetTask::Done()
{
	VCTask::Done(true);
}
