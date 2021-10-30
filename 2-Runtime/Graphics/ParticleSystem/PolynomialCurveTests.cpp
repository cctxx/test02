#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Animation/AnimationCurveUtility.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Graphics/ParticleSystem/PolynomialCurve.h"

static float PhysicsSimulate (float gravity, float velocity, float time)
{
	return velocity * time + gravity * time * time * 0.5F;
}

SUITE (PolynomialCurveTests)
{
TEST (PolynomialCurve_ConstantIntegrate)
{
	AnimationCurve editorCurve;
	OptimizedPolynomialCurve curve;
	PolynomialCurve polyCurve;

	SetPolynomialCurveToValue(editorCurve, curve, 1.0f);
	curve.Integrate();
	polyCurve.BuildCurve(editorCurve, 1.0f);
	polyCurve.Integrate();

	CHECK_CLOSE(0.0F, curve.EvaluateIntegrated(0.0F), 0.0001F);
	CHECK_CLOSE(0.5F, curve.EvaluateIntegrated(0.5F), 0.0001F);
	CHECK_CLOSE(1.0F, curve.EvaluateIntegrated(1.0F), 0.0001F);

	CHECK_CLOSE(0.0F, polyCurve.EvaluateIntegrated(0.0F), 0.0001F);
	CHECK_CLOSE(0.5F, polyCurve.EvaluateIntegrated(0.5F), 0.0001F);
	CHECK_CLOSE(1.0F, polyCurve.EvaluateIntegrated(1.0F), 0.0001F);

	CHECK_EQUAL(true, CompareApproximately(Vector2f(0.0f, 1.0f), curve.FindMinMaxIntegrated(), 0.0001F));
	CHECK_EQUAL(true, CompareApproximately(Vector2f(0.0f, 1.01f), polyCurve.FindMinMaxIntegrated(), 0.0001F));


	SetPolynomialCurveToLinear(editorCurve, curve);
	curve.Integrate();
	polyCurve.BuildCurve(editorCurve, 1.0f);
	polyCurve.Integrate();

	CHECK_CLOSE(0.0F, curve.EvaluateIntegrated(0.0F), 0.0001F);
	CHECK_CLOSE(0.125F, curve.EvaluateIntegrated(0.5F), 0.0001F);
	CHECK_CLOSE(0.5F, curve.EvaluateIntegrated(1.0F), 0.0001F);	

	CHECK_CLOSE(0.0F, polyCurve.EvaluateIntegrated(0.0F), 0.0001F);
	CHECK_CLOSE(0.125F, polyCurve.EvaluateIntegrated(0.5F), 0.0001F);
	CHECK_CLOSE(0.5F, polyCurve.EvaluateIntegrated(1.0F), 0.0001F);

	CHECK_EQUAL(true, CompareApproximately(Vector2f(0.0f, 0.5f), curve.FindMinMaxIntegrated(), 0.0001F));
	CHECK_EQUAL(true, CompareApproximately(Vector2f(0.0f, 0.51005f), polyCurve.FindMinMaxIntegrated(), 0.0001F));
}

// @TODO: Add generic poly
TEST (PolynomialCurve_ConstantDoubleIntegrate)
{
	AnimationCurve editorCurve;
	OptimizedPolynomialCurve curve;
	PolynomialCurve polyCurve;

	SetPolynomialCurveToValue(editorCurve, curve, 1.0f);
	curve.DoubleIntegrate();

	polyCurve.BuildCurve(editorCurve, 1.0f);
	polyCurve.DoubleIntegrate();

	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.00F), curve.EvaluateDoubleIntegrated(0.00F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.25F), curve.EvaluateDoubleIntegrated(0.25F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.50F), curve.EvaluateDoubleIntegrated(0.50F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.75F), curve.EvaluateDoubleIntegrated(0.75F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 1.00F), curve.EvaluateDoubleIntegrated(1.00F), 0.0001F);

	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.00F), polyCurve.EvaluateDoubleIntegrated(0.00F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.25F), polyCurve.EvaluateDoubleIntegrated(0.25F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.50F), polyCurve.EvaluateDoubleIntegrated(0.50F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.75F), polyCurve.EvaluateDoubleIntegrated(0.75F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 1.00F), polyCurve.EvaluateDoubleIntegrated(1.00F), 0.0001F);

	CHECK_EQUAL(true, CompareApproximately(Vector2f(0.0f, 0.5f), curve.FindMinMaxDoubleIntegrated(), 0.0001F));
	CHECK_EQUAL(true, CompareApproximately(Vector2f(0.0f, 0.5f), polyCurve.FindMinMaxDoubleIntegrated(), 0.0001F));

	AnimationCurve::Keyframe keys[3] = { AnimationCurve::Keyframe(0.0f, 1.0f), AnimationCurve::Keyframe(0.5f, 1.0f), AnimationCurve::Keyframe(1.0f, 1.0f) };
	editorCurve.Assign(keys, keys + 3);
	curve.BuildOptimizedCurve(editorCurve, 1);
	curve.DoubleIntegrate();

	polyCurve.BuildCurve(editorCurve, 1.0f);
	polyCurve.DoubleIntegrate();

	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.00F), curve.EvaluateDoubleIntegrated(0.00F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.25F), curve.EvaluateDoubleIntegrated(0.25F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.50F), curve.EvaluateDoubleIntegrated(0.50F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.75F), curve.EvaluateDoubleIntegrated(0.75F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 1.00F), curve.EvaluateDoubleIntegrated(1.00F), 0.0001F);

	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.00F), polyCurve.EvaluateDoubleIntegrated(0.00F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.25F), polyCurve.EvaluateDoubleIntegrated(0.25F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.50F), polyCurve.EvaluateDoubleIntegrated(0.50F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 0.75F), polyCurve.EvaluateDoubleIntegrated(0.75F), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(1, 0, 1.00F), polyCurve.EvaluateDoubleIntegrated(1.00F), 0.0001F);

	CHECK_EQUAL(true, CompareApproximately(Vector2f(0.0f, 0.5f), curve.FindMinMaxDoubleIntegrated(), 0.0001F));
	CHECK_EQUAL(true, CompareApproximately(Vector2f(0.0f, 0.5f), polyCurve.FindMinMaxDoubleIntegrated(), 0.0001F));
}

