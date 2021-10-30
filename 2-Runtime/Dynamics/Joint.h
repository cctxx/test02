#ifndef JOINT_H
#define JOINT_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Vector3.h"
#include "RigidBody.h"
#include "JointDescriptions.h"
class Rigidbody;
class NxJointDesc;
class NxJoint;

namespace Unity
{

class Joint : public Component
{
	public:
	
	REGISTER_DERIVED_ABSTRACT_CLASS (Joint, Component)
	
	Joint (MemLabelId label, ObjectCreationMode mode);
	// virtual ~Joint (); declared-by-macro
	
	Vector3f GetAxis () const { return m_Axis; }
	virtual void SetAxis (const Vector3f& axis);
	
	Vector3f GetAnchor () const { return m_Anchor; }
	virtual void SetAnchor (const Vector3f& axis);

	Vector3f GetConnectedAnchor () const { return m_ConnectedAnchor; }
	virtual void SetConnectedAnchor (const Vector3f& axis);

	bool GetAutoConfigureConnectedAnchor () const { return m_AutoConfigureConnectedAnchor; }
	virtual void SetAutoConfigureConnectedAnchor (bool anchor);

	void SetBreakForce (float force);
	float GetBreakForce () const;
	void SetBreakTorque (float torque);
	float GetBreakTorque () const;
		
	void SetConnectedBody (PPtr<Rigidbody> body);
	PPtr<Rigidbody> GetConnectedBody () const { return m_ConnectedBody; }

	virtual void CalculateGlobalHingeSpace (Vector3f& globalAnchor, Vector3f& globalAxis, Vector3f& globalNormal) const;
	Vector3f CalculateGlobalConnectedAnchor (bool autoConfigureConnectedFrame);

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	
	virtual void Deactivate (DeactivateOperation operation);
	virtual void Reset ();
	
	virtual void CheckConsistency();
	
	void NullJoint () { m_Joint = NULL; }
	
	protected:
	
	/// SUB CLASSES OVERRIDE THIS	
	virtual void Create () = 0;
	virtual void ApplySetupAxesToDesc (int option) = 0;
	
	template<class TransferFunction>
	void JointTransferPre (TransferFunction& transfer)
	{
		Super::Transfer (transfer);

		TRANSFER_SIMPLE (m_ConnectedBody);
		TRANSFER_SIMPLE (m_Anchor);
		TRANSFER_SIMPLE (m_Axis);
		TRANSFER_SIMPLE (m_AutoConfigureConnectedAnchor);
		transfer.Align();
		TRANSFER_SIMPLE (m_ConnectedAnchor);
		
	}

	template<class TransferFunction>
	void JointTransferPreNoAxis (TransferFunction& transfer)
	{
		Super::Transfer (transfer);

		TRANSFER_SIMPLE (m_ConnectedBody);
		TRANSFER_SIMPLE (m_Anchor);
		TRANSFER_SIMPLE (m_AutoConfigureConnectedAnchor);
		transfer.Align();
		TRANSFER_SIMPLE (m_ConnectedAnchor);
	}

	template<class TransferFunction>
	void JointTransferPost (TransferFunction& transfer)
	{
		TRANSFER (m_BreakForce);
		TRANSFER (m_BreakTorque);
	}
	
	void FinalizeCreateImpl (NxJointDesc& desc, bool swapActors = false);

	NxJoint*         m_Joint;
	bool			 m_AutoConfigureConnectedAnchor;
	Vector3f         m_Anchor;
	Vector3f		 m_ConnectedAnchor;
	Vector3f         m_Axis;

	enum { kChangeAxis = 1 << 0, kChangeAnchor = 1 << 1 };
	void SetupAxes (NxJointDesc& desc, int options = kChangeAnchor|kChangeAxis);

	private:

	void Cleanup ();
	
	bool             m_DidSetupAxes; 

	float            m_BreakForce;///< Maximum force the joint can withstand before breaking. Infinity means unbreakable. range { 0.001, infinity }
	float            m_BreakTorque;	///< Maximum torque the joint can withstand before breaking. Infinity means unbreakable. range { 0.001, infinity }

	protected:
	PPtr<Rigidbody> m_ConnectedBody;
	
	friend class ::Rigidbody;
};

/// We need to to know the type of the desc so unfortunately this function must be inside the subclasses.
/// For reuse of code we do it with macros
#define IMPLEMENT_AXIS_ANCHOR(klass,nxdesc)\
void klass::ApplySetupAxesToDesc (int option)\
{\
	if (IsActive () && m_Joint)\
	{\
		nxdesc desc;\
		AssertIf (m_Joint->getState () == NX_JS_BROKEN);\
		GET_JOINT()->saveToDesc (desc);\
		SetupAxes (desc, option);\
		GET_JOINT()->loadFromDesc (desc);\
		AssertIf (m_Joint->getState () == NX_JS_BROKEN);\
	}\
}\
	
#define FINALIZE_CREATE(desc, type)\
	FinalizeCreateImpl (desc);\
	if (GET_JOINT ())\
		GET_JOINT ()->loadFromDesc (desc);\
	else\
		m_Joint = GetDynamicsScene ().createJoint (desc);\
	AssertIf (m_Joint && m_Joint->getState () == NX_JS_BROKEN);\
	Assert (!GetGameObject().IsDestroying ());

}

#endif
