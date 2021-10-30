#include "UnityPrefix.h"
#include "Editor/Src/LightmapperLightProbes.h"
#include "Editor/Src/tetgen.h"
#include "Runtime/Math/Quaternion.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Runtime/Graphics/LightmapSettings.h"

using namespace LightProbeUtils;

void InitTetrahedra(LightProbeData& data, int innerTetrahedraCount);
void OptimizeSearchStartTetrahedron(LightProbeData& data);
void CalculateOuterTetrahedraMatrices(LightProbeData& data, int innerTetrahedraCount);

void LightProbeUtils::Tetrahedralize(LightProbeData& data)
{
	dynamic_array<Tetrahedron>& tetrahedra = data.tetrahedra;
	tetgenio in, out;

	int positionCount = data.positions.size();
	if (positionCount < 4)
	{
		data.tetrahedra.clear();
		data.hullRays.clear();
		return;
	}

	in.numberofpoints = positionCount;
	in.pointlist = new REAL[positionCount * 3];
	// REAL is defined as double, so we can't memcpy;
	// TODO: define REAL as float, but tetgen is screwed up and needs some fixes before it even compiles for floats
	for(int i = 0; i < positionCount; i++)
	{
		in.pointlist[i*3 + 0] = data.positions[i].x;
		in.pointlist[i*3 + 1] = data.positions[i].y;
		in.pointlist[i*3 + 2] = data.positions[i].z;
	}

	tetgenbehavior options;
	options.neighout = 2;
	// too small sets for the btree to make a difference
	options.btree = 0;
	// don't print anything
	options.quiet = 1;
	tetrahedralize(&options, &in, &out);

	if (out.numberofcorners != 4)
	{
		WarningString("Oops, something went wrong with the light probe tetrahedralization");
		tetrahedra.clear();
		return;
	}

	const int innerTetrahedraCount = out.numberoftetrahedra;
	const int outerTetrahedraCount = out.numberoftrifaces;
	tetrahedra.resize_uninitialized(innerTetrahedraCount + outerTetrahedraCount);
	for (int i = 0; i < innerTetrahedraCount; i++)
		memcpy(&tetrahedra[i].indices[0], &out.tetrahedronlist[i*4], 4 * sizeof(int));

	for (int i = 0; i < innerTetrahedraCount; i++)
		memcpy(&(tetrahedra[i].neighbors[0]), &out.neighborlist[i*4], 4 * sizeof(int));

	for(int i = 0; i < outerTetrahedraCount; i++)
	{
		int tet = i + innerTetrahedraCount;
		tetrahedra[tet].indices[0] = out.trifacelist[i*3 + 0];
		tetrahedra[tet].indices[1] = out.trifacelist[i*3 + 1];
		tetrahedra[tet].indices[2] = out.trifacelist[i*3 + 2];
		tetrahedra[tet].indices[3] = -1;

		int baseTet = out.adjtetlist[i*2];
		// Originally TetGen wasn't outputting the adjacent tetrahedron for the hull faces;
		// I fixed that, make sure it doesn't break with a TetGen update:
		Assert(baseTet >= 0);
		// Set the ref at index 3, because that's where the -1 vertex index is (the one NOT shared with the base tet)
		tetrahedra[tet].neighbors[3] = baseTet;
		
		// Set the connection the other way round as well
		// Find the first unassigned neighbor index
		int n;
		for(n = 0; n < 4; n++)
		{
			if (tetrahedra[baseTet].neighbors[n] < 0)
			{
				tetrahedra[baseTet].neighbors[n] = tet;
				break;
			}
		}
		// If we didn't find an unassigned neighbor index in our base tetrahedron, something is wrong
		Assert(n < 4);

		// Initialize other neighbors of this open tetrahedron to -1; they will be filled in in InitTetrahedra()
		tetrahedra[tet].neighbors[0] = tetrahedra[tet].neighbors[1] = tetrahedra[tet].neighbors[2] = -1;
	}

	InitTetrahedra(data, innerTetrahedraCount);
	OptimizeSearchStartTetrahedron(data);
}

