#pragma once

#include "AssetImporter.h"

class ComputeShaderImporter : public AssetImporter
{
public:
	REGISTER_DERIVED_CLASS (ComputeShaderImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (ComputeShaderImporter)
	
	ComputeShaderImporter(MemLabelId label, ObjectCreationMode mode);
	// ~ComputeShaderImporter (); declared-by-macro
	
	virtual void GenerateAssetData ();
	
	static void InitializeClass ();
	static void CleanupClass ();

private:
	void GenerateComputeShaderData ();
};
