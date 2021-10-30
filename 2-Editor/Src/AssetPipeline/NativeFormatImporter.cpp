#include "UnityPrefix.h"
#include "NativeFormatImporter.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "AssetDatabase.h"
#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Utilities/Word.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Shaders/Material.h"
#include "BumpMapSettings.h"

enum { kNativeFormatImporterVersion = 3 };

using namespace std;

static set<string> gNativeFormatExtensions;
static int CanLoadPathName (const string& pathName, int* queue);

void NativeFormatImporter::InitializeClass ()
{
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (NativeFormatImporter), kNativeFormatImporterVersion);
}

static int CanLoadPathName (const string& pathName, int* queue)
{
	string ext = ToLower (GetPathNameExtension (pathName));
	// Native formats should be imported directly after importing scripts.
	// Scripts need to come first because we might lose properties in prefabs otherwise
	// But otherwise we will never lose any pptrs. But other importers might depend on the native assets existing already.
	// Eg. materials from the fbx importer.
	*queue = -1000;

	// [case 469262] Project with Animator corrupted after deleting Library folder
	// Animator controller depend heavily on imported clip from the modelimporter
	// thus it need to be imported right after these
	if (ext == "controller")
		*queue = 150;

	if (gNativeFormatExtensions.count (ext))
		return kSerializedAsset;	
	else
		return 0;
}

NativeFormatImporter::NativeFormatImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{}

NativeFormatImporter::~NativeFormatImporter ()
{}


void NativeFormatImporter::EndImport (dynamic_array<Object*>& importedObjects)
{
	string assetPathName = GetAssetPathName ();

	dynamic_array<Object*> temp (kMemTempAlloc);
	Super::EndImport (temp);

	set<SInt32> serializedObjects;
	GetPersistentManager ().GetInstanceIDsAtPath (assetPathName, &serializedObjects);
	
	Prefab* prefab = NULL;
	// Find objects that should be displayed in the editor first	
	for (set<SInt32>::iterator i = serializedObjects.begin ();i != serializedObjects.end ();i++)
	{
		int classID = GetPersistentManager ().GetSerializedClassID(*i);
		if (classID != -1 && IsDerivedFromClassID(classID, ClassID(GlobalGameManager)) )
		{
			ErrorString ("You are trying to import an asset which contains a global game manager. This is not allowed.");
			continue;
		}

		Object* object = dynamic_instanceID_cast<Object*> (*i);
		if (object == NULL)
			continue;
		
		// Only support EditorExtension derived classes & Prefabs
		if (dynamic_pptr_cast<EditorExtension*> (object) == NULL && dynamic_pptr_cast<Prefab*> (object) == NULL)
			continue;
		
		Material* material = dynamic_pptr_cast<Material*> (object);
		if (material)
			BumpMapSettings::Get().PerformBumpMapCheck(*material);
		
		if (dynamic_pptr_cast<Prefab*> (object))
			prefab = dynamic_pptr_cast<Prefab*> (object);
		
		Unity::Component* component = dynamic_pptr_cast<Unity::Component*> (object);

		if (component && component->GetClassID() != ClassID(MonoBehaviour) && component->GetGameObjectPtr() == NULL)
		{
			LogImportWarning(Format("'%s' is not attached to any game object, the component will be deleted.", component->GetClassName().c_str()));
			DestroySingleObject(object);
			continue;
		}
		
		importedObjects.push_back (object);
	}
	
	// Merge data template changes, with children. This needs to be done in the importer because the prefab won't be dirty when
	// importing eg. from a package or maint. So the editor controller wont be picking the dirtyness up 
	if (prefab)
		 MergeAllPrefabInstances (prefab);
}

void UnloadObjects (int classID, const set<SInt32>& memoryIDs)
{
	for (set<SInt32>::const_iterator i=memoryIDs.begin ();i != memoryIDs.end ();i++)
	{
		Object* object = Object::IDToPointer(*i);
		if (object != NULL && object->IsDerivedFrom(classID))
		{
			if (object->IsPersistentDirty())
				continue;
				
			UnloadObject(object);	
		}
	}
}

void NativeFormatImporter::UnloadObjectsAfterImport (UnityGUID guid)
{
	Super::UnloadObjectsAfterImport(guid);
	set<SInt32> memoryIDs;
	GetPersistentManager ().GetLoadedInstanceIDsAtPath (GetAssetPathFromGUID(guid), &memoryIDs);
	GetPersistentManager ().GetLoadedInstanceIDsAtPath (GetMetaDataPathFromGUID(guid), &memoryIDs);

/// SUPPORT UNLOADING GAME OBJECTS	
//	vector<PPtr<GameObject> > gos;
	
	UnloadObjects(Object::StringToClassID("Material"), memoryIDs);
	UnloadObjects(Object::StringToClassID("Texture"), memoryIDs);
	UnloadObjects(Object::StringToClassID("AudioClip"), memoryIDs);
	UnloadObjects(Object::StringToClassID("TerrainData"), memoryIDs);
	UnloadObjects(ClassID(Prefab), memoryIDs);
}

void NativeFormatImporter::GenerateAssetData () { }

void NativeFormatImporter::RegisterNativeFormatExtension (const string& extension)
{
	gNativeFormatExtensions.insert (ToLower (extension));
}

template<class TransferFunction>
void NativeFormatImporter::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	PostTransfer (transfer);
}




IMPLEMENT_CLASS_HAS_INIT (NativeFormatImporter)
IMPLEMENT_OBJECT_SERIALIZE (NativeFormatImporter)
