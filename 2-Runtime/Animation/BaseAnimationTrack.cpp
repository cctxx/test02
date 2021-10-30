#include "UnityPrefix.h"

#if UNITY_EDITOR
#include "BaseAnimationTrack.h"

IMPLEMENT_CLASS (BaseAnimationTrack)

BaseAnimationTrack::BaseAnimationTrack(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{}

BaseAnimationTrack::~BaseAnimationTrack()
{}

#endif
