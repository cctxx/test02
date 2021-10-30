#pragma once
#include "ParticleSystemModule.h"
#include "Runtime/BaseClasses/BitField.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Utilities/SpatialHash.h"

struct ParticleSystemParticles;
class Transform;

class CollisionModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (CollisionModule)
	CollisionModule ();

	void AllocateAndCache(const ParticleSystemReadOnlyState& roState, ParticleSystemState& state);
	static void FreeCache(ParticleSystemState& state);

#if UNITY_EDITOR	
	float GetEnergyLoss() const { return m_EnergyLossOnCollision; }
	void SetEnergyLoss(float value) { m_EnergyLossOnCollision = value; };
	float GetMinKillSpeed() const { return m_MinKillSpeed; }
	void SetMinKillSpeed(float value) { m_MinKillSpeed = value; };	
#endif

	bool GetUsesCollisionMessages () const {return m_CollisionMessages;}
	bool IsWorldCollision() const {return m_Type==kWorldCollision;}
	bool IsApproximate() const {return m_Quality>0;}
	int GetQuality() const {return m_Quality;}

	void Update (const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, size_t fromIndex, float dt);

	void CheckConsistency ();

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

private:
	void ClearPrimitives();
	
	enum { kPlaneCollision, kWorldCollision };
	enum { kMaxNumPrimitives = 6 };

	PlaneColliderCache m_ColliderCache;

	// Serialized
	int m_Type;
	float m_Dampen;
	float m_Bounce;
	float m_EnergyLossOnCollision;
	float m_MinKillSpeed;
	float m_ParticleRadius;
	/// Collides the particles with every collider whose layerMask & m_CollidesWith != 0
	BitField m_CollidesWith;
	/// Perform approximate world particle collisions
	int m_Quality; // selected quality, 0 is high (no approximations), 1 is medium (approximate), 2 is low (approximate)
	float m_VoxelSize;
	bool m_CollisionMessages;
	PPtr<Transform> m_Primitives [kMaxNumPrimitives];
};

