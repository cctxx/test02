#include "UnityPrefix.h"
#include "ShadowCulling.h"
#include "Runtime/Camera/Light.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Geometry/Sphere.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/intshader.h"
#include "Runtime/Camera/BaseRenderer.h"
#include "Runtime/Camera/CullResults.h"
#include "Runtime/Camera/SceneCulling.h"
#include "Runtime/Camera/SceneNode.h"
#include "Runtime/Camera/UnityScene.h"
#include "Runtime/Geometry/BoundingUtils.h"

#include "SceneCulling.h"

PROFILER_INFORMATION(gShadowsCullCastersPoint, "Shadows.CullCastersPoint", kProfilerRender)
PROFILER_INFORMATION(gShadowsCullCastersSpot, "Shadows.CullCastersSpot", kProfilerRender)
PROFILER_INFORMATION(gShadowsCullCastersDir, "Shadows.CullCastersDir", kProfilerRender)

static const float kShadowFadeRange = 0.2f;

static bool AddShadowCasterCullPlane(const Plane frustumCullPlanes[6], const Matrix4x4f& clipToWorld, const Vector3f& centerWorld, float nearPlaneScale, float farPlaneScale, int sideA, int sideB, int sideNeighbor, LightType lightType, const Vector3f& lightVec, Plane& outPlane)
{
	static Vector3f sideOffsets[] = {
		Vector3f(-1.0f,  0.0f,  0.0f),	// kPlaneFrustumLeft
		Vector3f( 1.0f,  0.0f,  0.0f),	// kPlaneFrustumRight
		Vector3f( 0.0f, -1.0f,  0.0f),	// kPlaneFrustumBottom
		Vector3f( 0.0f,  1.0f,  0.0f),	// kPlaneFrustumTop
		Vector3f( 0.0f,  0.0f, -1.0f),	// kPlaneFrustumNear
		Vector3f( 0.0f,  0.0f,  1.0f)	// kPlaneFrustumFar
	};

	const Plane& planeA = frustumCullPlanes[sideA];
	const Plane& planeB = frustumCullPlanes[sideB];
	Vector3f edgeNormal = planeA.GetNormal() + planeB.GetNormal(); // No need to normalize
	Vector3f edgeCenter = sideOffsets[sideA] + sideOffsets[sideB];
	Vector3f edgeDir = sideOffsets[sideNeighbor];
	Vector3f posWorld1, posWorld2;
	Vector3f posLocal1 = edgeCenter - edgeDir;
	Vector3f posLocal2 = edgeCenter + edgeDir;
	clipToWorld.PerspectiveMultiplyPoint3( posLocal1, posWorld1 );
	clipToWorld.PerspectiveMultiplyPoint3( posLocal2, posWorld2 );
	float scale1 = (posLocal1.z < 0.0f) ? nearPlaneScale : farPlaneScale;
	float scale2 = (posLocal2.z < 0.0f) ? nearPlaneScale : farPlaneScale;
	posWorld1 = centerWorld + (posWorld1 - centerWorld) * scale1;
	posWorld2 = centerWorld + (posWorld2 - centerWorld) * scale2;

	if( lightType == kLightDirectional )
	{
		/////@TODO: RUnning around in debug mode this seems to hit quite often in the shantytown scene...
		Vector3f edgeDirWorld = (posWorld2 != posWorld1) ? Normalize( posWorld2 - posWorld1 ) : Vector3f(0.0f, 0.0f, 0.0f);
		Vector3f clipNormal = Cross( edgeDirWorld, lightVec );
		float len = Magnitude( clipNormal );
		// Discard plane if edge is almost parallel with light dir
		if (len < 0.001f)
			return false;
		clipNormal /= len;

		// Flip the normal if we got the wrong direction
		if( Dot( clipNormal, edgeNormal ) < 0.0f )
		{
			clipNormal *= -1.0f;
		}
		outPlane.SetNormalAndPosition( clipNormal, posWorld1 );
	}
	else
	{
		// We know the light is not too close to either plane
		outPlane.Set3Points( posWorld1, posWorld2, lightVec );

		if( Dot( outPlane.GetNormal(), edgeNormal ) < 0.0f )
		{
			outPlane *= -1.0f;
		}
	}
	return true;
}

