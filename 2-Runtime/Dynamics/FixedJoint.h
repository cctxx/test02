#ifndef FIXEDJOINT_H
#define FIXEDJOINT_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Vector3.h"
#include "JointDescriptions.h"
#include "Joint.h"
class Rigidbody;
class NxJointDesc;
class NxRevoluteJoint;

namespace Unity
{

class FixedJoint : public Joint
{
	public:
	
	REGISTER_DERIVED_CLASS (FixedJoint, Joint)
	DECLARE_OBJECT_SERIALIZE (FixedJoint)
	
	FixedJoint (MemLabelId label, ObjectCreationMode mode);

	private:

	virtual void ApplySetupAxesToDesc (int option);
	virtual void Create ();
};

}

#endif
