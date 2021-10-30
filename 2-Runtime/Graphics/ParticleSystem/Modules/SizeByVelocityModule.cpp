#include "UnityPrefix.h"
#include "SizeByVelocityModule.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemUtils.h"

SizeBySpeedModule::SizeBySpeedModule () : ParticleSystemModule(false)
,	m_Range (0.0f, 1.0f)
{}

template<ParticleSystemCurveEvalMode mode>
void UpdateTpl(const MinMaxCurve& curve, const ParticleSystemParticles& ps, float* tempSize, size_t fromIndex, size_t toIndex, const Vector2f offsetScale)
{
	for (size_t q = fromIndex; q < toIndex; ++q)
	{
		const Vector3f vel = ps.velocity[q] + ps.animatedVelocity[q];
		const float t = InverseLerpFast01 (offsetScale, Magnitude (vel));
		const float random = GenerateRandom(ps.randomSeed[q] + kParticleSystemSizeBySpeedCurveId);
		tempSize[q] *= max<float> (0.0f, Evaluate<mode> (curve, t, random));
	}
}

void SizeBySpeedModule::Update (const ParticleSystemParticles& ps, float* tempSize, size_t fromIndex, size_t toIndex)
{
	DebugAssert(toIndex <= ps.array_size ());
	Vector2f offsetScale = CalculateInverseLerpOffsetScale(m_Range);
	if (m_Curve.minMaxState == kMMCScalar)
		UpdateTpl<kEMScalar> (m_Curve, ps, tempSize, fromIndex, toIndex, offsetScale);
	else if (m_Curve.IsOptimized() && m_Curve.UsesMinMax())
		UpdateTpl<kEMOptimizedMinMax> (m_Curve, ps, tempSize, fromIndex, toIndex, offsetScale);
	else if(m_Curve.IsOptimized())
		UpdateTpl<kEMOptimized> (m_Curve, ps, tempSize, fromIndex, toIndex, offsetScale);
	else
		UpdateTpl<kEMSlow> (m_Curve, ps, tempSize, fromIndex, toIndex, offsetScale);
}

void SizeBySpeedModule::CheckConsistency ()
{
	const float MyEpsilon = 0.001f;
	m_Range.x = std::min (m_Range.x, m_Range.y - MyEpsilon);
}

template<class TransferFunction>
void SizeBySpeedModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_Curve, "curve");
	transfer.Transfer (m_Range, "range");
}
INSTANTIATE_TEMPLATE_TRANSFER(SizeBySpeedModule)