void CalculateShadowCasterCull(const Plane frustumCullPlanes[6], const Matrix4x4f& clipToWorld,
							   const Vector3f& centerWorld, float nearPlaneScale, float farPlaneScale,
							   LightType lightType, const Vector3f& lightVec, ShadowCasterCull& result,
							   const bool alreadyTested[kPlaneFrustumNum])
{
	// Create clip volume from the distant planes and silhouette edges of camera's frustum (as seen by the light)
	// Maximum planes generated should be 10, but we check at runtime to be sure
	result.planeCount = 0;
	bool facingLight[kPlaneFrustumNum];
	bool enablePlane[kPlaneFrustumNum];
	float epsilon = 0.01f;
	for( int i = 0; i < kPlaneFrustumNum; ++i )
	{
		Plane plane = frustumCullPlanes[i];
		if( lightType == kLightDirectional )
		{
			// Compare light direction and normal
			float d = Dot( plane.GetNormal(), lightVec );
			facingLight[i] = d < 0.0f;
			enablePlane[i] = true;
		}
		else
		{
			float dist = plane.GetDistanceToPoint( lightVec );
			facingLight[i] = false;
			enablePlane[i] = true;
			if( dist > -epsilon )
			{
				// Light is inside or just outside the frustum
				facingLight[i] = true;
				if ( dist < epsilon )
				{
					// Point is too close to the plane to care about edges
					enablePlane[i] = false;
				}
				if ( dist < 0.0f )
				{
					// Push plane slightly so the frustum encloses the light
					plane.distance -= dist;
				}
			}
		}
		if( facingLight[i] && !alreadyTested[i] )
		{
			result.planes[result.planeCount++] = plane;
			if( result.planeCount == ShadowCasterCull::kMaxPlanes )
				return;
		}
	}

	// Sides in running order (each plane touches previous)
	static int camera4Sides[] = {
		kPlaneFrustumLeft,
		kPlaneFrustumBottom,
		kPlaneFrustumRight,
		kPlaneFrustumTop
	};
	static int cameraNearFar[] = {
		kPlaneFrustumNear,
		kPlaneFrustumFar
	};

	// Iterate over diagonal edges
	for( int edge = 0; edge < 4; ++edge )
	{
		int sideA = camera4Sides[edge];
		// If we already tested side A, we also tested B
		if( alreadyTested[sideA] )
			continue;
		int sideB = camera4Sides[(edge + 1) % 4];
		int sideNeighbor = kPlaneFrustumFar;
		if( facingLight[sideA] != facingLight[sideB] &&
			enablePlane[sideA] && enablePlane[sideB] )
		{
			Plane& plane = result.planes[result.planeCount];
			bool success = AddShadowCasterCullPlane(frustumCullPlanes, clipToWorld,
				centerWorld, nearPlaneScale, farPlaneScale,
				sideA, sideB, sideNeighbor, lightType, lightVec, plane);
			result.planeCount += success ? 1 : 0;
			if( result.planeCount == ShadowCasterCull::kMaxPlanes )
				return;
		}
	}

	// Iterate over near and far edges
	for( int nearFar = 0; nearFar < 2; ++nearFar )
	{
		int sideA = cameraNearFar[nearFar];
		// If we already tested side A, we also tested B
		if( alreadyTested[sideA] )
			continue;
		for( int edge = 0; edge < 4; ++edge )
		{
			int sideB = camera4Sides[edge];
			int sideNeighbor = camera4Sides[(edge + 1) % 4];
			if( facingLight[sideA] != facingLight[sideB] &&
				enablePlane[sideA] && enablePlane[sideB] )
			{
				Plane& plane = result.planes[result.planeCount];
				bool success = AddShadowCasterCullPlane(frustumCullPlanes, clipToWorld,
					centerWorld, nearPlaneScale, farPlaneScale,
					sideA, sideB, sideNeighbor, lightType, lightVec, plane);
				result.planeCount += success ? 1 : 0;
				if( result.planeCount == ShadowCasterCull::kMaxPlanes )
					return;
			}
		}
	}
}

static const bool s_AlreadyTestedNone[kPlaneFrustumNum] = { false, false, false, false, false, false };

void CalculateShadowCasterCull(const Plane frustumCullPlanes[6], const Matrix4x4f& clipToWorld, const Vector3f& centerWorld, float nearPlaneScale, float farPlaneScale, LightType lightType, const Transform& trans, ShadowCasterCull& result)
{
	// Get position for positional lights and direction for directional lights
	Vector3f lightVec;
	if( lightType == kLightDirectional )
	{
		lightVec = trans.TransformDirection( Vector3f::zAxis );
	}
	else
	{
		Assert( lightType == kLightPoint || lightType == kLightSpot );
		lightVec = trans.GetPosition();
	}
	CalculateShadowCasterCull(frustumCullPlanes, clipToWorld, centerWorld, nearPlaneScale, farPlaneScale, lightType, lightVec, result, s_AlreadyTestedNone);
}

