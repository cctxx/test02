#include "UnityPrefix.h"
#include "ForceModule.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "../ParticleSystemUtils.h"
#include "Runtime/Math/Random/Random.h"

template<ParticleSystemCurveEvalMode mode>
void UpdateTpl(const MinMaxCurve& x, const MinMaxCurve& y, const MinMaxCurve& z, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, bool transform, const Matrix4x4f& matrix, float dt)
{
	for (size_t q = fromIndex; q < toIndex; ++q)
	{
		Vector3f random;
		GenerateRandom3(random, ps.randomSeed[q] + kParticleSystemForceCurveId);
		const float time = NormalizedTime(ps, q);
		Vector3f f = Vector3f (Evaluate<mode> (x, time, random.x), Evaluate<mode> (y, time, random.y), Evaluate<mode> (z, time, random.z));
		if(transform)
			f = matrix.MultiplyVector3 (f);
		ps.velocity[q] += f * dt;
	}
}

template<bool isOptimized>
void UpdateProceduralTpl(const DualMinMax3DPolyCurves& pos, const DualMinMax3DPolyCurves& vel, ParticleSystemParticles& ps, const Matrix4x4f& matrix, bool transform)
{
	const size_t count = ps.array_size();
	for (int q=0; q<count; q++)
	{
		Vector3f random;
		GenerateRandom3(random, ps.randomSeed[q] + kParticleSystemForceCurveId);
		float time = NormalizedTime(ps, q);
		float range = ps.startLifetime[q];

		Vector3f delta;
		Vector3f velocity;
		if(isOptimized)
		{
			delta = Vector3f (EvaluateDoubleIntegrated(pos.optX, time, random.x), EvaluateDoubleIntegrated(pos.optY, time, random.y), EvaluateDoubleIntegrated(pos.optZ, time, random.z));
			velocity = Vector3f (EvaluateIntegrated(vel.optX, time, random.x), EvaluateIntegrated(vel.optY, time, random.y), EvaluateIntegrated(vel.optZ, time, random.z));
		}
		else
		{
			delta = Vector3f (EvaluateDoubleIntegrated(pos.x, time, random.x), EvaluateDoubleIntegrated(pos.y, time, random.y), EvaluateDoubleIntegrated(pos.z, time, random.z));
			velocity = Vector3f (EvaluateIntegrated(vel.x, time, random.x), EvaluateIntegrated(vel.y, time, random.y), EvaluateIntegrated(vel.z, time, random.z));
		}

		// Sqr range
		delta *= range * range;
		velocity *= range;

		if(transform)
		{
			delta = matrix.MultiplyVector3 (delta);
			velocity = matrix.MultiplyVector3 (velocity);
		}

		ps.position[q] += delta;
		ps.velocity[q] += velocity;
	}
}

ForceModule::ForceModule () : ParticleSystemModule(false)
,	m_RandomizePerFrame (false)
,	m_InWorldSpace(false)
{}

void ForceModule::Update  (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, float dt)
{
	Matrix4x4f matrix;
	bool transform = GetTransformationMatrix(matrix, !roState.useLocalSpace, m_InWorldSpace, state.localToWorld);	
	
	if (m_RandomizePerFrame)
	{
		for (size_t q = fromIndex; q < toIndex; ++q)		
		{
			const float t = NormalizedTime (ps, q);
			const float randomX = Random01 (m_Random);
			const float randomY = Random01 (m_Random);
			const float randomZ = Random01 (m_Random);
			Vector3f f (Evaluate (m_X, t, randomX), Evaluate (m_Y, t, randomY), Evaluate (m_Z, t, randomZ));
			if(transform)
				f = matrix.MultiplyVector3 (f);
			ps.velocity[q] += f * dt;
		}
	}
	else
	{
		bool usesScalar = (m_X.minMaxState == kMMCScalar) && (m_Y.minMaxState == kMMCScalar) && (m_Z.minMaxState == kMMCScalar);
		bool isOptimized = m_X.IsOptimized() && m_Y.IsOptimized() && m_Z.IsOptimized();
		bool usesMinMax = m_X.UsesMinMax() && m_Y.UsesMinMax() && m_Z.UsesMinMax();
		if(usesScalar)
			UpdateTpl<kEMScalar>(m_X, m_Y, m_Z, ps, fromIndex, toIndex, transform, matrix, dt);
		else if(isOptimized && usesMinMax)
			UpdateTpl<kEMOptimizedMinMax>(m_X, m_Y, m_Z, ps, fromIndex, toIndex, transform, matrix, dt);
		else if(isOptimized)
			UpdateTpl<kEMOptimized>(m_X, m_Y, m_Z, ps, fromIndex, toIndex, transform, matrix, dt);
		else
			UpdateTpl<kEMSlow>(m_X, m_Y, m_Z, ps, fromIndex, toIndex, transform, matrix, dt);
	}
}

