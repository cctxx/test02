#include "UnityPrefix.h"
#include "EmissionModule.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Math/Vector2.h"

static int AccumulateBursts (const ParticleSystemEmissionData& emissionData, float t0, float t1)
{
	int burstParticles = 0;
	const size_t count = emissionData.burstCount;
	for (size_t q = 0; q < count; ++q)
	{
		if (emissionData.burstTime[q] >= t0 && emissionData.burstTime[q] < t1)
			burstParticles += emissionData.burstParticleCount[q];
	}
	return burstParticles;
}

static float AccumulateContinuous(const ParticleSystemEmissionData& emissionData, const float length, const float toT, const float dt)
{
	DebugAssert (length > 0.0001f);
	DebugAssert (toT >= 0.0f);
	DebugAssert (toT <= length);
	float normalizedT = toT / length;
	return std::max<float> (0.0f, Evaluate (emissionData.rate, normalizedT)) * dt;
};

EmissionModule::EmissionModule () : ParticleSystemModule(true)
{
	m_EmissionData.burstCount = 0;
	m_EmissionData.type = kEmissionTypeTime;
	for(int i = 0; i < ParticleSystemEmissionData::kMaxNumBursts; i++)
	{
		m_EmissionData.burstParticleCount[i] = 30;
		m_EmissionData.burstTime[i] = 0.0f;
	}
}

void EmissionModule::Emit (ParticleSystemEmissionState& emissionState, size_t& amountOfParticlesToEmit, size_t& numContinuous, const ParticleSystemEmissionData& emissionData, const Vector3f velocity, float fromT, float toT, float dt, float length)
{
	const float epsilon = 0.0001f;
	if(kEmissionTypeTime == emissionData.type)
	{
		float rate = 0.0f;
		float t0 = std::max<float> (0.0f, fromT);
		float t1 = std::max<float> (0.0f, toT);
		if (t1 < t0) // handle loop
		{
			rate += AccumulateContinuous (emissionData, length, t1, t1); // from start to current time
			t1 = length; // from last time to end
		}
		rate += AccumulateContinuous (emissionData, length, t1, t1 - t0); // from start to current time

		const float newParticles = rate;
		if(newParticles >= epsilon)
			emissionState.m_ParticleSpacing = 1.0f / newParticles;
		else
			emissionState.m_ParticleSpacing = 1.0f;

		emissionState.m_ToEmitAccumulator += newParticles;
		amountOfParticlesToEmit = (int)emissionState.m_ToEmitAccumulator;
		emissionState.m_ToEmitAccumulator -= (float)amountOfParticlesToEmit;

		// Continuous emits
		numContinuous = amountOfParticlesToEmit;

		// Bursts
		t0 = std::max<float> (0.0f, fromT);
		t1 = std::max<float> (0.0f, toT);
		if (t1 < t0) // handle loop
		{
			amountOfParticlesToEmit += AccumulateBursts (emissionData, 0.0f, t1); // from start to current time
			t1 = length + epsilon; // from last time to end
		}
		amountOfParticlesToEmit += AccumulateBursts (emissionData, t0, t1); // from start to current time
	}
	else
	{
		float newParticles = AccumulateContinuous (emissionData, length, toT, dt) * Magnitude (velocity); // from start to current time
		if(newParticles >= epsilon)
			emissionState.m_ParticleSpacing = 1.0f / newParticles;
		else
			emissionState.m_ParticleSpacing = 1.0f;

		emissionState.m_ToEmitAccumulator += newParticles;
		amountOfParticlesToEmit = (int)emissionState.m_ToEmitAccumulator;
		emissionState.m_ToEmitAccumulator -= (float)amountOfParticlesToEmit;

		// Continuous emits
		numContinuous = amountOfParticlesToEmit;
	}
}

void EmissionModule::CheckConsistency ()
{
	m_EmissionData.rate.SetScalar(std::max<float> (0.0f, m_EmissionData.rate.GetScalar()));
	
	const size_t count = m_EmissionData.burstCount;
	for (size_t q = 0; q < count; ++q)
	{
		m_EmissionData.burstTime[q] = std::max<float> (0.0f, m_EmissionData.burstTime[q]);
	}
}

template<class TransferFunction>
void EmissionModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	
	transfer.Transfer (m_EmissionData.type, "m_Type");
	transfer.Transfer (m_EmissionData.rate, "rate");
	
	const char* kCountNames [ParticleSystemEmissionData::kMaxNumBursts] = { "cnt0", "cnt1", "cnt2", "cnt3", }; 
	const char* kTimeNames [ParticleSystemEmissionData::kMaxNumBursts] = { "time0", "time1", "time2", "time3", }; 
	for(int i = 0; i < ParticleSystemEmissionData::kMaxNumBursts; i++)
		transfer.Transfer (m_EmissionData.burstParticleCount[i], kCountNames[i]);
	
	for(int i = 0; i < ParticleSystemEmissionData::kMaxNumBursts; i++)
		transfer.Transfer (m_EmissionData.burstTime[i], kTimeNames[i]);
	
	transfer.Transfer (m_EmissionData.burstCount, "m_BurstCount"); transfer.Align();
}

INSTANTIATE_TEMPLATE_TRANSFER(EmissionModule)
