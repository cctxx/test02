#pragma once

#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/mecanim/animation/denseclip.h"

void CreateDenseClip(mecanim::animation::DenseClip& clip, UInt32 curveCount, float begin, float end, float sampleRate, mecanim::memory::Allocator& alloc);

template<class T>
void AddCurveToDenseClip(mecanim::animation::DenseClip& clip, int curveIndex, const AnimationCurveTpl<T>& curve);
