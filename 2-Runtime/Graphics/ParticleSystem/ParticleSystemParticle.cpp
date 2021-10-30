#include "UnityPrefix.h"
#include "ParticleSystemParticle.h"

#include "Runtime/Dynamics/Collider.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Threads/Thread.h"

void InitializeEmitAccumulator(dynamic_array<float>& emitAccumulator, const size_t count)
{
	emitAccumulator.resize_uninitialized(count);
	memset(emitAccumulator.begin(), 0, count * sizeof(float));
}

ParticleCollisionEvent::ParticleCollisionEvent (const Vector3f& intersection, const Vector3f& normal, const Vector3f& velocity, int colliderInstanceID, int rigidBodyOrColliderInstanceID)
{
	m_Intersection = intersection;
	m_Normal = normal;
	m_Velocity = velocity;
	m_ColliderInstanceID = colliderInstanceID;
	m_RigidBodyOrColliderInstanceID = rigidBodyOrColliderInstanceID;
}

size_t ParticleSystemParticles::GetParticleSize()
{
	// @TODO: How do we guard against elements we remove
	return sizeof(ParticleSystemParticle);
}

void ParticleSystemParticles::SetUsesAxisOfRotation()
{
	usesAxisOfRotation = true;
	const size_t count = position.size();
	axisOfRotation.resize_uninitialized(count);
	for(int i = 0; i < count; i++)
		axisOfRotation[i] = Vector3f::yAxis;
}

void ParticleSystemParticles::SetUsesRotationalSpeed()
{
	usesRotationalSpeed = true;
	const size_t count = position.size();
	rotationalSpeed.resize_uninitialized(count);
	for(int i = 0; i < count; i++)
		rotationalSpeed[i] = 0.0f;
}

void ParticleSystemParticles::SetUsesCollisionEvents (bool wantUsesCollisionEvents)
{
	if (usesCollisionEvents == wantUsesCollisionEvents) return;
	usesCollisionEvents = wantUsesCollisionEvents;
	if (!usesCollisionEvents)
	{
		collisionEvents.Clear ();
	}
}

bool ParticleSystemParticles::GetUsesCollisionEvents () const
{
	return usesCollisionEvents;
}

void ParticleSystemParticles::SetUsesEmitAccumulator(int numAccumulators)
{
	Assert(numAccumulators <= kParticleSystemMaxNumEmitAccumulators);
	const size_t count = position.size();
	for(int i = numEmitAccumulators; i < numAccumulators; i++)
		InitializeEmitAccumulator(emitAccumulator[i], count);		

	numEmitAccumulators = numAccumulators;
}

void ParticleSystemParticles::CopyFromArrayAOS(ParticleSystemParticle* particles, int size)
{
	Assert(usesAxisOfRotation);
	Assert(numEmitAccumulators == kParticleSystemMaxNumEmitAccumulators);
	for(int i = 0; i < size; i++)
	{
		(*this).position[i] = particles[i].position;
		(*this).velocity[i] = particles[i].velocity;
		(*this).animatedVelocity[i] = particles[i].animatedVelocity;
		(*this).axisOfRotation[i] = particles[i].axisOfRotation;
		(*this).rotation[i] = particles[i].rotation;
		if(usesRotationalSpeed)
			(*this).rotationalSpeed[i] = particles[i].rotationalSpeed;
		(*this).size[i] = particles[i].size;
		(*this).color[i] = particles[i].color;
		(*this).randomSeed[i] = particles[i].randomSeed;
		(*this).lifetime[i] = particles[i].lifetime;
		(*this).startLifetime[i] = particles[i].startLifetime;
		for(int acc = 0; acc < kParticleSystemMaxNumEmitAccumulators; acc++)
			(*this).emitAccumulator[acc][i] = particles[i].emitAccumulator[acc];
	}
}

void ParticleSystemParticles::CopyToArrayAOS(ParticleSystemParticle* particles, int size)
{
	Assert(usesAxisOfRotation);
	Assert(numEmitAccumulators == kParticleSystemMaxNumEmitAccumulators);
	for(int i = 0; i < size; i++)
	{
		particles[i].position = (*this).position[i];
		particles[i].velocity = (*this).velocity[i];
		particles[i].animatedVelocity = (*this).animatedVelocity[i];
		particles[i].axisOfRotation = (*this).axisOfRotation[i];
		particles[i].rotation = (*this).rotation[i];
		if(usesRotationalSpeed)
			particles[i].rotationalSpeed = (*this).rotationalSpeed[i];
		particles[i].size = (*this).size[i];
		particles[i].color = (*this).color[i];
		particles[i].randomSeed = (*this).randomSeed[i];
		particles[i].lifetime = (*this).lifetime[i];
		particles[i].startLifetime = (*this).startLifetime[i];
		for(int acc = 0; acc < kParticleSystemMaxNumEmitAccumulators; acc++)
			particles[i].emitAccumulator[acc] = (*this).emitAccumulator[acc][i];
	}
}

