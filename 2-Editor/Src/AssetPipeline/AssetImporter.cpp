#include "UnityPrefix.h"
#include "AssetImporter.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/EditorExtensionImpl.h"
#include "Runtime/Graphics/Image.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Editor/Src/EditorHelper.h"
#include "ObjectHashGenerator.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "AssetMetaData.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Math/Random/Random.h"

#include <time.h>

///@TODO: * Test: for changing fileIDTOName mapping back and forth by modifying .meta file
//        * test: for animationclip naming back and forth and check that animationclip pptr is intact after the rename¡­
//        * Test: upgrade path for lightmapped projects that do not have any .meta files
//        * Test: upgrade path for asssets with .meta and m_YamlMapping thingy
//        * Don't store PPtr in asset database instead do the lookup completely manual. (Reduces Remapper size because none is necessary during import)
//        * Make sure that all imported prefabs cause remerging of prefabs in the scene.

using namespace std;

static Object* GetFirstDerivedObjectAtCompletePath (const string& pathName, int classID);
static Object* GetFirstDerivedObjectInAssetsAndLibrary (const string& pathName, int classID);

struct CanLoadPathNameHolder
{
	int classID;
	int importerVersion;
	int order;
	AssetImporter::CanLoadPathNameCallback* callback;
	
	friend bool operator < (const CanLoadPathNameHolder& lhs, const CanLoadPathNameHolder& rhs)
	{
		return lhs.order < rhs.order;
	}
};

typedef std::multiset<CanLoadPathNameHolder> CanLoadContainer;
static CanLoadContainer gCanLoadContainer;
static AssetImporter::UpdateProgressCallback* gProgressCallback = NULL;

typedef map<SInt32, Image> CachedThumbnails;
static CachedThumbnails gCachedThumbnails;

IMPLEMENT_CLASS (AssetImporter)
IMPLEMENT_OBJECT_SERIALIZE (AssetImporter)
INSTANTIATE_TEMPLATE_TRANSFER (AssetImporter)

AssetImporter::AssetImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

void AssetImporter::Reset ()
{
	Super::Reset();
	m_FileIDToRecycleName.clear();
	m_RecycleNameToFileID.clear();
}

AssetImporter::~AssetImporter ()
{}

void AssetImporter::Init (int options)
{
	m_ImportFlags = options;
	
	m_WriteDataArray.clear();
}

string AssetImporter::GetAssetPathName () const
{
	AssertIf(!IsPersistent());
	GUIDPersistentManager& pm = GetGUIDPersistentManager();
	return pm.AssetPathNameFromAnySerializedPath (pm.GetPathName(GetInstanceID()));
}

string AssetImporter::GetMetaDataPath() const
{
	AssertIf(!IsPersistent());
	GUIDPersistentManager& pm = GetGUIDPersistentManager();
	UnityGUID guid = pm.GUIDFromAnySerializedPath (pm.GetPathName(GetInstanceID()));
	// Since the importer is stored in the metadata wouldn't it be enough to return the result from pm.GetPathName?
	return GetMetaDataPathFromGUID(guid);
}

string AssetImporter::GetTextMetaDataPath() const
{	
	return GetTextMetaDataPathFromAssetPath(GetAssetPathName());
}

string AssetImporter::GetAbsoluteAssetPathName () const
{
	return PathToAbsolutePath(GetAssetPathName());
}

Object* AssetImporter::GenerateAssetObjectHashBasedImpl (int classID, const string& name, bool recycle)
{
	LocalIdentifierInFileType fileID = GenerateFileIDHashBased (classID, name);
	if (fileID == 0)
		return NULL;
	else
		return &GenerateAssetObjectImpl (classID, name, fileID, recycle);
}

Object& AssetImporter::GenerateAssetObjectImpl (int classID, const string& name, bool recycle)
{
	LocalIdentifierInFileType localIdentifier = RecycleFileID (classID, name);
	return GenerateAssetObjectImpl (classID, name, localIdentifier, recycle);
}