void InitTetrahedra(LightProbeData& data, int innerTetrahedraCount)
{
	// We want to be able to quickly calculate barycentric coordinates within each tetrahedron.
	// One way to do this for point P and tetrahedron ABCD and coordinates a, b, c, d:
	// |a b c| = (A - D | B - D | C - D)^-1 * (P - D)
	// d = 1 - a - b - c
	// For now let's cache the inverted (A - D | B - D | C - D)^-1 matrix and see how it works out.

	dynamic_array<Tetrahedron>& tetrahedra = data.tetrahedra;
	dynamic_array<Vector3f>& positions = data.positions;

	for (int i = 0; i < innerTetrahedraCount; i++)
	{
		Vector3f& D = positions[tetrahedra[i].indices[3]];
		Vector3f C0 = positions[tetrahedra[i].indices[0]] - D;
		Vector3f C1 = positions[tetrahedra[i].indices[1]] - D;
		Vector3f C2 = positions[tetrahedra[i].indices[2]] - D;

		Matrix3x3f m;
		m[0] = C0.x;
		m[1] = C0.y;
		m[2] = C0.z;
		m[3] = C1.x;
		m[4] = C1.y;
		m[5] = C1.z;
		m[6] = C2.x;
		m[7] = C2.y;
		m[8] = C2.z;
		m.Invert();
		tetrahedra[i].matrix = m;
	}
	
	// All the neighbor connections are now in place except connections between outer tetrahedra; fill those in
	for(int i = innerTetrahedraCount; i < tetrahedra.size(); i++)
	{
		Tetrahedron& t = tetrahedra[i];
		// Loop through all the other outer tetrahedra and find the ones sharing 0th and 1st, 1st and 2n or 2nd and 0th vertex with tetrahedron t;
		// put them as neighbors number 2, 0 and 1 (3 is taken by the base/inner tetrahedron), so that the index, at which the neighbor is at
		// corresponds to the index of the vertex that is NOT shared.
		for(int k = 0; k < 3; k++)
		{
			int v0 = t.indices[k];
			int v1 = t.indices[(k+1)%3];
			for(int j = innerTetrahedraCount; j < tetrahedra.size(); j++)
			{
				if (j == i)
					continue;
				Tetrahedron& adjt = tetrahedra[j];
				if ( (v0==adjt.indices[0] || v0==adjt.indices[1] || v0==adjt.indices[2]) && 
					(v1==adjt.indices[0] || v1==adjt.indices[1] || v1==adjt.indices[2]))
				{
					// adjt shares two vertices with t and is also an outer tetrahedron, so it's adjacent;
					// write it down at neighbor index k + 1;
					// (we are writing down the connection only one way round - the other direction will come from
					// searching through all the tetrahedra when i == j; this can be sped up by writing down the connection
					// for adjt as well, but I can't be bothered with it now ;)
					t.neighbors[(k+2)%3] = j;
					break;
				}
			}
		}
	}

	// Inner tetrahedra still have indices in incorrect order. Neighbor at position n should be the one
	// that doesn't share the vertex at position n
	// TODO: maybe fix that in TetGen?
	for (int i = 0; i < innerTetrahedraCount; i++)
	{
		Tetrahedron& t = tetrahedra[i];
		int neighbors[4];
		for (int j = 0; j < 4; j++)
			neighbors[j] = t.neighbors[j];

		for (int j = 0; j < 4; j++)
		{
			const Tetrahedron& neighbor = tetrahedra[neighbors[j]];
			for (int k = 0; k < 4; k++)
			{
				const int v = t.indices[k];
				if (v != neighbor.indices[0] && v != neighbor.indices[1] && v != neighbor.indices[2] && v != neighbor.indices[3])
				{
					t.neighbors[k] = neighbors[j];
					break;
				}
			}
		}
	}

	dynamic_array<Vector3f> hullFaceNormals;
	hullFaceNormals.resize_uninitialized(tetrahedra.size() - innerTetrahedraCount);
	// For every outer tetrahedron, calculate the hull face normal
	for (int i = innerTetrahedraCount; i < tetrahedra.size(); i++)
	{
		Vector3f& v = positions[tetrahedra[i].indices[0]];
		Vector3f v0 = positions[tetrahedra[i].indices[1]] - v;
		Vector3f v1 = positions[tetrahedra[i].indices[2]] - v;

		hullFaceNormals[i - innerTetrahedraCount] = Normalize(Cross(v1, v0));
	}

	// Calculate 'vertex' normals, i.e. direction vectors for the hull probe positions as 
	// averages of the hull face normals (a.k.a. outer tetrahedra normals)
	data.hullRays.resize_uninitialized(positions.size());
	for (int i = 0; i < positions.size(); i++)
	{
		// Accumulate normals from all the outer tetrahedra that are using this vertex
		Vector3f ray(0,0,0);
		for (int j = innerTetrahedraCount; j < tetrahedra.size(); j++)
		{
			Tetrahedron& t = tetrahedra[j];
			if (t.indices[0] == i || t.indices[1] == i || t.indices[2] == i)
			{
				// To get a bit nicer vertex normals, weigh each face normal by the angle adjacent to the vertex.
				// Even with FixHullRayDirections() run afterwards, it's still better to get a better starting point here
				// for fewer iterations/lower error.
				float weight = 1;
				for(int k = 0; k < 3; k++)
				{
					if (t.indices[k] == i)
					{
						Vector3f v0 = Normalize(positions[t.indices[(k+1)%3]] - positions[t.indices[k]]);
						Vector3f v1 = Normalize(positions[t.indices[(k+2)%3]] - positions[t.indices[k]]);
						weight = acos(clamp(Dot(v0, v1), -1.0f, 1.0f));
					}
				}
				// first 3 floats in the matrix are the normal in case of the outer tetrahedra
				ray += hullFaceNormals[j-innerTetrahedraCount]*weight;
			}
		}
		// if the ray is zero, the current vertex is not a hull vertex; write it anyway
		if (ray != Vector3f::zero)
			ray = Normalize(ray);
		data.hullRays[i] = ray;
	}

	CalculateOuterTetrahedraMatrices(data, innerTetrahedraCount);
}