void ParticleSystemParticles::AddParticle(ParticleSystemParticle* particle)
{
	const size_t count = array_size();
	array_resize(count+1);
	position[count] = particle->position;
	velocity[count] = particle->velocity;
	animatedVelocity[count] = Vector3f::zero;
	lifetime[count] = particle->lifetime;
	startLifetime[count] = particle->startLifetime;
	size[count] = particle->size;
	rotation[count] = particle->rotation;
	if(usesRotationalSpeed)
		rotationalSpeed[count] = particle->rotationalSpeed;
	color[count] = particle->color;
	randomSeed[count] = particle->randomSeed;
	if(usesAxisOfRotation)
		axisOfRotation[count] = particle->axisOfRotation;
	for(int acc = 0; acc < numEmitAccumulators; acc++)
		emitAccumulator[acc][count] = 0.0f;
}

size_t ParticleSystemParticles::array_size () const
{
	return position.size();
}

void ParticleSystemParticles::array_resize (size_t i)
{
	position.resize_uninitialized(i);
	velocity.resize_uninitialized(i);
	animatedVelocity.resize_uninitialized(i);
	rotation.resize_uninitialized(i);
	if(usesRotationalSpeed)
		rotationalSpeed.resize_uninitialized(i);
	size.resize_uninitialized(i);
	color.resize_uninitialized(i);
	randomSeed.resize_uninitialized(i);
	lifetime.resize_uninitialized(i);
	startLifetime.resize_uninitialized(i);
	if(usesAxisOfRotation)
		axisOfRotation.resize_uninitialized(i);
	for(int acc = 0; acc < numEmitAccumulators; acc++)
		emitAccumulator[acc].resize_uninitialized(i);
}

void ParticleSystemParticles::element_swap(size_t i, size_t j)
{
	std::swap(position[i], position[j]);
	std::swap(velocity[i], velocity[j]);
	std::swap(animatedVelocity[i], animatedVelocity[j]);
	std::swap(rotation[i], rotation[j]);
	if(usesRotationalSpeed)
		std::swap(rotationalSpeed[i], rotationalSpeed[j]);
	std::swap(size[i], size[j]);
	std::swap(color[i], color[j]);
	std::swap(randomSeed[i], randomSeed[j]);
	std::swap(lifetime[i], lifetime[j]);
	std::swap(startLifetime[i], startLifetime[j]);
	if(usesAxisOfRotation)
		std::swap(axisOfRotation[i], axisOfRotation[j]);
	for(int acc = 0; acc < numEmitAccumulators; acc++)
		std::swap(emitAccumulator[acc][i], emitAccumulator[acc][j]);
}

void ParticleSystemParticles::element_assign(size_t i, size_t j)
{
	position[i] = position[j];
	velocity[i] = velocity[j];
	animatedVelocity[i] = animatedVelocity[j];
	rotation[i] = rotation[j];
	if(usesRotationalSpeed)
		rotationalSpeed[i] = rotationalSpeed[j];
	size[i] = size[j];
	color[i] = color[j];
	randomSeed[i] = randomSeed[j];
	lifetime[i] = lifetime[j];
	startLifetime[i] = startLifetime[j];
	if(usesAxisOfRotation)
		axisOfRotation[i] = axisOfRotation[j];
	for(int acc = 0; acc < numEmitAccumulators; acc++)
		emitAccumulator[acc][i] = emitAccumulator[acc][j];
}

void ParticleSystemParticles::array_assign_external(void* data, const int numParticles)
{		
#define SHURIKEN_INCREMENT_ASSIGN_PTRS(element, type) { beginPtr = endPtr; endPtr += numParticles * sizeof(type); (element).assign_external((type*)beginPtr, (type*)endPtr); }
	
	UInt8* beginPtr = 0;
	UInt8* endPtr = (UInt8*)data;
	
	SHURIKEN_INCREMENT_ASSIGN_PTRS(position, Vector3f);
	SHURIKEN_INCREMENT_ASSIGN_PTRS(velocity, Vector3f);
	SHURIKEN_INCREMENT_ASSIGN_PTRS(animatedVelocity, Vector3f);
	SHURIKEN_INCREMENT_ASSIGN_PTRS(rotation, float);
	if(usesRotationalSpeed)
		SHURIKEN_INCREMENT_ASSIGN_PTRS(rotationalSpeed, float);
	SHURIKEN_INCREMENT_ASSIGN_PTRS(size, float);
	SHURIKEN_INCREMENT_ASSIGN_PTRS(color, ColorRGBA32);
	SHURIKEN_INCREMENT_ASSIGN_PTRS(randomSeed, UInt32);
	SHURIKEN_INCREMENT_ASSIGN_PTRS(lifetime, float);
	SHURIKEN_INCREMENT_ASSIGN_PTRS(startLifetime, float);
	if(usesAxisOfRotation)
		SHURIKEN_INCREMENT_ASSIGN_PTRS(axisOfRotation, Vector3f);
	for(int acc = 0; acc < numEmitAccumulators; acc++)
		SHURIKEN_INCREMENT_ASSIGN_PTRS(emitAccumulator[acc], float);
#undef SHURIKEN_INCREMENT_ASSIGN_PTRS
}

