#include "UnityPrefix.h"
#include "CollisionListener2D.h"

#if ENABLE_2D_PHYSICS

#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"
#include "Runtime/Scripting/ScriptingManager.h"

#include "Runtime/Profiler/Profiler.h"

PROFILER_INFORMATION(gPhysics2DProfileContactPreSolveAcquire, "Physics2D.ContactPreSolveAcquire", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileContactBeginAcquire, "Physics2D.ContactBeginAcquire", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileContactEndAcquire, "Physics2D.ContactEndAcquire", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileContactReporting, "Physics2D.ContactReporting", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileContactReportTriggers, "Physics2D.ContactReportTriggers", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileContactReportCollisions, "Physics2D.ContactReportCollisions", kProfilerPhysics)


// --------------------------------------------------------------------------


inline void VerifyObjectPtr(Object* obj)
{
	#if !UNITY_RELEASE
	if (obj == NULL)
		return;
	Assert(Object::IDToPointer(obj->GetInstanceID()) == obj);
	#endif
}


CollisionListener2D::CollisionListener2D()
	: m_ReportingCollisions(false)
{
	m_Collisions.set_empty_key(std::make_pair((Collider2D*)NULL,(Collider2D*)NULL));
	m_Collisions.set_deleted_key(std::make_pair((Collider2D*)~0,(Collider2D*)~0));
}


void CollisionListener2D::PreSolve(b2Contact* contact, const b2Manifold* oldManifold)
{
	PROFILER_AUTO(gPhysics2DProfileContactPreSolveAcquire, NULL);
	Assert (!m_ReportingCollisions);

	// Fetch the fixtures and colliders.
	b2Fixture* fixture = contact->GetFixtureA();
	b2Fixture* otherFixture = contact->GetFixtureB();
	Collider2D* collider = reinterpret_cast<Collider2D*>(fixture->GetUserData());
	VerifyObjectPtr (collider);
	Collider2D* otherCollider = reinterpret_cast<Collider2D*>(otherFixture->GetUserData());
	VerifyObjectPtr (otherCollider);

	// Calculate the contact key.
	Collider2D* firstCollider = collider;
	Collider2D* secondCollider = otherCollider;
	if (firstCollider->GetInstanceID() > secondCollider->GetInstanceID ())
		std::swap (firstCollider, secondCollider);
	const ColliderKey contactKey = std::make_pair (firstCollider, secondCollider);

	// Find the contact.
	ColliderMap::iterator colliderItr = m_Collisions.find (contactKey);
	Assert (colliderItr != m_Collisions.end ());
	Collision2D& collision = colliderItr->second;

	// Ignore if the contact was not just added.
	if (collision.m_ContactMode != Collision2D::ContactAdded)
		return;

	// Calculate the contacts.
	collision.m_ContactCount = contact->GetManifold()->pointCount;
	contact->GetWorldManifold( &collision.m_ContactManifold );

	// Fetch the rigid-bodies.
	Rigidbody2D* rigidbody = collider ? collider->GetRigidbody() : NULL;
	VerifyObjectPtr (rigidbody);
	Rigidbody2D* otherRigidbody = otherCollider ? otherCollider->GetRigidbody() : NULL;
	VerifyObjectPtr (otherRigidbody);
	b2Body* body = rigidbody != NULL ? rigidbody->GetBody () : GetPhysicsGroundBody ();
	b2Body* otherBody = otherRigidbody != NULL ? otherRigidbody->GetBody () : GetPhysicsGroundBody ();

	// Calculate relative velocity.
	const b2Vec2& contactPoint = collision.m_ContactManifold.points[0];
	const b2Vec2 bodyVelocity = body->GetLinearVelocityFromWorldPoint(contactPoint);
	const b2Vec2 otherBodyVelocity = otherBody->GetLinearVelocityFromWorldPoint(contactPoint);
	collision.m_RelativeVelocity.Set (otherBodyVelocity.x - bodyVelocity.x, otherBodyVelocity.y - bodyVelocity.y);
}


