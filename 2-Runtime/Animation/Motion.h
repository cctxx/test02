#pragma once

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Misc/UserList.h"
#include "Runtime/Math/Vector3.h"

class MessageIdentifier;
class AnimationClip;

class Motion : public NamedObject
{
public:	
	REGISTER_DERIVED_ABSTRACT_CLASS (Motion, NamedObject)

	Motion (MemLabelId label, ObjectCreationMode mode);

	void NotifyObjectUsers(const MessageIdentifier& msg);
	void AddObjectUser( UserList& user );

#if UNITY_EDITOR
	virtual float GetAverageDuration() = 0;
	virtual float GetAverageAngularSpeed() = 0;
	virtual Vector3f GetAverageSpeed() = 0;
	virtual float GetApparentSpeed() = 0;
	
	virtual bool ValidateIfRetargetable(bool showWarning = true) = 0;

	virtual bool IsLooping() = 0 ;

	virtual bool IsAnimatorMotion()const = 0;
	virtual bool IsHumanMotion() = 0;

#endif
	
	virtual void AddUser(UserList& dependencies) { dependencies.AddUser(GetUserList ());}
	
	UserList& GetUserList () { return m_ObjectUsers; }
	
private:
	
	UserList	m_ObjectUsers; 
};
