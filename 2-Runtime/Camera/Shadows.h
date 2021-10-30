#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_SHADOWS

#include "ShadowSettings.h"
#include "ShadowCulling.h"

class MinMaxAABB;
class Transform;
class Light;
class RenderTexture;
class Matrix4x4f;
struct ShadowedLight;
struct ActiveLight;
struct CullResults;

struct ShadowCameraData : public ShadowCullData
{
	ShadowCameraData(const ShadowCullData& cullData) : ShadowCullData(cullData) {}

	int			splitCount;
	float		splitDistances[kMaxShadowCascades+1];
	float		splitPercentages[kMaxShadowCascades+1];
	Vector4f	splitSphereCentersAndSquaredRadii[kMaxShadowCascades];
	int			qualityShift; // shadow quality (0 = best, larger numbers worse)
};

bool GetSoftShadowsEnabled ();

void SetShadowsKeywords (LightType lightType, ShadowType shadowType, bool screen, bool enableSoftShadows);

// outDistances is array of size [splitCount+1] with near&far distances for each split
// outPercentages is the same, only percentages of shadowFarPlane distance (0.5 = 50% of distance)
void CalculatePSSMDistances (float nearPlane, float shadowFarPlane, int splitCount, float* outDistances, float* outPercentages);

RenderTexture* RenderShadowMaps (ShadowCameraData& cameraData, const ActiveLight& activeLight, const MinMaxAABB& receiverBounds, bool excludeLightmapped, Matrix4x4f outShadowMatrices[kMaxShadowCascades]);
RenderTexture* BlurScreenShadowMap (RenderTexture* screenShadowMap, ShadowType shadowType, float farPlane, float blurWidth, float blurFade);

// Does not set first shadow matrix!
void SetCascadedShadowShaderParams (const Matrix4x4f* shadowMatrices, const float* splitDistances, const Vector4f* splitSphereCentersAndSquaredRadii);

#endif // ENABLE_SHADOWS


bool CheckPlatformSupportsShadows ();
void SetNoShadowsKeywords ();