Object& AssetImporter::GenerateAssetObjectImpl (int classID, const string& name, LocalIdentifierInFileType localIdentifier, bool recycle)
{
	Assert(localIdentifier != 0);
	
	Object* producedObject = NULL;

	// When recycling an existing object, we load it and reuse it
	if (recycle)
	{
		string metaDataPath = GetMetaDataPath();
		int instanceID = GetPersistentManager ().GetInstanceIDFromPathAndFileID (metaDataPath, localIdentifier);
		
		producedObject = PPtr<Object> (instanceID);

		// Ensure that the classID matches exactly!
		if (producedObject != NULL && producedObject->GetClassID() != classID)
		{
			ErrorString("Generated Asset has a non-matching classID");
			DestroySingleObject(producedObject);
			producedObject = NULL;
		}
			
		// Create object
		if (producedObject == NULL)
		{
			producedObject = Object::Produce (classID, instanceID);
			producedObject->Reset ();
			
			GetPersistentManager ().MakeObjectPersistentAtFileID (producedObject->GetInstanceID (), localIdentifier, metaDataPath);
		}
	}
	// When producing, we don't create any kind of persistent manager connection.
	else
	{
		producedObject = Object::Produce (classID);
		producedObject->Reset ();
	}
	
	// set name
	producedObject->SetName (name.c_str());
	producedObject->SetHideFlagsObjectOnly(Object::kNotEditable);
	
	RegisterObject(*producedObject, localIdentifier);	
	
	return *producedObject;
}

void AssetImporter::RegisterObject (Object& object, LocalIdentifierInFileType fileID)
{
	// Register fileID
	AssertIf (m_UsedFileIDs.count (fileID) == 1);
	m_UsedFileIDs.insert (fileID);
	m_WriteDataArray.push_back(WriteData(fileID, object.GetInstanceID ()));
}

PPtr<Object> AssetImporter::ExtractFinalPPtrForObject (Object& object)
{
	for (int i=0;i<m_WriteDataArray.size();i++)
	{
		if (object.GetInstanceID() == m_WriteDataArray[i].instanceID)
		{
			string metaDataPath = GetMetaDataPath();
			int instanceID = GetPersistentManager ().GetInstanceIDFromPathAndFileID (metaDataPath, m_WriteDataArray[i].localIdentifierInFile);
			
			return PPtr<Object> (instanceID);
		}
	}
	
	return PPtr<Object>();
}

void AssetImporter::RegisterObject (Object& object, const std::string& name)
{
	Assert(!name.empty());
	
	LocalIdentifierInFileType localIdentifier = RecycleFileID (object.GetClassID(), name);
	RegisterObject(object, localIdentifier);
}

LocalIdentifierInFileType AssetImporter::GenerateFileID (int classID, const std::string& name)
{
	// Generate a fileID by searching the range of possible fileIDs
	// and checking if they are already in use or reserved by named fileIDs
	LocalIdentifierInFileType firstPossibleFileID = kMaxObjectsPerClassID * classID;
	LocalIdentifierInFileType lastPossibleFileID = kMaxObjectsPerClassID * classID + kMaxObjectsPerClassID - 1;
	
	for (LocalIdentifierInFileType i=firstPossibleFileID;i < lastPossibleFileID;i+=2)
	{
		if (m_UsedFileIDs.count (i) == 0 &&
		    m_FileIDToRecycleName.find (i) == m_FileIDToRecycleName.end ())
			return i;
	}
	
	ErrorString ("We have grown over the fileID boundarys");
	return 0;
}

LocalIdentifierInFileType AssetImporter::GenerateFileIDHashBased (int classID, const std::string& name)
{
	MdFourGenerator mdfourGen;
	mdfourGen.Feed(classID);
	mdfourGen.Feed(name);
	MdFour mdfour = mdfourGen.Finish();
	LocalIdentifierInFileType fileID = *reinterpret_cast<UInt32*> (&mdfour);
	Assert(m_FileIDToRecycleName.empty());
	
	if (m_UsedFileIDs.count (fileID) == 1 || fileID == kAssetImporterFileID || fileID == kAssetMetaDataFileID)
		return 0;
	else 
		return fileID;
}

