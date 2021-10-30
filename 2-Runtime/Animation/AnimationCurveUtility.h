#pragma once

#include "Runtime/Math/AnimationCurve.h"


void EulerToQuaternionCurve (const AnimationCurve& curveX, const AnimationCurve& curveY, const AnimationCurve& curveZ, AnimationCurveQuat& collapsed);
void QuaternionCurveToEulerCurve (AnimationCurveQuat& quat, AnimationCurve* outCurves[3]);

/// /curve/ is the input float curve
/// /index/ is the component index in the collapsed curve (x,y,z,w -> usually between 0 and 3)
/// /collapsed/ is the curve
/// This functioncall is usually called as many times as there are components (for a quaternion curve 4 times, Vector3 curve 3 times)
template<class T>
void CombineCurve (const AnimationCurve& curve, int index, AnimationCurveTpl<T>& collapsed);

void ExpandVector3Curve (AnimationCurveVec3& quat, AnimationCurve* outCurves[3]);
void ExpandQuaternionCurve (AnimationCurveQuat& quat, AnimationCurve* outCurves[4]);

template<class T>
void AddLoopingFrame (AnimationCurveTpl<T>& curve, float time);

template<class T>
bool ClipAnimationCurve (const AnimationCurveTpl<T>& sourceCurve, AnimationCurveTpl<T>& curve, float begin, float end);

int UpdateCurveKey (AnimationCurve& curve, int index, const AnimationCurve::Keyframe& value);
int MoveCurveKey (AnimationCurve& curve, int index, AnimationCurve::Keyframe value);

// Calculates the keyframes in/out slope based on the bias parameter creating a smooth spline.
// a bias of zero is default.
// a positive bias bends the curve to the next key.
// a negative bias bends the curve to the previous key.
void RecalculateSplineSlope (AnimationCurveTpl<float>& curve, int key, float bias = 0.0F);

template<class T>
T InterpolateKeyframe (const KeyframeTpl<T>& lhs, const KeyframeTpl<T>& rhs, float curveT);

int AddInbetweenKey (AnimationCurve& curve, float curveT);

template<class T>
KeyframeTpl<T> CalculateInbetweenKey(const KeyframeTpl<T>& lhs, const KeyframeTpl<T>& rhs, float curveT);

template<class T>
KeyframeTpl<T> CalculateInbetweenKey(const AnimationCurveTpl<T>& curve, float curveT);

int AddKeySmoothTangents (AnimationCurve& curve, float time, float value);

template<class T>
void RecalculateSplineSlope (AnimationCurveTpl<T>& curve);

template<class T>
void RecalculateSplineSlopeLinear (AnimationCurveTpl<T>& curve, int key);

template<class T>
void RecalculateSplineSlopeLinear (AnimationCurveTpl<T>& curve);

void EnsureQuaternionContinuityLoopFrame (AnimationCurveQuat& curve);
void EnsureQuaternionContinuityPreserveSlope (AnimationCurveQuat& curve);
void EnsureQuaternionContinuityAndRecalculateSlope (AnimationCurveQuat& curve);

template<class T>
void RecalculateSplineSlopeLoop (AnimationCurveTpl<T>& curve, int key, float b);

// Fits tangents key0.outSlope and key1.inSlope to the point value1 and value2
// value1 and value2 - points to fit (at time1 and time2)
template <class T>
void FitTangents(KeyframeTpl<T>& key0, KeyframeTpl<T>& key1, float time1, float time2, const T& value1, const T& value2);

