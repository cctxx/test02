#include "UnityPrefix.h"
#include "AssetDatabase.h"
#include "AssetModificationCallbacks.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "AssetImporter.h"
#include "AssetMetaData.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Shaders/Shader.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/Utility/ObjectNames.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Editor/Src/Utility/AssetPreviews.h"
#include "Editor/Src/Utility/AssetPreviewPostFixFile.h"
#include "MdFourGenerator.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Runtime/Serialize/SerializedFile.h"
#include <iostream>
#include "NativeFormatImporter.h"
#include "Editor/Src/AssetServer/ASCache.h"
#include "Editor/Src/Utility/YAMLNode.h"
#include "Editor/Src/EditorSettings.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "AssetLabelsDatabase.h"
#include "Editor/Src/AssetPipeline/DefaultImporter.h"
#include "Editor/Src/AssetPipeline/CacheServer/CacheServer.h"
#include "Editor/Src/AssetPipeline/ModelImporter.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"
#include "Editor/Src/AssetPipeline/AudioImporter.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/EditorExtensionImpl.h"
#include "Editor/Src/Utility/AssetPreviewGeneration.h"
#include "Editor/Src/Utility/AssetPreviewPostFixFile.h"
#include "AssetHashing.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Runtime/BaseClasses/ClassIDs.h"

using namespace std;

const char* kFailedAssetsHolder = "Library/FailedAssetImports.txt";
const UnityGUID kAssetFolderGUID = UnityGUID (0,0,1,0);
AssetDatabase* gAssetDatabase = NULL;


IMPLEMENT_CLASS_HAS_INIT (AssetDatabase)
IMPLEMENT_OBJECT_SERIALIZE (AssetDatabase)

void AssetDatabase::InitializeClass ()
{
	RegisterAllowNameConversion("LibraryRepresentation", "hasFullPreviewImage", "flags");
}

AssetDatabase::PostprocessContainer	AssetDatabase::ms_Postprocess = PostprocessContainer ();
AssetDatabase::CalculateMostImportantCallback*	AssetDatabase::ms_MostImportantCallback = NULL;

static void WriteSerializedAssetRecursive (const UnityGUID& guid, AssetDatabase& database, const set<int>* dirtyPaths, std::set<UnityGUID>* savedAssets);
static bool IsVisibleInLibraryRepresention (PPtr<Object> object, const Asset& asset);


// This is only used when serializing the list of assets and time stamps
struct AssetCombinedDeprecated
{
	AssetCombinedDeprecated() ;
	Asset m_Asset;
	AssetTimeStamp m_TimeStamp;
	
	DECLARE_SERIALIZE (Asset)
};



AssetCombinedDeprecated::AssetCombinedDeprecated() :
m_Asset(),
m_TimeStamp() 
{}



// Try to catch how ".." folders get created
static void CheckAssetNameValid (const std::string& name)
{
	AssertIf(name.size()==2 && name[0]=='.' && name[1]=='.');
	AssertIf(name.size()==1 && name[0]=='.');
	AssertIf(name.size()==1 && name[0]=='/');
}


static const char* GetCustomImporterDisplayName (int classID)
{
	// Native importer and default asset importer are very fast to import. Showing the complete path in the progressbar is a performance bottleneck.
	if (classID == ClassID(DefaultImporter) || classID == ClassID(NativeFormatImporter))
		return "Importing small assets.";
	if (classID == ClassID(MonoImporter))
		return "Importing scripts.";
	else
		return NULL;
}

template<class TransferFunction>
void AssetDatabase::ReadOldAssetDatabase (TransferFunction& transfer)
{
	map<UnityGUID, AssetCombinedDeprecated> combinedAssets;

	transfer.Transfer (combinedAssets, "m_Assets");
	
	if ( transfer.IsReading() )
	{
		m_Assets.clear();
		m_AssetTimeStamps.clear();
		for( map<UnityGUID,AssetCombinedDeprecated>::iterator i = combinedAssets.begin() ; i != combinedAssets.end() ; i++ )
		{
			string path = GetAssetPathFromGUID(i->first);
			m_Assets.insert(make_pair(i->first, i->second.m_Asset));
			if (!path.empty())
				m_AssetTimeStamps.insert(make_pair(path, i->second.m_TimeStamp));
		}
	}
	
	if (transfer.IsOldVersion(1))
		SortAssetsBackwardsCompatibility();
	
	AssertIf( m_Assets.size() < m_AssetTimeStamps.size() );
}

void AssetDatabase::ValidateTimeStampInconsistency ()
{
	if (m_AssetTimeStamps.size() == m_Assets.size())
		return;
	
	WarningString(Format("Timestamps (%d) and assets (%d) maps out of sync.", (int)m_AssetTimeStamps.size(), (int)m_Assets.size()));
	
	vector<UnityGUID> invalidAssets;
	
	for (Assets::iterator i = m_Assets.begin();i != m_Assets.end();++i)
	{
		string path = GetAssetPathFromGUID(i->first);
		
		if (m_AssetTimeStamps.count(path) == 0)
		{
			invalidAssets.push_back(i->first);
			AssertString(Format("Asset '%s' is in assets but has no assettimestamp...", path.c_str()));
		}
		else if (path.empty())
		{
			invalidAssets.push_back(i->first);
			AssertString("Asset with empty path but has assettimestamp???");
		}
	}

	for (AssetTimeStamps::iterator i = m_AssetTimeStamps.begin();i != m_AssetTimeStamps.end();++i)
	{
		UnityGUID guid;
		string path = i->first;
		if (!GetGUIDPersistentManager().PathNameToGUID(path, &guid))
		{
			AssertString(Format("Asset '%s' is in timestamps but is not known in guidmapper...", path.c_str()));
		}
		else if (m_Assets.count(guid) == 0)
		{
			AssertString(Format("Asset '%s' is in timestamps but is not known in assetdatabase...", path.c_str()));
		}
		else if (guid == UnityGUID())
		{
			AssertString("Asset with empty guid");
		}
		else if (path.empty())
		{
			AssertString("Asset with empty path");
		}
		
		printf_console("timestamp: %s\n", path.c_str());
		
	}
	
	// Cleanup any assets that are not known about in the timestamps.
	// Otherwise they will never be cleaned up.
	for (int i=0;i<invalidAssets.size();i++)
	{
		RemoveFromHierarchy(invalidAssets[i]);
	}
}

template<class TransferFunction>
void AssetDatabase::Transfer (TransferFunction& transfer)
{
	AssertIf(transfer.GetFlags() & kPerformUnloadDependencyTracking);

	Super::Transfer (transfer);

	transfer.SetVersion(3);
	
	if (transfer.IsVersionSmallerOrEqual(2))
		ReadOldAssetDatabase (transfer);
	// With Unity 3.5 we introduced writing m_AssetTimeStamps and m_Assets separately since this is a lot faster.
	else
	{
		transfer.Transfer(m_AssetTimeStamps, "m_AssetTimeStamps");
		transfer.Transfer(m_Assets, "m_Assets");
		transfer.Transfer(m_UnityShadersVersion, "m_UnityShadersVersion");
		transfer.Transfer(m_lastValidVersionHashes, "m_lastValidVersionHashes");
	}	
}

bool AssetDatabase::ShouldIgnoreInGarbageDependencyTracking ()
{	
	return true;
}

template<class TransferFunction>
void AssetCombinedDeprecated::Transfer (TransferFunction& transfer)
{
	transfer.SetVersion(1);

	m_TimeStamp.Transfer(transfer);
	m_Asset.Transfer(transfer);
}

inline void RemoveAssetErrors (const UnityGUID& guid)
{
	int instanceID = GetPersistentManager ().GetInstanceIDFromPathAndFileID (GetMetaDataPathFromGUID (guid), kAssetMetaDataFileID);
	RemoveErrorWithIdentifierFromConsole (instanceID);
}

static bool IsVisibleInLibraryRepresention (PPtr<Object> object, const Asset& asset)
{
	if (asset.mainRepresentation.object == object)
		return true;
	for (int i=0;i<asset.representations.size ();i++)
	{
		if (asset.representations[i].object == object)
			return true;
	}
	return false;
}

AssetMetaData* CreateOrLoadAssetMetaDataAtPath (const string& serializePathName)
{
	int globalAssetID = GetPersistentManager ().GetInstanceIDFromPathAndFileID (serializePathName, kAssetMetaDataFileID);
	Object* importerCandidate = PPtr<Object> (globalAssetID);
	if (dynamic_pptr_cast<AssetMetaData*> (importerCandidate) != NULL)
	{
		AssertIf (!importerCandidate->IsPersistent () || GetPersistentManager ().GetPathName (globalAssetID) != serializePathName);
		AssertIf (GetPersistentManager ().GetLocalFileID (globalAssetID) != kAssetMetaDataFileID);
		return static_cast<AssetMetaData*> (importerCandidate);
	}
	else
	{
		DestroySingleObject (importerCandidate);

		AssetMetaData* metaData = static_cast<AssetMetaData*> (Object::Produce (ClassID (AssetMetaData), globalAssetID));
		metaData->Reset();
		metaData->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);

		GetPersistentManager ().MakeObjectPersistentAtFileID (metaData->GetInstanceID (), kAssetMetaDataFileID, serializePathName);		
		return metaData;
	}
}

bool AssetDatabase::IsAutomaticImportEnabled (const std::string& enabled)
{
	return m_FailedAssets.count(enabled) == 0;
}

AssetDatabase::AssetDatabase (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_AssetLabelDB = new AssetLabelsDatabase();
	m_AssetTimeStamps.set_empty_key ("");
	m_AssetTimeStamps.set_deleted_key ("!xxx!");
	m_UnityShadersVersion = 0;
}

AssetDatabase::~AssetDatabase ()
{
	delete m_AssetLabelDB;
}

void AssetDatabase::ForceMarkAsFoundMetaFile (const string& originalPathName, bool isHidden)
{
	AssetTimeStamps::iterator found = m_AssetTimeStamps.find (originalPathName);
	if (found != m_AssetTimeStamps.end ())
		found->second.refreshFlags = 
			found->second.refreshFlags 
			| (isHidden ? AssetTimeStamp::kRefreshFoundHiddenMetaFile : AssetTimeStamp::kRefreshFoundMetaFile)
			& (isHidden ? ~AssetTimeStamp::kRefreshFoundMetaFile : ~AssetTimeStamp::kRefreshFoundHiddenMetaFile);
}

void AssetDatabase::ForceTextMetaFileTimeStampUpdate(const UnityGUID& guid)
{
	const UnityStr& pathName = GetAssetPathFromGUID(guid);
	const UnityStr textMetaDataPath = GetTextMetaDataPathFromAssetPath(pathName);

	AssetTimeStamps::iterator found = m_AssetTimeStamps.find(pathName);
	if (found != m_AssetTimeStamps.end())
	{
		found->second.metaModificationDate = GetContentModificationDate(textMetaDataPath);
	}
}

