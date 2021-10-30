#include "UnityPrefix.h"
#include "BezierCurve.h"
#include <math.h>
#include "Runtime/Math/Matrix4x4.h"


BezierCurve::Knot::Knot ()
{
	texCoord = 0.0f;
	hardCorner = false;
	detail = 1;
}

/// \todo: 		Fix math to find accurate length of curves (instead of silly approximation)
BezierCurve::BezierCurve (const BezierCurve &other) : m_Knots (other.m_Knots) {
	m_Loop = other.m_Loop;
	m_alreadyPrecalculated = false;
}

void BezierCurve::GetTransform(float t, Matrix4x4f& transform) {
	Precalculate ();
	Vector3f tangent = NormalTangent(t);
	Vector3f z_normal = Normalize(tangent);
	Vector3f x_normal = Normalize(Cross(z_normal, Vector3f(0.0f, -1.0f, 0.0f)));
	Vector3f y_normal = Cross(x_normal, -z_normal);
	transform.SetPositionAndOrthoNormalBasis(NormalPoint(t), x_normal, y_normal, z_normal);
}

void BezierCurve::ApplyTransform (const Matrix4x4f &mat) {
	for (int i = 0; i < m_Knots.size(); i++) {
		m_Knots[i][0] = mat.MultiplyPoint3 (m_Knots[i][0]);
		m_Knots[i][1] = mat.MultiplyPoint3 (m_Knots[i][1]);
		m_Knots[i][2] = mat.MultiplyPoint3 (m_Knots[i][2]);
	}
	Touch();
}

void BezierCurve::Precalculate ()
{
//	if (m_alreadyPrecalculated) return;

	int knots = m_Knots.size();
	m_Weights.clear();
	m_Weights.reserve(knots * 4);
	
	for (int i = 0; i < knots; i++) {
		int knots2 = i + 1;
		if (knots2 == knots)
			knots2 = 0;
		CalcWeights(m_Knots[i][1], 
				     m_Knots[i][2], 
				     m_Knots[knots2][0], 
				     m_Knots[knots2][1]);
	}

	m_alreadyPrecalculated = true;
}

/// Precalculate weights that appear when the bezier functions are simplified
void BezierCurve::CalcWeights (Vector3f A, Vector3f B, Vector3f C, Vector3f D) {
	Vector3f weights[4];
	
	for (int i = 0; i < 3; i++) {
		weights[0][i] = -A[i] + 3*B[i] - 3*C[i] + D[i];
		weights[1][i] = 3*A[i] - 6*B[i] + 3*C[i];
		weights[2][i] = -3*A[i] + 3*B[i];
		weights[3][i] = A[i];
	}
	
	m_Weights.push_back(weights[0]);
	m_Weights.push_back(weights[1]);
	m_Weights.push_back(weights[2]);
	m_Weights.push_back(weights[3]);
}

Vector3f BezierCurve::NormalPoint (float t) {
	int section = FindNormalSection(t);
	t -= (float)section;
			
	Precalculate();
	
	Vector3f point;

	int sec2 = section + 1;
	if (sec2 == m_Knots.size())
		sec2 = 0;
	point = m_Knots[section][1] * (1 - t) * (1 - t) * (1 - t)
		    + m_Knots[section][2] * (1 - t) * (1 - t) * t * 3
		    + m_Knots[sec2][0] * (1 - t) * t * t * 3
		    + m_Knots[sec2][1] * t * t * t;
	return point;

}

float BezierCurve::TexturePoint (float t) {
	int section = FindNormalSection(t);
	t -= (float)section;
			
	int sec2 = section + 1;
	if (sec2 == m_Knots.size())
		sec2 = 0;

	return m_Knots[sec2].texCoord * t + m_Knots[section].texCoord * (1 - t);
}

Vector3f BezierCurve::NormalTangent (float t) {		
	int section = FindNormalSection(t);
	int index = FindWeightIndex (section);
	t -= (float)section;
	
	Vector3f tangent;
	Precalculate();
	return (3 * m_Weights[index] * t * t
		    + 2 * m_Weights[index + 1] * t
		    + 1 * m_Weights[index + 2]);
}
