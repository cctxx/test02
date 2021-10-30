#ifndef ASSETIMPORTER_H
#define ASSETIMPORTER_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Utilities/DateTime.h"
#include "Editor/Src/Utility/YAMLNode.h"
#include "Runtime/Utilities/GUID.h"
#include "AssetDatabaseStructs.h"
#include "Runtime/Serialize/WriteData.h"
#include "Runtime/Utilities/dynamic_array.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>

using std::string;
class Image;
class Texture2D;
class AssetMetaData;

const int kAllImporterVersion = 1;
enum 
{
	kAssetMetaDataFileID = 1,
	kAssetImporterFileID = 3,
	kMainAssetRepresentionFallbackFileID = 5,
};

struct AssetImporterSelection
{
	int importerClassID;
	int queueIndex;
	int generatedAssetType;
	int importerVersion;
	
	AssetImporterSelection()
	{
		queueIndex = 0;
		importerClassID = 0;
		generatedAssetType = -1;
		importerVersion = 1;
	}
};

/// An AssetImporter is used by the AssetDatabase to convert an asset file (textures/ sound/ meshes etc.) into 
/// an Object that can be serialized, like converting a jpg texture file into a Texture2D class, which can then 
/// be made persistent and serialized to disk. The Texture2D class should not know about how to load from assets.

/// void GenerateAssetData has to be overridden in order to convert the asset file to the serializable object.
/// You can get the AssetsPathName that should be loaded by using: GetCompleteAssetPathName ()
/// Inside GenerateAssetData you have to create the object, load it 
/// somehow from the pathname and add the instanceID of the the created objects to objectsLoadedFromAsset
/// If you generate datatemplates you have to do the same for the datatemplates

class AssetImporter : public NamedObject
{
  private:
	int          m_ImportFlags;

	public:
	
	REGISTER_DERIVED_ABSTRACT_CLASS (AssetImporter, NamedObject)
	DECLARE_OBJECT_SERIALIZE (AssetImporter)

	typedef dynamic_array<WriteData>                   WriteDataArray;

	
	AssetImporter (MemLabelId label, ObjectCreationMode mode);
	
	virtual void Reset ();
	
	// **** How does ID generation work and why should I care? ****
	// Assets are generated at specific LocalIdentifierInFile. These ids aim to be stable when reimporting assets.
	// For example if you have a mesh called "House" it will write this id and the name to a map, and will always reuse that ID.
	// This way even if you temporarily remove the house and then readd it with the same name, it will be regenerated into the same LocalIdentifierInFile.
	// Thus references to the Mesh will never get lost. This mapping information is stored in the .meta file.
	// ProduceAssetObject and RecycleExistingAssetObject, etc perform this ID generation implicitly based on the name you pass.
	// If the name is empty, we will not write it into the map and .meta file, for example the texture importer has no need for storing it,
	// because there is only one texture generated so we always know it will be the first ID in the range reserved for ClassID (Texture).
	// Basically it calls GenerateFileID directly in that case.


	/// Produces an asset object. Completely ignores any previously imported data, sets it up to be written to disk.
	/// When the import is done, all objects must be unloaded (AssetImporter.UnloadObjectsAfterImport does this by default)
	template<class T>
	T& ProduceAssetObject (const string& name = std::string());

	/// Recycles an existing object. Does not create any new objects if one already exists on disk or memory.
	template<class T>
	T& RecycleExistingAssetObject (const string& name = std::string());
	
	/// Register an object that has already been allocated.
	void RegisterObject (Object& object, const std::string& name);

	/// Objects created by the asset importer do not have the same ID as the actual objects.
	/// This way we dont have to register all assets in the Remapper when importing.
	/// However some objects must be known
	PPtr<Object> ExtractFinalPPtrForObject (Object& object);
	
	template<class T>
	T* ProduceAssetObjectHashBased (const string& name);

	template<class T>
	T* RecycleExistingAssetObjectHashBased (const string& name);

	/// Finds the first object at a pathname that is derived from the template type
	template<class T>
	static T* GetFirstDerivedObjectAtPath (const string& name);

	/// Finds the first object at a pathname that is derived from the template type
	template<class T>
	T* GetFirstDerivedObjectNamed (const string& name);
	
	/// Set the thumbnail for an object
	void SetThumbnail (const Image& image, int instanceID);
	
	/// Set up for import... 
	/// This is called before GenerateAssetData. The default implementation deletes all objects 
	/// so they can be recreated by the Importer...
	void StartImport ();
	
	/// End the import. this is where cleanup code should go.
	virtual void EndImport (dynamic_array<Object*>& objects);
	
	/// Override to implement Asset Loading functionality.
	virtual void GenerateAssetData () = 0;
	
	/// Returns the assetpathname relative to the project directory
	string GetAssetPathName () const;
	/// Returns the absolute assetpathname 
	string GetAbsoluteAssetPathName () const;
	
	string GetMetaDataPath() const;
	string GetTextMetaDataPath() const;

