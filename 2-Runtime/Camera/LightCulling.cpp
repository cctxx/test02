#include "UnityPrefix.h"
#include "LightManager.h"
#include "CullResults.h"
#include "Light.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Geometry/Sphere.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Geometry/BoundingUtils.h"
#include "Runtime/Math/Simd/math.h"
#include "ShadowCulling.h"
#include "Runtime/Profiler/Profiler.h"

#include "External/Umbra/builds/interface/runtime/umbraTome.hpp"
#include "External/Umbra/builds/interface/runtime/umbraQuery.hpp"

PROFILER_INFORMATION(gCullLightConnectivity, "CullLightConnectivity", kProfilerRender);
PROFILER_INFORMATION(gOcclusionCullLight, "OcclusionCullLight", kProfilerRender);

struct LocalLightCullingParameters
{
	Plane    eyePlane;
	float    farDistance;
	bool     enableShadows;
	UInt32   cullingMask;
};



static float CalculateIntensityForMainLight (const Light& source)
{
	DebugAssert(source.GetType() == kLightDirectional);
	if (source.GetRenderMode() == Light::kRenderNotImportant || source.GetLightmappingForRender() == Light::kLightmappingBakedOnly)
		return 0.0f;
	
	float lum = source.GetColor().GreyScaleValue() * source.GetIntensity();
	if (source.GetShadows() != kShadowNone)
		lum *= 16.0f;
	return lum;
}

static void OcclusionCullLocalLights (const SceneCullingParameters& cullingParams, const Vector4f* lightBSpheres, IndexList& visible)
{
	// Umbra light culling is not supported. Just output
	const Umbra::Visibility* umbraVisibility = cullingParams.sceneVisbilityForShadowCulling->umbraVisibility;
    if (!cullingParams.useLightOcclusionCulling)
		return;

	PROFILER_BEGIN(gOcclusionCullLight, NULL)
	
	Umbra::OcclusionBuffer* occlusionBuffer = umbraVisibility->getOutputBuffer();
	
	// Mark lights visible and add them to lightVisibleBits
	size_t visibleCount = 0;
	for (int l = 0; l < visible.size; l++)
	{
		int lightNdx = visible[l];
		
		const float* lightFloatParams = reinterpret_cast<const float*> (lightBSpheres + lightNdx);
		
		float r = lightFloatParams[3];
		Vector3f r3(r, r, r);
        Vector3f lc = Vector3f(lightFloatParams[0], lightFloatParams[1], lightFloatParams[2]);

		Vector3f mn = lc - r3;
		Vector3f mx = lc + r3;
		
		bool isLightVisible = occlusionBuffer->isAABBVisible((const Umbra::Vector3&)mn, (const Umbra::Vector3&)mx);
		if (isLightVisible)
			visible.indices[visibleCount++] = lightNdx;
	}
	visible.size = visibleCount;

	PROFILER_END
	
	PROFILER_BEGIN(gCullLightConnectivity, NULL)

	
	///@TODO: This doesn't make sense for non-shadowed lights...
	
	// Use connectivity data to detect which lights are can touch any visible geometry
	Umbra::IndexList visibleLightList(visible.indices, visible.size, visible.size);
	Umbra::QueryExt* umbraQuery = (Umbra::QueryExt*)cullingParams.umbraQuery;

	umbraQuery->queryLocalLights(
								 visibleLightList,
								 0,
								 (const Umbra::SphereLight*)lightBSpheres,
								 visible.reservedSize,
								 *umbraVisibility->getOutputClusters(),
								 &visibleLightList);
	
	visible.size = visibleLightList.getSize();
	
	PROFILER_END
}

