#include "UnityPrefix.h"
#include "GUIDPersistentManager.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Serialize/Remapper.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Threads/ProfilerMutex.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"

#include <sstream>

using namespace std;

const char* kMetaDataFolder = "library/metadata/";
const char* kDeprecatedCacheFolder = "library/cache/";
static const char* kSourceAssets = "Assets";
const UnityStr kEmptyString;

string GetMetaDataPathFromGUID (const UnityGUID& guid);
UnityGUID StringToGUID (const string& guidString);
string GUIDToStringInDirectory (const UnityGUID& guid, const char* directory);

static void DirtyGUIDMapper();

PROFILER_INFORMATION(gLoadLockPersistentManager, "Loading.LockPersistentManager", kProfilerLoading)

string GetMetaDataDirectoryPath ()
{
	return string (kMetaDataFolder, strlen(kMetaDataFolder) - 1);
}

string GetMetaDataPathFromGUID (const UnityGUID& guid)
{
	return GUIDToStringInDirectory (guid, kMetaDataFolder);
}

string GetMetaDataPathFromAssetPath (const string& path)
{
	UnityGUID guid;
	if (GetGUIDPersistentManager ().PathNameToGUID (path, &guid))
		return GetMetaDataPathFromGUID (guid);
	else
		return string ();
}

string AssetPathNameFromAnySerializedPath (const string& anyPath)
{
	return GetGUIDPersistentManager().AssetPathNameFromAnySerializedPath (anyPath);
}

const UnityStr& GetAssetPathFromGUID (const UnityGUID& guid)
{
	return GetGUIDPersistentManager ().AssetPathNameFromGUID (guid);
}

std::string GetPathNameFromGUIDAndType (const UnityGUID& guid, int fileType)
{
	switch (fileType)
	{
		case GUIDPersistentManager::kSerializedAssetType:
			return GetAssetPathFromGUID(guid);
		case GUIDPersistentManager::kMetaAssetType:
		case GUIDPersistentManager::kDeprecatedCachedAssetType:
		case GUIDPersistentManager::kNonAssetType:
			return GetMetaDataPathFromGUID(guid);
		default:
			return std::string ();
	}
}


// Generates 'directory'/ce/ceecf0590afac410fa59c4d380ae3909
string GUIDToStringInDirectory (const UnityGUID& guid, const char* directory)
{
	string name;
	int directorySize = strlen(directory);
	int folderOffset = 3 + directorySize;
	name.resize (32 + folderOffset);
	
	// Divide into folders with names corresponding to the first two chars in the file name
	name.replace(0, directorySize, directory);
	GUIDToString (guid, &name[folderOffset]);
	name[directorySize + 0] = name[folderOffset];
	name[directorySize + 1] = name[folderOffset + 1];
	name[directorySize + 2] = kPathNameSeparator;
	
	return name;
}

std::string GetTextMetaDataPathFromAssetPath (const string & path) 
{
	return Format("%s.meta", path.c_str() );
}

std::string GetTextMetaDataPathFromGUID (const UnityGUID& guid) 
{
	return GetTextMetaDataPathFromAssetPath(GetAssetPathFromGUID(guid));
}

GUIDPersistentManager::GUIDPersistentManager (int options, int cacheCount)
: PersistentManager (options, cacheCount)
{
}

bool GUIDPersistentManager::MoveAsset (const UnityGUID& guid, const string& notNicePathName)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	UnityStr newPathName = GetGoodPathNameForGUID (notNicePathName);
	
	GUIDToPath::iterator i = m_GUIDToPath.find (guid);
	AssertIf (i == m_GUIDToPath.end ());
	// GUID unknown fail!
	if (i == m_GUIDToPath.end ())
		return false;
	// Early out, we are not actually moving anything
	if (newPathName == i->second)
		return true;
	
	string oldPath = GetGoodPathNameForGUID(i->second);

	int serializedPath = InsertPathNameInternal (oldPath, false);
	if (serializedPath != -1 && m_Streams[serializedPath].stream != NULL)
	{
		ErrorString("Moving loaded serialized file!" + oldPath);
	}
	
	FatalErrorIf (m_ConstantGUIDs.count (newPathName));

	// Remove old guid <-> oldPath
	FatalErrorIf (!m_PathToGUID.erase (oldPath));
	m_GUIDToPath.erase (i);
	
	// Replace any previously existing guids at that path
	PathToGUID::iterator replace = m_PathToGUID.find (newPathName);
	if (replace != m_PathToGUID.end ())
	{	
		MoveGUIDToTrash (replace->second);
	}

	// Establish new guid <-> path
	FatalErrorIf (!m_GUIDToPath.insert (make_pair (guid, notNicePathName)).second);
	FatalErrorIf (!m_PathToGUID.insert (make_pair (newPathName, guid)).second);
	
	DirtyGUIDMapper();

	return true;
}