Object& AssetImporter::ProduceAssetDeprecated (int classID, int recycleClassID)
{
	LocalIdentifierInFileType localID = RecycleFileID (recycleClassID, "");
	return GenerateAssetObjectImpl (classID, "", localID, false);
}

Object& AssetImporter::ProducePreviewAsset (int classID, int previewAssetClassID)
{
	LocalIdentifierInFileType localID = RecycleFileID (previewAssetClassID, "");
	return GenerateAssetObjectImpl (classID, "", localID, false);
}

LocalIdentifierInFileType AssetImporter::RecycleFileID (int classID, const string& name)
{
	LocalIdentifierInFileType fileID = 0;
	// Find asset fileID by searching through recycle names or 
	// generating a new one and inserting into the recyclenames
	if (!name.empty ())
	{
		LocalIdentifierInFileType firstPossibleFileID = kMaxObjectsPerClassID * classID;
		LocalIdentifierInFileType lastPossibleFileID = kMaxObjectsPerClassID * classID + kMaxObjectsPerClassID - 1;
		
		pair<multimap<UnityStr, LocalIdentifierInFileType>::iterator, multimap<UnityStr, LocalIdentifierInFileType>::iterator> range;
		range = m_RecycleNameToFileID.equal_range (name);
		
		for (multimap<UnityStr, LocalIdentifierInFileType>::iterator i = range.first;i != range.second;i++)
		{
			LocalIdentifierInFileType curFileID = i->second;
			// Only look for objects with the same classID
			if (curFileID < firstPossibleFileID || curFileID > lastPossibleFileID)
				continue;
			
			// Only recycle if it isn't in use yet
			if (m_UsedFileIDs.count (curFileID) == 1)
				continue;
			
			fileID = curFileID;
			break;
		}
		
		// If the name couldnt be found, generate fileID and insert into recyclenames
		if (fileID == 0)
		{
			fileID = GenerateFileID (classID, name);
			m_RecycleNameToFileID.insert (make_pair (name, fileID));
			m_FileIDToRecycleName.insert (make_pair (fileID, name));
			
			if (NeedToRetainFileIDToRecycleNameMapping())
				SetDirty();
		}
	}
	// Without recycle name just generate a fileID
	else
		fileID = GenerateFileID (classID, name);
	
	return fileID;
}



void AssetImporter::StartImport () 
{
	m_UsedFileIDs.clear ();
	m_ImportErrors.clear ();
	
	// Build m_RecycleNameToFileID
	m_RecycleNameToFileID.clear ();
	for (map<LocalIdentifierInFileType, UnityStr>::iterator i=m_FileIDToRecycleName.begin ();i != m_FileIDToRecycleName.end ();i++)
		m_RecycleNameToFileID.insert (make_pair (i->second, i->first));
	
	// clear thumbnail cache
	gCachedThumbnails.clear ();
	
	InitPostprocessors(GetAssetPathName());
}

void AssetImporter::MarkAllAssetsAsUsed ()
{
	// Force load all objects
	set<SInt32> memoryIDs;
	GetPersistentManager ().GetInstanceIDsAtPath (GetMetaDataPath(), &memoryIDs);
	for (set<SInt32>::iterator i=memoryIDs.begin ();i != memoryIDs.end ();i++)
	{	
		Object* obj = dynamic_instanceID_cast<Object*> (*i);
		if (obj == NULL)
			continue;

		LocalIdentifierInFileType localID = GetPersistentManager().GetLocalFileID(obj->GetInstanceID());
		if (localID == kAssetMetaDataFileID || localID == kAssetImporterFileID)
			continue;

		RegisterObject (*obj, localID);
	}
}

void AssetImporter::EndImport (dynamic_array<Object*>& importedObjects)
{
	AssertIf (GetMetaDataPath().empty ());
	
	m_UsedFileIDs.clear ();
	m_RecycleNameToFileID.clear ();
	
	CleanupPostprocessors();

	importedObjects.reserve(m_WriteDataArray.size());
	// Delete all assets that arent registered as being in use
	for (int i=0;i<m_WriteDataArray.size();i++)
	{
		const WriteData& writeData = m_WriteDataArray[i];
		
		// Asset postprocessors might delete objects
		Object* object = Object::IDToPointer (writeData.instanceID);
		if (object == NULL)
			continue;
		
		importedObjects.push_back (object);
	}
}

