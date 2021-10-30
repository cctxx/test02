#pragma once

#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/mecanim/animation/streamedclip.h"

struct StreamedClipBuilder;

StreamedClipBuilder* CreateStreamedClipBuilder(UInt32 curveCount, UInt32 keyCount);
void DestroyStreamedClipBuilder(StreamedClipBuilder* builder);

template<class T>
void AddCurveToStreamedClip(StreamedClipBuilder* builder, int curveIndex, const AnimationCurveTpl<T>& curve);

void AddIntegerCurveToStreamedClip(StreamedClipBuilder* builder, int curveIndex, float* time, int* value, int count);

void CreateStreamClipConstant (StreamedClipBuilder* builder, mecanim::animation::StreamedClip& clip, mecanim::memory::Allocator& alloc);
