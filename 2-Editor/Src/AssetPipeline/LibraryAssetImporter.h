#ifndef LIBRARYASSETIMPORTER_H
#define LIBRARYASSETIMPORTER_H

#include "AssetImporter.h"

class LibraryAssetImporter : public AssetImporter
{
	public:
	
	REGISTER_DERIVED_CLASS (LibraryAssetImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (LibraryAssetImporter);
	
	LibraryAssetImporter (MemLabelId label, ObjectCreationMode mode);
	// ~LibraryAssetImporter (); declared-by-macro
	
	virtual void GenerateAssetData ();
	virtual void EndImport (dynamic_array<Object*>& importedObjects);
		
	static int CanLoadPathName (const string& pathName, int* queue);
	static void RegisterSupportedPath (const string& extension);
	static void InitializeClass ();
	static void CleanupClass () {}
};

#endif