void CollisionListener2D::BeginContact(b2Contact* contact)
{
	PROFILER_AUTO(gPhysics2DProfileContactBeginAcquire, NULL);

	Assert (!m_ReportingCollisions);
	Assert (contact->IsTouching());

	// Fetch the fixtures and colliders.
	b2Fixture* fixture = contact->GetFixtureA();
	b2Fixture* otherFixture = contact->GetFixtureB();
	Collider2D* collider = reinterpret_cast<Collider2D*>(fixture->GetUserData());
	VerifyObjectPtr (collider);
	Collider2D* otherCollider = reinterpret_cast<Collider2D*>(otherFixture->GetUserData());
	VerifyObjectPtr (otherCollider);

	// Calculate the contact key.
	Collider2D* firstCollider = collider;
	Collider2D* secondCollider = otherCollider;
	if (firstCollider->GetInstanceID() > secondCollider->GetInstanceID ())
		std::swap (firstCollider, secondCollider);
	const ColliderKey contactKey = std::make_pair (firstCollider, secondCollider);

	// Find the contact.
	ColliderMap::iterator colliderItr = m_Collisions.find (contactKey);

	// If we already have a contact added for this collider-pair then we'll just bump-up the contact references.
	// We do this because each collider can have multiple fixtures resulting in multiple contacts generated.
	if (colliderItr != m_Collisions.end ())
	{
		Collision2D& collision = colliderItr->second;

		// Increase contact references.
		collision.m_ContactReferences++;

		// Contact mode should revert to stay if it was removed else it's just added.
		collision.m_ContactMode = collision.m_ContactMode == Collision2D::ContactRemoved ? Collision2D::ContactStay : Collision2D::ContactAdded;

		return;
	}

	// Add the contact.
	Collision2D& collision = m_Collisions[contactKey];

	// Fetch the rigid-bodies.
	Rigidbody2D* rigidbody = collider ? collider->GetRigidbody() : NULL;
	VerifyObjectPtr (rigidbody);
	Rigidbody2D* otherRigidbody = otherCollider ? otherCollider->GetRigidbody() : NULL;
	VerifyObjectPtr (otherRigidbody);

	// Populate the collision entry.
	collision.m_ContactReferences = 1;
	collision.m_Rigidbody = rigidbody;
	collision.m_OtherRigidbody = otherRigidbody;
	collision.m_Collider = collider;
	collision.m_OtherCollider = otherCollider;
	collision.m_Flipped = false;
	collision.m_ContactMode = Collision2D::ContactAdded;
	collision.m_TriggerCollision = fixture->IsSensor() || otherFixture->IsSensor();
	collision.m_ContactCount = 0;
	collision.m_RelativeVelocity = Vector2f::zero;
}


void CollisionListener2D::EndContact(b2Contact* contact)
{
	if (m_ReportingCollisions)
		return;

	PROFILER_AUTO(gPhysics2DProfileContactEndAcquire, NULL);

	// Fetch the fixtures and colliders.
	b2Fixture* fixture = contact->GetFixtureA();
	b2Fixture* otherFixture = contact->GetFixtureB();
	Collider2D* collider = reinterpret_cast<Collider2D*>(fixture->GetUserData());
	VerifyObjectPtr (collider);
	Collider2D* otherCollider = reinterpret_cast<Collider2D*>(otherFixture->GetUserData());
	VerifyObjectPtr (otherCollider);

	// Calculate the contact key.
	Collider2D* firstCollider = collider;
	Collider2D* secondCollider = otherCollider;
	if (firstCollider->GetInstanceID() > secondCollider->GetInstanceID ())
		std::swap (firstCollider, secondCollider);
	const ColliderKey contactKey = std::make_pair (firstCollider, secondCollider);

	// Find the contact.
	ColliderMap::iterator colliderItr = m_Collisions.find (contactKey);
	Assert (colliderItr != m_Collisions.end ());
	Collision2D& collision = colliderItr->second;

	// We need to reduce the contact references.
	// We do this because each collider can have multiple fixtures resulting in multiple contacts generated.
	UInt32& contactReferences = collision.m_ContactReferences;
	contactReferences--;

	// Finish if there still exists contact references.
	if (contactReferences > 0)
		return;

	// Flag the contact as invalid if either collider is not active.
	if (!collider->IsActive () || !collider->GetEnabled () || !otherCollider->IsActive() || !otherCollider->GetEnabled ())
	{
		collision.m_ContactMode = Collision2D::ContactInvalid;
		return;
	}

	// Sanity!
	VerifyObjectPtr (collision.m_Collider);
	VerifyObjectPtr (collision.m_OtherCollider);
	VerifyObjectPtr (collision.m_Rigidbody);
	VerifyObjectPtr (collision.m_OtherRigidbody);

	// Flag as just removed.
	collision.m_ContactMode = Collision2D::ContactRemoved;
}


