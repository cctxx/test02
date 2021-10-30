#ifndef AVATARCONTROLLER_H
#define AVATARCONTROLLER_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/BaseClasses/MessageIdentifier.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/Animation/MecanimUtility.h"
#include "Runtime/mecanim/animation/avatar.h"

#include "Runtime/Misc/UserList.h"

template<class T>
class PPtr;
class AnimationClip;
class StateMachine;
class AvatarMask;

class RuntimeAnimatorController;

typedef std::vector<PPtr<AnimationClip> > AnimationClipVector;

namespace UnityEngine 
{
	namespace Animation
	{
		struct AnimationSetBindings;
	}
}

namespace mecanim 
{
	namespace animation
	{
		struct ControllerConstant;
	}
}

class RuntimeAnimatorController : public NamedObject
{

public:

	REGISTER_DERIVED_ABSTRACT_CLASS (RuntimeAnimatorController, NamedObject)
	DECLARE_OBJECT_SERIALIZE (RuntimeAnimatorController)

	RuntimeAnimatorController(MemLabelId label, ObjectCreationMode mode);


	static void InitializeClass ();
	static void CleanupClass () {}

	virtual mecanim::animation::ControllerConstant*	GetAsset() = 0 ;
	virtual UnityEngine::Animation::AnimationSetBindings* GetAnimationSetBindings() = 0 ;				
	
	virtual AnimationClipVector	GetAnimationClips()  const = 0;	 

	virtual std::string	StringFromID(unsigned int ID) const = 0;		

	void AddObjectUser( UserList& user ) { m_ObjectUsers.AddUser(user); }
	void AddObjectUser( UserListNode& user ) { m_ObjectUsers.AddUser(user); }

	void NotifyObjectUsers(const MessageIdentifier& msg);
	UserList& GetUserList () { return m_ObjectUsers; }
		

protected:
	
	virtual void RegisterAnimationClips() ;

	UserList								m_ObjectUsers;		// for animatorControllers
	UserList								m_DependencyList;	// for animationclips

private:

	virtual AnimationClipVector GetAnimationClipsToRegister() const = 0;
	
		
};


#endif
