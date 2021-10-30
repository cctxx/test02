#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Utilities/dense_hash_map.h"
#include "Runtime/Physics2D/Collider2D.h"
#include "Box2D/Box2D.h"
#include <list>

class Rigidbody2D;
class b2Contact;


// --------------------------------------------------------------------------


struct Collision2D
{
	Collision2D() :
		m_Rigidbody(NULL),
		m_OtherRigidbody(NULL),
		m_Collider(NULL),
		m_OtherCollider(NULL),
		m_ContactMode(ContactInvalid),
		m_Flipped(false),
		m_TriggerCollision(false),
		m_ContactCount(0),
		m_ContactReferences(0) {}

	Rigidbody2D* m_Rigidbody;
	Rigidbody2D* m_OtherRigidbody;
	Collider2D* m_Collider;
	Collider2D* m_OtherCollider;

	UInt32 m_ContactCount;
	UInt32 m_ContactReferences;
	b2WorldManifold m_ContactManifold;
	Vector2f m_RelativeVelocity;

	enum ContactMode
	{
		ContactInvalid,
		ContactAdded,
		ContactRemoved,
		ContactStay
	};

	ContactMode m_ContactMode;
	bool m_Flipped;
	bool m_TriggerCollision;
};


// --------------------------------------------------------------------------

#if ENABLE_SCRIPTING
struct ScriptingContactPoint2D
{
	Vector2f point;
	Vector2f normal;
	ScriptingObjectPtr collider;
	ScriptingObjectPtr otherCollider;
};

struct ScriptingCollision2D
{
	ScriptingObjectPtr rigidbody;
	ScriptingObjectPtr collider;
	ScriptingArrayPtr contacts;
	Vector2f relativeVelocity;
};

ScriptingObjectPtr ConvertCollision2DToScripting (Collision2D* input);
#endif


// --------------------------------------------------------------------------


class CollisionListener2D : public b2ContactListener
{
public:
	CollisionListener2D();

	// b2ContactListener interface
	virtual void BeginContact(b2Contact* contact);
	virtual void EndContact(b2Contact* contact);
	virtual void PreSolve(b2Contact* contact, const b2Manifold* oldManifold);

	void ReportCollisions();
	void InvalidateColliderCollisions(Collider2D* collider);
	void DestroyColliderCollisions(Collider2D* collider);

private:
	// Collision information is stored for each collider pair (the key is sorted by instance IDs so it always identifies the pair).
	// In the collision information, contact points etc. are stored.
	typedef std::pair<Collider2D*,Collider2D*> ColliderKey;
	struct ColliderKeyHashFunctor
	{
		inline size_t operator()(const ColliderKey& x) const
		{
			UInt32 xa = x.first->GetInstanceID();
			UInt32 xb = x.second->GetInstanceID();
			UInt32 a = xa;
			a = (a+0x7ed55d16) + (a<<12);
			a = (a^0xc761c23c) ^ (a>>19);
			a ^= xb;
			a = (a+0x165667b1) + (a<<5);
			a = (a+0xd3a2646c) ^ (a<<9);
			return a;
		}
	};
	typedef std::pair<const ColliderKey, Collision2D> ColliderKeyToCollisionPair;
	typedef dense_hash_map<ColliderKey, Collision2D, ColliderKeyHashFunctor, std::equal_to<ColliderKey>, STL_ALLOCATOR(kMemSTL, ColliderKeyToCollisionPair) > ColliderMap;

private:
	ColliderMap	m_Collisions;
	bool m_ReportingCollisions;
};

#endif
