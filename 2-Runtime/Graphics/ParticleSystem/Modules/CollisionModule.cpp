#include "UnityPrefix.h"
#include <float.h>
#include "CollisionModule.h"

#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemCurves.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemParticle.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemUtils.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Interfaces/IRaycast.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

#include "Editor/Src/Utility/DebugPrimitives.h"

struct ParticleSystemCollisionParameters
{	
	float bounceFactor;
	float energyLossOnCollision;
	float minKillSpeedSqr;
	float particleRadius;
	float dampen;

	PlaneColliderCache* planeColliderCache;
	IRaycast* raycaster;
	size_t rayBudget;
	size_t nextParticleToTrace;
	float voxelSize;
};

struct ColliderInfo
{
	Plane m_CollisionPlane;
	bool m_Traced;
	int		m_ColliderInstanceID;
	int		m_RigidBodyOrColliderInstanceID;
};

struct CollisionInfo
{
	CollisionInfo():m_NumWorldCollisions(0),m_NumCachedCollisions(0),m_NumPlaneCollisions(0),m_Colliders(NULL) {}
	
	size_t m_NumWorldCollisions;
	size_t m_NumCachedCollisions;
	size_t m_NumPlaneCollisions;
	ColliderInfo* m_Colliders;

	size_t AllCollisions() const { return m_NumWorldCollisions+m_NumCachedCollisions+m_NumPlaneCollisions; }
};

/// @TODO: Why does Vector3f has a constructor? WTF.
inline void CalculateCollisionResponse(const ParticleSystemReadOnlyState& roState, 
									   ParticleSystemState& state, 
									   ParticleSystemParticles& ps, 
									   const size_t q, 
									   const ParticleSystemCollisionParameters& params,
									   const Vector3f& position,
									   const Vector3f& velocity,
									   const HitInfo& hitInfo)
{
	// Reflect + dampen
	Vector3f positionOffset = ReflectVector (position - hitInfo.intersection, hitInfo.normal) * params.dampen;
	Vector3f newVelocity = ReflectVector(velocity, hitInfo.normal) * params.dampen;
	
	// Apply bounce
	positionOffset -= hitInfo.normal * (Dot(positionOffset, hitInfo.normal)) * params.bounceFactor;
	newVelocity -= hitInfo.normal * Dot(newVelocity, hitInfo.normal) * params.bounceFactor;
	
	ps.position[q] = hitInfo.intersection + positionOffset;
	ps.velocity[q] = newVelocity - ps.animatedVelocity[q];
	
	for(int s = 0; s < state.numCachedSubDataCollision; s++)
	{
		ParticleSystemEmissionState emissionState;
		RecordEmit(emissionState, state.cachedSubDataCollision[s], roState, state, ps, kParticleSystemSubTypeCollision, s, q, 0.0f, 0.0001f, 1.0f);
	}
	ps.lifetime[q] -= params.energyLossOnCollision * ps.startLifetime[q];

	if (ps.GetUsesCollisionEvents () && !(hitInfo.colliderInstanceID == 0))
	{
		Vector3f wcIntersection = hitInfo.intersection;
		Vector3f wcNormal = hitInfo.normal;
		Vector3f wcVelocity = velocity;
		if ( roState.useLocalSpace )
		{
			wcIntersection = state.localToWorld.MultiplyPoint3 (wcIntersection);
			wcNormal = state.localToWorld.MultiplyVector3 (wcNormal);
			wcVelocity = state.localToWorld.MultiplyVector3 (wcVelocity);
		}
		ps.collisionEvents.AddEvent (ParticleCollisionEvent (wcIntersection, wcNormal, wcVelocity, hitInfo.colliderInstanceID, hitInfo.rigidBodyOrColliderInstanceID));
		//printf_console (Format ("Intersected '%s' -> id: %d -> go id: %d.\n", hitInfo.collider->GetName (), hitInfo.collider->GetInstanceID (), hitInfo.collider->GetGameObject ().GetInstanceID ()).c_str ());
}
}

CollisionModule::CollisionModule () : ParticleSystemModule(false)
,	m_Type(0)
,	m_Dampen(0.0f)
,	m_Bounce(1.0f)
,	m_EnergyLossOnCollision(0.0f)
,	m_MinKillSpeed(0.0f)
,	m_ParticleRadius(0.01f)
,	m_Quality(0)
,	m_VoxelSize(0.5f)
,	m_CollisionMessages(false)
{
	m_CollidesWith.m_Bits = 0xFFFFFFFF;
}

