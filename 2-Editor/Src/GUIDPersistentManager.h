#ifndef GUIDPERSISTENTMANAGER_H
#define GUIDPERSISTENTMANAGER_H

#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/vector_set.h"

enum CaseSensitivityMode { kReusePathCaseSensitive, kForceCaseSensitivityFromInputPath, kNewAssetPathsUseGetActualPathSlow };

class GUIDPersistentManager : public PersistentManager
{
	private:
	
	struct GUIDAndType
	{
		UnityGUID guid;
		SInt32 type;
		
		GUIDAndType (const UnityGUID& g, int t) : guid (g), type (t) {}
		
		friend bool operator < (const GUIDAndType& lhs, const GUIDAndType& rhs)
		{
			int cmp = CompareGUID (lhs.guid, rhs.guid);
			if (cmp == 0)
				return lhs.type < rhs.type;
			else
				return cmp == -1;
		}
	};
	typedef std::map<UnityStr, UnityGUID> PathToGUID;
	typedef map<UnityGUID, UnityStr> GUIDToPath;
	typedef map<GUIDAndType, int> GUIDToIndex;
	typedef map<UnityStr, int> PathToIndex;
	typedef map<UnityStr, UnityGUID> ConstantGUIDs;

	PathToGUID	m_PathToGUID;
	GUIDToPath	m_GUIDToPath;
	
	GUIDToIndex	m_GUIDToIndex;
	PathToIndex	m_PathToIndex;

	// Constant guids are registered RegisterConstantGUID eg. library/versioned/projectsettings.asset has a constant guid of 0,0,4,0
	ConstantGUIDs m_ConstantGUIDs;

	vector<FileIdentifier>	m_IndexToFile;
	
	GUIDPersistentManager(const GUIDPersistentManager& copy); // make copy constructor private
	public:
	
	enum { kNonAssetType = 0, kDeprecatedCachedAssetType = 1, kSerializedAssetType = 2, kMetaAssetType = 3 };
	
	GUIDPersistentManager (int options, int cacheCount = 2);
	
	// Changes the asset's (The asset is defined by the guid) current pathName location by remapping it to this path.
	// This will kill any mappings that existed between the guid that might have been at newPathName before.
	// Returns true on success.
	bool MoveAsset (const UnityGUID& guid, const string& newPathName);
	

	// If the pathname isn't bound to a guid, creates a new guid and maps it to the pathname
	// This only creates the mapping between guid <-> pathname
	UnityGUID CreateAsset (const string& pathName, CaseSensitivityMode mode = kReusePathCaseSensitive);

	// Generates a unique path name from a parentGuid and a file name. Will add
	// a sequential number to the name if the path generated already exists.
	// In order to exclude additional names, one can pass a third parameter
	// consisting of a pointer set of lowercase strings with disallowed names.
	// If the file name ends with a number, it will be stripped off and the
	// counter will start from there.
	string GenerateUniqueAssetPathName (const UnityGUID& parentGuid, const string& name, const set<string>* disallowedAssetNames = NULL);
	string GenerateUniqueAssetPathName (const string& folder, const string& name, const set<string>* disallowedAssetNames = NULL);
	string GenerateUniqueAssetPathName (const string& path, const set<string>* disallowedAssetNames = NULL);


	// Creates a mapping guid <-> pathname
	// Has to be called before any assets are loaded
	// returns the error why the mapping could not be established, empty if successful
	string CreateDefinedAsset (const string& pathName, const UnityGUID& guid);
	/// ----------write docs for this one
	void ForceCreateDefinedAsset (const string& notNicePathName, const UnityGUID& guid);
	
	bool PathNameToGUID (const string& pathName, UnityGUID* guid);
	
	// This will cleanup the guid from the database when writing.
	// We delay deleting meta data files when removing something from the project so that the user can drag his asset back and all 
	// settings are restored.
	void CleanupUnusedGUIDs (const vector_set<UnityGUID>& inUseAssets);
	
	/////////-------------write docs
	const UnityStr& AssetPathNameFromGUID (const UnityGUID& guid);
	void AssetPathNamesFromGUIDs (std::vector<UnityStr>& paths,
								  const std::set<UnityGUID>& guids);
	string AssetPathNameFromAnySerializedPath (const string& anyPath);
	UnityGUID GUIDFromAnySerializedPath (const string& anyPath);

	///// DO NOT USE IF YOU DONT KNOW THE INTERNALS
	///// This function will set all exisiting streams to have empty pathnames.
	///// Thus when someone still has a ptr to the file referenced by this guid. It will be unrestorably lost
	void RemoveGUID (const UnityGUID& guid);

	bool IsConstantGUID (UnityGUID guid);

	/// Registers a constant path -> guid mapping.
	/// Those paths can be outside the assets folder and they are not allowed to be moved or removed
	void RegisterConstantGUID (const string& path, UnityGUID guid);

	
	///////////// PersistentManager interface

	/// Called when translating FileIdentifier from inside a loaded SerializedFile into pathIDs
	/// - on return the pathname should match the one in file.pathname
	virtual int InsertFileIdentifierInternal (FileIdentifier file, bool create);

	protected:
	
	// Called from a lot of places
	virtual int InsertPathNameInternal (const std::string& pathName, bool create);
	
	// GetStreamNameSpace (int pathID) and GetPathName (SInt32 memoryID)
	virtual string PathIDToPathNameInternal (int pathID);

	public:
	// MemoryIDToFileID when putting FileIdentifier into serializedFile
	virtual FileIdentifier PathIDToFileIdentifierInternal (int pathID);
	
	
	
	private:
	
	/// Create a unique asset. If the creation fails. An empty guid will be returned
	UnityGUID CreateUniqueAsset (const string& notNicePathName);
	void MoveGUIDToTrash (const UnityGUID& guid);

	
	friend class GUIDSerializer;
};

string GetMetaDataDirectoryPath ();

const UnityStr& GetAssetPathFromGUID (const UnityGUID& guid);
string GetMetaDataPathFromGUID (const UnityGUID& guid);
string GetMetaDataPathFromAssetPath (const string& path);
string AssetPathNameFromAnySerializedPath (const string& path);

std::string GetPathNameFromGUIDAndType (const UnityGUID& guid, int fileType);

std::string GetTextMetaDataPathFromAssetPath (const std::string & path);
std::string GetTextMetaDataPathFromGUID (const UnityGUID& guid);

inline GUIDPersistentManager& GetGUIDPersistentManager ()
{
	AssertIf (dynamic_cast<GUIDPersistentManager*> (&GetPersistentManager ()) == NULL);
	return reinterpret_cast<GUIDPersistentManager&> (GetPersistentManager ());
}

void InitGUIDPersistentManager();

#endif
