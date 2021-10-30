#ifndef ASSETDATABASE_H
#define ASSETDATABASE_H

#if UNITY_OSX
#include <sys/time.h>
#endif

#include "Runtime/Utilities/dense_hash_map.h"
#include "Runtime/Utilities/vector_set.h"
#include "Runtime/Utilities/HashFunctions.h"
#include "AssetDatabaseStructs.h"

using std::map;
using std::string;
using std::set;
using std::vector;
using std::pair;

class AssetImporter;
class AssetMetaData;
class AssetLabelsDatabase;


class AssetDatabase : public Object
{
	public:
	
	REGISTER_DERIVED_CLASS (AssetDatabase, Object)
	DECLARE_OBJECT_SERIALIZE (AssetDatabase)
	
	AssetDatabase(MemLabelId label, ObjectCreationMode mode);
	// ~AssetDatabase (); declared-by-macro

	static void InitializeClass ();
	static void CleanupClass () {}
	
	/// Find out if an asset needs update:
	/// either by being modified lately, or kForceDisplayPrefs, or kForceUpdate in options
	bool DoesAssetFileNeedUpdate (const string& originalPathName, const DateTime& modificationDate, bool isDirectory, bool isMeta,
		bool isHidden = false);
	
	
	/// Load an asset at pathname and serialize it
	/// You have to call Postprocess after a batch of UpdateAssets
	bool UpdateAsset (const UnityGUID& selfGUID, const UnityGUID& parent, const MdFour& forcedHash, int options, MdFour newhash = MdFour());
	
	/// Saves the import settings to a .meta file next to the asset if this is enabled in the project settings
	bool WriteTextMetaData (const UnityGUID& guid, const Asset& asset, AssetTimeStamp& timeStamp, const vector<UnityStr>& labels, AssetImporter* importer);

	/// Generates a string representing the contents that should be written to the .meta file. Used by export package to be able to calculate
	/// a digest of the meta file even when .meta files are disabled.
	std::string GenerateTextMetaData(const UnityGUID& guid, bool isFolder);
	
	/// Actual implementation of GenerateTextMetaData. Use this function if you already have the importer and labels extracted
	std::string GenerateTextMetaDataIMPL(const UnityGUID& guid, bool isFolder, const vector<UnityStr> & labels, AssetImporter* importer);
	
	/// Moves an asset to newPathName. Returns empty string on success, error message on failure.
	std::string MoveAsset (const UnityGUID& guid, const string& newPathName);
	std::string ValidateMoveAsset (const UnityGUID& guid, const string& newPathName);
	
	bool WriteImportSettingsIfDirty(const UnityGUID& guid);
	
	/// Copies an asset and places it at newPath
	/// Before and after you copy an asset you should UpdateAsset the asset
	bool CopyAsset (const UnityGUID& guid, const string& newPath);

	/// Moves an asset and its children to the trash.
	enum RemoveAssetOptions { kMoveAssetToTrash = 0, kRemoveCacheOnly = 1, kDeleteAssets = 2 };
	bool RemoveAsset (const UnityGUID& guid, RemoveAssetOptions options, set<UnityGUID>* outTrashedAssets = NULL);
	bool RemoveFromHierarchy (const UnityGUID& guid);
	
	/// Tells thasset database to forget any time stamp information it has on the given asset file name.
	void RemoveAssetTimeStamp(const string& path);

	/// Tells the asset database to update the time stamp information on the given asset file name.
	void UpdateAssetTimeStamp(const string& path);

	/// Postprocess has to be called after all assets are updated
	/// It calls all registered postprocess callbacks
	void Postprocess (const set<UnityGUID>& refreshed, const set<UnityGUID>& added, const set<UnityGUID>& removed, const map<UnityGUID, string>& moved);
	
	/// On return collection guid and all deep children of guid in collection is added to collection
	bool CollectAllChildren (const UnityGUID& guid, set<UnityGUID>* collection);
	
