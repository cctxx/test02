#include "UnityPrefix.h"
#include "SizeModule.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemUtils.h"

template<ParticleSystemCurveEvalMode mode>
void UpdateTpl(const MinMaxCurve& curve, const ParticleSystemParticles& ps, float* tempSize, size_t fromIndex, size_t toIndex)
{
	for (size_t q = fromIndex; q < toIndex; ++q)
	{
		const float time = NormalizedTime(ps, q);
		const float random = GenerateRandom(ps.randomSeed[q] + kParticleSystemSizeCurveId);
		tempSize[q] *= max<float>(0.0f, Evaluate<mode> (curve, time, random));
	}
}

SizeModule::SizeModule() : ParticleSystemModule(false)
{}

void SizeModule::Update (const ParticleSystemParticles& ps, float* tempSize, size_t fromIndex, size_t toIndex)
{
	DebugAssert(toIndex <= ps.array_size ());
	if(m_Curve.minMaxState == kMMCScalar)
		UpdateTpl<kEMScalar>(m_Curve, ps, tempSize, fromIndex, toIndex);
	else if (m_Curve.IsOptimized() && m_Curve.UsesMinMax ())
		UpdateTpl<kEMOptimizedMinMax>(m_Curve, ps, tempSize, fromIndex, toIndex);
	else if(m_Curve.IsOptimized())
		UpdateTpl<kEMOptimized>(m_Curve, ps, tempSize, fromIndex, toIndex);
	else
		UpdateTpl<kEMSlow>(m_Curve, ps, tempSize, fromIndex, toIndex);
}

template<class TransferFunction>
void SizeModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_Curve, "curve");
}
INSTANTIATE_TEMPLATE_TRANSFER(SizeModule)
