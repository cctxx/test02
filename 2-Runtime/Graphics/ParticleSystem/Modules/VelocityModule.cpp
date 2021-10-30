#include "UnityPrefix.h"
#include "VelocityModule.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "../ParticleSystemUtils.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Math/Matrix4x4.h"

template<ParticleSystemCurveEvalMode mode>
void UpdateTpl(const MinMaxCurve& x, const MinMaxCurve& y, const MinMaxCurve& z, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, bool transform, const Matrix4x4f& matrix)
{
	for (size_t q = fromIndex; q < toIndex; ++q)
	{
		Vector3f random;
		GenerateRandom3(random, ps.randomSeed[q] + kParticleSystemVelocityCurveId);

		const float normalizedTime = NormalizedTime (ps, q);
		Vector3f vel = Vector3f (Evaluate<mode> (x, normalizedTime, random.x), Evaluate<mode> (y, normalizedTime, random.y), Evaluate<mode> (z, normalizedTime, random.z));
		if(transform)
			vel = matrix.MultiplyVector3 (vel);
		ps.animatedVelocity[q] += vel;
	}
}

template<bool isOptimized>
void UpdateProceduralTpl(const DualMinMax3DPolyCurves& curves, const MinMaxCurve& x, const MinMaxCurve& y, const MinMaxCurve& z, ParticleSystemParticles& ps, const Matrix4x4f& matrix, bool transform)
{
	const size_t count = ps.array_size ();
	for (int q=0;q<count;q++)
	{
		Vector3f random;
		GenerateRandom3(random, ps.randomSeed[q] + kParticleSystemVelocityCurveId);
		const float time = NormalizedTime(ps, q);

		Vector3f delta;
		if(isOptimized)
			delta = Vector3f(EvaluateIntegrated(curves.optX, time, random.x), EvaluateIntegrated(curves.optY, time, random.y), EvaluateIntegrated(curves.optZ, time, random.z)) * ps.startLifetime[q];
		else
			delta = Vector3f(EvaluateIntegrated(curves.x, time, random.x), EvaluateIntegrated(curves.y, time, random.y), EvaluateIntegrated(curves.z, time, random.z)) * ps.startLifetime[q];

		Vector3f velocity (Evaluate(x, time, random.x), Evaluate(y, time, random.y), Evaluate(z, time, random.z));
		if(transform)
		{
			delta = matrix.MultiplyVector3 (delta);
			velocity = matrix.MultiplyVector3 (velocity);
		}
		ps.position[q] += delta;
		ps.animatedVelocity[q] += velocity;
	}
}

VelocityModule::VelocityModule () : ParticleSystemModule(false)
,	m_InWorldSpace (false)
{}

void VelocityModule::Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex)
{
	Matrix4x4f matrix;
	bool transform = GetTransformationMatrix(matrix, !roState.useLocalSpace, m_InWorldSpace, state.localToWorld);	
	
	bool usesScalar = (m_X.minMaxState == kMMCScalar) && (m_Y.minMaxState == kMMCScalar) && (m_Z.minMaxState == kMMCScalar);
	bool isOptimized = m_X.IsOptimized() && m_Y.IsOptimized() && m_Z.IsOptimized();
	bool usesMinMax = m_X.UsesMinMax() && m_Y.UsesMinMax() && m_Z.UsesMinMax();
	if(usesScalar)
		UpdateTpl<kEMScalar>(m_X, m_Y, m_Z, ps, fromIndex, toIndex, transform, matrix);
	else if(isOptimized && usesMinMax)
		UpdateTpl<kEMOptimizedMinMax>(m_X, m_Y, m_Z, ps, fromIndex, toIndex, transform, matrix);
	else if(isOptimized)
		UpdateTpl<kEMOptimized>(m_X, m_Y, m_Z, ps, fromIndex, toIndex, transform, matrix);
	else
		UpdateTpl<kEMSlow>(m_X, m_Y, m_Z, ps, fromIndex, toIndex, transform, matrix);

}

void VelocityModule::UpdateProcedural (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps)
{
	Matrix4x4f matrix;
	bool transform = GetTransformationMatrix(matrix, !roState.useLocalSpace, m_InWorldSpace, state.localToWorld);

	DualMinMax3DPolyCurves curves;
	if(m_X.IsOptimized() && m_Y.IsOptimized() && m_Z.IsOptimized())
	{
		curves.optX = m_X.polyCurves; curves.optX.Integrate();
		curves.optY = m_Y.polyCurves; curves.optY.Integrate();
		curves.optZ = m_Z.polyCurves; curves.optZ.Integrate();
		UpdateProceduralTpl<true>(curves, m_X, m_Y, m_Z, ps, matrix, transform);
	}
	else
	{
		DebugAssert(CurvesSupportProcedural (m_X.editorCurves, m_X.minMaxState));
		DebugAssert(CurvesSupportProcedural (m_Y.editorCurves, m_Y.minMaxState));
		DebugAssert(CurvesSupportProcedural (m_Z.editorCurves, m_Z.minMaxState));
		BuildCurves(curves.x, m_X.editorCurves, m_X.GetScalar(), m_X.minMaxState); curves.x.Integrate();
		BuildCurves(curves.y, m_Y.editorCurves, m_Y.GetScalar(), m_Y.minMaxState); curves.y.Integrate();
		BuildCurves(curves.z, m_Z.editorCurves, m_Z.GetScalar(), m_Z.minMaxState); curves.z.Integrate();
		UpdateProceduralTpl<false>(curves, m_X, m_Y, m_Z, ps, matrix, transform);
	}
}

void VelocityModule::CalculateProceduralBounds(MinMaxAABB& bounds, const Matrix4x4f& localToWorld, float maxLifeTime)
{
	Vector2f xRange = m_X.FindMinMaxIntegrated();
	Vector2f yRange = m_Y.FindMinMaxIntegrated();
	Vector2f zRange = m_Z.FindMinMaxIntegrated();
	bounds.m_Min = Vector3f(xRange.x, yRange.x, zRange.x) * maxLifeTime;
	bounds.m_Max = Vector3f(xRange.y, yRange.y, zRange.y) * maxLifeTime;
	if(m_InWorldSpace)
	{
		Matrix4x4f matrix;
		Matrix4x4f::Invert_General3D(localToWorld, matrix);
		matrix.SetPosition(Vector3f::zero);
		AABB aabb = bounds;
		TransformAABBSlow(aabb, matrix, aabb);
		bounds = aabb;
	}
}

template<class TransferFunction>
void VelocityModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_X, "x");
	transfer.Transfer (m_Y, "y");
	transfer.Transfer (m_Z, "z");
	transfer.Transfer (m_InWorldSpace, "inWorldSpace"); transfer.Align();
}
INSTANTIATE_TEMPLATE_TRANSFER(VelocityModule)
