#ifndef MESHSKINNING_H
#define MESHSKINNING_H

#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Mesh.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include <vector>
#include <list>

class GPUSkinningInfo;

typedef std::vector<BoneInfluence> CompactSkin;
struct BlendShapeData;

enum TransformInstruction { kNormalizeFastest = 0, kNormalizeFast = 1, kNoNormalize = 3 };
class VertexData;

struct SkinMeshInfo
{
	int bonesPerVertex;
	
	void* compactSkin;
	int boneCount;

	const void* inVertices;
	void*	outVertices;
	int		inStride;
	int		outStride;

	int		normalOffset;
	int		tangentOffset;
	bool	skinNormals;
	bool	skinTangents;

	int   vertexCount;

	// This is instance data and must be double buffered so the render thread can work in paralell.
	UInt8*                      allocatedBuffer;
	Matrix4x4f*                 cachedPose;
	float*                      blendshapeWeights;

	int                         blendshapeCount;
	const BlendShapeData*       blendshapes;

	bool memExport; // Is set up for memexport (Xbox) or streamout (DX11)

#if UNITY_PS3
	const VertexData* vertexData;
#endif

	GPUSkinningInfo *mei;

	SkinMeshInfo();
	
	void Allocate();
	void Release () const;
};

void DeformSkinnedMesh (SkinMeshInfo& info);
void* DeformSkinnedMeshJob (void* rawData);

#endif
