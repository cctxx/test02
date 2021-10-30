#pragma once

class AnimationClip;
#include "Runtime/Math/AnimationCurve.h"

// Rotation error is defined as maximum angle deviation allowed in degrees
// For others it is defined as maximum distance/delta deviation allowed in percents
void ReduceKeyframes (AnimationClip& clip, float rotationError, float positionError, float scaleError, float floatError);

void EulerToQuaternionCurveBake (const AnimationCurve& curveX, const AnimationCurve& curveY, const AnimationCurve& curveZ, AnimationCurveQuat& collapsed, float sampleRate);