bool AssetDatabase::DoesAssetFileNeedUpdate (const string& originalPathName, const DateTime& modificationDate, bool isDirectory, 
	bool isMeta, bool isHidden)
{
	AssetTimeStamps::iterator found = m_AssetTimeStamps.find (originalPathName);
	
	if (found != m_AssetTimeStamps.end ())
	{
		if (isMeta)
		{
			found->second.refreshFlags |= isHidden ? AssetTimeStamp::kRefreshFoundHiddenMetaFile : AssetTimeStamp::kRefreshFoundMetaFile;
		}
		else
		{
			found->second.refreshFlags |= AssetTimeStamp::kRefreshFoundAssetFile;
		}

		if (!isDirectory)
		{
			// for files, check modification date
			if ( isMeta ? (modificationDate != found->second.metaModificationDate) :  ( modificationDate != found->second.modificationDate) )
				return true;
		}
		return false;
	}
	else
	{
		// this is a new asset (or renamed)
		return true;
	}
}

AssetImporter* AssetDatabase::ImportAsset (Asset& asset, const string& assetPathName, const string& metaDataPath, int importerClass, int options, const TEMP_STRING &importSettings, bool allowFullImport)
{
	AssetImporter* importer = FindAssetImporterAtPath (metaDataPath);

	// Create new assetimporter
	if (importer == NULL || importerClass != importer->GetClassID ())
	{
		DestroySingleObject (importer);
		importer = assert_cast<AssetImporter*> (Object::Produce (importerClass));
		importer->Reset();
		importer->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
	}

	// The first two fileIDs are reserved for assetimporter
	GetPersistentManager ().MakeObjectPersistentAtFileID (importer->GetInstanceID (), kAssetImporterFileID, metaDataPath);

	importer->Init (options);
	
	if( importSettings.size() > 0 ) 
	{
		importer->Reset();
		importer->ApplyImportSettings(importSettings);
		importer->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
		
		// Prevents rewriting the meta file. Since we just read it, the importer should not be dirty.
		importer->ClearPersistentDirty();
	}
	
	// Startup importing
	importer->StartImport ();
	
	dynamic_array<Object*> importedObjects (kMemTempAlloc);
	if (allowFullImport)
	{
		importer->GenerateAssetData ();

		// Hook for the Importer to do any cleanups neccessary...
		importer->EndImport (importedObjects);
	}
	else
	{
		GenerateDefaultImporterAssets(*importer, true);
		importer->ClearPreviousImporterOutputs ();
		importer->AssetImporter::EndImport (importedObjects);
	}

	Assert (asset.representations.empty ());

	// Update assetdatabase to point to the generated assets
	size_t counter = 0;
	for (int i=0;i<importedObjects.size();i++)
	{
		Object& serializedAsset = *importedObjects[i];
		
		if (serializedAsset.TestHideFlag(Object::kHideInHierarchy) || serializedAsset.GetClassID() == ClassID(EditorExtensionImpl))
			continue;

		counter++;
	}
	
	asset.representations.reserve(counter);
	for (int i=0;i<importedObjects.size();i++)
	{
		Object& serializedAsset = *importedObjects[i];
		
		if (serializedAsset.TestHideFlag(Object::kHideInHierarchy) || serializedAsset.GetClassID() == ClassID(EditorExtensionImpl))
			continue;
		
		LibraryRepresentation representation;
		representation.name = serializedAsset.GetName();
		CheckAssetNameValid(representation.name);
		representation.thumbnail = importer->GetThumbnailForInstanceID (serializedAsset.GetInstanceID ());
		representation.object = &serializedAsset;
		representation.classID = serializedAsset.GetClassID ();
		
		if (MonoScript* monoScript = dynamic_pptr_cast<MonoScript*> (&serializedAsset))
			representation.scriptClassName = monoScript->GetScriptFullClassName();		// For MonoScripts we want class names with namespaces
		Shader* shader = dynamic_pptr_cast<Shader*> (&serializedAsset);
		if (shader)
		{
			representation.scriptClassName = shader->GetScriptClassName();
			if (shader->GetErrors().HasErrors())
				representation.flags |= LibraryRepresentation::kAssetHasErrors;
		}
		
		if (MonoBehaviour* monoBehaviour = dynamic_pptr_cast<MonoBehaviour*> (&serializedAsset))
			representation.scriptClassName = monoBehaviour->GetScriptFullClassName();		// For MonoBehaviors we want class name with namespace

		asset.representations.push_back (representation);
	}
	
	return importer;
}

void AssetDatabase::AddFailedAssetImporter (const std::string& path)
{
	// Ignore scripts for asset importing failures. Usually this is not actually caused by the specific script.
	string extension = GetPathNameExtension(path);
	if (StrICmp(extension, "cs") == 0 || StrICmp(extension, "js") == 0 || StrICmp(extension, "boo") == 0)
		return;
	
	// Only in the assets folder. Library assets must always be importable.
	if (ToLower(path).find("assets/") == 0)
	{
		m_FailedAssets.insert(path);
		WriteFailedAssets();
	}
}

void AssetDatabase::RemoveFailedAssetImporter (const std::string& path)
{
	if (m_FailedAssets.erase(path) == 1)
	{
		WriteFailedAssets();
	}
}

void AssetDatabase::ReadFailedAssets ()
{
	m_FailedAssets.clear();
	InputString all;
	ReadStringFromFile(&all, kFailedAssetsHolder);
	vector<string> paths = FindSeparatedPathComponents(all.c_str(), all.size(), '\n');
	bool needWrite = false;
	for (int i=0;i<paths.size();i++)
	{
		if (IsFileCreated(paths[i]))
		{
			LogString(Format("Automatic import for '%s' was disabled because the asset importer crashed on it last time.\nYou can use 'Assets -> Reimport' to enable automatic importing again.", paths[i].c_str()));
			m_FailedAssets.insert(paths[i]);
		}
		else
		{
			needWrite = true;
		}
	}	
	
	if (needWrite)
		WriteFailedAssets ();
}

void AssetDatabase::WriteFailedAssets ()
{
	string all;
	for (set<UnityStr>::iterator i=m_FailedAssets.begin();i!=m_FailedAssets.end();i++)
	{
		UnityStr tmp = *i;
		all += tmp;
		all += "\n";
	}

	// Using atomic files doesn't work
	// because the file gets overwritten too quickly on windows...
	SetFileFlags(kFailedAssetsHolder, kFileFlagDontIndex, kFileFlagDontIndex);
	WriteBytesToFile(all.c_str(), all.size(), kFailedAssetsHolder);
}

void AssetDatabase::RemoveFromParent (const UnityGUID& selfGUID, const UnityGUID& parent)
{
	Assets::iterator found = m_Assets.find (parent);
	if (found != m_Assets.end ()) 
	{
		vector<UnityGUID>::iterator i = find (found->second.children, selfGUID);
		if (i != found->second.children.end())
			found->second.children.erase (i);
		else
		{
			FatalErrorIf (parent != UnityGUID ());
		}
	}
}

struct SortChildren : std::binary_function<UnityGUID, UnityGUID, bool>
{
	AssetDatabase* m_DB;
	SortChildren (AssetDatabase* db) { m_DB = db; }	
	
	bool operator ()(UnityGUID lhs, UnityGUID rhs) const
	{
		const Asset* lhsAsset = m_DB->AssetPtrFromGUID (lhs);
		const Asset* rhsAsset = m_DB->AssetPtrFromGUID (rhs);
		if (lhsAsset && rhsAsset)
		{
			return SemiNumericCompare(lhsAsset->mainRepresentation.name, rhsAsset->mainRepresentation.name) < 0;
		}
		else
			return lhsAsset < rhsAsset;
	}
};


/// With Unity 2.5 all assets have to be sorted in the asset database.
/// (2.1 had assets stored in random order)
void AssetDatabase::SortAssetsBackwardsCompatibility ()
{
	SortChildren sortFunc (this);
	for (Assets::iterator i=m_Assets.begin();i != m_Assets.end();i++)
	{
		std::sort(i->second.children.begin(), i->second.children.end(), sortFunc);
	}
}
void AssetDatabase::InsertChildSorted (vector<UnityGUID>& children, UnityGUID guid, string name)
{
	for (vector<UnityGUID>::iterator i = children.begin(); i != children.end();i++)
	{
		const Asset* asset = AssetPtrFromGUID (*i);
		if (asset && SemiNumericCompare(asset->mainRepresentation.name, name) > 0)
		{
			children.insert(i, guid);
			return;
		}
	}
	children.push_back(guid);
}

bool AssetDatabase::VerifyAssetDatabase() 
{
	for (Assets::iterator i=m_Assets.begin();i!=m_Assets.end();i++)
	{
		// AssetVersion was introduced in 3.5. So just add it automatically.
		if (i->second.importerClassId == -1)
		{
			string path = GetAssetPathFromGUID(i->first);
			AssetImporterSelection as = AssetImporter::FindAssetImporterSelection(path);
			i->second.importerClassId = as.importerClassID;
		}
		
		// Map from deprecated CopyAssetWithSerializedData to kCopyAsset
		if (i->second.type == 3)
			i->second.type = kCopyAsset;
		
		for (int j=0;j<i->second.children.size();j++)
		{
			const Asset* asset = AssetPtrFromGUID(i->second.children[j]);
			// Child does not actually exist in database
			if (asset == NULL)
			{
				ErrorString("Asset database inconsistent");
				return false;
			}
			
			// Child thinks that it's parent is someone else
			if (asset->parent != i->first)
			{
				ErrorString("Asset database inconsistent");
				return false;
			}
		}
	}

	for (Assets::iterator i=m_Assets.begin();i!=m_Assets.end();i++)
	{
		const Asset* parent = AssetPtrFromGUID(i->second.parent);
		if (parent == NULL)
		{
			// Parent can't be found but parent is not empty GUID
			if (i->second.parent != UnityGUID())
			{
				ErrorString("Asset database inconsistent");
				return false;
			}
			
			//printf_console("%s - %s is a root guid.\n", GetAssetPathFromGUID(i->first).c_str(), GUIDToString(i->first).c_str());
			
			if (!GetGUIDPersistentManager().IsConstantGUID(i->first))
			{
				ErrorString("Asset database inconsistent - root asset must be a const guid");
				return false;
			}
			continue;
		}
		
		// Parent doesn't know about it's child
		if (count(parent->children.begin(), parent->children.end(), i->first) != 1)
		{
			ErrorString("Asset database inconsistent");
			return false;
		}
	}

	return true;
}

bool AssetDatabase::WriteYamlString(const std::string& yaml, const std::string& filename, UInt32 flags) 
{
	InputString old;
	if( ReadStringFromFile(&old, filename))
	{
		// Skip saving if they are the same
		string oldNormalized = old.c_str();
		if (oldNormalized == yaml)
			return true;
		
		// Also skip saving if the only difference is line endings
		// This is a temporary workaround for perforce auto fixing line endings
		// We can remove this when we stop always writing yaml files during import!
		replace_string(oldNormalized, "\r\n", "\n");
		replace_string(oldNormalized, "\r", "\n");
		if (oldNormalized == yaml)
			return true;
	}	
	
	// Invoke asset modification callbacks. They might not allow us to actually write to the meta file!
	if (!old.empty())
	{
		if (!AssetModificationCallbacks::ShouldSaveSingleFile(filename))
			return false;
	}
	else
		AssetModificationCallbacks::WillCreateAssetFile(filename);

	
	if (! WriteStringToFile(yaml, filename, kNotAtomic, flags) )
	{
		ErrorString(Format("Failed to write meta file '%s'", filename.c_str()));
		return false;
	}
	
	return true;
}

