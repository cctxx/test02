#include "UnityPrefix.h"
#include "MoveTask.h"
#include "Editor/Src/AssetPipeline/AssetModificationCallbacks.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"

MoveTask::MoveTask(const VCAsset& src, const VCAsset& dst, bool noLocalFileMove) : 
	m_src(src), m_dst(dst), m_NoLocalFileMove(noLocalFileMove)
{
}

const string& MoveTask::GetDescription() const
{
	static string desc = "moving";
	return desc;
}

void MoveTask::Execute()
{
	// Need to ensure the P4 version is 2009.2 or above for moving files with -k.  If i dont do
	// -k then unity will moan about meta files so letting unity do the actual move solves this.
	// if unity didnt complain then -k could be removed and the subsequent version checks as connection
	// can be validated as part of the status update after this.
/*
	float version = ServerVersion;
	
	if (version == 0)
	{
		SetError ("Rename/move failed as a connection to the p4 server cannot be established");
		return AssetMoveResult.FailedMove;
	}
	else if (version < 2009.2)
	{
		SetError ("Unity asset rename/move is only supported from server version 2009.2, Current version is " + version);
		return AssetMoveResult.FailedMove;
	}
*/

	// Make sure that dst is a folder if src is a folder
	if (m_src.IsFolder() && !m_dst.IsFolder())
		m_dst.SetPath(m_dst.GetPath() + "/");
	
	try
	{
		GetVCProvider().ClearPluginMessages();
		VCPluginSession& ps = GetVCProvider().EnsurePluginIsRunning(m_Messages);
		ps.SetProgressListener(this);
	}
	catch (VCPluginSession& e) {
		m_ResultCode = AssetModificationCallbacks::FailedMove;
		return;
	}

	VCAssetList vcsMoveList;
	vcsMoveList.push_back(m_src);
	vcsMoveList.push_back(m_dst);
	VCAssetList vcsMoveListMeta(vcsMoveList);
	vcsMoveListMeta.ReplaceWithMeta();
	vcsMoveList.insert(vcsMoveList.begin(), vcsMoveListMeta.begin(), vcsMoveListMeta.end());
	
#ifdef META_FILES_NOT_LOCATED_NEXT_TO_ASSET
	
	// Since the meta files may be located somewhere else that the assets we need move each file 
	// separately in order to ensure that the metadata associated is also moved correctly at the
	// same time.
	VCAssetList srcs;
	srcs.AddRecursive(m_src);

	// Handle single asset moves, we need to update the status. The updated srcs will include the meta files.
	const bool recursive = false;
	srcs = Status(srcs, recursive);
	
	if (srcs.empty())
	{
		m_Messages.push_back(VCMessage(kSevWarning, "Could not fetch assets status during move", kMASystem));
		return AssetModificationCallbacks::FailedMove;
	}
	
	// Handle single asset moves, we need to update the status
	if (m_dst.IsFolder() && srcs.size() != 2)
	{
		// An error to have a non-folder dst with multi files srcs
		// (2 != multi since it means a single asset+metafile)
		m_Messages.push_back(VCMessage(kSevWarning, "Invalid move of multiple files to single file", kMASystem));
		return AssetModificationCallbacks::FailedMove;
	}
	
	for (VCAssetList::const_iterator i = srcs.begin(); i != srcs.end(); ++i)
	{
		const VCAsset& isrc = *i;
		if (isrc.IsMeta()) continue;

		VCAsset srcMeta(isrc.GetMetaPath());
		// Copy the files state in order to determine editability on the plugin side
		srcMeta.SetState((States) (isrc.GetState() | kMetaFile)); 
		VCAsset dest(m_dst);
		
		// Append the filename to the target if its a folder passed and the source is not a folder
		if (m_dst.IsFolder())
			dest.PathAppend (isrc.GetFullName());

		VCAsset destMeta(dest.GetMetaPath());
		
		// Handle the actual move in the plugin. The plugin will also handle non-versioned asset moves.
		vcsMoveList.push_back(isrc);
		vcsMoveList.push_back(dest);

		vcsMoveList.push_back(srcMeta);
		vcsMoveList.push_back(destMeta);
	}
#endif
		
	try 
	{
		VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
		p.SetProgressListener(this);
		if (m_NoLocalFileMove)
			p.SendCommand("move", "noLocalFileMove");
		else
			p.SendCommand("move");
		p << vcsMoveList;
		p >> m_Assetlist;
		m_Success = GetVCProvider().ReadPluginStatus();
		m_Messages = p.GetMessages();
		m_ResultCode = m_Success ? AssetModificationCallbacks::DidMove : AssetModificationCallbacks::FailedMove;
	}
	catch (VCPluginSession& e) 
	{
		m_Messages = e.GetMessages();
		m_ResultCode = AssetModificationCallbacks::FailedMove;
	}
}

void MoveTask::Done()
{
	CreateFoldersFromMetaFiles();

	// The correct state is really set in the VCAssetPostProcessor because the GUID mapping
	// at this point is wrong. The GUIDPersistantMananger simply hasn't been updated with the
	// move result and VCCache cannot update the correct GUID/path mapping.
	VCTask::Done(false);

	// Enable auto refreshing again
	GetApplication().AllowAutoRefresh();
}
