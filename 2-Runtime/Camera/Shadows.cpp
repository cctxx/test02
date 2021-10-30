#include "UnityPrefix.h"
#include "Shadows.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Shaders/ShaderKeywords.h"

#if ENABLE_SHADOWS

#include "Runtime/Geometry/AABB.h"
#include "Light.h"
#include "RenderManager.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "BaseRenderer.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Graphics/GraphicsHelper.h"
#include "Runtime/Graphics/Transform.h"
#include "Renderqueue.h"
#include "Runtime/Geometry/BoundingUtils.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Camera/Culler.h"
#include "IntermediateRenderer.h"
#include "Runtime/GfxDevice/VramLimits.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/GraphicsDevicesDB.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "Runtime/Interfaces/ITerrainManager.h"
#include "Runtime/GfxDevice/BatchRendering.h"
#include "External/shaderlab/Library/intshader.h"
#include "CullResults.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "UnityScene.h"
#include "Runtime/Profiler/Profiler.h"

#include "Configuration/UnityConfigure.h"
#if ENABLE_MONO && ENABLE_TERRAIN
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#endif
#include "Runtime/Scripting/ScriptingUtility.h"

#if UNITY_PS3 || UNITY_XENON
#	define kShadowmapPointSizeMin	512
#	define kShadowmapPointSizeMax	1024
#	define kShadowmapSpotSizeMin	1024
#	define kShadowmapSpotSizeMax	2048
#	define kShadowmapDirSizeMin		1024
#	define kShadowmapDirSizeMax		4096
#elif UNITY_WINRT
#	define kShadowmapPointSizeMin   512
#	define kShadowmapPointSizeMax   1024
#	define kShadowmapSpotSizeMin    1024
#	define kShadowmapSpotSizeMax    2048
#	define kShadowmapDirSizeMin     1024
#	define kShadowmapDirSizeMax     2048
#else
#	define kShadowmapPointSizeMin	512
#	define kShadowmapPointSizeMax	1024
#	define kShadowmapSpotSizeMin	1024
#	define kShadowmapSpotSizeMax	2048
#	define kShadowmapDirSizeMin		2048
#	define kShadowmapDirSizeMax		4096
#endif

// 4000 is PS3 / Xbox360 and it should be fine on those.
#define kShadowRotatedBlurFillrateThreshold (4000)


#endif // ENABLE_SHADOWS

// --------------------------------------------------------------------------

static ShaderKeyword kShadowsOffKeyword = keywords::Create("SHADOWS_OFF");
static ShaderKeyword kShadowsDepthKeyword = keywords::Create("SHADOWS_DEPTH");
static ShaderKeyword kShadowsScreenKeyword = keywords::Create("SHADOWS_SCREEN");
static ShaderKeyword kShadowsCubeKeyword = keywords::Create("SHADOWS_CUBE");
static ShaderKeyword kShadowsSoftKeyword = keywords::Create("SHADOWS_SOFT");
static ShaderKeyword kShadowsSplitSpheresKeyword = keywords::Create("SHADOWS_SPLIT_SPHERES");
static ShaderKeyword kShadowsNativeKeyword = keywords::Create("SHADOWS_NATIVE");

void SetNoShadowsKeywords()
{
	g_ShaderKeywords.Enable( kShadowsOffKeyword );
	g_ShaderKeywords.Disable( kShadowsDepthKeyword );
	g_ShaderKeywords.Disable( kShadowsScreenKeyword );
	g_ShaderKeywords.Disable( kShadowsCubeKeyword );
	g_ShaderKeywords.Disable( kShadowsSoftKeyword );
	g_ShaderKeywords.Disable( kShadowsSplitSpheresKeyword );
	g_ShaderKeywords.Disable( kShadowsNativeKeyword );
}


bool CheckPlatformSupportsShadows ()
{
	return
		gGraphicsCaps.hasRenderToTexture &&
		(gGraphicsCaps.shaderCaps >= kShaderLevel2) &&
		gGraphicsCaps.supportsRenderTextureFormat[kRTFormatDepth] &&
		#if !UNITY_FLASH && !UNITY_WINRT //@TODO: remove me
		gGraphicsCaps.hasRenderToCubemap &&
		#endif
		(gGraphicsCaps.npotRT != kNPOTNone);
}

// --------------------------------------------------------------------------

#if ENABLE_SHADOWS

bool GetSoftShadowsEnabled ()
{
	// Check build settings
	const BuildSettings& buildSettings = GetBuildSettings();
	if( !buildSettings.hasShadows || !buildSettings.hasSoftShadows )
		return false;

	// Disabled by graphics caps?
	if( gGraphicsCaps.disableSoftShadows )
		return false;

	// Check quality settings
	const QualitySettings::QualitySetting& quality = GetQualitySettings().GetCurrent();
	if( quality.shadows < QualitySettings::kShadowsAll )
		return false;

	const float shadowDistance = QualitySettings::GetShadowDistanceForRendering();
	return shadowDistance > 0.0f;
}

void SetShadowsKeywords( LightType lightType, ShadowType shadowType, bool screen, bool enableSoftShadows )
{
	ShadowProjection proj = (ShadowProjection)GetQualitySettings().GetCurrent().shadowProjection;
	g_ShaderKeywords.Disable( kShadowsOffKeyword );
	g_ShaderKeywords.Disable (kShadowsNativeKeyword);

	if( IsSoftShadow(shadowType) && enableSoftShadows )
		g_ShaderKeywords.Enable( kShadowsSoftKeyword );
	else
		g_ShaderKeywords.Disable( kShadowsSoftKeyword );

	if( lightType == kLightDirectional && shadowType != kShadowNone &&
		proj == kShadowProjStableFit )
		g_ShaderKeywords.Enable( kShadowsSplitSpheresKeyword );
	else
		g_ShaderKeywords.Disable( kShadowsSplitSpheresKeyword );

	if( screen )
	{
		g_ShaderKeywords.Enable( kShadowsScreenKeyword );
		g_ShaderKeywords.Disable( kShadowsDepthKeyword );
		g_ShaderKeywords.Disable( kShadowsCubeKeyword );
		if (gGraphicsCaps.hasNativeShadowMap && !gGraphicsCaps.hasShadowCollectorPass)
			g_ShaderKeywords.Enable (kShadowsNativeKeyword);
	}
	else if( lightType == kLightPoint )
	{
		g_ShaderKeywords.Enable( kShadowsCubeKeyword );
		g_ShaderKeywords.Disable( kShadowsDepthKeyword );
		g_ShaderKeywords.Disable( kShadowsScreenKeyword );
	}
	else
	{
		g_ShaderKeywords.Enable( kShadowsDepthKeyword );
		g_ShaderKeywords.Disable( kShadowsCubeKeyword );
		g_ShaderKeywords.Disable( kShadowsScreenKeyword );
		if (gGraphicsCaps.hasNativeShadowMap &&
			!(lightType == kLightSpot && gGraphicsCaps.buggySpotNativeShadowMap))
		{
			g_ShaderKeywords.Enable (kShadowsNativeKeyword);
		}
	}
}


PROFILER_INFORMATION(gShadowsRender, "Shadows.RenderShadowmap", kProfilerRender)
PROFILER_INFORMATION(gShadowsRenderPoint, "Shadows.RenderShadowmapPoint", kProfilerRender)
PROFILER_INFORMATION(gShadowsRenderSpot, "Shadows.RenderShadowmapSpot", kProfilerRender)
PROFILER_INFORMATION(gShadowsRenderDir, "Shadows.RenderShadowmapDir", kProfilerRender)
PROFILER_INFORMATION(gCullShadowCasters, "CullShadowCasters", kProfilerRender);