bool AssetDatabase::WriteImportSettingsIfDirty(const UnityGUID& selfGUID)
{
	string metaDataPath = GetMetaDataPathFromGUID (selfGUID);

	const Asset* asset = AssetPtrFromGUID(selfGUID);
	if (asset == NULL)
		return false;
	
	AssetImporter* importer = FindAssetImporterAtPath (metaDataPath);
	if (importer == NULL || !importer->IsPersistentDirty())
		return false;
	
	AssetMetaData* metaData = FindAssetMetaDataAtPath (metaDataPath);
	if (metaData == NULL)
		return false;

	// Save text-based import settings to .meta file if required 
	// Need to do this before writing binary meta data, so importer dirty state
	// is not reset.
	AssetTimeStamp temp;
	WriteTextMetaData(selfGUID, *asset, temp, metaData->labels, importer);
	
	WriteAssetMetaDataKeepPreviews (selfGUID);
	
	return true;
}

std::string AssetDatabase::GenerateTextMetaData(const UnityGUID& guid, bool isFolder)
{
	string metaDataPath = GetMetaDataPathFromGUID (guid);
	
	AssetImporter* importer = FindAssetImporterAtPath (metaDataPath);
	if (importer == NULL)
		return "";
	
	AssetMetaData* metaData = FindAssetMetaDataAtPath (metaDataPath);
	if (metaData == NULL)
		return "";

	return GenerateTextMetaDataIMPL(guid, isFolder, metaData->labels, importer);
}

std::string AssetDatabase::GenerateTextMetaDataIMPL(const UnityGUID& guid, bool isFolder, const vector<UnityStr> & labels, AssetImporter* importer)
{
	YAMLWrite write (0);
	int version = 2;
	write.Transfer(version, "fileFormatVersion");
	UnityGUID guidNoConst = guid;
	write.Transfer(guidNoConst, "guid");
	if (labels.size())
	{
		vector<UnityStr> labelsNoConst = labels;
		write.Transfer(labelsNoConst, "labels");
	}

	if (isFolder)
	{
		UnityStr yes = "yes"; 
		write.Transfer(yes, "folderAsset");
	}
	
	string header;
	write.OutputToString (header);
	string meta;
	importer->GenerateImportSettings(meta);
	return header+meta;
}

bool AssetDatabase::WriteTextMetaData(const UnityGUID& guid, const Asset& asset, AssetTimeStamp& timeStamp, const vector<UnityStr> & labels, AssetImporter* importer) 
{
	string assetPath = GetAssetPathFromGUID(guid);
	string textMetaDataPath = GetTextMetaDataPathFromAssetPath(assetPath);
	
	bool isFileCreated = IsFileCreated( textMetaDataPath );

	if( asset.parent.IsValid() )
	{
		bool visibleMetaFile = GetEditorSettings().GetVersionControlRequiresMetaFiles();

		if (!isFileCreated || importer->IsPersistentDirty() )
		{
			// If meta files are disabled, set file flag to kFileFlagHidden.
			UInt32 flags = visibleMetaFile ? 0 : kFileFlagHidden;
			string meta = GenerateTextMetaDataIMPL(guid, asset.type == kFolderAsset, labels, importer);
			
			WriteYamlString(meta, textMetaDataPath, flags);

			timeStamp.metaModificationDate=GetContentModificationDate ( textMetaDataPath );
			return true;
		}

		// If the file is created and meta file is enabled, we need to remove the hidden flag.
		if(isFileCreated && isHiddenFile(textMetaDataPath) && visibleMetaFile)
		{
			SetFileFlags(textMetaDataPath, kFileFlagHidden, 0);

			timeStamp.refreshFlags = 
				timeStamp.refreshFlags | AssetTimeStamp::kRefreshFoundMetaFile & ~AssetTimeStamp::kRefreshFoundHiddenMetaFile;
		}
	}
	else if ( isFileCreated ) 
	{
		DeleteFile( textMetaDataPath );
		timeStamp.metaModificationDate =  DateTime();
	}
	return false;
}

void MapToPersistentInstanceID (int serializedFileIndex, const AssetImporter::WriteDataArray& writeData, LibraryRepresentation& representation)
{
	for (int i=0;i<writeData.size();i++)
	{
		if (representation.object.GetInstanceID() == writeData[i].instanceID)
		{
			SerializedObjectIdentifier identifier (serializedFileIndex, writeData[i].localIdentifierInFile);
			representation.object.SetInstanceID(GetPersistentManager().SerializedObjectIdentifierToInstanceID(identifier));
			return;
		}
	}
}

static void ResolveAssetImporterWrite (SInt32 instanceID, LocalSerializedObjectIdentifier& localIdentifier, void* context)
{
	const InstanceIDToLocalIdentifier& remap = *reinterpret_cast<const InstanceIDToLocalIdentifier*> (context);
	
	InstanceIDToLocalIdentifier::const_iterator found = remap.find(instanceID);
	if (found != remap.end())
	{
		localIdentifier.localSerializedFileIndex = 0;
		localIdentifier.localIdentifierInFile = found->second;
		return;
	}
	
	GetPersistentManager ().InstanceIDToLocalSerializedObjectIdentifierInternal (instanceID, localIdentifier);
	
	// Remapping to object that is on disk
	if (localIdentifier.localSerializedFileIndex == 0 && localIdentifier.localIdentifierInFile != 0)
	{
		ErrorString("AssetImporter is referencing an asset from the previous import. This should not happen.");
		localIdentifier.localSerializedFileIndex = 0;
		localIdentifier.localIdentifierInFile = 0;
		return;
	}
}

static void AddInstanceIDToLocalIdentifier (int instanceID, InstanceIDToLocalIdentifier& remap)
{
	remap.push_unsorted(instanceID, GetPersistentManager().GetLocalFileID(instanceID));
}