static void FrustumCullLocalLights (const CullingParameters& cullingParameters, const Vector4f* lightBSpheres, IndexList& visibleLights, IndexList& offScreenLights, float* visibilityFades)
{
	int visibleLightCount = 0;
	int offScreenLightCount = 0;
		
	for (int i=0;i<visibleLights.reservedSize;i++)
	{
		float distance = PointDistanceToFrustum(lightBSpheres[i], cullingParameters.cullingPlanes, cullingParameters.cullingPlaneCount);

		// Light is inside or intersecting the frustum
		if (distance < lightBSpheres[i].w)
		{
			// lights that intersect or are inside the frustum
			DebugAssert(visibleLightCount < visibleLights.reservedSize);
			visibleLights.indices[visibleLightCount++] = i;
		}
		// Light is outside of the frustum and must be faded out
		else if (distance < lightBSpheres[i].w + lightBSpheres[i].w)
		{
			//off screen lights whose distance from frustum is less than light radius 
			DebugAssert(offScreenLightCount < offScreenLights.reservedSize);

			offScreenLights.indices[offScreenLightCount] = i;
			
			distance -= lightBSpheres[i].w;
			
			distance = distance / lightBSpheres[i].w;
			visibilityFades[offScreenLightCount++] = 1.0F - distance;
			DebugAssert(distance > 0.0f);
			DebugAssert(distance < 1.0f);
		}
	}
	visibleLights.size = visibleLightCount;
	offScreenLights.size = offScreenLightCount;
}	

static bool IsValidRenderingLight (const Light& light, LightType lightType, UInt32 cullingMask)
{
	const Light::Lightmapping lightmappingMode = light.GetLightmappingForRender();
	// If light is lightmap only - just skip it
	if (lightmappingMode == Light::kLightmappingBakedOnly)
		return false;
	
	// If light not visible in camera's culling mask - just skip it
	if ((light.GetCullingMask() & cullingMask) == 0)
		return false;
	
	// Light with zero intensity - just skip it
	if (light.GetIntensity() < 0.01f)
		return false;
	
	// Check if light has valid properties
	return light.IsValidToRender();
}

static void SetupActiveDirectionalLight (const Light& light, ActiveLight& outLight)
{
	const Light::Lightmapping lightmappingMode = light.GetLightmappingForRender();
	
	outLight.light = const_cast<Light*> (&light);
#if ENABLE_SHADOWS
	outLight.insideShadowRange = true;
#endif
	outLight.boundingBox = AABB(Vector3f::zero, Vector3f::infinityVec);
	
	outLight.lightmappingForRender = lightmappingMode;
	outLight.isVisibleInPrepass = true;
	outLight.screenRect = Rectf(0,0,1,1);
	outLight.cullingMask = light.GetCullingMask();
	outLight.hasCookie = light.GetCookie() ? true : false;
	outLight.lightRenderMode = light.GetRenderMode();
	outLight.lightType = light.GetType();
	outLight.isOffscreenVertexLight = false;
	outLight.visibilityFade = 1.0;
}

static int FindBestMainDirectionalLight (const Light** lights, size_t count)
{
	// Find main directional light based on intensity
	int mainLightIndex = -1;
	float bestMainLightIntensity = 0.0f;
	
	for (int i=0;i<count;i++)
	{
		const Light& light = *lights[i];
		float mainLightIntensity = CalculateIntensityForMainLight(light);
		if (mainLightIntensity > bestMainLightIntensity)
		{
			mainLightIndex = i;
			bestMainLightIntensity = mainLightIntensity;
		}
	}
	
	return mainLightIndex;
}

static void AddDirectionalLights (const Light** lights, size_t count, ActiveLights& outLights)
{
	Assert(outLights.lights.size() == 0);
	
	// Add main light as the first light!
	int mainLightIndex = FindBestMainDirectionalLight(lights, count);
	if (mainLightIndex != -1)
	{
		SetupActiveDirectionalLight (*lights[mainLightIndex], outLights.lights.push_back());
		outLights.hasMainLight = true;
	}
	else
		outLights.hasMainLight = false;
	
	// Add any other lights
	for (int i=0;i<count;i++)
	{
		if (i == mainLightIndex)
			continue;
		
		SetupActiveDirectionalLight (*lights[i], outLights.lights.push_back());
	}
	outLights.numDirLights = outLights.lights.size();
}

