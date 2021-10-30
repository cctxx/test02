#include "UnityPrefix.h"
#include "DeleteTask.h"

DeleteTask::DeleteTask(VCAssetList assetList) : m_inputList(assetList)
{
}

void DeleteTask::Execute()
{
	// Filter the list to the asset types we are interested in
	const bool includeFolders = true;
	VCAssetList filteredList;
	m_inputList.Filter(filteredList, includeFolders, kAny, kDeletedLocal);

	// Strip the list so we do not have parents of children in the same list
	VCAssetList deleteList;
	filteredList.FilterChildren(deleteList);

	deleteList.IncludeMeta();

	if (deleteList.empty())
	{
		m_Success = true;
		return;
	}

	GetVCProvider().ClearPluginMessages();
	m_Success = false;
	try
	{
		VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
		p.SetProgressListener(this);

		p.SendCommand("delete");
		p << deleteList;
		p >> m_Assetlist;
		bool res = GetVCProvider().ReadPluginStatus();
		m_Messages = p.GetMessages();
		m_Success = res;
	}
	catch (VCPluginException ex)
	{
		VCMessages msgs = ex.GetMessages();
		msgs.push_back(VCMessage(kSevWarning, "Not all files were deleted by version control", kMASystem));
		throw VCPluginException(msgs);
	}
	catch (ExternalProcessException ex)
	{
		VCMessages msgs;
		msgs.push_back(VCMessage(kSevError, ex.Message(), kMASystem));
		msgs.push_back(VCMessage(kSevWarning, "Not all files were deleted by version control", kMASystem));
		throw VCPluginException(msgs);
	}
}