void CollisionModule::AllocateAndCache(const ParticleSystemReadOnlyState& roState, ParticleSystemState& state)
{
	Assert(!state.cachedCollisionPlanes);

	Matrix4x4f matrix = Matrix4x4f::identity;
	if(roState.useLocalSpace)
		matrix = state.worldToLocal;

	state.numCachedCollisionPlanes = 0;
	for (unsigned i=0; i<kMaxNumPrimitives; ++i)
	{
		if (m_Primitives[i].IsNull ())
			continue;
		state.numCachedCollisionPlanes++;
	}

	state.cachedCollisionPlanes = ALLOC_TEMP_MANUAL(Plane, state.numCachedCollisionPlanes);

	int planeCount = 0;

	for (unsigned i=0; i<kMaxNumPrimitives; ++i)
	{
		if (m_Primitives[i].IsNull ())
			continue;

		const Transform* transform = m_Primitives[i]->QueryComponent(Transform);
		const Vector3f position = matrix.MultiplyPoint3(transform->GetPosition());

		Assert(planeCount <= state.numCachedCollisionPlanes);
		const Vector3f normal = matrix.MultiplyVector3(RotateVectorByQuat (transform->GetRotation (), Vector3f::yAxis));			
		state.cachedCollisionPlanes[planeCount].SetNormalAndPosition (normal, position);
		state.cachedCollisionPlanes[planeCount].NormalizeRobust();
		planeCount++;
	}
}

void CollisionModule::FreeCache(ParticleSystemState& state)
{
	if(state.cachedCollisionPlanes)
	{
		FREE_TEMP_MANUAL(state.cachedCollisionPlanes);
		state.cachedCollisionPlanes = 0;
		state.numCachedCollisionPlanes = 0;
	}
}

// read the plane cache for a given range
size_t ReadCache(const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, const ParticleSystemParticles& ps, const ParticleSystemCollisionParameters& params, CollisionInfo& collision, const size_t fromIndex, const size_t& toIndex, const float dt)
{
	size_t numIntersections = 0;
	for (size_t q = fromIndex; q < toIndex; ++q)
	{
		// initialise plane to value that guarantees no intersection
		collision.m_Colliders[q].m_CollisionPlane.distance = FLT_MAX;
		collision.m_Colliders[q].m_Traced = false;
		collision.m_Colliders[q].m_ColliderInstanceID = -1;
		collision.m_Colliders[q].m_RigidBodyOrColliderInstanceID = -1;

		// build start/end points
		Vector3f to = ps.position[q];
		const Vector3f v = ps.velocity[q] + ps.animatedVelocity[q];
		const Vector3f d = v * dt;
		Vector3f from = to - d;
		// convert to WC
		if ( roState.useLocalSpace )
		{
			from = state.localToWorld.MultiplyPoint3(from);
			to = state.localToWorld.MultiplyPoint3(to);
		}

		// lookup the cache
		Plane plane;
		int colliderInstanceID;
		int rigidBodyOrColliderInstanceID;
		if ( params.planeColliderCache->Find (from, to-from, plane, colliderInstanceID, rigidBodyOrColliderInstanceID, params.voxelSize) )
		{
			collision.m_Colliders[q].m_CollisionPlane = plane;
			collision.m_Colliders[q].m_ColliderInstanceID = colliderInstanceID;
			collision.m_Colliders[q].m_RigidBodyOrColliderInstanceID = rigidBodyOrColliderInstanceID;
			numIntersections++;
		}
	}
	return numIntersections;
}

