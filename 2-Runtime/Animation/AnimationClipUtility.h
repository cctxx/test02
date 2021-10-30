#pragma once

class AnimationClip;

void ClipAnimation (AnimationClip& sourceClip, AnimationClip& destinationClip, float startTimeSeconds, float endTimeSeconds, bool duplicateLastFrame);
void CopyAnimation (AnimationClip& sourceClip, AnimationClip& destinationClip);
