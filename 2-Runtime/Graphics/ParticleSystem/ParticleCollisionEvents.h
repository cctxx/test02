#pragma once

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Utilities/dynamic_array.h"


struct ParticleCollisionEvent
{
					ParticleCollisionEvent (const Vector3f& intersection, const Vector3f& normal, const Vector3f& velocity, int colliderInstanceID, int rigidBodyOrColliderInstanceID);	
	Vector3f		m_Intersection;
	Vector3f		m_Normal;
	Vector3f		m_Velocity;
	int				m_ColliderInstanceID;
	int				m_RigidBodyOrColliderInstanceID; // This can be a Collider or a RigidBody component
};

struct MonoParticleCollisionEvent
{
	Vector3f			m_Intersection;
	Vector3f			m_Normal;
	Vector3f			m_Velocity;
	int					m_ColliderInstanceID;
};

struct CollisionEvents
{	
													CollisionEvents ();
	dynamic_array<ParticleCollisionEvent>			collisionEvents[2];
	int												currentCollisionEventThreadArray;
	
	void											Clear ();
	bool											AddEvent (const ParticleCollisionEvent& event);
	int												GetCollisionEventCount () const;
	void											SwapCollisionEventArrays ();
	void											SortCollisionEventThreadArray ();
	void											SendCollisionEvents (Unity::Component& particleSystem) const;
	int												GetCollisionEvents (int instanceId, MonoParticleCollisionEvent* collisionEvents, int size) const;
	dynamic_array<ParticleCollisionEvent>&			GetCollisionEventThreadArray ();
	const dynamic_array<ParticleCollisionEvent>&	GetCollisionEventScriptArray () const;
};
