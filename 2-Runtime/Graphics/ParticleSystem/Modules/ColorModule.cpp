#include "UnityPrefix.h"
#include "ColorModule.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemUtils.h"

template<MinMaxGradientEvalMode mode>
void UpdateTpl(const ParticleSystemParticles& ps, ColorRGBA32* colorTemp, const MinMaxGradient& gradient, const OptimizedMinMaxGradient& optGradient, size_t fromIndex, size_t toIndex)
{
	for (size_t q = fromIndex; q < toIndex; ++q)
	{
		const float time = NormalizedTime(ps, q);
		const int random = GenerateRandomByte(ps.randomSeed[q] + kParticleSystemColorGradientId);

		ColorRGBA32 value;
		if(mode == kGEMGradient)
			value = EvaluateGradient (optGradient, time);
		else if(mode == kGEMGradientMinMax)
			value = EvaluateRandomGradient (optGradient, time, random);
		else
			value = Evaluate (gradient, time, random);

		colorTemp[q] *= value;
	}
}

ColorModule::ColorModule () : ParticleSystemModule(false)
{}

void ColorModule::Update (const ParticleSystemParticles& ps, ColorRGBA32* colorTemp, size_t fromIndex, size_t toIndex)
{
	DebugAssert(toIndex <= ps.array_size());

	OptimizedMinMaxGradient gradient;
	m_Gradient.InitializeOptimized(gradient);
	if(m_Gradient.minMaxState == kMMGGradient)
		UpdateTpl<kGEMGradient>(ps, colorTemp, m_Gradient, gradient, fromIndex, toIndex);
	else if(m_Gradient.minMaxState == kMMGRandomBetweenTwoGradients)
		UpdateTpl<kGEMGradientMinMax>(ps, colorTemp, m_Gradient, gradient, fromIndex, toIndex);
	else
		UpdateTpl<kGEMSlow>(ps, colorTemp, m_Gradient, gradient, fromIndex, toIndex);
}

template<class TransferFunction>
void ColorModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_Gradient, "gradient");
}
INSTANTIATE_TEMPLATE_TRANSFER(ColorModule)