	/// Marks all assets as being in use.
	/// This is used to revert to the last import state.
	/// Eg. when mesh importing fails. You can abort importing and revert to the last state.
	/// You should however not have changed or added any new objects yet.
	void MarkAllAssetsAsUsed ();
	
	/// Add a warning/error which will be serialized!
	/// If the error string is "" it will be ignored!
	#define LogImportError(x) AddImportError (x, __FILE__, __LINE__, kLog | kAssetImportError, NULL)
	#define LogImportWarning(x) AddImportError (x, __FILE__, __LINE__, kLog | kAssetImportWarning, NULL)
	#define LogImportErrorObject(x,o) AddImportError (x, __FILE__, __LINE__, kLog | kAssetImportError, o)
	#define LogImportWarningObject(x,o) AddImportError (x, __FILE__, __LINE__, kLog | kAssetImportWarning, o)
	void AddImportError (const std::string& error, const char* file, int line, int mode, const EditorExtension* object);
	
	/// Determines if the asset can be imported by the importer registered with RegisterCanLoadPathNameCallback.
	/// the path name to be loaded
	/// The asset importing is sorted by the queue index (default is 0)
	/// Returns kCopyAsset, kSerializedAsset, kFolderAsset
	typedef int CanLoadPathNameCallback (const string& pathName, int* queue);

	/// The callback that determines if the asset importer /classID/ can import the asset.
	/// The order in which CanLoadPathNameCallback are called to determine who gets to import the asset
	static void RegisterCanLoadPathNameCallback (CanLoadPathNameCallback* callback, int classID, UInt32 assetImporterVersion, int order = 0);
	
	// Detects which asset importer & queue & type should be used for the asset at completePathName.
	static AssetImporterSelection FindAssetImporterSelection (const string& completePathName);

	/// individualAssetProgress 0...1 can be used to indicate progress for large imports (eg movie encoding)
	/// totalOverrideProgress -1 or 0...1 defines an explicit progress value (-1.0 is used to display the normal progress bar)
	/// displayText is the text shown below the progress bar usually indicates the asset name.
	typedef void UpdateProgressCallback (float individualAssetProgress, float totalOverrideProgress, const std::string& displayText);
	static void SetUpdateProgressCallback (UpdateProgressCallback* progressCallback);
	static UpdateProgressCallback* GetUpdateProgressCallback ();
	
	
	virtual void UnloadObjectsAfterImport (UnityGUID guid);

	
	/// The import flags passed in from the AssetDatabase
	/// eg. UpdateAssetOptions.kForceDisplayPrefs
	int GetImportFlags() { return m_ImportFlags; }
	
	void ReloadAssetsThatWereModifiedOnDisk ();
	

	/// Validates if we can upload the imported asset to the cache server.
	/// By default it checks that no errors have occurred during import.
	/// Some importers also check that the asset was imported without "don't compress assets on import"
	virtual bool ValidateAllowUploadToCacheServer();
		
	/// Text-based import settings interface
	
	/// Called after a meta data file has been seen to modify the import settings.
	/// The settings argument points to a YAMLMapping containing a key/value pairs produced by GenerateImportSettings()
	void ApplyImportSettings( const TEMP_STRING& meta ) ;
	
	/// Should populate the YAMLMapping opject passed in with keys of all import settings that are different from the default settings.
	/// Should also add an importerVersion key for ensuring future backwards compatibility.
	void GenerateImportSettings( std::string& meta );
	
	static void GetAllAssetImporterClassIDAndVersion (std::map<int, UInt32>& assetImporterClassToVersion);
	
	GET_SET_DIRTY(std::string, UserData, m_UserData)

	WriteDataArray& GetWriteDataArray () { return m_WriteDataArray; }

	/// Clear data that may have been left over from a previous import.  This is used if
	/// an asset got blacklisted and substituted by a default asset.
	virtual void ClearPreviousImporterOutputs () {}

	Object& ProduceAssetDeprecated (int classID, int recycleClassID);
	Object& ProducePreviewAsset (int classID, int previewAssetClassID);


	protected:
	
	template<class TransferFunction>
	void PostTransfer (TransferFunction& transfer);
	
	/// Override this method in subclasses if the text based meta files need to retain the FileIDToRecycleName mapping when reimporting
	/// assets. Currently, only the ModelImporter does this.
	virtual bool NeedToRetainFileIDToRecycleNameMapping();

	/// Asset database interface
	void Init (int options);

	protected:
	std::map<LocalIdentifierInFileType, UnityStr> m_FileIDToRecycleName;
	std::multimap<UnityStr, LocalIdentifierInFileType> m_RecycleNameToFileID;

	private:
	std::set<LocalIdentifierInFileType>        m_UsedFileIDs;
	WriteDataArray                             m_WriteDataArray;
	
	UnityStr m_UserData;
	
	struct ImportError
	{
		UnityStr error;
		int mode;
		int line;
		UnityStr file;
		PPtr<Object> object;
	};
	
