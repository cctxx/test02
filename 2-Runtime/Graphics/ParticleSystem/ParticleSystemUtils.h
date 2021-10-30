#ifndef SHURIKENUTILS_H
#define SHURIKENUTILS_H

#include "ParticleSystemCommon.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Random/Random.h"

class Matrix4x4f;
struct ParticleSystemParticles;
struct ParticleSystemReadOnlyState;
struct ParticleSystemState;
struct ParticleSystemEmissionState;
struct ParticleSystemSubEmitterData;

inline float InverseLerpFast01 (const Vector2f& scaleOffset, float v)
{
	return clamp01 (v * scaleOffset.x + scaleOffset.y);
}

inline float GenerateRandom(UInt32 randomIn)
{
	Rand rand(randomIn);
	return Random01(rand);
}

inline void GenerateRandom3(Vector3f& randomOut, UInt32 randomIn)
{
	Rand rand(randomIn);
	randomOut.x = Random01(rand);
	randomOut.y = Random01(rand);
	randomOut.z = Random01(rand);
}

inline UInt8 GenerateRandomByte (UInt32 seed)
{
	Rand rand (seed);
	return Rand::GetByteFromInt (rand.Get ());
}

UInt32 GetGlobalRandomSeed ();
void ResetGlobalRandomSeed ();
Vector2f CalculateInverseLerpOffsetScale (const Vector2f& range);
void KillParticle(const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, size_t index, size_t& particleCount);
void RecordEmit(ParticleSystemEmissionState& emissionState, const ParticleSystemSubEmitterData& subEmitterData, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, ParticleSystemSubType type, int subEmitterIndex, size_t particleIndex, float t, float dt, float length);
bool GetTransformationMatrix(Matrix4x4f& output, const bool isSystemInWorld, const bool isCurveInWorld, const Matrix4x4f& localToWorld);
bool GetTransformationMatrices(Matrix4x4f& output, Matrix4x4f& outputInverse, const bool isSystemInWorld, const bool isCurveInWorld, const Matrix4x4f& localToWorld);

#endif // SHURIKENUTILS_H