void ParticleSystemParticles::array_merge_preallocated(const ParticleSystemParticles& rhs, const int offset, const bool needAxisOfRotation, const bool needEmitAccumulator)
{
#define SHURIKEN_COPY_DATA(element, type) memcpy(&element[offset], &rhs.element[0], count * sizeof(type))
	
	const size_t count = rhs.array_size();
	if(0 == count)
		return;
	
	Assert((rhs.array_size() + offset) <= array_size());
	SHURIKEN_COPY_DATA(position, Vector3f);
	SHURIKEN_COPY_DATA(velocity, Vector3f);
	SHURIKEN_COPY_DATA(animatedVelocity, Vector3f);
	SHURIKEN_COPY_DATA(rotation, float);
	if(usesRotationalSpeed)
		SHURIKEN_COPY_DATA(rotationalSpeed, float);
	SHURIKEN_COPY_DATA(size, float);
	SHURIKEN_COPY_DATA(color, ColorRGBA32);
	SHURIKEN_COPY_DATA(randomSeed, UInt32);
	SHURIKEN_COPY_DATA(lifetime, float);
	SHURIKEN_COPY_DATA(startLifetime, float);

	if(needAxisOfRotation)
	{
		Assert(usesAxisOfRotation && rhs.usesAxisOfRotation);
		SHURIKEN_COPY_DATA(axisOfRotation, Vector3f);
	}
	if(needEmitAccumulator)
	{
		Assert(numEmitAccumulators && rhs.numEmitAccumulators);
		Assert(numEmitAccumulators == rhs.numEmitAccumulators);
		for(int acc = 0; acc < numEmitAccumulators; acc++)
			SHURIKEN_COPY_DATA(emitAccumulator[acc], float);
	}

#undef SHURIKEN_COPY_DATA 
}


void ParticleSystemParticles::array_assign(const ParticleSystemParticles& rhs)
{
	position.assign(rhs.position.begin(), rhs.position.end());
	velocity.assign(rhs.velocity.begin(), rhs.velocity.end());
	animatedVelocity.assign(rhs.animatedVelocity.begin(), rhs.animatedVelocity.end());
	rotation.assign(rhs.rotation.begin(), rhs.rotation.end());
	if(usesRotationalSpeed)
		rotationalSpeed.assign(rhs.rotationalSpeed.begin(), rhs.rotationalSpeed.end());
	size.assign(rhs.size.begin(), rhs.size.end());
	color.assign(rhs.color.begin(), rhs.color.end());
	randomSeed.assign(rhs.randomSeed.begin(), rhs.randomSeed.end());
	lifetime.assign(rhs.lifetime.begin(), rhs.lifetime.end());
	startLifetime.assign(rhs.startLifetime.begin(), rhs.startLifetime.end());
	if(usesAxisOfRotation)
		axisOfRotation.assign(rhs.axisOfRotation.begin(), rhs.axisOfRotation.end());
	for(int acc = 0; acc < numEmitAccumulators; acc++)
		emitAccumulator[acc].assign(rhs.emitAccumulator[acc].begin(), rhs.emitAccumulator[acc].end());
}

void ParticleSystemParticles::array_lerp(ParticleSystemParticles& output, const ParticleSystemParticles& a, const ParticleSystemParticles& b, float factor)
{
	DebugAssert(a.array_size() == b.array_size()); // else it doesn't really make sense
	DebugAssert(a.usesRotationalSpeed == b.usesRotationalSpeed);
	
	const int count = a.array_size(); 
	output.array_resize(count);

	// Note: Not all data is interpolated here intentionally, because it doesn't change anything or because it's incorrect

	#define SHURIKEN_LERP_DATA(element, type) for(int i = 0; i < count; i++) output.element[i] = Lerp(a.element[i], b.element[i], factor)			
	SHURIKEN_LERP_DATA(position, Vector3f);
	SHURIKEN_LERP_DATA(velocity, Vector3f);
	SHURIKEN_LERP_DATA(animatedVelocity, Vector3f);

	SHURIKEN_LERP_DATA(rotation, float);
	if(a.usesRotationalSpeed)
		SHURIKEN_LERP_DATA(rotationalSpeed, float);
	SHURIKEN_LERP_DATA(lifetime, float);

	#undef SHURIKEN_LERP_DATA
}

ParticleSystemParticlesTempData::ParticleSystemParticlesTempData()
:color(0)
,size(0)
,sheetIndex(0)
,particleCount(0)
{}

void ParticleSystemParticlesTempData::element_swap(size_t i, size_t j)
{
	DebugAssert(i <= particleCount);
	DebugAssert(j <= particleCount);

	std::swap(color[i], color[j]);
	std::swap(size[i], size[j]);
	if(sheetIndex)
		std::swap(sheetIndex[i], sheetIndex[j]);
}