bool AssetDatabase::UpdateAsset (const UnityGUID& selfGUID, const UnityGUID& parentGUID, const MdFour& forcedHash, int options,
	MdFour newhash)
{
	Assets::iterator found;
	
	ABSOLUTE_TIME time = START_TIME;
	
	// Get asset path name + meta data path
	string assetPathName = GetGUIDPersistentManager ().AssetPathNameFromGUID (selfGUID);
	string metaDataPath = GetMetaDataPathFromGUID (selfGUID);
	string textMetaDataPath = GetTextMetaDataPathFromAssetPath(assetPathName);
	
	DateTime modificationDate = GetContentModificationDate (assetPathName);
	
	Asset originalAsset;
	Asset asset;
	if (m_Assets.count(selfGUID))
	{
		originalAsset = asset = m_Assets[selfGUID];
	}
	else
	{
		// When importing an asset for the first time we may show import settings.
		// Eg. for a movie it makes sense to show it since it will take ages to actually import
		// You dont want that twice.
		options &= ~kMayCancelImport;

		if (IsSymlink (assetPathName))
		{
			// Originally Unity would ignore symlinks to avoid the risks of users breaking stuff using them,
			// such as recursive symlinks, assets being shared by different versions of Unity or someone accidentally
			// adding the file system root to the project folder, spawning .meta files everywhere.
			// However, the windows version did not implement IsSymlink(), so it would work there (with all the
			// risks), and people started depending on this behaviour, and were very upset when we tried to make it consistent
			// with OS X. So now we allow symlinks everywhere, but give people a frank warning.
			WarningStringMsg ("%s is a symbolic link. Using symlinks in Unity projects may cause your project to become corrupted if you create multiple references to the same asset, use recursive symlinks or use symlinks to share assets between projects used with different versions of Unity. Make sure you know what you are doing.", assetPathName.c_str());
		}
	
		// Warn when using space in file name
		if (!CheckValidFileName(GetLastPathNameComponent(assetPathName)))
		{
			WarningStringMsg ("'%s' is not a recommended asset file name. Please make sure that it does not start with a space or contain characters like ?, <, >, :, *, |, \".", assetPathName.c_str());
		}
	}
	
	AssetTimeStamp origTimeStamp;
	AssetTimeStamp timeStamp;
	if (m_AssetTimeStamps.count(assetPathName)) 
	{
		origTimeStamp = timeStamp = m_AssetTimeStamps[assetPathName];
	}
	else 
	{
		 origTimeStamp = timeStamp = AssetTimeStamp();
	}
	
	// Make sure that the asset path exists!
	if (assetPathName.empty () || ! IsPathCreated(assetPathName) )
	{
		return false;
	}
	
	// Shortcut if we only need to update the text-based .meta file:
	if ( options & kRefreshTextMetaFile ) 
	{
		AssetImporter* importer = FindAssetImporterAtPath (metaDataPath);	
		if ( importer )
		{
			// 
			// Save text-based import settings to .meta file if required 
			
			AssetMetaData* metaData = FindAssetMetaDataAtPath (metaDataPath);
			if (metaData == NULL)
				FatalErrorString (Format("The file %s has no meta data. Most likely the meta data file was deleted. To fix this reimport the asset first.", assetPathName.c_str()));
			ANALYSIS_ASSUME(metaData);

			WriteTextMetaData(selfGUID, asset, timeStamp, metaData->labels, importer);

			if (!(originalAsset == asset))
			{
				SetDirty ();
			}
			m_Assets[selfGUID] = asset;
			
			if (!(origTimeStamp	== timeStamp))
			{
				SetDirty ();
			}
			m_AssetTimeStamps[assetPathName] = timeStamp;
			
			return true;
		}
	}

	if ( options & kAssetWasModifiedOnDisk )
	{
		UnloadAllLoadedAssetsAtPath(assetPathName);
	}

	bool isImportAllowed = true;
	if (options & kImportAssetThroughRefresh)
		isImportAllowed = IsAutomaticImportEnabled (assetPathName);

	// Set name of main representation
	string name = GetMainAssetNameFromAssetPath(assetPathName);
	CheckAssetNameValid(name);

	// Do we really need to update the asset database connections
	bool needToReconnectAsset = true;
	if (parentGUID == asset.parent && asset.mainRepresentation.name == name)
	{
		found = m_Assets.find (asset.parent);
		if (found != m_Assets.end () && find(found->second.children, selfGUID) != found->second.children.end())
			needToReconnectAsset = false;
	}
	
	if (needToReconnectAsset)
	{
		RemoveFromParent(selfGUID, asset.parent);

		// Attach to new parent
		if (parentGUID != UnityGUID ())
		{
			found = m_Assets.find (parentGUID);
			FatalErrorIf (found == m_Assets.end ());
			//bool did = found->second.children.insert (selfGUID).second;
			
			InsertChildSorted(found->second.children, selfGUID, name);
			SetDirty();
		}
		
		
		// We can't cancel import for new assets or moved asssets
		options &= ~kMayCancelImport;
	}
	
	
	asset.parent = parentGUID;
	asset.representations.clear ();
	asset.mainRepresentation = LibraryRepresentation();
	asset.labels.ClearLabels();
	
	// Find the type of the asset (serialized, copy asset) and import it
	AssetImporterSelection importerSelection = AssetImporter::FindAssetImporterSelection (assetPathName);
	AssetImporter* importer = NULL;
	
	TEMP_STRING metaDataString;

	asset.importerClassId = importerSelection.importerClassID;
	asset.importerVersionHash = m_ImporterVersionHashesCache[importerSelection.importerClassID];

	// Read import settings from .meta file if it exists and is modified.
	if (IsFileCreated(textMetaDataPath)) 
	{
		if (GetCacheServer().LoadFromCacheServer (asset, selfGUID, assetPathName, importerSelection, options))
		{
			// Update progressbar
			AssetImporter::GetUpdateProgressCallback () (0.0F, -1.0F, "Importing through CacheServer.");
			
			// Update modification date
			timeStamp.modificationDate = modificationDate;
			timeStamp.metaModificationDate = GetContentModificationDate(textMetaDataPath);
			// Set type (not imported = 0, kSerializedAsset, kCopyAsset)
			RemoveAssetErrors (selfGUID);

			// Change the assets structure only if something has actually changed!
			if (!CompareLibraryRepChanged (asset, m_Assets[selfGUID]))
				m_DidChangeAssetsGraphically = true;	
			if (!(originalAsset == asset) || !(origTimeStamp == timeStamp))
				SetDirty ();
			
			if ( newhash != MdFour() )
			{
				// If the new hash is valid, assign it to the asset.hash.
				// The only case that can go in to here is we've already computed the hash for this asset when we''re doing the 
				// content comparison.
				asset.hash = newhash;
			}

			m_Assets[selfGUID] = asset;
			m_AssetTimeStamps[assetPathName] = timeStamp;

			RemoveFailedAssetImporter(assetPathName);

			// printf_console (" Updating '%s' (CacheServer) [Time: %f ms]\n", assetPathName.c_str(), GetElapsedTimeInSeconds(time) * 1000.0F);

			return true;
		}
		
		// Always load from .meta file (Except when the importer is dirty and .meta file has not changed on disk)
		DateTime modDate = GetContentModificationDate(textMetaDataPath);
		bool useDirtyAssetImporter = IsAssetImporterDirty(selfGUID) && modDate == timeStamp.metaModificationDate;
		if (!useDirtyAssetImporter)
		{
			ReadStringFromFile (&metaDataString, textMetaDataPath);
			timeStamp.metaModificationDate = modDate;
		}
	}
	
	printf_console ("Updating %s - GUID: %s...\n", assetPathName.c_str(), GUIDToString(selfGUID).c_str());
	
	// When we have very quick imports and a lot of them then the time it takes to draw the progressbar becomes a bottleneck.
	// By keeping the asset name the same we only repaint if the overall percentage has changed by at least 1% point.
	const char* customProgressDisplay = GetCustomImporterDisplayName(importerSelection.importerClassID);
	if (customProgressDisplay != NULL)
		AssetImporter::GetUpdateProgressCallback () (0.0F, -1.0F, customProgressDisplay);
	else	
		AssetImporter::GetUpdateProgressCallback () (0.0F, -1.0F, DeleteFirstPathNameComponent(assetPathName));
	
	// Mark it as failed, so that if we crash anywhere during import
	// the information will be tracked.
	AddFailedAssetImporter(assetPathName);
	
	if (importerSelection.generatedAssetType > 0)
		importer = ImportAsset (asset, assetPathName, metaDataPath, importerSelection.importerClassID, options, metaDataString, isImportAllowed);	
	else
		ErrorString ("Couldn't find asset importer. Default importer was probably not setup correctly.");
	
	// Importing was cancelled. We will early out and use the already exiting 
	if(importer == NULL)
	{
		RemoveFailedAssetImporter(assetPathName);
		return false;
	}
	
	// Create, initialize, write AssetMetaData
	AssetMetaData* metaData = CreateOrLoadAssetMetaDataAtPath (metaDataPath);
	metaData->guid = selfGUID;
	metaData->pathName = assetPathName;
	if( metaDataString.size() > 0 )
	{
		string yamlInput(metaDataString.c_str());
		YAMLRead read (yamlInput.c_str(), yamlInput.size(), 0);
		read.Transfer(metaData->labels, "labels");
	}
	
	metaData->SetDirty ();
	
	asset.labels.SetLabels(metaData->labels.begin(), metaData->labels.end());

	// If we have only one object in the representations use let it be the mainRepresentation
	if (asset.representations.size () == 1)
	{
		asset.mainRepresentation = *asset.representations.begin ();
		asset.representations.clear ();
	}
	else if (ms_MostImportantCallback)
	{
		int newMostImportant = ms_MostImportantCallback (asset.representations);
		if (newMostImportant != -1)
		{
			// Set most important object
			asset.mainRepresentation = asset.representations[newMostImportant];
			// Remove item from objects field
			asset.representations.erase (asset.representations.begin () + newMostImportant);
		}
	}

	// We dont have any mainRepresentation, create one default asset
	if (!asset.mainRepresentation.object)
	{
		int id = GetPersistentManager ().GetInstanceIDFromPathAndFileID (metaDataPath, kMainAssetRepresentionFallbackFileID);
		Object* fallbackMainRep = PPtr<Object> (id);
		DestroySingleObject (fallbackMainRep);
		asset.mainRepresentation.classID = Object::StringToClassID ("DefaultAsset");
		fallbackMainRep = Object::Produce (asset.mainRepresentation.classID, id);
		fallbackMainRep->Reset();
		fallbackMainRep->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
		GetPersistentManager ().MakeObjectPersistentAtFileID (id, kMainAssetRepresentionFallbackFileID, metaDataPath);
		asset.mainRepresentation.object = PPtr<EditorExtension> (fallbackMainRep->GetInstanceID ());

		// Put the default asset instance on the list for serialization.
		importer->GetWriteDataArray().push_back (WriteData (kMainAssetRepresentionFallbackFileID, id));
	}
	
	if (asset.mainRepresentation.object->GetName () != name)
		asset.mainRepresentation.object->SetName (name.c_str());
	asset.mainRepresentation.name = name;

	// Set type (not imported = 0, kSerializedAsset, kCopyAsset)
	asset.type = importerSelection.generatedAssetType;

	if ( options & kForceRewriteTextMetaFile ) 
		importer->SetDirty();

	// Save text-based import settings to .meta file if required 
	// Need to do this before writing binary meta data, so importer dirty state
	// is not reset.

	// If a text meta file has appeared or was updated while importing, eg. by download from version control software
	// don't over write it. 
	bool metaFileExists = IsFileCreated(textMetaDataPath);
	bool assetIsKnown = false;
	bool modificationDatesMatch = false;
	if (metaFileExists)
	{
		// if the asset does not exists in the database but the text file exists there is no need to overwrite it
		// either we read the import settings from the file or it has appeared during import
		AssetTimeStamps::iterator found = m_AssetTimeStamps.find(assetPathName);
		if (found != m_AssetTimeStamps.end())
		{
			// The asset is already known
			assetIsKnown = true;

			// If the assets is already know, we need to check the modification date of the text meta file
			DateTime modDate = GetContentModificationDate(textMetaDataPath);

			// If the meta modification date has changed we do not overwrite the meta file
			// if the modification date did change, the text meta file is only overwritten if the importer is dirty
			modificationDatesMatch = modDate == timeStamp.metaModificationDate;
		}
	}

	// Write the text meta if
	//  1. It does not exists
	//  2. It exists, the asset is not known, but we have forced the writing of the meta file
	//     This can happen if there are duplicate guids or the meta is to be deleted
	//  3. It exists, the asset is known and the modification date of the previous import matches the current modification date
	bool forceWrite = options & kForceRewriteTextMetaFile;

	bool didWriteMetaFile = false;
	if (!metaFileExists || (metaFileExists && !assetIsKnown && forceWrite) || (metaFileExists && assetIsKnown && modificationDatesMatch))
		didWriteMetaFile = WriteTextMetaData(selfGUID, asset, timeStamp, metaData->labels, importer);
	else
	{
		// In this case:
		// 1. The asset is new; 2. The meta file is new and hidden ; 3. Enable meta file.
		// we goes into this else and set the meta file to visible.
		bool visibleMetaFile = GetEditorSettings().GetVersionControlRequiresMetaFiles();

		// If the meta file is created and meta file is enabled, we need to remove the hidden flag.
		if(metaFileExists && isHiddenFile(textMetaDataPath) && visibleMetaFile)
		{
			SetFileFlags(textMetaDataPath, kFileFlagHidden, 0);
			timeStamp.refreshFlags = 
				timeStamp.refreshFlags | AssetTimeStamp::kRefreshFoundMetaFile & ~AssetTimeStamp::kRefreshFoundHiddenMetaFile;
		}
	}

	if (didWriteMetaFile)
	{
		if (importerSelection.importerClassID != ClassID(LibraryAssetImporter) 
			&& importerSelection.importerClassID != ClassID(DefaultImporter)
			&& importerSelection.importerClassID != ClassID(Object)
			)
		{
			// Generates hash for the asset if the meta file is changed.
			BuildTargetSelection target = GetEditorUserBuildSettings().GetActiveBuildTargetSelection();
			asset.hash = GenerateHashForAsset (assetPathName, importerSelection.importerClassID, asset.importerVersionHash, target);

			printf_console( "-----------Compute hash for %s.\n", assetPathName.c_str() );
		}
	}
	else if ( newhash != MdFour() )
	{
		// If the extra hash is valid, assign it to the asset.hash.
		// The only case that can go in to here is we've already computed the hash for this asset in 
		// AssetInterface::CheckAssetsChangedActually() method.
		asset.hash = newhash;
	}

	WriteCachedMetaDataAndPreviews (assetPathName, metaDataPath, importer, metaData, asset);

	// Remove and log errors
	RemoveAssetErrors (selfGUID);
	for (AssetImporter::ImportErrors::iterator i=importer->m_ImportErrors.begin ();i != importer->m_ImportErrors.end ();i++)
	{
		if (IsVisibleInLibraryRepresention (i->object, asset))
			DebugStringToFile (i->error, 0, i->file.c_str (), i->line, i->mode, i->object.GetInstanceID(), metaData->GetInstanceID ());
		else
			DebugStringToFile (i->error, 0, i->file.c_str (), i->line, i->mode, asset.mainRepresentation.object.GetInstanceID(), metaData->GetInstanceID ());
	}

	// Update modification date
	timeStamp.modificationDate = modificationDate;
	
	// Change the assets structure only if something has actually changed!
	if (!CompareLibraryRepChanged (asset, m_Assets[selfGUID]))
		m_DidChangeAssetsGraphically = true;	
	
	if (!(originalAsset == asset) || !(origTimeStamp == timeStamp))
		SetDirty ();
	
	m_Assets[selfGUID] = asset;
	m_AssetTimeStamps[assetPathName] = timeStamp;

	bool didUploadToCacheServer = GetCacheServer().CommitToCacheServer (asset, *importer, selfGUID, assetPathName, importerSelection);
	
	// If for some reason the stream got reloaded right away, then there is some issue in the asset importer.
	// Not unloading the stream leads to an explosion in memory usage.
	if (GetPersistentManager().IsStreamLoaded(metaDataPath))
	{
		// This is most likely an issue in the preview generation code.
		// The preview generation may not access data on disk, it has to access data passed to it via the 
		// Ask Steen or Joachim for more details if you can't figure it out.
		
		ErrorString("Asset import did not unload metadata path. This will leak memory. File a bug with repro steps please.");
	}
	
	PPtr<AssetMetaData> metaDataPPtr = metaData;
	PPtr<AssetImporter> importerPPtr = importer;
	importer->UnloadObjectsAfterImport (selfGUID);
	
	// Unload importer & meta data, if they haven't been cleaned up already
	if (Object::IDToPointer(importerPPtr.GetInstanceID()))
		UnloadObject(importerPPtr);
	
	if (Object::IDToPointer(metaDataPPtr.GetInstanceID()))
		UnloadObject(metaDataPPtr);
	
	// If for some reason the stream got reloaded right away, then there is some issue in the asset importer.
	// Not unloading the stream leads to an explosion in memory usage.
	if (GetPersistentManager().IsStreamLoaded(metaDataPath))
	{
		ErrorString("Asset import did not unload metadata path. This will leak memory. File a bug with repro steps please.");
	}
	
	const char* cacheServerInfo = "";
	if (didUploadToCacheServer && didWriteMetaFile)
		cacheServerInfo = " (Uploaded to cache server. .meta file was written.)";
	else if (didUploadToCacheServer)
		cacheServerInfo = " (Uploaded to cache server.)";
	
	printf_console (" done. [Time: %f ms] %s\n", GetElapsedTimeInSeconds(time) * 1000.0F, cacheServerInfo);

	RemoveFailedAssetImporter(assetPathName);
	
	return true;
}

