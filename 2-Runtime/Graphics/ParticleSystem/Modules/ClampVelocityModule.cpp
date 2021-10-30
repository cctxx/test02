#include "UnityPrefix.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "ClampVelocityModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemUtils.h"
#include "Runtime/Misc/BuildSettings.h"

inline float DampenOutsideLimit (float v, float limit, float dampen)
{
	float sgn = Sign (v);
	float abs = Abs (v);
	if (abs > limit)
		abs = Lerp(abs, limit, dampen);
	return abs * sgn;
}

ClampVelocityModule::ClampVelocityModule () : ParticleSystemModule(false)
,	m_SeparateAxis (false)
,	m_InWorldSpace (false)
,	m_Dampen (1.0f)
{
}

template<ParticleSystemCurveEvalMode mode>
void MagnitudeUpdateTpl(const MinMaxCurve& magnitude, ParticleSystemParticles& ps, size_t fromIndex, size_t toIndex, float dampen)
{
	for (size_t q = fromIndex; q < toIndex; ++q)
	{
		float limit = Evaluate<mode> (magnitude, NormalizedTime(ps, q), GenerateRandom(ps.randomSeed[q] + kParticleSystemClampVelocityCurveId));
		Vector3f vel = ps.velocity[q] + ps.animatedVelocity[q];
		vel = NormalizeSafe (vel) * DampenOutsideLimit (Magnitude (vel), limit, dampen);
		ps.velocity[q] = vel - ps.animatedVelocity[q];
	}
}

void ClampVelocityModule::Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex)
{
	if (m_SeparateAxis)
	{
		Matrix4x4f matrix;
		Matrix4x4f invMatrix;
		bool transform;
		if(IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
			transform = GetTransformationMatrices(matrix, invMatrix, !roState.useLocalSpace, m_InWorldSpace, state.localToWorld);
		else		
			transform = false; // Revert to old broken behavior

		for (size_t q = fromIndex; q < toIndex; ++q)
		{
			Vector3f random;
			GenerateRandom3(random, ps.randomSeed[q] + kParticleSystemClampVelocityCurveId);
			const float time = NormalizedTime(ps, q);

			Vector3f vel = ps.velocity[q] + ps.animatedVelocity[q];
			if(transform)
				vel = matrix.MultiplyVector3 (vel);
			const Vector3f limit (Evaluate (m_X, time, random.x), Evaluate (m_Y, time, random.y), Evaluate (m_Z, time, random.z));
			vel.x = DampenOutsideLimit (vel.x, limit.x, m_Dampen);
			vel.y = DampenOutsideLimit (vel.y, limit.y, m_Dampen);
			vel.z = DampenOutsideLimit (vel.z, limit.z, m_Dampen);
			vel = vel - ps.animatedVelocity[q];
			if(transform)
				vel = invMatrix.MultiplyVector3 (vel);
			ps.velocity[q] = vel;
		}
	}
	else
	{
		if(m_Magnitude.minMaxState == kMMCScalar)
			MagnitudeUpdateTpl<kEMScalar> (m_Magnitude, ps, fromIndex, toIndex, m_Dampen);
		else if(m_Magnitude.IsOptimized() && m_Magnitude.UsesMinMax())
			MagnitudeUpdateTpl<kEMOptimizedMinMax> (m_Magnitude, ps, fromIndex, toIndex, m_Dampen);
		else if(m_Magnitude.IsOptimized())
			MagnitudeUpdateTpl<kEMOptimized> (m_Magnitude, ps, fromIndex, toIndex, m_Dampen);
		else
			MagnitudeUpdateTpl<kEMSlow> (m_Magnitude, ps, fromIndex, toIndex, m_Dampen);
	}
}

void ClampVelocityModule::CheckConsistency ()
{
	m_Dampen = clamp<float> (m_Dampen, 0.0f, 1.0f);
	m_X.SetScalar(std::max<float> (0.0f, m_X.GetScalar()));
	m_Y.SetScalar(std::max<float> (0.0f, m_Y.GetScalar()));
	m_Z.SetScalar(std::max<float> (0.0f, m_Z.GetScalar()));
	m_Magnitude.SetScalar(std::max<float> (0.0f, m_Magnitude.GetScalar()));
}


template<class TransferFunction>
void ClampVelocityModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_X, "x");
	transfer.Transfer (m_Y, "y");
	transfer.Transfer (m_Z, "z");
	transfer.Transfer (m_Magnitude, "magnitude");
	transfer.Transfer (m_SeparateAxis, "separateAxis");
	transfer.Transfer (m_InWorldSpace, "inWorldSpace"); transfer.Align ();
	transfer.Transfer (m_Dampen, "dampen");
}
INSTANTIATE_TEMPLATE_TRANSFER(ClampVelocityModule)
