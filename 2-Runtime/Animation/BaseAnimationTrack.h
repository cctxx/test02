#if UNITY_EDITOR
#ifndef BASEANIMATIONTRACK_H
#define BASEANIMATIONTRACK_H

#include "Runtime/BaseClasses/NamedObject.h"

template<class T> class AnimationCurveTpl;
typedef AnimationCurveTpl<float> AnimationCurve;

class BaseAnimationTrack : public NamedObject
{
	public: 
	
	REGISTER_DERIVED_ABSTRACT_CLASS (BaseAnimationTrack, NamedObject)
	
	BaseAnimationTrack(MemLabelId label, ObjectCreationMode mode);
	// ~BaseAnimationTrack (); declared-by-macro
};

#endif
#endif
