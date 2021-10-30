#ifndef PERLINNOISE_H
#define PERLINNOISE_H
class PerlinNoise {
public:
	static float Noise(float x, float y);
	// Returns noise between 0 - 1
	inline static float NoiseNormalized(float x, float y)
	{
		//-0.697 - 0.795 + 0.697
		float value = Noise(x, y);
		value = (value + 0.69F) / (0.793F + 0.69F);
		return value;
	}
};

#endif
