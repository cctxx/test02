#include "UnityPrefix.h"
#include "InputAxis.h"
#include "TimeManager.h"
#include "InputManager.h"

using namespace std;

InputAxis::InputAxis () {
	positiveButton = 0;
	negativeButton = 0;
	altPositiveButton = 0;
	altNegativeButton = 0;
	joyNum = 0;
	type=kAxisButton;
	dead = 0.001f;
	gravity = 0.0;
	sensitivity= .1f;
	invert = false;
	snap = false;
	value = 0.0f;
	axis = 0;
	descriptiveName="";
	descriptiveNegativeName="";
}

InputAxis::InputAxis (const string &name) {
	positiveButton = 0;
	negativeButton = 0;
	altPositiveButton = 0;
	altNegativeButton = 0;
	joyNum = 0;
	type=kAxisButton;
	dead = 0.001f;
	gravity = 0.0;
	sensitivity= .1f;
	invert = false;
	snap = false;
	value = 0.0f;
	axis = 0;
	m_Name = name;
	descriptiveName="";
	descriptiveNegativeName="";
}

void InputAxis::DoGravity (float time) {
	// Handle gravity
	if (gravity) {
		if (value > 0) {
			value -= gravity * time;
			if (value < 0)
				value = 0;
		} else if (value < 0) {
			value += gravity * time;
			if (value > 0)
				value = 0;
		}
	}
}

void InputAxis::Update () {
	float time = GetDeltaTime();
	Vector2f pos;
#if !ENABLE_NEW_EVENT_SYSTEM
	Vector3f delta;
#endif
	
//	Vector2f pos;
	int posFlag, negFlag;

	if (invert) 
		value = -value;
		
	switch (type) {
	case kAxisButton: 
		posFlag = GetInputManager().GetKey (positiveButton)||GetInputManager().GetKey (altPositiveButton);
		negFlag = GetInputManager().GetKey (negativeButton)||GetInputManager().GetKey (altNegativeButton);
		rawValue = 0.0F;

		// Lock if both up and down are held
		if (!(posFlag && negFlag)) {
			if (posFlag) {
				if (snap && value < 0.0)
					value = 0.0;
				else
					value += sensitivity * time;
				if (value < 0.0)
					value += gravity * time;
				
				value = min (1.0F, value);
				rawValue = 1.0F;
				
			}
			else if (negFlag) {
				if (snap && value > 0.0)
					value = 0.0;
				else
					value -= sensitivity * time;
				if (value > 0.0)
					value -= gravity * time;

				value = max (-1.0F, value);
				rawValue = -1.0F;

			} else 
				DoGravity (time);
		}
		
		break;

	case kAxisMouse:
#if ENABLE_NEW_EVENT_SYSTEM
		if (axis == 0)
			value = GetInputManager().GetMouseDelta().x;
		else if (axis == 1)
			value = GetInputManager().GetMouseDelta().y;
		else
			value = GetInputManager().GetMouseScroll().x;
#else
		delta = GetInputManager().GetMouseDelta ();
		if (axis == 0)
			value = delta.x;
		else if (axis == 1)
			value = delta.y;
		else
			value = delta.z;
#endif
		
		rawValue = value;
		value *= sensitivity;

		break;
	case kAxisJoystick:
		value = GetInputManager().GetJoystickPosition (joyNum, axis);
		rawValue = value;
		value *= sensitivity;
		
		
		if (value > 1.0F)
			value = 1.0F;
		else if (value < -1.0F)
			value = -1.0F;
		else if (value < dead && value > -dead) 
			value = 0.0F;
		else if (value > 0.0F)
			value = Lerp (0.0F, 1.0F, (value - dead) / (1.0F - dead));
		else 
			value = Lerp (0.0F, -1.0F, (-value - dead) / (1.0F - dead));
		break;
	}

	if (invert)
	{
		value = -value;
		rawValue = -rawValue;
	}
}


void InputAxis::MakeAnalogKey (int pos, int neg, int altpos, int altnegpos) {
	positiveButton = pos;
	negativeButton = neg;
	altPositiveButton = altpos;
	altNegativeButton = altnegpos;
	type = kAxisButton;
	sensitivity = 3;
	gravity = 3;
	snap = true;
}

void InputAxis::MakeButton (int button, int altbutton) {
	positiveButton = button;
	negativeButton = 0;
	altPositiveButton = altbutton;
	altNegativeButton = 0;
	type = kAxisButton;
	sensitivity = 1000;
	gravity = 1000;
	snap = false;
}

void InputAxis::MakeMouse (int a) {
	type = kAxisMouse; 
	axis = a;
	dead = 0.0;
	sensitivity = 0.1f;
}

void InputAxis::MakeJoystick (int a) {
	type = kAxisJoystick; 
	axis = a;
	sensitivity = 1.0F;
	dead = 0.19F;
	gravity = 0.0F;
	snap = false;
	invert = false;
}

UnityStr InputAxis::GetDescriptiveName(bool neg) {
	if(neg)
		if(descriptiveNegativeName.length())
			return descriptiveNegativeName;
		else 
			if(descriptiveName.length())
				return descriptiveName+" (-)";
			else
				return m_Name+" (-)";
	else
		if(descriptiveName.length())
			if(negativeButton&&!descriptiveNegativeName.length())
				return descriptiveName+" (+)";
			else
				return descriptiveName;
		else
			if(negativeButton)
				return m_Name+" (+)";
			else
				return m_Name;
}

float InputAxis::GetValueRaw () const
{
	if (type == kAxisButton)
		return rawValue;
	else
		return value;
}
