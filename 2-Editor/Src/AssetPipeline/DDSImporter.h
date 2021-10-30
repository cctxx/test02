#ifndef DDSIMPORTER_H
#define DDSIMPORTER_H

#include "AssetImporter.h"
#include "Runtime/Graphics/TextureFormat.h"

void FlipTextureData(int width, int height, TextureFormat format, int mipMapCount, UInt8 *src, UInt8 *dst);

class DDSImporter : public AssetImporter
{
	private:
		
	public:
	
	REGISTER_DERIVED_CLASS (DDSImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (DDSImporter);

	DDSImporter (MemLabelId label, ObjectCreationMode mode);
	// ~DDSImporter ();

	
	virtual void GenerateAssetData ();

	static void InitializeClass ();
	static void CleanupClass () {}

};


#endif