void AssetImporter::AddImportError (const std::string& error, const char* file, int line, int mode, const EditorExtension* object)
{
	if (error.empty ())
		return;
	m_ImportErrors.push_back (ImportError ());
	m_ImportErrors.back ().error = error;
	m_ImportErrors.back ().object = object;
	m_ImportErrors.back ().line = line;
	m_ImportErrors.back ().file = file;
	m_ImportErrors.back ().mode = mode;
}

void AssetImporter::SetThumbnail (const Image& image, int instanceID)
{
	Assert(image.GetFormat() != 0);
	gCachedThumbnails[instanceID] = image;
}

Image AssetImporter::GetThumbnailForInstanceID (int instanceID)
{
	CachedThumbnails::iterator i = gCachedThumbnails.find (instanceID);
	// Just returns the user specified thumbnail
	if (i != gCachedThumbnails.end ())
	{
		Assert(i->second.GetFormat() != 0);
		return i->second;
	}
	return Image ();
}

template<class TransferFunction>
void AssetImporter::Transfer (TransferFunction& transfer) 
{
	if (!transfer.AssetMetaDataOnly())
		Super::Transfer (transfer);
	
	if (NeedToRetainFileIDToRecycleNameMapping())
		TRANSFER (m_FileIDToRecycleName);
}

static Object* GetFirstDerivedObjectAtCompletePath (const string& pathName, int classID)
{
	set<SInt32> objects;
	GetPersistentManager().GetInstanceIDsAtPath (pathName, &objects);

	for (set<SInt32>::iterator i=objects.begin ();i != objects.end ();i++)
	{
		Object& object = *PPtr<Object> (*i);
		if (object.IsDerivedFrom (classID))
			return &object;
	}
	return NULL;
}

static Object* GetFirstDerivedObjectInAssetsAndLibrary (const string& pathName, int classID)
{
	if (pathName.empty ())
		return NULL;
		
	Object* object = NULL;

	GetGUIDPersistentManager ().CreateAsset (pathName);
	// Look in assets folder inside the same folder as this asset
	object = GetFirstDerivedObjectAtCompletePath (pathName, classID);
	if (object) return object;

	// Look in library folder inside the same folder as this asset
	object = GetFirstDerivedObjectAtCompletePath (GetMetaDataPathFromAssetPath (pathName), classID);
	if (object) return object;
	
	return NULL;
}

Object* AssetImporter::GetFirstDerivedObjectNamedImpl (const string& name, int classID)
{
	Object* object = NULL;
	string curPath;

	/// Look it up in the same folder
	curPath = AppendPathName (DeleteLastPathNameComponent (GetAssetPathName ()), name);
	object = GetFirstDerivedObjectInAssetsAndLibrary (curPath, classID);
	if (object) return object;
	
	return NULL;
}

Object* AssetImporter::GetFirstDerivedObjectAtPathImpl (const string& curPath, int classID)
{
	return GetFirstDerivedObjectInAssetsAndLibrary (curPath, classID);
}

void AssetImporter::GetAllAssetImporterClassIDAndVersion (map<int, UInt32>& assetImporterClassToVersion)
{
	for (CanLoadContainer::iterator i=gCanLoadContainer.begin();i != gCanLoadContainer.end();i++)
		assetImporterClassToVersion.insert(make_pair(i->classID, i->importerVersion));
}

void AssetImporter::RegisterCanLoadPathNameCallback (CanLoadPathNameCallback* callback, int classID, UInt32 assetImporterVersion, int order)
{
	Assert(assetImporterVersion < 0x7FFFFFFF);

	CanLoadPathNameHolder holder;
	holder.classID = classID;
	holder.callback = callback;
	holder.order = order;
	holder.importerVersion = assetImporterVersion;
	
	gCanLoadContainer.insert (holder);
}