// Perform ray casts. Ray casts are done in WC and results are transformed back into sim space if necessary.
// There are three sets of indices:
// [fromIndex ; params.nextParticleToTrace[ for these we do caching if the cache is available
// [params.nextParticleToTrace ; params.nextParticleToTrace + params.rayBudget[ for these we trace fresh rays
// [params.nextParticleToTrace + params.rayBudget ; toIndex[ for these we do caching if the cache is available
CollisionInfo WorldCollision(const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, const ParticleSystemParticles& ps, const ParticleSystemCollisionParameters& params, const size_t fromIndex, const int filter, const float dt)
{	
	CollisionInfo collisionCounter;
	const bool approximate = ( params.planeColliderCache ? true : false );
	size_t numIntersections = 0;

	const size_t toIndex = ps.array_size();
	
	// pre range that is cached
	const size_t traceRangeFrom	= approximate ? params.nextParticleToTrace : fromIndex;
	const size_t traceRangeTo	= approximate ? std::min(params.nextParticleToTrace+params.rayBudget, toIndex) : toIndex;

	const size_t fromIndex0 = fromIndex;
	const size_t toIndex0 = traceRangeFrom;
	// range to actually ray trace
	const size_t fromIndex1 = traceRangeFrom;
	const size_t toIndex1 = traceRangeTo;
	// post range that is cached
	const size_t fromIndex2 = traceRangeTo;
	const size_t toIndex2 = toIndex;

	collisionCounter.m_Colliders = ALLOC_TEMP_MANUAL(ColliderInfo, ps.array_size());
	collisionCounter.m_NumCachedCollisions = (toIndex0-fromIndex0)+(toIndex2-fromIndex2);
	collisionCounter.m_NumWorldCollisions = toIndex1-fromIndex1;	

	if ( toIndex1-fromIndex1 > 0 )
	{
		// batch trace selected range
		dynamic_array< BatchedRaycast > rayBatch(toIndex1-fromIndex1,kMemTempAlloc);
		dynamic_array< BatchedRaycastResult > rayResults(toIndex1-fromIndex1,kMemTempAlloc);
		// build request array
		size_t i = 0;
		for (size_t q = fromIndex1; q < toIndex1; ++q, ++i)
		{
			// build start/end points
			const Vector3f to = ps.position[q];
			const Vector3f v = ps.velocity[q] + ps.animatedVelocity[q];
			const Vector3f d = v * dt;
			const Vector3f from = to - d;

			if ( approximate )
			{
				// extend ray to trace across the entire voxel (and then some)
				const float voxSize = std::max(params.voxelSize,params.voxelSize*kVoxelHeightMultiplier);
				const Vector3f displacement = to-from;
			
				///@TODO: handle magnitude 0. Should we skip the raycast???
				const Vector3f direction = NormalizeFast(displacement);
				const Vector3f extendedTo = from + direction * voxSize;

				// insert into batch
				rayBatch[i] = BatchedRaycast(q,from,extendedTo);
			}
			else
				rayBatch[i] = BatchedRaycast(q,from,to);

			// initialise plane to value that guarantees no intersection
			collisionCounter.m_Colliders[q].m_CollisionPlane.distance = FLT_MAX;
			collisionCounter.m_Colliders[q].m_Traced = (!approximate);
			collisionCounter.m_Colliders[q].m_ColliderInstanceID = -1;
			collisionCounter.m_Colliders[q].m_RigidBodyOrColliderInstanceID = -1;
		}		
		// convert to WC
		if ( roState.useLocalSpace )
		{
			const Matrix4x4f m = state.localToWorld;
			for (size_t i = 0; i < rayBatch.size(); ++i)
			{
				rayBatch[i].from = m.MultiplyPoint3(rayBatch[i].from);
				rayBatch[i].to = m.MultiplyPoint3(rayBatch[i].to);
			}
		}
		// trace the rays		
		const size_t numIx = params.raycaster->BatchIntersect( rayBatch, rayResults, filter, approximate );

		// convert back to local space
		if ( roState.useLocalSpace )
		{
			const Matrix4x4f m = state.worldToLocal;
			for (size_t i = 0; i < numIx; ++i)
			{
				// the plane intersection was computed in WC, transform intersection to local space.
				rayResults[i].hitInfo.intersection = m.MultiplyPoint3( rayResults[i].hitInfo.intersection );
				rayResults[i].hitInfo.normal = m.MultiplyVector3( rayResults[i].hitInfo.normal );
			}
		}

		// store planes in the particles that intersected something
		for (size_t i = 0; i < numIx; ++i)
		{
			const size_t q = rayBatch[rayResults[i].index].index;
			collisionCounter.m_Colliders[q].m_CollisionPlane.normal = rayResults[i].hitInfo.normal;
			collisionCounter.m_Colliders[q].m_CollisionPlane.distance = -Dot(rayResults[i].hitInfo.intersection, rayResults[i].hitInfo.normal);
			collisionCounter.m_Colliders[q].m_ColliderInstanceID = rayResults[i].hitInfo.colliderInstanceID;
			collisionCounter.m_Colliders[q].m_RigidBodyOrColliderInstanceID = rayResults[i].hitInfo.rigidBodyOrColliderInstanceID;
		}

		// store intersections in cache
		if (approximate)
		{
			for (size_t i = 0; i < numIx; ++i)
			{
				const size_t r = rayResults[i].index;
				const size_t q = rayBatch[rayResults[i].index].index;
				params.planeColliderCache->Replace (rayBatch[r].from, rayBatch[r].to-rayBatch[r].from, Plane(collisionCounter.m_Colliders[q].m_CollisionPlane), collisionCounter.m_Colliders[q].m_ColliderInstanceID, collisionCounter.m_Colliders[q].m_RigidBodyOrColliderInstanceID, params.voxelSize);
			}
		}
		numIntersections += numIx;
	}

	// process pre cache range
	if ( toIndex0-fromIndex0 > 0 )
	{
		numIntersections += ReadCache(roState, state, ps, params, collisionCounter, fromIndex0, toIndex0, dt);
	}
	// process post cache range
	if ( toIndex2-fromIndex2 > 0 )
	{
		numIntersections += ReadCache(roState, state, ps, params, collisionCounter, fromIndex2, toIndex2, dt);
	}

	return collisionCounter;	
}


