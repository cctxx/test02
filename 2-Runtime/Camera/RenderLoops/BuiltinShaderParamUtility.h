#pragma once


///@TODO: This should probably be 0. But for now we don't have proper ifdef support for switching to a different subshader.
#define LOD_FADE_DISABLED 0.999F

#define LOD_FADE_BATCH_EPSILON 0.0625 // 1/16

class GfxDevice;

void SetObjectScale (GfxDevice& device, float lodFade, float invScale);