void GetLightProbeInterpolationWeights (const LightProbeData& data, const Vector3f& position, int& tetIndex, Vector4f& weights, float& t, int& steps);

void OptimizeSearchStartTetrahedron(LightProbeData& data)
{
	// To find the best candidate for the search start tetrahedron, as a heuristic,
	// find which tetrahedron contains the average position of all the probes.
	// TODO: possibly a slightly better approach is to find the tetrahedron, which
	// minimizes the maximum number of steps to reach any other tetrahedron from,
	// but that takes much more time to find.
	Vector3f averagePos(0,0,0);
	const int posCount = data.positions.size();
	if (posCount < 4)
		return;
	
	for (int i = 0; i < posCount; i++)
		averagePos += data.positions[i];
	averagePos /= posCount;

	int centerTet = -1;
	Vector4f weights;
	float t;
	int steps;
	GetLightProbeInterpolationWeights(data, averagePos, centerTet, weights, t, steps);
	if (centerTet == 0)
		// Nothing to do
		return;

	// Swap tetrahedron 0 with tetrahedron centerTet
	Tetrahedron temp = data.tetrahedra[0];
	data.tetrahedra[0] = data.tetrahedra[centerTet];
	data.tetrahedra[centerTet] = temp;
	// Swap any references to 0 and centerTet
	const int tetCount = data.tetrahedra.size();
	for (int i = 0; i < tetCount; i++)
	{
		Tetrahedron& tet = data.tetrahedra[i];
		for (int j = 0; j < 4; j++)
		{
			if (tet.neighbors[j] == 0)
				tet.neighbors[j] = centerTet;
			else if (tet.neighbors[j] == centerTet)
				tet.neighbors[j] = 0;
		}
	}
}

// This precalculates matrix M, which allows for calculating the coefficients of the cubic polynomial for t:
// t^3 + coeffs.x*t^2 + coeffs.y*t + coeffs.z
// by just doing a matrix mult at runtime:
// coeffs = M.MultiplyPoint3(position)
// The cubic is actually det(T) = 0, where T:
//       A.x + t*Ap.x, B.x + t*Bp.x, C.x + t*Cp.x  
// T = [ A.y + t*Ap.y, B.y + t*Bp.y, C.y + t*Cp.y ]
//       A.z + t*Ap.z, B.z + t*Bp.z, C.z + t*Cp.z  
// A = P0 - P2, Ap = V0 - V2, B = P1 - P2, Bp = V1 - V2, C = P - P2, Cp = -V2
// Px are hull vertices, Vx are hull rays, P is the current object's position.
// det(T) = 0 gives
//
//float b =
//	+Ap.y*Bp.z*C.x
//	-Ap.z*Bp.y*C.x
//
//	-Ap.x*Bp.z*C.y
//	+Ap.z*Bp.x*C.y
//
//	+Ap.x*Bp.y*C.z
//	-Ap.y*Bp.x*C.z
//
//	+A.x*Bp.y*Cp.z
//	-A.y*Bp.x*Cp.z
//	+Ap.x*B.y*Cp.z
//	-Ap.y*B.x*Cp.z
//	+A.z*Bp.x*Cp.y
//	-A.z*Bp.y*Cp.x
//	+Ap.z*B.x*Cp.y
//	-Ap.z*B.y*Cp.x
//	-A.x*Bp.z*Cp.y
//	+A.y*Bp.z*Cp.x
//	-Ap.x*B.z*Cp.y
//	+Ap.y*B.z*Cp.x;
//
//float c =	 
//	+Ap.y*B.z*C.x
//	+A.y*Bp.z*C.x
//	-Ap.z*B.y*C.x
//	-A.z*Bp.y*C.x
//
//	-A.x*Bp.z*C.y
//	-Ap.x*B.z*C.y
//	+A.z*Bp.x*C.y
//	+Ap.z*B.x*C.y
//
//	+A.x*Bp.y*C.z
//	-A.y*Bp.x*C.z
//	+Ap.x*B.y*C.z
//	-Ap.y*B.x*C.z
//	
//	+A.x*B.y*Cp.z
//	-A.y*B.x*Cp.z
//	-A.x*B.z*Cp.y
//	+A.y*B.z*Cp.x
//	+A.z*B.x*Cp.y
//	-A.z*B.y*Cp.x;