// Plane collide all particles in simulation space (remember the cached planes are defined in sim space).
CollisionInfo PlaneCollision(const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, const ParticleSystemParticles& ps, const ParticleSystemCollisionParameters& params, const int fromIndex, const float dt)
{
	CollisionInfo collisionInfo;
	collisionInfo.m_Colliders = ALLOC_TEMP_MANUAL(ColliderInfo, ps.array_size());
	
	const bool newBehaviour = IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1);

	size_t toIndex = ps.array_size();
	for (size_t q = fromIndex; q < toIndex; ++q)
	{
		// initialise to value that guarantees no intersection
		collisionInfo.m_Colliders[q].m_CollisionPlane.distance = FLT_MAX;

		const Vector3f position = ps.position[q];
		const Vector3f velocity = ps.velocity[q] + ps.animatedVelocity[q];
		const Vector3f displacement = velocity * dt;
		const Vector3f origin = position - displacement;

		// walk the planes
		for (unsigned i=0; i<state.numCachedCollisionPlanes; ++i)
		{
			const Plane plane = state.cachedCollisionPlanes[i];

			if ( newBehaviour )
			{
				// new behaviour:
				// plane collisions are single sided only, all particles 'behind' the plane will be forced onto the plane
				const float dist = plane.GetDistanceToPoint(position);
				if (dist > params.particleRadius)
					continue;
			}
			else
			{
				// old (but fixed) behaviour, particles can collide with both front and back plane
				const float d0 = plane.GetDistanceToPoint(origin);
				const float d1 = plane.GetDistanceToPoint(position);
				const bool sameSide = ( (d0 > 0.0f && d1 > 0.0f) || (d0 <= 0.0f && d1 <= 0.0f) );
				const float aD0 = Abs(d0);
				const float aD1 = Abs(d1);
				if ( sameSide && aD0 > params.particleRadius && aD1 > params.particleRadius )
					continue;
			}

			collisionInfo.m_Colliders[q].m_CollisionPlane = plane;
			collisionInfo.m_Colliders[q].m_ColliderInstanceID = 0;
			collisionInfo.m_Colliders[q].m_RigidBodyOrColliderInstanceID = 0;
			collisionInfo.m_NumPlaneCollisions++;
			break;
		}
	}	
	
	return collisionInfo;
}

// Compute the collision plane to use for each particle
CollisionInfo UpdateCollisionPlanes(bool worldCollision, UInt32 collisionFilter, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, const ParticleSystemCollisionParameters& params, ParticleSystemParticles& ps, size_t fromIndex, float dt)
{
	if( worldCollision )
	{	
		// Check if we support raycasting
		if (params.raycaster == NULL)
			return CollisionInfo();
		
		////@TODO: dtPerParticle
		return WorldCollision(roState, state, ps, params, fromIndex, collisionFilter, dt);				
	}
	else
	{
		////@TODO: dtPerParticle
		return PlaneCollision(roState, state, ps, params, fromIndex, dt);
	}
}

