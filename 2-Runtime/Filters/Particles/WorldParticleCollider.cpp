#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "WorldParticleCollider.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Geometry/Ray.h"
#include "Runtime/Input/TimeManager.h"
#include "ParticleStruct.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Interfaces/IRaycast.h"

using namespace std;

#pragma message ("Support collides with")
///@TODO: SUPPORT COLLIDES WITH

WorldParticleCollider::WorldParticleCollider (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

WorldParticleCollider::~WorldParticleCollider ()
{
}

void WorldParticleCollider::Reset ()
{
	Super::Reset ();
	m_BounceFactor = 0.5;
	m_MinKillVelocity = 0.0F;
	m_CollisionEnergyLoss = 0.0F;
	
	m_CollidesWith.m_Bits = -1;
	m_SendCollisionMessage = false;
}

IMPLEMENT_CLASS (WorldParticleCollider)
IMPLEMENT_OBJECT_SERIALIZE (WorldParticleCollider)

template<class T> inline
void WorldParticleCollider::Transfer (T& transfer)
{
	Super::Transfer (transfer);
	TRANSFER (m_BounceFactor);
	TRANSFER (m_CollisionEnergyLoss);
	TRANSFER (m_CollidesWith);
	TRANSFER (m_SendCollisionMessage);
	transfer.Align();
	TRANSFER (m_MinKillVelocity);
}

void WorldParticleCollider::UpdateParticleCollider( ParticleArray& particles, PrivateParticleInfo& privateInfo, float deltaTime )
{
	int particleCount = particles.size ();

	float sqrMinKillVelocity = m_MinKillVelocity * m_MinKillVelocity;
	// not sure if multiplication of infinity works on every platform
	AssertIf (std::numeric_limits<float>::infinity () * std::numeric_limits<float>::infinity () != std::numeric_limits<float>::infinity ()); 

	if (privateInfo.useWorldSpace)
	{
		for (int i=0;i<particleCount;i++)
		{
			Vector3f& position = particles[i].position;
			Vector3f& velocity = particles[i].velocity;
			Vector3f delta = velocity * deltaTime;
			
			float psize		= 0.5f  * particles[i].size;
			float poffset	= 0.51f * particles[i].size;
			
			Ray ray;
			ray.SetOrigin (position - delta);
			float deltaLength = Magnitude (delta);
			if (deltaLength < Vector3f::epsilon)
				continue;			

			ray.SetDirection (delta / deltaLength);
			
			float checkLen	= deltaLength + psize;
			float t			= checkLen;
			
			#if ENABLE_PHYSICS
			HitInfo hit;
			IRaycast *raycast = GetRaycastInterface();
			if(raycast)
			{
				if(raycast->Raycast(ray, t, m_CollidesWith.m_Bits, hit))
				{
					Particle& particle = particles[i];

					// Reflect velocity and apply velocity * time left for particle to position
					// Test and factor changes into other particle collider
					velocity = ReflectVector (m_BounceFactor * velocity, hit.normal);
					float fractionLeftForReflection = (checkLen - t) / deltaLength;
				
					// Place particle slightly above the surface. Otherwise in next frame raycast can
					// detect collision again and reflect particle back!
					position = hit.intersection + hit.normal * poffset + velocity * (deltaTime * fractionLeftForReflection);
				
					if (m_SendCollisionMessage)
					{
						AssertIf (hit.colliderInstanceID == 0);
						PPtr<Component> collider_pptr(hit.colliderInstanceID);
						Component* collider = collider_pptr;
						SendMessage (kParticleCollisionEvent, &collider->GetGameObject (), ClassID (GameObject));
						collider->SendMessage (kParticleCollisionEvent, &GetGameObject (), ClassID (GameObject));
					}
					// Update energy
					particle.energy -= m_CollisionEnergyLoss;
					float sqrVelocity = SqrMagnitude (velocity);
					if (particle.energy <= 0.0f || sqrVelocity < sqrMinKillVelocity) {
						// Kill particle (replace particle i with last, and continue updating the moved particle)
						KillParticle (particles, i);
						particleCount = particles.size ();
						i--;
						continue;
					}
					privateInfo.aabb.Encapsulate (position);
				}
			}

			#endif // ENABLE_PHYSICS
		}
	}
	else
	{
		Matrix4x4f localToWorld = GetComponent (Transform).GetLocalToWorldMatrixNoScale ();
		for (int i=0;i<particleCount;i++)
		{
			Vector3f& position = particles[i].position;
			Vector3f& velocity = particles[i].velocity;
			
			float psize		= 0.5f  * particles[i].size;
			float poffset	= 0.51f * particles[i].size;

			Vector3f worldSpaceDelta = localToWorld.MultiplyVector3 (velocity * deltaTime);
			Vector3f worldPosition = localToWorld.MultiplyPoint3 (position);
			
			Ray ray;
			ray.SetOrigin (worldPosition - worldSpaceDelta);
			float deltaLength = Magnitude (worldSpaceDelta);
			if (deltaLength < Vector3f::epsilon)
				continue;			

			ray.SetDirection (worldSpaceDelta / deltaLength);
			
			float checkLen	= deltaLength + psize;
			float t			= checkLen;

			#if ENABLE_PHYSICS
			HitInfo hit;
			IRaycast *raycast = GetRaycastInterface();
			if(raycast)
			{
				if(raycast->Raycast(ray, t, m_CollidesWith.m_Bits, hit))
				{
					Particle& particle = particles[i];

					// Reflect velocity and apply velocity * time left for particle to position
					Vector3f worldVelocity = localToWorld.MultiplyVector3 (velocity);
					// Test and factor changes into other particle collider
					worldVelocity = ReflectVector (m_BounceFactor * worldVelocity, hit.normal);
					float fractionLeftForReflection = (checkLen - t) / deltaLength;
				
					// Place particle slightly above the surface. Otherwise in next frame raycast can
					// detect collision again and reflect particle back!
					worldPosition = hit.intersection + hit.normal * poffset + worldVelocity * (deltaTime * fractionLeftForReflection);
				
					position = localToWorld.InverseMultiplyPoint3Affine (worldPosition);
					velocity = localToWorld.InverseMultiplyVector3Affine (worldVelocity);
				
					if (m_SendCollisionMessage)
					{
						AssertIf (hit.colliderInstanceID == 0);
						PPtr<Component> collider_pptr(hit.colliderInstanceID);
						Component* collider = collider_pptr;
						SendMessage (kParticleCollisionEvent, &collider->GetGameObject (), ClassID (GameObject));
						collider->SendMessage (kParticleCollisionEvent, &GetGameObject (), ClassID (GameObject));
					}
				
					// Update energy
					particle.energy -= m_CollisionEnergyLoss;
					float sqrVelocity = SqrMagnitude (velocity);
					if (particle.energy <= 0.0f || sqrVelocity < sqrMinKillVelocity) {
						// Kill particle (replace particle i with last, and continue updating the moved particle)
						KillParticle (particles, i);
						particleCount = particles.size ();
						i--;
						continue;
					}
				
					privateInfo.aabb.Encapsulate (position);
				}
			}
			#endif // ENABLE_PHYSICS
		}
	}
}