static float EvaluateGravityIntegrated (OptimizedPolynomialCurve& gravityCurve, float time, float maximumRange)
{
	float res = gravityCurve.EvaluateDoubleIntegrated(time / maximumRange) * maximumRange * maximumRange;
	return res;
}

TEST (PolynomialCurve_GravityIntegrate)
{
	const float kGravity = -9.81f;
	AnimationCurve editorCurve;
	OptimizedPolynomialCurve gravityCurve;
	SetPolynomialCurveToValue(editorCurve, gravityCurve, kGravity);
	gravityCurve.DoubleIntegrate();

	const float kMaxRange = 5.0F;

	CHECK_CLOSE(PhysicsSimulate(kGravity, 0.0F, 0.0F), EvaluateGravityIntegrated(gravityCurve, 0.0F, kMaxRange), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(kGravity, 0.0F, 0.5F), EvaluateGravityIntegrated(gravityCurve, 0.5F, kMaxRange), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(kGravity, 0.0F, 1.0F), EvaluateGravityIntegrated(gravityCurve, 1.0F, kMaxRange), 0.0001F);
	CHECK_CLOSE(PhysicsSimulate(kGravity, 0.0F, 5.0F), EvaluateGravityIntegrated(gravityCurve, 5.0F, kMaxRange), 0.0001F);
}

float TriangleShapeIntegralFirstHalf (float x)
{
	return (2.0F / 3.0F) *x*x*x - 0.5F*x*x;
}

// Test that a simple triangle shape 0,-1 to 0.5,1 to 1,-1
// Gives expected results during double integration
TEST (PolynomialCurve_TriangleShapeDoubleIntegrate)
{
	OptimizedPolynomialCurve curve;
	AnimationCurve::Keyframe keys[3] = { AnimationCurve::Keyframe(0.0f, -1.0f), AnimationCurve::Keyframe(0.5f, 1.0f), AnimationCurve::Keyframe(1.0f, -1.0f) };
	AnimationCurve editorCurve;
	editorCurve.Assign(keys, keys + 3);

	RecalculateSplineSlopeLinear(editorCurve);

	curve.BuildOptimizedCurve(editorCurve, 1.0F);

	CHECK_CLOSE(-1, curve.Evaluate(0), 0.0001F);
	CHECK_CLOSE(0, curve.Evaluate(0.25F), 0.0001F);
	CHECK_CLOSE(1.0F, curve.Evaluate(0.5F), 0.0001F);
	CHECK_CLOSE(0.0F, curve.Evaluate(0.75F), 0.0001F);
	CHECK_CLOSE(-1, curve.Evaluate(1), 0.0001F);

	curve.DoubleIntegrate();

	CHECK_CLOSE(TriangleShapeIntegralFirstHalf(0),     EvaluateGravityIntegrated(curve, 0.0F , 1.0F), 0.0001F);
	CHECK_CLOSE(TriangleShapeIntegralFirstHalf(0.25F), EvaluateGravityIntegrated(curve, 0.25F, 1.0F), 0.0001F);
	CHECK_CLOSE(TriangleShapeIntegralFirstHalf(0.5F),  EvaluateGravityIntegrated(curve, 0.5F , 1.0F), 0.0001F);
	CHECK_CLOSE(-0.0208333,							   EvaluateGravityIntegrated(curve, 0.75F, 1.0F), 0.0001F);
	CHECK_CLOSE(0,                                     EvaluateGravityIntegrated(curve, 1.0F , 1.0F), 0.0001F);
}