// Collide the particles against their selected planes and update collision response - this is done purely in simulation space
static const float kEpsilon		= 0.000001f;
static const float kRayEpsilon	= 0.00001f;
void PerformPlaneCollisions(bool worldCollision, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, const ParticleSystemCollisionParameters& params, const CollisionInfo& collisionInfo, const int fromIndex, const float dt)
{
	// for world collisions we use and epsilon but for plane intersections we use the particle radius
	const float radius = worldCollision ? kRayEpsilon : params.particleRadius;
	const bool newBehaviour = IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1);
	//const bool approximate = ( params.planeColliderCache ? true : false );

	// collide with plane
	size_t toIndex = ps.array_size();

	for (size_t q = fromIndex; q < toIndex; q++)
	{			
		// check if the actual ray was traced in which case we want to disable the ray testing. Note: does not apply to approximate mode as we trace longer rays to improve cache quality
		const bool tracedParticle = collisionInfo.m_Colliders[q].m_Traced;
		const Plane plane = collisionInfo.m_Colliders[q].m_CollisionPlane;
		if ( plane.distance != FLT_MAX )
		{
			const Vector3f position = ps.position[q];
			const Vector3f velocity = ps.velocity[q] + ps.animatedVelocity[q];
			const Vector3f displacement = velocity * dt;
			const Vector3f origin = position - displacement;

			HitInfo hit;
			hit.normal = plane.GetNormal();
			hit.colliderInstanceID = collisionInfo.m_Colliders[q].m_ColliderInstanceID;
			hit.rigidBodyOrColliderInstanceID = collisionInfo.m_Colliders[q].m_RigidBodyOrColliderInstanceID;

			if ( worldCollision )
			{
				// plane ray dot product
				const float VdN = Dot( plane.GetNormal(), displacement );
				if ( !tracedParticle && VdN >= 0 )
					continue; // only pick up front face intersections

				// Recreate hit point.
				const float t = -( Dot( origin, plane.GetNormal() ) + plane.distance ) / VdN;
				if ( !tracedParticle && (t < 0 || t > 1) )
					continue;

				// build intersection description from t value and ray
				hit.intersection = origin + displacement * t;

				// Adjust intersection along normal to make sure the particle doesn't fall through geometry when it comes to rest.
				// This is also an issue when dampen and bounce is zero in which case CalculateCollisionResponse will set the particle
				// position to *exactly* the intersection point, which will then have issues next time the intersection is executed
				// where it will try and compare two floating point numbers which are equal and the intersection test will come out
				// either way depending on fp accuracy
				//
				// for world collisions we use and epsilon but for plane intersections we use the particle radius
				hit.intersection += radius * hit.normal;
			}
			else
			{
				if (newBehaviour)
				{
					const float dist = plane.GetDistanceToPoint(position);

					if (dist > radius)
						continue;

					const float VdN = Dot( plane.GetNormal(), velocity );
					if (VdN ==  0.0F || VdN == -0.0F)
						continue;

					const float t = -( Dot( position, plane.GetNormal() ) + plane.distance - radius ) / VdN;

					hit.intersection = position + velocity * t;
				}
				else
				{
					const float OriginDotPlane = Dot (plane.GetNormal(), origin);
					const float signedDistanceToOrigin = OriginDotPlane + plane.distance;
					const float PositionDotPlane = Dot (plane.GetNormal(), position);
					const float signedDistanceToPosition = PositionDotPlane + plane.distance;
					const bool originInside = Abs(signedDistanceToOrigin)<radius;
					const bool positionInside = Abs(signedDistanceToPosition)<=radius;
					const bool oppositeSide = !( (signedDistanceToOrigin > 0.0f && signedDistanceToPosition > 0.0f) || (signedDistanceToOrigin <= 0.0f && signedDistanceToPosition <= 0.0f) );

					// if the points are both inside or outside the radius we can bail if they're not on opposite sides outside the radius
					if ( originInside==positionInside && !(oppositeSide && !originInside && !positionInside )  )
						continue;

					// compute the side of the face we are on - this determines if we are trying to intersect with the front or back face of the plane
					const float signOrigin = signedDistanceToOrigin < 0.0 ? -1.f : 1.f;
					const float signedRadius = radius*signOrigin;					

					// check the direction of the ray is opposite to the plane normal (the sign flips the normal as appropriate)
					const float VdN = Dot( plane.GetNormal(), velocity );
					if (VdN*signOrigin >= 0)
						continue;

					// calculate intersection point
					const float t = -( signedDistanceToPosition - signedRadius ) / VdN;
					hit.intersection = position + velocity * t;	
				}
			}			

			// compute the bounce
			CalculateCollisionResponse(roState, state, ps, q, params, ps.position[q], ps.velocity[q] + ps.animatedVelocity[q], hit);
			
			// Kill particle?
			const float speedSqr = SqrMagnitude(ps.velocity[q] + ps.animatedVelocity[q]);
			if (ps.lifetime[q] < 0.0f || speedSqr < params.minKillSpeedSqr)
			{
				// when killing a particle the last element from the array is pulled into the position of the killed particle and toIndex is updated accordingly
				// we do a q-- in order to make sure the new particle at q is also collided
				collisionInfo.m_Colliders[q] = collisionInfo.m_Colliders[toIndex-1];
				KillParticle(roState, state, ps, q, toIndex);
				q--;
			}
		}
	}
		
	// resize array to account for killed particles
	ps.array_resize(toIndex);
}

