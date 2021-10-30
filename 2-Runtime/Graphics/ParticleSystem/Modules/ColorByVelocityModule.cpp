#include "UnityPrefix.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "ColorByVelocityModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemUtils.h"

template<MinMaxGradientEvalMode mode>
void UpdateTpl(const ParticleSystemParticles& ps, ColorRGBA32* colorTemp, const MinMaxGradient& gradient, const OptimizedMinMaxGradient& optGradient, const Vector2f offsetScale, size_t fromIndex, size_t toIndex)
{
	for (size_t q = fromIndex; q < toIndex; ++q)
	{
		Vector3f vel = ps.velocity[q] + ps.animatedVelocity[q];
		const float time = InverseLerpFast01 (offsetScale, Magnitude(vel));
		const int random = GenerateRandomByte(ps.randomSeed[q] + kParticleSystemColorByVelocityGradientId);

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

ColorBySpeedModule::ColorBySpeedModule () : ParticleSystemModule(false)
,	m_Range (0.0f, 1.0f)
{}

void ColorBySpeedModule::Update (const ParticleSystemParticles& ps, ColorRGBA32* colorTemp, size_t fromIndex, size_t toIndex)
{
	DebugAssert(toIndex <= ps.array_size());

	Vector2f offsetScale = CalculateInverseLerpOffsetScale (m_Range);
	OptimizedMinMaxGradient gradient;
	m_Gradient.InitializeOptimized(gradient);
	if(m_Gradient.minMaxState == kMMGGradient)
		UpdateTpl<kGEMGradient>(ps, colorTemp, m_Gradient, gradient, offsetScale, fromIndex, toIndex);
	else if(m_Gradient.minMaxState == kMMGRandomBetweenTwoGradients)
		UpdateTpl<kGEMGradientMinMax>(ps, colorTemp, m_Gradient, gradient, offsetScale, fromIndex, toIndex);
	else
		UpdateTpl<kGEMSlow>(ps, colorTemp, m_Gradient, gradient, offsetScale, fromIndex, toIndex);
}

void ColorBySpeedModule::CheckConsistency ()
{
	const float MyEpsilon = 0.001f;
	m_Range.x = std::min (m_Range.x, m_Range.y - MyEpsilon);
}

template<class TransferFunction>
void ColorBySpeedModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_Gradient, "gradient");
	transfer.Transfer (m_Range, "range");
}
INSTANTIATE_TEMPLATE_TRANSFER(ColorBySpeedModule)
