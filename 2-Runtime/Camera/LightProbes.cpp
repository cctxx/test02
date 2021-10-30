#include "UnityPrefix.h"
#include "LightProbes.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Polynomials.h"
#if UNITY_EDITOR
#include "Editor/Src/LightmapperLightProbes.h"
#endif

template<class T>
void LightProbeCoefficients::Transfer (T& transfer)
{
	TRANSFER(sh[0]);
	TRANSFER(sh[1]);
	TRANSFER(sh[2]);
	TRANSFER(sh[3]);
	TRANSFER(sh[4]);
	TRANSFER(sh[5]);
	TRANSFER(sh[6]);
	TRANSFER(sh[7]);
	TRANSFER(sh[8]);
	TRANSFER(sh[9]);
	TRANSFER(sh[10]);
	TRANSFER(sh[11]);
	TRANSFER(sh[12]);
	TRANSFER(sh[13]);
	TRANSFER(sh[14]);
	TRANSFER(sh[15]);
	TRANSFER(sh[16]);
	TRANSFER(sh[17]);
	TRANSFER(sh[18]);
	TRANSFER(sh[19]);
	TRANSFER(sh[20]);
	TRANSFER(sh[21]);
	TRANSFER(sh[22]);
	TRANSFER(sh[23]);
	TRANSFER(sh[24]);
	TRANSFER(sh[25]);
	TRANSFER(sh[26]);
}

template<class T>
void Tetrahedron::Transfer (T& transfer)
{
	TRANSFER(indices[0]);
	TRANSFER(indices[1]);
	TRANSFER(indices[2]);
	TRANSFER(indices[3]);
	TRANSFER(neighbors[0]);
	TRANSFER(neighbors[1]);
	TRANSFER(neighbors[2]);
	TRANSFER(neighbors[3]);
	TRANSFER(matrix);
}

template<class T>
void LightProbes::Transfer (T& transfer)
{
	Super::Transfer(transfer);
	transfer.Transfer(m_Data.positions, "bakedPositions", kHideInEditorMask);
	transfer.Transfer(m_Data.coefficients, "bakedCoefficients");
	transfer.Transfer(m_Data.tetrahedra, "tetrahedra", kHideInEditorMask);
	transfer.Transfer(m_Data.hullRays, "hullRays", kHideInEditorMask);
}

LightProbes::LightProbes(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

LightProbes::~LightProbes ()
{
}

void LightProbes::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad(awakeMode);
}

#if UNITY_EDITOR
void LightProbes::SetBakedData( const dynamic_array<Vector3f>& positions, const dynamic_array<LightProbeCoefficients>& coefficents )
{
	m_Data.coefficients = coefficents;
	m_Data.positions = positions;
	LightProbeUtils::Tetrahedralize(m_Data);
	SetDirty();
}
#endif

LightProbes* GetLightProbes ()
{
	return GetLightmapSettings().GetLightProbes();
}

static inline float TriArea2D(float x1, float y1, float x2, float y2, float x3, float y3)
{
	return (x1-x2)*(y2-y3) - (x2-x3)*(y1-y2);
}

// Taken from Real-Time Collision Detection
static inline void BarycentricCoordinates3DTriangle(const Vector3f tri[3], const Vector3f& p, Vector4f& coords)
{
	// Unnormalized(!) triangle normal
	Vector3f normal = Cross(tri[1] - tri[0], tri[2] - tri[0]);
	// Nominators and one-over-denominator for u and v ratios
	float nu, nv, ood;
	// Absolute components for determining projection plane
	float x = Abs(normal.x), y = Abs(normal.y), z = Abs(normal.z);

	// Compute areas in plane of largest projection
	if (x >= y && x >= z) {
		// x is largest, project to the yz plane
		nu = TriArea2D(p.y, p.z, tri[1].y, tri[1].z, tri[2].y, tri[2].z); // Area of PBC in yz plane
		nv = TriArea2D(p.y, p.z, tri[2].y, tri[2].z, tri[0].y, tri[0].z); // Area of PCA in yz plane
		ood = 1.0f / normal.x;                             // 1/(2*area of ABC in yz plane)
	} else if (y >= x && y >= z) {
		// y is largest, project to the xz plane
		nu = TriArea2D(p.x, p.z, tri[1].x, tri[1].z, tri[2].x, tri[2].z);
		nv = TriArea2D(p.x, p.z, tri[2].x, tri[2].z, tri[0].x, tri[0].z);
		ood = 1.0f / -normal.y;
	} else {
		// z is largest, project to the xy plane
		nu = TriArea2D(p.x, p.y, tri[1].x, tri[1].y, tri[2].x, tri[2].y);
		nv = TriArea2D(p.x, p.y, tri[2].x, tri[2].y, tri[0].x, tri[0].y);
		ood = 1.0f / normal.z;
	}
	coords.x = nu * ood;
	coords.y = nv * ood;
	coords.z = 1.0f - coords.x - coords.y;
}

