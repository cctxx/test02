#ifndef SHURIKENPARTICLE_H
#define SHURIKENPARTICLE_H

#include "Runtime/Graphics/ParticleSystem/ParticleCollisionEvents.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/dynamic_array.h"

class Collider;
// Keep in sync with struct ParticleSystem.Particle
enum{ kParticleSystemMaxNumEmitAccumulators = 2 };

// TODO: Optimization:
// . Store startLifetime as 1.0f/startLifeTime and store lifetime as 1.0f - lifetime.
//   This means that a lot of fdivs turn into muls and mads turns into nothing (NormalizedTime will become cheaper).
//   Remember: script must still convert into legacy format.

// Keep in sync with struct ParticleSystem.Particle
struct ParticleSystemParticle
{
	Vector3f		position;
	Vector3f		velocity;
	Vector3f		animatedVelocity;
	Vector3f		axisOfRotation;
	float			rotation;
	float			rotationalSpeed;
	float			size;
	ColorRGBA32		color;
	UInt32			randomSeed;
	float			lifetime;
	float			startLifetime;
	float			emitAccumulator[kParticleSystemMaxNumEmitAccumulators];
};

typedef dynamic_array<Vector3f> ParticleSystemVector3Array;
typedef dynamic_array<float> ParticleSystemFloatArray;
typedef dynamic_array<ColorRGBA32> ParticleSystemColor32Array;
typedef dynamic_array<UInt32> ParticleSystemUInt32Array;

// Keep in sync with struct ParticleSystem.Particle
struct ParticleSystemParticles
{	
	ParticleSystemParticles()
	:numEmitAccumulators(0)
	,usesAxisOfRotation(false)
	,usesRotationalSpeed(false)
	,usesCollisionEvents(false)
	,currentCollisionEventThreadArray(0)
	{}

	ParticleSystemVector3Array position;
	ParticleSystemVector3Array velocity;
	ParticleSystemVector3Array animatedVelocity; // Would actually only need this when modules with force and velocity curves are used
	ParticleSystemVector3Array axisOfRotation;
	ParticleSystemFloatArray rotation;
	ParticleSystemFloatArray rotationalSpeed;
	ParticleSystemFloatArray size;
	ParticleSystemColor32Array color;
	ParticleSystemUInt32Array randomSeed;
	ParticleSystemFloatArray lifetime;
	ParticleSystemFloatArray startLifetime;
	ParticleSystemFloatArray emitAccumulator[kParticleSystemMaxNumEmitAccumulators]; // Usage: Only needed if particle system has time sub emitter
	CollisionEvents collisionEvents;

	bool usesAxisOfRotation;
	bool usesRotationalSpeed;
	bool usesCollisionEvents;
	int currentCollisionEventThreadArray;
	int numEmitAccumulators;

	void AddParticle(ParticleSystemParticle* particle);

	void SetUsesAxisOfRotation ();
	void SetUsesRotationalSpeed();
	
	void											SetUsesCollisionEvents(bool usesCollisionEvents);
	bool											GetUsesCollisionEvents() const;
	void											SetUsesEmitAccumulator (int numAccumulators);

	static size_t GetParticleSize();

	size_t array_size () const;	
	void array_resize (size_t i);
	void element_swap(size_t i, size_t j);
	void element_assign(size_t i, size_t j);
	void array_assign_external(void* data, const int numParticles);
	void array_merge_preallocated(const ParticleSystemParticles& rhs, const int offset, const bool needAxisOfRotation, const bool needEmitAccumulator);
	void array_assign(const ParticleSystemParticles& rhs);
	static void array_lerp(ParticleSystemParticles& output, const ParticleSystemParticles& a, const ParticleSystemParticles& b, float factor);

	void CopyFromArrayAOS(ParticleSystemParticle* particles, int size);
	void CopyToArrayAOS(ParticleSystemParticle* particles, int size);
};

struct ParticleSystemParticlesTempData
{
	ParticleSystemParticlesTempData();
	void element_swap(size_t i, size_t j);

	ColorRGBA32* color;
	float* size;
	float* sheetIndex;
	size_t particleCount;
};

inline float NormalizedTime (const ParticleSystemParticles& ps, size_t i)
{
	return (ps.startLifetime[i] - ps.lifetime[i]) / ps.startLifetime[i];
}

inline float NormalizedTime (float wholeTime, float currentTime)
{
	return (wholeTime - currentTime) / wholeTime;
}

#endif // SHURIKENPARTICLE_H