// Update collision for the particle system.
void CollisionModule::Update (const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, size_t fromIndex, float dt )
{
	// any work todo?
	if (fromIndex == ps.array_size())
		return;

	ps.SetUsesCollisionEvents (m_CollisionMessages);

	// setup params
	ParticleSystemCollisionParameters params;
	params.bounceFactor = 1.0f - m_Bounce;
	params.energyLossOnCollision = m_EnergyLossOnCollision;
	params.minKillSpeedSqr = m_MinKillSpeed * m_MinKillSpeed;
	params.particleRadius = m_ParticleRadius;
	params.dampen = 1.0f - m_Dampen;
	params.planeColliderCache = ( IsApproximate() ? &m_ColliderCache : NULL );
	params.raycaster = GetRaycastInterface();
	params.rayBudget = state.rayBudget;
	params.voxelSize = m_VoxelSize;
	params.nextParticleToTrace = ( state.nextParticleToTrace >= ps.array_size() ? fromIndex : std::max(state.nextParticleToTrace,fromIndex) );
	
	// update particle collision planes
	const CollisionInfo collisionCounter = UpdateCollisionPlanes(m_Type != kPlaneCollision, m_CollidesWith.m_Bits, roState, state, params, ps, fromIndex, dt);

	// update collider index
	state.nextParticleToTrace = params.nextParticleToTrace + params.rayBudget;

	// decrement ray budget
	state.rayBudget = (state.rayBudget>collisionCounter.m_NumWorldCollisions ? state.rayBudget-collisionCounter.m_NumWorldCollisions : 0);

	// early out if there were no collisions at all
	if ( collisionCounter.AllCollisions() <= 0 )
	{
		FREE_TEMP_MANUAL(collisionCounter.m_Colliders);
		return;
	}

	// perform plane collisions
	PerformPlaneCollisions(m_Type != kPlaneCollision, roState, state, ps, params, collisionCounter, fromIndex, dt);
	
	FREE_TEMP_MANUAL(collisionCounter.m_Colliders);

	if (ps.GetUsesCollisionEvents ())
	{
		ps.collisionEvents.SortCollisionEventThreadArray ();
	}
}

void CollisionModule::CheckConsistency ()
{
	m_Dampen = clamp<float> (m_Dampen, 0.0f, 1.0f);
	m_Bounce = clamp<float> (m_Bounce, 0.0f, 2.0f);
	m_EnergyLossOnCollision = clamp<float> (m_EnergyLossOnCollision, 0.0f, 1.0f);
	m_ParticleRadius = max<float>(m_ParticleRadius, 0.01f);
}

template<class TransferFunction>
void CollisionModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_Type, "type");

	const char* kPrimitiveNames [kMaxNumPrimitives] = { "plane0", "plane1", "plane2", "plane3", "plane4", "plane5"}; 
	for(int i = 0; i < kMaxNumPrimitives; i++)
		transfer.Transfer (m_Primitives[i], kPrimitiveNames[i]);

	transfer.Transfer (m_Dampen, "dampen");
	transfer.Transfer (m_Bounce, "bounce");
	transfer.Transfer (m_EnergyLossOnCollision, "energyLossOnCollision");
	transfer.Transfer (m_MinKillSpeed, "minKillSpeed");
	transfer.Transfer (m_ParticleRadius, "particleRadius");
	transfer.Align();
	transfer.Transfer (m_CollidesWith, "collidesWith");
	transfer.Transfer (m_Quality, "quality");
	transfer.Align();
	transfer.Transfer (m_VoxelSize, "voxelSize");
	transfer.Transfer (m_CollisionMessages, "collisionMessages");
}

INSTANTIATE_TEMPLATE_TRANSFER(CollisionModule)
