#ifndef WORLDPARTICLECOLLIDER_H
#define WORLDPARTICLECOLLIDER_H

#include "Runtime/BaseClasses/GameObject.h"
#include "ParticleStruct.h"



class WorldParticleCollider : public Unity::Component
{
public:
	REGISTER_DERIVED_CLASS (WorldParticleCollider, Unity::Component)
	DECLARE_OBJECT_SERIALIZE (WorldParticleCollider)

	WorldParticleCollider (MemLabelId label, ObjectCreationMode mode);
		
	virtual void Reset ();

	void UpdateParticleCollider( ParticleArray& particles, PrivateParticleInfo& privateInfo, float deltaTime );

private:
	/// The velocity at which a particle is killed after the bounced velocity is calculated
	float				m_MinKillVelocity;
	float				m_BounceFactor;
	/// Seconds of energy a particle loses when colliding
	float				m_CollisionEnergyLoss;
	/// Collides the particles with every collider whose layerMask & m_CollidesWith != 0
	BitField			m_CollidesWith;
	/// Should we send out a collision message for every particle that has collided?
	bool				m_SendCollisionMessage;
};

#endif