void ForceModule::UpdateProcedural  (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps)
{
	Assert(!m_RandomizePerFrame);

	Matrix4x4f matrix;
	bool transform = GetTransformationMatrix(matrix, !roState.useLocalSpace, m_InWorldSpace, state.localToWorld);
	
	DualMinMax3DPolyCurves posCurves;
	DualMinMax3DPolyCurves velCurves;	
	if(m_X.IsOptimized() && m_Y.IsOptimized() && m_Z.IsOptimized())
	{
		posCurves.optX = m_X.polyCurves; posCurves.optX.DoubleIntegrate();
		posCurves.optY = m_Y.polyCurves; posCurves.optY.DoubleIntegrate();
		posCurves.optZ = m_Z.polyCurves; posCurves.optZ.DoubleIntegrate();
		velCurves.optX = m_X.polyCurves; velCurves.optX.Integrate();
		velCurves.optY = m_Y.polyCurves; velCurves.optY.Integrate();
		velCurves.optZ = m_Z.polyCurves; velCurves.optZ.Integrate();
		UpdateProceduralTpl<true>(posCurves, velCurves, ps, matrix, transform);
	}
	else
	{
		DebugAssert(CurvesSupportProcedural (m_X.editorCurves, m_X.minMaxState));
		DebugAssert(CurvesSupportProcedural (m_Y.editorCurves, m_Y.minMaxState));
		DebugAssert(CurvesSupportProcedural (m_Z.editorCurves, m_Z.minMaxState));
		BuildCurves(posCurves.x, m_X.editorCurves, m_X.GetScalar(), m_X.minMaxState); posCurves.x.DoubleIntegrate();
		BuildCurves(posCurves.y, m_Y.editorCurves, m_Y.GetScalar(), m_Y.minMaxState); posCurves.y.DoubleIntegrate();
		BuildCurves(posCurves.z, m_Z.editorCurves, m_Z.GetScalar(), m_Z.minMaxState); posCurves.z.DoubleIntegrate();
		BuildCurves(velCurves.x, m_X.editorCurves, m_X.GetScalar(), m_X.minMaxState); velCurves.x.Integrate();
		BuildCurves(velCurves.y, m_Y.editorCurves, m_Y.GetScalar(), m_Y.minMaxState); velCurves.y.Integrate();
		BuildCurves(velCurves.z, m_Z.editorCurves, m_Z.GetScalar(), m_Z.minMaxState); velCurves.z.Integrate();
		UpdateProceduralTpl<false>(posCurves, velCurves, ps, matrix, transform);
	}
}
void ForceModule::CalculateProceduralBounds(MinMaxAABB& bounds, const Matrix4x4f& localToWorld, float maxLifeTime)
{	
	Vector2f xRange = m_X.FindMinMaxDoubleIntegrated();
	Vector2f yRange = m_Y.FindMinMaxDoubleIntegrated();
	Vector2f zRange = m_Z.FindMinMaxDoubleIntegrated();
	bounds.m_Min = Vector3f(xRange.x, yRange.x, zRange.x) * maxLifeTime * maxLifeTime;
	bounds.m_Max = Vector3f(xRange.y, yRange.y, zRange.y) * maxLifeTime * maxLifeTime;
	
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
void ForceModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_X, "x");
	transfer.Transfer (m_Y, "y");
	transfer.Transfer (m_Z, "z");
	transfer.Transfer (m_InWorldSpace, "inWorldSpace");
	transfer.Transfer (m_RandomizePerFrame, "randomizePerFrame"); transfer.Align ();
}
INSTANTIATE_TEMPLATE_TRANSFER(ForceModule)
