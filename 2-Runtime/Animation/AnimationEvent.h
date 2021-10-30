#ifndef ANIMATIONEVENT_H
#define ANIMATIONEVENT_H

#include "Runtime/BaseClasses/GameObject.h"
class AnimationState;

struct AnimationEvent
{
	DECLARE_SERIALIZE (AnimationEvent)

	float        time;
	UnityStr     functionName;
	UnityStr     stringParameter;
	PPtr<Object> objectReferenceParameter;
	float        floatParameter;
	int          intParameter;
	
	int		     messageOptions;
	mutable AnimationState* stateSender;
	
	AnimationEvent() { messageOptions = 0; stateSender = NULL; floatParameter = 0.0F; intParameter = 0; } 
	
	friend bool operator < (const AnimationEvent& lhs, const AnimationEvent& rhs) { return lhs.time < rhs.time; }
};

bool FireEvent (AnimationEvent& event, AnimationState* state, Unity::Component& animation);


#endif
