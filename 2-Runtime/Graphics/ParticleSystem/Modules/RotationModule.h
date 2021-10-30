#ifndef SHURIKENMODULEROTATION_H
#define SHURIKENMODULEROTATION_H

#include "ParticleSystemModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemCurves.h"

class RotationModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (RotationModule)	
	
	RotationModule();

	void Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex);
	void UpdateProcedural (const ParticleSystemState& state, ParticleSystemParticles& ps);
	void CheckConsistency() {};

	inline MinMaxCurve& GetCurve() { return m_Curve; }

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);
	
private:
	MinMaxCurve m_Curve;
};

#endif // SHURIKENMODULEROTATION_H
