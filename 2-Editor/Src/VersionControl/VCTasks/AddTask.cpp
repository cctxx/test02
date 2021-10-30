#include "UnityPrefix.h"
#include "AddTask.h"
#include "Editor/Src/VersionControl/VCProvider.h"

AddTask::AddTask(VCAssetList assetList, bool recursive) : m_inputList(assetList), m_Recursive(recursive)
{
}

void AddTask::Execute()
{
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);

	// Filter any children of other assets in this operation since add is recursive
	VCAssetList assets;
	if (m_Recursive)
		m_inputList.FilterChildren(assets);
	else
		assets = m_inputList;

	// Get all the folders and local files to add
	const bool includeFolders = true;
	const bool excludeFolders = false;

	// Folders
	VCAssetList adds;
	assets.Filter(adds, includeFolders, kNone);

	// Files
	VCAssetList files;
	assets.Filter(files, excludeFolders, kLocal);

	// Build a list of the files to add.
	// Note that add must be called per directory
	for (VCAssetList::const_iterator i = adds.begin(); i != adds.end(); ++i)
	{
		if (!i->IsFolder())
			continue;

		// Check each folders meta data as well as recursing their contents
		if (m_Recursive)
			files.AddRecursive(*i);
		else 
			files.push_back(*i);
	}

	// Make sure we have an entry for all folders the path to an asset since 
	// they may need to be added if they haven't been added before.
	files.IncludeAncestors();

	files.IncludeMeta();

	// Must have some selections to continue
	if (files.empty())
	{
		m_Success = true;
		return;
	}

	p.SendCommand("add");
	p << files;
	p >> m_Assetlist;

	bool res = GetVCProvider().ReadPluginStatus();
	m_Messages = p.GetMessages();
	m_Success = res;
}
