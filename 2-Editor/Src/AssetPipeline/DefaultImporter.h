#ifndef DEFAULTIMPORTER_H
#define DEFAULTIMPORTER_H

#include "AssetImporter.h"

class DefaultImporter : public AssetImporter
{
	public:
	
	REGISTER_DERIVED_CLASS (DefaultImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (DefaultImporter);
	
	DefaultImporter (MemLabelId label, ObjectCreationMode mode);
	// ~DefaultImporter (); declared-by-macro
	
	virtual void GenerateAssetData ();
		
	static void InitializeClass();
	static void CleanupClass() {}
};

// Default asset class containing nothing
class DefaultAsset : public NamedObject
{
	public:
	REGISTER_DERIVED_CLASS (DefaultAsset, NamedObject)
	
	DefaultAsset (MemLabelId label, ObjectCreationMode mode);
	// ~DefaultAsset (); declared-by-macro
};

// Scene asset class containing nothing (we want to be able to search for scenes by classID)
class SceneAsset : public DefaultAsset
{
	public:
	REGISTER_DERIVED_CLASS (SceneAsset, DefaultAsset)
	
	SceneAsset (MemLabelId label, ObjectCreationMode mode);
	// ~SceneAsset (); declared-by-macro
};


void GenerateDefaultImporterAssets (AssetImporter& assetImporter, bool generateWarningIcon);

/// Creates a default asset with the thumbnail
/// generated from the asset file's finder representation.
//DefaultAsset& CreateDefaultAsset (AssetImporter& importer);













#endif
