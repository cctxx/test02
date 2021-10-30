#ifndef PARTICLESTRUCT_H
#define PARTICLESTRUCT_H

#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Geometry/AABB.h"
#if UNITY_WII
#include "Runtime/Misc/Allocator.h"
#endif

struct Particle
{
	Vector3f		position;			//12
	Vector3f		velocity;			//12
	float			size;				//4
	float			rotation;			//4
	float			angularVelocity;	//4	
	float			energy;				//4
	float			startEnergy;		//4
	ColorRGBA32		color;				//4
};

struct SimpleParticle
{
	Vector3f		position;
	Vector3f		velocity;
	float			size;
	float			rotation;
	float			angularVelocity;
	float			energy;
	float			startEnergy;
	ColorRGBAf		color;
};


// 24 bytes
typedef UNITY_VECTOR(kMemParticles, Particle) ParticleArray;

struct PrivateParticleInfo
{
	MinMaxAABB		aabb;
	float			maxEmitterParticleSize;// Maximum size of any particle of emitted particles
	float			maxParticleSize;// max particle size of any particle after particle animation is done
	float			maxEnergy;
	bool			useWorldSpace;
	bool			hadEverEmitOn; // had "emit" flag ever set?
	bool			isEmitOn; // is "emit" flag currently set?
};


const float kTimeEpsilon = 0.001f;

inline void KillParticle (ParticleArray& array, int i)
{
	array[i] = array.back ();
	array.pop_back ();
}

#endif
