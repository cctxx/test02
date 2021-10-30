#pragma once

#include "Runtime/BaseClasses/BaseObject.h"

struct PPtrKeyframe
{
	float        time;
	PPtr<Object> value;
	
	DECLARE_SERIALIZE(PPtrKeyframe)
};
typedef dynamic_array<PPtrKeyframe> PPtrKeyframes;