void CollisionListener2D::InvalidateColliderCollisions(Collider2D* collider)
{
	// Flag collider collision information for the specified collider as invalid.
	// NOTE: Does not send any collision/trigger messages (matches what 3D physics is doing).
	for (ColliderMap::iterator colliderItr = m_Collisions.begin(); colliderItr != m_Collisions.end(); ++colliderItr)
	{
		// Does this contact relate to this collider?
		if (colliderItr->first.first == collider || colliderItr->first.second == collider)
		{
			// Yes, so flag it as a bad contact.
			colliderItr->second.m_ContactMode = Collision2D::ContactInvalid;
		}
	}
}


void CollisionListener2D::DestroyColliderCollisions(Collider2D* collider)
{
	// Destroy collider collision information for the specified collider immediately.
	for (ColliderMap::iterator colliderItr = m_Collisions.begin(); colliderItr != m_Collisions.end(); /**/)
	{
		// Fetch next collider.
		ColliderMap::iterator nextColliderItr = colliderItr;
		++nextColliderItr;

		// Does this contact relate to this collider?
		if (colliderItr->first.first == collider || colliderItr->first.second == collider)
		{
			// Yes, so remove it.
			m_Collisions.erase (colliderItr);
		}

		colliderItr = nextColliderItr;
	}
}


void CollisionListener2D::ReportCollisions()
{
	PROFILER_AUTO(gPhysics2DProfileContactReporting, NULL);

	Assert (!m_ReportingCollisions);
	m_ReportingCollisions = true;

	// Iterate all the active collider collisions.
	for (ColliderMap::iterator colliderItr = m_Collisions.begin(); colliderItr != m_Collisions.end(); /**/)
	{
		// Fetch next collider.
		ColliderMap::iterator nextColliderItr = colliderItr;
		++nextColliderItr;

		// Fetch the collision.
		Collision2D& collision = colliderItr->second;

		// Fetch the contact mode.
		Collision2D::ContactMode& contactMode = collision.m_ContactMode;

		// Process the collision if it's not invalid.
		if (contactMode != Collision2D::ContactInvalid)
		{
			// Further validate the collision.
			VerifyObjectPtr (collision.m_Collider);
			VerifyObjectPtr (collision.m_OtherCollider);
			VerifyObjectPtr (collision.m_Rigidbody);
			VerifyObjectPtr (collision.m_OtherRigidbody);

			// Calculate the message targets.
			Unity::Component* messageTarget = (Unity::Component*)collision.m_Collider;
			Unity::Component* otherMessageTarget = (Unity::Component*)collision.m_OtherCollider;

			// Reset the callback message.
			const MessageIdentifier* callbackMessage = NULL;

			// Is this a trigger collision?
			if (collision.m_TriggerCollision)
			{
				PROFILER_AUTO(gPhysics2DProfileContactReportTriggers, NULL);

				// Yes, so calculate the appropriate trigger callback message.
				if (collision.m_ContactMode == Collision2D::ContactAdded)
					callbackMessage = &kTriggerEnter2D;
				else if (collision.m_ContactMode == Collision2D::ContactRemoved)
					callbackMessage = &kTriggerExit2D;
				else
					callbackMessage = &kTriggerStay2D;

				// Send trigger callbacks to both colliders
				messageTarget->SendMessage (*callbackMessage, collision.m_OtherCollider, ClassID (Collider2D));
				otherMessageTarget->SendMessage (*callbackMessage, collision.m_Collider, ClassID(Collider2D));
			}
			else
			{
				PROFILER_AUTO(gPhysics2DProfileContactReportCollisions, NULL);

				// No, so calculate the appropriate collision callback message.
				if (collision.m_ContactMode == Collision2D::ContactAdded)
					callbackMessage = &kCollisionEnter2D;
				else if (collision.m_ContactMode == Collision2D::ContactRemoved)
					callbackMessage = &kCollisionExit2D;
				else
					callbackMessage = &kCollisionStay2D;

				// Send collision callbacks to both colliders.
				collision.m_Flipped = true;
				messageTarget->SendMessage (*callbackMessage, &collision, ClassID (Collision2D));
				collision.m_Flipped = false;
				otherMessageTarget->SendMessage (*callbackMessage, &collision, ClassID (Collision2D));
			}
		}

		// Remove contacts that are flagged as being removed.
		if (contactMode == Collision2D::ContactRemoved || contactMode == Collision2D::ContactInvalid)
		{
			m_Collisions.erase (colliderItr);
		}
		else
		{
			// The collision is now at "stay" mode.
			contactMode = Collision2D::ContactStay;
		}

		colliderItr = nextColliderItr;
	}

	m_ReportingCollisions = false;
}


