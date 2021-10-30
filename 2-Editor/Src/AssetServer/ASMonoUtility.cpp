#include "UnityPrefix.h"
#include "ASMonoUtility.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/PackageUtility.h"
#include "Editor/Src/Selection.h"
#include "Runtime/Mono/MonoScript.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Scripting/ScriptingUtility.h"

namespace AssetServer 
{

static ASMonoUtility *gSingleton = NULL;

ASMonoUtility& ASMonoUtility::Get ()
{
	if (gSingleton == NULL)
		gSingleton = new ASMonoUtility();
	return *gSingleton;
}

ASMonoUtility::ASMonoUtility()
{
	SetNeedsToRefreshCommit();
	SetNeedsToRefreshUpdate();
	SetLatestPassedChangeset(0);
}

bool ASMonoUtility::GetRefreshUpdate() 
{ 
	return m_RefreshUpdate;
}

bool ASMonoUtility::GetRefreshCommit()
{ 
	return m_RefreshCommit; 
}

void ASMonoUtility::CheckForServerUpdates()
{
	if (!m_RefreshUpdate)
		m_RefreshUpdate = m_LatestChangesetPassedToMono != Controller::Get().GetLatestServerChangeset();
}

// Ugly but effective
// these are used to clear update conflict resolutions after update is finished
// downloadResolutions is filled when UpdateSetResolutions is called
// nameConflicts are collected on calls to SetNameConflictResolution 
// After this FinishASUpdate resets resolutions as it is supposed to and clears buffers
map<UnityGUID, DownloadResolution> g_DownloadResolutions;
std::vector<UnityGUID> g_NameConflicts;
vector<DeletedAsset> g_RecoverAssets;

void AssetsItemToMono (const ImportPackageAsset &src, MonoAssetsItem &dest) 
{
	dest.guid = MonoStringNew(GUIDToString(src.guid));
	dest.parentGuid = MonoStringNew(GUIDToString(src.parentGuid));
	dest.pathName = MonoStringNew(Controller::Get().GetAssetPathName(src.guid));
	dest.exportedAssetPath = MonoStringNew(src.exportedAssetPath);
	dest.enabled = src.enabled;
	dest.assetIsDir = Controller::Get().AssetIsDir(src.guid);
	dest.message = MonoStringNew(src.message);
	dest.guidFolder = MonoStringNew(src.guidFolder);
	dest.changeFlags = Configuration::Get().GetWorkingItem(src.guid).GetChangeFlags();
	dest.previewPath = MonoStringNew(src.previewPath);
	dest.exists = src.exists;

}

void ImportAssetsItemToMono (const ImportPackageAsset &src, MonoAssetsItem &dest) 
{
	dest.guid = MonoStringNew(GUIDToString(src.guid));
	dest.parentGuid = MonoStringNew(GUIDToString(src.parentGuid));
	dest.pathName = MonoStringNew(src.exportedAssetPath);
	dest.exportedAssetPath = MonoStringNew(src.exportedAssetPath);
	dest.enabled = src.enabled;
	dest.assetIsDir = src.isFolder;
	dest.message = MonoStringNew(src.message);
	dest.guidFolder = MonoStringNew(src.guidFolder);
	dest.changeFlags = Configuration::Get().GetWorkingItem(src.guid).GetChangeFlags();
	dest.previewPath = MonoStringNew(src.previewPath);
	dest.exists = src.exists;
}

void AssetsItemToMono (const UnityGUID &guid, const Item &item, MonoAssetsItem &dest)
{
	dest.guid = MonoStringNew(GUIDToString(guid));
	dest.pathName = MonoStringNew(item.name); // this is actually a full path (set when caching)
	dest.assetIsDir = item.type == kTDir;
	dest.changeFlags = item.changeFlags;
}

void AssetsItemToCpp( MonoAssetsItem & src, ImportPackageAsset & dst)
{
	dst.enabled = src.enabled;
	dst.exportedAssetPath = MonoStringToCpp(src.exportedAssetPath);
	dst.guid = StringToGUID( MonoStringToCpp( src.guid) );
	dst.parentGuid = StringToGUID( MonoStringToCpp(src.parentGuid) );	
	dst.message = MonoStringToCpp(src.message);
	dst.guidFolder = MonoStringToCpp(src.guidFolder);
	dst.isFolder = src.assetIsDir;
}

void AssetsItemToMono (const AssetServerCache::DeletedItem &src, MonoAssetsItem &dest) 
{
	dest.guid = MonoStringNew(GUIDToString(src.guid));
	dest.parentGuid = MonoStringNew(GUIDToString(src.parent));
	dest.pathName = MonoStringNew(src.fullPath);
	dest.enabled = true;
	dest.assetIsDir = src.type == kTDir;
	dest.changeFlags = kCFDeleted;
}

void DeletedAssetToMono (const DeletedAsset &src, MonoDeletedAsset &dest) 
{
	dest.changeset = src.changeset;
	dest.guid = MonoStringNew(GUIDToString(src.guid));
	dest.parent = MonoStringNew(GUIDToString(src.parent));
	dest.name = MonoStringNew(src.name);
	dest.fullPath = MonoStringNew(Configuration::Get().CachedPathFromID(src.parentFolderID) + NicifyAssetName(src.name));
	dest.date = MonoStringNew(src.GetDateString());
	dest.assetIsDir = src.type == kTDir;
}

void MaintDatabaseRecordToMono(const MaintDatabaseRecord &src, MonoMaintDatabaseRecord &dest)
{
	dest.name = MonoStringNew(src.name);
	dest.dbName = MonoStringNew(src.dbName);
}

void MaintUserRecordToMono(const MaintUserRecord &src, MonoMaintUserRecord &dest)
{
	dest.enabled = src.enabled;
	dest.userName = MonoStringNew(src.userName);
	dest.fullName = MonoStringNew(src.fullName);
	dest.email = MonoStringNew(src.email);
}

void AddChangesetFlag(string &str, const string& strToAdd)
{
	if (!str.empty())
	{
		str += ", ";
		str += strToAdd;
	}else
		str = strToAdd;
}

void ChangesetHeadersToMono (const Changeset &src, MonoChangeset &dest)
{
	dest.changeset = src.changeset;
	dest.message = MonoStringNew(src.message);
	dest.date = MonoStringNew(src.GetDateString());
	dest.owner = MonoStringNew(src.owner);
}

void ItemToMonoChangesetItem (const Item &src, MonoChangesetItem &dest)
{
	dest.fullPath = MonoStringNew(Configuration::Get().CachedPathFromID(src.parentFolderID) + NicifyAssetName(src.name));
	dest.guid = MonoStringNew(GUIDToString(src.guid));

	ChangeFlags flags = ((Item)src).GetChangeFlags(); // TODO: change this function to const
	string changeString;

	if (flags & kCFDeleted) 
		AddChangesetFlag(changeString, "Deleted"); 
	else
	if (flags & kCFUndeleted) AddChangesetFlag(changeString, "Undeleted"); 
	else
	{
		if (flags & kCFModified) AddChangesetFlag(changeString, "Modified");
		if (flags & kCFRenamed) AddChangesetFlag(changeString, "Renamed");
		if (flags & kCFMoved) AddChangesetFlag(changeString, "Moved");
		if (flags & kCFCreated) AddChangesetFlag(changeString, "Created");
	}

	dest.assetOperations = MonoStringNew(changeString);
	dest.assetIsDir = src.type == kTDir;
	dest.changeFlags = flags;
}

void ChangesetToMono (const ChangesetDummy &src, MonoChangeset &dest)
{
	dest.changeset = src.srcChangeset.changeset;
	dest.message = MonoStringNew(src.srcChangeset.message);
	dest.date = MonoStringNew(src.srcChangeset.GetDateString());
	dest.owner = MonoStringNew(src.srcChangeset.owner);
	dest.items = VectorToMonoClassArray<Item, MonoChangesetItem> (src.items, 
		GetMonoManager().GetBuiltinEditorMonoClass("ChangesetItem"), ItemToMonoChangesetItem);
}

void ChangesetMapToMono(const int &changeset, const ChangesetDummy &src, MonoChangeset &dest)
{
	dest.changeset = changeset;
	dest.message = MonoStringNew(src.srcChangeset.message);
	dest.date = MonoStringNew(src.srcChangeset.GetDateString());
	dest.owner = MonoStringNew(src.srcChangeset.owner);
	dest.items = VectorToMonoClassArray<Item, MonoChangesetItem> (src.items, 
		GetMonoManager().GetBuiltinEditorMonoClass("ChangesetItem"), ItemToMonoChangesetItem);
}

set<UnityGUID> ConvertToGUIDs (MonoArray* array)
{
	set<UnityGUID> cguids;
	
	for (int i = 0; i < mono_array_length_safe(array); i++) 
	{
		UnityGUID guid = StringToGUID(MonoStringToCpp(GetMonoArrayElement<MonoString*> (array, i)));
		if (guid == UnityGUID())
			Scripting::RaiseMonoException ("Invalid GUID");
			
		cguids.insert(guid);
	}
	return cguids;
}

MonoArray* ConvertToGUIDs (const set<UnityGUID>& guids)
{
	vector<string> cstrings;
	cstrings.reserve(guids.size());
	for (set<UnityGUID>::const_iterator i = guids.begin(); i != guids.end(); i++)
	{
		cstrings.push_back(GUIDToString(*i));
	}
	
	return Scripting::StringVectorToMono (cstrings);
}

map<UnityGUID, DownloadResolution> ConvertToResolutions (MonoArray* guids, MonoArray* resolutions)
{
	g_DownloadResolutions.clear();
	
	if (mono_array_length_safe(guids) != mono_array_length_safe(resolutions))
		Scripting::RaiseMonoException ("Resolution and GUID counts do not match");
		
	for (int i = 0; i < mono_array_length_safe(guids); i++) 
	{
		UnityGUID guid = StringToGUID(MonoStringToCpp(GetMonoArrayElement<MonoString*> (guids, i)));
		DownloadResolution resolution = (DownloadResolution)GetMonoArrayElement<int> (resolutions, i);
		
		if (guid == UnityGUID())
			Scripting::RaiseMonoException ("Invalid GUID");
			
		g_DownloadResolutions[guid] = resolution;
	}

	return g_DownloadResolutions;
}

map<UnityGUID, CompareInfo> ConvertToCompareInfo (MonoArray* guids, MonoArray* compInfos)
{
	map<UnityGUID, CompareInfo> cinfos;

	if (mono_array_length_safe(guids) != mono_array_length_safe(compInfos))
		Scripting::RaiseMonoException ("Compare info and GUID counts do not match");
		
	for (int i = 0; i < mono_array_length_safe(guids); i++) 
	{
		UnityGUID guid = StringToGUID(MonoStringToCpp(GetMonoArrayElement<MonoString*> (guids, i)));
		CompareInfo compInfo = (CompareInfo)GetMonoArrayElement<CompareInfo> (compInfos, i);
		
		if (guid == UnityGUID())
			Scripting::RaiseMonoException ("Invalid GUID");
			
		cinfos[guid] = compInfo;
	}

	return cinfos;
}

void CollectSelection(set<UnityGUID>* guids)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	set<int> instances = Selection::GetSelectionID();