void GUIDPersistentManager::MoveGUIDToTrash (const UnityGUID& guid)
{
	string trashPath = "Assets/__DELETED_GUID_Trash/" + GUIDToString(guid);
	string trashPathGood = GetGoodPathNameForGUID (trashPath);
	
	GUIDToPath::iterator i = m_GUIDToPath.find(guid);
	FatalErrorIf (i == m_GUIDToPath.end());
	
	m_PathToGUID.erase(GetGoodPathNameForGUID(i->second));
	FatalErrorIf (!m_PathToGUID.insert (make_pair (trashPathGood, guid)).second);
	i->second = trashPath;
	DirtyGUIDMapper();
}

////@TODO: THis looks like it will reuse the metadata file from a previously existing asset. That seems so wrong!
/////      Write test and fix
UnityGUID GUIDPersistentManager::CreateUniqueAsset (const string& notNicePathName)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	string pathName = GetGoodPathNameForGUID (notNicePathName);
	Assert (pathName != "assets/");
	Assert (pathName.find("assets/") == 0);
	
	PathToGUID::iterator i = m_PathToGUID.find (pathName);
	// This pathname exists
	if (i != m_PathToGUID.end ())
	{
		if( !IsPathCreated (pathName) )
		{
			// Remove old guid <-> oldPath
			MoveGUIDToTrash(i->second);
		}
		else
			return UnityGUID();
	}

	string actualPath = GetActualPathSlow(notNicePathName);
	
	UnityGUID guid;

	if (guid != UnityGUID())
	{
		ForceCreateDefinedAsset(pathName, guid);
		return guid;
	}
	else
	{
		guid.Init ();
	}

	// Setup guid <-> pathName
	SET_ALLOC_OWNER(this);
	bool didInsert = m_PathToGUID.insert (make_pair (pathName, guid)).second;
	FatalErrorIf (!didInsert);
	didInsert = m_GUIDToPath.insert (make_pair (guid, actualPath)).second;
	FatalErrorIf (!didInsert);
	
	DirtyGUIDMapper();
	
	return guid;
}

UnityGUID GUIDPersistentManager::CreateAsset (const string& notNicePathName, CaseSensitivityMode caseMode)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	string pathName = GetGoodPathNameForGUID (notNicePathName);
	
	FileNameValid validName = CheckValidFileNameDetail(GetLastPathNameComponent(pathName));
	if (validName == kFileNameInvalid)
	{
		ErrorStringMsg ("%s is not a valid asset file name. Please make sure there are no slashes or other unallowed characters in the file name. The file will be ignored.", pathName.c_str());
		return UnityGUID ();
	}
	
	if (pathName == "assets/")
	{
		ErrorString ("CreateAsset: Cannot create Assets Folder!");
		return UnityGUID ();
	}

	if (pathName.find("/../") != std::string::npos)
	{
		ErrorString ("CreateAsset: Cannot create Assets with relative path!");
		return UnityGUID ();		
	}
	
	PathToGUID::iterator i = m_PathToGUID.find (pathName);
	// This pathname exists
	if (i != m_PathToGUID.end ())
	{
		// Make sure the m_GUIDToPath mapping to the path is accurate.
		if (caseMode == kForceCaseSensitivityFromInputPath)
		{
			GUIDToPath::iterator guidToPathFound = m_GUIDToPath.find(i->second);
			Assert(guidToPathFound != m_GUIDToPath.end());
			if (guidToPathFound->second != notNicePathName)
			{
				guidToPathFound->second = notNicePathName;
				DirtyGUIDMapper();
			}	
		}
		
		return i->second;
	}
	
	UnityGUID guid;
	// Create guid
	if (m_ConstantGUIDs.count (pathName) == 0)
	{
		if (pathName.find ("assets/") != 0)
			return UnityGUID ();

		// Create a GUID that is not yet known...
		while (guid == UnityGUID())
		{	
			guid.Init ();
			if (m_GUIDToPath.count (guid) != 0)
				guid = UnityGUID();
		}
	}
	else
	{
		guid = m_ConstantGUIDs.find (pathName)->second;
	}

	if (caseMode == kNewAssetPathsUseGetActualPathSlow)
		ForceCreateDefinedAsset(GetActualPathSlow(notNicePathName), guid);
	else
		ForceCreateDefinedAsset(notNicePathName, guid);

	return guid;
}