static void SetupActiveLocalLight (const LocalLightCullingParameters& params, const ShadowCullData& shadowCullData, const Light& light, const Vector4f& lightBSpheres, const Rectf lightScreenRectangle, bool isVisible, float visibilityFade, ActiveLight& outLight)
{
	const Transform& trans = light.GetComponent(Transform);
	Matrix4x4f lightMatrix = trans.GetLocalToWorldMatrixNoScale();
	float radius = lightBSpheres.w;
	Vector3f center = Vector3f(lightBSpheres.x, lightBSpheres.y, lightBSpheres.z);
	float nearDistanceFudged = shadowCullData.camera->GetNear() * 1.001f;
	float farDistanceFudged = shadowCullData.camera->GetFar() * 0.999f;

	// Add to spot or point lights
	outLight.light = const_cast<Light*> (&light);
	float viewDistance = params.eyePlane.GetDistanceToPoint(center);
	float closestDistance = std::numeric_limits<float>::infinity();
	float farthestDistance = -closestDistance;
	
	outLight.isVisibleInPrepass = isVisible;
	outLight.screenRect = lightScreenRectangle;
	outLight.visibilityFade = visibilityFade;
	
	// If light survived from culling, but is not visible
	outLight.isOffscreenVertexLight = !isVisible;
	
	// Baked-only lights are already rejected, so lightmaps are either off or auto
	const Light::Lightmapping lightmappingMode = light.GetLightmappingForRender();
	outLight.lightmappingForRender = lightmappingMode;
	
	// Keep cached copy of culling mask for efficiency
	outLight.cullingMask = light.GetCullingMask();
	outLight.hasCookie = light.GetCookie() ? true : false;
	outLight.lightRenderMode = light.GetRenderMode();
	
	LightType lightType = light.GetType();
	outLight.lightType = lightType;

	if (lightType == kLightSpot)
	{
		// Find nearest point
		SpotLightBounds spotBounds;
		CalculateSpotLightBounds (light.GetRange(), light.GetCotanHalfSpotAngle(), lightMatrix, spotBounds);
		const Vector3f* points = spotBounds.points;

		for (int i = 0; i < SpotLightBounds::kPointCount; i++)
		{
			float dist = params.eyePlane.GetDistanceToPoint(points[i]);
			closestDistance = std::min (closestDistance, dist);
			farthestDistance = std::max (farthestDistance, dist);
		}
		outLight.intersectsNear = closestDistance <= nearDistanceFudged;
		outLight.intersectsFar = farthestDistance >= farDistanceFudged;

		// Nearest point is also bounded by light radius (cull by far plane distance)
		float dist = viewDistance - radius;
		closestDistance = std::max(closestDistance, dist);
		if (closestDistance > params.farDistance)
		{
			outLight.isVisibleInPrepass = false;
			outLight.screenRect = Rectf(0,0,0,0);
		}

		// Compute bounding box
		MinMaxAABB bounds(points[0], points[0]);
		for (int i = 1; i < SpotLightBounds::kPointCount; i++)
			bounds.Encapsulate(points[i]);
		outLight.boundingBox = AABB(bounds);
	}
	else
	{
		DebugAssert(lightType == kLightPoint);
		closestDistance = viewDistance - radius;
		Vector3f boxSize(radius, radius, radius);
		outLight.boundingBox = AABB(center, boxSize);

		#if GFX_USE_SPHERE_FOR_POINT_LIGHT
			// If we're drawing an icosphere or icosahedron, check for the radius of a sphere
			// circumscribed on an icosahedron, which was circumscribed on a unit sphere.
			const float proxyMeshSize = 1.27f;
		#else
			// If we're drawing a bounding cube, check if the farthest corner would not cross the near plane
			const float proxyMeshSize = 1.7321f;
		#endif
		const float intersectionRadius = radius * proxyMeshSize;
		outLight.intersectsNear = (viewDistance - intersectionRadius) <= nearDistanceFudged;
		outLight.intersectsFar = (viewDistance + intersectionRadius) >= farDistanceFudged;
	}
	
#if ENABLE_SHADOWS
	// TODO: tighter shadow culling for spot lights
	outLight.insideShadowRange = (closestDistance < shadowCullData.shadowDistance) && params.enableShadows;
	if (outLight.insideShadowRange && shadowCullData.useSphereCulling)
	{
		float sumRadii = shadowCullData.shadowCullRadius + radius;
		if (SqrMagnitude(center - shadowCullData.shadowCullCenter) > Sqr(sumRadii))
			outLight.insideShadowRange = false;
		else if (!IsObjectWithinShadowRange(shadowCullData, outLight.boundingBox))
			outLight.insideShadowRange = false;
	}
	
	// If light is auto but behind shadow distance (so dual lightmaps normally) - just skip it
	if ((lightmappingMode == Light::kLightmappingAuto) && !outLight.insideShadowRange)
	{
		outLight.isVisibleInPrepass = false;
		outLight.screenRect = Rectf(0,0,0,0);
	}
#endif
}

