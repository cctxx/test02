#pragma once

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Matrix3x3.h"

class Renderer;

// column-major
// m0   m3   m6   m9	
// m1   m4   m7   m10
// m2   m5   m8   m11
// So far only used for light probes, move it to Matrix3x4f.h if it ever grows beyond that
class Matrix3x4f
{
public:
	float m_Data[12];
	///@todo: Can't be Transfer optimized because Transfer doesn't write the same as memory layout
	DECLARE_SERIALIZE_NO_PPTR (Matrix3x4f)
	Matrix3x4f () {}
	Matrix3x4f (const Matrix3x3f& other);
	float& operator [] (int index) { return m_Data[index];}
	Vector3f MultiplyVector3 (const Vector3f& v) const;
	Vector3f MultiplyPoint3 (const Vector3f& v) const;
};

enum { kLightProbeBasisCount = 9};
enum { kLightProbeCoefficientCount = kLightProbeBasisCount*3};

struct LightProbeCoefficients
{
	float sh[kLightProbeBasisCount*3];
	float& operator [] (int i) { return sh[i]; }
	DECLARE_SERIALIZE(LightmapData)
};

struct Tetrahedron
{
	int indices[4];
	int neighbors[4];
	// For an inner tetrahedron: the matrix is the cached inverted matrix used for calculating barycentric coordinates
	// For an outer tetrahedron: it's a matrix allowing the calculation of the cubic coefficients for the
	// t parameter polynomial, used for calculating barycentric coordinates as well
	Matrix3x4f matrix;
	DECLARE_SERIALIZE(Tetrahedron)
};

struct LightProbeData
{
	dynamic_array<Vector3f> positions;
	dynamic_array<LightProbeCoefficients> coefficients;
	dynamic_array<Tetrahedron> tetrahedra;
	// TODO: Sort probes so that all outer probes are at the beginning of m_BakedPositions. Thanks to that m_HullRays will be only 
	// as long as there are outer probes, not all probes.
	dynamic_array<Vector3f> hullRays;
};

class LightProbes;
LightProbes* GetLightProbes ();

class LightProbes : public NamedObject
{
public:
	REGISTER_DERIVED_CLASS (LightProbes, NamedObject)
	DECLARE_OBJECT_SERIALIZE(LightProbes)

	LightProbes(MemLabelId label, ObjectCreationMode mode);

	void AwakeFromLoad(AwakeFromLoadMode mode);
	Vector3f* GetPositions() { return m_Data.positions.size() > 0 ? &m_Data.positions[0] : NULL; }
	int GetPositionsSize() { return m_Data.positions.size(); }
	LightProbeCoefficients* GetCoefficients() { return m_Data.coefficients.size() > 0 ? &m_Data.coefficients[0] : NULL; }
	void SetCoefficients( float* data, int size );
	int GetTetrahedraSize() { return m_Data.tetrahedra.size();}
#if UNITY_EDITOR
	Tetrahedron* GetTetrahedra() { return m_Data.tetrahedra.size() > 0 ? &m_Data.tetrahedra[0] : NULL;}
	Vector3f* GetHullRays() { return m_Data.hullRays.size() > 0 ? &m_Data.hullRays[0] : NULL; }
	void SetBakedData (const dynamic_array<Vector3f>& positions, const dynamic_array<LightProbeCoefficients>& coefficents);
#endif
	inline void GetInterpolatedLightProbe(const Vector3f& position, Renderer* renderer, float* coefficients)
	{
		int tetIndex = -1;
		Vector4f weights;
		float t;
		GetInterpolatedLightProbe(position, renderer, coefficients, tetIndex, weights, t);
	}
	void GetInterpolatedLightProbe(const Vector3f& position, Renderer* renderer, float* coefficients, int& tetIndex, Vector4f& weights, float& t);

	static void InitializeClass() {}
	static void CleanupClass() {}
	inline static bool AreBaked() { LightProbes* lp = GetLightProbes(); return lp && lp->GetTetrahedraSize() > 0; }

private:
	LightProbeData m_Data;
};