string GUIDPersistentManager::GenerateUniqueAssetPathName (const string& path, const set<string>* disallowedAssetNames)
{
	return GenerateUniqueAssetPathName(DeleteLastPathNameComponent(path), GetLastPathNameComponent(path), disallowedAssetNames);
}

string GUIDPersistentManager::GenerateUniqueAssetPathName (const UnityGUID& parentGuid, const string& name, const set<string>* disallowedAssetNames)
{
	string folder;
	string pathName = AssetPathNameFromGUID (parentGuid);
	if (IsDirectoryCreated (pathName))
		folder = pathName;
	else
		folder = DeleteLastPathNameComponent (pathName);
	
	return GenerateUniqueAssetPathName(folder, name, disallowedAssetNames);
}
	
string GUIDPersistentManager::GenerateUniqueAssetPathName (const string& folder, const string& uncleanName, const set<string>* disallowedAssetNames)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	CreateDirectory (kSourceAssets);
	if(! IsDirectoryCreated(folder))
		return string();
	
	string name = MakeFileNameValid(uncleanName);
	if (name.empty())
		return string();
	
	string base = DeletePathNameExtension(name);
	string ext = GetPathNameExtension(name);
	string dot = (base == name?"":"."); // Don't add dot if there was no dot to begin with
	bool isScript = (ext == "cs" || ext == "js" || ext == "boo");
	
	// detect if base ends with a number, strip it off and begin counting from there.
	int initial = 0;
	string::size_type nonNum = base.find_last_not_of ("0123456789");
	string numPad = ""; // Used to keep any zero-padding if already there
	
	if(base.size() - nonNum  > 9) // we only care about the last 8 digits
		nonNum = base.size()-1;
	
	// the name does not end with a number
	if (nonNum != string::npos && nonNum >= base.size()-1)
	{ 
		// if the asset is not a script, add a space to make it look nicer if the name doesn't already end with a space
		if (!isScript && base[base.size()-1] != ' ')
			base+=" ";
	}
	// the name ends with a number
	else
	{ 
		numPad = "0" + IntToString(base.size() - nonNum - 1);
		initial = StringToInt(base.substr(nonNum + 1));
		base = base.substr(0,nonNum+1);
	}
	
	// Generate a custom format string for inserting the number into the file name
	string nameTemplate =  "%s%" + numPad +"d" + dot + "%s";
	
	// Create unique pathName in folder with name and extension
	// If asset - guid mappping already exists, keep trying
	UnityGUID newGUID;
	string newPathName;
	for (int i=initial;i<initial+5000 && newGUID == UnityGUID();i++)
	{
		string renamed;
		if (i == 0)
			renamed = name;
		else
			renamed = Format(nameTemplate.c_str(), base.c_str(), i, ext.c_str() );
		
		newPathName = AppendPathName (folder, renamed);

		// Skip names explicitly disallowed by the caller
		if( disallowedAssetNames != NULL && disallowedAssetNames->find(ToLower(renamed)) != disallowedAssetNames->end() )
			continue;

		newGUID = CreateUniqueAsset (newPathName);
	}
	
	if (newGUID == UnityGUID())
		return string();
	else
		return newPathName;
}

