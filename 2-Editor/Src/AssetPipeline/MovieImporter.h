#ifndef MOVIEIMPORTER_H
#define MOVIEIMPORTER_H

#include "AssetImporter.h"
#include "Runtime/Video/MovieTexture.h"

class MovieImporter : public AssetImporter
{
private:
	
	float m_Quality;
	bool m_LinearTexture;
	
	bool ImportOggMovie (string pathName);
	void ImportQuickTimeMovie (string pathName);
	
public:
	
	REGISTER_DERIVED_CLASS (MovieImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (MovieImporter)

	MovieImporter(MemLabelId label, ObjectCreationMode mode);
	// ~MovieImporter (); declared-by-macro
	
	GET_SET_COMPARE_DIRTY (float, Quality, m_Quality);
	GET_SET_COMPARE_DIRTY (bool, LinearSampled, m_LinearTexture);

	float GetDuration() const;
	
	virtual void GenerateAssetData ();
	
	static void InitializeClass ();
	static void CleanupClass () {}
};

bool ExtractOggFile(Object* obj, std::string path);

#endif
