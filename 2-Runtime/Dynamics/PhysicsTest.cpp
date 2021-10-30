#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_UNIT_TESTS

#include "Runtime/Dynamics/BoxCollider.h"
#include "Runtime/Dynamics/RigidBody.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Graphics/Transform.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"


void RunPhysicsTests ();

void SetParentAttachesColliderToRigidbody ();
void RemovingRididbodyComponentReattachesStaticCollider ();
void RemovingRididbodyComponentReattachesColliderToNextRigidbody ();

///@TODO: Tests for Joints losing connectiong with rigidbody etc. There seems to be no C++ code handling this properly???


void RunPhysicsTests ()
{
	SetParentAttachesColliderToRigidbody ();
	RemovingRididbodyComponentReattachesStaticCollider ();
	RemovingRididbodyComponentReattachesColliderToNextRigidbody ();
}


// Changing parent of collider will reattach collider to rigidbody and deatch it.
void SetParentAttachesColliderToRigidbody ()
{
	GameObject& root = CreateGameObject("Root", "Transform", "Rigidbody", NULL);
	GameObject& colliderChild = CreateGameObject("Child", "Transform", "BoxCollider", NULL);
	colliderChild.GetComponent(Transform).SetParent(root.QueryComponent(Transform));
	
	Assert(colliderChild.GetComponent(BoxCollider).GetShape()->getActor().userData != root.QueryComponent(Rigidbody));
	Assert(&colliderChild.GetComponent(BoxCollider).GetShape()->getActor() == root.GetComponent(Rigidbody).GetActor());
	
	colliderChild.GetComponent(Transform).SetParent(NULL);
	
	Assert(colliderChild.GetComponent(BoxCollider).GetShape()->getActor().userData == NULL);
	
	colliderChild.GetComponent(Transform).SetParent(root.QueryComponent(Transform));
	
	Assert(colliderChild.GetComponent(BoxCollider).GetShape()->getActor().userData != root.QueryComponent(Rigidbody));
	Assert(&colliderChild.GetComponent(BoxCollider).GetShape()->getActor() == root.GetComponent(Rigidbody).GetActor());
}

/// @TODO: Test Modify hierarchy through prefab instantiate change. For example parenting change. Does it set up shapes correctly.
/// Manually verify that we dont call Recreate too often, especially when destroying a whole hierarchy.

void RemovingRididbodyComponentReattachesStaticCollider ()
{
	// Rigidbody
	// - Collider
	// 1) Remove Rigidbody component
	// -> Rigidbody Cleanup is called once. No create is called.
	// -> Collider becomes static collider
	
	GameObject& root = CreateGameObject("Root", "Transform", "Rigidbody", NULL);
	GameObject& colliderChild = CreateGameObject("Child", "Transform", "BoxCollider", NULL);
	colliderChild.GetComponent(Transform).SetParent(root.QueryComponent(Transform));
	
	Assert(colliderChild.GetComponent(BoxCollider).GetShape()->getActor().userData != root.QueryComponent(Rigidbody));
	Assert(&colliderChild.GetComponent(BoxCollider).GetShape()->getActor() == root.GetComponent(Rigidbody).GetActor());
	DestroyObjectHighLevel(root.QueryComponent(Rigidbody));
	Assert(colliderChild.GetComponent(BoxCollider).GetShape()->getActor().userData == NULL);
}

void RemovingRididbodyComponentReattachesColliderToNextRigidbody ()
{
	// Rigidbody
	// - Rigidbody
	//   - Collider
	// 1) Remove Rigidbody component
	// -> Rigidbody Cleanup is called once. No create is called.
	// -> Collider 
	
	GameObject& root = CreateGameObject("Root", "Transform", "Rigidbody", NULL);
	GameObject& rbChild = CreateGameObject("Root", "Transform", "Rigidbody", NULL);
	GameObject& colliderChild = CreateGameObject("Child", "Transform", "BoxCollider", NULL);
	rbChild.GetComponent(Transform).SetParent(root.QueryComponent(Transform));
	colliderChild.GetComponent(Transform).SetParent(rbChild.QueryComponent(Transform));
	
	Assert(colliderChild.GetComponent(BoxCollider).GetShape()->getActor().userData != NULL);
	Assert(&colliderChild.GetComponent(BoxCollider).GetShape()->getActor() == rbChild.GetComponent(Rigidbody).GetActor());
	
	DestroyObjectHighLevel(root.QueryComponent(Rigidbody));
	Assert(colliderChild.GetComponent(BoxCollider).GetShape()->getActor().userData == root.GetComponent(Rigidbody).GetActor());
}

#endif