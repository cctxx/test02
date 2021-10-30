#ifndef SHURIKENMODULESIZEBYVELOCITY_H
#define SHURIKENMODULESIZEBYVELOCITY_H

#include "ParticleSystemModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemCurves.h"
#include "Runtime/Math/Vector2.h"

class SizeBySpeedModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (SizeBySpeedModule)
	SizeBySpeedModule ();	

	void Update (const ParticleSystemParticles& ps, float* tempSize, size_t fromIndex, size_t toIndex);
	void CheckConsistency ();

	inline MinMaxCurve& GetCurve() { return m_Curve; }

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

private:
	MinMaxCurve m_Curve;
	Vector2f m_Range;
};

#endif // SHURIKENMODULESIZEBYVELOCITY_H