// --------------------------------------------------------------------------


// from Parallel Split Shadow Maps paper: practical splitting scheme
void CalculatePSSMDistances (float nearPlane, float shadowFarPlane, int splitCount, float* outDistances, float* outPercentages)
{
	AssertIf( !outDistances || !outPercentages || splitCount < 1 );

	// very first & last ones are always these (deals with rounding issues as well)
	outDistances[0] = nearPlane;
	outDistances[splitCount] = shadowFarPlane;
	outPercentages[0] = 0.0f;
	outPercentages[splitCount] = 1.0f;

	// Each next split is 2x larger than the previous one.
	// Different from classic PSSM paper; split ratios don't depent on near plane at all.
	// Dependance on near plane is not very intuitive anyway!
	if( splitCount == 2 )
	{
		// 2 splits: 0, 1/3, 1
		outPercentages[1] = 1.0f / 3.0f;
	}
	else if( splitCount == 4 )
	{
		// 4 splits: 0, 1/15, 3/15, 7/15, 1
		outPercentages[1] = 1.0f / 15.0f;
		outPercentages[2] = 3.0f / 15.0f;
		outPercentages[3] = 7.0f / 15.0f;
	}
	for( int i = 1; i < splitCount; ++i )
		outDistances[i] = nearPlane + (shadowFarPlane - nearPlane) * outPercentages[i];

	//if( splitCount == 4 )
	//	printf_console("PSSM: splits=%i near=%g far=%g p=%g %g %g %g\n", splitCount, nearPlane, shadowFarPlane, outDistances[0], outDistances[1], outDistances[2], outDistances[3] );
	//if( splitCount == 2 )
	//	printf_console("PSSM: splits=%i near=%g far=%g p=%g %g\n", splitCount, nearPlane, shadowFarPlane, outDistances[0], outDistances[1] );
}


// --------------------------------------------------------------------------

static SHADERPROP(ShadowProjectionParams); // x = unused, y = near plane, z = far plane, w = unused


// Shadow caster sort data structure
struct CompactCasterSortData
{
	UInt64 key;
	int casterIndex;
	size_t partsIndex; 

	CompactCasterSortData(UInt32 _smallMeshIndex, UInt32 _hashOfShadowCasterPass, TransformType _transformType, float _depth, int _casterIndex, size_t _partsIndex )
	{
		// 64b key: 32 bit shadow caster pass hash, 16b mesh ID, 2b transform type, and 14b depth
		key=0;
		UInt32 transformType = static_cast<UInt32>(_transformType);
		UInt32 z = (UInt32)(16383.0f*_depth);
		
		key |= (_hashOfShadowCasterPass);
		key = key << 32;
		key |= ((_smallMeshIndex&0x0000ffff)<<16)|((transformType&0x00000003)<<14)|(z&0x00003fff);

		casterIndex = _casterIndex;
		partsIndex = _partsIndex;
	}
};


struct CompactShadowCasterKeySorter
{
	inline bool operator()(const CompactCasterSortData& a, const CompactCasterSortData& b)
{
		return a.key < b.key;
	}
};

// Shadow caster sorting 
// Input:	_splitIndex		- cascade index
//			_data			- casters object related data
//			_dataParts		- casters materials related data
// Output:	_resultOrder		- sorted shadow caster draws
// Returns: Number of active casters
static int SortCastersCompact( int _splitIndex, ShadowCasters& _data, ShadowCasterParts& _dataParts, const ShadowCameraData& _cameraData, CompactCasterSortData* _resultOrder)
{
	int activeCasters = 0;

	// Generate key array for sorting
	int cascadeMask = 1 << _splitIndex;
	for( int i = 0; i < _data.size(); ++i )
	{
		ShadowCasterData& caster = _data[i];

		// This caster can be skipped for this shadow render pass (e.g. this face of cubemap or this split of directional shadow).
		if( caster.visibleCascades & cascadeMask )
		{
			for( size_t m = caster.partsStartIndex; m < caster.partsEndIndex; ++m )
			{
				const TransformInfo& xformInfo = caster.node->renderer->GetTransformInfo ();
				Matrix4x4f worldToClipMatrix = _cameraData.cameraWorldToClip;

			const Vector3f& worldPos = caster.worldAABB->GetCenter();
				float z = worldToClipMatrix.Get (2, 0) * worldPos.x + worldToClipMatrix.Get (2, 1) * worldPos.y + worldToClipMatrix.Get (2, 2) * worldPos.z + worldToClipMatrix.Get (2, 3);
				float w = worldToClipMatrix.Get (3, 0) * worldPos.x + worldToClipMatrix.Get (3, 1) * worldPos.y + worldToClipMatrix.Get (3, 2) * worldPos.z + worldToClipMatrix.Get (3, 3);
				float z_proj = z/w;
				z_proj = max(z_proj,0.0f);
				z_proj = min(z_proj,1.0f);
				_resultOrder[activeCasters++] = CompactCasterSortData( caster.node->renderer->GetMeshIDSmall(), _dataParts[m].material->GetShadowCasterHash(),
					xformInfo.transformType, z_proj, i, m );
			}
		}
	}

	std::sort( _resultOrder, _resultOrder + activeCasters, CompactShadowCasterKeySorter() );

	return activeCasters;
		}

static void RenderCasters( int splitIndex, const Light& light, const Vector3f& lightPos, const Vector3f& lightDir, ShadowCasters& data, ShadowCasterParts& dataParts, const ShadowCameraData& cameraData )
{
	GfxDevice& device = GetGfxDevice();

	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);

	device.SetInverseScale(1.0f);

#if GFX_ENABLE_SHADOW_BATCHING

	CompactCasterSortData* sortOrder;
	ALLOC_TEMP(sortOrder, CompactCasterSortData, dataParts.size());
	int activeCasters = SortCastersCompact( splitIndex, data, dataParts, cameraData, sortOrder);
	device.GetFrameStats().AddShadowCasters(activeCasters);

	if (activeCasters == 0)
		return;

	BatchRenderer casterBatchRenderer;	
	UInt64 previousKey = ((sortOrder[0].key)&0xFFFFFFFFFFFFC000ULL); // depth component does not affect state change boundaries
	UInt32 prevCustomPropsHash = 0;
	const ShadowCasterPartData* part = &dataParts[sortOrder[0].partsIndex];
	const ChannelAssigns* channels = part->material->SetShadowCasterPassWithShader(part->shader, part->subShaderIndex);

	for(int i=0; i<activeCasters;i++)
	{
		UInt64 currentKey = ((sortOrder[i].key)&0xFFFFFFFFFFFFC000ULL);

		BaseRenderer* renderer = data[sortOrder[i].casterIndex].node->renderer;
		const TransformInfo& xformInfo = renderer->GetTransformInfo ();
		part = &dataParts[sortOrder[i].partsIndex];
		
		const UInt32 customPropsHash = renderer->GetCustomPropertiesHash();
		renderer->ApplyCustomProperties(*part->material, part->shader, part->subShaderIndex);

		if (previousKey != currentKey || prevCustomPropsHash != customPropsHash)	// Flush() and update state when key changes
		{
			casterBatchRenderer.Flush();
			channels = part->material->SetShadowCasterPassWithShader(part->shader, part->subShaderIndex);
		}
			
		// if this pass needs to be rendered
		if (channels)
			casterBatchRenderer.Add(renderer, part->subMeshIndex, channels, xformInfo.worldMatrix, xformInfo.transformType);

		previousKey = currentKey;
		prevCustomPropsHash = customPropsHash;
	}
	casterBatchRenderer.Flush();

