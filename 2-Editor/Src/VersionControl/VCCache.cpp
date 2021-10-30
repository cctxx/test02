#include "UnityPrefix.h"
#include "VCCache.h"
#include "VCProvider.h"
#include "VCTask.h"
#include "Editor/Src/Selection.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/AssetPipeline/AssetModificationCallbacks.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include <set>
using namespace std;

VCCache* VCCache::instance = NULL;

VCCache::VCCache()
: m_lastUpdate(0)
{
	Assert(instance == NULL);
	instance = this;
}

VCCache::~VCCache()
{
	instance = NULL;
}

void VCCache::GetSelection (VCAssetList& assets)
{
	set<UnityGUID> selection = GetSelectedAssets ();

	for(set<UnityGUID>::iterator it = selection.begin(); it != selection.end(); ++it)
	{
		string path = GetAssetPathFromGUID(*it);
		if (path.size() > 0)
		{
			VCAsset newEntry;
			if (GetAssetByPath(path, newEntry))
				assets.push_back(newEntry);
		}
	}
}

bool VCCache::GetAssetByPath (string const& inpath, VCAsset& vcAsset, bool allowBackendQuery)
{
	UnityGUID guid;
	string path = Trim(inpath, "/");
	GetGUIDPersistentManager().PathNameToGUID(path, &guid);

	if (!guid.IsValid())
		return false;

	return GetAssetByGUID(guid, vcAsset, allowBackendQuery);
}

bool VCCache::GetAssetByGUID (UnityGUID const& guid, VCAsset& vcAsset, bool allowBackendQuery)
{
	GUID2VCAssetMap::iterator dbIter = m_Database.find(guid);

	bool found = false;
	if (dbIter != m_Database.end())
	{
		VCCacheEntry& centry = dbIter->second;

		if (centry.validity == kVInvalid && allowBackendQuery)
		{
			// Schedule status update
			centry.asset.AddState(kUpdating);
			centry.validity = kVUpdating;
			m_Pending.push_back(centry.asset);
		}
		vcAsset = centry.asset;
		found = centry.validity != kVInvalid;
	} 
	else if (allowBackendQuery)
	{
		string path = GetAssetPathFromGUID(guid);
		if (!path.empty())
		{
			Asset const* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
			if (asset->type == kFolderAsset && path[path.size() - 1] != '/')
				path += "/";

			vcAsset = CreateNewVCAssetEntry(path, guid);
			found = true;
		}
	}
	
	vcAsset.m_guid = guid;
	return found;
}

VCAsset& VCCache::CreateNewVCAssetEntry (string const& unityPath, UnityGUID guid)
{
	VCAsset newRequest;
	newRequest.SetPath(unityPath);
	//if (!newRequest.IsFolder())
	newRequest.SetState(kUpdating);
	newRequest.m_guid = guid;
	
	VCCacheEntry entry(newRequest, kVUpdating);
	pair<GUID2VCAssetMap::iterator, bool> ret = m_Database.insert(make_pair<UnityGUID, VCCacheEntry>(guid, entry));
	Assert(ret.second == true);

	//	if (!newRequest.IsFolder())
	m_Pending.push_back(newRequest);

	return ret.first->second.asset;
}

VCTask* VCCache::Tick (bool forcePending)
{
	// If an invalidation has been performed we remember the Status task
	// initiated below. That way we know when the invalidated assets has 
	// had their state updated and can remove invalidated assets not in 
	// the status result. The original invalidated assets are keps in
	// the m_Invalidated list.
	
	if (!GetVCProvider().Enabled())
	{
		return NULL;
	}

	// Trigger Async Refresh
	bool hasTimedOut = (GetTimeManager().GetRealtime () - m_lastUpdate) > 1.0;
	if ((hasTimedOut || forcePending) && m_Pending.size() > 0)
	{
		VCTask* task = GetVCProvider().Status(m_Pending, false);

		task->SetDoneCallback(&VCCache::StatusCallback);

		m_lastUpdate = GetTimeManager().GetRealtime ();
		m_Pending.clear();
		return task;
	}
	return NULL;
}

void VCCache::Invalidate()
{
	for (GUID2VCAssetMap::iterator i = m_Database.begin(); i != m_Database.end(); ++i)
		i->second.validity = kVInvalid;
}