// Function returns true if screen rectangle (outRect) is inside camera viewport
// Returned rectangle is 0..1 coordinates 
bool CalculateLightScreenBounds (const Matrix4x4f& cameraWorldToClip, const Light& light, const Matrix4x4f& lightMatrix, Rectf& outRect)
{
	Assert ( light.GetType() != kLightDirectional );

	// Compute the hull of light's bounds
	Vector3f lightPos = lightMatrix.GetPosition();

	UInt8 hullFaces;
	UInt8 hullCounts[6]; // 6 faces
	Vector3f hullPoints[24];	// this input hull has maximum of 6 faces x 4 points hence 24 vectors

	switch( light.GetType() )
	{
	case kLightSpot:
		// Spot light's hull is the light position and four points on the plane at Range
		{
			SpotLightBounds spotBounds;
			CalculateSpotLightBounds(light.GetRange(), light.GetCotanHalfSpotAngle(), lightMatrix, spotBounds);
			const Vector3f* points = spotBounds.points;

			hullFaces = 5;
			hullCounts[0] = 4;
			hullCounts[1] = hullCounts[2] = hullCounts[3] = hullCounts[4] = 3;

			// far plane
			hullPoints[0] = points[4]; hullPoints[1] = points[3]; hullPoints[2] = points[2]; hullPoints[3] = points[1];

			// sides
			hullPoints[ 4] = points[0]; hullPoints[ 5] = points[1]; hullPoints[ 6] = points[2];
			hullPoints[ 7] = points[0]; hullPoints[ 8] = points[2]; hullPoints[ 9] = points[3];
			hullPoints[10] = points[0]; hullPoints[11] = points[3]; hullPoints[12] = points[4];
			hullPoints[13] = points[0]; hullPoints[14] = points[4]; hullPoints[15] = points[1];
		}
		break;

	case kLightPoint:
		// Point light's hull is the cube at position with half-size Range
		{
			float r = light.GetRange();
			Vector3f points[8];
			points[0].Set( lightPos.x - r, lightPos.y - r, lightPos.z - r );
			points[1].Set( lightPos.x + r, lightPos.y - r, lightPos.z - r );
			points[2].Set( lightPos.x + r, lightPos.y + r, lightPos.z - r );
			points[3].Set( lightPos.x - r, lightPos.y + r, lightPos.z - r );
			points[4].Set( lightPos.x - r, lightPos.y - r, lightPos.z + r );
			points[5].Set( lightPos.x + r, lightPos.y - r, lightPos.z + r );
			points[6].Set( lightPos.x + r, lightPos.y + r, lightPos.z + r );
			points[7].Set( lightPos.x - r, lightPos.y + r, lightPos.z + r );

			hullFaces = 6;
			hullCounts[0] = hullCounts[1] = hullCounts[2] = hullCounts[3] = hullCounts[4] = hullCounts[5] = 4;

			hullPoints[ 0] = points[0]; hullPoints[ 1] = points[1]; hullPoints[ 2] = points[2]; hullPoints[ 3] = points[3];
			hullPoints[ 4] = points[7]; hullPoints[ 5] = points[6]; hullPoints[ 6] = points[5]; hullPoints[ 7] = points[4];
			hullPoints[ 8] = points[0]; hullPoints[ 9] = points[3]; hullPoints[10] = points[7]; hullPoints[11] = points[4];
			hullPoints[12] = points[1]; hullPoints[13] = points[5]; hullPoints[14] = points[6]; hullPoints[15] = points[2];
			hullPoints[16] = points[4]; hullPoints[17] = points[5]; hullPoints[18] = points[1]; hullPoints[19] = points[0];
			hullPoints[20] = points[6]; hullPoints[21] = points[7]; hullPoints[22] = points[3]; hullPoints[23] = points[2];

		}
		break;

	default:
		hullFaces = 0;
		AssertString( "Unknown light type" );
		break;
	}

	// Clip hull by camera's near plane - needed because point behind near plane don't have
	// proper projection on the screen.
	Plane nearPlane;
	ExtractProjectionNearPlane( cameraWorldToClip, &nearPlane );
	// Push near plane forward a bit, by a small number proportional to plane's distance from
	// the origin (precision gets worse at larger numbers).
	nearPlane.d() = nearPlane.d() - Abs(nearPlane.d())*0.0001f;
	DebugAssertIf(!IsNormalized(nearPlane.GetNormal()));

	MinMaxAABB aabb;
	CalcHullBounds(hullPoints, hullCounts, hullFaces, nearPlane, cameraWorldToClip, aabb);
	outRect.Set (
		(aabb.m_Min.x + 1.0f) * 0.5f,
		(aabb.m_Min.y + 1.0f) * 0.5f,
		(aabb.m_Max.x - aabb.m_Min.x) * 0.5f,
		(aabb.m_Max.y - aabb.m_Min.y) * 0.5f
		);
	
	// Is screen rect inside viewport [0,1]
	return ((aabb.m_Max.x > aabb.m_Min.x) || (aabb.m_Max.y > aabb.m_Min.y)) ? true : false;
}

