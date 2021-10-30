#include "UnityPrefix.h"
#include "ParticleCollisionEvents.h"

#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Threads/Thread.h"

CollisionEvents::CollisionEvents (): currentCollisionEventThreadArray(0)
{
}

void CollisionEvents::Clear ()
{
	collisionEvents[0].clear ();
	collisionEvents[1].clear ();
}

bool CollisionEvents::AddEvent (const ParticleCollisionEvent& event)
{
	GetCollisionEventThreadArray ().push_back (event);
	return true;
}

int CollisionEvents::GetCollisionEventCount () const
{
	return GetCollisionEventScriptArray ().size ();
}

void CollisionEvents::SwapCollisionEventArrays ()
{
	Assert (Thread::CurrentThreadIsMainThread ());
	currentCollisionEventThreadArray = (currentCollisionEventThreadArray+1)%2;
	collisionEvents[currentCollisionEventThreadArray].clear ();
}

dynamic_array<ParticleCollisionEvent>& CollisionEvents::GetCollisionEventThreadArray ()
{
	return collisionEvents[currentCollisionEventThreadArray];
}

const dynamic_array<ParticleCollisionEvent>& CollisionEvents::GetCollisionEventScriptArray () const
{
	const int currentCollisionEventScriptArray = (currentCollisionEventThreadArray+1)%2;
	return collisionEvents[currentCollisionEventScriptArray];
}

struct SortCollisionEventsByGameObject {
	bool operator()( const ParticleCollisionEvent& ra, const ParticleCollisionEvent& rb ) const;
};

bool SortCollisionEventsByGameObject::operator()( const ParticleCollisionEvent& ra, const ParticleCollisionEvent& rb ) const
{
	return ra.m_RigidBodyOrColliderInstanceID < rb.m_RigidBodyOrColliderInstanceID;
}

void CollisionEvents::SortCollisionEventThreadArray ()
{
	std::sort (GetCollisionEventThreadArray ().begin (), GetCollisionEventThreadArray ().end (), SortCollisionEventsByGameObject ());
}

static GameObject* GetGameObjectFromInstanceID (int instanceId)
{
	Object* temp = Object::IDToPointer (instanceId);
	if ( temp )
	{
		return (reinterpret_cast<Unity::Component*> (temp))->GetGameObjectPtr ();
	}
	return NULL;
}

static int GetGameObjectIDFromInstanceID (int instanceId)
{
	Object* temp = Object::IDToPointer (instanceId);
	if ( temp )
	{
		return (reinterpret_cast<Unity::Component*> (temp))->GetGameObjectInstanceID ();
	}
	return 0;
}

void CollisionEvents::SendCollisionEvents (Unity::Component& particleSystem) const
{
	const dynamic_array<ParticleCollisionEvent>& scriptEventArray = GetCollisionEventScriptArray ();
	GameObject* pParticleSystem = &particleSystem.GetGameObject ();
	GameObject* collideeGO = NULL;
	int currentId = -1;
	for (int e = 0; e < scriptEventArray.size (); ++e)
	{
		if (currentId != scriptEventArray[e].m_RigidBodyOrColliderInstanceID)
		{
			collideeGO = GetGameObjectFromInstanceID (scriptEventArray[e].m_RigidBodyOrColliderInstanceID);
			if (!collideeGO)
				continue;
			currentId = scriptEventArray[e].m_RigidBodyOrColliderInstanceID;
			pParticleSystem->SendMessage (kParticleCollisionEvent, collideeGO, ClassID (GameObject)); // send message to particle system
			collideeGO->SendMessage (kParticleCollisionEvent, pParticleSystem, ClassID (GameObject)); // send message to object collided with
		}
	}
}

int CollisionEvents::GetCollisionEvents (int instanceId, MonoParticleCollisionEvent* collisionEvents, int size) const
{
	const dynamic_array<ParticleCollisionEvent>& scriptEventArray = GetCollisionEventScriptArray ();
	dynamic_array<ParticleCollisionEvent>::const_iterator iter = scriptEventArray.begin ();
	for (; iter != scriptEventArray.end (); ++iter)
	{
		if (instanceId == GetGameObjectIDFromInstanceID (iter->m_RigidBodyOrColliderInstanceID))
		{
			int count = 0;
			while (iter != scriptEventArray.end () && GetGameObjectIDFromInstanceID (iter->m_RigidBodyOrColliderInstanceID) == instanceId && count < size)
			{
				collisionEvents[count].m_Intersection = iter->m_Intersection;
				collisionEvents[count].m_Normal = iter->m_Normal;
				collisionEvents[count].m_Velocity = iter->m_Velocity;
				collisionEvents[count].m_ColliderInstanceID = iter->m_ColliderInstanceID;
				count++;
				iter++;
			}
			return count;
		}
	}
	return 0;
}