	/// Is the Asset available?
	bool IsAssetAvailable (const UnityGUID& guid) { return m_Assets.find (guid) != m_Assets.end (); }
	
	/// Before a refresh we reset these flags, in order to quickly detect which assets have disappear, need a reimport etc.
	void ResetRefreshFlags ();
	void GetNeedRemovalAndAddMetaFile (set<string> *needRemoval, set<string> *needAddMetaFile);

	/// Marks a path as having a meta file, this is used on singleton assets (assets in the library folder).
	/// To mark them so they do not require a reimport.
	void ForceMarkAsFoundMetaFile (const string& originalPathName, bool isHidden = false);
	
	const Asset& GetRoot ();

	const Asset& AssetFromGUID (const UnityGUID& guid) const
	{
		Assets::const_iterator found = m_Assets.find (guid);
		if(found == m_Assets.end ())
		{
			AssertString("Use IsAssetAvailable before calling AssetFromGUID");
		}
		return found->second;
	}

	const Asset* AssetPtrFromGUID (const UnityGUID& guid)
	{
		Assets::iterator found = m_Assets.find (guid);
		if (found != m_Assets.end ())
			return &found->second;
		else
			return NULL;
	}

	const Asset* AssetPtrFromPath (std::string path);
	
	void GetDirtySerializedAssets (set<UnityGUID> &dirtyAssets);
	void WriteSerializedAssets (const set<UnityGUID> &writeAssets);
	
	typedef void PostprocessCallback (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved);
	
	// Returns the object that is most important, PPtr<EditorExtension> is NULL if none is found, bool is true if it should be removed from expanded items list
	typedef int CalculateMostImportantCallback (const vector<LibraryRepresentation>& objects);

	static void RegisterPostprocessCallback (PostprocessCallback* callback);
	static void RegisterCalculateMostImportantCallback (CalculateMostImportantCallback* callback);

	// DidChangeAssetsGraphically is set to true whenever the asset database changes graphical aspects of the database!
	// (Eg. an asset was moved or thumbnail of a texture changed)
	// It can be probed by eg. the library view to see if it needs to redraw.
	// ClearDidChangeAssetsGraphically is to be called before starting an asset import!
	bool DidChangeAssetsGraphically () { return m_DidChangeAssetsGraphically; }
	void SetAssetsChangedGraphically () { m_DidChangeAssetsGraphically = true; }
	void ClearDidChangeAssetsGraphically () { m_DidChangeAssetsGraphically = false; }
	
	static AssetDatabase& Get ();
	static void SetAssetDatabase (AssetDatabase* database);

	void FindAssetsWithName (const string& name, std::set<std::string>& output);
	
#if ENABLE_SPRITES
	void FindAssetsWithImporterClassID (int importerClassId, std::set<UnityGUID>& output);
#endif
	
	bool GetAllResources (std::multimap<UnityStr, PPtr<Object> >& resources);

	std::set<std::string> FindMainAssetsWithClassID (int classID);

	void ReadFailedAssets ();
	void WriteSerializedAsset (const UnityGUID& guid);
	
	bool ShouldIgnoreInGarbageDependencyTracking ();
	
	Assets::const_iterator begin () const { return m_Assets.begin(); }
	Assets::const_iterator end() const { return m_Assets.end(); }
	Assets::size_type size() const { return m_Assets.size (); }
	
	void GetAllAssets (vector_set<UnityGUID>& rootAssets, vector_set<UnityGUID>& realAssets);
	void GetAllAssets (vector_set<UnityGUID>& allAssets);
	void GetAllAssets (std::set<UnityGUID>& allAssets);
	set<UnityGUID> GetAllRootGUIDs ();

	bool AssetTimeStampExistsAndRefreshFoundAsset (const std::string& path);
	
	bool VerifyAssetDatabase();
	void ValidateTimeStampInconsistency ();

	AssetLabelsDatabase& GetLabelsDatabase() { return *m_AssetLabelDB; }
	
