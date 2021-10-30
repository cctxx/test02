#ifndef TEXTSCRIPTIMPORTER_H
#define TEXTSCRIPTIMPORTER_H

#include "AssetImporter.h"

class TextAsset;

class TextScriptImporter : public AssetImporter {
	public:
	
	REGISTER_DERIVED_CLASS (TextScriptImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (TextScriptImporter)
	
	TextScriptImporter (MemLabelId label, ObjectCreationMode mode);
	// ~TextScriptImporter (); declared-by-macro

	virtual void GenerateAssetData ();
	
	static void InitializeClass ();
	static void CleanupClass () {}

};

#endif