void AssetDatabase::WriteCachedMetaDataAndPreviews (const std::string& assetPath, const std::string& metaDataPath, AssetImporter* importer, AssetMetaData* metaData, Asset& asset)
{
	// Extract all objects that have to be written
	int metaDataFileIndex = GetPersistentManager ().GetSerializedFileIndexFromPath (metaDataPath);
	AssetImporter::WriteDataArray& writeData = importer->GetWriteDataArray();
	writeData.push_back(WriteData (kAssetImporterFileID, importer->GetInstanceID()));
	writeData.push_back(WriteData(kAssetMetaDataFileID, metaData->GetInstanceID()));
	sort(writeData.begin(), writeData.end());
	
	// Create remap for all objects that we are about to write to disk (In order to make references between objects resolve)
	InstanceIDToLocalIdentifier remap;
	for (int i=0;i<writeData.size();i++)
		remap.push_unsorted(writeData[i].instanceID, writeData[i].localIdentifierInFile);
	remap.sort();
	
	// Write using the remap above
	GetPersistentManager ().Lock();
	SetInstanceIDResolveCallback (ResolveAssetImporterWrite, &remap);
	FatalErrorOSErr (GetPersistentManager ().WriteFileInternal (metaDataPath, metaDataFileIndex, writeData.begin(), writeData.size(), NULL, BuildTargetSelection::NoTarget(), 0));
	SetInstanceIDResolveCallback (NULL);
	GetPersistentManager ().Unlock();
	
	Assert(!GetPersistentManager().IsStreamLoaded(metaDataPath));
	
	// Generate mapping of instanceIDs to localIdentifierInFile for previews
	InstanceIDToLocalIdentifier previewIdentifierRemap;
	if (asset.type == kSerializedAsset)
	{
		AddInstanceIDToLocalIdentifier(asset.mainRepresentation.object.GetInstanceID(), previewIdentifierRemap);
		for (int i=0;i<asset.representations.size();i++)
			AddInstanceIDToLocalIdentifier(asset.representations[i].object.GetInstanceID(), previewIdentifierRemap);
		previewIdentifierRemap.sort();
	}
	else
	{
		previewIdentifierRemap = remap;
	}
	
	// Render previews and store them in metadata file
	bool appendSuccessful = AppendAssetPreviewToMetaData(metaDataPath, asset.mainRepresentation, asset.representations, assetPath, previewIdentifierRemap);
	if (!appendSuccessful)
		importer->LogImportError("Asset Preview generation failed");	


	// Map instanceIDs
	if (asset.type != kSerializedAsset)
	{
		MapToPersistentInstanceID (metaDataFileIndex, writeData, asset.mainRepresentation);
		for (int i=0;i<asset.representations.size();i++)
			MapToPersistentInstanceID(metaDataFileIndex, writeData, asset.representations[i]);
	}
	
	Assert(!GetPersistentManager().IsStreamLoaded(metaDataPath));
}


#define FEED_VERSIONS_TO_HASH(x) dynamic_array<UInt32> versions(kMemTempAlloc); \
	GetPostProcessorVersions(versions, x); \
	std::sort(versions.begin(), versions.end()); \
	for (int i = 0; i < versions.size(); ++i) \
		gen.Feed(versions[i]); \


UInt32 HashImporterVersion (int classId, UInt32 version)
{
	MdFourGenerator gen;

	// In order to add a main switch to force re-import all the assets, we add the kAllImporterVersion to the importer hashes.
	// Skip the LibraryAssetImporter which handles the assets under Library folder.
	if ( classId > 0 && classId != ClassID(LibraryAssetImporter) )
		gen.Feed(kAllImporterVersion);
	
	gen.Feed(version);

	if (Object::IsDerivedFromClassID(classId, ModelImporter::GetClassIDStatic()))
	{
		FEED_VERSIONS_TO_HASH("GetMeshProcessorVersions")
	}
	else if (Object::IsDerivedFromClassID(classId, TextureImporter::GetClassIDStatic()))
	{
		FEED_VERSIONS_TO_HASH("GetTextureProcessorVersions")
	}
	else if (Object::IsDerivedFromClassID(classId, AudioImporter::GetClassIDStatic()))
	{
		FEED_VERSIONS_TO_HASH("GetAudioProcessorVersions");
	}

	MdFour hash = gen.Finish();
	return *reinterpret_cast<UInt32*> (hash.bytes);
}

void AssetDatabase::GenerateAssetImporterVersionHashes(AssetImporterVersionHash& hashes)
{
	if (GetMonoManager().HasCompileErrors())
	{
		hashes = m_lastValidVersionHashes;
		return;
	}

	map<int, UInt32> assetImporterClassToVersion;
	AssetImporter::GetAllAssetImporterClassIDAndVersion (assetImporterClassToVersion);
	
	//@TODO: include asset postprocessors
	//       Mesh Pre/Postprocessor supported
	for (map<int, UInt32>::iterator i=assetImporterClassToVersion.begin();i != assetImporterClassToVersion.end();i++)
	{
		// Output the new importer version data.
		UInt32 hash = HashImporterVersion(i->first, i->second);
		hashes[i->first] = hash;
	}

	m_lastValidVersionHashes = hashes;
} 

void AssetDatabase::GetAssetsThatNeedsReimporting (const AssetImporterVersionHash& hashes, std::vector<UnityGUID>& outAssetsToImport) const
{
	UInt32 defaultVersionHash = HashImporterVersion(0, 1);
	Assert(defaultVersionHash == kDefaultImporterVersionHash);
	
	for (Assets::const_iterator it = m_Assets.begin(); it != m_Assets.end(); ++it)
	{
		SInt32 importerClassId = it->second.importerClassId;
		
		AssetImporterVersionHash::const_iterator found = hashes.find(importerClassId);
		
		// if the importer does not have the default hash, we need to test the importer hash of the asset
		if (found != hashes.end () && found->second != it->second.importerVersionHash)
		{
			// Hash mismatch, reimport
			outAssetsToImport.push_back(it->first);
		}
	}
}

void AssetDatabase::RefreshAssetImporterVersionHashesCache ()
{
	m_ImporterVersionHashesCache.clear();
	GenerateAssetImporterVersionHashes(m_ImporterVersionHashesCache);
}

void WriteAssetMetaDataKeepPreviews (const UnityGUID& selfGUID)
{
	string metaDataPath = GetMetaDataPathFromGUID (selfGUID);

	AssetPreviewTemporaryStorage previewStorage;
	ReadAssetPreviewTemporaryStorage (metaDataPath, previewStorage);
	
	FatalErrorOSErr (GetPersistentManager ().WriteFile (metaDataPath));
	
	WriteAssetPreviewTemporaryStorage (metaDataPath, previewStorage);
}


void AssetDatabase::GetAllAssets (vector_set<UnityGUID>& allAssets)
{
	allAssets.reserve(m_Assets.size());

	for (Assets::iterator i=m_Assets.begin ();i != m_Assets.end ();i++)
		allAssets.push_unsorted(i->first);
	
	allAssets.verify_duplicates_and_sorted();
}

void AssetDatabase::GetAllAssets (std::set<UnityGUID>& allAssets)
{
	for (Assets::iterator i=m_Assets.begin ();i != m_Assets.end ();i++)
		allAssets.insert(i->first);
}

void AssetDatabase::GetAllAssets (vector_set<UnityGUID>& rootAssets, vector_set<UnityGUID>& realAssets)
{
	realAssets.reserve(m_Assets.size());
	for (Assets::iterator i=m_Assets.begin ();i != m_Assets.end ();i++)
	{
		if (i->second.parent == UnityGUID ())
			rootAssets.push_unsorted(i->first);
		else
			realAssets.push_unsorted(i->first);
	}	
	
	realAssets.verify_duplicates_and_sorted();
	rootAssets.verify_duplicates_and_sorted();
}

bool AssetDatabase::AssetTimeStampExistsAndRefreshFoundAsset (const std::string& path)
{
	AssetTimeStamps::iterator found = m_AssetTimeStamps.find(path);
	if (found != m_AssetTimeStamps.end())
		return found->second.refreshFlags & AssetTimeStamp::kRefreshFoundAssetFile;
	else
		return false;
}


void AssetDatabase::ResetRefreshFlags ()
{
	for (AssetTimeStamps::iterator i = m_AssetTimeStamps.begin(); i != m_AssetTimeStamps.end(); i++)
		i->second.refreshFlags = 0;
}

void AssetDatabase::GetNeedRemovalAndAddMetaFile (set<string> *needRemoval, set<string> *needAddMetaFile)
{
	bool visibleMetaFile = GetEditorSettings().GetVersionControlRequiresMetaFiles();

	for (AssetTimeStamps::iterator i = m_AssetTimeStamps.begin(); i != m_AssetTimeStamps.end(); ++i)
	{
		UInt32 refreshFlags = i->second.refreshFlags;
		
		// Asset can not be found -> remove asset
		if ((refreshFlags & AssetTimeStamp::kRefreshFoundAssetFile) == 0)
			needRemoval->insert (i->first);
		// This happens
		// 1. useMetaFile is true, but has hidden meta file or no meta file.
		// 2. useMetaFile is false, but no hidden meta file(could have visible meta file which is handled in RecursiveRefreshInternal()).
		//    In RecursiveRefreshInternal(), if there is visible meta file, and use MetaFile is false, it's added to needRemoveMetafile list.
		else if ( (refreshFlags & AssetTimeStamp::kRefreshFoundMetaFile) == 0 && visibleMetaFile
			|| (refreshFlags & (AssetTimeStamp::kRefreshFoundHiddenMetaFile|AssetTimeStamp::kRefreshFoundMetaFile)) == 0 && !visibleMetaFile
			)
			needAddMetaFile->insert (i->first);
	}
}

const Asset& AssetDatabase::GetRoot ()
{
	return m_Assets.insert (make_pair (kAssetFolderGUID, Asset ())).first->second;
}

