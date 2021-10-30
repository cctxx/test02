#ifndef SHURIKENMODULECLAMPVELOCITY_H
#define SHURIKENMODULECLAMPVELOCITY_H

#include "ParticleSystemModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemCurves.h"

class ClampVelocityModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (ClampVelocityModule)
	ClampVelocityModule ();

	void Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex);
	void CheckConsistency ();
	
	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);
	
private:
	MinMaxCurve m_X;
	MinMaxCurve m_Y;
	MinMaxCurve m_Z;
	MinMaxCurve m_Magnitude;
	bool m_InWorldSpace;
	bool m_SeparateAxis;
	float m_Dampen;
};

#endif // SHURIKENMODULECLAMPVELOCITY_H
