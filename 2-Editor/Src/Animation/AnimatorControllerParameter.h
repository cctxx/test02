
#pragma once

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Math/Vector3.h"

class AnimatorController;

// This enum need to match 
//	Runtime/mecanim/generic/typetraits.h ValueType enum
//	Editor/Mono/AnimatorControllerBindings.txt AnimatorControllerParameterType enum
enum AnimatorControllerParameterType
{
	AnimatorControllerParameterTypeFloat = 1,
	AnimatorControllerParameterTypeInt = 3,
	AnimatorControllerParameterTypeBool = 4,
	AnimatorControllerParameterTypeTrigger = 9,
};


class AnimatorControllerParameter 
{
public :

	AnimatorControllerParameter();	
	
	DECLARE_SERIALIZE (AnimatorControllerParameter)	

	char const* GetName() const;
	void SetName(char const *name);

	AnimatorControllerParameterType GetType()const;
	void SetType(AnimatorControllerParameterType type);

	float GetDefaultFloat()const;
	void SetDefaultFloat(float val);

	int GetDefaultInt() const;
	void SetDefaultInt(int val);

	bool GetDefaultBool() const;
	void SetDefaultBool(bool val);

	AnimatorController* GetController() const;
	void SetController(AnimatorController* controller);

private:
	UnityStr						m_Name;
	AnimatorControllerParameterType m_Type;

	float							m_DefaultFloat;		
	int								m_DefaultInt;	
	bool							m_DefaultBool;

private:

	PPtr<AnimatorController>		 m_Controller ; // used to set unique name

};