#else 
	
	int castersSize = data.size();
	device.GetFrameStats().AddShadowCasters(castersSize);
	int cascadeMask = 1 << splitIndex;
	for( int i = 0; i < castersSize; ++i )
	{
		ShadowCasterData& caster = data[i];

		// This caster can be skipped for this shadow render pass (e.g. this face of cubemap
		// or this split of directional shadow).
		if( caster.visibleCascades & cascadeMask )
		{
			BaseRenderer* renderer = caster.node->renderer;
			const TransformInfo& xformInfo = renderer->GetTransformInfo ();

			SetupObjectMatrix(xformInfo.worldMatrix, xformInfo.transformType);
			size_t partsStartIndex = caster.partsStartIndex;
			size_t partsEndIndex = caster.partsEndIndex;
			for( size_t m = partsStartIndex; m < partsEndIndex; ++m )
			{
				ShadowCasterPartData& part = dataParts[m];

				//@TODO: if this returns true and we have any sort of batching, we'd have to break batches here
				renderer->ApplyCustomProperties(*(part.material), part.shader, part.subShaderIndex);

				const ChannelAssigns* channels = part.material->SetShadowCasterPassWithShader(part.shader, part.subShaderIndex);
				renderer->Render( part.subMeshIndex, *channels );
			}
		}
	}

#endif // GFX_ENABLE_SHADOW_BATCHING

	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}

// return false if focus region is empty: no need to render anything in that case
static bool SetupDirectionalLightShadowCamera(
		const ShadowCameraData& cameraData,
		const Light& light,
		int splitIndex, int shadowSizeX, int shadowSizeY,
		const MinMaxAABB& casterBounds, const MinMaxAABB& receiverBounds, const Transform& lt,
		ShadowCascadeInfo& outCascade )
{
	DebugAssertIf( light.GetType() != kLightDirectional );
	DebugAssertIf( splitIndex < 0 || splitIndex >= cameraData.splitCount );

	const Camera& camera = *cameraData.camera;
	const Matrix4x4f* frustumTransform = &cameraData.cameraClipToWorld;
	Matrix4x4f localFrustumTransform;
	float cameraFarZ = cameraData.projectionFar;
	float shadowFarZ = cameraData.shadowDistance;
	float farPlaneScale = 1.0f;

	ShadowProjection projectionType = (ShadowProjection)GetQualitySettings().GetCurrent().shadowProjection;
	if( projectionType == kShadowProjStableFit )
	{
		/////////////////@TODO: WTF? Static abuse??? static Matrix4x4f cameraProjection;
		
		// Choose the camera-local frustum matrix to keep us numerically stable!
		static Matrix4x4f cameraProjection;
		camera.GetImplicitProjectionMatrix( cameraData.projectionNear, cameraData.projectionFar, cameraProjection );
		Matrix4x4f::Invert_Full( cameraProjection, localFrustumTransform );
		frustumTransform = &localFrustumTransform;

		Vector3f cornerPos;
		localFrustumTransform.PerspectiveMultiplyPoint3( Vector3f(1, 1, 1), cornerPos );
		float cornerDist = Magnitude( cornerPos );

		// We scale our frustum to unit size by dividing lengths by shadowDistance
		// and intersect the sphere with center (0,0,ShadowSphereOffset) going through (0,0,1)
		// We need to get the Z distance where the sphere intersects the frustum edge
		// This is really a 2D problem in the plane between two opposite edges of the frustum
		// Let's look at the right-angled triangle with sides b = 1, c = cornerDist / farPlaneZ
		// Pythagoras gives us the length of a: a^2 + b^2 = c^2, b=1 -> a = sqrt(c^2 - 1)
		// We want to intersect the line y = a*x -> y = sqrt(c^2 - 1) * x
		// and the circle (x-p)^2 + y^2 = r^2 with radius r and center (p,0)
		float c = cornerDist / cameraFarZ;
		float p = CalculateShadowSphereOffset(camera);
		float r = 1.0f - p;
		// Wolfram Alpha solution for (x-p)^2 + (sqrt(c^2 - 1)*x)^2 = r^2
		farPlaneScale = (sqrt(-c*c*p*p+c*c*r*r+p*p)+p)/(c*c);

		#if !UNITY_RELEASE
		// Check that the distance we calculate the frustum from is correct
		Vector3f edgeVector = cornerPos / cameraFarZ;
		Vector3f frustumIntersection = edgeVector * shadowFarZ * farPlaneScale;
		float shadowRadius = r * shadowFarZ;
		float centerDist = p * shadowFarZ;
		Vector3f shadowCenter(0, 0, -centerDist);
		float dist = Magnitude( frustumIntersection - shadowCenter );
		DebugAssert( Abs(dist - shadowRadius) < 0.001f * shadowRadius );
		#endif
	}
	float nearZ = cameraData.projectionNear;
	float scaledShadowRange = shadowFarZ * farPlaneScale - nearZ;
	float frustumScale = scaledShadowRange / (cameraFarZ - nearZ);
	if( frustumScale <= Vector3f::epsilon )
	{
		return false;
	}

	// calculate frustum split corners
	Vector3f cameraFrustum[8];
	GetFrustumPoints( *frustumTransform, cameraFrustum );
	Vector3f frustumSplit[8];
	// split factors are relative to camera frustum (not shadow frustum)
	float nearSplit = cameraData.splitPercentages[splitIndex] * frustumScale;
	float farSplit = cameraData.splitPercentages[splitIndex+1] * frustumScale;
	outCascade.minViewDistance = nearZ + nearSplit * (cameraFarZ - nearZ);
	outCascade.maxViewDistance = nearZ + farSplit * (cameraFarZ - nearZ);
	GetFrustumPortion( cameraFrustum, nearSplit, farSplit, frustumSplit );

	std::vector<Vector3f> focusPoints;
	if( projectionType == kShadowProjCloseFit )
	{
		// find the focused body: intersection of frustum & receiver bounds, extruded along
		// light to include all casters
		Vector3f lightDir = lt.TransformDirection(Vector3f(0,0,1));
		CalculateFocusedLightHull( frustumSplit, lightDir, receiverBounds, focusPoints);
		if( focusPoints.empty() )
		{
			outCascade.lightMatrix.SetIdentity();
			outCascade.projMatrix.SetOrtho( -1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 10.0f );
			return false;
		}
	}
	else
	{
		// TODO: if frustum does not intersect scene caster&receiver bounds, return false
	}

	// do initial light placement
	Vector3f center = casterBounds.GetCenter();
	float castersRadius = Magnitude(casterBounds.GetMax() - casterBounds.GetMin()) * 0.5f;
	Vector3f axisX = lt.TransformDirection(Vector3f(1,0,0));
	Vector3f axisY = lt.TransformDirection(Vector3f(0,1,0));
	Vector3f axisZ = lt.TransformDirection(Vector3f(0,0,1));
	Vector3f pos = center - axisZ * castersRadius * 1.2f;

	outCascade.lightMatrix.SetPositionAndOrthoNormalBasis( pos, axisX, axisY, axisZ );

	// In Z direction, the final light frustum must encapsulate both caster and receiver bounds.
	// So take union of those, transform into light space and figure out min/max Z.
	MinMaxAABB unionBounds = AddAABB( casterBounds, receiverBounds );
	float minLightZ = std::numeric_limits<float>::infinity();
	float maxLightZ = -std::numeric_limits<float>::infinity();
	Vector3f unionPoints[8];
	unionBounds.GetVertices( unionPoints );
	for( int i = 0; i < 8; ++i )
	{
		Vector3f p = outCascade.lightMatrix.InverseMultiplyPoint3Affine( unionPoints[i] );
		minLightZ = std::min( p.z, minLightZ );
		maxLightZ = std::max( p.z, maxLightZ );
	}
	float centerLightSpaceZ = (minLightZ + maxLightZ) * 0.5f;
	float lightDistanceZ = (maxLightZ - minLightZ) * 0.5f;

	Vector3f boundsSize;

	// calculate frustum bounds in light space
	MinMaxAABB frustumBounds;
	if( projectionType == kShadowProjCloseFit )
	{
		for( int i = 0; i < focusPoints.size(); ++i )
		{
			Vector3f p = outCascade.lightMatrix.InverseMultiplyPoint3Affine( focusPoints[i] );
			p.z = centerLightSpaceZ;
			frustumBounds.Encapsulate( p );
		}
		boundsSize = frustumBounds.GetMax() - frustumBounds.GetMin();
	}
	else if( projectionType == kShadowProjStableFit )
	{
		Vector3f sphereCenter;
		float radius;
		// Sphere is in camera space, so view vector is along negative Z
		CalculateBoundingSphereFromFrustumPoints( frustumSplit, sphereCenter, radius );
		float maxViewDist = Abs( sphereCenter.z ) + radius;
		outCascade.maxViewDistance = std::min( maxViewDist, shadowFarZ );

		// Now we transform our sphere center into world coordinates
		sphereCenter = camera.GetCameraToWorldMatrix().MultiplyPoint3( sphereCenter );
		outCascade.outerSphere.Set( sphereCenter, radius );

		Vector3f p = outCascade.lightMatrix.InverseMultiplyPoint3Affine( sphereCenter );
		p.z = centerLightSpaceZ;
		frustumBounds.Encapsulate( p );
		frustumBounds.Expand( radius );
		boundsSize = Vector3f(radius, radius, radius) * 2.0f;
	}
	else
	{
		for( int i = 0; i < 8; ++i )
		{
			Vector3f p = outCascade.lightMatrix.InverseMultiplyPoint3Affine( frustumSplit[i] );
			p.z = centerLightSpaceZ;
			frustumBounds.Encapsulate( p );
		}
		boundsSize = frustumBounds.GetMax() - frustumBounds.GetMin();
	}
	Vector3f boundsCenter = frustumBounds.GetCenter();
	Vector3f halfSize = boundsSize * 0.5f;

	// add a small guard band to prevent sampling outside map
	//static const float kGuardPixels = 1.0f;
	//frustumBounds.Expand( Vector3f(kGuardPixels / shadowSizeX, kGuardPixels / shadowSizeY, 0) );

	// quantize the position to shadow map texel size; gets rid of some "shadow swimming"
	double texelSizeX = boundsSize.x / shadowSizeX;
	double texelSizeY = boundsSize.y / shadowSizeY;
	pos = outCascade.lightMatrix.MultiplyPoint3(boundsCenter);
	double projX = axisX.x * (double)pos.x + axisX.y * (double)pos.y + axisX.z * (double)pos.z;
	double projY = axisY.x * (double)pos.x + axisY.y * (double)pos.y + axisY.z * (double)pos.z;
	float modX = float( fmod( projX, texelSizeX ) );
	float modY = float( fmod( projY, texelSizeY ) );
	pos -= axisX * modX;
	pos -= axisY * modY;

	// move position back so it encloses everything we need
	pos -= axisZ * lightDistanceZ * 1.1f;
	outCascade.lightMatrix.SetPosition( pos );	

	outCascade.nearPlane = lightDistanceZ*0.1f;
	outCascade.farPlane = lightDistanceZ*2.2f;
	outCascade.projMatrix.SetOrtho( -halfSize.x, halfSize.x, -halfSize.y, halfSize.y, outCascade.nearPlane, outCascade.farPlane );

	outCascade.viewMatrix = outCascade.lightMatrix;
	outCascade.viewMatrix.SetAxisZ( -outCascade.viewMatrix.GetAxisZ() );
	outCascade.viewMatrix.Invert_Full();

	Matrix4x4f texMatrix = Matrix4x4f::identity;
	texMatrix.Get(0,0) = 0.5f;
	texMatrix.Get(1,1) = 0.5f;
	texMatrix.Get(2,2) = 0.5f;
	texMatrix.Get(0,3) = 0.5f;
	texMatrix.Get(1,3) = 0.5f;
	texMatrix.Get(2,3) = 0.5f;

	MultiplyMatrices4x4 (&outCascade.projMatrix, &outCascade.viewMatrix, &outCascade.worldToClipMatrix);
	MultiplyMatrices4x4 (&texMatrix, &outCascade.worldToClipMatrix, &outCascade.shadowMatrix);
	return true;
}


