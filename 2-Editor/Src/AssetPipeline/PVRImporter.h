#ifndef PVRIMPORTER_H
#define PVRIMPORTER_H

#include "AssetImporter.h"
#include "Runtime/Graphics/TextureFormat.h"

class PVRImporter : public AssetImporter
{
	private:
		
	public:
	
	REGISTER_DERIVED_CLASS (PVRImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (PVRImporter)

	PVRImporter (MemLabelId label, ObjectCreationMode mode);
	// PVRImporter ();

	virtual void GenerateAssetData ();

	static void InitializeClass ();
	static void CleanupClass () {}

};


#endif
