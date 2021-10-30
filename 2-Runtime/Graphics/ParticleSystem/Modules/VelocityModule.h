#ifndef SHURIKENMODULEVELOCITY_H
#define SHURIKENMODULEVELOCITY_H

#include "ParticleSystemModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemCurves.h"
#include "Runtime/Math/Random/rand.h"

class VelocityModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (VelocityModule)
	VelocityModule ();

	void Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex);
	void UpdateProcedural (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps);
	void CalculateProceduralBounds(MinMaxAABB& bounds, const Matrix4x4f& localToWorld, float maxLifeTime);
	void CheckConsistency() {};

	inline MinMaxCurve& GetXCurve() { return m_X; };
	inline MinMaxCurve& GetYCurve() { return m_Y; };
	inline MinMaxCurve& GetZCurve() { return m_Z; };

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);
	
private:
	MinMaxCurve m_X;
	MinMaxCurve m_Y;
	MinMaxCurve m_Z;
	bool m_InWorldSpace; 

	Rand	m_Random;
};


#endif // SHURIKENMODULEVELOCITY_H