set<UnityGUID> AssetDatabase::GetAllRootGUIDs ()
{
	set<UnityGUID> all;
	for (Assets::iterator i=m_Assets.begin ();i != m_Assets.end ();i++)
	{
		if (i->second.parent == UnityGUID ())
			all.insert (i->first);
	}
	return all;
}

const Asset* AssetDatabase::AssetPtrFromPath (std::string path)
{
	const UnityGUID guid = GetGUIDPersistentManager().GUIDFromAnySerializedPath(path);

	Assets::iterator found = m_Assets.find (guid);
	if (found != m_Assets.end ())
		return &found->second;
	else
		return NULL;
}

std::string AssetDatabase::ValidateMoveAsset (const UnityGUID& guid, const string& newPathName)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	
	string lowerCaseOldPathName = GetGoodPathName (pm.AssetPathNameFromGUID (guid));
	string lowerCaseNewPathName = GetGoodPathName (newPathName);

	if (GetActualPathSlow(lowerCaseOldPathName) == newPathName)
		return Format("Trying to move asset to location it came from %s", newPathName.c_str());

	if (lowerCaseNewPathName.find(lowerCaseOldPathName) == 0 && lowerCaseNewPathName[lowerCaseOldPathName.size()] == kPathNameSeparator)
		return Format("Trying to move asset as a sub directory of itself %s", newPathName.c_str());

	if (!IsDirectoryCreated(DeleteLastPathNameComponent(newPathName)))
		return Format("Trying to move asset as a sub directory of a directory that does not exist %s", newPathName.c_str());

	if (lowerCaseNewPathName.find ("assets/") != 0)
		return "Can not move asset out of assets/ folder";
	if (m_Assets.find (guid) == m_Assets.end ())
		return "Asset to move is not in asset database";
	
	// Find parent directory guid
	UnityGUID newParentGUID;
	if (!pm.PathNameToGUID (DeleteLastPathNameComponent (newPathName), &newParentGUID))
		return "Could not find parent directory GUID";
	if (m_Assets.find (newParentGUID) == m_Assets.end ())
		return "Parent directory is not in asset database";

	if( !CheckValidFileName(GetLastPathNameComponent(newPathName)) )
		return Format("Invalid file name: '%s'", GetLastPathNameComponent(newPathName).c_str());
	
	if (!IsPathCreated (lowerCaseOldPathName))
		return Format("Can not move asset from %s to %s. Source path does not exist", lowerCaseOldPathName.c_str(), newPathName.c_str());
	
	if (lowerCaseNewPathName != lowerCaseOldPathName && ( IsPathCreated (newPathName)) )
		return Format("Can not move asset from %s to %s. Destination path name does already exist", lowerCaseOldPathName.c_str(), newPathName.c_str());
	
	if (!IsDirectoryCreated (DeleteLastPathNameComponent(newPathName)))
		return Format("Can not move asset from %s to %s. Destination parent directory does not exist", lowerCaseOldPathName.c_str(), newPathName.c_str());
	
	return string();
}


string AssetDatabase::MoveAsset (const UnityGUID& guid, const string& newPathName)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	
	string oldPath = pm.AssetPathNameFromGUID (guid);
	string lowerCaseOldPath = GetGoodPathName(pm.AssetPathNameFromGUID (guid));
	string lowerCaseNewPath = GetGoodPathName(newPathName);

	string error = ValidateMoveAsset(guid, newPathName);
	if (!error.empty())
		return error;
	
	// Find parent directory guid
	UnityGUID newParentGUID;
	if (!pm.PathNameToGUID (DeleteLastPathNameComponent (newPathName), &newParentGUID))
		return "Could not find parent directory GUID";

	Asset& asset = m_Assets.find (guid)->second;

	// Collect the moving assets
	set<UnityGUID> movedAssets;
	map<UnityGUID, string> movedAssetsMap;
	movedAssets.insert (guid);
	CollectAllChildren (guid, &movedAssets);
	
	// Move all the assets (The moved asset itself and all its children)
	for (set<UnityGUID>::iterator i=movedAssets.begin ();i != movedAssets.end ();i++)
	{
		// Unload prior to moving so we don't have the file open when moving it
		string path = pm.AssetPathNameFromGUID (*i);
		GetPersistentManager().UnloadStream(path);
		
		AssetMetaData* metaData = FindAssetMetaDataAtPath (GetMetaDataPathFromGUID (*i));
		if (metaData == NULL)
		{
			string error = Format("The file %s has no meta data. Most likely the meta data file was deleted. To fix this reimport the asset first.", path.c_str()); 
			return error;
		}
	}

	int callbackResult = AssetModificationCallbacks::WillMoveAsset(oldPath, newPathName);
	if (callbackResult == AssetModificationCallbacks::FailedMove)
	{
		return "OnWillMoveAsset callback failed to move asset";
	}

	if (callbackResult == AssetModificationCallbacks::DidNotMove)
	{
		// Move physical files in asset folder
		if (!MoveFileOrDirectory (oldPath, newPathName))
		{
			return Format("Can not move asset from %s to %s. Destination parent directory does not exist", lowerCaseOldPath.c_str(), newPathName.c_str());
		}

		//**** move meta file if exists to new path
		string oldTextMetaDataPath = GetTextMetaDataPathFromAssetPath(oldPath);
		string newTextMeatDataPath = GetTextMetaDataPathFromAssetPath(newPathName);
		if ( IsFileCreated(oldTextMetaDataPath) )
		{
			if (!MoveFileOrDirectory (oldTextMetaDataPath, newTextMeatDataPath))
			{
				return Format("Can not move metafile from %s to %s. Destination parent directory does not exist", oldTextMetaDataPath.c_str(), newTextMeatDataPath.c_str());
			}
		}
	}

	// Move all the assets (The moved asset itself and all its children)
	for (set<UnityGUID>::iterator i=movedAssets.begin ();i != movedAssets.end ();i++)
	{
		// Get old pathname
		string curOldPath = pm.AssetPathNameFromGUID (*i);

		// Build new pathname
		string curNewLowerCasePathName = GetGoodPathName(curOldPath);

		FatalErrorIf (curNewLowerCasePathName.find (lowerCaseOldPath) != 0);
		curNewLowerCasePathName.erase (0, lowerCaseOldPath.size ());
		curNewLowerCasePathName = AppendPathName (lowerCaseNewPath, curNewLowerCasePathName);
		string curNewPath = GetActualPathSlow(curNewLowerCasePathName);

		movedAssetsMap[*i] = curOldPath;

		// Register movement with metadata
		AssetMetaData* metaData = FindAssetMetaDataAtPath (GetMetaDataPathFromGUID (*i));

		if (metaData == NULL)
		{
			FatalErrorString (Format("The file %s has no meta data. Most likely the meta data file was deleted. To fix this reimport the asset first.", curNewPath.c_str()));
		}
		ANALYSIS_ASSUME(metaData);

		RemoveFailedAssetImporter(curOldPath);
		RemoveFailedAssetImporter(curNewPath);

		metaData->pathName = curNewLowerCasePathName;
		metaData->SetDirty ();

		m_AssetTimeStamps[curNewPath] = m_AssetTimeStamps[curOldPath];
		m_AssetTimeStamps.erase(curOldPath);

		// Register movement with persistentmanager
		if (!pm.MoveAsset (*i, curNewPath))
		{
			ErrorString("Failed to move file to " + curNewPath);
		}
	}

	// Remove from old parents child list
	Assets::iterator parent = m_Assets.find (asset.parent);
	AssertIf (parent == m_Assets.end ());

	if (find(parent->second.children.begin(), parent->second.children.end(), guid) != parent->second.children.end())
	{
		parent->second.children.erase (find(parent->second.children.begin(), parent->second.children.end(), guid));
	}
	else
	{
		AssertString ("Child asset was already removed from asset parent inconsistency");
	}

	// Rename the library representation and object name
	string name = GetLastPathNameComponent (newPathName);
	if (asset.type != kFolderAsset)
		name = DeletePathNameExtension (name);
	CheckAssetNameValid(name);

	// Add to new parents child list
	InsertChildSorted(m_Assets.find (newParentGUID)->second.children, guid, name);

	asset.parent = newParentGUID;
	asset.mainRepresentation.name = name;
	SetObjectName (asset.mainRepresentation.object.GetInstanceID (), name);

	// Write out meta data changes
	for (set<UnityGUID>::iterator i=movedAssets.begin ();i != movedAssets.end ();i++)
	{
		WriteAssetMetaDataKeepPreviews (*i);
	}
		

	m_DidChangeAssetsGraphically = true;
	SetDirty ();

	set<UnityGUID> empty;
	Postprocess (empty, empty, empty, movedAssetsMap);
	
	return "";
}

static void DeleteAssetAndMetaFile (const std::string& assetPath, AssetDatabase::RemoveAssetOptions options)
{
	if (assetPath.empty ())
		return;

	string textMetaPath = GetTextMetaDataPathFromAssetPath(assetPath);
	
	if (options == AssetDatabase::kMoveAssetToTrash) 
	{
		if ( IsFileCreated(textMetaPath) )
			MoveToTrash(textMetaPath);
		MoveToTrash(assetPath);
	}
	else
	{
		if ( IsFileCreated(textMetaPath) ) 
			DeleteFile(textMetaPath);
		
		DeleteFileOrDirectory(assetPath);
	}
}


/// We will not Delete the file since that would call MakeObjectUnpersistent.
/// We dont want to call MakeObjectUnpersistent since that would make any ptrs to this object lose it forever on write.
bool AssetDatabase::RemoveAsset (const UnityGUID& guid, RemoveAssetOptions options, set<UnityGUID>* outTrashedAssets)
{
	if (!IsAssetAvailable (guid))
		return false;
	
	// Collect all guids that will be trashed
	set<UnityGUID> trashed;
	trashed.insert (guid);
	CollectAllChildren (guid, &trashed);

	GetPersistentManager ().UnloadStreams ();
	
	// Delete asset files or move files to trash
	if (options == kMoveAssetToTrash || options == kDeleteAssets)
	{
		string mainAssetPath = GetAssetPathFromGUID (guid);
		int deleteResult = AssetModificationCallbacks::WillDeleteAsset(mainAssetPath, options);
		if (deleteResult == AssetModificationCallbacks::FailedDelete)
			return false;

		if (deleteResult == AssetModificationCallbacks::DidNotDelete)
			DeleteAssetAndMetaFile(mainAssetPath, options);
	}

	// Remove from parent. It might not have a parent eg. serialized singletons Library/TagManager.asset
	if (IsAssetAvailable (AssetFromGUID (guid).parent))
	{
		Asset& parent = const_cast<Asset&> (AssetFromGUID(AssetFromGUID (guid).parent));

		if (find (parent.children.begin(), parent.children.end(), guid) != parent.children.end())
		{
			parent.children.erase (find (parent.children.begin(), parent.children.end(), guid));
		}
		else
		{
			ErrorString("Asset was already removed from parent inconsistency");
		}
	}

	// Unload all loaded objects at the assets that are going to be trashed
	set<UnityGUID>::iterator i;
	for (i=trashed.begin ();i != trashed.end ();i++)
	{
		string serializedPath;
		UnityGUID curGUID = *i;
		FatalErrorIf (m_Assets.find (curGUID) == m_Assets.end ());
		Asset& asset = m_Assets.find (curGUID)->second;

		// Remove errors from the log
		RemoveAssetErrors (curGUID);

		// Remove asset from list of failed assets. If the asset did not fail to import nothing happens.
		RemoveFailedAssetImporter (GetGUIDPersistentManager ().AssetPathNameFromGUID (curGUID));

		AssetServerCache::Get().AddDeletedItem(*i); // this needs to know some info about asset before it's removed from database

		// Determine path of to be removed data
		if (asset.type == kSerializedAsset)
		{
			UnloadFileEmptied (GetAssetPathFromGUID (curGUID));
		}

		// Unload cache and meta file
		UnloadFileEmptied (GetMetaDataPathFromGUID (curGUID));
	}	

	// @TODO: Necessary because of AssetServerCache::Get().AddDeletedItem(*i);
	// It would be best if AddDeletedItem would not be loading the metafile...
	GetPersistentManager ().UnloadStreams ();
	
	if (outTrashedAssets)
		outTrashedAssets->insert(trashed.begin(), trashed.end());

	//Unload, delete serialized file
	for (i=trashed.begin ();i != trashed.end ();i++)
	{
		UnityGUID curGUID = *i;
		FatalErrorIf (m_Assets.find (curGUID) == m_Assets.end ());
		Asset& asset = m_Assets.find (curGUID)->second;

		string assetPath = GetAssetPathFromGUID (curGUID);
		string metaPath = GetMetaDataPathFromGUID (curGUID);

		if (asset.type == kSerializedAsset)
		{
			UnloadFileEmptied (assetPath);
			Assert(!GetPersistentManager().IsStreamLoaded(assetPath));
		}
		
		DeleteFile(metaPath);
		Assert(!GetPersistentManager().IsStreamLoaded(metaPath));
		
		m_AssetTimeStamps.erase(assetPath);
		m_Assets.erase (curGUID);
	}

	m_DidChangeAssetsGraphically = true;
	SetDirty ();

	return true;
}

