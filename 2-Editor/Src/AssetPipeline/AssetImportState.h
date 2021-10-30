#ifndef ASSET_IMPORT_STATE_H
#define ASSET_IMPORT_STATE_H

#include "Runtime/Serialize/SerializationMetaFlags.h"

class AssetImportState
{
public:

	AssetImportState ();	
	void SetImportedForTarget (BuildTargetSelection selection);
	
	BuildTargetSelection GetImportedForTarget ();
	
	void SetDidImportTextureUncompressed ();
	
	void SetDidImportAssetForTarget (BuildTargetSelection selection);
	void SetDidImportAssetForTarget (BuildTargetPlatform targetPlatform);

	int GetDidImportRemappedSRGB () { return m_RemappedSRGB; }
	void SetDidImportRemappedSRGB (int sRGB);
	
	void SetCompressAssetsPreference (bool compress);
	bool GetCompressAssetsPreference ();
protected:
	
	void WriteStateToFile();

	BuildTargetSelection m_SelectionForTarget;
	int                  m_RemappedSRGB;
};

enum { kNoRemappedSRGB = -1, kRemappedSRGBUndefined = -2 };

AssetImportState& GetAssetImportState ();

#endif
