#ifndef MESHBLENDSHAPES_H
#define MESHBLENDSHAPES_H

#include "Runtime/Geometry/AABB.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Containers/ConstantString.h"
#include "Runtime/Containers/ConstantStringSerialization.h"

typedef UInt32 BindingHash;

struct BlendShapeVertex
{
	// vertex, normal & tangent are stored as deltas
	Vector3f  vertex;
	Vector3f  normal;
	Vector3f  tangent;
	UInt32    index;

	BlendShapeVertex() : vertex(Vector3f::zero), normal(Vector3f::zero), tangent(Vector3f::zero), index(0) {}

	DECLARE_SERIALIZE_NO_PPTR (BlendShapeVertex)
};
typedef dynamic_array<BlendShapeVertex> BlendShapeVertices;

struct BlendShapeChannel
{
	ConstantString name;
	BindingHash    nameHash;
	
	int            frameIndex;
	int            frameCount;
	
	DECLARE_SERIALIZE_NO_PPTR(MeshBlendShapeChannel)
};

struct BlendShape
{
	BlendShape() : firstVertex(0), vertexCount(0), hasNormals(false), hasTangents(false) {}
	
	UInt32   firstVertex;
	UInt32   vertexCount;

	bool     hasNormals;
	bool     hasTangents;

	
	///@TODO: MOve
	// updates hasNormals and hasTangents based on data in vertices
	void UpdateFlags(const BlendShapeVertices& sharedSparceVertices);

	DECLARE_SERIALIZE_NO_PPTR (MeshBlendShape)
};

struct BlendShapeData
{
	BlendShapeVertices                 vertices;
	dynamic_array<BlendShape>          shapes;
	std::vector<BlendShapeChannel>     channels;
	dynamic_array<float>               fullWeights;
	
	DECLARE_SERIALIZE_NO_PPTR(BlendShapeData)
};


// Convert between blendshape name and index
const char* GetChannelName (const BlendShapeData& data, int index);
inline size_t GetBlendShapeChannelCount (const BlendShapeData& data) { return data.channels.size(); }
int GetChannelIndex (const BlendShapeData& data, const char* name);
int GetChannelIndex (const BlendShapeData& data, BindingHash name);

// data is passed as non-sparce arrays, i.e. deltaVertices.size() has to be the same as vertex count on the Mesh
void SetBlendShapeVertices(const std::vector<Vector3f>& deltaVertices, const std::vector<Vector3f>& deltaNormals, const std::vector<Vector3f>& deltaTangents, BlendShapeVertices& sharedSparceVertices, BlendShape& frame);
void InitializeChannel (const UnityStr& inName, int frameIndex, int frameCount, BlendShapeChannel& channel);
void ClearBlendShapes (BlendShapeData& data);

template<class TransferFunc>
void BlendShape::Transfer (TransferFunc& transfer)
{
	TRANSFER(firstVertex);
	TRANSFER(vertexCount);
	TRANSFER(hasNormals);
	TRANSFER(hasTangents);
	transfer.Align();
}

template<class TransferFunc>
void BlendShapeData::Transfer (TransferFunc& transfer)
{
	TRANSFER (vertices);
	TRANSFER (shapes);
	TRANSFER (channels);
	TRANSFER (fullWeights);
}

template<class TransferFunc>
void BlendShapeVertex::Transfer (TransferFunc& transfer)
{
	TRANSFER(vertex);
	TRANSFER(normal);
	TRANSFER(tangent);
	TRANSFER(index);
}

template<class TransferFunc>
void BlendShapeChannel::Transfer (TransferFunc& transfer)
{
	TransferConstantString (name, "name", kNoTransferFlags, kMemGeometry, transfer);
	TRANSFER (nameHash);
	TRANSFER (frameIndex);
	TRANSFER (frameCount);
}

#endif