void AddActiveLocalLights (const LocalLightCullingParameters& params, const ShadowCullData& shadowCullData, const Vector4f* lightBSpheres, const Light** lights, const IndexList& visibleLocalLights, float* visibilityFades, IndexList& offScreenLocalLights, ActiveLights& outLights)
{
	int offScreenLightCount = offScreenLocalLights.size;
	
	//Add spot lights first, and point lights second
	int lightTypes[2] = {kLightSpot, kLightPoint};
	int lightCount[2] = {0, 0};
	for (int j=0; j<2; j++)
		for (int i=0;i<visibleLocalLights.size;i++)
		{
			int lightIndex = visibleLocalLights[i];
			const Light &light = *lights[lightIndex];
			if (light.GetType() == lightTypes[j])
			{
				// Calculate local light screen rectangle
				const Transform& trans = lights[lightIndex]->GetComponent(Transform);
				Matrix4x4f lightMatrix = trans.GetLocalToWorldMatrixNoScale();

				Rectf lightScreenRect;
				bool isInside = CalculateLightScreenBounds (shadowCullData.cameraWorldToClip, *lights[lightIndex], lightMatrix, lightScreenRect);
							
				// Setup visible local light if it is inside camera viewport and has a valid screen rectangle
				if (isInside && !lightScreenRect.IsEmpty())
				{
					SetupActiveLocalLight (params, shadowCullData, *lights[lightIndex], lightBSpheres[lightIndex], lightScreenRect, true, 1.0f, outLights.lights.push_back());
					lightCount[j]++;
				}
				else if (!isInside)	// change visible light to off screen light if it is outside camera viewport
				{
					visibilityFades[offScreenLightCount] = 1.0f;	// don't fade as the local light is close to the frustum 
					offScreenLocalLights[offScreenLightCount++] = lightIndex;
				}
			}
  		}
	
	outLights.numSpotLights = lightCount[0];
	outLights.numPointLights = lightCount[1];

	//Add off screen spot lights third, and off screen point lights fourth
	lightCount[0] = lightCount[1] = 0;
	for (int j=0; j<2; j++)
		for (int i=0;i<offScreenLightCount;i++)
		{
			int lightIndex = offScreenLocalLights[i];
			const Light &light = *lights[lightIndex];
			if (light.GetType() == lightTypes[j])
			{
				SetupActiveLocalLight (params, shadowCullData, *lights[lightIndex], lightBSpheres[lightIndex], Rectf(0,0,0,0), false, visibilityFades[i], outLights.lights.push_back());
				lightCount[j]++;
			}
        }
	
	outLights.numOffScreenSpotLights = lightCount[0];
	outLights.numOffScreenPointLights = lightCount[1];
}