// Warped prism is an outer open cell (a.k.a. outer tetrahedron) formed by a tri face of the convex hull and
// 3 vertex normals. There are no additional constraints on the normals other than they have point in the
// half-space above the face, so the prism can be warped and the function will still project correctly.
// This is done by solving the equation
// P = a*(V0*t+P0) + b*(V1*t+P1) + (1-a-b)*(V2*t+P2)
// where V_ is the vertex normal, P_ the hull vertex, so V_*t+P_ is a hull ray for t >= 0
// P is the point we want to find the three barycentric coordinates for, so a, b and c=1-a-b
// If we substitute 
// A = P0 - P2, Ap = V0 - V2, B = P1 - P2, Bp = V1 - V2, C = P - P2, Cp = -V2,
// we can rewrite as:
// a*(A + t*Ap) + b*(B + t*Bp) = C + t*Cp
// or, as a matrix equation:
//   A.x + t*Ap.x, B.x + t*Bp.x, C.x + t*Cp.x     a       0
// [ A.y + t*Ap.y, B.y + t*Bp.y, C.y + t*Cp.y ] [ b ] = [ 0 ]
//   A.z + t*Ap.z, B.z + t*Bp.z, C.z + t*Cp.z    -1       0
// which only has a solution, if the determinant is 0, which gives a cubic in t.
// The geometrical interpretation shows that there should be exactly one positive root for that cubic
// and that's our t. (There might be two real negative roots too, but we don't care).
// Finally we can find a and b either by substituting t into the two equations, or plug t
// into the hull rays V_*t+P_ and use BarycentricCoordinates3DTriangle()
inline void GetBarycentricCoordinatesForOuterCell (const dynamic_array<Vector3f>& bakedPositions, const dynamic_array<Vector3f>& hullRays, const Vector3f& p, const Tetrahedron& tet, Vector4f& coords, float& t)
{
	// It's an outer tetrahedron;
	// Take the position relative to one of the triangle points (say, 0th) and dot it with normal
	const int (&ind)[4] = tet.indices;
	const Vector3f& v0 = bakedPositions[ind[0]];
	const Vector3f edge0 = bakedPositions[ind[1]] - v0;
	const Vector3f edge1 = bakedPositions[ind[2]] - v0;
	// We could cache the normal... or not.
	const Vector3f normal = Cross(edge1, edge0);
	t = Dot(p - v0, normal);
	if (t < 0)
	{
		// p is below the hull surface of this tetrahedron, so let's just return the 4th barycentric coordinate
		// as the lowest (and negative), so that the tetrahedron adjacent at the base gets tested next
		coords.Set(0, 0, 0, -1);
		return;
	}

	// CalculateOuterTetrahedraMatrices() prepares the Tetrahedron.matrix, so that
	// the coefficients of the cubic can be found just like that:
	Vector3f polyCoeffs = tet.matrix.MultiplyPoint3(p);
	// If the polynomial degenerated to quadratic, the unused ind[3] will be set to -2 instead of -1
	t = ind[3] == -1 ? CubicPolynomialRoot(polyCoeffs.x, polyCoeffs.y, polyCoeffs.z) : QuadraticPolynomialRoot(polyCoeffs.x, polyCoeffs.y, polyCoeffs.z);

	// We could directly calculate the barycentric coords by plugging t into
	// a*(A + t*Ap) + b*(B + t*Bp) = C + t*Cp, checking which coord to ignore
	// and using the two other equations, but it's actually almost the same
	// as using BarycentricCoordinates3DTriangle()

	Vector3f tri[3];
	tri[0] = bakedPositions[ind[0]] + hullRays[ind[0]]*t;
	tri[1] = bakedPositions[ind[1]] + hullRays[ind[1]]*t;
	tri[2] = bakedPositions[ind[2]] + hullRays[ind[2]]*t;
	BarycentricCoordinates3DTriangle(tri, p, coords);
	coords.w = 0;	
}

