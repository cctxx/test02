#ifndef NATIVEFORMATIMPORTER_H
#define NATIVEFORMATIMPORTER_H

#include "AssetImporter.h"

class NativeFormatImporter : public AssetImporter
{
	public:
	
	REGISTER_DERIVED_CLASS (NativeFormatImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (NativeFormatImporter);

	NativeFormatImporter (MemLabelId label, ObjectCreationMode mode);
	// ~NativeFormatImporter (); declared-by-macro
	
	virtual void GenerateAssetData ();
	
	virtual void EndImport (dynamic_array<Object*>& importedObjects);
	virtual void UnloadObjectsAfterImport (UnityGUID guid);

	
	static void RegisterNativeFormatExtension (const string& extension);
	static void InitializeClass ();
	static void CleanupClass () {}

};

#endif
