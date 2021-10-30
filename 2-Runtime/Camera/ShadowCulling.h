#pragma once

#include "Configuration/UnityConfigure.h"

#include "Lighting.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Geometry/Sphere.h"
#include "Runtime/BaseClasses/Tags.h"
#include "CullingParameters.h"
#include "ShaderReplaceData.h"

const int kMaxShadowCascades = 4;

class Camera;
class Light;
class Transform;
class AABB;
class MinMaxAABB;
struct SceneNode;
struct CullingOutput;
struct SceneCullState;
struct SceneCullingParameters;
class Shader;

namespace Unity { class Material; }
namespace Umbra { class Visibility; class Tome; }

struct ShadowCullData
{
	Camera*		camera;
	Matrix4x4f	cameraClipToWorld;
	Matrix4x4f	cameraWorldToClip;
	Matrix4x4f	actualWorldToClip;
	Vector3f	eyePos;
	Vector3f	viewDir;
	float		viewWidth, viewHeight; // in pixels
	float		layerCullDistances[kNumLayers];
	bool		layerCullSpherical;
	float		baseFarDistance;
	Plane		cameraCullPlanes[6];
	Plane		shadowCullPlanes[6];
	Vector3f	shadowCullCenter;
	float		shadowCullRadius;
	float		shadowCullSquareRadius;
	bool		useSphereCulling;
	float		shadowDistance;
	float		projectionNear;
	float		projectionFar;
	float		farPlaneScale;

	const CullingOutput* visbilityForShadowCulling;
	const SceneCullingParameters* sceneCullParameters;

	ShaderReplaceData shaderReplace;
};

struct ShadowCasterCull
{
	enum { kMaxPlanes = 10 };
	Plane planes[kMaxPlanes];
	int planeCount;
};

struct ShadowCascadeInfo
{
	bool enabled;
	Matrix4x4f lightMatrix;
	Matrix4x4f viewMatrix;
	Matrix4x4f projMatrix;
	Matrix4x4f worldToClipMatrix;
	Matrix4x4f shadowMatrix;
	Sphere outerSphere;
	float minViewDistance;
	float maxViewDistance;
	float nearPlane;
	float farPlane;
};

struct ShadowCasterPartData
{
	int	subMeshIndex;					// 4
	int	subShaderIndex;					// 4
	Shader* shader;						// 4
	Unity::Material* material;			// 4
										// 16 bytes
};

struct ShadowCasterData
{
	const SceneNode* node;				// 4
	const AABB*		worldAABB;			// 4
	size_t			partsStartIndex;	// 4
	size_t			partsEndIndex;		// 4
	UInt32			visibleCascades;	// 4
										// 20 bytes
};

typedef UNITY_TEMP_VECTOR(ShadowCasterData) ShadowCasters;
typedef UNITY_TEMP_VECTOR(ShadowCasterPartData) ShadowCasterParts;

void CalculateShadowCasterCull(const Plane frustumCullPlanes[6], const Matrix4x4f& clipToWorld, const Vector3f& centerWorld, float nearPlaneScale, float farPlaneScale, LightType lightType, const Vector3f& lightVec, ShadowCasterCull& result, const bool alreadyTested[kPlaneFrustumNum]);
void CalculateShadowCasterCull (const Plane frustumClipPlanes[6], const Matrix4x4f& clipToWorld, const Vector3f& centerWorld, float nearPlaneScale, float farPlaneScale, LightType lightType, const Transform& trans, ShadowCasterCull& result);
void SetupShadowCullData (Camera& camera, const Vector3f& cameraPos, const ShaderReplaceData& shaderReplaceData, const SceneCullingParameters* sceneCullParams, ShadowCullData& cullData);
float CalculateShadowDistance (const Camera& camera);
float CalculateShadowSphereOffset (const Camera& camera);
bool CalculateSphericalShadowRange (const Camera& camera, Vector3f& outCenter, float& outRadius);
void CalculateLightShadowFade (const Camera& camera, float shadowStrength, Vector4f& outParams, Vector4f& outCenterAndType);

void CullDirectionalCascades(ShadowCasters& casters, const ShadowCascadeInfo cascades[kMaxShadowCascades], int cascadeCount,
							 const Quaternionf& lightRot, const Vector3f& lightDir, const ShadowCullData& cullData);

void CullShadowCasters (const Light& light, const ShadowCullData& cullData, bool excludeLightmapped, CullingOutput& cullingOutput  );

void GenerateShadowCasterParts (const Light& light, const ShadowCullData& cullData, const CullingOutput& visibleRenderers, MinMaxAABB& casterBounds, ShadowCasters& casters, ShadowCasterParts& casterParts);


bool IsObjectWithinShadowRange (const ShadowCullData& shadowCullData, const AABB& bounds);