	typedef std::list<ImportError> ImportErrors;
	ImportErrors                   m_ImportErrors;

	// Generates identifier in file in various ways (Hashing or by storing it in the metafile)
	LocalIdentifierInFileType GenerateFileID (int classID, const std::string& name);
	LocalIdentifierInFileType GenerateFileIDHashBased (int classID, const std::string& name);
	LocalIdentifierInFileType RecycleFileID (int classID, const string& name);
	
	// Produce objects internal functions
	Object* GenerateAssetObjectHashBasedImpl (int classID, const string& name, bool recycle);
	Object& GenerateAssetObjectImpl (int classID, const string& name, bool recycle);

	Object& GenerateAssetObjectImpl (int classID, const string& name, LocalIdentifierInFileType localIdentifier, bool recycle);

	
	static Object* GetFirstDerivedObjectAtPathImpl (const string& name, int classID);
	Object* GetFirstDerivedObjectNamedImpl (const string& name, int classID);
	
	void RegisterObject (Object& object, LocalIdentifierInFileType fileID);
	
	Image GetThumbnailForInstanceID (int instanceID);
		
	friend class AssetDatabase;
	friend class CacheServer;
};


template<class T>
T* AssetImporter::GetFirstDerivedObjectNamed (const string& name)
{
	return dynamic_pptr_cast<T*> (GetFirstDerivedObjectNamedImpl (name, T::GetClassIDStatic ()));
}

template<class T>
T* AssetImporter::GetFirstDerivedObjectAtPath (const string& name)
{
	return dynamic_pptr_cast<T*> (GetFirstDerivedObjectAtPathImpl (name, T::GetClassIDStatic ()));
}


template<class T> 
T* AssetImporter::ProduceAssetObjectHashBased (const string& name)
{
	Object* o = GenerateAssetObjectHashBasedImpl(T::GetClassIDStatic (), name, false);
	return static_cast<T*> (o);
}

template<class T> 
T* AssetImporter::RecycleExistingAssetObjectHashBased (const string& name)
{
	Object* o = GenerateAssetObjectHashBasedImpl(T::GetClassIDStatic (), name, true);
	return static_cast<T*> (o);
}

template<class T> 
T& AssetImporter::ProduceAssetObject (const string& name)
{
	Object& o = GenerateAssetObjectImpl (T::GetClassIDStatic (), name, false);
	AssertIf (dynamic_pptr_cast<T*> (&o) == NULL);
	return static_cast<T&> (o);
}

template<class T> 
T& AssetImporter::RecycleExistingAssetObject (const string& name)
{
	Object& o = GenerateAssetObjectImpl (T::GetClassIDStatic (), name, true);
	AssertIf (dynamic_pptr_cast<T*> (&o) == NULL);
	return static_cast<T&> (o);
}

template<class TransferFunction>
void AssetImporter::PostTransfer (TransferFunction& transfer) 
{
	transfer.Align();
	TRANSFER(m_UserData);
}

struct smaller_name_classid : std::binary_function<Object*, Object*, std::size_t>
{
	bool operator () (Object* lhs, Object* rhs) const;
};


enum { kMaxObjectsPerClassID = 100000 };


/// Adds an asset to the same asset as root asset is currently in
/// This can be used to add new objects to serialzed asset files (shader graph, animation)
void AddAssetToSameFile (EditorExtension& asset, EditorExtension& rootAsset, bool randomFileId = false);

/// Adds to the asset pathName
/// This can be used to add new objects to serialzed asset files (shader graph, animation)
void MakeAssetPersistent (EditorExtension& asset, const string& pathName, bool randomFileId = false);

Object* FindFirstAssetWithClassID(const string& pathName, int classID);

std::vector<Object*> FindAllAssetsAtPath(const string& pathName, int classID = ClassID(Object));

/// Returns the asset importer for any object that is stored in the same asset.
/// Returns NULL if none could be found.
AssetImporter* FindAssetImporterForObject (int instanceID);

/// Finds the asset metadata at metaDataPath. (Library/MetaData/...)
AssetMetaData* FindAssetMetaDataAtPath (const string& serializePathName);
/// Finds the asset metadata from GUID
AssetMetaData* FindAssetMetaData (const UnityGUID& guid);
AssetMetaData* FindAssetMetaDataAndMemoryID (const UnityGUID& guid, int *memoryID);
/// Finds the asset importer at metaDataPath. (Library/MetaData/...)
AssetImporter* FindAssetImporterAtPath (const string& metaDataPath);
int FindAssetImporterInstanceID (const string& serializePathName);
/// Finds the asset importer at asset path.
AssetImporter* FindAssetImporterAtAssetPath (const string& assetPath);

bool IsAssetImporterDirty (const UnityGUID& guid);

void UnloadAllLoadedAssetsAtPath (const std::string& path, Object* ignoreObject = NULL);
void UnloadAllLoadedAssetsAtPathAndDeleteMetaData (const std::string& path, Object* ignoreObject = NULL);

#endif
