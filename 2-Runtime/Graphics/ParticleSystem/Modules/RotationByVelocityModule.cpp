#include "UnityPrefix.h"
#include "RotationByVelocityModule.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemUtils.h"


RotationBySpeedModule::RotationBySpeedModule () : ParticleSystemModule(false)
,	m_Range (0.0f, 1.0f)
{}

template<ParticleSystemCurveEvalMode mode>
void UpdateTpl(const MinMaxCurve& curve, ParticleSystemParticles& ps, size_t fromIndex, size_t toIndex, const Vector2f offsetScale)
{	
	if (!ps.usesRotationalSpeed) return;
	for (size_t q = fromIndex; q < toIndex; ++q)
	{
		const Vector3f vel = ps.velocity[q] + ps.animatedVelocity[q];
		const float t = InverseLerpFast01 (offsetScale, Magnitude(vel));
		const float random = GenerateRandom(ps.randomSeed[q] + kParticleSystemRotationBySpeedCurveId);
		ps.rotationalSpeed[q] += Evaluate<mode> (curve, t, random);
	}
}

void RotationBySpeedModule::Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex)
{
	Vector2f offsetScale = CalculateInverseLerpOffsetScale (m_Range);
	if (m_Curve.minMaxState == kMMCScalar)
		UpdateTpl<kEMScalar>(m_Curve, ps, fromIndex, toIndex, offsetScale);
	else if(m_Curve.IsOptimized() && m_Curve.UsesMinMax())
		UpdateTpl<kEMOptimizedMinMax>(m_Curve, ps, fromIndex, toIndex, offsetScale);
	else if(m_Curve.IsOptimized())
		UpdateTpl<kEMOptimized>(m_Curve, ps, fromIndex, toIndex, offsetScale);
	else
		UpdateTpl<kEMSlow>(m_Curve, ps, fromIndex, toIndex, offsetScale);
}

void RotationBySpeedModule::CheckConsistency ()
{
	const float MyEpsilon = 0.001f;
	m_Range.y = std::max (m_Range.x + MyEpsilon, m_Range.y);
}

template<class TransferFunction>
void RotationBySpeedModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_Curve, "curve");
	transfer.Transfer (m_Range, "range");
}
INSTANTIATE_TEMPLATE_TRANSFER(RotationBySpeedModule)