static bool PositionShadowSpotCamera( const ShadowCameraData& cameraData, const Light* light, Matrix4x4f& outShadowMatrix )
{
	DebugAssertIf( light->GetType() != kLightSpot );
	GfxDevice& device = GetGfxDevice();

	Matrix4x4f viewMatrix, projMatrix;
	const Transform& lt = light->GetComponent(Transform);
	// just use spotlight
	Matrix4x4f s;
	s.SetScale( Vector3f(1,1,-1) );

	Matrix4x4f worldToLocalMatrixNoScale = lt.GetWorldToLocalMatrixNoScale();
	MultiplyMatrices4x4 (&s, &worldToLocalMatrixNoScale, &viewMatrix);
	// On NVIDIA cards in OpenGL using too low near plane results in shadow artifacts. Something like 0.02
	// is when artifacts start to appear. So I set near plane to be 4% of the range, that seems to work ok.
	float nearPlane = light->GetRange() * 0.04f;
	float farPlane = light->GetRange();
	projMatrix.SetPerspectiveCotan( light->GetCotanHalfSpotAngle(), nearPlane, farPlane );

	device.SetProjectionMatrix (projMatrix);
	device.SetViewMatrix( viewMatrix.GetPtr() );
	SetClippingPlaneShaderProps();

	// shadow bias
	float bias = light->GetShadowBias ();
	float clampVerts = 0.0f; // disable vertex clamping for spot lights
	device.GetBuiltinParamValues().SetVectorParam(kShaderVecLightShadowBias, Vector4f(bias, clampVerts, 0, 0));

	Matrix4x4f texMatrix = Matrix4x4f::identity;
	texMatrix.Get(0,0) = 0.5f;
	texMatrix.Get(1,1) = 0.5f;
	texMatrix.Get(2,2) = 0.5f;
	texMatrix.Get(0,3) = 0.5f;
	texMatrix.Get(1,3) = 0.5f;
	texMatrix.Get(2,3) = 0.5f;

	Matrix4x4f temp;
	// outShadowMatrix = texMatrix * projMatrix * viewMatrix
	MultiplyMatrices4x4 (&texMatrix, &projMatrix, &temp);
	MultiplyMatrices4x4 (&temp, &viewMatrix, &outShadowMatrix);

	ShaderLab::g_GlobalProperties->SetVector( kSLPropShadowProjectionParams, 0.0f, nearPlane, farPlane, 0.0f );
	return true;
}


