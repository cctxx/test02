#include "UnityPrefix.h"
#include "LibraryAssetImporter.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Graphics/Image.h"
#include "AssetInterface.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "AssetDatabase.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Misc/SaveAndLoadHelper.h"

// Increasing the version number causes an automatic reimport when the last imported version is a different.
// Only increase this number if an older version of the importer generated bad data or the importer applies data optimizations that can only be done in the importer.
// Usually when adding a new feature to the importer, there is no need to increase the number since the feature should probably not auto-enable itself on old import settings.
enum { kLibraryAssetImporterVersion = 2 };

using namespace std;

static set<string> gSupportedPaths;
static int CanLoadPathName (const string& pathName, int*);

void LibraryAssetImporter::InitializeClass ()
{
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (LibraryAssetImporter), kLibraryAssetImporterVersion, -900);
}

int LibraryAssetImporter::CanLoadPathName (const string& pathName, int* queue)
{
	*queue = -9999999;
	if (gSupportedPaths.count (ToLower (pathName)))
		return kSerializedAsset;	
	else
		return 0;
}

LibraryAssetImporter::LibraryAssetImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

LibraryAssetImporter::~LibraryAssetImporter ()
{}

void LibraryAssetImporter::EndImport (dynamic_array<Object*>& importedObjects)
{
	string assetPathName = GetAssetPathName ();
	
	dynamic_array<Object*> temp (kMemTempAlloc);
	Super::EndImport (temp);
	
	ReloadAssetsThatWereModifiedOnDisk();
	
	// Reload managers, bail out if the manager isn't availible anymore!
	AssetInterface::Get ().ReloadSingletonAssets ();

	set<SInt32> serializedObjects;
	GetPersistentManager ().GetInstanceIDsAtPath (assetPathName, &serializedObjects);
	
	std::set<PPtr<Object> > imported;
	
	for (set<SInt32>::iterator i = serializedObjects.begin ();i != serializedObjects.end ();i++)
	{
		EditorExtension* object = dynamic_instanceID_cast<EditorExtension*> (*i);
		if (object == NULL)
			continue;

		Image image = ImageForClass (object->GetClassID ());
		SetThumbnail (image, object->GetInstanceID ());
		importedObjects.push_back (object);
	}
}

void LibraryAssetImporter::GenerateAssetData () { }

void LibraryAssetImporter::RegisterSupportedPath (const string& extension)
{
	gSupportedPaths.insert (ToLower (extension));
}

template<class TransferFunction>
void LibraryAssetImporter::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	PostTransfer (transfer);
}


IMPLEMENT_CLASS_HAS_INIT (LibraryAssetImporter)
IMPLEMENT_OBJECT_SERIALIZE (LibraryAssetImporter)
