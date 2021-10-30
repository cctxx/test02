#ifndef SHURIKENMODULEEMISSION_H
#define SHURIKENMODULEEMISSION_H

#include "ParticleSystemModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemCurves.h"

class Vector2f;

class EmissionModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (EmissionModule)
	EmissionModule ();

	enum { kEmissionTypeTime, kEmissionTypeDistance };

	static void Emit (ParticleSystemEmissionState& emissionState, size_t& amountOfParticlesToEmit, size_t& numContinuous, const ParticleSystemEmissionData& emissionData, const Vector3f velocity, float fromT, float toT, float dt, float length);
	void CheckConsistency ();

	int CalculateMaximumEmitCountEstimate(float deltaTime) const;

	const ParticleSystemEmissionData& GetEmissionDataRef() { return m_EmissionData; }
	const ParticleSystemEmissionData& GetEmissionDataRef() const { return m_EmissionData; }
	void GetEmissionDataCopy(ParticleSystemEmissionData* emissionData) { *emissionData = m_EmissionData; };
	ParticleSystemEmissionData& GetEmissionData() { return m_EmissionData; }

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);
	
private:
	ParticleSystemEmissionData m_EmissionData;
};

#endif // SHURIKENMODULEEMISSION_H