static bool CanUseProjectionForShadows (const Matrix4x4f& clipToWorld, float farNearRatio, const Camera& camera, const Vector3f& cameraPos)
{
	// Check if eye position is the focal point of the camera projection
	// and we can scale near plane from the eye pos to get the far plane.
	// This works for asymmetric projections, not for oblique etc.
	Vector3f cameraFrustum[8];
	GetFrustumPoints(clipToWorld, cameraFrustum);
	for (int i = 0; i < 4; i++)
	{
		const Vector3f& nearPos = cameraFrustum[i];
		const Vector3f& farPos = cameraFrustum[i + 4];
		Vector3f derivedFar = cameraPos + (nearPos - cameraPos) * farNearRatio;
		float diff = SqrMagnitude(derivedFar - farPos);
		float length = SqrMagnitude(farPos - nearPos);
		if (!(diff <= length * 0.01f))
		{
			return false;
		}
	}
	return true;
}

void SetupShadowCullData (Camera& camera, const Vector3f& cameraPos, const ShaderReplaceData& shaderReplaceData, const SceneCullingParameters* sceneCullParams, ShadowCullData& cullData)
{
	const Rectf screenRect = camera.GetScreenViewportRect();
	float shadowDistance = CalculateShadowDistance (camera);

	Vector3f viewDir = -NormalizeSafe(camera.GetCameraToWorldMatrix().GetAxisZ());

	cullData.camera = &camera;
	cullData.eyePos = cameraPos;
	cullData.viewDir = viewDir;
	cullData.shadowDistance = shadowDistance;
	cullData.projectionNear = camera.GetProjectionNear();
	cullData.projectionFar = camera.GetProjectionFar();
	cullData.farPlaneScale = shadowDistance / cullData.projectionFar;
	cullData.viewWidth = screenRect.width;
	cullData.viewHeight = screenRect.height;
	cullData.actualWorldToClip = camera.GetWorldToClipMatrix();
	cullData.visbilityForShadowCulling = NULL;
	Matrix4x4f::Invert_Full(cullData.actualWorldToClip, cullData.cameraClipToWorld);

	float farNearRatio = cullData.projectionFar / cullData.projectionNear;
	if (!CanUseProjectionForShadows(cullData.cameraClipToWorld, farNearRatio, camera, cameraPos))
	{
		// Use implicit projection instead (which we used always before 3.5)
		Matrix4x4f proj;
		camera.GetImplicitProjectionMatrix(camera.GetNear(), camera.GetFar(), proj);
		MultiplyMatrices4x4 (&proj, &camera.GetWorldToCameraMatrix(), &cullData.cameraWorldToClip);
		Matrix4x4f::Invert_Full(cullData.cameraWorldToClip, cullData.cameraClipToWorld);
	}
	else
		cullData.cameraWorldToClip = cullData.actualWorldToClip;

	camera.CalculateFrustumPlanes(cullData.shadowCullPlanes, cullData.cameraWorldToClip, shadowDistance, cullData.baseFarDistance, true);
	for( int i = 0; i < kPlaneFrustumNum; i++ )
		cullData.cameraCullPlanes[i] = cullData.shadowCullPlanes[i];
	cullData.cameraCullPlanes[kPlaneFrustumFar].distance = cullData.baseFarDistance + camera.GetFar();
	cullData.shadowCullCenter = Vector3f::zero;
	float cullRadius = 1e15f;
	cullData.useSphereCulling = CalculateSphericalShadowRange(camera, cullData.shadowCullCenter, cullRadius);
	cullData.shadowCullRadius = cullRadius;
	cullData.shadowCullSquareRadius = cullRadius * cullRadius;
	const float* layerCullDistances = camera.GetLayerCullDistances();
	std::copy(layerCullDistances, layerCullDistances + kNumLayers, cullData.layerCullDistances);
	cullData.layerCullSpherical = camera.GetLayerCullSpherical();
	cullData.shaderReplace = shaderReplaceData;
	cullData.sceneCullParameters = sceneCullParams;
}

float CalculateShadowDistance (const Camera& camera)
{
	return std::min (QualitySettings::GetShadowDistanceForRendering(), camera.GetFar());
}

