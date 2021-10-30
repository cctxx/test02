#include "UnityPrefix.h"
#include "Motion.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/MessageIdentifier.h"
#include "Runtime/Misc/UserList.h"

Motion::Motion (MemLabelId label, ObjectCreationMode mode) : Super (label, mode), m_ObjectUsers(this)
{ }

Motion::~Motion ()
{
	
}


void Motion::NotifyObjectUsers(const MessageIdentifier& msg)
{	
	m_ObjectUsers.SendMessage(msg);
}

void Motion::AddObjectUser( UserList& user ) 
{ 
	m_ObjectUsers.AddUser(user); 
}

IMPLEMENT_CLASS (Motion)

