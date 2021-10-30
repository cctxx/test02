#ifndef SHURIKENMODULESUB_H
#define SHURIKENMODULESUB_H

#include "ParticleSystemModule.h"
#include "Runtime/BaseClasses/BaseObject.h"

class ParticleSystem;
struct ParticleSystemParticles;


class SubModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (SubModule)
	SubModule ();

	void Update (const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, size_t fromIndex, size_t toIndex, float dt) const;
	void CheckConsistency() {};

	int GetSubEmitterTypeCount(ParticleSystemSubType type) const;

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

	static void RemoveDuplicatePtrs (ParticleSystem** shurikens);
	int GetSubEmitterPtrs (ParticleSystem** shurikens) const;
	int GetSubEmitterPtrsBirth (ParticleSystem** shurikens) const;
	int GetSubEmitterPtrsCollision (ParticleSystem** shurikens) const;
	int GetSubEmitterPtrsDeath(ParticleSystem** shurikens) const;


private:
	PPtr<ParticleSystem> m_SubEmittersBirth[kParticleSystemMaxSubBirth];
	PPtr<ParticleSystem> m_SubEmittersCollision[kParticleSystemMaxSubCollision];
	PPtr<ParticleSystem> m_SubEmittersDeath[kParticleSystemMaxSubDeath];
};

#endif // SHURIKENMODULESUB_H