string GUIDPersistentManager::CreateDefinedAsset (const string& notNicePathName, const UnityGUID& guid)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	std::string pathName = GetGoodPathNameForGUID (notNicePathName);
	string error;
	if (m_GUIDToPath.find (guid) != m_GUIDToPath.end ())
	{
		if (GetGoodPathNameForGUID(m_GUIDToPath.find (guid)->second) != pathName)
			error = "The GUID " + GUIDToString (guid) + " is already mapped to " + (std::string)m_GUIDToPath.find (guid)->second + ". But the meta data wants it to be mapped to " + pathName;
		return error;
	}
	if (m_PathToGUID.find (pathName) != m_PathToGUID.end ())
	{
		if (m_PathToGUID.find (pathName)->second != guid)
			error = "The pathName " + pathName + " is already mapped to " + GUIDToString (m_PathToGUID.find (pathName)->second) + ". But the meta data wants it to be mapped to " + GUIDToString (guid);
		return error;
	}	
	SET_ALLOC_OWNER(this);
	m_GUIDToPath.insert (make_pair (guid, notNicePathName));
	m_PathToGUID.insert (make_pair (pathName, guid));
	
	DirtyGUIDMapper();
	
	return error;
}

void GUIDPersistentManager::ForceCreateDefinedAsset (const string& notNicePathName, const UnityGUID& guid)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	string pathName = GetGoodPathNameForGUID (notNicePathName);
	
	GUIDToPath::iterator a = m_GUIDToPath.find (guid);
	if (a != m_GUIDToPath.end ())
	{
		string aGood = GetGoodPathNameForGUID (a->second);
		if( aGood != pathName ) 
			printf_console("*** %s replaces %s for guid %s \n", pathName.c_str(), aGood.c_str(), GUIDToString(guid).c_str());

		FatalErrorIf (m_PathToGUID.erase (aGood) != 1);
		m_GUIDToPath.erase (a);
	}

	PathToGUID::iterator b = m_PathToGUID.find (pathName);
	if (b != m_PathToGUID.end ())
	{
		if( b->second != guid ) 
			printf_console("*** %s replaces %s at path %s \n", GUIDToString(guid).c_str(), GUIDToString(b->second).c_str(), pathName.c_str());

		MoveGUIDToTrash(b->second);
	}
	SET_ALLOC_OWNER(this);
	ErrorIf (!m_GUIDToPath.insert (make_pair (guid, notNicePathName)).second);
	ErrorIf (!m_PathToGUID.insert (make_pair (pathName, guid)).second);
	
	DirtyGUIDMapper();
}


bool GUIDPersistentManager::PathNameToGUID (const string& notNicePathName, UnityGUID* guid)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	const string& pathName = GetGoodPathNameForGUID (notNicePathName);
	PathToGUID::iterator i = m_PathToGUID.find (pathName);
	// This pathname exists
	if (i != m_PathToGUID.end ())
	{
		*guid = i->second;
		return true;
	}
	else
		return false;
}

const UnityStr& GUIDPersistentManager::AssetPathNameFromGUID (const UnityGUID& guid)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	GUIDToPath::iterator found = m_GUIDToPath.find (guid);
	if (found != m_GUIDToPath.end ())
		return found->second;
	else
		return kEmptyString;
}

void GUIDPersistentManager::AssetPathNamesFromGUIDs (std::vector<UnityStr>& paths, const std::set<UnityGUID>& guids)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	paths.reserve(paths.size());
	for (std::set<UnityGUID>::const_iterator i = guids.begin(); i != guids.end(); ++i)
	{
		GUIDToPath::const_iterator found = m_GUIDToPath.find (*i);
		if (found != m_GUIDToPath.end ())
			paths.push_back(found->second);
		else
			paths.push_back(kEmptyString);
	}
}

int GUIDPersistentManager::InsertPathNameInternal (const std::string& notNicePathName, bool create)
{
	SET_ALLOC_OWNER(this);
	string pathName = GetGoodPathNameForGUID (notNicePathName);

	PathToIndex::iterator i = m_PathToIndex.find (pathName);
	if (i == m_PathToIndex.end ())
	{
		string guidString = GetLastPathNameComponent (pathName);
		// Do we have a asset meta data file?
		if (pathName.find (kMetaDataFolder) == 0)
		{
			UnityGUID guid = StringToGUID (guidString);
			if (guid != UnityGUID ())
				return InsertFileIdentifierInternal (FileIdentifier ("", guid, kMetaAssetType), create);
		}
		// Do we have a asset cache file
		else if (pathName.find (kDeprecatedCacheFolder) == 0)
		{
			AssertString("Insert path at cache directory... wrong???");
			UnityGUID guid = StringToGUID (guidString);
			if (guid != UnityGUID ())
				return InsertFileIdentifierInternal (FileIdentifier ("", guid, kMetaAssetType), create);
		}
		// Do we have a serialized asset file? (in Assets/ folder or a singleton manager in Library/)
		else if (pathName.find ("assets/") == 0)
		{	
			//@TODO: Calling Create asset even though we are not in create mode seems wrong.
			//       Write a test and remove it. (Could be because if you import an asset with a reference to another asset that does not yet exist in the guidmapper, then it could potentially have an incorrect guid mapping...)
			//      
			UnityGUID guid = CreateAsset (notNicePathName, kReusePathCaseSensitive);
			if (guid != UnityGUID ())
				return InsertFileIdentifierInternal (FileIdentifier ("", guid, kSerializedAssetType), create);
			else
				return -1;
		}
		
		// Setup non asset file identifier
		return InsertFileIdentifierInternal (FileIdentifier (pathName, UnityGUID (), kNonAssetType), create);
	}
	else
		return i->second;
}

