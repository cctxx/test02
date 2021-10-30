#include "UnityPrefix.h"
#include "AssetMetaData.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

IMPLEMENT_CLASS (AssetMetaData)
IMPLEMENT_OBJECT_SERIALIZE (AssetMetaData)

AssetMetaData::AssetMetaData(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	guid()
,	pathName()
,	originalChangeset(0)
,	originalName()
,	originalParent()
,	originalDigest()
,	labels()
,	assetStoreRef(0)
{ }

AssetMetaData::~AssetMetaData ()
{
}

template<class TransferFunction>
void AssetMetaData::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion(2);
	TRANSFER (guid);
	TRANSFER (pathName);

	TRANSFER (originalChangeset);
	TRANSFER (originalName);
	TRANSFER (originalParent);
	TRANSFER (originalDigest);
	TRANSFER (labels);
	TRANSFER (assetStoreRef);

	
	if (transfer.IsVersionSmallerOrEqual(1))
	{
		transfer.Transfer(originalChangeset,"originalVersion");
	}
}

