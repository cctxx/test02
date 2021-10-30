#pragma once

#include "Runtime/Camera/LightProbes.h"

#define LIGHT_PROBE_ASSET_NAME "LightProbes.asset"
namespace LightProbeUtils
{
	// Tetrahedralizes output from the lightmapper, so assumes there are no duplicates in the input positions (RemoveDuplicateLightProbePositions has been used)
	void Tetrahedralize(LightProbeData& data);
	// Tetrahedralizes arbitrary user input, so removes duplicates and puts the new positions into outPositions
	void Tetrahedralize(const Vector3f* positions, const int positionCount, int** tetrahedra, int* tetrahedraCount, Vector3f** outPositions, int* outPositionCount);
	void RemoveDuplicateLightProbePositions(const Vector3f* inPositions, const int inPositionCount, dynamic_array<Vector3f>& outPositions);
	void Clear();
}