float CalculateShadowSphereOffset (const Camera& camera)
{
	float fov = camera.GetFov();
	static float maxDegrees = 180.0f;
	const float maxOffset = (1.0f - kShadowFadeRange) * 0.5f;
	float weight = clamp(1.0f - fov / maxDegrees, 0.0f, 1.0f);
	return maxOffset * weight;
}

bool CalculateSphericalShadowRange (const Camera& camera, Vector3f& outCenter, float& outRadius)
{
	const QualitySettings::QualitySetting& quality = GetQualitySettings().GetCurrent();
	if (quality.shadowProjection == kShadowProjCloseFit)
		return false;

	outCenter = camera.GetPosition();
	outRadius = CalculateShadowDistance(camera);

	float sphereOffset = CalculateShadowSphereOffset(camera);
	Vector3f vectorOffset(0, 0, -sphereOffset * outRadius);
	outCenter += camera.GetCameraToWorldMatrix().MultiplyVector3(vectorOffset);
	outRadius *= (1.0f - sphereOffset);
	return true;

	Assert(quality.shadowProjection == kShadowProjStableFit);
	return true;
}

void CalculateLightShadowFade (const Camera& camera, float shadowStrength, Vector4f& outParams, Vector4f& outCenterAndType)
{
	float shadowDistance = CalculateShadowDistance(camera);
	float shadowRange = shadowDistance;
	Vector3f center = camera.GetPosition();
	bool spherical = CalculateSphericalShadowRange(camera, center, shadowRange);

	// R = 1-strength
	// G = 1.0 / shadowDistance
	// B = 1.0 / (shadowDistance - shadowStartFade)
	// A = -shadowStartFade / (shadowDistance - shadowStartFade)
	outParams.x = 1.0f - shadowStrength; // R = 1-strength

	if (shadowRange > 0)
	{
		outParams.y = camera.GetFar() / shadowDistance;
		// fade factor = (x-start)/len = x * invlen - start*invlen
		const float shadowStartFade = shadowRange - shadowDistance * kShadowFadeRange;
		const float shadowFadeInvLen = 1.0f / (shadowRange - shadowStartFade);
		outParams.z = shadowFadeInvLen;
		outParams.w = -shadowStartFade * shadowFadeInvLen;
	}
	else
	{
		outParams.y = std::numeric_limits<float>::infinity();
		outParams.z = 0;
		outParams.w = 1;
	}

	outCenterAndType = Vector4f(center.x, center.y, center.z, spherical);
}

struct ShadowCullContext
{
	const ShadowCullData* camera;
	bool excludeLightmapped;
	UInt32 cullLayers;
};


static void AddShadowCaster (const SceneNode& node, const AABB& aabb, const ShadowCullData& context, bool expandCasterBounds, MinMaxAABB& casterBounds, ShadowCasters& casters, ShadowCasterParts& casterParts)
{
	const BaseRenderer* renderer = node.renderer;
	
	Assert( renderer && renderer->GetCastShadows() );

	// Find out which sub-materials do cast shadows
	// and prefetch shader pointers.
	size_t partsStartIndex = casterParts.size();
	int matCount = renderer->GetMaterialCount();
	
	Shader* replaceShader = context.shaderReplace.replacementShader;
	const bool replacementTagSet = context.shaderReplace.replacementTagSet;
	const int replacementTagID = context.shaderReplace.replacementTagID;
	
	for( int i = 0; i < matCount; ++i )
	{
		Material* mat = renderer->GetMaterial(i);
		if( !mat )
			continue;

		Shader* originalShader = mat->GetShader();
		Shader* actualShader = replaceShader ? replaceShader : originalShader;
		
		// Find the subshader...
		int usedSubshaderIndex = -1;
		if (replaceShader)
		{
			if (replacementTagSet)
			{
				int subshaderTypeID = originalShader->GetShaderLabShader()->GetTag (replacementTagID, true);
				if (subshaderTypeID < 0)
					continue; // skip rendering
				usedSubshaderIndex = replaceShader->GetSubShaderWithTagValue (replacementTagID, subshaderTypeID);
				if (usedSubshaderIndex == -1)
					continue; // skip rendering
			}
			else
				usedSubshaderIndex = 0;
		}
		else
			usedSubshaderIndex = originalShader->GetActiveSubShaderIndex();
		
		Assert (usedSubshaderIndex != -1);
		
		if (actualShader->GetShadowCasterPassToUse(usedSubshaderIndex))
		{
			ShadowCasterPartData part;
			part.subMeshIndex = renderer->GetSubsetIndex(i);
			part.shader = actualShader;
			part.subShaderIndex = usedSubshaderIndex;
			part.material = mat;
			casterParts.push_back (part);
		}
	}

	// This object does cast shadows, store info
	size_t partsEndIndex = casterParts.size();
	if( partsEndIndex != partsStartIndex )
	{
		ShadowCasterData data;
		data.node = &node;
		data.worldAABB = &aabb;
		data.visibleCascades = 1;
		data.partsStartIndex = partsStartIndex;
		data.partsEndIndex = partsEndIndex;

		casters.push_back( data );

		if( expandCasterBounds )
			casterBounds.Encapsulate( aabb );
	}
}