static void PositionShadowPointCamera( const Vector3f& lightPos, float lightRange, CubemapFace face, Matrix4x4f& outWorldToClipMatrix, Vector3f& outViewDir )
{
	GfxDevice& device = GetGfxDevice();

	Matrix4x4f viewMatrix, projMatrix;

	switch( face ) {
	case kCubeFacePX:
		outViewDir = Vector3f( 1, 0, 0);
		viewMatrix.SetOrthoNormalBasisInverse( Vector3f( 0, 0,-1), Vector3f( 0,-1, 0), Vector3f(-1, 0, 0) );
		break;
	case kCubeFaceNX:
		outViewDir = Vector3f(-1, 0, 0);
		viewMatrix.SetOrthoNormalBasisInverse( Vector3f( 0, 0, 1), Vector3f( 0,-1, 0), Vector3f( 1, 0, 0) );
		break;
	case kCubeFacePY:
		outViewDir = Vector3f( 0, 1, 0);
		viewMatrix.SetOrthoNormalBasisInverse( Vector3f( 1, 0, 0), Vector3f( 0, 0, 1), Vector3f( 0,-1, 0) );
		break;
	case kCubeFaceNY:
		outViewDir = Vector3f( 0,-1, 0);
		viewMatrix.SetOrthoNormalBasisInverse( Vector3f( 1, 0, 0), Vector3f( 0, 0,-1), Vector3f( 0, 1, 0) );
		break;
	case kCubeFacePZ:
		outViewDir = Vector3f( 0, 0, 1);
		viewMatrix.SetOrthoNormalBasisInverse( Vector3f( 1, 0, 0), Vector3f( 0,-1, 0), Vector3f( 0, 0,-1) );
		break;
	case kCubeFaceNZ:
		outViewDir = Vector3f( 0, 0,-1);
		viewMatrix.SetOrthoNormalBasisInverse( Vector3f(-1, 0, 0), Vector3f( 0,-1, 0), Vector3f( 0, 0, 1) );
		break;
	default:
		AssertString("Invalid cube face!");
		outViewDir = Vector3f( 0, 0, 0);
		viewMatrix.SetIdentity();
		break;
	}

	Matrix4x4f tr;
	tr.SetTranslate( -lightPos );
	viewMatrix *= tr;

	float nearPlane = std::min(lightRange * 0.01f,0.01f);
	float farPlane = lightRange * 1.01f;
	projMatrix.SetPerspective( 90.0f, 1.0f, nearPlane, farPlane );
	device.SetProjectionMatrix (projMatrix);
	device.SetViewMatrix( viewMatrix.GetPtr() );
	SetClippingPlaneShaderProps();
	MultiplyMatrices4x4 (&projMatrix, &viewMatrix, &outWorldToClipMatrix);

	ShaderLab::g_GlobalProperties->SetVector( kSLPropShadowProjectionParams, 0.0f, nearPlane, farPlane, 0.0f );
}


static int CalculateShadowMapSize( const ShadowCameraData& cameraData, const ActiveLight& activeLight )
{
	const Light* light = activeLight.light;
	const Rectf& bounds = activeLight.screenRect;
	int mapSize = 128;

#if UNITY_PS3
	// Always allow high quality shadows.
	bool allowHighQualityShadows = true;
#elif UNITY_XENON
	// Only allow high quality shadows if shadow resolution is Very High or higher. This enables predicated tiling.
	bool allowHighQualityShadows = (light->GetFinalShadowResolution() >= 3);
#else
	bool allowHighQualityShadows = (gGraphicsCaps.videoMemoryMB >= kVRAMEnoughForLargeShadowmaps);
#endif

	const float kMultPoint = 1.0f; // Assume "Very High" shadow map resolution is 1x screen size for point lights.
	const float kMultSpot = 2.0f;  // Assume "Very High" shadow map resolution is 2x screen size for spot lights.
	const float kMultDir = 3.8f;   // Assume "Very High" shadow map resolution is almost 4x of screen size for directional lights.

	switch( light->GetType() )
	{
	case kLightPoint:
		{
			const int kMaxShadowSize = std::min( gGraphicsCaps.maxCubeMapSize, allowHighQualityShadows ? kShadowmapPointSizeMax : kShadowmapPointSizeMin );
			// Based on light size on screen
			float pixelSize = std::max( bounds.width * cameraData.viewWidth, bounds.height * cameraData.viewHeight );
			mapSize = NextPowerOfTwo( int(pixelSize * kMultPoint) );
			mapSize >>= cameraData.qualityShift;
			mapSize = clamp<int>( mapSize, 16, kMaxShadowSize );
		}
		break;
	case kLightSpot:
		{
			const int kMaxShadowSize = std::min( gGraphicsCaps.maxRenderTextureSize, allowHighQualityShadows ? kShadowmapSpotSizeMax : kShadowmapSpotSizeMin );
			// Based on light size on screen
			float pixelSize = std::max( bounds.width * cameraData.viewWidth, bounds.height * cameraData.viewHeight );
			mapSize = NextPowerOfTwo( int( pixelSize * kMultSpot ) );
			mapSize >>= cameraData.qualityShift;
			mapSize = clamp<int>( mapSize, 16, kMaxShadowSize );
		}
		break;
	case kLightDirectional:
		{
			const int kMaxShadowSize = std::min( gGraphicsCaps.maxRenderTextureSize, allowHighQualityShadows ? kShadowmapDirSizeMax : kShadowmapDirSizeMin );
			int viewSize = int( std::max( cameraData.viewWidth, cameraData.viewHeight ) );
			mapSize = NextPowerOfTwo( int( viewSize * kMultDir ) );
			mapSize >>= cameraData.qualityShift;
			mapSize = clamp<int>( mapSize, 32, kMaxShadowSize );
		}
		break;
	default:
		AssertString( "Unknown light type!" );
	}

	return mapSize;
}

static void PrepareShadowMapParams (ShadowCameraData& camData, const Light* light, Matrix4x4f outShadowMatrices[kMaxShadowCascades])
{
	// Use cascaded shadow maps for directional lights and perspective cameras only.
	// (cascaded shadow maps lose their point for ortho cameras).
	bool usePSSM = (light->GetType() == kLightDirectional) && (!camData.camera->GetOrthographic());

	// Quality setting for shadow maps
	const int lightShadowResolution = light->GetFinalShadowResolution();
	camData.qualityShift = QualitySettings::kShadowResolutionCount - 1 - lightShadowResolution;


	if( usePSSM )
	{
		if ( GetGfxDevice().GetRenderer() == kGfxRendererOpenGLES20Mobile || GetGfxDevice().GetRenderer() == kGfxRendererOpenGLES30 )
			camData.splitCount = 1;
		else
			camData.splitCount = GetQualitySettings().GetCurrent().shadowCascades;
		
		CalculatePSSMDistances( camData.camera->GetNear(), camData.shadowDistance, camData.splitCount, camData.splitDistances, camData.splitPercentages );
		for( int i = camData.splitCount+1; i < kMaxShadowCascades+1; ++i )
		{
			camData.splitDistances[i] = camData.splitDistances[i-1] * 1.1f;
			camData.splitPercentages[i] = camData.splitPercentages[i-1] * 1.1f;
		}
	}
	else
	{
		camData.splitDistances[0] = camData.camera->GetNear();
		camData.splitDistances[1] = camData.shadowDistance;
		camData.splitPercentages[0] = 0.0f;
		camData.splitPercentages[1] = 1.0f;
		camData.splitCount = 1;
	}
	// Clear shadow split spheres
	Vector4f unusedSphere(0, 0, 0, -std::numeric_limits<float>::infinity());
	for( int i = 0; i < kMaxShadowCascades; ++i )
	{
		camData.splitSphereCentersAndSquaredRadii[i] = unusedSphere;
	}
	// Zero out unused shadow map matrices. Otherwise for some reason occasionally causes wrong rendering
	// on D3D REF.
	for (int i = camData.splitCount; i < kMaxShadowCascades; ++i)
	{
		memset (&outShadowMatrices[i].m_Data[0], 0, sizeof(outShadowMatrices[i]));
	}
}


