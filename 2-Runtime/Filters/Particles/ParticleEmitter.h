#ifndef PARTICLEEMITTER_H
#define PARTICLEEMITTER_H

#include "Runtime/BaseClasses/GameObject.h"
#include "ParticleStruct.h"
#include "Runtime/Utilities/LinkedList.h"


class Rand;

#define GET_SET_DIRTY_MIN(TYPE,PROP_NAME,VAR_NAME,MIN_VAL) void Set##PROP_NAME (TYPE val) { VAR_NAME = std::max(val,MIN_VAL); SetDirty(); } TYPE Get##PROP_NAME () const { return (TYPE)VAR_NAME; }



class ParticleEmitter : public Unity::Component
{
public:
	REGISTER_DERIVED_ABSTRACT_CLASS (ParticleEmitter, Unity::Component)
	DECLARE_OBJECT_SERIALIZE (ParticleEmitter)

	ParticleEmitter (MemLabelId label, ObjectCreationMode mode);
	
	void SetEmit (bool emit);
	bool IsEmitting () const { return m_Emit; }
	
	GET_SET_DIRTY_MIN (float, MinSize, m_MinSize, 0.0f);
	GET_SET_DIRTY_MIN (float, MaxSize, m_MaxSize, 0.0f);
	GET_SET_DIRTY_MIN (float, MinEnergy, m_MinEnergy, 0.0f);
	GET_SET_DIRTY_MIN (float, MaxEnergy, m_MaxEnergy, 0.0f);
	GET_SET_DIRTY_MIN (float, MinEmission, m_MinEmission, 0.0f);
	GET_SET_DIRTY_MIN (float, MaxEmission, m_MaxEmission, 0.0f);

	GET_SET_DIRTY (float, EmitterVelocityScale, m_EmitterVelocityScale);
	GET_SET_DIRTY (Vector3f, WorldVelocity, m_WorldVelocity);
	GET_SET_DIRTY (Vector3f, LocalVelocity, m_LocalVelocity);
	GET_SET_DIRTY (Vector3f, RndVelocity, m_RndVelocity);
	GET_SET_DIRTY (bool, RndRotation, m_RndInitialRotations);
	GET_SET_DIRTY (float, AngularVelocity, m_AngularVelocity);

	float GetRndAngularVelocity () const { return m_RndAngularVelocity; }
	void SetRndAngularVelocity (float val) { if (val < 0) val = 0; m_RndAngularVelocity = val; SetDirty(); }
	
	bool GetUseWorldSpace () const {	return m_UseWorldSpace; }
	void SetUseWorldSpace (bool val) { m_UseWorldSpace = val; } // TODO: set dirty?
	
	// Emit particleCount particles
	void Emit (unsigned int particleCount, float invDeltaTime);
	
	// This is needed, so that subsequent calls to Emit() will each only emit at the current location, and not along the line
	// between the current and last location.
	void EmitResetEmitterPos (int particleCount, float invDeltaTime) { ResetEmitterPos(); Emit (particleCount, invDeltaTime); }

	// Emit one particle
	void Emit (const Vector3f &pos, const Vector3f &dir, float size, float energy, const ColorRGBA32 &color, float rotation, float angularVelocity);
	
	void Deactivate (DeactivateOperation operation);
	void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	// clear all particles.
	void ClearParticles ();
	
	void ReadParticles(  SimpleParticle*__restrict particle, int baseIndex, int count ) const;
	void WriteParticles( const SimpleParticle* __restrict particle, int count );
	
	int GetParticleCount() const { return m_Particles.size(); }
	
	static void UpdateAllParticleSystems();
	void UpdateParticleSystem(float deltaTime);

	const ParticleArray& GetParticles() const { return m_Particles; }
	ParticleArray& GetParticles() { return m_Particles; }
	const PrivateParticleInfo& GetPrivateInfo() const { return m_PrivateInfo; }

	bool GetEnabled () const { return m_Enabled; } 
	void SetEnabled (bool enabled);

	static void InitializeClass ();
	static void CleanupClass ();
	
protected:
	// Sets "does this emitter have to be updated next frame" flags.
	void UpdateManagerState( bool updateParticles );

private:
	// Subclasses have to override this function to place particles
	virtual void SetupParticles (ParticleArray& particles, const Vector3f& velocityOffset,
			const Matrix3x3f& rotation, int firstIndex) = 0;

	// Emit particles
	void TimedEmit (float deltaTime);
	
	void ResetEmitterPos	();
	void CalcOffsets (Vector3f *velocityOffset, Matrix3x3f *localRotation, float invDeltaTime);

protected:
	Vector3f            m_EmitterPos;
	Vector3f            m_PreviousEmitterPos;

protected:
	ParticleArray       m_Particles;
	PrivateParticleInfo m_PrivateInfo;
	float               m_EmissionFrac;

	float		m_MinSize; ///< minimum size range {0, infinity }
	float		m_MaxSize; ///< maximum size range {0, infinity }
	float		m_MinEnergy; ///< minimum energy range { 0, infinity }
	float		m_MaxEnergy; ///< maximum energy	range { 0, infinity }

	float		m_MinEmission; ///< minimum emissions per second range { 0, infinity }
	float		m_MaxEmission; ///< maximum emissions per second range { 0, infinity }

	float		m_EmitterVelocityScale;///< Scales velocity of the emitter 

	Vector3f	m_WorldVelocity;

	Vector3f	m_LocalVelocity;

	Vector3f    m_TangentVelocity;

	Vector3f	m_RndVelocity;		

	bool		m_UseWorldSpace; ///< Are particles simulated in WorldSpace or in LocalSpace?

	bool		m_RndInitialRotations; ///< Should initial particle rotations be randomized?
	float		m_RndAngularVelocity;
	float		m_AngularVelocity;
	
	bool        m_Enabled;
	bool        m_Emit;
	bool        m_OneShot;
	bool        m_FirstFrame;
	
protected:

	void InitParticleEnergy(Rand& r, Particle& p, float dt);
		
	#if DOXYGEN
	///////// @TODO: DO EASIER BACKWARDS COMPATIBILITY: THEN WE DONT NEED THIS CRAP
	float minSize; ///< minimum size range {0, infinity }
	float maxSize; ///< minimum size range {0, infinity }
	float minEnergy; ///< minimum size range {0, infinity }
	float maxEnergy; ///< minimum size range {0, infinity }
	float minEmission; ///< minimum size range {0, infinity }
	float maxEmission; ///< minimum size range {0, infinity }
	#endif

private:
	ListNode<ParticleEmitter> m_EmittersListNode;
};

#endif
