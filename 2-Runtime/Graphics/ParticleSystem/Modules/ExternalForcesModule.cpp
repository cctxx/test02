#include "UnityPrefix.h"
#include "ExternalForcesModule.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "../ParticleSystemUtils.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Geometry/Sphere.h"
#include "Runtime/Terrain/Wind.h"
#include "Runtime/Input/TimeManager.h"


void ApplyRadialForce(ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, const Vector3f position, const float radius, const float force, const float dt)
{
	for(int q = fromIndex; q < toIndex; q++)
	{
		Vector3f toForce = position - ps.position[q];
		float distanceToForce = Magnitude(toForce);
		toForce = NormalizeSafe(toForce);
		float forceFactor = clamp(distanceToForce/radius, 0.0f, 1.0f);
		forceFactor = 1.0f - forceFactor * forceFactor;
		Vector3f delta = toForce * force * forceFactor * dt;
		ps.velocity[q] += delta;
	}
}

void ApplyDirectionalForce(ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, const Vector3f position, const Vector3f direction, const float radius, const float force, const float dt)
{
	for(int q = fromIndex; q < toIndex; q++)
	{
		Vector3f delta = direction * force * dt;
		ps.velocity[q] += delta;
	}
}

ExternalForcesModule::ExternalForcesModule () : ParticleSystemModule(false)
,	m_Multiplier (1.0f)
{
}

void ExternalForcesModule::Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, float dt)
{
	Matrix4x4f matrix = Matrix4x4f::identity;
	if(roState.useLocalSpace)
		Matrix4x4f::Invert_General3D(state.localToWorld, matrix);

	AABB aabb = state.minMaxAABB;
	for(int i = 0; i < state.numCachedForces; i++)
	{
		const ParticleSystemExternalCachedForce& cachedForce = state.cachedForces[i];
		const Vector3f position = matrix.MultiplyPoint3(cachedForce.position);
		const Vector3f direction = matrix.MultiplyVector3(cachedForce.direction);
		const float radius = cachedForce.radius;
		const float force = cachedForce.forceMain * m_Multiplier;

		const WindZone::WindZoneMode forceType = (WindZone::WindZoneMode)cachedForce.forceType;
		if(WindZone::Spherical == forceType)
		{
			Sphere sphere (position, radius);
			if(!IntersectAABBSphere (aabb, sphere))
				continue;
			ApplyRadialForce(ps, fromIndex, toIndex, position, radius, force, dt);
		}
		else if(WindZone::Directional == forceType)
			ApplyDirectionalForce(ps, fromIndex, toIndex, position, direction, radius, force, dt);
	}
}

// TODO: Perform culling here, instead of caching all of them
void ExternalForcesModule::AllocateAndCache(const ParticleSystemReadOnlyState& roState, ParticleSystemState& state)
{
	float time = GetTimeManager ().GetTimeSinceLevelLoad ();
	WindManager::WindZoneList& windZones = WindManager::GetInstance().GetList();

	state.numCachedForces = 0;
	for (WindManager::WindZoneList::iterator it = windZones.begin (); it != windZones.end (); ++it)
		state.numCachedForces++;

	// Allocate
	state.cachedForces = ALLOC_TEMP_MANUAL(ParticleSystemExternalCachedForce, state.numCachedForces);

	// Cache
	int i = 0;
	for (WindManager::WindZoneList::iterator it = windZones.begin (); it != windZones.end (); ++it)
	{
		const WindZone& zone = **it;
		ParticleSystemExternalCachedForce& cachedForce = state.cachedForces[i++];
		cachedForce.position = zone.GetComponent (Transform).GetPosition();
		cachedForce.direction = zone.GetComponent (Transform).GetLocalToWorldMatrix().GetAxisZ();
		cachedForce.forceType = zone.GetMode();
		cachedForce.radius = zone.GetRadius();

		float phase = time * kPI * zone.GetWindPulseFrequency();
		float pulse = (cos (phase) + cos (phase * 0.375f) + cos (phase * 0.05f)) * 0.333f;
		pulse = 1.0f + (pulse * zone.GetWindPulseMagnitude());
		cachedForce.forceMain = zone.GetWindMain() * pulse;
		// Maybe implement using turbulence and time based phasing?
		// ForceTurbulenceMultiply (maybe) // @TODO: Figure out what to do about turbulence. Do perlin field? Expensive but maybe cool! If use it: only do it when turbulence force is set to something

		//cachedForce.force = 1.0f;
	}
}

void ExternalForcesModule::FreeCache(ParticleSystemState& state)
{
	if(state.cachedForces)
		FREE_TEMP_MANUAL(state.cachedForces);
	state.cachedForces = NULL;
	state.numCachedForces = 0;
}

template<class TransferFunction>
void ExternalForcesModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_Multiplier, "multiplier");
}
INSTANTIATE_TEMPLATE_TRANSFER(ExternalForcesModule)