static bool CullCastersCommon(const ShadowCullContext& context, const SceneNode& node, const AABB& aabb)
{
	const BaseRenderer* renderer = node.renderer;
	if (!renderer->GetCastShadows())
		return false;

	if (context.excludeLightmapped && renderer->IsLightmappedForShadows())
		return false;

	const int nodeLayer = node.layer;
	const int nodeLayerMask = 1 << nodeLayer;
	if (!(nodeLayerMask & context.cullLayers))
		return false;

	// For casters that use per-layer culling distance: check if they are behind cull distance.
	// If they are, don't cast shadows.
	const ShadowCullData& cullData = *context.camera;
	float layerCullDist = cullData.layerCullDistances[nodeLayer];
	if (layerCullDist)
	{
		if (cullData.layerCullSpherical)
		{

			float sqDist = SqrMagnitude(aabb.GetCenter() - cullData.eyePos);
			if (sqDist > Sqr(layerCullDist))
				return false;
		}
		else
		{
			Plane farPlane = cullData.shadowCullPlanes[kPlaneFrustumFar];
			farPlane.distance = layerCullDist + cullData.baseFarDistance;
			if( !IntersectAABBPlaneBounds( aabb, &farPlane, 1 ) )
				return false;			
		}
	}

	return true;
}

//
// Point light shadow caster culling

struct PointCullContext : public ShadowCullContext
{
	Sphere lightSphere;
};

static bool CullCastersPoint( PointCullContext* context, const SceneNode& node, const AABB& aabb )
{
	if (!CullCastersCommon(*context, node, aabb))
		return false;
	
	if (!IntersectAABBSphere( aabb, context->lightSphere ))
		return false;

	return true;
}

//
// Spot light shadow caster culling

struct SpotCullContext : public ShadowCullContext
{
	Matrix4x4f worldToLightMatrix, projectionMatrix;
	Plane spotLightFrustum[6];
};

static bool CullCastersSpot( SpotCullContext* context, const SceneNode& node, const AABB& aabb )
{
	if (!CullCastersCommon(*context, node, aabb))
		return false;

	// World space light frustum vs. global AABB
	if (!IntersectAABBFrustumFull (aabb, context->spotLightFrustum))
		return false;

	const TransformInfo& xformInfo = node.renderer->GetTransformInfo ();

	Plane planes[6];
	// NOTE: it's important to multiply world->light and node->world matrices
	// before multiplying in the projection matrix. Otherwise when object/light
	// coordinates will approach 10 thousands range, we'll get culling errors
	// because of imprecision.
	//Matrix4x4f proj = context->projectionMatrix * (context->worldToLightMatrix * node->worldMatrix);
	Matrix4x4f temp, proj;
	MultiplyMatrices4x4 (&context->worldToLightMatrix, &xformInfo.worldMatrix, &temp);
	MultiplyMatrices4x4 (&context->projectionMatrix, &temp, &proj);
	ExtractProjectionPlanes( proj, planes );

	if (!IntersectAABBFrustumFull( xformInfo.localAABB, planes ) )
		return false;

	return true;
}


//
// Directional light shadow caster culling

struct DirectionalCullContext : public ShadowCullContext
{
};

static bool CullCastersDirectional( DirectionalCullContext* context, const SceneNode& node, const AABB& aabb )
{
	return CullCastersCommon(*context, node, aabb);
}


static void CullDirectionalShadows (IndexList& visible, const SceneNode* nodes, const AABB* boundingBoxes, DirectionalCullContext &context )
{
	// Generate Visible nodes from all static & dynamic objects
	int visibleCount = 0;
	for (int i=0;i<visible.size;i++)
	{
		const SceneNode& node = nodes[visible.indices[i]];
		const AABB& bounds = boundingBoxes[visible.indices[i]];

		if (CullCastersDirectional( &context, node, bounds))
			visible.indices[visibleCount++] = visible.indices[i];
	}
	visible.size = visibleCount;
}

