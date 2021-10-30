#pragma once

#include "Runtime/Math/Vector3.h"
#include <list>

class Collider;
class Rigidbody;
struct MonoObject;

struct ContactPoint
{
	Collider* collider[2];
	Vector3f point;
	Vector3f normal;
};

struct Collision
{
	int status;
	
	bool flipped;
	
	Rigidbody* thisRigidbody;
	Rigidbody* otherRigidbody;
	Collider* thisCollider;
	Collider* otherCollider;

	Vector3f impactForceSum;
	Vector3f frictionForceSum;
	Vector3f relativeVelocity;
	typedef std::list<ContactPoint> Contacts;
	Contacts contacts;
};

ScriptingObjectPtr ConvertContactToMono (Collision* input);
