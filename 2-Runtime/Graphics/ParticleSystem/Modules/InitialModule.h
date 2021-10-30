#ifndef SHURIKENMODULEINITIAL_H
#define SHURIKENMODULEINITIAL_H

#include "ParticleSystemModule.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemCurves.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemGradients.h"
#include "Runtime/Math/Random/rand.h"

struct ParticleSystemState;

class InitialModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (InitialModule)
	InitialModule ();

	void Start (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const Matrix4x4f& matrix, size_t fromIndex, float t);
	void Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, float dt) const;
	void GenerateProcedural (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const ParticleSystemEmitReplay& emit);
	void CheckConsistency ();
	void AwakeFromLoad (ParticleSystem* system, const ParticleSystemReadOnlyState& roState);
	void ResetSeed(const ParticleSystemReadOnlyState& roState);

	inline MinMaxCurve& GetLifeTimeCurve() { return m_Lifetime; }
	inline const MinMaxCurve& GetLifeTimeCurve() const { return m_Lifetime; }
	inline MinMaxCurve& GetSpeedCurve() { return m_Speed; }
	inline const MinMaxCurve& GetSpeedCurve() const { return m_Speed; }
	inline MinMaxCurve& GetSizeCurve() { return m_Size; }
	inline const MinMaxCurve& GetSizeCurve() const { return m_Size; }
	inline MinMaxCurve& GetRotationCurve() { return m_Rotation; }
	inline const MinMaxCurve& GetRotationCurve() const { return m_Rotation; }
	inline MinMaxGradient& GetColor() { return m_Color; }
	inline const MinMaxGradient& GetColor() const { return m_Color; }
	inline void SetGravityModifier(float value) { m_GravityModifier = value; }
	inline float GetGravityModifier() const { return m_GravityModifier; }

	inline void SetMaxNumParticles(int value) { m_MaxNumParticles = value; }
	inline int GetMaxNumParticles() const { return m_MaxNumParticles; }
	inline float GetInheritVelocity() const { return m_InheritVelocity; }
	Vector3f GetGravity (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state) const;

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

private:
	Rand& GetRandom();
	
	MinMaxCurve m_Lifetime;
	MinMaxCurve m_Speed;
	MinMaxGradient m_Color;
	MinMaxCurve m_Size;
	MinMaxCurve m_Rotation;
	float m_GravityModifier;
	float m_InheritVelocity;
	int m_MaxNumParticles;
	
	Rand m_Random;
	
#if UNITY_EDITOR
public:
	Rand m_EditorRandom;
#endif
};


#endif // SHURIKENMODULEINITIAL_H