void FindAndCullActiveLights (const SceneCullingParameters& sceneCullParameters, const ShadowCullData& cullData, ActiveLights& outLights)
{
	const List<Light>& allLights = GetLightManager().GetAllLights();
	
	LocalLightCullingParameters localLightCullParameters;
	localLightCullParameters.eyePlane.SetNormalAndPosition(cullData.viewDir, cullData.eyePos);
	localLightCullParameters.farDistance = cullData.camera->GetFar();
	localLightCullParameters.enableShadows = cullData.shadowDistance > cullData.camera->GetNear();
	localLightCullParameters.cullingMask = cullData.camera->GetCullingMask();

	size_t lightCount = allLights.size_slow();
	
	dynamic_array<Vector4f>       localLightBSpheres (kMemTempAlloc);
	dynamic_array<const Light*>   localLights (kMemTempAlloc);
	dynamic_array<const Light*>   directionalLights (kMemTempAlloc);
	dynamic_array<float>          offScreenLocalLightvisibilityFades (kMemTempAlloc);
	
	localLightBSpheres.reserve(lightCount);
	localLights.reserve(lightCount);
	directionalLights.reserve(lightCount);
	offScreenLocalLightvisibilityFades.reserve(lightCount);

    LightManager::Lights::const_iterator it, itEnd = allLights.end();
	for( it = allLights.begin(); it != itEnd; ++it)
	{
		const Light& light = *it;
		LightType lightType = light.GetType();
		
		if (!IsValidRenderingLight (light, lightType, localLightCullParameters.cullingMask))
			continue;
		
		// Add directional light
		if (lightType == kLightDirectional)
		{
			directionalLights.push_back(&light);
		}
		// Setup data necessary for culling point / spot lights
		else if (lightType == kLightPoint || lightType == kLightSpot)
		{
			float radius = light.GetRange();
			
			if (lightType == kLightSpot)
				radius *= light.GetInvCosHalfSpotAngle();

			// Set radius to negative by default, use it to skip lights in the next loop
			Vector3f lightPos = light.GetWorldPosition();
			localLightBSpheres.push_back ( Vector4f(lightPos.x, lightPos.y, lightPos.z, radius) );
			localLights.push_back (&light);
		}
		else
		{
			ErrorStringObject("Unsupported light type", &light);
		}
	}
	
	IndexList visibleLocalLightIndices;
	InitIndexList(visibleLocalLightIndices, localLightBSpheres.size());
	IndexList offScreenLocalLightIndices;
	InitIndexList(offScreenLocalLightIndices, localLightBSpheres.size());
	
	// 1x and 2x light radius frustum culling for local lights
	FrustumCullLocalLights (sceneCullParameters, localLightBSpheres.begin(), visibleLocalLightIndices, offScreenLocalLightIndices, offScreenLocalLightvisibilityFades.begin());

	// Occlusion cull 1x local lights
	OcclusionCullLocalLights (sceneCullParameters, localLightBSpheres.begin(), visibleLocalLightIndices);

	// Reserve memory for lights...
	outLights.lights.reserve (visibleLocalLightIndices.size + offScreenLocalLightIndices.size + directionalLights.size());

	// Add directional lights to the outLights...
	AddDirectionalLights (directionalLights.begin(), directionalLights.size(), outLights);
	
	// Add visible local lights and off screen local lights to the outlights...
	AddActiveLocalLights (localLightCullParameters, cullData, localLightBSpheres.begin(), localLights.begin(), visibleLocalLightIndices, offScreenLocalLightvisibilityFades.begin(), offScreenLocalLightIndices, outLights);
	
	DestroyIndexList(visibleLocalLightIndices);
	DestroyIndexList(offScreenLocalLightIndices);
}


static bool IsLightCulled (const ActiveLight& light, int layerMask, bool lightmappedObject, bool dualLightmapsMode)
{
	// Skip light if has lightmap for object
	if ( (lightmappedObject && light.lightmappingForRender == Light::kLightmappingAuto) && (dualLightmapsMode == false) )
		return true;
	
	// Cull by layer mask
	if ((layerMask & light.cullingMask) == 0)
		return true;
	
	return false;
}

static bool IsDirectionalLightCulled (const ActiveLight& light, int layerMask, bool lightmappedObject, bool dualLightmapsMode)
{
	DebugAssert(light.light->GetType() == kLightDirectional);
	
	return IsLightCulled (light, layerMask, lightmappedObject, dualLightmapsMode);
}

