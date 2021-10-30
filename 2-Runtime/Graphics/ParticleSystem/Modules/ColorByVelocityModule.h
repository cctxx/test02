#ifndef SHURIKENMODULECOLORBYVELOCITY_H
#define SHURIKENMODULECOLORBYVELOCITY_H

#include "ParticleSystemModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemGradients.h"
#include "Runtime/Math/Vector2.h"

class ColorBySpeedModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (ColorBySpeedModule)
	ColorBySpeedModule ();

	void Update (const ParticleSystemParticles& ps, ColorRGBA32* colorTemp, size_t fromIndex, size_t toIndex);
	void CheckConsistency ();

	inline MinMaxGradient& GetGradient() { return m_Gradient; };

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

private:
	MinMaxGradient m_Gradient;
	Vector2f m_Range;
};

#endif // SHURIKENMODULECOLORBYVELOCITY_H