AssetImporterSelection AssetImporter::FindAssetImporterSelection (const string& completePathName)
{
	for (CanLoadContainer::iterator f = gCanLoadContainer.begin ();f != gCanLoadContainer.end ();f++)
	{
		int queue = 0;
		int result = f->callback (completePathName, &queue);
		if (result != 0)
		{
			AssetImporterSelection output;
			output.queueIndex = queue;
			output.generatedAssetType = result;
			output.importerClassID = f->classID;
			output.importerVersion = f->importerVersion;
			return output;
		}
	}
	
	AssertString("Default importer should always catch an asset");
	return AssetImporterSelection();
}

void AssetImporter::SetUpdateProgressCallback (UpdateProgressCallback* progressCallback)
{
	gProgressCallback = progressCallback;
}

bool AssetImporter::ValidateAllowUploadToCacheServer()
{
	// Check if there are any errors (ignore warnings).
	// If there are errors we don't cache the result.
	for (ImportErrors::iterator i=m_ImportErrors.begin(); i != m_ImportErrors.end();i++)
	{
		if (i->mode & kAssetImportError)
			return false;
	}
	
	return true;
}

void AssetImporter::UnloadObjectsAfterImport (UnityGUID guid)
{
	string metaDataPath = GetMetaDataPath();

	// Unload all the objects created by the asset importer
	WriteDataArray& writeData = GetWriteDataArray ();
	for (int i=0;i<writeData.size();i++)
	{
		Object* object = Object::IDToPointer(writeData[i].instanceID);
		if (object != NULL && object != this)
			UnloadObject(object);
	}
	
	// Unload any assets that were loaded before we started importing
	UnloadAllLoadedAssetsAtPath(metaDataPath);
}

void UnloadAllLoadedAssetsAtPath (const std::string& path, Object* ignoreObject)
{
	set<SInt32> loadedObjects;
	GetPersistentManager ().GetLoadedInstanceIDsAtPath(path, &loadedObjects);
	
	for (set<SInt32>::iterator i = loadedObjects.begin ();i != loadedObjects.end ();i++) 
	{
		Object* o = Object::IDToPointer(*i);
		if ( o != NULL && o != ignoreObject) 
			UnloadObject(o);
	}
	GetPersistentManager().UnloadStream(path);
}

void UnloadAllLoadedAssetsAtPathAndDeleteMetaData (const std::string& path, Object* ignoreObject)
{
	UnloadAllLoadedAssetsAtPath(path, ignoreObject);

	// case 490616, Don't delete the metadata before we have turned on .meta files always
	//DeleteFile(path);
}

AssetImporter::UpdateProgressCallback* AssetImporter::GetUpdateProgressCallback ()
{
	return gProgressCallback;
}

static LocalIdentifierInFileType FindNextUnusedFileID (const string& pathName, LocalIdentifierInFileType fileID, bool randomFileId)
{
	set<SInt32> objects;
	GetPersistentManager ().GetInstanceIDsAtPath (pathName, &objects);
	set<LocalIdentifierInFileType> fileIDs;

	for (set<SInt32>::iterator i = objects.begin ();i != objects.end ();i++)
		fileIDs.insert (GetPersistentManager ().GetLocalFileID (*i));
	
	LocalIdentifierInFileType startFileID = fileID;
	Rand rand (time(NULL));
	while (true)
	{
		if (fileIDs.count (fileID) == 0)
			return fileID;
		
		if(randomFileId)
			fileID = startFileID + RangedRandom(rand, 0, kMaxObjectsPerClassID - 2);
		else
			fileID += 2;
	}
	return 0;
}

void MakeAssetPersistent (EditorExtension& o, const string& pathName, bool randomFileId)
{
	if (o.IsPersistent())
	{
		ErrorString("You may not change the path if an object is already peristent in another one");
		return;
	}

	int fileID = FindNextUnusedFileID (pathName, o.GetClassID () * kMaxObjectsPerClassID, randomFileId);
	if (fileID > (o.GetClassID () + 1) * kMaxObjectsPerClassID - 2)
	{
		ErrorString ("Too many objects in file");
		return;
	}
	GetPersistentManager ().MakeObjectPersistentAtFileID (o.GetInstanceID (), fileID, pathName);
}

