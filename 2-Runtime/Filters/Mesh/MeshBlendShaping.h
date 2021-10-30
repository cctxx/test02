#pragma once

struct SkinMeshInfo;

// Does "mesh skinning" logic for BlendShapes
void ApplyBlendShapes (SkinMeshInfo& info, UInt8* dst);

inline bool HasValidWeight(const float w)
{
	const float kWeightEpsilon = 1e-4f;
	return w > kWeightEpsilon;
}
