#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_UNIT_TESTS && UNITY_EDITOR

#include "External/UnitTest++/src/UnitTest++.h"
#include "Editor/Src/LightmapperLightProbes.h"

void GetLightProbeInterpolationWeights (const LightProbeData& data, const Vector3f& position, int& tetIndex, Vector4f& weights, float& t, int& steps);
SUITE (LightProbeTests)
{
TEST (LightProbe_AllFeatures)
{
	// Input positions for probes
	LightProbeData data;
	data.positions.resize_uninitialized(8);
	data.positions[0] = Vector3f (-2.76f, 0.98f, -3.18f);
	data.positions[1] = Vector3f ( 3.23f, 0.98f, -3.18f);
	data.positions[2] = Vector3f ( 3.23f, 0.98f,  2.21f);
	data.positions[3] = Vector3f (-2.76f, 0.98f,  2.76f);
	data.positions[4] = Vector3f (-4.36f, 4.60f, -0.23f);
	data.positions[5] = Vector3f ( 4.30f, 4.60f, -0.23f);
	data.positions[6] = Vector3f ( 0.88f, 3.93f,  3.68f);
	data.positions[7] = Vector3f ( 0.88f, 3.93f, -5.81f);

	{
	// Verify tetrahedralization
	LightProbeUtils::Tetrahedralize(data);

	const int tetIndices[20][4] =  {{1, 5, 0, 6},
									{2, 5, 1, 6},
									{0, 2, 1, 6},
									{4, 3, 0, 6},
									{0, 5, 4, 6},
									{1, 7, 0, 5},
									{0, 7, 4, 5},
									{3, 2, 0, 6},
									{3, 2, 0, -1},
									{3, 6, 2, -1},
									{2, 5, 1, -1},
									{2, 6, 5, -1},
									{0, 2, 1, -1},
									{4, 3, 0, -1},
									{4, 6, 3, -1},
									{5, 6, 4, -1},
									{1, 7, 0, -1},
									{1, 5, 7, -1},
									{0, 7, 4, -1},
									{7, 5, 4, -1}};

	const int tetCount = data.tetrahedra.size();
	CHECK_EQUAL (20, tetCount);
	for (int i = 0; i < tetCount; i++)
		for (int j = 0; j < 4; j++)
			CHECK_EQUAL (tetIndices[i][j], data.tetrahedra[i].indices[j]);

	// Verify neighbor connections

	const int tetNeighbors[20][4] ={{4, 2, 1, 5},
									{0, 2, 11, 10},
									{1, 0, 7, 12},
									{7, 4, 14, 13},
									{15, 3, 0, 6},
									{6, 0, 17, 16},
									{19, 4, 5, 18},
									{2, 3, 9, 8},
									{12, 13, 9, 7},
									{11, 8, 14, 7},
									{17, 12, 11, 1},
									{15, 10, 9, 1},
									{10, 16, 8, 2},
									{8, 18, 14, 3},
									{9, 13, 15, 3},
									{14, 19, 11, 4},
									{18, 12, 17, 5},
									{19, 16, 10, 5},
									{19, 13, 16, 6},
									{15, 18, 17, 6}};

	for (int i = 0; i < tetCount; i++)
		for (int j = 0; j < 4; j++)
			CHECK_EQUAL (tetNeighbors[i][j], data.tetrahedra[i].neighbors[j]);

	// Check interpolation weights
	const int testPositionCount = 8;
	const float testPositions[testPositionCount][3] =  {{-3.0f, 3.0f, 3.0f},
														{1.4858840f, 2.0687418f, -1.0313016f},
														{0.56982428f, 2.7362425f, 0.0036439002f},
														{-0.33467144f, -0.21997118f, -0.45071134f},
														{6.112422f, 2.92597f, -0.1436141f},
														{6.112422f, 2.92597f, -0.1436141f},
														{6.112422f, 2.92597f, -0.1436141f},
														{6.112422f, 2.92597f, -0.1436141f}};
	const float testWeights [testPositionCount][4] =   {{0.3010f, 0.2757f, 0.4232f, 0.0000f},
														{0.4542f, 0.0701f, 0.1926f, 0.2831f},
														{0.1228f, 0.1647f, 0.3192f, 0.3933f},
														{0.0670f, 0.4303f, 0.5027f, 0.0000f},
														{0.2466f, 0.5211f, 0.2324f, 0.0000f},
														{0.2466f, 0.5211f, 0.2324f, 0.0000f},
														{0.2466f, 0.5211f, 0.2324f, 0.0000f},
														{0.2466f, 0.5211f, 0.2324f, 0.0000f}};
	const int testInputTetIndices[testPositionCount] = {6, -1, 9, 3, 13, 17, 12, -1};
	const int testTetIndices[testPositionCount] = {14, 0, 0, 8, 10, 10, 10, 10};
	// In both the 6th and 7th test start tetrahedra (17 and 12) are adjacent to the target one (10),
	// but the point lies below the base surface of the 12 tet, so we can't calc barycentric coordinates
	// there, and we have to walk through some inner tetrahedra, so it's 3 steps instead of 1
	const int testSteps[testPositionCount] = {3, 0, 3, 2, 5, 1, 3, 2};
	// Note, this tolerance only works if the floats being compared are small
	const float tolerance = 0.001f;
	
	for (int i = 0; i < testPositionCount; i++)
	{
		Vector3f position(testPositions[i][0], testPositions[i][1], testPositions[i][2]);
		Vector4f weights;
		float t;
		int steps;
		int tetIndex = testInputTetIndices[i];
		GetLightProbeInterpolationWeights(data, position, tetIndex, weights, t, steps);

		CHECK_CLOSE(testWeights[i][0], weights[0], tolerance);
		CHECK_CLOSE(testWeights[i][1], weights[1], tolerance);
		CHECK_CLOSE(testWeights[i][2], weights[2], tolerance);
		CHECK_CLOSE(testWeights[i][3], weights[3], tolerance);

		CHECK_EQUAL(testTetIndices[i], tetIndex);
		CHECK_EQUAL(testSteps[i], steps);
	}

	// TODO: test the case when an outer tetrahedron stores a matrix for quadratic, not cubic coeffs
	// TODO: test actual coefficient interpolation? but that's simple
	// TODO: test if Renderers get their last tetrahedron index cached properly? but that's not stateless anymore
	}

	{
	// Test the Tetrahedralization method exposed in the API. It should correctly handle duplicates, so add a few
	dynamic_array<Vector3f> duplicatedPositions;
	duplicatedPositions = data.positions;
	duplicatedPositions.insert(duplicatedPositions.begin() + 1, Vector3f (-2.76f, 0.98f, -3.23f));
	duplicatedPositions.push_back(Vector3f (0.88f, 3.93f,  3.68f));
	int* indices;
	int indexCount;
	Vector3f* newPositions;
	int newPositionCount;
	LightProbeUtils::Tetrahedralize(&duplicatedPositions[0], duplicatedPositions.size(), &indices, &indexCount, &newPositions, &newPositionCount);

	const int testTetrahedraCount = 8;
	// This is slightly different than the original set of indices, because the 0th tet
	// position hasn't been swapped
	const int testIndices[testTetrahedraCount][4] = {	{3, 2, 0, 6},
														{2, 5, 1, 6},
														{0, 2, 1, 6},
														{4, 3, 0, 6},
														{0, 5, 4, 6},
														{1, 7, 0, 5},
														{0, 7, 4, 5},
														{1, 5, 0, 6}};

	
	CHECK_EQUAL(testTetrahedraCount*4, indexCount);
	CHECK_EQUAL(data.positions.size(), newPositionCount);
	CHECK(indices != NULL);
	CHECK(newPositions != NULL);
	for (int i = 0; i < testTetrahedraCount; i++)
		for (int j = 0; j < 4; j++)
			CHECK_EQUAL (testIndices[i][j], indices[i*4 + j]);

	for (int i = 0; i < newPositionCount; i++)
		CHECK(data.positions[i] == newPositions[i]);

	delete indices;
	delete newPositions;
	}
}
}

#endif