inline void GetBarycentricCoordinatesForInnerTetrahedron (const dynamic_array<Vector3f>& bakedPositions, const Vector3f& p, const Tetrahedron& tet, Vector4f& coords)
{
	// It's an inner tetrahedron, just use the precalculated matrix to find the barycentric coordinates
	Vector3f mult = tet.matrix.MultiplyVector3(p - bakedPositions[tet.indices[3]]);
	coords.x = mult.x;
	coords.y = mult.y;
	coords.z = mult.z;
	coords.w = 1.0f - mult.x - mult.y - mult.z;
}

void GetBarycentricCoordinates (const dynamic_array<Vector3f>& bakedPositions, const dynamic_array<Vector3f>& hullRays, const Vector3f& p, const Tetrahedron& tet, Vector4f& coords, float& t)
{
	if (tet.indices[3] >= 0)
		GetBarycentricCoordinatesForInnerTetrahedron (bakedPositions, p, tet, coords);
	else
		GetBarycentricCoordinatesForOuterCell (bakedPositions, hullRays, p, tet, coords, t);
}

void GetLightProbeInterpolationWeights (const LightProbeData& data, const Vector3f& position, int& tetIndex, Vector4f& weights, float& t, int& steps)
{
	// If we don't have an initial guess, always start from tetrahedron 0.
	// Tetrahedron 0 is picked to be roughly in the center of the probe cloud,
	// to minimize the number of steps to any other tetrahedron.
	const int tetCount = data.tetrahedra.size();
	if (tetIndex < 0 || tetIndex >= tetCount)
		tetIndex = 0;
	
	steps = 0;
	int prev = -1, prevprev = -1;
	for (; steps < tetCount; steps++)
	{
		// Check if we're in the current "best guess" tetrahedron
		const Tetrahedron& tet = data.tetrahedra[tetIndex];
		GetBarycentricCoordinates(data.positions, data.hullRays, position, tet, weights, t);
		if (weights.x >= 0.0f && weights.y >= 0.0f && weights.z >= 0.0f && weights.w >= 0.0f)
		{
			// Success!
			return;
		}


		// There's a chance the position lies "between" two tetrahedra, i.e. both return a slightly negative weight
		// due to numerical errors and we ping-pong between them.
		if (tetIndex == prevprev)
			return;

		prevprev = prev;
		prev = tetIndex;

		// Otherwise find the smallest barycentric coord and move in that direction
		if (weights.x < weights.y && weights.x < weights.z && weights.x < weights.w)
			tetIndex = tet.neighbors[0];
		else if (weights.y < weights.z && weights.y < weights.w)
			tetIndex = tet.neighbors[1];
		else if (weights.z < weights.w)
			tetIndex = tet.neighbors[2];
		else
			tetIndex = tet.neighbors[3];
	}
}

void InterpolateLightProbeCoefficients(const dynamic_array<LightProbeCoefficients>& bakedCoefficients, const Tetrahedron& tet, const Vector4f& weights, float* coefficients)
{
	// Outer tetrahedra don't have a fourth probe
	int probeCount = tet.indices[3] < 0 ? 3 : 4;

	for (int i = 0; i < probeCount; i++)
	{
		int probeIndex = tet.indices[i];
		float probeWeight = weights[i];
		for (int j = 0; j < kLightProbeCoefficientCount; j++)
		{
			coefficients[j] += bakedCoefficients[probeIndex].sh[j] * probeWeight;
		}
	}
}

