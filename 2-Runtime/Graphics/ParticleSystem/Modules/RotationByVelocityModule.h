#ifndef SHURIKENMODULEROTATIONBYVELOCITY_H
#define SHURIKENMODULEROTATIONBYVELOCITY_H

#include "ParticleSystemModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemCurves.h"
#include "Runtime/Math/Vector2.h"

class RotationBySpeedModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (RotationBySpeedModule)
	RotationBySpeedModule ();

	void Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex);
	void CheckConsistency ();
	
	inline MinMaxCurve& GetCurve() { return m_Curve; }

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

private:
	MinMaxCurve m_Curve;
	Vector2f m_Range;
};

#endif // SHURIKENMODULEROTATIONBYVELOCITY_H
