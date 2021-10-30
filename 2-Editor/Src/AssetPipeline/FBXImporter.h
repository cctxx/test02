#ifndef FBXIMPORTER_H
#define FBXIMPORTER_H

#include "ModelImporter.h"

class FBXImporter : public ModelImporter
{
	int m_OverrideSampleRate;
	
public:
	
	REGISTER_DERIVED_CLASS (FBXImporter, ModelImporter)
	
	FBXImporter (MemLabelId label, ObjectCreationMode mode) : Super(label, mode) {}
	// ~FBXImporter (); declared-by-macro
	
	void SetOverrideSampleRate (int rate) { m_OverrideSampleRate = rate; }
	
	virtual bool DoMeshImport (ImportScene& outputScene);
	virtual bool IsUseFileUnitsSupported() const;
	virtual bool IsBakeIKSupported ();
	virtual bool IsTangentImportSupported() const;

	static void InitializeClass ();
	static void CleanupClass () {}

private:
	std::string ConvertToFBXFile ();
};

bool RequiresExternalImporter (const std::string& pathName);

#endif
