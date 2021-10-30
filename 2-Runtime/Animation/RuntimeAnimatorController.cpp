#include "UnityPrefix.h"

#include "RuntimeAnimatorController.h"
#include "AnimationSetBinding.h"


#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/Blobification/BlobWrite.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "AnimationClip.h"

#if UNITY_EDITOR
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Mono/MonoManager.h"
#endif

#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Scripting/Scripting.h"

IMPLEMENT_OBJECT_SERIALIZE (RuntimeAnimatorController)
IMPLEMENT_CLASS(RuntimeAnimatorController)
INSTANTIATE_TEMPLATE_TRANSFER(RuntimeAnimatorController)


RuntimeAnimatorController::RuntimeAnimatorController(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode), 	
	m_ObjectUsers(this),
	m_DependencyList(this)
{

}

RuntimeAnimatorController::~RuntimeAnimatorController()
{		
	NotifyObjectUsers( kDidModifyAnimatorController );
}


template<class TransferFunction>
void RuntimeAnimatorController::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
}


void RuntimeAnimatorController::NotifyObjectUsers(const MessageIdentifier& msg)
{	
	m_ObjectUsers.SendMessage(msg);
}

void RuntimeAnimatorController::RegisterAnimationClips() 
{		
	AnimationClipVector clips = GetAnimationClipsToRegister();
	m_DependencyList.Clear();
	m_DependencyList.Reserve(clips.size()); // Reserve space just for niceness
	for(int i = 0 ; i < clips.size() ; i++)
	{	
		AnimationClip* clip = clips[i];		
		if (clip)
		{		
			// We could do this either way, adding is symmetrical
			clip->GetUserList().AddUser(m_DependencyList);
		}
	}
}