void AddAssetToSameFile (EditorExtension& asset, EditorExtension& rootAsset, bool randomFileId)
{
	if (rootAsset.IsPersistent())
	{
		string path = GetPersistentManager().GetPathName(rootAsset.GetInstanceID());
		
		MakeAssetPersistent(asset, path, randomFileId);
	}
	else
	{
		ErrorString(Format("AddAssetToSameFile failed because the other asset %s is not persistent", rootAsset.GetName()));
	}
}

Object* FindFirstAssetWithClassID(const string& pathName, int classID)
{
	if (pathName.empty())
		return NULL;
	int instanceID = GetPersistentManager().GetInstanceIDFromPathAndFileID(pathName, classID * kMaxObjectsPerClassID);
	return dynamic_instanceID_cast<Object*> (instanceID);
}

vector<Object*> FindAllAssetsAtPath(const string& pathName, int classID)
{
	vector<Object*> output;
	if (pathName.empty())
		return vector<Object*> ();
	
	set<SInt32> objects;
	
	GetPersistentManager ().GetInstanceIDsAtPath(pathName, &objects);
	GetPersistentManager ().GetInstanceIDsAtPath(GetMetaDataPathFromAssetPath(pathName), &objects);
		
	int ignoreClassIDs[] = {Object::StringToClassID ("Prefab"), Object::StringToClassID ("DefaultAsset"), ClassID(AssetImporter), ClassID(AssetMetaData) };

	// Loop over all objects in asset !! 
	for(set<SInt32>::iterator i=objects.begin(); i != objects.end(); i++) 
	{
		Object* object = PPtr<Object> (*i);
		
		if (object == NULL)
			continue;
		bool ignore = false;
		for (int i=0;i<ARRAY_SIZE(ignoreClassIDs);i++)
		{
			Assert(ignoreClassIDs[i] != classID);
			if (object->IsDerivedFrom(ignoreClassIDs[i]))
				ignore = true;
		}

		if (!ignore && object->IsDerivedFrom(classID))
		{
			output.push_back(object);
		}
	}
	
	return output;
}

bool smaller_name_classid::operator () (Object* lhs, Object* rhs) const
{
	if (lhs->GetClassID() != rhs->GetClassID())
		return lhs->GetClassID() < rhs->GetClassID();
	else
	{
		// In order to make the sorting stable but still have alphabetically correct sorting
		// the sorting is done in two steps and the sorting becomes: Arnold, arnold, Victor, victor, Xenia, xenia
		// if we only had case sensitive sorting we would get: Arnold, Victor, Xenia, arnold, victor, xenia
		int result = StrICmp(lhs->GetName(), rhs->GetName());
		if (result == 0)
		{
			result = StrCmp(lhs->GetName(), rhs->GetName());
		}
		return result < 0;
	}
}

/// Called after a meta data file has been seen to modify the import settings.
/// The settings argument points to a YAMLMapping containing a key/value pairs produced by GenerateImportSettings()
void AssetImporter::ApplyImportSettings( const TEMP_STRING& meta ) 
{
	YAMLRead read (meta.c_str(), meta.size(), kAssetMetaDataOnly | kYamlGlobalPPtrReference | kNeedsInstanceIDRemapping);
	
	YAMLMapping * metaMapping = dynamic_cast<YAMLMapping*>(read.GetCurrentNode());
	if (metaMapping != NULL)
	{
		YAMLMapping * settings = dynamic_cast<YAMLMapping*>(metaMapping->Get("AssetImporter"));
		if(settings) 
		{
			YAMLScalar* importerVersion = dynamic_cast<YAMLScalar*>( settings->Get("importerVersion") );
			if( importerVersion && int(*importerVersion) != kAllImporterVersion  ) 
			{
				printf_console("Unknown importer version %d. Assuming current version\n", int(*importerVersion) );
			}
			YAMLMapping* fileIDToRecycleName = dynamic_cast<YAMLMapping*>(settings->Get("fileIDToRecycleName"));
			if( fileIDToRecycleName ) 
			{
				m_FileIDToRecycleName.clear();
				for( YAMLMapping::const_iterator i = fileIDToRecycleName->begin() ; i != fileIDToRecycleName->end(); i++) 
				{
					YAMLScalar * value = dynamic_cast<YAMLScalar*> (i->second);
					if( value ) 
					{
						m_FileIDToRecycleName.insert( make_pair(int(* (i->first)), string(*value) ) );
					}
				}
			}
		}
		delete metaMapping;
	}

	this->VirtualRedirectTransfer(read);
}