bool AssetDatabase::RemoveFromHierarchy (const UnityGUID& guid)
{
	if (!IsAssetAvailable (guid))
		return true;

	// Collect all guids that will be trashed
	set<UnityGUID> trashed;
	trashed.insert (guid);
 	CollectAllChildren (guid, &trashed);
	
	// Remove from parent. It might not have a parent eg. serialized singletons Library/TagManager.asset
	if (IsAssetAvailable (AssetFromGUID (guid).parent))
	{
		Asset& parent = const_cast<Asset&> (AssetFromGUID(AssetFromGUID (guid).parent));
		
		if (find (parent.children.begin(), parent.children.end(), guid) != parent.children.end())
		{
			parent.children.erase (find (parent.children.begin(), parent.children.end(), guid));
			SetDirty();
		}
		else
		{
			ErrorString("Asset was already removed from parent inconsistency\n");
			return false;
		}
	}
	
	for (set<UnityGUID>::iterator i=trashed.begin();i!=trashed.end();i++)
	{
		ErrorIf(m_Assets.erase(*i) != 1);
		
		RemoveAssetTimeStamp(GetAssetPathFromGUID(*i));
	}
	
	Assert(!IsAssetAvailable (guid));

	return true;
}

void AssetDatabase::RemoveAssetTimeStamp(const string& path) 
{
	AssetTimeStamps::iterator found = m_AssetTimeStamps.find(path);
	if ( found != m_AssetTimeStamps.end() ) 
	{
		m_AssetTimeStamps.erase(found);
		SetDirty();
	}
}

void AssetDatabase::UpdateAssetTimeStamp(const string& path)
{
	AssetTimeStamps::iterator found = m_AssetTimeStamps.find(path);

	if (found == m_AssetTimeStamps.end() )
		return;

	AssetTimeStamp& timeStamp = (*found).second;
	timeStamp.modificationDate = GetContentModificationDate (path);
	timeStamp.metaModificationDate = GetContentModificationDate( GetTextMetaDataPathFromAssetPath(path) );
}

bool AssetDatabase::CollectAllChildren (const UnityGUID& guid, set<UnityGUID>* collection)
{
	if (m_Assets.find (guid) == m_Assets.end ())
		return false;
	
	Asset& asset = m_Assets.find (guid)->second;
	collection->insert (asset.children.begin (), asset.children.end ());
	
	vector<UnityGUID>::iterator i;
	bool result = true;
	for (i=asset.children.begin ();i != asset.children.end ();i++)
		result = result && CollectAllChildren (*i, collection);
	
	return result;
}

bool AssetDatabase::CopyAsset (const UnityGUID& guid, const string& newPathName)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	string lowerCaseOldPathName = GetGoodPathName(pm.AssetPathNameFromGUID (guid));
	string lowerCaseNewPathName = GetGoodPathName (newPathName);
	
	if (lowerCaseOldPathName == lowerCaseNewPathName)
		return false;
	if (m_Assets.find (guid) == m_Assets.end ())
		return false;
	
	// Write out all serialized assets so we can copy them
	bool success = true;

	// Copy the asset
	if (!CopyFileOrDirectory (pm.AssetPathNameFromGUID (guid), newPathName))
		return false;
	
	// Copy metadata
	set<UnityGUID> copiedAssets;
	copiedAssets.insert (guid);
	CollectAllChildren (guid, &copiedAssets);
	
	for (set<UnityGUID>::iterator i=copiedAssets.begin ();i != copiedAssets.end ();i++)
	{
		// Get old pathname
		string curOldLowerCaseAssetPathName = GetGoodPathName(pm.AssetPathNameFromGUID (*i));

		// Build new pathname
		string curNewLowerCasePathName = curOldLowerCaseAssetPathName;
		FatalErrorIf (curOldLowerCaseAssetPathName.find (lowerCaseOldPathName) != 0);
		curNewLowerCasePathName.erase (0, lowerCaseOldPathName.size ());
		curNewLowerCasePathName = AppendPathName (lowerCaseNewPathName, curNewLowerCasePathName);
		
		string curNewPath = GetActualPathSlow(curNewLowerCasePathName);

		// Delete any existing meta files - we are copying the meta data in the library folder,
		// and need to change the GUID while doing so. Copying the meta files will give GUID
		// conflicts.
		string curNewMetaDataPath = GetTextMetaDataPathFromAssetPath(curNewPath);
		DeleteFile (curNewMetaDataPath);

		// Delete old guid
		UnityGUID oldGUIDAtNewPathName;
		if (GetGUIDPersistentManager ().PathNameToGUID (curNewLowerCasePathName, &oldGUIDAtNewPathName))
			GetGUIDPersistentManager ().RemoveGUID (oldGUIDAtNewPathName);
		
		// Aquire copied file guid
		UnityGUID newGUID = GetGUIDPersistentManager ().CreateAsset (curNewPath);
		string newMetaDataPath = GetMetaDataPathFromGUID (newGUID);
		string oldMetaDataPath = GetMetaDataPathFromGUID (*i);

		// Delete old cache/meta files
		GetPersistentManager ().DeleteFile (newMetaDataPath, PersistentManager::kDeleteLoadedObjects);
		
		// Copy metadata file
		if (!CopyFileOrDirectory (oldMetaDataPath, newMetaDataPath))
			success = false;

		// Set metadata to point at copied file
		AssetMetaData* metaData = FindAssetMetaDataAtPath (newMetaDataPath);
		if (metaData)
		{
			metaData->pathName = curNewLowerCasePathName;
			metaData->guid = newGUID;
			metaData->originalChangeset = 0;
			metaData->originalParent = UnityGUID();
			metaData->originalName = "";
			metaData->originalDigest = MdFour();
			metaData->SetDirty ();
			pm.WriteFile (newMetaDataPath);
		}
	}
	
	m_DidChangeAssetsGraphically = true;
	SetDirty ();

	return success;
}

set<int> CalculateDirtyPaths ()
{
	vector<Object*> objects;
	Object::FindObjectsOfType (&objects);
	set<int> paths;
	for (int i=0;i<objects.size();i++)
	{
		Object* o = objects[i];
		if (o->IsPersistent () && o->IsPersistentDirty ())
		{
			SerializedObjectIdentifier identifier;
			GetPersistentManager().InstanceIDToSerializedObjectIdentifier(o->GetInstanceID(), identifier);
			paths.insert(identifier.serializedFileIndex);
		}
	}
	
	return paths;
}

bool AssetDatabase::ShouldWriteSerializedAsset (const UnityGUID& guid, const std::set<int>* dirty) const
{
	const Asset& asset = AssetFromGUID (guid);
	
	if (asset.type == kSerializedAsset)
	{
		FileIdentifier identifier;
		identifier.guid = guid;
		identifier.type = GUIDPersistentManager::kSerializedAssetType;
	
		GUIDPersistentManager& pm = GetGUIDPersistentManager();	

		if (pm.TestNeedWriteFile(pm.InsertFileIdentifierInternal(identifier, false), dirty))
			return true;
	}
	
	return false;
}

void AssetDatabase::ShouldWriteSerializedAssetRecursive (const UnityGUID& guid, const set<int>* dirtyPaths, set<UnityGUID>& savedAssets) const
{
	if (ShouldWriteSerializedAsset(guid, dirtyPaths))
		savedAssets.insert (guid);
		
	const Asset& asset = AssetFromGUID (guid);

	vector<UnityGUID>::const_iterator i;
	const vector<UnityGUID>& children = asset.children;
	for (i=children.begin ();i != children.end ();i++)
		ShouldWriteSerializedAssetRecursive (*i, dirtyPaths, savedAssets);
}

void AssetDatabase::WriteSerializedAsset (const UnityGUID& guid)
{
	Assets::iterator found = m_Assets.find (guid);
	if (found == m_Assets.end())
		return;
		
	GUIDPersistentManager& pm = GetGUIDPersistentManager();	
	Asset originalAsset;
	Asset asset;
	originalAsset = asset = found->second;
	pm.Lock();
	
	FileIdentifier identifier;
	identifier.guid = guid;

	if (asset.type == kSerializedAsset)
	{
		identifier.type = GUIDPersistentManager::kSerializedAssetType;

		string path = pm.AssetPathNameFromGUID (guid);
		if (path.empty ())
		{
			pm.Unlock();
			return;
		}

		NativeFormatImporter* assetImporter = dynamic_pptr_cast<NativeFormatImporter*> (FindAssetImporterAtAssetPath(path));
		
		DateTime oldModificationDate = GetContentModificationDate(path);
		int options = GetEditorSettings().GetSerializationMode() == EditorSettings::kForceText ? kAllowTextSerialization : 0;
		GetPersistentManager ().WriteFile (path, BuildTargetSelection::NoTarget(), options);
		
		// Update hash modification date with the current modification date of the file
		// - We do this only if the written file has been marked as writing due to serialization change, not actual modification
		// - If the modification date of the old file is different than the current one, we also recalculate the modification date
		if (assetImporter)
		{
			AssetMetaData* metaData = FindAssetMetaDataAtPath (GetMetaDataPathFromGUID(guid));
			if (metaData == NULL)
				FatalErrorString (Format("The file %s has no meta data. Most likely the meta data file was deleted. To fix this reimport the asset first.", path.c_str()));
			ANALYSIS_ASSUME(metaData);
			
			WriteTextMetaData(guid, asset, m_AssetTimeStamps[path], metaData->labels, assetImporter);
		}
		m_AssetTimeStamps[path].modificationDate = GetContentModificationDate(path);
		SetDirty();
		
		identifier.type = GUIDPersistentManager::kMetaAssetType;
		if (pm.TestNeedWriteFile(pm.InsertFileIdentifierInternal(identifier, false), NULL))
			GetPersistentManager ().WriteFile (GetMetaDataPathFromGUID (guid));
	}
	
	if ( !(originalAsset == asset) )
	{			
		m_Assets[guid]=asset;
		SetDirty();
	}
	
	pm.Unlock();
}