void LightProbes::GetInterpolatedLightProbe (const Vector3f& position, Renderer* renderer, float* coefficients, int& tetIndex, Vector4f& weights, float& t)
{
	// Init to black
	memset (coefficients, 0, sizeof(float)*kLightProbeCoefficientCount);

	// If there are no probes baked
	const int tetCount = m_Data.tetrahedra.size();
	if (tetCount == 0)
	{
		tetIndex = -1;
		weights.Set(0,0,0,0);
		return;
	}

	// Use tetIndex as an initial guess; if it's not set, check for it in the renderer
	if ((tetIndex < 0  || tetIndex >= tetCount) && renderer != NULL)
		tetIndex = renderer->GetLastLightProbeTetIndex();
	
	int steps;
	GetLightProbeInterpolationWeights(m_Data, position, tetIndex, weights, t, steps);

	// Return black if we're not within any tetrahedron. It should never happen.
	if (tetIndex < 0 || tetIndex >= tetCount)
		return;

	// Tetrahedron found, set it's index to the renderer, to be used as a good guess for the next frame
	if (renderer)
		renderer->SetLastLightProbeTetIndex(tetIndex);

	// Use the weights to actually interpolate the probes and get the coefficients
	InterpolateLightProbeCoefficients(m_Data.coefficients, m_Data.tetrahedra[tetIndex], weights, coefficients);
}

void LightProbes::SetCoefficients( float* data, int size )
{
	int count = m_Data.positions.size();
	if ( count * kLightProbeCoefficientCount != size || !data)
	{
		ErrorString(Format("Number of coefficient sets (%i) has to be equal to current light probe count (%i).", size/kLightProbeCoefficientCount, count));
		return;
	}
	LightProbeCoefficients* lpData = reinterpret_cast<LightProbeCoefficients*>(data);
	m_Data.coefficients.assign(lpData, lpData + count);
	SetDirty();
}

IMPLEMENT_CLASS_HAS_INIT (LightProbes)
IMPLEMENT_OBJECT_SERIALIZE (LightProbes)

Matrix3x4f::Matrix3x4f( const Matrix3x3f& other )
{
	memcpy(&m_Data[0], &other.m_Data[0], sizeof(Matrix3x3f));
	m_Data[9] = m_Data[10] = m_Data[11] = 0.0f;
}

inline Vector3f Matrix3x4f::MultiplyVector3( const Vector3f& v ) const
{
	Vector3f res;
	res.x = m_Data[0] * v.x + m_Data[3] * v.y + m_Data[6] * v.z;
	res.y = m_Data[1] * v.x + m_Data[4] * v.y + m_Data[7] * v.z;
	res.z = m_Data[2] * v.x + m_Data[5] * v.y + m_Data[8] * v.z;
	return res;
}

inline Vector3f Matrix3x4f::MultiplyPoint3( const Vector3f& v ) const
{
	Vector3f res;
	res.x = m_Data[0] * v.x + m_Data[3] * v.y + m_Data[6] * v.z + m_Data[9];
	res.y = m_Data[1] * v.x + m_Data[4] * v.y + m_Data[7] * v.z + m_Data[10];
	res.z = m_Data[2] * v.x + m_Data[5] * v.y + m_Data[8] * v.z + m_Data[11];
	return res;
}

template<class TransferFunction> inline
void Matrix3x4f::Transfer (TransferFunction& t)
{
	t.Transfer (m_Data[0], "e00"); t.Transfer (m_Data[3], "e01"); t.Transfer (m_Data[6], "e02"); t.Transfer (m_Data[ 9], "e03");
	t.Transfer (m_Data[1], "e10"); t.Transfer (m_Data[4], "e11"); t.Transfer (m_Data[7], "e12"); t.Transfer (m_Data[10], "e13");
	t.Transfer (m_Data[2], "e20"); t.Transfer (m_Data[5], "e21"); t.Transfer (m_Data[8], "e22"); t.Transfer (m_Data[11], "e23");
}
