#ifndef ELLIPSOIDPARTICLEEMITTER_H
#define ELLIPSOIDPARTICLEEMITTER_H

#include "ParticleEmitter.h"
#include "Runtime/Math/Vector3.h"
#include "ParticleStruct.h"

class Matrix4x4f;



class EllipsoidParticleEmitter : public ParticleEmitter
{
public:
	REGISTER_DERIVED_CLASS (EllipsoidParticleEmitter, ParticleEmitter)
	DECLARE_OBJECT_SERIALIZE (EllipsoidParticleEmitter)
	EllipsoidParticleEmitter (MemLabelId label, ObjectCreationMode mode);
	
	static void InitializeClass ();
	static void CleanupClass ();
	
private:
		
	void SetupParticle (Particle& p, const Vector3f& velocityOffset, const Matrix3x3f& rotation, float deltaTime);
	virtual void SetupParticles (ParticleArray& particles, const Vector3f& velocityOffset,
			const Matrix3x3f& rotation, int firstIndex);
		
public:
	Vector3f	m_Ellipsoid;	///< Size of emission area
	float		m_MinEmitterRange;///< [0...1] relative range to maxEmitterSize where particles will not be spawned
										// 0 means that a full ellipsoid will be filled with particles
										// 1 means only the outline will be filled with particles
};

#endif
