#include "UnityPrefix.h"
#include "Editor/Src/VersionControl/VCCache.h"
#include "Editor/Src/VersionControl/VCProvider.h"
#include "Editor/Src/VersionControl/VCTask.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/EditorUserSettings.h"

using namespace std;

static void onStatusAddTaskCompleted(VCTask* task)
{
	const VCAssetList& result = task->GetAssetList();
	VCAssetList vcAdded;
	
	if (result.size() > 0)
	{
		for ( VCAssetList::const_iterator i = result.begin(); i != result.end(); ++i)
		{
			if ((!i->IsUnderVersionControl() || i->HasState(kDeletedLocal)) && GetEditorUserSettings().GetVCAutomaticAdd())
				vcAdded.push_back(*i);
		}

		if (!vcAdded.empty())
			GetVCProvider().Add(vcAdded, false)->SetCompletionAction(kUpdatePendingWindow);
	}
}

static void onStatusMoveTaskCompleted(VCTask* mtask, const map<UnityGUID, string> &moved)
{
	// The result contain status of all fromPaths for the move.
	// * In case of a rename/move inside Unity or if the asset is not versioned 
	//   the state of these will be non-versioned and nothing should be done
	// * In case of a rename/move outside Unity and the asset was versioned the state of these will 
	//   be versioned and we should move the asset in the VCS backend.
	
	const VCAssetList& result = mtask->GetAssetList();
	if (result.empty())
		return;

	VCTask* task = NULL;
	
	for ( VCAssetList::const_iterator i = result.begin(); i != result.end(); ++i)
	{
		if (i->IsUnderVersionControl() && !i->HasState(kDeletedLocal))
		{
			// This is a versioned file that need to be moved to its toPath in VCS
			// We only get here in case of file renamed/removed outside Unity
			const bool noLocalFileMove = true;
			
			for (map<UnityGUID, string>::const_iterator j = moved.begin(); j != moved.end(); ++j)
			{
				VCAsset fromAsset(j->second);
				if (fromAsset.GetPath() != i->GetPath())
					continue;

				string toPath = GetGUIDPersistentManager().AssetPathNameFromGUID(j->first);
				VCAsset toAsset;
				toAsset.SetPath(toPath);
				task = GetVCProvider().Move(fromAsset, toAsset, noLocalFileMove);
			}
		}
	}

	if (task)
		task->SetCompletionAction(kUpdatePendingWindow);
}

static void onStatusRemoveTaskCompleted(VCTask* task)
{
	const VCAssetList& result = task->GetAssetList();

	if (result.empty())
		return;

	VCAssetList removed;
		
	// Create the list of assets that has been deleted external to unity ie. has
	// not really been deleted from vcs yet.
	for ( VCAssetList::const_iterator i = result.begin(); i != result.end(); ++i)
	{
		if (!i->HasState(kLocal | kDeletedLocal) && i->IsUnderVersionControl())
			removed.push_back(*i);
	}

	if (!removed.empty())
	{
		GetVCProvider().Delete(removed)->SetCompletionAction(kUpdatePendingWindow);
	}
}

static void HandleRemovedAssets( const set<UnityGUID> &removed )
{
	VCAssetList removedList;
	for (std::set<UnityGUID>::iterator i = removed.begin(); i != removed.end(); ++i)
	{
		string projectPath = GetGUIDPersistentManager().AssetPathNameFromGUID(*i);
		VCAsset asset;
		asset.SetPath(projectPath);
		removedList.push_back(asset);
	}

	if ( !removedList.empty() )
	{
		// Need to get newest status because a delete can be triggered inside Unity in which
		// case it has already been deleted in vcs backend. It can also be deleted externally
		// in which case has not yet been deleted in vcs backend. Ie. we have two code paths
		// ending up here.
		VCTask* task = GetVCProvider().Status(removedList, false);
		task->SetDoneCallback(onStatusRemoveTaskCompleted);
	}
}

static void HandleAddedAssets( const set<UnityGUID> &added )
{
	VCAssetList statusCheckList;
	for (std::set<UnityGUID>::iterator i = added.begin(); i != added.end(); ++i)
	{
		string projectPath = GetGUIDPersistentManager().AssetPathNameFromGUID(*i);
		VCAsset asset;
		asset.SetPath(projectPath);
		statusCheckList.push_back(asset);
	}

	/* Disabled for now since this may be a too large performance hit to take
	for (std::set<UnityGUID>::iterator i = refreshed.begin(); i != refreshed.end(); ++i)
	{
	string projectPath = GetGUIDPersistentManager().AssetPathNameFromGUID(*i);
	if (EndsWith(projectPath, ".meta"))
	continue;

	VCAsset asset;

	// The asset .meta should be in cache at the same time that the normal asset
	// is.
	if (!GetVCCache().GetAssetByPath(projectPath + ".meta", asset, false))
	list.push_back(asset);
	}
	*/

	if ( !statusCheckList.empty() )
	{
		VCTask* task = GetVCProvider().Status(statusCheckList, false);
		task->SetDoneCallback(onStatusAddTaskCompleted);
	}
}

static void HandleMovedAssets( const map<UnityGUID, string> &moved )
{
	VCAssetList statusCheckList;  // Status list to check if VCS backend needs to move the asset because we got here as a result of moving the asset externally.
	VCAssetList statusUpdateList; // Status list of asset that need their status updated in vccache
 
	// Also fetch the status of the moved files since it is only at this point of the move that the paths/guid mapping internally is correct.
	for (std::map<UnityGUID,string>::const_iterator i = moved.begin(); i != moved.end(); ++i)
	{
		VCAsset asset1;
		asset1.SetPath(i->second); // fromPath
		statusCheckList.push_back(asset1);
		
		VCAsset asset2;
		string toPath = GetGUIDPersistentManager().AssetPathNameFromGUID(i->first);
		asset2.SetPath(toPath); // toPath
		statusUpdateList.push_back(asset2);
	}

	if ( !statusCheckList.empty() )
	{
		VCTask* task = GetVCProvider().Status(statusCheckList, false);
		task->Retain();
		task->Wait();
		
		// TODO: change to callback once a better callback system is in place. That way we do not have to Wait()
		onStatusMoveTaskCompleted(task, moved);
		task->Release();

		GetVCProvider().Status(statusUpdateList, false)->SetCompletionAction(kUpdatePendingWindow);
	}
}

void VCAutoAddAssetPostprocess(const set<UnityGUID>& refreshed, const set<UnityGUID>& added, const set<UnityGUID>& removed, const map<UnityGUID, string>& moved)
{
	if (!GetVCProvider().IsActive())
		return;

	HandleAddedAssets(added);
	HandleMovedAssets(moved);
	HandleRemovedAssets(removed);
}