	void GenerateAssetImporterVersionHashes(AssetImporterVersionHash& hashes);
	void GetAssetsThatNeedsReimporting (const AssetImporterVersionHash& updatedImporters, std::vector<UnityGUID>& outAssetsToImport) const;
	void RefreshAssetImporterVersionHashesCache ();

	int GetShaderImportVersion () const {return m_UnityShadersVersion;}
	void SetShaderImportVersion (int shaderVersion);

	void ForceTextMetaFileTimeStampUpdate(const UnityGUID& guid);


	private:

	template<class TransferFunction>
	void ReadOldAssetDatabase (TransferFunction& transfer);
	
	void SortAssetsBackwardsCompatibility ();
	void InsertChildSorted (std::vector<UnityGUID>& children, UnityGUID guid, std::string name);
	
	bool ShouldWriteSerializedAsset (const UnityGUID& guid, const std::set<int>* dirty) const;
	void ShouldWriteSerializedAssetRecursive (const UnityGUID& guid, const set<int>* dirtyPaths, set<UnityGUID>& savedAssets) const;

	void AddFailedAssetImporter (const std::string& path);
	void RemoveFailedAssetImporter (const std::string& path);
	void WriteFailedAssets ();
	void WriteCachedMetaDataAndPreviews (const std::string& assetPath, const std::string& metaDataPath, AssetImporter* importer, AssetMetaData* metaData, Asset& asset);
	void CopyLabelsToMetaData(const UnityGUID&, const AssetLabels& labels);
	bool WriteYamlString(const std::string& yaml, const std::string& filename, UInt32 flags);
	
	bool IsAutomaticImportEnabled (const std::string& enabled);

	const Asset& GetParent (const UnityGUID& guid) { return AssetFromGUID (AssetFromGUID (guid).parent); }
	void RemoveFromParent (const UnityGUID& selfGUID, const UnityGUID& parent);
	AssetImporter* ImportAsset (Asset& asset, const string& assetPathName, const string& metaDataPath, int importerClass, int options, const TEMP_STRING& importSettings, bool disallowImport);

	typedef std::vector<PostprocessCallback*>  PostprocessContainer;
	typedef dense_hash_map<UnityStr, AssetTimeStamp, djb2_hash> AssetTimeStamps;

	Assets                   m_Assets;
	set<UnityStr>            m_FailedAssets;
	AssetTimeStamps          m_AssetTimeStamps;
	bool                     m_DidChangeAssetsGraphically;
	AssetLabelsDatabase*     m_AssetLabelDB;
	int                      m_UnityShadersVersion;

	AssetImporterVersionHash m_ImporterVersionHashesCache;
	AssetImporterVersionHash m_lastValidVersionHashes;

		
	static PostprocessContainer				ms_Postprocess;
	static CalculateMostImportantCallback*	ms_MostImportantCallback;
};



/// Creates the metadata class at metaDataPath. This is used by maint and AssetDatabase
AssetMetaData* CreateOrLoadAssetMetaDataAtPath (const string& metaDataPath);

Object* GetMainAsset (const std::string& path);
int GetMainAssetInstanceID (const std::string& path);

/// Is the object the main Asset?
bool IsMainAsset (int instanceID);
bool IsSubAsset (int instanceID);

void GetAllAssetRepresentations (const std::string& path, vector<PPtr<Object> >& output);

typedef UNITY_SET(kMemTempAlloc, Object*) TempSelectionSet;
set<UnityGUID> ObjectsToGuids (const TempSelectionSet& objects);
void GuidsToObjects (const set<UnityGUID>& guids, TempSelectionSet& objects);
UnityGUID ObjectToGUID (PPtr<Object> o);
bool CalculateParentGUID (const UnityGUID& guid, UnityGUID* parentGUID, std::string* warning);

void WriteAssetMetaDataKeepPreviews (const UnityGUID& selfGUID);

UInt32 HashImporterVersion(int classId, UInt32 version);

extern const UnityGUID kAssetFolderGUID;

#endif