RenderTexture* RenderShadowMaps( ShadowCameraData& cameraData, const ActiveLight& activeLight, const MinMaxAABB& receiverBounds, bool excludeLightmapped, Matrix4x4f outShadowMatrices[kMaxShadowCascades] )
{
	const Light* light = activeLight.light;
	PROFILER_AUTO_GFX(gShadowsRender, light);
	GPU_AUTO_SECTION(kGPUSectionShadowPass);

	DebugAssertIf( outShadowMatrices == NULL );

	if (!receiverBounds.IsValid())
		return NULL;

	PrepareShadowMapParams (cameraData, light, outShadowMatrices);

	RenderTexture* shadowmap = NULL;
	GfxDevice& device = GetGfxDevice();

	int shadowSize = CalculateShadowMapSize( cameraData, activeLight );
	int shadowWidth = shadowSize;
	int shadowHeight = shadowSize;
	DepthBufferFormat depthFormat = kDepthFormat16;
	RenderTextureFormat shadowFormat;
	bool shadowCubeMap;
	if( light->GetType() == kLightPoint )
	{
		shadowFormat = kRTFormatARGB32;
		shadowCubeMap = true;
		if (!gGraphicsCaps.hasRenderToCubemap)
			return NULL;
	}
	else
	{
		// two splits cascaded shadow map should use 2:1 aspect texture
		if( cameraData.splitCount == 2 )
			shadowHeight /= 2;
		shadowFormat = gGraphicsCaps.hasNativeShadowMap ? kRTFormatShadowMap : kRTFormatDepth;
		if (gGraphicsCaps.buggySpotNativeShadowMap && light->GetType() == kLightSpot)
			shadowFormat = kRTFormatDepth;
		
		shadowCubeMap = false;
	}

	// Try to somewhat intelligently reduce shadow map resolution if we're getting close to VRAM limits.
	// Only take into account things that can't be easily moved off-VRAM (screen + render textures).
	// Allow shadowmap to take 1/3 of the available VRAM at max.
	const int vramSizeKB = int(gGraphicsCaps.videoMemoryMB * 1024);
	const GfxDeviceStats::MemoryStats& memoryStats = device.GetFrameStats().GetMemoryStats();
	const int currentVramUsageKB = (memoryStats.screenBytes + memoryStats.renderTextureBytes) / 1024;
	const int allowedVramUsageKB = int((vramSizeKB - currentVramUsageKB) * kVRAMMaxFreePortionForShadowMap);
	int neededVramForShadowmapKB;
	do {
		neededVramForShadowmapKB = EstimateRenderTextureSize (shadowWidth, shadowHeight, 1, shadowFormat, depthFormat, shadowCubeMap?kTexDimCUBE:kTexDim2D, false) / 1024;
		if( neededVramForShadowmapKB < allowedVramUsageKB )
			break;
		#if !UNITY_RELEASE
		printf_console("Shadowmap %ix%i won't fit, reducing size (needed mem=%i used mem=%i allowedmem=%i)\n", shadowWidth, shadowHeight, neededVramForShadowmapKB, currentVramUsageKB, allowedVramUsageKB );
		#endif
		shadowWidth /= 2;
		shadowHeight /= 2;
	} while( shadowWidth > 4 && shadowHeight > 4 );
	// We totally don't have VRAM for shadows left! Continue without shadows...
	if( shadowWidth <= 4 || shadowHeight <= 4 )
		return NULL;

	
	///////////////@TODO: Move creation of the shadow buffer until after we have determined if there is anything to be culled...
	
	
	// Create the shadowmap
	UInt32 flags = 0;
	if (shadowCubeMap)
		flags |= RenderBufferManager::kRBCubemap;
	shadowmap = GetRenderBufferManager().GetTempBuffer (shadowWidth, shadowHeight, depthFormat, shadowFormat, flags, kRTReadWriteLinear);
	
	// By default, enable PCF filtering for native shadow maps.
	// However on mobile that's quite expensive, so only enable it if light has Soft
	// shadows set.
	bool enablePCFFilter = (shadowFormat==kRTFormatShadowMap);
	if (!gGraphicsCaps.hasShadowCollectorPass && (light->GetShadows() < kShadowSoft))
		enablePCFFilter = false;
	// Disable PCF filtering if we need to due to driver issues
	if (gGraphicsCaps.buggyShadowMapBilinearSampling)
		enablePCFFilter = false;
	
	shadowmap->GetSettings().m_FilterMode = enablePCFFilter ? kTexFilterBilinear : kTexFilterNearest;
	shadowmap->ApplySettings();

	// Check if shadow map can be actually created. If for some reason it can't, return NULL.
	if( !shadowmap->IsCreated() )
	{
		if( !shadowmap->Create() )
		{
			GetRenderBufferManager().ReleaseTempBuffer( shadowmap );
			return NULL;
		}
	}

	MinMaxAABB casterBounds;
	ShadowCasters casters;
	ShadowCasterParts casterParts;

	// Cull shadow casters
	CullingOutput visibleShadowCasters; 
	{
		PROFILER_AUTO(gCullShadowCasters, NULL);
		CreateCullingOutput(cameraData.sceneCullParameters->renderers, visibleShadowCasters);
		CullShadowCasters (*activeLight.light, cameraData, cameraData.sceneCullParameters->excludeLightmappedShadowCasters, visibleShadowCasters);
		
	}
	// Send OnBecameVisible / OnBecameInvisible callback for culled shadow caster renderers
	GetScene().NotifyVisible (visibleShadowCasters);

	casters.reserve(64);
	casterParts.reserve(64);
	
	GenerateShadowCasterParts (*light, cameraData, visibleShadowCasters, casterBounds, casters, casterParts);

	DestroyCullingOutput(visibleShadowCasters);
	
	int castersSize = casters.size();
	if( castersSize == 0 )
	{
		// If there are no shadow casters, there will be no shadows. Return NULL shadowmap
		// in this case, the render queue code will use non-shadowed path in then.
		GetRenderBufferManager().ReleaseTempBuffer( shadowmap );
		return NULL;
	}


	SetAndRestoreWireframeMode setWireframeOff(false); // turn off wireframe; will restore old value in destructor

	// If all casters for directional light are outside the view frustum, then caster bounds
	// will be invalid at this point. In that case, make them equal to receiver bounds (case 17871).
	if( !casterBounds.IsValid() )
		casterBounds = receiverBounds;

	const Transform& lt = light->GetComponent(Transform);
	Vector3f lightPos = lt.GetPosition();
	Quaternionf lightRot = lt.GetRotation();
	Vector3f lightDir = RotateVectorByQuat(lightRot, Vector3f(0,0,1));

	if( light->GetType() == kLightPoint )
	{
		PROFILER_AUTO_GFX(gShadowsRenderPoint,light);
		// point light: render into cube map
		device.GetBuiltinParamValues().SetVectorParam(kShaderVecLightPositionRange, Vector4f(lightPos.x, lightPos.y, lightPos.z, 1.0f/light->GetRange()));
		for( int f = 0; f < 6; ++f )
		{
			CubemapFace face = (CubemapFace)f;
			// activate shadow render target
			RenderTexture::SetActive (shadowmap, 0, face, RenderTexture::kFlagDontRestore);

			GraphicsHelper::Clear (kGfxClearAll, ColorRGBAf(1,1,1,1).GetPtr(), 1.0f, 0);
			GPU_TIMESTAMP();

			// position the shadow camera
			Matrix4x4f shadowWorldToClip;
			Vector3f viewDir;
			PositionShadowPointCamera( lightPos, light->GetRange(), face, shadowWorldToClip, viewDir );
			Plane planes[6];
			ExtractProjectionPlanes( shadowWorldToClip, planes );

			// Go over casters and mark the ones that are not in our face pyramid as skipped.
			// First four planes are the ones to check against (left, right, bottom, top).
			int casterCount = casters.size();
			for( int c = 0; c < casterCount; ++c )
			{
				ShadowCasterData& caster = casters[c];
				if( IntersectAABBFrustum( *caster.worldAABB, planes, 15 ) )
					caster.visibleCascades = 1;
				else
					caster.visibleCascades = 0;
			}

			RenderCasters( 0, *light, lightPos, viewDir, casters, casterParts, cameraData );
		}
	}
	else if( light->GetType() == kLightDirectional )
	{
		PROFILER_AUTO_GFX(gShadowsRenderDir,light);
		// directional light: render splits
		RenderTexture::SetActive (shadowmap, 0, kCubeFaceUnknown, RenderTexture::kFlagDontRestore);
		GraphicsHelper::Clear (kGfxClearAll, ColorRGBAf(1,1,1,1).GetPtr(), 1.0f, 0);
		GPU_TIMESTAMP();

		int tilesX, tilesY;
		switch( cameraData.splitCount )
		{
		case 1: tilesX = 1; tilesY = 1; break;
		case 2: tilesX = 2; tilesY = 1; break;
		case 4: tilesX = 2; tilesY = 2; break;
		default: tilesX = 1; tilesY = 1; AssertString( "Unknown split count!" );
		}

		int splitIndex = 0;
		bool validMatrices[kMaxShadowCascades];
		memset(validMatrices, 0, sizeof(validMatrices));
		int lastValidMatrix = 0;
		int tileSizeX = shadowWidth / tilesX;
		int tileSizeY = shadowHeight / tilesY;
		ShadowCascadeInfo cascades[4];
		for( int ty = 0; ty < tilesY; ++ty )
		{
			for( int tx = 0; tx < tilesX; ++tx )
			{
				// position the shadow camera for this split
				ShadowCascadeInfo& cascade = cascades[splitIndex];
				cascade.shadowMatrix.SetIdentity();
				cascade.outerSphere.Set( Vector3f::zero, -1e9f );
				cascade.enabled = SetupDirectionalLightShadowCamera( cameraData, *light, splitIndex,
					tileSizeX, tileSizeY, casterBounds, receiverBounds, lt, cascade );
				const Sphere& sphere = cascade.outerSphere;
				outShadowMatrices[splitIndex] = cascade.shadowMatrix;
				cameraData.splitSphereCentersAndSquaredRadii[splitIndex] = Vector4f(sphere.GetCenter(), Sqr(sphere.GetRadius()));
				++splitIndex;
			}
		}
		CullDirectionalCascades( casters, cascades, splitIndex, lightRot, lightDir, cameraData);
		splitIndex = 0;
		for( int ty = 0; ty < tilesY; ++ty )
		{
			for( int tx = 0; tx < tilesX; ++tx )
			{
				const ShadowCascadeInfo& cascade = cascades[splitIndex];
				if( cascade.enabled )
				{
					device.SetProjectionMatrix(cascade.projMatrix);
					device.SetViewMatrix( cascade.viewMatrix.GetPtr() );
					SetClippingPlaneShaderProps();

					// shadow bias
					float bias = light->GetShadowBias ();
					bias *= device.GetDeviceProjectionMatrix()[2*4+2] * -1.0f; // make bias constant in world space
					float clampVerts = 1.0f; // enable vertex clamping for directional lights
					device.GetBuiltinParamValues().SetVectorParam(kShaderVecLightShadowBias, Vector4f(bias, clampVerts, 0, 0));

					ShaderLab::g_GlobalProperties->SetVector( kSLPropShadowProjectionParams, 0.0f, cascade.nearPlane, cascade.farPlane, 0.0f );

					Matrix4x4f texMatrix = Matrix4x4f::identity;
					texMatrix.Get(0,0) = 1.0f / tilesX;
					texMatrix.Get(1,1) = 1.0f / tilesY;
					texMatrix.Get(2,2) = 1.0f;
					texMatrix.Get(0,3) = (float)tx / (float)tilesX;
					texMatrix.Get(1,3) = (float)ty / (float)tilesY;

					MultiplyMatrices4x4 (&texMatrix, &cascade.shadowMatrix, &outShadowMatrices[splitIndex]);
					lastValidMatrix = splitIndex;
					validMatrices[splitIndex] = true;

					if( cameraData.splitCount == 1 )
						device.SetViewport( tx * tileSizeX+1, ty * tileSizeY+1, tileSizeX-2, tileSizeY-2 );
					else
						device.SetViewport( tx * tileSizeX, ty * tileSizeY, tileSizeX, tileSizeY );

					RenderCasters( splitIndex, *light, lightPos, lightDir, casters, casterParts, cameraData );
				}
				else
				{
					outShadowMatrices[splitIndex].SetIdentity();
				}
				++splitIndex;
			}
		}
		// make sure all matrices are valid, since depth comparisons are not exact
		//for( int i = 0; i < kMaxPSSMSplits; i++ )
		//	if( !validMatrices[i] )
		//		outShadowMatrices[i] = outShadowMatrices[lastValidMatrix];
	}
	else
	{
		PROFILER_AUTO_GFX(gShadowsRenderSpot,light);
		// spot light: render into single shadow map
		RenderTexture::SetActive (shadowmap, 0, kCubeFaceUnknown, RenderTexture::kFlagDontRestore);
		GraphicsHelper::Clear (kGfxClearAll, ColorRGBAf(1,1,1,1).GetPtr(), 1.0f, 0);
		GPU_TIMESTAMP();

		// position the shadow camera
		if( PositionShadowSpotCamera( cameraData, light, outShadowMatrices[0] ) )
		{
			RenderCasters( 0, *light, lightPos, lightDir, casters, casterParts, cameraData );
		}
	}

	return shadowmap;
}

