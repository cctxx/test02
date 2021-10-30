#ifndef ASSETMETADATA_H
#define ASSETMETADATA_H

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Utilities/GUID.h"
#include "MdFourGenerator.h"
#include "AssetLabels.h"

class AssetMetaData : public Object {
 public:

	DECLARE_OBJECT_SERIALIZE (AssetMetaData)
	REGISTER_DERIVED_CLASS (AssetMetaData, Object)

	UnityGUID guid;
	UnityStr pathName;
	UInt32 originalChangeset;
	UnityStr originalName;
	UnityGUID originalParent;
	MdFour originalDigest;
	std::vector<UnityStr> labels;
	UInt64 assetStoreRef; // The asset store will insert the package_version id into this variable when building packages

	AssetMetaData(MemLabelId label, ObjectCreationMode mode);
	// ~AssetMetaData (); declared-by-macro
};

#endif
