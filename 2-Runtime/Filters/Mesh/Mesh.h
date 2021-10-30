#ifndef MESH_H
#define MESH_H

#include <vector>
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Misc/Allocator.h"

class Quaternionf;

/// A face in the mesh.
struct Face {
	UInt16 v1, v2, v3;
	Face (UInt16 vert1, UInt16 vert2, UInt16 vert3) 
		{v1 = vert1; v2 = vert2; v3 = vert3;}
	Face () {}
	
	UInt16 &operator[] (int i) { return (&v1)[i]; }
	UInt16 operator[] (int i) const  { return (&v1)[i]; }
	
	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (Face)
};

template<class TransferFunc>
void Face::Transfer (TransferFunc& transfer)
{
	TRANSFER (v1);
	TRANSFER (v2);
	TRANSFER (v3);
}

struct DeprecatedTangent
{
	Vector3f normal;
	Vector3f tangent;
	float handedness;
	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (Tangent)
};

template<class TransferFunc>
void DeprecatedTangent::Transfer (TransferFunc& transfer)
{
	TRANSFER (normal);
	TRANSFER (tangent);
	TRANSFER (handedness);
}

struct BoneInfluence
{
	float weight[4];
	int   boneIndex[4];

	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (BoneInfluence)
};

struct BoneInfluence2
{
	float weight[2];
	int   boneIndex[2];
};

template<class TransferFunc>
void BoneInfluence::Transfer (TransferFunc& transfer)
{
	TRANSFER (weight[0]);
	TRANSFER (weight[1]);
	TRANSFER (weight[2]);
	TRANSFER (weight[3]);

	TRANSFER (boneIndex[0]);
	TRANSFER (boneIndex[1]);
	TRANSFER (boneIndex[2]);
	TRANSFER (boneIndex[3]);
}

#endif