bool AssetImporter::NeedToRetainFileIDToRecycleNameMapping()
{
	return false;
}

/// Should populate the YAMLMapping object passed in with keys of all import settings that are different from the default settings.
/// Should also add an importerVersion key for ensuring future backwards compatibility.
void AssetImporter::GenerateImportSettings( std::string& meta ) 
{
	YAMLWrite write (kAssetMetaDataOnly | kYamlGlobalPPtrReference | kNeedsInstanceIDRemapping);
	
	this->VirtualRedirectTransfer(write);

	write.OutputToString (meta);
}

void AssetImporter::ReloadAssetsThatWereModifiedOnDisk ()
{
	if ( GetImportFlags() & kAssetWasModifiedOnDisk ) 
	{
		set<SInt32> loadedObjects;
		GetPersistentManager ().GetLoadedInstanceIDsAtPath(GetAssetPathName(), &loadedObjects);
		GetPersistentManager ().UnloadStream(GetAssetPathName());
		
		for (set<SInt32>::iterator i = loadedObjects.begin ();i != loadedObjects.end ();i++) 
		{
			Object* o = IDToPointer(*i);
			if ( o != NULL ) 
				GetPersistentManager().ReloadFromDisk(o);
		}
	}
}

AssetMetaData* FindAssetMetaDataAtPath (const string& serializePathName)
{
	int globalAssetID = GetPersistentManager ().GetInstanceIDFromPathAndFileID (serializePathName, kAssetMetaDataFileID);
	Object* importerCandidate = PPtr<Object> (globalAssetID);
	return dynamic_pptr_cast<AssetMetaData*> (importerCandidate);
}

inline int FindAssetMetaDataInstanceID (const UnityGUID& guid)
{
	return GetPersistentManager ().GetInstanceIDFromPathAndFileID (GetMetaDataPathFromGUID(guid), kAssetMetaDataFileID);
}

/// Finds the asset metadata from GUID
AssetMetaData* FindAssetMetaData (const UnityGUID& guid) 
{
	return FindAssetMetaDataAtPath(GetMetaDataPathFromGUID(guid));
}

AssetMetaData* FindAssetMetaDataAndMemoryID (const UnityGUID& guid, int *memoryID)
{
	*memoryID = GetPersistentManager ().GetInstanceIDFromPathAndFileID (GetMetaDataPathFromGUID(guid), kAssetMetaDataFileID);
	Object* importerCandidate = PPtr<Object> (*memoryID);
	return dynamic_pptr_cast<AssetMetaData*> (importerCandidate);
}


AssetImporter* FindAssetImporterForObject (int instanceID)
{
	string path = GetPersistentManager ().GetPathName (instanceID);
	if (path.empty())
		return NULL;
	
	path = GetGUIDPersistentManager ().AssetPathNameFromAnySerializedPath (path);
	path = GetMetaDataPathFromAssetPath (path);
	return FindAssetImporterAtPath (path);
}

int FindAssetImporterInstanceID (const string& serializePathName)
{
	if (!serializePathName.empty())
		return GetPersistentManager ().GetInstanceIDFromPathAndFileID (serializePathName, kAssetImporterFileID);
	else
		return 0;
}

AssetImporter* FindAssetImporterAtPath (const string& serializePathName)
{
	return dynamic_instanceID_cast<AssetImporter*> (FindAssetImporterInstanceID(serializePathName));
}

AssetImporter* FindAssetImporterAtAssetPath (const string& assetPath)
{
	return FindAssetImporterAtPath(GetMetaDataPathFromAssetPath(assetPath));
}

bool IsAssetImporterDirty (const UnityGUID& guid)
{
	int instanceID = FindAssetImporterInstanceID(GetMetaDataPathFromGUID(guid));
	Object* obj = Object::IDToPointer (instanceID);
	return obj != NULL && obj->IsPersistentDirty();
}