string GUIDPersistentManager::AssetPathNameFromAnySerializedPath (const string& anyPath)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	int pathID = InsertPathNameInternal (anyPath, true);
	if (pathID == -1)
		return string();

	FileIdentifier f = PathIDToFileIdentifierInternal (pathID);
	return AssetPathNameFromGUID (f.guid);
}


UnityGUID GUIDPersistentManager::GUIDFromAnySerializedPath (const string& anyPath)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	int pathID = InsertPathNameInternal (anyPath, true);
	if (pathID == -1)
		return UnityGUID();
	
	FileIdentifier f = PathIDToFileIdentifierInternal (pathID);
	return f.guid;
}

	
int GUIDPersistentManager::InsertFileIdentifierInternal (FileIdentifier file, bool create)
{
	SET_ALLOC_OWNER(this);
	file.pathName = GetGoodPathNameForGUID(file.pathName);
	
	// Handle non assets (this is just pathname to index translation)
	if (file.type == kNonAssetType)
	{
		Assert(file.pathName.find ("assets/") != 0);
		
		// Make sure the path contains the guid from the constant GUID map
		// (For example builtin resources are non-asset types)
		if (m_ConstantGUIDs.count(file.pathName) == 1)
			file.guid = m_ConstantGUIDs[file.pathName];
		
		// Find guid -> pathID
		GUIDAndType guidAndType (file.guid, file.type);
		GUIDToIndex::iterator found2 = m_GUIDToIndex.find (guidAndType);
		if (found2 != m_GUIDToIndex.end ())
			return found2->second;

		// Find pathname -> pathID
		PathToIndex::iterator found = m_PathToIndex.find (file.pathName);
		if (found != m_PathToIndex.end ())
		{
			AssertIf (file.pathName != PathIDToPathNameInternal (found->second));
			return found->second; 
		}

		if (create && !file.pathName.empty())
		{
			// no such pathname known, create new one
			AddStream ();
			
			file.CheckValidity();
			
			int pathID = m_IndexToFile.size ();
			m_IndexToFile.push_back (file);

			// insert GUID 
			if (file.guid != UnityGUID())
			{
				bool did = m_GUIDToIndex.insert (make_pair (guidAndType, pathID)).second;
				AssertIf (!did);
			}
			
			bool did = m_PathToIndex.insert (make_pair (file.pathName, pathID)).second;
			AssertIf (!did);

			return pathID;
		}
		else
			return -1;
	}
	// Handle assets (this is guid+type -> index translation)
	else
	{
		if (file.guid == UnityGUID ())
			FatalErrorString ("guid is uninitialized");

		// Check if the type is corresponding to the pathName
		FatalErrorIf (file.type == kDeprecatedCachedAssetType);
		FatalErrorIf (create && file.type == kMetaAssetType && file.pathName.find (kMetaDataFolder) != 0 && !file.pathName.empty());
		file.CheckValidity();
		
		// Find guid+type -> pathID
		GUIDAndType guidAndType (file.guid, file.type);
		GUIDToIndex::iterator found = m_GUIDToIndex.find (guidAndType);
		if (found != m_GUIDToIndex.end ())
			return found->second;
		
		if (create)
		{
			// no such guid+type known, create new one
			AddStream ();
			int pathID = m_IndexToFile.size ();
			m_IndexToFile.push_back (file);
			bool did = m_GUIDToIndex.insert (make_pair (guidAndType, pathID)).second;
			AssertIf (!did);

			return pathID;
		}
		else
			return -1;
	}
}

