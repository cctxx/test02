#ifndef BEZIERCURVE_H
#define BEZIERCURVE_H

#include <vector>
#include "Runtime/Math/Vector3.h"

class Matrix4x4f;

class BezierCurve {
public:
	struct Knot {
		Vector3f m_Val[3];
		float texCoord;
		bool hardCorner;
		float detail;
		/// 3 bytes left		
		const Vector3f& operator[] (int i) const { return m_Val[i]; }
		Vector3f& operator[] (int i) { return m_Val[i]; }
		Knot ();
	};
	
	BezierCurve () : m_alreadyPrecalculated(false), m_Loop(false) {}
	BezierCurve (const BezierCurve &other);	
	void GetTransform(float t, Matrix4x4f& transform);
	void ApplyTransform (const Matrix4x4f &mat);

	std::vector<Knot>& Knots () {
		return m_Knots;
	}
	
	void Touch () {
		m_alreadyPrecalculated = false;
	}

	Vector3f NormalPoint (float t);
	float TexturePoint (float t);

	int FindWeightIndex (int section) {
		return section * 4;
	}

	Vector3f NormalTangent (float t);
	
	int FindNormalSection (float t) {
		int section = int(t);
		
		AssertIf ( (section < 0) || (section + (m_Loop ? 0 : 1) > (float)m_Knots.size()) );
		return section;
	}

	int Size () const {
		return m_Loop ? m_Knots.size() : m_Knots.size() - 1;
	}
	
	void SetLoop (bool loop) { m_Loop = loop; }
	bool GetLoop () const { return m_Loop; }

private:
	void Precalculate ();
	/// Precalculate weights that appear when the bezier functions are simplified
	void CalcWeights (Vector3f A, Vector3f B, Vector3f C, Vector3f D);

	// Actual, given values
	std::vector<Knot> m_Knots;
	bool m_Loop;

	// Precalculated stuff
	bool m_alreadyPrecalculated;
	std::vector<Vector3f> m_Weights;
};


#endif
