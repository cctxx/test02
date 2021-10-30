#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Physics2D/Physics2DManager.h"
#include "Runtime/Physics2D/JointDescriptions2D.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/GameCode/Behaviour.h"

class Rigidbody2D;
class b2Joint;
class b2Body;
struct b2JointDef;

// --------------------------------------------------------------------------


class Joint2D : public Behaviour
{
	friend class Rigidbody2D;

public:	
	REGISTER_DERIVED_ABSTRACT_CLASS (Joint2D, Behaviour)
	DECLARE_OBJECT_SERIALIZE (Joint2D)
	
	Joint2D (MemLabelId label, ObjectCreationMode mode);
	// virtual ~Joint2D (); declared-by-macro
	
	virtual void Reset ();
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	virtual void Deactivate (DeactivateOperation operation);

	virtual void AddToManager ();
	virtual void RemoveFromManager ();

	void RecreateJoint (const Rigidbody2D* ignoreRigidbody);

	void SetConnectedBody (PPtr<Rigidbody2D> body);
	PPtr<Rigidbody2D> GetConnectedBody () const { return m_ConnectedRigidBody; }

	void SetCollideConnected (bool Collide);
	bool GetCollideConnected () const { return m_CollideConnected; }

protected:
	virtual void Create () = 0;
	virtual void ReCreate();
	virtual void Cleanup ();

	b2Body* FetchBodyA () const;
	b2Body* FetchBodyB () const;
	void FinalizeCreateJoint (b2JointDef* jointDef);
	
protected:
	PPtr<Rigidbody2D>	m_ConnectedRigidBody;	///< The rigid body to connect to.  No rigid body connects to the scene.
	bool				m_CollideConnected;		///< Whether rigid bodies connected with this joint can collide or not.
	b2Joint*			m_Joint;
};

#endif //ENABLE_2D_PHYSICS