string GUIDPersistentManager::PathIDToPathNameInternal (int pathID)
{
	int type = m_IndexToFile[pathID].type;
	if (type == kNonAssetType)
		return m_IndexToFile[pathID].pathName;

	const UnityGUID& guid = m_IndexToFile[pathID].guid;

	// meta asset data are looked up simply by their guid encoded as string inside "Library/MetaData"
	if (type == kMetaAssetType)	return GetMetaDataPathFromGUID (guid);
	// Serialized assets (.mat, .prefab) are simply m_GUIDToPath map
	else if (type == kSerializedAssetType)
	{
		// The guid doesn't exist!
		GUIDToPath::iterator i = m_GUIDToPath.find (guid);
		if (i == m_GUIDToPath.end ())
			return string ();
		else return i->second;
	}

	ErrorString ("Fatal Unknown file type in guid lookup");
	return string ();
}

FileIdentifier GUIDPersistentManager::PathIDToFileIdentifierInternal (int pathID)
{
	int type = m_IndexToFile[pathID].type;
	if (type == kNonAssetType)
		return FileIdentifier (m_IndexToFile[pathID].pathName, m_IndexToFile[pathID].guid, kNonAssetType);

	const UnityGUID& guid = m_IndexToFile[pathID].guid;
	GUIDToPath::iterator i = m_GUIDToPath.find (guid);

	// The guid doesn't exist!
	if (i == m_GUIDToPath.end ())
		return m_IndexToFile[pathID];

	// Check if the type is corresponding to the pathName
	// meta asset data are looked up simply by their guid encoded as string inside "Library/MetaData"
	if (type == kMetaAssetType)
		return FileIdentifier ("", guid, kMetaAssetType);
	// Serialized assets (.mat, .prefab) are simply m_GUIDToPath map
	else if (type == kSerializedAssetType)
		return FileIdentifier ("", guid, kSerializedAssetType);
	
	
	ErrorString ("Fatal Unknown file type in guid lookup");	
	
	return FileIdentifier ();
}

void GUIDPersistentManager::RemoveGUID (const UnityGUID& guid)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	// If the asset file still has exisings objects loaded this is really really bad
	set<SInt32> loadedIDs;
	string assetPath = AssetPathNameFromGUID (guid);
	int pathID = InsertPathNameInternal (assetPath, false);
	if (pathID != -1)
	{
		m_Remapper->GetAllLoadedObjectsAtPath (pathID, &loadedIDs);
		if (!loadedIDs.empty ())
		{
			ErrorString ("Failed to remove asset meta data because some of its object's were still loaded!\n(" + assetPath + ") [" + GUIDToString (guid) + "]");
		}
	}	

	// Delete cache and metadata
	DeleteFile (GetMetaDataPathFromGUID (guid), PersistentManager::kDeleteLoadedObjects);

	// Remove the guid mapping
	GUIDToPath::iterator i = m_GUIDToPath.find (guid);
	FatalErrorIf (i == m_GUIDToPath.end ());
	
	PathToGUID::iterator i2 = m_PathToGUID.find (GetGoodPathNameForGUID(i->second));
	FatalErrorIf (i2 == m_PathToGUID.end ());

	m_GUIDToPath.erase (i);
	m_PathToGUID.erase (i2);
	
	DirtyGUIDMapper();
	
	// Also if there are open files referencing this guid.
	// Delete those references
	for (int j=kSerializedAssetType;j<=kMetaAssetType;j++)
	{
		GUIDToIndex::iterator found = m_GUIDToIndex.find (GUIDAndType (guid, j));
		if (found != m_GUIDToIndex.end ())
		{
			m_IndexToFile[found->second].pathName.clear ();
			m_IndexToFile[found->second].guid = UnityGUID ();
			m_IndexToFile[found->second].type = kNonAssetType;
			m_GUIDToIndex.erase (found);
		}
	}
}