	AssetDatabase *db = &AssetDatabase::Get();
	
	for(set<int>::iterator i = instances.begin(); i != instances.end(); i++) 
	{
		UnityGUID guid = pm.GUIDFromAnySerializedPath (pm.GetPathName (*i));
		if (db->IsAssetAvailable (guid))
			guids->insert( guid );
	}
}

}; // namespace AssetServer


void ShowImportPackageGUI (const vector<ImportPackageAsset> &assets, const string& packageIconPath)
{
	MonoClass* klass = GetMonoManager().GetBuiltinEditorMonoClass("AssetsItem");
	void* args[] = { VectorToMonoClassArray<ImportPackageAsset, AssetServer::MonoAssetsItem> (assets, klass, AssetServer::ImportAssetsItemToMono),
					MonoStringNew(packageIconPath) };
	CallStaticMonoMethod("PackageImport", "ShowImportPackage", args);
}

int ShowASConflictResolutionsWindow(const set<UnityGUID>& cconflicting, string backendFunctionForConflictResolutions)
{
	MonoClass* klass = GetMonoManager().GetMonoClass("ASEditorBackend", "UnityEditor");

	if (!klass)
		return false;

	ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(klass,backendFunctionForConflictResolutions.c_str(), ScriptingMethodRegistry::kStaticOnly);

	if (!method)
		return false;

	ScriptingInvocation invocation(method);
	invocation.AddArray(AssetServer::ConvertToGUIDs(cconflicting));
	invocation.InvokeChecked();

	return true;
}