static bool IsSpotLightCulled (const ActiveLight& light, int layerMask, bool lightmappedObject, bool dualLightmapsMode, const AABB& globalObjectAABB, const AABB& localObjectAABB, const Matrix4x4f& objectTransform)
{
	DebugAssert(light.light->GetType() == kLightSpot);
	
	// Common code
	if (IsLightCulled (light, layerMask, lightmappedObject, dualLightmapsMode))
		return true;
	
	// AABB vs AABB
	if (!IntersectAABBAABB (globalObjectAABB, light.boundingBox))
		return true;
	
	const Light& source = *light.light;
	
	// Detailed culling: frustum vs local AABB
	Plane planes[6];
	Matrix4x4f zscale, objectToLightMatrix, projectionMatrix;
	zscale.SetScale (Vector3f (1.0F, 1.0F, -1.0F));
	
	const float minNearDist = 0.0001F;
	const float minNearFarRatio = 0.00001F;
	float nearDist = std::max(minNearDist, source.GetRange() * minNearFarRatio);
	projectionMatrix.SetPerspectiveCotan( source.GetCotanHalfSpotAngle(), nearDist, source.GetRange() );
	
	// objectToLightMatrix = zscale * GetWorldToLocalMatrix * objectTransform
	Matrix4x4f temp;
	MultiplyMatrices4x4 (&zscale, &source.GetWorldToLocalMatrix(), &temp);
	MultiplyMatrices4x4 (&temp, &objectTransform, &objectToLightMatrix);
	
	// finalProjMatrix = projectionMatrix * objectToLightMatrix
	Matrix4x4f finalProjMatrix;
	MultiplyMatrices4x4 (&projectionMatrix, &objectToLightMatrix, &finalProjMatrix);
	ExtractProjectionPlanes (finalProjMatrix, planes);
	
	if (!IntersectAABBFrustumFull (localObjectAABB, planes))
		return true;
	
	return false;
}

static bool IsPointLightCulled (const ActiveLight& light, int layerMask, bool lightmappedObject, bool dualLightmapsMode, const AABB& globalObjectAABB, const AABB& localObjectAABB, const Matrix4x4f& objectTransform, float invScale)
{
	DebugAssert(light.light->GetType() == kLightPoint);
	
	// Common code
	if (IsLightCulled (light, layerMask, lightmappedObject, dualLightmapsMode))
		return true;
	
	// AABB vs AABB
	if (!IntersectAABBAABB (globalObjectAABB, light.boundingBox))
		return true;
	
	const Light& source = *light.light;
	
	// Detailed culling
	// Test light sphere transformed into the object's local space
	// against local aabb of the object
	Vector3f objectRelativeLightPos = objectTransform.InverseMultiplyPoint3Affine (source.GetWorldPosition()) * invScale;
	Sphere objectRelLightSphere (objectRelativeLightPos * invScale, source.GetRange () * invScale);
	
	if (!IntersectAABBSphere (localObjectAABB, objectRelLightSphere))
		return true;
	
	return false;
}

void CullPerObjectLights (const ActiveLights& activeLights, const AABB& globalObjectAABB, const AABB& localObjectAABB, const Matrix4x4f& objectTransform, float invScale, int layerMask, bool lightmappedObject, bool dualLightmapsMode, ObjectLightIndices& outIndices)
{
	size_t index = 0;
	size_t endIndex = activeLights.numDirLights;
	for ( ; index < endIndex; index++)
		if (!IsDirectionalLightCulled (activeLights.lights[index], layerMask, lightmappedObject, dualLightmapsMode))
			outIndices.push_back (index);

	endIndex += activeLights.numSpotLights;
	for ( ; index < endIndex; index++)
		if (!IsSpotLightCulled (activeLights.lights[index], layerMask, lightmappedObject, dualLightmapsMode, globalObjectAABB, localObjectAABB, objectTransform))
				outIndices.push_back (index);
	
	endIndex += activeLights.numPointLights;
	for ( ; index < endIndex; index++)
		if (!IsPointLightCulled (activeLights.lights[index], layerMask, lightmappedObject, dualLightmapsMode, globalObjectAABB, localObjectAABB, objectTransform, invScale))
			outIndices.push_back (index);

	endIndex += activeLights.numOffScreenSpotLights;
	for ( ; index < endIndex; index++)
		if (!IsSpotLightCulled (activeLights.lights[index], layerMask, lightmappedObject, dualLightmapsMode, globalObjectAABB, localObjectAABB, objectTransform))
			outIndices.push_back (index);

	endIndex += activeLights.numOffScreenPointLights;
	for ( ; index < endIndex; index++)
		if (!IsPointLightCulled (activeLights.lights[index], layerMask, lightmappedObject, dualLightmapsMode, globalObjectAABB, localObjectAABB, objectTransform, invScale))
			outIndices.push_back (index);
}