void GUIDPersistentManager::CleanupUnusedGUIDs (const vector_set<UnityGUID>& inUseAssets)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	set<UnityGUID> cleanup;
	for (GUIDToPath::iterator i=m_GUIDToPath.begin ();i != m_GUIDToPath.end ();i++)
	{
		UnityGUID guid = i->first;
		
		// In use assets and constant guids 
		if (inUseAssets.count (guid) == 0 && m_ConstantGUIDs.count(i->second) == 0)
			cleanup.insert (guid);
	}
	
	for (set<UnityGUID>::iterator i=cleanup.begin ();i != cleanup.end ();i++)
		RemoveGUID (*i);
}

void GUIDPersistentManager::RegisterConstantGUID (const string& path, UnityGUID guid)
{
	string goodPath = GetGoodPathNameForGUID (path);

	Assert(m_PathToIndex.count(goodPath) == 0);
	Assert(m_PathToGUID.count(goodPath) == 0);
	
	for (ConstantGUIDs::iterator i=m_ConstantGUIDs.begin();i!=m_ConstantGUIDs.end();i++)
	{
		Assert(i->first != goodPath);
		Assert(i->second != guid);
	}

	m_ConstantGUIDs.insert (make_pair (goodPath, guid));
}

bool GUIDPersistentManager::IsConstantGUID (UnityGUID guid)
{
	for (ConstantGUIDs::iterator i=m_ConstantGUIDs.begin();i!=m_ConstantGUIDs.end();i++)
	{
		if (i->second == guid)
			return true;
	}
	return false;
}


class GUIDSerializer : public Object
{
	public:
	
	DECLARE_OBJECT_SERIALIZE (GUIDSerializer)
	REGISTER_DERIVED_CLASS (GUIDSerializer, Object)
	virtual bool ShouldIgnoreInGarbageDependencyTracking () {return true;}

	GUIDSerializer (MemLabelId label, ObjectCreationMode mode);
};

static GUIDSerializer* gGUIDSerializer = NULL;


GUIDSerializer::GUIDSerializer (MemLabelId label, ObjectCreationMode mode)
 : Super(label, mode)
{
	gGUIDSerializer = this;
}

static void DirtyGUIDMapper()
{
	if (gGUIDSerializer != NULL)
	{
		// This might be called from another thread, so we avoid calling SetDirty
		gGUIDSerializer->SetPersistentDirtyIndex(1);
	}
}

GUIDSerializer::~GUIDSerializer ()
{}

template<class TransferFunction>
void GUIDSerializer::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	
	GUIDPersistentManager& manager = GetGUIDPersistentManager ();
	
	transfer.SetVersion(2);
	
	map<UnityGUID, UnityStr> guidToPath;
	map<UnityGUID, UnityStr>::iterator i;
	GUIDPersistentManager::ConstantGUIDs::iterator c;

	if (transfer.IsWriting ())
	{
		// Build GUID to path map (Clean of any constant guids)
		set<UnityGUID> constantGUIDs;
		for (c=manager.m_ConstantGUIDs.begin();c != manager.m_ConstantGUIDs.end();c++)
			constantGUIDs.insert(c->second);
		for (i=manager.m_GUIDToPath.begin ();i != manager.m_GUIDToPath.end();i++)
		{
			if (constantGUIDs.count(i->first) == 0)
				guidToPath.insert(*i);
		}
	}
	transfer.Transfer (guidToPath, "guidToPath");
	
	if (transfer.DidReadLastProperty ())
	{
		SET_ALLOC_OWNER(NULL);
		manager.m_GUIDToPath.clear ();
		manager.m_PathToGUID.clear ();
		
		for (i=guidToPath.begin ();i != guidToPath.end ();i++)
		{
			bool didInsert = manager.m_PathToGUID.insert (make_pair (GetGoodPathNameForGUID(i->second), i->first)).second;
			AssertIf (!didInsert);
			
			if (transfer.IsOldVersion(1))
				didInsert = manager.m_GUIDToPath.insert (make_pair (i->first, GetActualPathSlow(i->second))).second;
			else
				didInsert = manager.m_GUIDToPath.insert (make_pair (i->first, i->second)).second;
			
			AssertIf (!didInsert);
		}
	}
}

void InitGUIDPersistentManager()
{
	UNITY_NEW_AS_ROOT( GUIDPersistentManager(0), kMemManager, "GUIDManager", "");
}

IMPLEMENT_CLASS (GUIDSerializer)
IMPLEMENT_OBJECT_SERIALIZE (GUIDSerializer)