//float d =	 
//	-A.z*B.y*C.x
//	+A.y*B.z*C.x
//	
//	-A.x*B.z*C.y
//	+A.z*B.x*C.y
//
//	+A.x*B.y*C.z
//	-A.y*B.x*C.z;

//float a =	 
//	Ap.x*Bp.y*Cp.z
//	-Ap.y*Bp.x*Cp.z
//	+Ap.z*Bp.x*Cp.y
//	-Ap.z*Bp.y*Cp.x
//	+Ap.y*Bp.z*Cp.x
//	-Ap.x*Bp.z*Cp.y;

//coeffs.x = b/a;
//coeffs.y = c/a;
//coeffs.z = d/a;
//
// And all that can be squeezed into a 3x4 matrix, so that the input is just C,
// or even better: P, as C = P - P2
void CalculateOuterTetrahedraMatrices(LightProbeData& data, int innerTetrahedraCount)
{
	for (int i = innerTetrahedraCount; i < data.tetrahedra.size(); i++)
	{
		Vector3f V[3];
		for (int j = 0; j < 3; j++)
			V[j] = data.hullRays[data.tetrahedra[i].indices[j]];

		Vector3f P[3];
		for (int j = 0; j < 3; j++)
			P[j] = data.positions[data.tetrahedra[i].indices[j]];

		Vector3f A = P[0] - P[2];
		Vector3f Ap = V[0] - V[2];
		Vector3f B = P[1] - P[2];
		Vector3f Bp = V[1] - V[2];
		//Vector3f C = p - P[2];
		Vector3f P2 = P[2];
		Vector3f Cp = -V[2];

		Matrix3x4f& m = data.tetrahedra[i].matrix;

		// output.x = 
		// input.x*
		m[0] =	Ap.y*Bp.z
				-Ap.z*Bp.y;
		// input.y*
		m[3] =	-Ap.x*Bp.z
				+Ap.z*Bp.x;
		// input.z*
		m[6] =	+Ap.x*Bp.y
				-Ap.y*Bp.x;
		// 1*
		m[9] = +A.x*Bp.y*Cp.z
				-A.y*Bp.x*Cp.z
				+Ap.x*B.y*Cp.z
				-Ap.y*B.x*Cp.z
				+A.z*Bp.x*Cp.y
				-A.z*Bp.y*Cp.x
				+Ap.z*B.x*Cp.y
				-Ap.z*B.y*Cp.x
				-A.x*Bp.z*Cp.y
				+A.y*Bp.z*Cp.x
				-Ap.x*B.z*Cp.y
				+Ap.y*B.z*Cp.x;

		m[9] -= P2.x*m[0] + P2.y*m[3] + P2.z*m[6];

		// output.y = 
		// input.x*
		m[1] =	+Ap.y*B.z
				+A.y*Bp.z
				-Ap.z*B.y
				-A.z*Bp.y;
		// input.y*
		m[4] =	-A.x*Bp.z
				-Ap.x*B.z
				+A.z*Bp.x
				+Ap.z*B.x;
		// input.z*
		m[7] =	+A.x*Bp.y
				-A.y*Bp.x
				+Ap.x*B.y
				-Ap.y*B.x;
		// 1*
		m[10] =	+A.x*B.y*Cp.z
				-A.y*B.x*Cp.z
				-A.x*B.z*Cp.y
				+A.y*B.z*Cp.x
				+A.z*B.x*Cp.y
				-A.z*B.y*Cp.x;

		m[10] -= P2.x*m[1] + P2.y*m[4] + P2.z*m[7];

		// output.z = 
		// input.x*
		m[2] =	-A.z*B.y
				+A.y*B.z;
		// input.y*
		m[5] =	-A.x*B.z
				+A.z*B.x;
		// input.z*
		m[8] =	+A.x*B.y
				-A.y*B.x;
		// 1*
		m[11] = 0.0f;

		m[11] -= P2.x*m[2] + P2.y*m[5] + P2.z*m[8];

		float a =	 
			Ap.x*Bp.y*Cp.z
			-Ap.y*Bp.x*Cp.z
			+Ap.z*Bp.x*Cp.y
			-Ap.z*Bp.y*Cp.x
			+Ap.y*Bp.z*Cp.x
			-Ap.x*Bp.z*Cp.y;

		if (Abs(a) > 0.00001f)
		{
			// d is not zero, so the polymial at^3 + bt^2 + ct + d = 0 is actually cubic
			// and we can simplify to the monic form t^3 + pt^2 + qt + r = 0
			for (int k = 0; k < 12; k++)
				m[k] /= a;
		}
		else
		{
			// It's actually a quadratic or even linear equation,
			// Set the last vertex index of the outer cell to -2
			// instead of -1, so at runtime we know the equation is
			// pt^2 + qt + r = 0 and not t^3 + pt^2 + qt + r = 0
			data.tetrahedra[i].indices[3] = -2;
		}
	}
}