void CompareIntegrateCurve (const AnimationCurve& curve, const OptimizedPolynomialCurve& integratedCurve)
{
	CHECK_CLOSE(0, integratedCurve.EvaluateIntegrated(0), 0.0001F);

	int intervals = 100;

	float sum = 0.0F;
	for (int i=1;i<intervals;i++)
	{
		float t = (float)i / (float)intervals;
		float dt = 1.0F / (float)intervals;

		float avgValue = curve.Evaluate(t - dt * 0.5F);
		sum += avgValue * dt;

		float integratedValue = integratedCurve.EvaluateIntegrated(t);
		CHECK_CLOSE(sum, integratedValue, 0.0001F);
	}
}

void CompareDoubleIntegrateCurve (const AnimationCurve& curve, const OptimizedPolynomialCurve& integratedCurve)
{
	CHECK_CLOSE(0, integratedCurve.EvaluateIntegrated(0), 0.0001F);

	int intervals = 1000;

	float integratedSum = 0.0F;
	float sum = 0.0F;
	for (int i=1;i<intervals;i++)
	{
		float t = (float)i / (float)intervals;
		float dt = 1.0F / (float)intervals;

		float avgValue = curve.Evaluate(t - dt * 0.5F);
		sum += avgValue * dt;
		integratedSum += sum * dt;

		float integratedValue = integratedCurve.EvaluateDoubleIntegrated(t);
		CHECK_CLOSE(integratedSum, integratedValue, 0.001F);
	}
}

void CompareIntegrateCurve (const AnimationCurve::Keyframe* keys, int size)
{
	AnimationCurve sourceCurve;
	sourceCurve.Assign(keys, keys + size);

	OptimizedPolynomialCurve curve;
	AnimationCurve editorCurve;
	editorCurve = sourceCurve;
	curve.BuildOptimizedCurve(editorCurve, 1);
	curve.Integrate();

	CompareIntegrateCurve(sourceCurve, curve);
}

void CompareDoubleIntegrateCurve (const AnimationCurve::Keyframe* keys, int size)
{
	AnimationCurve sourceCurve;
	sourceCurve.Assign(keys, keys + size);

	OptimizedPolynomialCurve curve;
	AnimationCurve editorCurve;
	editorCurve = sourceCurve;
	curve.BuildOptimizedCurve(editorCurve, 1);
	curve.DoubleIntegrate();

	CompareDoubleIntegrateCurve(sourceCurve, curve);
}

TEST (PolynomialCurve_TriangleCurve)
{
	AnimationCurve sourceCurve;
	AnimationCurve::Keyframe keys[3] = { AnimationCurve::Keyframe(0.0f, 0.0f), AnimationCurve::Keyframe(0.5f, 1.0f), AnimationCurve::Keyframe(1.0f, 0.0f) };
	sourceCurve.Assign(keys, keys + 3);
	RecalculateSplineSlopeLinear(sourceCurve);

	CompareIntegrateCurve(&sourceCurve.GetKey(0), sourceCurve.GetKeyCount());
	CompareDoubleIntegrateCurve(&sourceCurve.GetKey(0), sourceCurve.GetKeyCount());
}


TEST (PolynomialCurve_LineCurve)
{
	AnimationCurve sourceCurve;
	AnimationCurve::Keyframe keys[2] = { AnimationCurve::Keyframe(0.0f, 0.0f), AnimationCurve::Keyframe(1.0f, 1.0f) };
	sourceCurve.Assign(keys, keys + 2);
	RecalculateSplineSlopeLinear(sourceCurve);

	CompareIntegrateCurve(&sourceCurve.GetKey(0), sourceCurve.GetKeyCount());
	CompareDoubleIntegrateCurve(&sourceCurve.GetKey(0), sourceCurve.GetKeyCount());
}
}

#endif