RenderTexture* BlurScreenShadowMap (RenderTexture* screenShadowMap, ShadowType shadowType, float farPlane, float blurWidth, float blurFade)
{
	DebugAssert (shadowType != kShadowHard); // paranoia
	DebugAssert (shadowType != kShadowNone); // paranoia

	const float kBlurThreshold = 0.2f; // 20 cm. If needed, this could be exposed per-light and passed down here.

	float shaderBlurThreshold = kBlurThreshold / farPlane;
	float invFarMul4 = 4.0f / farPlane * blurFade;
	ShaderLab::g_GlobalProperties->SetVector (ShaderLab::Property("unity_ShadowBlurParams"), shaderBlurThreshold, invFarMul4, 0, 0);

	screenShadowMap->GetSettings().m_FilterMode = kTexFilterNearest;

	RenderTexture* blurredShadowMap = GetRenderBufferManager().GetTempBuffer (RenderBufferManager::kFullSize, RenderBufferManager::kFullSize, kDepthFormatNone, kRTFormatARGB32, 0, kRTReadWriteLinear);
	RenderTexture::SetActive (blurredShadowMap, 0, kCubeFaceUnknown, RenderTexture::kFlagDontRestore);
	// no need to clear

	SetAndRestoreWireframeMode setWireframeOff(false); // turn off wireframe; will restore old value in destructor

	const int* viewport = GetRenderManager().GetCurrentViewPort();
	const float fWidth = viewport[2];
	const float fHeight = viewport[3];

	static Material* shadowBlurDiscMaterial = NULL;
	static Material* shadowBlurDiscRotatedMaterial = NULL;
	if(shadowBlurDiscMaterial == NULL)
	{
		Shader* shader = GetScriptMapper().FindShader ("Hidden/Shadow-ScreenBlur");
		if (shader)
			shadowBlurDiscMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
	}
	if(shadowBlurDiscRotatedMaterial == NULL)
	{
		Shader* shader = GetScriptMapper().FindShader ("Hidden/Shadow-ScreenBlurRotated");
		if (shader)
			shadowBlurDiscRotatedMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
	}

	Material* material = NULL;
	int fillrate = GetGraphicsPixelFillrate(gGraphicsCaps.vendorID, gGraphicsCaps.rendererID);
	const bool shadowRotatedBlurFastEnough = (fillrate >= kShadowRotatedBlurFillrateThreshold) || (fillrate == -1); // if unknown, assume it's fast enough
	if(shadowBlurDiscRotatedMaterial && shadowBlurDiscRotatedMaterial->GetShader()->IsSupported() && shadowRotatedBlurFastEnough)
	{
		// This value is for compensating that rotated shadows generally look softer than non rotated and makes them look more like the same.
		const float kShadowBlurRotatedMultiplier = 0.8f;
		blurWidth *= kShadowBlurRotatedMultiplier;
		material = shadowBlurDiscRotatedMaterial;
	}
	else
	{
		material = shadowBlurDiscMaterial;
	}

	SHADERPROP (MainTex);
	SHADERPROP (BlurOffsets0);
	SHADERPROP (BlurOffsets1);
	SHADERPROP (BlurOffsets2);
	SHADERPROP (BlurOffsets3);
	SHADERPROP (BlurOffsets4);
	SHADERPROP (BlurOffsets5);
	SHADERPROP (BlurOffsets6);
	SHADERPROP (BlurOffsets7);

	static ColorRGBAf kBlurTable[8] = {
		// 9 tap Poisson disc
		//ColorRGBAf( 0.098484f,	 0.0951260f, 0.0f, 0.0f), // center tap, we always sample it on (0,0)
		ColorRGBAf(-0.957152f,	-0.3877980f, 0.0f, 0.0f),
		ColorRGBAf(-0.799006f,	 0.9533680f, 0.0f, 0.0f),
		ColorRGBAf( 0.940856f,	 0.7262480f, 0.0f, 0.0f),
		ColorRGBAf( 0.599230f,	-0.8810998f, 0.0f, 0.0f),
		ColorRGBAf(-0.288248f,	-0.8555254f, 0.0f, 0.0f),
		ColorRGBAf( 0.038728f,	 0.8900720f, 0.0f, 0.0f),
		ColorRGBAf( 0.954100f,	-0.1302840f, 0.0f, 0.0f),
		ColorRGBAf(-0.455428f,	 0.3171780f, 0.0f, 0.0f),
	};

	float kBlurRadius = fWidth * (1.f / 640.0f) * fHeight * (1.0f / 480.0f);
	kBlurRadius = blurWidth * clamp(kBlurRadius, 1.0f, 2.0f);

	ColorRGBAf multiplier(
						  kBlurRadius * screenShadowMap->GetTexelSizeX(),
						  kBlurRadius * screenShadowMap->GetTexelSizeY(),
						  0.0f,
						  0.0f );

	DeviceMVPMatricesState preserveMVP;

	GfxDevice& device = GetGfxDevice();
	// Clear so that tiled and multi-GPU systems don't do a RT unresolve
	float clearColor[4] = {1,0,1,0};
	device.Clear(kGfxClearColor, clearColor, 1.0f, 0);

	LoadFullScreenOrthoMatrix();
	material->SetColor( kSLPropBlurOffsets0, kBlurTable[0] * multiplier );
	material->SetColor( kSLPropBlurOffsets1, kBlurTable[1] * multiplier );
	material->SetColor( kSLPropBlurOffsets2, kBlurTable[2] * multiplier );
	material->SetColor( kSLPropBlurOffsets3, kBlurTable[3] * multiplier );
	material->SetColor( kSLPropBlurOffsets4, kBlurTable[4] * multiplier );
	material->SetColor( kSLPropBlurOffsets5, kBlurTable[5] * multiplier );
	material->SetColor( kSLPropBlurOffsets6, kBlurTable[6] * multiplier );
	material->SetColor( kSLPropBlurOffsets7, kBlurTable[7] * multiplier );
	material->SetTexture( kSLPropMainTex, screenShadowMap );
	material->SetPass( 0 );

	device.ImmediateBegin( kPrimitiveQuads );
	device.ImmediateTexCoord( 0, 0,0,0 ); device.ImmediateVertex( 0, 0, 0 );
	device.ImmediateTexCoord( 0, 1,0,0 ); device.ImmediateVertex( 1, 0, 0 );
	device.ImmediateTexCoord( 0, 1,1,0 ); device.ImmediateVertex( 1, 1, 0 );
	device.ImmediateTexCoord( 0, 0,1,0 ); device.ImmediateVertex( 0, 1, 0 );
	device.ImmediateEnd();
	GPU_TIMESTAMP();

	GetRenderBufferManager().ReleaseTempBuffer( screenShadowMap );
	return blurredShadowMap;
}