void AssetDatabase::GetDirtySerializedAssets (set<UnityGUID> &dirtyAssets)
{
	set<int> dirtyPaths = CalculateDirtyPaths();
	ShouldWriteSerializedAssetRecursive (kAssetFolderGUID, &dirtyPaths, dirtyAssets);
}

void AssetDatabase::WriteSerializedAssets (const set<UnityGUID> &writeAssets)
{
	for (set<UnityGUID>::const_iterator i = writeAssets.begin(); i != writeAssets.end(); i++)
		WriteSerializedAsset (*i);
		
	if (!writeAssets.empty())
	{
		set<UnityGUID> empty;
		map<UnityGUID, string> emptyMap;
		Postprocess (writeAssets, empty, empty, emptyMap);
	}
}

void AssetDatabase::Postprocess (const set<UnityGUID>& refreshed, const set<UnityGUID>& added, const set<UnityGUID>& removed, const map<UnityGUID, string>& moved)
{
	for (PostprocessContainer::iterator i = ms_Postprocess.begin ();i != ms_Postprocess.end ();i++)
		(*i) (refreshed, added, removed, moved);
}

void AssetDatabase::RegisterPostprocessCallback (PostprocessCallback* callback)
{
	if (find(ms_Postprocess.begin(), ms_Postprocess.end(), callback) == ms_Postprocess.end())
		ms_Postprocess.push_back (callback);
}

void AssetDatabase::RegisterCalculateMostImportantCallback (CalculateMostImportantCallback* callback)
{
	ms_MostImportantCallback = callback;
}

void AssetDatabase::FindAssetsWithName (const string& fileName, set<string>& outputPaths)
{
	for (Assets::const_iterator i=begin();i != end();i++)
	{
		// Ignore any library assets
		if (i->second.parent == UnityGUID())
			continue;
		
		const string& path = GetAssetPathFromGUID(i->first);
		const char* lastComponent = GetLastPathNameComponent (path.c_str(), path.size());
		if (StrICmp(fileName.c_str(), lastComponent) == 0)
			outputPaths.insert(path);
	}
}

bool AssetDatabase::GetAllResources (std::multimap<UnityStr, PPtr<Object> >& resources)
{
	resources.clear();
	int gameObjectClassID = Object::StringToClassID("GameObject");
	for (Assets::iterator i=m_Assets.begin();i != m_Assets.end();i++)
	{
		Asset& folderAsset = i->second;
		if (folderAsset.type == kFolderAsset && StrICmp(folderAsset.mainRepresentation.name, "Resources") == 0)
		{
			string folderPath = GetAssetPathFromGUID(i->first);
			
			// Ignore Resources folders located inside .bundle files (osx plugins),
			// and .app bundles (in case the user has built a player and saved it to the assets folder) 
			if( ToLower(folderPath).find(".bundle/") != string::npos || ToLower(folderPath).find(".app/") != string::npos) 
				continue;
	
			set<UnityGUID> children;
			if (!CollectAllChildren (i->first, &children))
				return false;
			
			for (set<UnityGUID>::iterator c=children.begin();c != children.end();c++)
			{
				string path = GetAssetPathFromGUID(*c);
				const Asset& asset = AssetFromGUID(*c);
				
				if (asset.type == kFolderAsset)
					continue;
				
				// Extract path relative to Resources folder
				path.erase(path.begin(), path.begin() + folderPath.size() + 1);
				
				// Build resources map
				string lowerPath = DeletePathNameExtension(ToLower(path));
				bool isPrefab = asset.mainRepresentation.classID == gameObjectClassID;

				// Add to resources (But ignore ClassID(DefaultAsset))
				if (asset.mainRepresentation.classID != 1029)
					resources.insert(make_pair(lowerPath, PPtr<Object> (asset.mainRepresentation.object.GetInstanceID())));
				
				for (int j=0;j<asset.representations.size();j++)
				{
					// We only want the root game object in the resources. Not all child game objects! 
					if (isPrefab && asset.representations[j].classID == gameObjectClassID)
						continue;
					
					resources.insert(make_pair(lowerPath, PPtr<Object> (asset.representations[j].object.GetInstanceID())));
				}
			}
		}
	}
	return true;
}
std::set<std::string> AssetDatabase::FindMainAssetsWithClassID (int classID)
{
	set<std::string> paths;
	for (Assets::iterator i=m_Assets.begin();i != m_Assets.end();i++)
	{
		if (i->second.mainRepresentation.classID == classID)
		{
			string path = GetAssetPathFromGUID(i->first);
			paths.insert(path);
		}
	}
	return paths;
}

#if ENABLE_SPRITES
void AssetDatabase::FindAssetsWithImporterClassID (int importerClassId, std::set<UnityGUID>& output)
{
	output.clear();
	for (Assets::iterator i = m_Assets.begin(); i != m_Assets.end(); i++)
	{
		if (i->second.importerClassId == importerClassId)
		{
			output.insert(i->first);
		}
	}
}
#endif

bool operator == (const LibraryRepresentation& lhs, const LibraryRepresentation& rhs)
{
	return
		lhs.name == rhs.name &&
		lhs.object == rhs.object &&
		lhs.classID == rhs.classID &&
		lhs.flags == rhs.flags &&
		lhs.scriptClassName == rhs.scriptClassName &&
		lhs.thumbnail == rhs.thumbnail;
}

bool operator == (const Asset& lhs, const Asset& rhs)
{
	return lhs.type == rhs.type && lhs.parent == rhs.parent && lhs.mainRepresentation == rhs.mainRepresentation && lhs.children == rhs.children && lhs.representations == rhs.representations && lhs.importerClassId == rhs.importerClassId && lhs.importerVersionHash == rhs.importerVersionHash;
}

bool operator == (const AssetTimeStamp& lhs, const AssetTimeStamp& rhs)
{
	return lhs.modificationDate == rhs.modificationDate && lhs.metaModificationDate == rhs.metaModificationDate;
}

bool CompareLibraryRepChanged (const Asset& lhs, const Asset& rhs)
{
	return lhs.type == rhs.type && lhs.parent == rhs.parent && lhs.mainRepresentation == rhs.mainRepresentation && lhs.children == rhs.children && lhs.representations == rhs.representations;
}

AssetDatabase& AssetDatabase::Get ()
{
	AssertIf (gAssetDatabase == NULL);
	return *gAssetDatabase;
}

void AssetDatabase::SetAssetDatabase (AssetDatabase* database)
{
	gAssetDatabase = database;
}

void AssetDatabase::SetShaderImportVersion (int shaderVersion)
{
	if (m_UnityShadersVersion != shaderVersion)
	{
		m_UnityShadersVersion = shaderVersion;
		SetDirty();
	}
}

set<UnityGUID> ObjectsToGuids (const TempSelectionSet& objects)
{
	set<UnityGUID> guids;
	for (TempSelectionSet::const_iterator i=objects.begin();i != objects.end();i++)
	{
		UnityGUID guid = ObjectToGUID (*i);
		if (guid.IsValid() )
			guids.insert(guid);
	}
	return guids;
}

void GuidsToObjects (const set<UnityGUID>& guids, TempSelectionSet& objects)
{
	objects.clear();
	for (set<UnityGUID>::const_iterator i=guids.begin();i != guids.end();i++)
	{
		const Asset* asset = AssetDatabase::Get ().AssetPtrFromGUID (*i);
		if (asset && asset->mainRepresentation.object.IsValid())
			objects.insert(asset->mainRepresentation.object);
	}
}

UnityGUID ObjectToGUID (PPtr<Object> o)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	UnityGUID guid = pm.GUIDFromAnySerializedPath (pm.GetPathName (o.GetInstanceID ()));
	return guid;
}

int GetMainAssetInstanceID (const std::string& path)
{
	std::string convertedPath = path;
	ConvertSeparatorsToUnity(convertedPath);
	
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	UnityGUID guid;
	if (!pm.PathNameToGUID(convertedPath, &guid))
		return 0;
	
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset)
		return asset->mainRepresentation.object.GetInstanceID();
	else
		return 0;
}

Object* GetMainAsset (const std::string& path)
{
	return dynamic_instanceID_cast<Object*> (GetMainAssetInstanceID(path));
}

void GetAllAssetRepresentations (const std::string& path, vector<PPtr<Object> >& output)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	UnityGUID guid;
	if (!pm.PathNameToGUID(path, &guid))
	{
		output = std::vector<PPtr<Object> > ();
		return;
	}
	
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset)
	{
		for (int i=0;i<asset->representations.size ();i++)
			output.push_back(PPtr<Object>(asset->representations[i].object));
		return;
	}
	
	output = std::vector<PPtr<Object> > ();
}

bool IsMainAsset (int instanceID)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager();
	UnityGUID guid = pm.GUIDFromAnySerializedPath(pm.GetPathName(instanceID));
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset)
		return asset->mainRepresentation.object.GetInstanceID () == instanceID;
	else
		return false;
}

bool IsSubAsset (int instanceID)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager();
	UnityGUID guid = pm.GUIDFromAnySerializedPath(pm.GetPathName(instanceID));
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset)
	{
		int mainInstanceID = asset->mainRepresentation.object.GetInstanceID();
		if (mainInstanceID != instanceID)
		{
			for (int i = 0; i < asset->representations.size(); i++)
			{
				if (asset->representations[i].object.GetInstanceID() == instanceID)
					return true;
			}
		}
	}

	return false;
}


inline bool IsConstantGUID (const UnityGUID& guid)
{
	return guid.data[0] == 0 && guid.data[1] == 0 && guid.data[3] == 0 ;
}

bool CalculateParentGUID (const UnityGUID& guid, UnityGUID* parentGUID, std::string* warning)
{
	if (!IsConstantGUID (guid))
	{
		string assetPath = GetGUIDPersistentManager ().AssetPathNameFromGUID (guid);
		string parentPath = ToLower (DeleteLastPathNameComponent (assetPath));
		if (parentPath.empty ())
		{
			*warning = Format("Ignoring asset refresh of %s because the file or its directory couldn't be found!\n", assetPath.c_str ());
			return false;
		}
		if (parentPath.find ("assets") != 0)
		{
			*warning = Format("Ignoring asset refresh of %s because the file isn't in the Assets directory!\n", assetPath.c_str ());
			return false;
		}
		
		if (GetGUIDPersistentManager ().PathNameToGUID(parentPath, parentGUID) && AssetDatabase::Get ().IsAssetAvailable (*parentGUID))
			return true;
		else
		{
			*warning = Format("Ignoring asset refresh of %s because the folder it is in has not been imported yet!\n", assetPath.c_str ());
			*parentGUID = UnityGUID ();
			return false;
		}
	}
	else
	{
		*parentGUID = UnityGUID ();
		return true;
	}
}