#if ENABLE_SCRIPTING
ScriptingObjectPtr ConvertCollision2DToScripting (Collision2D* input)
{
	Collision2D& collision = *reinterpret_cast<Collision2D*>(input);
	ScriptingCollision2D scriptCollision;
	ScriptingObjectPtr collider;
	ScriptingObjectPtr otherCollider;

	// Populate object targets.
	if (collision.m_Flipped)
	{
		scriptCollision.rigidbody = Scripting::ScriptingWrapperFor (collision.m_OtherRigidbody);
		collider = scriptCollision.collider = Scripting::ScriptingWrapperFor (collision.m_OtherCollider);
		otherCollider = Scripting::ScriptingWrapperFor (collision.m_Collider);
		scriptCollision.relativeVelocity = -collision.m_RelativeVelocity;
	}
	else
	{
		scriptCollision.rigidbody = Scripting::ScriptingWrapperFor (collision.m_Rigidbody);
		collider = scriptCollision.collider = Scripting::ScriptingWrapperFor (collision.m_Collider);
		otherCollider = Scripting::ScriptingWrapperFor (collision.m_OtherCollider);
		scriptCollision.relativeVelocity = collision.m_RelativeVelocity;
	}

	// Populate contact array.
	ScriptingArrayPtr contacts = CreateScriptingArray<ScriptingContactPoint2D>(GetScriptingManager ().GetCommonClasses ().contactPoint2D, collision.m_ContactCount);
	scriptCollision.contacts = contacts;

	// Fetch collision normal.
	const b2Vec2& manifoldNormal = collision.m_Flipped ? -collision.m_ContactManifold.normal : collision.m_ContactManifold.normal;
	const Vector2f collisionNormal (manifoldNormal.x, manifoldNormal.y);

	// Populate contacts.
	for (int index = 0; index < collision.m_ContactCount; ++index )
	{
#if UNITY_WINRT
		ScriptingContactPoint2D contactPoint;
#else
		ScriptingContactPoint2D& contactPoint = Scripting::GetScriptingArrayElement<ScriptingContactPoint2D> (contacts, index);
#endif
		// Set contact point.
		const b2Vec2& manifoldPoint = collision.m_ContactManifold.points[index];
		contactPoint.point.Set (manifoldPoint.x, manifoldPoint.y);
		contactPoint.normal = collisionNormal;

		// Set colliders.
		contactPoint.collider = collider;
		contactPoint.otherCollider = otherCollider;

#if UNITY_WINRT
		// A slower way to set a value in the array:
		//  * we create a scripting object;
		//  * then marshal data from contactPoint to that scripting object
		//  * and only then we're setting it in the array
		// At the moment there's no other way, unless we remove all ScriptingObjectPtr from ScriptingContactPoint2D
		Scripting::SetScriptingArrayElement(contacts, index, CreateScriptingObjectFromNativeStruct<ScriptingContactPoint2D>(GetScriptingManager ().GetCommonClasses ().contactPoint2D, contactPoint));
#endif
	}

	return CreateScriptingObjectFromNativeStruct<ScriptingCollision2D>(GetScriptingManager ().GetCommonClasses ().collision2D, scriptCollision);
}
#endif

#endif // #if ENABLE_2D_PHYSICS
