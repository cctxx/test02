#ifndef SHURIKENMODULESIZE_H
#define SHURIKENMODULESIZE_H

#include "ParticleSystemModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemCurves.h"

class SizeModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (SizeModule)	
	
	SizeModule();
	
	void Update (const ParticleSystemParticles& ps, float* tempSize, size_t fromIndex, size_t toIndex);

	void CheckConsistency () {};

	inline MinMaxCurve& GetCurve() { return m_Curve; }

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);
	
private:	
	MinMaxCurve m_Curve;
};

#endif // SHURIKENMODULESIZE_H
