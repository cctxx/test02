#ifndef HEIGHTMAPDATA_H
#define HEIGHTMAPDATA_H

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/dynamic_array.h"

struct HeightmapData
{
	DECLARE_SERIALIZE (HeightmapData)

	Vector3f position;
	PPtr<Object> terrainData;
};

typedef dynamic_array<HeightmapData> HeightmapDataVector;

template<class TransferFunction>
void HeightmapData::Transfer (TransferFunction& transfer)
{
	TRANSFER (position);
	TRANSFER (terrainData);
}

#endif