static void CullSpotShadows (IndexList& visible, const SceneNode* nodes, const AABB* boundingBoxes, SpotCullContext &context )
{
	// Generate Visible nodes from all static & dynamic objects
	int visibleCount = 0;
	for (int i=0;i<visible.size;i++)
	{
		const SceneNode& node = nodes[visible.indices[i]];
		const AABB& bounds = boundingBoxes[visible.indices[i]];

		if (CullCastersSpot( &context, node, bounds))
			visible.indices[visibleCount++] = visible.indices[i];
	}
	visible.size = visibleCount;
}

static void CullPointShadows (IndexList& visible, const SceneNode* nodes, const AABB* boundingBoxes, PointCullContext &context )
{
	// Generate Visible nodes from all static & dynamic objects
	int visibleCount = 0;
	for (int i=0;i<visible.size;i++)
	{
		const SceneNode& node = nodes[visible.indices[i]];
		const AABB& bounds = boundingBoxes[visible.indices[i]];

		if (CullCastersPoint( &context, node, bounds ))
			visible.indices[visibleCount++] = visible.indices[i];
	}
	visible.size = visibleCount;
}

static void CullShadowCastersDetail( const Light& light, const ShadowCullData& cullData, bool excludeLightmapped, CullingOutput& visibleRenderers )
{
	const RendererCullData* rendererCullData = cullData.sceneCullParameters->renderers;
	
	int cullLayers = light.GetCullingMask() & cullData.camera->GetCullingMask();
	LightType lightType = light.GetType();
	switch (lightType)
	{
		case kLightPoint:
		{
			PointCullContext context;
			const Transform& lt = light.GetComponent (Transform);
			Vector3f lightPos = lt.GetPosition();
			context.lightSphere = Sphere( lightPos, light.GetRange() );
			context.camera = &cullData;
			context.cullLayers = cullLayers;
			context.excludeLightmapped = excludeLightmapped;
			
			for (int i=0;i<kVisibleListCount;i++)
				CullPointShadows (visibleRenderers.visible[i], rendererCullData[i].nodes, rendererCullData[i].bounds, context);
		}
			break;
			
		case kLightSpot:
		{
			SpotCullContext context;
			Matrix4x4f zscale, proj;
			zscale.SetScale (Vector3f (1.0F, 1.0F, -1.0F));
			proj.SetPerspectiveCotan( light.GetCotanHalfSpotAngle(), 0.0001F, light.GetRange() );
			MultiplyMatrices4x4 (&proj, &zscale, &context.projectionMatrix);
			const Transform& lt = light.GetComponent (Transform);
			context.worldToLightMatrix = lt.GetWorldToLocalMatrixNoScale();
			
			Matrix4x4f temp;
			MultiplyMatrices4x4 (&context.projectionMatrix, &context.worldToLightMatrix, &temp);
			ExtractProjectionPlanes (temp, context.spotLightFrustum);
			
			context.camera = &cullData;
			context.cullLayers = cullLayers;
			context.excludeLightmapped = excludeLightmapped;
			
			for (int i=0;i<kVisibleListCount;i++)
				CullSpotShadows (visibleRenderers.visible[i], rendererCullData[i].nodes, rendererCullData[i].bounds, context);
		}
			break;
		case kLightDirectional:
		{
			DirectionalCullContext context;
			context.camera = &cullData;
			context.cullLayers = cullLayers;
			context.excludeLightmapped = excludeLightmapped;
			
			for (int i=0;i<kVisibleListCount;i++)
				CullDirectionalShadows (visibleRenderers.visible[i], rendererCullData[i].nodes, rendererCullData[i].bounds, context);
		}
			break;
	}
}

void CullShadowCasters (const Light& light, const ShadowCullData& cullData, bool excludeLightmapped, CullingOutput& cullingOutput )
{
	LightType lightType = light.GetType();
	
	if (lightType == kLightPoint)
	{
		PROFILER_BEGIN(gShadowsCullCastersPoint,&light)
	}
	else if (lightType == kLightSpot)
	{
		PROFILER_BEGIN(gShadowsCullCastersSpot,&light)
	}
	else
	{
		PROFILER_BEGIN(gShadowsCullCastersDir,&light)
	}
	
	const Transform& lt = light.GetComponent (Transform);
	ShadowCasterCull casterCull;
	
	CalculateShadowCasterCull( cullData.shadowCullPlanes, cullData.cameraClipToWorld, cullData.eyePos,
							  1.0, cullData.farPlaneScale, lightType, lt, casterCull );
	
	SceneCullingParameters cullParams = *cullData.sceneCullParameters;
	cullData.camera->CalculateCustomCullingParameters(cullParams, casterCull.planes, casterCull.planeCount);
	
	if (lightType == kLightDirectional)
	{
		Vector3f lightDir = lt.TransformDirection (Vector3f(0,0,-1));
		cullParams.lightDir = lightDir;
	}
	
	if (cullParams.useShadowCasterCulling && (lightType == kLightDirectional))
	{
		CullShadowCastersWithUmbra(cullParams, cullingOutput);
	}
	else
	{
		CullSceneWithoutUmbra (cullParams, cullingOutput);
	}
	
	
	CullShadowCastersDetail (light, cullData, excludeLightmapped, cullingOutput);
	
	PROFILER_END
}


