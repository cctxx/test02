#ifndef SHURIKENMODULEEXTERNALFORCES_H
#define SHURIKENMODULEEXTERNALFORCES_H

#include "ParticleSystemModule.h"

class ExternalForcesModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (ExternalForcesModule)
	ExternalForcesModule ();

	void Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, float dt);
	void CheckConsistency() {};

	static void AllocateAndCache(const ParticleSystemReadOnlyState& roState, ParticleSystemState& state);
	static void FreeCache(ParticleSystemState& state);

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

private:
	float m_Multiplier;
};

#endif // SHURIKENMODULEEXTERNALFORCES_H