void VCCache::OnStatus(VCTask* task)
{
	// Update the database

	// Needs to ref the tasks assets directly in order
	// to be able to set them as modified according to dirty state
	// if necessary.
	VCAssetList& assetList = task->m_Assetlist;

	set<UnityGUID> dirtyGuids;
	const VCPlugin::Traits& pluginTraits = GetVCProvider().GetActivePlugin()->GetTraits();
	bool enablesCheckout = GetVCProvider().GetActivePlugin() && pluginTraits.enablesCheckout;
	if (!enablesCheckout)
	{
		// Build an asset list of the dirty asset paths
		AssetInterface::Get().GetDirtyAssets(dirtyGuids);
		
		// Current scene is a speciel asset regarding dirtyness
		if (GetApplication().GetCurrentSceneGUID() != UnityGUID() &&
			GetApplication().IsSceneDirty())
			dirtyGuids.insert(GetApplication().GetCurrentSceneGUID());
	}	
	
	bool callStatusUpdatedCallbacks = false;
	
	for (int i = 0; i < assetList.size(); ++i)
	{
		VCAsset& updatedAsset = assetList[i];

		// We are only caching assets themselves and not meta files
		// If the vcs backend does not support versioning of folders we use the folder .meta state instead
		if (!pluginTraits.enablesVersioningFolders)
		{
			if (updatedAsset.IsMeta())
			{
				// If meta is for a folder the convert into folder asset
				if (IsDirectoryCreated(updatedAsset.GetAssetPath()))
				{
					updatedAsset.SetPath(updatedAsset.GetAssetPath());
				}
			}
			else if (updatedAsset.IsFolder())
			{
				continue; // skip folders because we use the .meta state instead
			}
		}
				
		string p = Trim(updatedAsset.GetPath(), " \t/"); // when looking up paths we do not want ending /
		UnityGUID guid;
		GetGUIDPersistentManager().PathNameToGUID(p, &guid);
		if (guid.IsValid())
		{
			// E.g. svn needs to have the modified flag (kCheckOutLocal) set when an asset is dirty.
			if (!enablesCheckout && dirtyGuids.count(guid))
				updatedAsset.AddState(kCheckedOutLocal);
			
			VCAsset asset;
			bool alreadyInCache = GetAssetByGUID(guid, asset, false);
			updatedAsset.m_guid = guid;
			m_Database[guid] = VCCacheEntry(updatedAsset, kVValid);
			
			callStatusUpdatedCallbacks = callStatusUpdatedCallbacks ||
					!alreadyInCache || 
					asset.GetState() != updatedAsset.GetState() ||
					asset.GetPath() != updatedAsset.GetPath();
		}
	}
	
	if (callStatusUpdatedCallbacks)
		AssetModificationCallbacks::OnStatusUpdated();
}

void VCCache::StatusCallback (VCTask* task)
{
	instance->OnStatus(task);
}

void VCCache::SetDirty (UnityGUID guid)
{
	bool enablesCheckout = GetVCProvider().GetActivePlugin() != NULL && GetVCProvider().GetActivePlugin()->GetTraits().enablesCheckout;
	if (enablesCheckout)
		return; // The checkout of an asset in vcs will determine it modified state
	
	GUID2VCAssetMap::iterator result = m_Database.find(guid);
	
	if (result != m_Database.end())
	{
		VCAsset& a = result->second.asset;

		// Newly added asset or local only assets should not have the modified flag set (CheckedOutLocal)
		// TODO: Fix condition when a vcs needing functionallity this is added
		if (!a.HasState(kAddedLocal | kLocal | kCheckedOutLocal))
		{
			a.AddState(kCheckedOutLocal);
			
			// Update views to reflect checked out state in e.g. project browser
			MonoException* exception = NULL;
			CallStaticMonoMethod("EditorApplication", "Internal_RepaintAllViews", NULL, &exception);
			
			// Update pending window to make it include modified asset in outgoing list
			// TODO: Use vcprovider global update signal instead of one call for each guid changed
			
			ScriptingInvocation  invoke ("UnityEditor.VersionControl", "WindowPending", "UpdateAllWindows");
			invoke.Invoke ();
		}
	}
}

VCCache& GetVCCache()
{
	Assert(VCCache::instance != NULL);
	return *VCCache::instance;
}