void GenerateShadowCasterParts (const Light& light, const ShadowCullData& cullData, const CullingOutput& visibleRenderers, MinMaxAABB& casterBounds, ShadowCasters& casters, ShadowCasterParts& casterParts)
{
	const bool isDirectionalLight = light.GetType() == kLightDirectional;
	
	const RendererCullData* rendererCullData = cullData.sceneCullParameters->renderers;
	for (int t=0;t<kVisibleListCount;t++)
	{
		const IndexList& visibleList = visibleRenderers.visible[t];
		const RendererCullData& renderLists = rendererCullData[t];
		for (int i=0;i<visibleList.size;i++)
		{
			int renderIndex = visibleList[i];
			bool casterTouchesView = false;
			if (isDirectionalLight)
			{
				// Only compute caster AABB for casters that touch the view frustum.
				// Other casters might be valid casters, but behind near plane of resulting
				// shadow camera. This is ok, we manually push those onto near plane in caster vertex shader.
				//
				// If we'd bound all casters we get the issue of insufficient depth precision when some caster
				// has a valid caster, but is waaay far away (50000 units or so).
				casterTouchesView = IntersectAABBFrustumFull( renderLists.bounds[renderIndex], cullData.cameraCullPlanes );
			}
			
			// Last argument expandCasterBounds should be false for point lights as they don't use casterBounds to adjust shadow map view
			AddShadowCaster( renderLists.nodes[renderIndex], renderLists.bounds[renderIndex], cullData, casterTouchesView, casterBounds, casters, casterParts );
		}
	}
}



/*
 
 static bool CullCastersDirectional( DirectionalCullContext* context, const SceneNode& node, const AABB& aabb, float lodFade )
 {
 if (!CullCastersCommon(*context, node, aabb))
 return false;
 
 // Only compute caster AABB for casters that touch the view frustum.
 // Other casters might be valid casters, but behind near plane of resulting
 // shadow camera. This is ok, we manually push those onto near plane in caster vertex shader.
 //
 // If we'd bound all casters we get the issue of insufficient depth precision when some caster
 // has a valid caster, but is waaay far away (50000 units or so).
 bool casterTouchesView = IntersectAABBFrustumFull( aabb, context->camera->cameraCullPlanes );
 AddShadowCaster( node, aabb, *context, casterTouchesView );
 return true;
 }
 
 
 */



struct Circle2f
{
	Vector2f center;
	float radius;
};

