
#include "UnityPrefix.h"
#include "AnimatorControllerParameter.h"

#include "Runtime/Animation/AnimatorController.h"
#include "Runtime/Allocator/AllocationHeader.h"

#include "Editor/Src/Animation/StateMachine.h"

#define DIRTY_AND_INVALIDATE() m_Controller->OnInvalidateAnimatorController(); m_Controller->SetDirty(); 

INSTANTIATE_TEMPLATE_TRANSFER(AnimatorControllerParameter)

template<class TransferFunction>
void AnimatorControllerParameter::Transfer (TransferFunction& transfer)
{	
	TRANSFER (m_Name);

	TRANSFER_ENUM(m_Type);
	
	TRANSFER(m_DefaultFloat);
	TRANSFER(m_DefaultInt);
	TRANSFER(m_DefaultBool);

	transfer.Align();
	TRANSFER(m_Controller);	

}

AnimatorControllerParameter::AnimatorControllerParameter ()
:	m_Type(AnimatorControllerParameterTypeFloat), 
	m_DefaultFloat(0), 
	m_DefaultBool(false), 
	m_DefaultInt(0),
	m_Controller(0)
{
}


char const* AnimatorControllerParameter::GetName() const
{
	return m_Name.c_str();
}

void AnimatorControllerParameter::SetName(char const *name)
{
	if (strcmp (GetName(), name) != 0)
	{ 
		// make name unique
		string uniqueName = m_Controller->MakeUniqueParameterName(name);

		// rename all refering the parameter
		for(int i = 0 ; i < m_Controller->GetLayerCount(); i++)
		{
			if(m_Controller->GetLayer(i)->GetStateMachine())
				m_Controller->GetLayer(i)->GetStateMachine()->RenameParameter(uniqueName, GetName());
		}		

		m_Name = uniqueName;
		DIRTY_AND_INVALIDATE();
	}
}

AnimatorControllerParameterType AnimatorControllerParameter::GetType()const
{
	return m_Type;
}

void AnimatorControllerParameter::SetType(AnimatorControllerParameterType type)
{
	if(m_Type != type)
	{
		m_Type = type;
		DIRTY_AND_INVALIDATE();
	}
}

float AnimatorControllerParameter::GetDefaultFloat()const
{
	if( m_Type == AnimatorControllerParameterTypeFloat)
	{
		return m_DefaultFloat;
	}
	else
	{
		ErrorString("Can't get DefaultFloat for a Parameter that is not of type AnimatorControllerParameterTypeFloat");
		return 0;
	}
}

void AnimatorControllerParameter::SetDefaultFloat(float val)
{
	if( m_Type == AnimatorControllerParameterTypeFloat)
	{
		if(m_DefaultFloat != val)
		{
			m_DefaultFloat = val;
			DIRTY_AND_INVALIDATE();
		}
	}
	else
	{
		ErrorString("Can't set DefaultFloat for a Parameter that is not of type AnimatorControllerParameterTypeFloat");
	}

}

int AnimatorControllerParameter::GetDefaultInt() const
{
	if( m_Type == AnimatorControllerParameterTypeInt)
	{
		return m_DefaultInt;
	}
	else
	{
		ErrorString("Can't get DefaultInt for a Parameter that is not of type AnimatorControllerParameterTypeInt");
		return 0;
	}
}

void AnimatorControllerParameter::SetDefaultInt(int val)
{
	if( m_Type == AnimatorControllerParameterTypeInt)
	{
		if(m_DefaultInt != val)
		{
			m_DefaultInt = val;
			DIRTY_AND_INVALIDATE();
		}
	}
	else
	{
		ErrorString("Can't set DefaultInt for a Parameter that is not of type AnimatorControllerParameterTypeInt");
	}
}

bool AnimatorControllerParameter::GetDefaultBool() const
{
	if( m_Type == AnimatorControllerParameterTypeBool || m_Type == AnimatorControllerParameterTypeTrigger)
	{
		return m_DefaultBool;
	}
	else
	{
		ErrorString("Can't get DefaultBool for a Parameter that is not of type AnimatorControllerParameterTypeBool or AnimatorControllerParameterTypeTrigger");
		return false;
	}
}

void AnimatorControllerParameter::SetDefaultBool(bool val)
{
	if( m_Type == AnimatorControllerParameterTypeBool || m_Type == AnimatorControllerParameterTypeTrigger)
	{
		if(m_DefaultBool != val)
		{
			m_DefaultBool = val;
			DIRTY_AND_INVALIDATE();
		}
	}
	else
	{
		ErrorString("Can't set DefaultBool for a Parameter that is not of type AnimatorControllerParameterTypeBool or AnimatorControllerParameterTypeTrigger");
	}
}

AnimatorController* AnimatorControllerParameter::GetController() const
{
	return m_Controller;
}

void AnimatorControllerParameter::SetController(AnimatorController* controller)
{
	m_Controller = controller;
}
