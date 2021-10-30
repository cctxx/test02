#ifndef SHURIKENMODULEFORCE_H
#define SHURIKENMODULEFORCE_H

#include "ParticleSystemModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemCurves.h"
#include "Runtime/Math/Random/rand.h"

class ForceModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (ForceModule)
	ForceModule ();

	void Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, float dt);
	void UpdateProcedural  (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps);
	void CalculateProceduralBounds(MinMaxAABB& bounds, const Matrix4x4f& localToWorld, float maxLifeTime);
	void CheckConsistency() {};

	inline MinMaxCurve& GetXCurve() { return m_X; }
	inline MinMaxCurve& GetYCurve() { return m_Y; }
	inline MinMaxCurve& GetZCurve() { return m_Z; }
	inline bool GetRandomizePerFrame() { return m_RandomizePerFrame; }
	
	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

private:
	MinMaxCurve m_X;
	MinMaxCurve m_Y;
	MinMaxCurve m_Z;
	bool m_InWorldSpace;
	bool m_RandomizePerFrame;
	Rand m_Random;
};

#endif // SHURIKENMODULEFORCE_H