void CullDirectionalCascades(ShadowCasters& casters, const ShadowCascadeInfo cascades[kMaxShadowCascades], int cascadeCount,
									const Quaternionf& lightRot, const Vector3f& lightDir, const ShadowCullData& cullData)
{
	Assert(cascadeCount <= kMaxShadowCascades);
	const QualitySettings::QualitySetting& quality = GetQualitySettings().GetCurrent();
	if (quality.shadowProjection == kShadowProjCloseFit && cascadeCount == 1)
	{
		// We already culled casters for one non-spherical cascade
		return;
	}
	const Camera& camera = *cullData.camera;

	// Split frustums extruded towards light
	ShadowCasterCull cullPlanes[kMaxShadowCascades];

	// Only objects inside a cylinder can cast shadows onto a sphere
	// We cull in light space so the cylinders become circles
	Circle2f cullCylinders[kMaxShadowCascades];

	Matrix4x4f lightMat;
	QuaternionToMatrix(lightRot, lightMat);

	for (int casc = 0; casc < cascadeCount; casc++)
	{
		const ShadowCascadeInfo& cascade = cascades[casc];
		if (!cascade.enabled)
			continue;
		if (quality.shadowProjection == kShadowProjStableFit)
		{
			// We use spherical splits so cull against a cylinder
			const Vector3f& centerWorld = cascade.outerSphere.GetCenter();
			cullCylinders[casc].center.x = Dot(centerWorld, lightMat.GetAxisX());
			cullCylinders[casc].center.y = Dot(centerWorld, lightMat.GetAxisY());
			cullCylinders[casc].radius = cascade.outerSphere.GetRadius();
		}
		if (cascadeCount == 1)
		{
			// We have already culled against non-cascaded frustum
			cullPlanes[casc].planeCount = 0;
			continue;
		}

		bool alreadyTested[kPlaneFrustumNum];
		for (int p = 0; p < kPlaneFrustumNear; p++)
			alreadyTested[p] = true;
		alreadyTested[kPlaneFrustumNear] = (casc == 0);
		alreadyTested[kPlaneFrustumFar] = ((casc + 1) == cascadeCount);

		Plane splitPlanes[kPlaneFrustumNum];
		memcpy(splitPlanes, cullData.shadowCullPlanes, sizeof(splitPlanes));
		splitPlanes[kPlaneFrustumNear].distance += cascade.minViewDistance - camera.GetNear();
		splitPlanes[kPlaneFrustumFar].distance += cascade.maxViewDistance - cullData.shadowDistance;
		float nearPlaneScale = cascade.minViewDistance / cullData.projectionNear;
		float farPlaneScale = cascade.maxViewDistance / cullData.projectionFar;
		CalculateShadowCasterCull(splitPlanes, cullData.cameraClipToWorld, cullData.eyePos,
			nearPlaneScale, farPlaneScale, kLightDirectional, lightDir, cullPlanes[casc], alreadyTested);
	}

	UInt32 allVisible = 0;
	for (int casc = 0; casc < cascadeCount; casc++)
		allVisible = (allVisible << 1) | 1;

	// Go over casters and determine in which cascades they are visible
	int casterCount = casters.size();
	for (int i = 0; i < casterCount; i++)
	{
		ShadowCasterData& caster = casters[i];
		const AABB& bounds = *caster.worldAABB;
		caster.visibleCascades = allVisible;
		UInt32 currentCascadeMask = 1;

		if (quality.shadowProjection == kShadowProjStableFit)
		{
			float casterRadius = Magnitude(bounds.GetExtent());
			Vector2f lightSpaceCenter;
			lightSpaceCenter.x = Dot(bounds.GetCenter(), lightMat.GetAxisX());
			lightSpaceCenter.y = Dot(bounds.GetCenter(), lightMat.GetAxisY());
			currentCascadeMask = 1;
			for (int casc = 0; casc < cascadeCount; casc++, currentCascadeMask <<= 1)
			{
				if (!cascades[casc].enabled)
					continue;
				// Do the caster bounds and cascade intersect in light space?
				Vector2f vec = lightSpaceCenter - cullCylinders[casc].center;
				float sqrDist = Sqr(vec.x) + Sqr(vec.y);
				float cullRadius = cullCylinders[casc].radius;
				if (sqrDist > Sqr(casterRadius + cullRadius))
				{
					// Circles do not intersect, mark as invisible
					caster.visibleCascades &= ~currentCascadeMask;
				}
			}
		}

		if (cascadeCount > 1)
		{
			currentCascadeMask = 1;
			for (int casc = 0; casc < cascadeCount; casc++, currentCascadeMask <<= 1)
			{
				if (!cascades[casc].enabled)
					continue;
				// Did we already cull this?
				if (caster.visibleCascades & currentCascadeMask)
				{
					if (!IntersectAABBPlaneBounds(bounds, cullPlanes[casc].planes, cullPlanes[casc].planeCount))
					{
						// Outside cull planes, mark this as invisible
						caster.visibleCascades &= ~currentCascadeMask;
					}
				}
			}
		}
	}
}

bool IsObjectWithinShadowRange (const ShadowCullData& shadowCullData, const AABB& bounds)
{
	if (shadowCullData.useSphereCulling)
	{
		float sqrDist = SqrMagnitude(bounds.GetCenter() - shadowCullData.shadowCullCenter);
		if (sqrDist < shadowCullData.shadowCullSquareRadius)
			return true;
		Sphere sphere(shadowCullData.shadowCullCenter, shadowCullData.shadowCullRadius);
		return IntersectAABBSphere(bounds, sphere);
	}
	else
	{
		return IntersectAABBPlaneBounds(bounds, &shadowCullData.shadowCullPlanes[kPlaneFrustumFar], 1);
	}
}