void SetCascadedShadowShaderParams (const Matrix4x4f* shadowMatrices, const float* splitDistances, const Vector4f* splitSphereCentersAndSquaredRadii)
{
	BuiltinShaderParamValues& params = GetGfxDevice().GetBuiltinParamValues();

	// Does not set first shadow matrix!
	DebugAssert (shadowMatrices);
	params.SetMatrixParam(kShaderMatWorldToShadow1, shadowMatrices[1]);
	params.SetMatrixParam(kShaderMatWorldToShadow2, shadowMatrices[2]);
	params.SetMatrixParam(kShaderMatWorldToShadow3, shadowMatrices[3]);

	DebugAssert (splitDistances);
	params.SetVectorParam(kShaderVecLightSplitsNear, Vector4f(splitDistances));
	params.SetVectorParam(kShaderVecLightSplitsFar, Vector4f(splitDistances+1));
	DebugAssert (splitSphereCentersAndSquaredRadii);
	params.SetVectorParam(kShaderVecShadowSplitSpheres0, splitSphereCentersAndSquaredRadii[0]);
	params.SetVectorParam(kShaderVecShadowSplitSpheres1, splitSphereCentersAndSquaredRadii[1]);
	params.SetVectorParam(kShaderVecShadowSplitSpheres2, splitSphereCentersAndSquaredRadii[2]);
	params.SetVectorParam(kShaderVecShadowSplitSpheres3, splitSphereCentersAndSquaredRadii[3]);
	params.SetVectorParam(kShaderVecShadowSplitSqRadii,  Vector4f(splitSphereCentersAndSquaredRadii[0].w, splitSphereCentersAndSquaredRadii[1].w, splitSphereCentersAndSquaredRadii[2].w, splitSphereCentersAndSquaredRadii[3].w));
}

#endif // ENABLE_SHADOWS