void LightProbeUtils::Tetrahedralize(const Vector3f* inPositions, const int inPositionCount, int** tetrahedra, int* tetrahedraIndexCount, Vector3f** outPositions, int* outPositionCount)
{
	// TODO: remove code duplication between this and Tetrahedralize(LightProbeData& data)

	dynamic_array<Vector3f> positions;
	RemoveDuplicateLightProbePositions(inPositions, inPositionCount, positions);
	const int positionCount = positions.size();

	if (positionCount < 4)
	{
		*tetrahedra = NULL;
		*tetrahedraIndexCount = 0;
		*outPositions = NULL;
		*outPositionCount = positionCount;
		if (positionCount > 0)
		{
			*outPositions = new Vector3f[positionCount];
			memcpy(*outPositions, &positions[0], sizeof(Vector3f)*positionCount);
		}
		return;
	}
	
	tetgenio in, out;
	in.numberofpoints = positionCount;

	// TODO: get rid of this stuff, make TetGen operate on floats
	in.pointlist = new REAL[positionCount * 3];
	for(int i = 0; i < positionCount; i++)
	{
		in.pointlist[i*3 + 0] = positions[i].x;
		in.pointlist[i*3 + 1] = positions[i].y;
		in.pointlist[i*3 + 2] = positions[i].z;
	}

	tetgenbehavior options;
	// too small sets for the btree to make a difference
	options.btree = 0;
	// don't print anything
	options.quiet = 1;
	tetrahedralize(&options, &in, &out);
	Assert(out.numberofcorners == 4);

	*tetrahedraIndexCount = out.numberoftetrahedra * out.numberofcorners;
	*tetrahedra = new int[*tetrahedraIndexCount];
	memcpy(*tetrahedra, out.tetrahedronlist, *tetrahedraIndexCount * sizeof(int));
	*outPositionCount = positionCount;
	// Need to copy the positions, so that they don't get deallocated with the dynamic_array
	*outPositions = new Vector3f[positionCount];
	memcpy(*outPositions, &positions[0], positionCount * sizeof(Vector3f));
}

void LightProbeUtils::RemoveDuplicateLightProbePositions(const Vector3f* inPositions, const int inPositionCount, dynamic_array<Vector3f>& outPositions)
{	
	const float kDuplicateEpsilonSq = 0.1f;

	outPositions.clear();
	for (int i = 0; i < inPositionCount; i++)
	{
		int j = 0;
		const Vector3f& v = inPositions[i];
		for (; j < i; j++)
		{
			if (SqrMagnitude(v - inPositions[j]) < kDuplicateEpsilonSq)
				break;
		}

		if (j == i)
			// A position like that hasn't been added yet - safe to add
			outPositions.push_back(v);
	}
}

void LightProbeUtils::Clear ()
{
	LightProbes* lightProbes = GetLightmapSettings().GetLightProbes();
	string assetPath = GetAssetPathFromObject(lightProbes);
	// Destroy the object only if it's NOT an asset
	if (assetPath.empty())
		DestroySingleObject (lightProbes);

	// Delete the asset at the default location.
	// Do NOT delete the asset that's referenced by the scene.
	string folderPath = GetSceneBakedAssetsPath();
	if (folderPath == "")
		return;

	string defaultAssetPath = AppendPathName(folderPath, LIGHT_PROBE_ASSET_NAME);
	if (IsFileCreated(defaultAssetPath))
		AssetInterface::Get().DeleteAsset(defaultAssetPath);

	GetLightmapSettings().SetLightProbes(NULL);
}
