#include "UnityPrefix.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"

#if GFX_SUPPORTS_RENDERLOOP_PREPASS
#include "RenderLoopPrivate.h"
#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Camera/BaseRenderer.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/ImageFilters.h"
#include "Runtime/Geometry/Intersection.h"
#include "External/shaderlab/Library/intshader.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "Runtime/Graphics/GraphicsHelper.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Camera/Light.h"
#include "Runtime/Camera/Shadows.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Graphics/GeneratedTextures.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/GfxDevice/BatchRendering.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "ReplacementRenderLoop.h"
#if UNITY_EDITOR
#include "Runtime/BaseClasses/Tags.h"
#endif
#include "BuiltinShaderParamUtility.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Math/SphericalHarmonics.h"
#include "Runtime/Camera/LightManager.h"
#include "External/MurmurHash/MurmurHash2.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Graphics/DrawUtil.h"

// Enable/disable hash based pre pass render loop sorting functionality.
#define ENABLE_PRE_PASS_LOOP_HASH_SORTING 0

#define SEPERATE_PREPASS_SPECULAR UNITY_XENON

PROFILER_INFORMATION(gPrepassSort, "RenderPrePass.Sort", kProfilerRender)
PROFILER_INFORMATION(gPrepassGeom, "RenderPrePass.GeometryPass", kProfilerRender)
PROFILER_INFORMATION(gPrepassLighting, "RenderPrePass.Lighting", kProfilerRender)
PROFILER_INFORMATION(gPrepassLight, "RenderPrePass.Light", kProfilerRender)
PROFILER_INFORMATION(gPrepassFinal, "RenderPrePass.FinalPass", kProfilerRender)
PROFILER_INFORMATION(gPrepassFwdDepth, "RenderPrePass.ForwardObjectsToDepth", kProfilerRender)
PROFILER_INFORMATION(gPrepassCombineDepthNormals, "RenderPrePass.CombineDepthNormals", kProfilerRender)


static SHADERPROP (LightPos);
static SHADERPROP (LightDir);
static SHADERPROP (LightColor);
static SHADERPROP (LightTexture0);
static SHADERPROP (LightBuffer);
static SHADERPROP (LightAsQuad);

// ShadowMapTexture must be in namespace or otherwise it conflicts with property in
// ForwardShaderRenderLoop.cpp in batched Android build.
namespace PrePassPrivate
{
static SHADERPROP (ShadowMapTexture);
}

#if SEPERATE_PREPASS_SPECULAR
static SHADERPROP (LightSpecBuffer);
#endif

static Material* s_LightMaterial = NULL;
static Material* s_CollectMaterial = NULL;

static ShaderKeyword kKeywordHDRLightPrepassOn = keywords::Create ("HDR_LIGHT_PREPASS_ON");

static PPtr<Mesh> s_Icosahedron = NULL;
static PPtr<Mesh> s_Icosphere = NULL;
static PPtr<Mesh> s_Pyramid = NULL;


enum {
	kLightingLayerCount = 4, // bits of stencil used for lighting layers
	
	// 3 highest bits used for excluding lights for other reasons.
	kStencilMaskSomething = (1<<7),	// any object (i.e. not background)
	kStencilMaskNonLightmapped = (1<<6), // non-lightmapped object
	kStencilMaskBeyondShadowDistace = (1<<5), // beyond shadow distance
	kStencilMaskLightBackface = (1<<4), // don't render light where it's backface passes z test
	
	// Next 4 highest bits (3 down to 0) used for lighting layers.
	kStencilBitLayerStart = 0, // start of lighting layer bits
	kStencilMaskLayers = ((1<<kLightingLayerCount)-1) << kStencilBitLayerStart,

	kStencilGeomWriteMask = kStencilMaskSomething | kStencilMaskNonLightmapped | kStencilMaskBeyondShadowDistace | kStencilMaskLayers,
};



// Lights can illuminate arbitrary layer masks. Say we have several lights:
//	La = XXXXXXXX
//	Lb = XXXXXXX-
//	Lc = XXXX-XXX
//	Ld = XXXX-X--
// Layers used for excluding lights are then:
//       ----O-OO (3 in total)
// In stencil buffer, we allocate 3 consecutive bits to handle this:
// LaS = ---
// LbS = --O
// LcS = O--
// LdS = OOO
//
// When rendering an object, set that bit if object belongs to one of light layers.
//
// When drawing a light, set stencil mask to light layer stencil mask, and stencil
// test should be equal to zero in those bits.

struct LightingLayers
{
	enum { kLayerCount = 32 };

	LightingLayers (UInt32 lightMask)
		: lightingLayerMask(lightMask)
	{
		for (int i = 0; i < kLayerCount; ++i)
			layerToStencil[i] = -1;

		int bit = kStencilBitLayerStart + kLightingLayerCount - 1;
		lightLayerCount = 0;
		UInt32 mask = 1;
		for (int i = 0; i < kLayerCount; ++i, mask<<=1)
		{
			if (lightMask & mask)
			{
				if (lightLayerCount < kLightingLayerCount)
					layerToStencil[i] = bit;
				--bit;
				++lightLayerCount;
			}
		}
	}

	UInt32 lightingLayerMask;
	int layerToStencil[kLayerCount];
	int lightLayerCount;
};

struct PrePassRenderData {
	int	roIndex;
#if ENABLE_PRE_PASS_LOOP_HASH_SORTING
	UInt32 hash;
#endif
};
typedef dynamic_array<PrePassRenderData> PreRenderPasses;


struct PrePassRenderLoop
{
	const RenderLoopContext*	m_Context;
	RenderObjectDataContainer*	m_Objects;

	#if GFX_ENABLE_DRAW_CALL_BATCHING
	BatchRenderer				m_BatchRenderer;
	#endif
	
	PreRenderPasses				m_PlainRenderPasses;

	RenderTexture* RenderBasePass (RenderTexture* rtMain, const LightingLayers& lightingLayers, RenderObjectDataContainer& outRemainingObjects, MinMaxAABB& receiverBounds);
	
	void RenderLighting (
								   ActiveLights& activeLights,
								   RenderTexture* rtMain, 
								   TextureID depthTextureID, 
								   RenderTexture* rtNormalsSpec, 
								   RenderTexture*& rtLight,
								   
#if SEPERATE_PREPASS_SPECULAR
								   RenderTexture*& rtLightSpec,
#endif
								   const Vector4f& lightFade, 
								   const LightingLayers& lightingLayers, 
								   MinMaxAABB& receiverBounds, 
								   RenderTexture** outMainShadowMap);
	
	void RenderFinalPass (RenderTexture* rtMain, 
						  RenderTexture* rtLight,
#if SEPERATE_PREPASS_SPECULAR
						  RenderTexture* rtLightSpec,
#endif 
						  bool hdr, 
						  bool linearLighting);

	struct RenderPrePassObjectSorterHash
	{
		bool operator()( const PrePassRenderData& ra, const PrePassRenderData& rb ) const;
		const PrePassRenderLoop* queue;
	};

	void SortPreRenderPassData( PreRenderPasses& passes )
	{
		RenderPrePassObjectSorterHash sorter;
		sorter.queue = this;
		std::sort( passes.begin(), passes.end(), sorter );
	}
};


struct RenderPrePassObjectSorter {
	bool operator()( const RenderObjectData& ra, const RenderObjectData& rb ) const;
};

	


bool RenderPrePassObjectSorter::operator()( const RenderObjectData& ra, const RenderObjectData& rb ) const
{
	// Sort by layering depth.
	bool globalLayeringResult;
	if (CompareGlobalLayeringData(ra.globalLayeringData, rb.globalLayeringData, globalLayeringResult))
		return globalLayeringResult;
	
	// Sort by render queues first
	if( ra.queueIndex != rb.queueIndex )
		return ra.queueIndex < rb.queueIndex;
	
	// sort by lightmap index
	if( ra.lightmapIndex != rb.lightmapIndex )
		return ra.lightmapIndex < rb.lightmapIndex;

#if GFX_ENABLE_DRAW_CALL_BATCHING
	// if part of predefined static batch, then sort by static batch index
	if( ra.staticBatchIndex != rb.staticBatchIndex )
		return ra.staticBatchIndex > rb.staticBatchIndex; // assuming that statically batched geometry occlude more - render it first
#endif

	// then sort by material (maybe better sort by shader?)
	if( ra.material != rb.material )
		return ra.material->GetInstanceID() < rb.material->GetInstanceID(); // just compare instance IDs
	
	// Sort front to back
	return ra.distance > rb.distance;
}

#if ENABLE_PRE_PASS_LOOP_HASH_SORTING
bool PrePassRenderLoop::RenderPrePassObjectSorterHash::operator()( const PrePassRenderData& ra, const PrePassRenderData& rb ) const
{
	const RenderObjectData& dataa = (*queue->m_Objects)[ra.roIndex];
	const RenderObjectData& datab = (*queue->m_Objects)[rb.roIndex];
	
	// Sort by layering depth.
	bool globalLayeringResult;
	if (CompareGlobalLayeringData(dataa.globalLayeringData, datab.globalLayeringData, globalLayeringResult))
		return globalLayeringResult;

	// Sort by render queues first
	if( dataa.queueIndex != datab.queueIndex )
		return dataa.queueIndex < datab.queueIndex;

	// sort by hash
	if( ra.hash != rb.hash )
		return ra.hash < rb.hash;

	// Sort front to back
	return dataa.distance > datab.distance;
}
#endif

static Texture* defaultSpotCookie = NULL;

static void AssignCookieToMaterial(const Light& light, Material* lightMaterial)
{
	//@TODO: when computing positions from screen space, mipmapping of cookie will really play against
	// us, when some adjacent pixels will happen to have very similar UVs. It will sample high levels which
	// will be mostly black!
	// Proper fix would be manual derivatives based on something else in the shader, but that needs SM3.0 on D3D
	// and GLSL in GL. So just use bad mip bias for now.

	Texture* cookie = light.GetCookie();

	if(cookie)
	{
		lightMaterial->SetTexture (kSLPropLightTexture0, cookie);
	}
	else if(light.GetType() == kLightSpot)
	{
		if(!defaultSpotCookie)
		{
			defaultSpotCookie = (Texture*)GetRenderSettings().GetDefaultSpotCookie();
		}
		lightMaterial->SetTexture (kSLPropLightTexture0, defaultSpotCookie);
	}
}


// To properly collect & blur directional light's screen space shadow map,
// we need to have shadow receivers that are forward-rendered in the depth buffer.
// Also, if camera needs a depth texture, forward-rendered objects should be there
// as well.
static void RenderForwardObjectsIntoDepth (
	const RenderLoopContext& ctx,
	RenderTexture* rt,
	RenderObjectDataContainer* forwardRenderedObjects,
	RenderSurfaceHandle rtColorSurface,
	RenderSurfaceHandle rtDepthSurface,
	int width, int height,
	bool cameraNeedsDepthTexture)
{
	Assert (rt);

	if (!forwardRenderedObjects || forwardRenderedObjects->size() == 0)
		return; // nothing to do
	
	PROFILER_AUTO_GFX(gPrepassFwdDepth, ctx.m_Camera);
	GPU_AUTO_SECTION(kGPUSectionOpaquePass);

	Shader* depthShader = GetCameraDepthTextureShader ();
	if (!depthShader)
		return;

	// If we do not need the depth texture, leave only the objects that will possibly receive shadows;
	// no need to render all forward objects.
	RenderObjectDataContainer forwardRenderedShadowReceivers;
	if (!cameraNeedsDepthTexture)
	{
		size_t n = forwardRenderedObjects->size();
		forwardRenderedShadowReceivers.reserve (n / 4);
		for (size_t i = 0; i < n; ++i)
		{
			RenderObjectData& roData = (*forwardRenderedObjects)[i];
			DebugAssert (roData.visibleNode);
			BaseRenderer* renderer = roData.visibleNode->renderer;
			DebugAssert (renderer);
			if (!renderer->GetReceiveShadows())
				continue; // does not receive shadows
			Shader* shader = roData.shader;
			int ss = shader->GetShaderLabShader()->GetDefaultSubshaderIndex (kRenderPathExtForward);
			if (ss == -1)
				continue; // is not forward rendered
			forwardRenderedShadowReceivers.push_back (roData);
		}

		if (forwardRenderedShadowReceivers.size() == 0)
			return; // nothing left to render
		forwardRenderedObjects = &forwardRenderedShadowReceivers;
	}

	RenderTexture::SetActive (1, &rtColorSurface, rtDepthSurface, rt);
	RenderSceneShaderReplacement (*forwardRenderedObjects, depthShader, "RenderType");
}

static RenderTexture* ComputeScreenSpaceShadowMap (
	const RenderLoopContext& ctx,
	RenderTexture* shadowMap,
	float blurWidth,
	float blurFade,
	ShadowType shadowType)
{
	Assert (shadowMap);

	GfxDevice& device = GetGfxDevice();

	if (!s_CollectMaterial)
	{
		Shader* shader = GetScriptMapper().FindShader ("Hidden/Internal-PrePassCollectShadows");
		s_CollectMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
	}

	SetShadowsKeywords (kLightDirectional, shadowType, false, false);
	RenderBufferManager& rbm = GetRenderBufferManager ();

	RenderTexture* screenShadowMap = rbm.GetTempBuffer (RenderBufferManager::kFullSize, RenderBufferManager::kFullSize, kDepthFormatNone, kRTFormatARGB32, 0, kRTReadWriteLinear);
	RenderTexture::SetActive (screenShadowMap);

	// Clear so that tiled and multi-GPU systems don't do a RT unresolve
	float clearColor[4] = {1,0,1,0};
	device.Clear(kGfxClearColor, clearColor, 1.0f, 0);

	LoadFullScreenOrthoMatrix ();
	s_CollectMaterial->SetTexture (PrePassPrivate::kSLPropShadowMapTexture, shadowMap);
	s_CollectMaterial->SetPass (0);

	Vector3f ray;
	device.ImmediateBegin (kPrimitiveQuads);

	float x1 = 0.0f;
	float x2 = 1.0f;
	float y1 = 0.0f;
	float y2 = 1.0f;
	float f = ctx.m_Camera->GetProjectionFar();

	const Transform& camtr = ctx.m_Camera->GetComponent(Transform);
	Matrix4x4f cameraWorldToLocalNoScale = camtr.GetWorldToLocalMatrixNoScale();

	device.ImmediateTexCoord (0, x1, y1, 0.0f);
	ray = cameraWorldToLocalNoScale.MultiplyPoint3(ctx.m_Camera->ViewportToWorldPoint (Vector3f(x1, y1, f)));
	device.ImmediateNormal (ray.x, ray.y, ray.z);
	device.ImmediateVertex (x1, y1, 0.1f);

	device.ImmediateTexCoord (0, x2, y1, 0.0f);
	ray = cameraWorldToLocalNoScale.MultiplyPoint3(ctx.m_Camera->ViewportToWorldPoint (Vector3f(x2, y1, f)));
	device.ImmediateNormal (ray.x, ray.y, ray.z);
	device.ImmediateVertex (x2, y1, 0.1f);

	device.ImmediateTexCoord (0, x2, y2, 0.0f);
	ray = cameraWorldToLocalNoScale.MultiplyPoint3(ctx.m_Camera->ViewportToWorldPoint (Vector3f(x2, y2, f)));
	device.ImmediateNormal (ray.x, ray.y, ray.z);
	device.ImmediateVertex (x2, y2, 0.1f);

	device.ImmediateTexCoord (0, x1, y2, 0.0f);
	ray = cameraWorldToLocalNoScale.MultiplyPoint3(ctx.m_Camera->ViewportToWorldPoint (Vector3f(x1, y2, f)));
	device.ImmediateNormal (ray.x, ray.y, ray.z);
	device.ImmediateVertex (x1, y2, 0.1f);

	device.ImmediateEnd ();
	GPU_TIMESTAMP();

	rbm.ReleaseTempBuffer (shadowMap);

	// possibly blur into another screen-space render texture
	SetShadowsKeywords (kLightDirectional, shadowType, true, true);
	if (IsSoftShadow(shadowType) && GetSoftShadowsEnabled())
		return BlurScreenShadowMap (screenShadowMap, shadowType, f, blurWidth, blurFade);

	return screenShadowMap;
}

static void RenderLightGeom (const RenderLoopContext& ctx, const ActiveLight& light, const Vector3f& lightPos, const Matrix4x4f& lightMatrix, const bool renderAsQuad)
{
	// Spot and point lights: render as tight geometry. If it doesn't intersect near or far, stencil optimisation will be used
	// (rendering the z tested back faces into stencil and then front faces will only pass for these pixels).
	// If it intersects near, back faces with z test greater will be rendered (shouldn't use that when not intersecting near, because
	// then there could be objects between the cam and the light, not touching the light).
	// If it intersects far, render front faces without any gimmicks.
	// If it intersects both near and far, render as a quad.

	GfxDevice& device = GetGfxDevice();
	Light& l = *light.light;
	float r = l.GetRange();
	float n = ctx.m_Camera->GetProjectionNear() * 1.001f;

	if (l.GetType() == kLightPoint && !renderAsQuad)
	{
		#if GFX_USE_SPHERE_FOR_POINT_LIGHT
			ChannelAssigns ch;
			ch.Bind (kShaderChannelVertex, kVertexCompVertex);

			// Older content might have included/overriden old Internal-PrePassLighting.shader,
			// which relied on normals being zeros here. Light .fbx files have zero normals just for that.
			if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1))
				ch.Bind (kShaderChannelNormal, kVertexCompNormal);

			Matrix4x4f m;
			m.SetTranslate (lightPos);
			m.Get (0, 0) = r;
			m.Get (1, 1) = r;
			m.Get (2, 2) = r;
			// Point lights bigger than 0.25 of the screen height can be rendered with high-poly, but tighter geometry.
			DrawUtil::DrawMesh (ch, light.screenRect.height > 0.25f ? *s_Icosphere : *s_Icosahedron, m, -1);
		#else
			// PS3 is not the best at vertex processing, so stick to low-poly meshes
			device.ImmediateShape(lightPos.x, lightPos.y, lightPos.z, r, GfxDevice::kShapeCube);
		#endif
	}
	else if (l.GetType() == kLightSpot && !renderAsQuad)
	{
		Matrix4x4f m (lightMatrix);
		ChannelAssigns ch;
		ch.Bind (kShaderChannelVertex, kVertexCompVertex);
		if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1))
			ch.Bind (kShaderChannelNormal, kVertexCompNormal);
		float sideLength = r / l.GetCotanHalfSpotAngle ();
		m.Scale (Vector3f(sideLength, sideLength, r));
		DrawUtil::DrawMesh (ch, *s_Pyramid, m, -1);
	}
	else // Directional light or spot/point that needs to be rendered as a quad
	{ 
		DeviceViewProjMatricesState preserveViewProj;

		const Camera* camera = ctx.m_Camera;
		float nearPlane = 0;

		float x1 = light.screenRect.x;
		float x2 = light.screenRect.x + light.screenRect.width;
		float y1 = light.screenRect.y;
		float y2 = light.screenRect.y + light.screenRect.height;

		// Calculate rays pointing from the camera to the near plane's corners in camera space
		Vector3f ray1 = camera->ViewportToCameraPoint (Vector3f(x1, y1, n));
		Vector3f ray2 = camera->ViewportToCameraPoint (Vector3f(x1, y2, n));
		Vector3f ray3 = camera->ViewportToCameraPoint (Vector3f(x2, y2, n));
		Vector3f ray4 = camera->ViewportToCameraPoint (Vector3f(x2, y1, n));

		// Set up orthographic projection not to have to deal with precision problems
		// that show up when drawing a full screen quad in perspective projection in world space.
		LoadFullScreenOrthoMatrix (nearPlane, camera->GetProjectionFar(), true);

		// Draw the fullscreen quad on the near plane
		device.ImmediateBegin (kPrimitiveQuads);

		device.ImmediateNormal (ray1.x, ray1.y, ray1.z);
		device.ImmediateVertex (x1, y1, nearPlane);

		device.ImmediateNormal (ray2.x, ray2.y, ray2.z);
		device.ImmediateVertex (x1, y2, nearPlane);

		device.ImmediateNormal (ray3.x, ray3.y, ray3.z);
		device.ImmediateVertex (x2, y2, nearPlane);

		device.ImmediateNormal (ray4.x, ray4.y, ray4.z);
		device.ImmediateVertex (x2, y1, nearPlane);

		device.ImmediateEnd ();
		GPU_TIMESTAMP();
	}
}

static UInt32 LightMask (const Light& l, const LightingLayers& lightingLayers)
{
	UInt32 mask = 0U;
	UInt32 lightExcludeLayers = ~l.GetCullingMask();
	int bit = 0;
	while (lightExcludeLayers)
	{
		if (lightExcludeLayers & 1)
		{
			int layerStencilBit = lightingLayers.layerToStencil[bit];
			if (layerStencilBit != -1)
				mask |= 1 << layerStencilBit;
		}
		lightExcludeLayers >>= 1;
		++bit;
	}
	return mask;
}

static RenderTexture* RenderLight (
						 const RenderLoopContext& ctx,
						 const ShadowCullData& shadowCullData,
						 QualitySettings::ShadowQuality shadowQuality,
						 const LightmapSettings::LightmapsMode lightmapsMode,
						 RenderTexture*& rtLight,
						 RenderTexture* rtMain,
						 int width, int height,
						 DeviceStencilState* devStDisabled,
						 const MinMaxAABB& receiverBounds,
						 const DeviceMVPMatricesState& mvpState,
						 const Vector4f& lightFade,
						 const LightingLayers& lightingLayers,
						 const ActiveLight& light,
#if SEPERATE_PREPASS_SPECULAR
						 bool specularPass,
#endif
						 bool returnShadowMap)
{
	Light& l = *light.light;

	PROFILER_AUTO_GFX(gPrepassLight, &l);

	const Light::Lightmapping lightmappingMode = l.GetLightmappingForRender();
	const Transform& trans = l.GetComponent(Transform);
	Matrix4x4f lightMatrix = trans.GetLocalToWorldMatrixNoScale();
	Vector3f lightPos = lightMatrix.GetPosition();

	Assert(light.isVisibleInPrepass);
	Assert(!light.screenRect.IsEmpty());

	ShadowType lightShadows = l.GetShadows();
	// Shadows on local lights are Pro only
	if (lightShadows != kShadowNone && l.GetType() != kLightDirectional &&
		!GetBuildSettings().hasLocalLightShadows)
		lightShadows = kShadowNone;

	// Check if soft shadows are allowed by license, quality settings etc.
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a1) &&
		lightShadows > kShadowHard && !GetSoftShadowsEnabled())
		lightShadows = kShadowHard;

	GfxDevice& device = GetGfxDevice();
	BuiltinShaderParamValues& params = device.GetBuiltinParamValues();

	RenderSurfaceHandle rtSurfaceColor;
	RenderSurfaceHandle rtSurfaceDepth = rtMain->GetDepthSurfaceHandle(); // re-use depth from final target
	RenderSurfaceHandle rtSurfaceMainColor = rtMain->GetColorSurfaceHandle(); // will allocate color later (if any lights will actually be present)

	bool hdr = ctx.m_Camera->GetUsingHDR();
	float white[] = {1,1,1,1};
	float black[] = {0,0,0,0};
	UInt32 rtFlags = RenderTexture::kFlagDontRestoreColor;
	
	if (!rtLight)
	{
		rtLight = GetRenderBufferManager().GetTempBuffer (RenderBufferManager::kFullSize, RenderBufferManager::kFullSize, kDepthFormatNone, hdr ? GetGfxDevice().GetDefaultHDRRTFormat() : kRTFormatARGB32, 0, kRTReadWriteLinear);

		if (!rtLight->IsCreated())
			rtLight->Create();
		
		rtLight->SetFilterMode (kTexFilterNearest);
		rtSurfaceColor = rtLight->GetColorSurfaceHandle();


		RenderTexture::SetActive (1, &rtSurfaceColor, rtSurfaceDepth, rtLight, 0, kCubeFaceUnknown, rtFlags);
		GraphicsHelper::Clear(kGfxClearColor, hdr ? black : white, 1.0f, 0);
		GPU_TIMESTAMP();
	}
	
	rtSurfaceColor = rtLight->GetColorSurfaceHandle();

	l.SetLightKeyword();

	Vector3f lightDir = lightMatrix.GetAxisZ();
	ColorRGBAf lightCol = GammaToActiveColorSpace (l.GetColor()) * l.GetIntensity() * 2.0f;

	Matrix4x4f temp1, temp2, temp3;
	if (l.GetType() == kLightSpot)
	{
		Matrix4x4f worldToLight = l.GetWorldToLocalMatrix();
		{
			temp1.SetScale (Vector3f (-.5f, -.5f, 1.0f));
			temp2.SetTranslate (Vector3f (.5f, .5f, 0.0f));
			temp3.SetPerspectiveCotan( l.GetCotanHalfSpotAngle(), 0.0f, l.GetRange() );
			// temp2 * temp3 * temp1 * worldToLight
			Matrix4x4f temp4;
			MultiplyMatrices4x4 (&temp2, &temp3, &temp4);
			MultiplyMatrices4x4 (&temp4, &temp1, &temp2);
			MultiplyMatrices4x4 (&temp2, &worldToLight, &params.GetWritableMatrixParam(kShaderMatLightMatrix));
		}
	}
	else if (l.GetCookie())
	{
		if (l.GetType() == kLightPoint)
		{
			params.SetMatrixParam(kShaderMatLightMatrix, l.GetWorldToLocalMatrix());
		}
		else if (l.GetType() == kLightDirectional)
		{
			float scale = 1.0f / l.GetCookieSize();
			temp1.SetScale (Vector3f (scale, scale, 0));
			temp2.SetTranslate (Vector3f (.5f, .5f, 0));
			// temp2 * temp1 * l.GetWorldToLocalMatrix()
			MultiplyMatrices4x4 (&temp2, &temp1, &temp3);
			MultiplyMatrices4x4 (&temp3, &l.GetWorldToLocalMatrix(), &params.GetWritableMatrixParam(kShaderMatLightMatrix));
		}
	}

	AssignCookieToMaterial(l, s_LightMaterial);

	const bool renderAsQuad = light.intersectsNear && light.intersectsFar || l.GetType() == kLightDirectional;
	ShaderLab::g_GlobalProperties->SetFloat(kSLPropLightAsQuad, renderAsQuad ? 1.0f : 0.0f);
	ShaderLab::g_GlobalProperties->SetVector (kSLPropLightPos, lightPos.x, lightPos.y, lightPos.z, 1.0f / (l.GetRange() * l.GetRange()));
	ShaderLab::g_GlobalProperties->SetVector (kSLPropLightDir, lightDir.x, lightDir.y, lightDir.z, 0.0f);
	ShaderLab::g_GlobalProperties->SetVector (kSLPropLightColor, lightCol.GetPtr());
	///@TODO: cleanup, remove this from Internal-PrePassLighting shader
	s_LightMaterial->SetTexture (ShaderLab::Property("_LightTextureB0"), builtintex::GetAttenuationTexture());

	RenderTexture* shadowMap = NULL;
	ShadowCameraData camData(shadowCullData);


	if (light.shadowedLight != NULL && receiverBounds.IsValid() && shadowQuality != QualitySettings::kShadowsDisable)
	{
		Assert(light.insideShadowRange);

		ShadowType lightShadows = l.GetShadows();

		if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a1))
		{
			if (shadowQuality == QualitySettings::kShadowsHardOnly && lightShadows != kShadowNone)
				lightShadows = kShadowHard;
		}

		SetShadowsKeywords (l.GetType(), lightShadows, false, true);

		Matrix4x4f shadowMatrices[kMaxShadowCascades];
		device.SetViewMatrix (ctx.m_CurCameraMatrix.GetPtr());
		device.SetStencilState (devStDisabled, 0);

		// Rendering shadowmaps will switch away from the lighting buffer and then will switch back.
		// Nothing we can do about it, so don't produce the warning.
		device.IgnoreNextUnresolveOnCurrentRenderTarget();

		shadowMap = RenderShadowMaps (camData, light, receiverBounds, false, shadowMatrices);
		
		if (!shadowMap)
		{
			// If shadow map could not actually be created (no casters, out of VRAM, whatever),
			// set the no shadows keywords and proceed. So there will be no shadows,
			// but otherwise it will be ok.
			SetNoShadowsKeywords();
		}
		else
		{
			Vector4f data;
			
			// ambient & shadow fade out
			data.x = 1.0f - l.GetShadowStrength(); // R = 1-strength
			data.y = data.z = data.w = 0.0f;
			params.SetVectorParam(kShaderVecLightShadowData, data);
			
			if (l.GetType() == kLightDirectional)
			{
				params.SetMatrixParam(kShaderMatWorldToShadow, shadowMatrices[0]);
				SetCascadedShadowShaderParams (shadowMatrices, camData.splitDistances, camData.splitSphereCentersAndSquaredRadii);

				shadowMap = ComputeScreenSpaceShadowMap (
					ctx,
					shadowMap,
					l.GetShadowSoftness(),
					l.GetShadowSoftnessFade(),
					lightShadows);
			}
			else if (l.GetType() == kLightSpot)
			{
				params.SetMatrixParam(kShaderMatWorldToShadow, shadowMatrices[0]);
			}

			// texel offsets for PCF
			float offX = 0.5f / shadowMap->GetGLWidth();
			float offY = 0.5f / shadowMap->GetGLHeight();
			data.z = 0.0f; data.w = 0.0f;
			data.x = -offX; data.y = -offY; params.SetVectorParam(kShaderVecShadowOffset0, data);
			data.x =  offX; data.y = -offY; params.SetVectorParam(kShaderVecShadowOffset1, data);
			data.x = -offX; data.y =  offY; params.SetVectorParam(kShaderVecShadowOffset2, data);
			data.x =  offX; data.y =  offY; params.SetVectorParam(kShaderVecShadowOffset3, data);
			s_LightMaterial->SetTexture (PrePassPrivate::kSLPropShadowMapTexture, shadowMap);

			if (rtLight != NULL)
				RenderTexture::SetActive (1, &rtSurfaceColor, rtSurfaceDepth, rtLight);
			else
				RenderTexture::SetActive (1, &rtSurfaceMainColor, rtSurfaceDepth, rtMain);
		}
		device.SetViewMatrix(mvpState.GetView().GetPtr());
		device.SetProjectionMatrix(mvpState.GetProj());
		SetClippingPlaneShaderProps();

		// restore the cull mode, since it could be changed by a shadow caster with odd-negative scale
		device.SetNormalizationBackface( kNormalizationDisabled, false );
	}
	else
	{
		SetNoShadowsKeywords ();
	}
	
	// Draw each light in two passes: illuminate non-lightmapped objects; illuminate lightmapped objects.
	int lightPassCount = 2;
	int lightPassAddBits[2] = { kStencilMaskNonLightmapped, 0 };
	if (lightmappingMode == Light::kLightmappingRealtimeOnly)
	{
		// If light is realtime only, it's enough to draw one pass; that illuminates any object
		// be it lightmapped or not.
		lightPassCount = 1;
		lightPassAddBits[0] = 0;
	}
	else if (lightmappingMode == Light::kLightmappingAuto && lightmapsMode != LightmapSettings::kDualLightmapsMode)
	{
		// If it's an auto light but we're in single lightmaps mode, draw in one pass only to illuminate
		// non-lightmapped objects
		// TODO: realtime shadows from auto lights won't be received by lightmapped objects. Do we want to fix it?
		lightPassCount = 1;
		lightPassAddBits[0] = kStencilMaskNonLightmapped;
	}

	//TODO: skip if smaller than certain size
	const bool useStencilMask = !light.intersectsNear && !light.intersectsFar &&
		(lightmappingMode == Light::kLightmappingRealtimeOnly) &&
		(l.GetType() == kLightSpot || l.GetType() == kLightPoint );

	const UInt32 lightmask = LightMask (l, lightingLayers);

	// Render stencil mask, to discard all light pixels, at which the light is fully in front of scene geometry.
	if (useStencilMask)
	{
		Material::GetDefault ()->SetPass (0);
		#if UNITY_XENON
		device.SetNullPixelShader ();
		#endif

		GfxBlendState blendstate;
		blendstate.renderTargetWriteMask = 0U;
		device.SetBlendState (device.CreateBlendState(blendstate), 0);

		GfxRasterState rasterstate;
		rasterstate.cullMode = kCullOff;
		device.SetRasterState (device.CreateRasterState(rasterstate));

		GfxDepthState depthstate;
		depthstate.depthWrite = false;
		depthstate.depthFunc = kFuncLEqual;
		device.SetDepthState (device.CreateDepthState(depthstate));

		GfxStencilState lightStencil;
		lightStencil.stencilEnable = true;
		lightStencil.readMask = 0xFFU;
		lightStencil.writeMask = kStencilMaskLightBackface;
		lightStencil.stencilZFailOpBack = kStencilOpInvert;
		lightStencil.stencilZFailOpFront = kStencilOpInvert;
		lightStencil.stencilPassOpBack = kStencilOpKeep;
		lightStencil.stencilPassOpFront = kStencilOpKeep;
		lightStencil.stencilFuncBack = (lightmask != 0 ) ? kFuncNotEqual : kFuncAlways;
		lightStencil.stencilFuncFront = (lightmask != 0 ) ? kFuncNotEqual : kFuncAlways;
		device.SetStencilState (device.CreateStencilState(lightStencil), lightmask|kStencilMaskSomething|kStencilMaskNonLightmapped);

		#if UNITY_XENON
		// Clear within light-geom, sets all HiS to cull.
		// Set to cull where equal to background (to deal with lightmasks), unoptimal but works
		if (useStencilMask)
			device.SetHiStencilState (false, true, kStencilMaskSomething|kStencilMaskNonLightmapped, kFuncEqual);
		#endif

		RenderLightGeom (ctx, light, lightPos, lightMatrix, renderAsQuad);

		blendstate.renderTargetWriteMask = KColorWriteAll;
		device.SetBlendState (device.CreateBlendState(blendstate), 0);

		#if UNITY_XENON
		device.HiStencilFlush (kHiSflush_sync);
		#endif
	}

	for (int pp = 0; pp < lightPassCount; ++pp)
	{
		Vector4f lightingFade = lightFade;
		Vector4f shadowFade = lightFade;
		shadowFade.x = 1.0f - l.GetShadowStrength();
		if (pp == 0 || lightmappingMode == Light::kLightmappingRealtimeOnly)
			lightingFade.z = lightingFade.w = 0.0f;
		else
			shadowFade.z = shadowFade.w = 0.0f;
		params.SetVectorParam(kShaderVecLightmapFade, lightingFade);
		params.SetVectorParam(kShaderVecLightShadowData, shadowFade);

		// Disable mipmapping on light cookies
		ShaderLab::TexEnv* cookieEnv = s_LightMaterial->GetProperties().GetTexEnv(kSLPropLightTexture0);
		if (cookieEnv)
		{
			cookieEnv->TextureMipBiasChanged (-8);
		}

		#if SEPERATE_PREPASS_SPECULAR
		if (s_LightMaterial->GetPassCount () > 2 && ctx.m_Camera->GetUsingHDR() && specularPass)
			s_LightMaterial->SetPass (2);
		else
		#endif
		if (s_LightMaterial->GetPassCount () > 1 && ctx.m_Camera->GetUsingHDR())
			s_LightMaterial->SetPass (1);
		else 
			s_LightMaterial->SetPass (0);

		// Construct stencil read mask
		GfxStencilState stencil;
		stencil.stencilEnable = true;
		stencil.stencilFuncFront = stencil.stencilFuncBack = kFuncEqual;
		stencil.readMask = kStencilMaskSomething;
		// Check lightmapped vs. non-lightmapped unless it's a realtime light that
		// only cares about not illuminating non-something.
		if (lightmappingMode != Light::kLightmappingRealtimeOnly)
			stencil.readMask |= kStencilMaskNonLightmapped;

		if (pp != 0 && lightmappingMode != Light::kLightmappingRealtimeOnly)
			stencil.readMask |= kStencilMaskBeyondShadowDistace;

		stencil.readMask |= lightmask;
		int stencilRef = kStencilMaskSomething + lightPassAddBits[pp];

		if (useStencilMask)
		{
			// Clear stencil while rendering
			stencil.writeMask = kStencilMaskLightBackface;
			stencil.stencilZFailOpBack = kStencilOpZero;
			stencil.stencilZFailOpFront = kStencilOpZero;
			stencil.stencilPassOpBack = kStencilOpZero;
			stencil.stencilPassOpFront = kStencilOpZero;
			// Clear the kStencilMaskLightBackface bit even if rejecting pixel due to stencil layer mask
			stencil.stencilFailOpBack = kStencilOpZero;
			stencil.stencilFailOpFront = kStencilOpZero;

			stencil.readMask |= kStencilMaskLightBackface;
			stencilRef |= kStencilMaskLightBackface;
		}

		DeviceStencilState* devStCheck = device.CreateStencilState (stencil);
		device.SetStencilState (devStCheck, stencilRef);

		#if UNITY_XENON
		// Set to cull when all == background (to deal with lightmasks), unoptimal but works
		if (useStencilMask)
			device.SetHiStencilState (true, true, kStencilMaskSomething|kStencilMaskNonLightmapped, kFuncEqual);
		#endif

		// Draw light shape
		GfxRasterState rasterstate;
		GfxDepthState depthstate;
		depthstate.depthWrite = false;
		if (light.intersectsNear && !light.intersectsFar && (l.GetType() == kLightSpot || l.GetType() == kLightPoint))
		{
			// When near (but not far) plane intersects the light, render back faces (tighter than rendering a bounding quad).
			// Can't use this when not intersecting, since it would waste processing for objects between
			// the light and the cam, even when they don't touch the light.
			rasterstate.cullMode = kCullFront;
			depthstate.depthFunc = kFuncGreater;
		}
		else
		{
			depthstate.depthFunc = kFuncLEqual;
			#if UNITY_XENON
			device.SetHiZEnable (kHiZEnable);
			#endif
		}
		device.SetRasterState (device.CreateRasterState (rasterstate));
		device.SetDepthState (device.CreateDepthState (depthstate));

		RenderLightGeom (ctx, light, lightPos, lightMatrix, renderAsQuad);

		#if UNITY_XENON
		device.SetHiZEnable (kHiZAuto);
		if (useStencilMask) 
			device.HiStencilFlush (kHiSflush_async);
		#endif
	}

	if (shadowMap && !returnShadowMap)
		GetRenderBufferManager().ReleaseTempBuffer (shadowMap);

	return returnShadowMap ? shadowMap : NULL;
}


void PrePassRenderLoop::RenderLighting (
									  ActiveLights& activeLights,
									  RenderTexture* rtMain,
									  TextureID depthTextureID,
									  RenderTexture* rtNormalsSpec,
									  RenderTexture*& rtLight,
										
#if SEPERATE_PREPASS_SPECULAR
									  RenderTexture*& rtLightSpec,
#endif
									  const Vector4f& lightFade,
									  const LightingLayers& lightingLayers,
									  MinMaxAABB& receiverBounds,
									  RenderTexture** outMainShadowMap)
{
	PROFILER_AUTO_GFX(gPrepassLighting, m_Context->m_Camera);
	GPU_AUTO_SECTION(kGPUSectionDeferedLighting);
	*outMainShadowMap = NULL;

	Assert(rtLight == NULL);
#if SEPERATE_PREPASS_SPECULAR
	Assert(rtLightSpec == NULL);
#endif
	const QualitySettings::ShadowQuality shadowQuality = static_cast<QualitySettings::ShadowQuality>(GetQualitySettings().GetCurrent().shadows);
	const LightmapSettings::LightmapsMode lightmapsMode = static_cast<LightmapSettings::LightmapsMode>(GetLightmapSettings().GetLightmapsMode());

	ShadowCameraData camData(*m_Context->m_ShadowCullData);

	// Prevent receiver bounds to be zero size in any dimension;
	// causes trouble with calculating intersection of frustum and bounds.
	receiverBounds.Expand( 0.01f );	

	const Rectf screenRect = m_Context->m_Camera->GetScreenViewportRect();

	if (!s_LightMaterial) {
		Shader* shader = GetScriptMapper().FindShader ("Hidden/Internal-PrePassLighting");
		s_LightMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
	}

	if (s_Icosahedron.IsNull ())
		s_Icosahedron = GetBuiltinResource<Mesh> ("icosahedron.fbx");
	if (s_Icosphere.IsNull ())
		s_Icosphere = GetBuiltinResource<Mesh> ("icosphere.fbx");
	if (s_Pyramid.IsNull ())
		s_Pyramid = GetBuiltinResource<Mesh> ("pyramid.fbx");
	
	static SHADERPROP (CameraDepthTexture);
	static SHADERPROP (CameraNormalsTexture);
	const int width = rtNormalsSpec->GetGLWidth();
	const int height = rtNormalsSpec->GetGLHeight();
	if (gGraphicsCaps.hasStencilInDepthTexture)
	{
		ShaderLab::g_GlobalProperties->SetRectTextureID (
														 kSLPropCameraDepthTexture,
														 depthTextureID,
														 width,
														 height,
														 rtMain->GetTexelSizeX(),
														 rtMain->GetTexelSizeY(),
														 rtMain->GetUVScaleX(),
														 rtMain->GetUVScaleY()
														 );
	}
	
	// set as _CameraNormalsTexture for external access
	ShaderLab::g_GlobalProperties->SetTexture (kSLPropCameraNormalsTexture, rtNormalsSpec);

	GfxDevice& device = GetGfxDevice();

	SetAndRestoreWireframeMode setWireframeOff(false); // turn off wireframe; will restore old value in destructor
	device.SetNormalizationBackface( kNormalizationDisabled, false );

	DeviceStencilState* devStDisabled = device.CreateStencilState (GfxStencilState());


	DeviceMVPMatricesState preserveMVP;

	device.SetWorldMatrix (Matrix4x4f::identity.GetPtr());

	RenderTexture** currentLightTex = &rtLight;
#if SEPERATE_PREPASS_SPECULAR
	//Do 2 passes for HDR prepass lighting on xenon
	for (int lp = 0; lp < (m_Context->m_Camera->GetUsingHDR() ? 2 : 1); ++lp)
	{
		if (lp == 0)
			currentLightTex = &rtLight;
		else
			currentLightTex = &rtLightSpec;
#endif
	
	const ActiveLight* mainActiveLight = GetMainActiveLight(activeLights);
	ActiveLights::Array::iterator it, itEnd = activeLights.lights.end();
	for (it = activeLights.lights.begin(); it != itEnd; ++it)
	{
		if (!it->isVisibleInPrepass)
			continue;
		if (&*it == mainActiveLight)
		{
			// skip main light now; will render it last
			continue;
		}
		RenderLight (*m_Context, camData, shadowQuality, lightmapsMode,
			*currentLightTex,
			rtMain,
			width, height, devStDisabled, receiverBounds,
			preserveMVP, lightFade, lightingLayers, *it,
#if SEPERATE_PREPASS_SPECULAR
			lp == 1, 
#endif
			false);
	}

	#if UNITY_XENON
	device.SetStencilState (devStDisabled, 0);
	device.SetHiStencilState (false, false, 0, kFuncEqual);
	#endif

	// render main light
	if (mainActiveLight)
	{
		RenderTexture* shadowMap = RenderLight (
			*m_Context, camData, shadowQuality, lightmapsMode,
			*currentLightTex,
			rtMain,
			width, height, devStDisabled, receiverBounds,
			preserveMVP, lightFade, lightingLayers, *mainActiveLight,
#if SEPERATE_PREPASS_SPECULAR
			lp == 1, 
#endif
			true);
		if (shadowMap)
		{
			AddRenderLoopTempBuffer (m_Context->m_RenderLoop, shadowMap);
			*outMainShadowMap = shadowMap;
		}
	}
#if SEPERATE_PREPASS_SPECULAR
	}
#endif
	SetNoShadowsKeywords ();

	Vector4f lightmapFade = lightFade;
	// if we're not in dual lightmaps mode, always use the far lightmap, i.e. lightmapFade = 1
	if (GetLightmapSettings().GetLightmapsMode() != LightmapSettings::kDualLightmapsMode)
		lightmapFade.z = lightmapFade.w = 1.0f;

	device.GetBuiltinParamValues().SetVectorParam(kShaderVecLightmapFade, lightmapFade);

	device.SetStencilState (devStDisabled, 0);
	
	#if !UNITY_XENON
	// Ok, we didn't really have any lights worth rendering.
	// Create a small render texture and clear it to white and pass it as the lighting buffer.
	// Don't do that on 360; pointless and saves a resolve.
	if (!rtLight)
	{
		rtLight = GetRenderBufferManager().GetTempBuffer (16, 16, kDepthFormatNone, kRTFormatARGB32, 0, kRTReadWriteLinear);
		RenderTexture::SetActive (rtLight);
		float white[] = {1,1,1,1};
		float black[] = {0,0,0,0};
		GraphicsHelper::Clear (kGfxClearColor, m_Context->m_Camera->GetUsingHDR() ? black : white, 1.0f, 0);
		GPU_TIMESTAMP();

		// We just switched away from a Z buffer (only in case when no lights were there!),
		// and we'll switch back to it. So ignore the unresolve warning on it.
		device.IgnoreNextUnresolveOnRS(rtMain->GetDepthSurfaceHandle());
	}
	#endif
}


static RenderTexture* CombineDepthNormalsTexture (const RenderLoopContext& ctx, RenderObjectDataContainer& remainingObjects)
{
	PROFILER_AUTO_GFX(gPrepassCombineDepthNormals, ctx.m_Camera);
	
	static Material* s_CombineMaterial = NULL;
	if (!s_CombineMaterial)
	{
		Shader* shader = GetScriptMapper ().FindShader ("Hidden/Internal-CombineDepthNormals");
		if (shader)
			s_CombineMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
		if (!s_CombineMaterial) {
			AssertString ("Coult not find depth+normals combine shader");
			return NULL;
		}
	}
	
	RenderTexture* depthNormals = GetRenderBufferManager().GetTempBuffer (RenderBufferManager::kFullSize, RenderBufferManager::kFullSize, kDepthFormatNone, kRTFormatARGB32, 0, kRTReadWriteLinear);
	RenderTexture::SetActive (depthNormals);
	GraphicsHelper::Clear (kGfxClearColor, ColorRGBAf(0.5f,0.5f,1.0f,1.0f).GetPtr(), 1.0f, 0);
	GPU_TIMESTAMP();
	
	// Combine depth & normals into single texture
	ImageFilters::Blit (NULL, depthNormals, s_CombineMaterial, 0, false);

	AddRenderLoopTempBuffer (ctx.m_RenderLoop, depthNormals);
	
	static SHADERPROP (CameraDepthNormalsTexture);
	ShaderLab::g_GlobalProperties->SetTexture (kSLPropCameraDepthNormalsTexture, depthNormals);
	
	return depthNormals;
}



// Separate pass to render depth into a separate target. Only used on Macs with Radeon HDs, since
// only there doing it the regular way is broken.
#if GFX_SUPPORTS_OPENGL
static RenderTexture* RenderBasePassDepth (const RenderLoopContext& ctx, RenderObjectDataContainer& renderData, PreRenderPasses& plainRenderPasses)
{
	GPU_AUTO_SECTION(kGPUSectionDeferedPrePass);

	GfxDevice& device = GetGfxDevice();
	
	RenderTexture* rt = GetRenderBufferManager().GetTempBuffer (RenderBufferManager::kFullSize, RenderBufferManager::kFullSize, kDepthFormat24, kRTFormatDepth, 0, kRTReadWriteLinear);
	rt->SetFilterMode (kTexFilterNearest);
	if (!rt->IsCreated())
		rt->Create();
	RenderTexture::SetActive (rt);	
	AddRenderLoopTempBuffer (ctx.m_RenderLoop, rt);
	
	float black[] = {0,0,0,0};
	GraphicsHelper::Clear (kGfxClearAll, black, 1.0f, 0);
	GPU_TIMESTAMP();
	
	device.SetViewMatrix (ctx.m_CurCameraMatrix.GetPtr());
	
	size_t ndata = renderData.size();
	
	for( size_t i = 0; i < ndata; ++i )
	{
		const PrePassRenderData& rpData = plainRenderPasses[i];
		const RenderObjectData& roData = renderData[rpData.roIndex];
		Shader* shader = roData.shader;
		int ss = shader->GetShaderLabShader()->GetDefaultSubshaderIndex(kRenderPathExtPrePass);
		if (ss == -1)
			continue;
		
		const VisibleNode *node = roData.visibleNode;
		
		SetObjectScale (device, node->lodFade, node->invScale);

		//@TODO: if this returns true and we have any sort of batching, we'd have to break batches here
		node->renderer->ApplyCustomProperties(*roData.material, shader, ss);		
		
		ShaderLab::SubShader& subshader = roData.shader->GetShaderLabShader()->GetSubShader (ss);
		int shaderPassCount = subshader.GetValidPassCount();
		for (int p = 0; p < shaderPassCount; ++p)
		{
			ShaderPassType passType;
			UInt32 passRenderOptions;
			subshader.GetPass(p)->GetPassOptions (passType, passRenderOptions);
			if (passType != kPassLightPrePassBase)
				continue;
			
			const ChannelAssigns* channels = roData.material->SetPassWithShader(p, shader, ss);
			if (channels)
			{
				SetupObjectMatrix (node->worldMatrix, node->transformType);
				node->renderer->Render( roData.subsetIndex, *channels );
			}
		}
	}
	
	return rt;
}
#endif

inline float MultiplyAbsVectorZ (const Matrix4x4f& m, const Vector3f& v)
{
	return Abs(m.m_Data[2]) * v.x + Abs(m.m_Data[6]) * v.y + Abs(m.m_Data[10]) * v.z;
}


RenderTexture* PrePassRenderLoop::RenderBasePass (
									  RenderTexture* rtMain,
									  const LightingLayers& lightingLayers,
									  RenderObjectDataContainer& outRemainingObjects,
									  MinMaxAABB& receiverBounds
									 )
{
	PROFILER_AUTO_GFX(gPrepassGeom, m_Context->m_Camera);
	GPU_AUTO_SECTION(kGPUSectionDeferedPrePass);

	const float shadowDistance = m_Context->m_ShadowCullData->shadowDistance;

	GfxDevice& device = GetGfxDevice();
	device.SetNormalizationBackface( kNormalizationDisabled, false );

	GfxStencilState stRender;
	stRender.writeMask = kStencilGeomWriteMask;
	stRender.stencilEnable = true;
	stRender.stencilPassOpFront = stRender.stencilPassOpBack = kStencilOpReplace;
	DeviceStencilState* devStRender = device.CreateStencilState (stRender);
	
	RenderTexture* rtNormalsSpec = GetRenderBufferManager().GetTempBuffer (RenderBufferManager::kFullSize, RenderBufferManager::kFullSize, kDepthFormatNone, kRTFormatARGB32, 0, kRTReadWriteLinear);
	rtNormalsSpec->SetFilterMode (kTexFilterNearest);
	if (!rtNormalsSpec->IsCreated())
		rtNormalsSpec->Create();
	RenderSurfaceHandle rtSurfaceColor = rtNormalsSpec->GetColorSurfaceHandle();
	RenderSurfaceHandle rtSurfaceDepth = rtMain->GetDepthSurfaceHandle(); // reuse depth buffer from final pass

	UInt32 rtFlags = RenderTexture::kFlagDontRestoreColor;
	UInt32 gfxClearFlags = kGfxClearAll;
	// do not clear depth/stencil if camera set to DontClear
	if (m_Context->m_Camera->GetClearFlags() == Camera::kDontClear)
	{
		gfxClearFlags &= ~kGfxClearDepthStencil;
	}
	else
	{
		rtFlags |= RenderTexture::kFlagDontRestoreDepth;
	}

	// set base pass render texture
	RenderTexture::SetActive (1, &rtSurfaceColor, rtSurfaceDepth, rtNormalsSpec, 0, kCubeFaceUnknown, rtFlags);

	AddRenderLoopTempBuffer (m_Context->m_RenderLoop, rtNormalsSpec);

	float black[] = {0,0,0,0};
	GraphicsHelper::Clear (gfxClearFlags, black, 1.0f, 0);
	GPU_TIMESTAMP();

	device.SetViewMatrix (m_Context->m_CurCameraMatrix.GetPtr());

	const ChannelAssigns* channels = NULL;

	#if GFX_ENABLE_DRAW_CALL_BATCHING

	int prevTransformType = -1;
	Material* prevMaterial = 0;
	Shader* prevShader = 0;
	int prevSubshaderIndex = -1;
	float prevInvScale = 0.0f;
	float prevLodFade = 0.0f;
	UInt32 prevCustomPropsHash = 0;
	int prevPassIndex = -1;
	int prevStencilRef = 0;

	int canBatch = 0;

	#endif

	const bool directLightBakedInLightProbes = LightProbes::AreBaked() && GetLightmapSettings().GetLightmapsMode() != LightmapSettings::kDualLightmapsMode;

	size_t ndata = m_Objects->size();
	outRemainingObjects.reserve (ndata / 16);

	for( size_t i = 0; i < ndata; ++i )
	{
		const PrePassRenderData& rpData = m_PlainRenderPasses[i];
		const RenderObjectData& roData = (*m_Objects)[rpData.roIndex];
		Shader* shader = roData.shader;

		int ss = roData.subShaderIndex;
		if (ss == -1)
			ss = shader->GetShaderLabShader()->GetDefaultSubshaderIndex(kRenderPathExtPrePass);

		const VisibleNode *node = roData.visibleNode;
		
		bool withinShadowDistance = true;
		float distanceAlongView = roData.distanceAlongView;
		if (distanceAlongView > shadowDistance)
		{
			// check whether its bounds is actually further than shadow distance
			// project extents onto camera forward axis
			float z = MultiplyAbsVectorZ (m_Context->m_CurCameraMatrix, node->worldAABB.GetExtent());
			Assert(z >= 0.0f);
			if (distanceAlongView - z > shadowDistance)
				withinShadowDistance = false;
		}
		
		if (ss == -1)
		{
			if (withinShadowDistance && node->renderer->GetReceiveShadows())
				receiverBounds.Encapsulate (node->worldAABB);
			outRemainingObjects.push_back() = roData;
			continue;
		}


		const float invScale = node->invScale;
		const float lodFade = node->lodFade;
		const int transformType = node->transformType;
		const UInt32 customPropsHash = node->renderer->GetCustomPropertiesHash();

		#if GFX_ENABLE_DRAW_CALL_BATCHING

		if (
			node->renderer->GetStaticBatchIndex() == 0 ||
			prevTransformType != transformType ||
			prevMaterial != roData.material ||
			prevShader != shader ||
			prevSubshaderIndex != ss ||
			!CompareApproximately(prevInvScale,invScale) || 
			!CompareApproximately(prevLodFade,lodFade, LOD_FADE_BATCH_EPSILON) ||
			prevCustomPropsHash != customPropsHash)
		{
			m_BatchRenderer.Flush();
			
			prevTransformType = transformType;
			prevMaterial = roData.material;
			prevShader = shader;
			prevSubshaderIndex = ss;
			prevInvScale = invScale;
			prevLodFade = lodFade;
			prevCustomPropsHash = customPropsHash;
			
			canBatch = 0;
		}
		else
			++canBatch;

		#endif

		SetObjectScale (device, lodFade, invScale);

		node->renderer->ApplyCustomProperties(*roData.material, shader, ss);

		const bool lightmapped = node->renderer->IsLightmappedForRendering();
		const Renderer* renderer = static_cast<Renderer*>(node->renderer);
		const bool directLightFromLightProbes = directLightBakedInLightProbes && node->renderer->GetRendererType() != kRendererIntermediate && renderer->GetUseLightProbes();

		ShaderLab::SubShader& subshader = shader->GetShaderLabShader()->GetSubShader (ss);
		int shaderPassCount = subshader.GetValidPassCount();
		for (int p = 0; p < shaderPassCount; ++p)
		{
			ShaderPassType passType;
			UInt32 passRenderOptions;
			subshader.GetPass(p)->GetPassOptions (passType, passRenderOptions);
			if (passType != kPassLightPrePassBase)
				continue;

			int stencilRef = kStencilMaskSomething;
			if (!lightmapped && !directLightFromLightProbes)
			{
				stencilRef += kStencilMaskNonLightmapped;
			}

			if (!withinShadowDistance)
				stencilRef += kStencilMaskBeyondShadowDistace;

			int layerStencilBit = lightingLayers.layerToStencil[node->renderer->GetLayer()];
			if (layerStencilBit != -1)
				stencilRef |= 1<<layerStencilBit;

			#if GFX_ENABLE_DRAW_CALL_BATCHING

			if ((p != prevPassIndex) ||
				(stencilRef != prevStencilRef))
			{
				m_BatchRenderer.Flush();
				prevPassIndex = p;
				prevStencilRef = stencilRef;
				canBatch = 0;
			}

			if (canBatch <= 1)
			#endif
			{
				device.SetStencilState (devStRender, stencilRef);
				channels = roData.material->SetPassWithShader(p, shader, ss);
			#if GFX_ENABLE_DRAW_CALL_BATCHING
				prevPassIndex = p;
				prevStencilRef = stencilRef;
			#endif
			}

			receiverBounds.Encapsulate (node->worldAABB);

			if (channels)
			{
				#if GFX_ENABLE_DRAW_CALL_BATCHING
				m_BatchRenderer.Add (node->renderer, roData.subsetIndex, channels, node->worldMatrix, transformType);
				#else
				SetupObjectMatrix (node->worldMatrix, transformType);
				node->renderer->Render (roData.subsetIndex, *channels);
				#endif
			}
		}
	}

	#if GFX_ENABLE_DRAW_CALL_BATCHING
	m_BatchRenderer.Flush();
	#endif
	
	return rtNormalsSpec;
}

void PrePassRenderLoop::RenderFinalPass (RenderTexture* rtMain, 
										 RenderTexture* rtLight, 
#if SEPERATE_PREPASS_SPECULAR
										 RenderTexture* rtLightSpec,
#endif
										 bool hdr, 
										 bool linearLighting)
{
	PROFILER_AUTO_GFX(gPrepassFinal, m_Context->m_Camera);
	GPU_AUTO_SECTION(kGPUSectionOpaquePass);

	GfxDevice& device = GetGfxDevice();
	device.SetNormalizationBackface( kNormalizationDisabled, false );

	RenderTexture::SetActive (rtMain);

	// Clear with background. Do not clear depth since we need the already
	// filled one from the base pass.
	device.SetSRGBWrite(!hdr && linearLighting && (!rtMain || rtMain->GetSRGBReadWrite()) );
	m_Context->m_Camera->ClearNoSkybox(true);

	if(rtLight)
		rtLight->SetGlobalProperty (kSLPropLightBuffer);
	else
	{
		ShaderLab::TexEnv *te = ShaderLab::g_GlobalProperties->SetTexture (kSLPropLightBuffer, hdr ? builtintex::GetBlackTexture() : builtintex::GetWhiteTexture());
		te->ClearMatrix();
	}
	
#if SEPERATE_PREPASS_SPECULAR
	if(rtLightSpec)
		rtLightSpec->SetGlobalProperty (kSLPropLightSpecBuffer);
	else
	{
		ShaderLab::TexEnv *te = ShaderLab::g_GlobalProperties->SetTexture (kSLPropLightSpecBuffer, hdr ? builtintex::GetBlackTexture() : builtintex::GetWhiteTexture());
		te->ClearMatrix();
	}	
#endif

	const ChannelAssigns* channels = NULL;
	const LightmapSettings& lightmapper = GetLightmapSettings();

	#if GFX_ENABLE_DRAW_CALL_BATCHING

	int prevPassIndex = -1;

	int prevLightmapIndex = -1;
	Vector4f prevLightmapST (0,0,0,0);
	int prevTransformType = -1;
	Material* prevMaterial = 0;
	Shader* prevShader = 0;
	int prevSubshaderIndex = -1;
	float prevInvScale = 0.0f;
	float prevLodFade = 0.0f;
	UInt32 prevCustomPropsHash = 0;

	int canBatch = 0;

	#endif

	if (hdr)
		g_ShaderKeywords.Enable (kKeywordHDRLightPrepassOn);
	else
		g_ShaderKeywords.Disable (kKeywordHDRLightPrepassOn);

	LightProbes* lightProbes = GetLightProbes();
	const bool areLightProbesBaked =  LightProbes::AreBaked();
	BuiltinShaderParamValues& builtinParamValues = GetGfxDevice().GetBuiltinParamValues();
	Vector3f ambientSH;
	SHEvalAmbientLight(GetRenderSettings().GetAmbientLightInActiveColorSpace(), &ambientSH[0]);

	size_t ndata = m_Objects->size();
	for( size_t i = 0; i < ndata; ++i )
	{
		const PrePassRenderData& rpData = m_PlainRenderPasses[i];
		const RenderObjectData& roData = (*m_Objects)[rpData.roIndex];
		
		const VisibleNode *node = roData.visibleNode;
		Shader* shader = roData.shader;
		
		int ss = roData.subShaderIndex;
		if (ss == -1)
			ss = shader->GetShaderLabShader()->GetDefaultSubshaderIndex(kRenderPathExtPrePass);
		if (ss == -1)
			continue;

		const Vector4f lightmapST = node->renderer->GetLightmapSTForRendering();
		const int lightmapIndex = roData.lightmapIndex;
		DebugAssert(lightmapIndex == node->renderer->GetLightmapIndex());

		const float invScale = node->invScale;
		const float lodFade = node->lodFade;
		const int transformType = node->transformType;
		const UInt32 customPropsHash = node->renderer->GetCustomPropertiesHash();

		#if GFX_ENABLE_DRAW_CALL_BATCHING

		if (
			node->renderer->GetStaticBatchIndex() == 0 ||
			prevTransformType != transformType ||
			prevMaterial != roData.material ||
			prevShader != shader ||
			prevSubshaderIndex != ss ||
			prevLightmapIndex != lightmapIndex ||
			!CompareMemory(prevLightmapST, lightmapST) ||
			!CompareApproximately(prevInvScale,invScale) ||
			!CompareApproximately(prevLodFade,lodFade) ||
			prevCustomPropsHash != customPropsHash)
		{
			m_BatchRenderer.Flush();
			
			prevLightmapIndex = lightmapIndex;
			prevLightmapST = lightmapST;
			prevTransformType = transformType;
			prevMaterial = roData.material;
			prevShader = shader;
			prevSubshaderIndex = ss;
			prevInvScale = invScale;
			prevLodFade = lodFade;
			prevCustomPropsHash = customPropsHash;

			canBatch = 0;
		}
		else
			++canBatch;

		#endif

		SetObjectScale (device, lodFade, invScale);

		node->renderer->ApplyCustomProperties(*roData.material, roData.shader, ss);

		ShaderLab::SubShader& subshader = roData.shader->GetShaderLabShader()->GetSubShader (ss);
		int shaderPassCount = subshader.GetValidPassCount();
		for (int p = 0; p < shaderPassCount; ++p)
		{
			ShaderPassType passType;
			UInt32 passRenderOptions;
			subshader.GetPass(p)->GetPassOptions (passType, passRenderOptions);
			if (passType != kPassLightPrePassFinal)
				continue;

			#if GFX_ENABLE_DRAW_CALL_BATCHING
			if (p != prevPassIndex)
			{
				m_BatchRenderer.Flush();
				canBatch = 0;
			}

			if (canBatch <= 1)
			#endif
			{
				// lightmap
				SetupObjectLightmaps (lightmapper, lightmapIndex, lightmapST, false);

				// light probes
				// TODO: figure how does that interact with lightmaps and with batching;
				// if we are about to use light probes and the renderer gets different coeffs (maybe a simpler check?) => can't batch
				float lightProbeCoeffs[9][3];
				memset (lightProbeCoeffs, 0, sizeof(lightProbeCoeffs));
				if (areLightProbesBaked && node->renderer->GetRendererType() != kRendererIntermediate)
				{
					Renderer* renderer = static_cast<Renderer*>(node->renderer);
					if (renderer && renderer->GetUseLightProbes())
						lightProbes->GetInterpolatedLightProbe(renderer->GetLightProbeInterpolationPosition(node->worldAABB), renderer, &(lightProbeCoeffs[0][0]));
				}
				lightProbeCoeffs[0][0] += ambientSH[0];
				lightProbeCoeffs[0][1] += ambientSH[1];
				lightProbeCoeffs[0][2] += ambientSH[2];
				SetSHConstants (lightProbeCoeffs, builtinParamValues);
				
				// set pass
				channels = roData.material->SetPassWithShader(p, shader, ss);
			}

			#if GFX_ENABLE_DRAW_CALL_BATCHING
			prevPassIndex = p;
			#endif
			
			if (channels)
			{
				#if GFX_ENABLE_DRAW_CALL_BATCHING
				m_BatchRenderer.Add (node->renderer, roData.subsetIndex, channels, node->worldMatrix, transformType);
				#else
				SetupObjectMatrix (node->worldMatrix, transformType);
				node->renderer->Render (roData.subsetIndex, *channels);
				#endif
			}
		}
	}

	#if GFX_ENABLE_DRAW_CALL_BATCHING
	m_BatchRenderer.Flush();
	#endif

	GetGfxDevice().SetSRGBWrite(false);
}


PrePassRenderLoop* CreatePrePassRenderLoop()
{
	return new PrePassRenderLoop();
}

void DeletePrePassRenderLoop (PrePassRenderLoop* queue)
{
	delete queue;
}


static UInt32 CalculateLightingLayers ()
{
	// TODO: Use active lights instead
	const LightManager::Lights& lights = GetLightManager().GetAllLights();
	LightManager::Lights::const_iterator it, itEnd = lights.end();
	UInt32 layers = ~0;
	for (it = lights.begin(); it != itEnd; ++it)
	{
		UInt32 mask = it->GetCullingMask();
		if (mask == 0)
			continue;
		layers &= mask;
	}
	return ~layers;
}


#if UNITY_EDITOR
static void CheckLightLayerUsage (const LightingLayers& layers)
{
	static bool s_UsageWasOK = true;
	bool usageIsOK = (layers.lightLayerCount <= kLightingLayerCount);

	// Only log/remove warning message when broken vs. okay has changed
	if (usageIsOK == s_UsageWasOK)
		return;

	s_UsageWasOK = usageIsOK;

	// Remove any previous error
	// Use instanceID of QualitySettings as log identifier
	RemoveErrorWithIdentifierFromConsole (GetQualitySettings().GetInstanceID());

	if (!usageIsOK)
	{
		std::string msg = Format(
			"Too many layers used to exclude objects from lighting. Up to %i layers can be used to exclude lights, while your lights use %i:",
			kLightingLayerCount,
			layers.lightLayerCount);
		for (int i = 0; i < LightingLayers::kLayerCount; ++i)
		{
			if (layers.lightingLayerMask & (1<<i))
			{
				std::string layerName = LayerToString (i);
				if (layerName.empty())
					layerName = "Unnamed " + IntToString (i);
				msg += " '" + layerName + "'";
			}
		}
		// Use instanceID of QualitySettings as log identifier
		DebugStringToFile (msg, 0, __FILE__, __LINE__, kScriptingWarning, 0, GetQualitySettings().GetInstanceID());
	}
}
#endif

static void ResolveDepthIntoTextureIfNeeded (
	GfxDevice& device,
	RenderLoop& renderLoop,
	DepthBufferFormat depthFormat,
	RenderTexture*& outDepthRT,
	TextureID* outDepthTextureID,
	bool* outDepthWasCopied)
{
	// TODO FIXME!! Should add GLES20 here as well, but it's missing GfxDevice::ResolveDepthIntoTexture!

#if GFX_SUPPORTS_D3D9 || GFX_SUPPORTS_D3D11 || GFX_SUPPORTS_OPENGL || GFX_SUPPORTS_OPENGLES30
	bool needsDepthResolve = false;
#if GFX_SUPPORTS_D3D9
	// If doing depth tests & sampling as INTZ is very slow,
	// do a depth resolve into a separate texture first.
	needsDepthResolve |= (device.GetRenderer() == kGfxRendererD3D9 && gGraphicsCaps.hasStencilInDepthTexture && gGraphicsCaps.d3d.hasDepthResolveRESZ && gGraphicsCaps.d3d.slowINTZSampling);
#endif
#if GFX_SUPPORTS_D3D11
	// Always needs resolve on D3D11.
	needsDepthResolve |= (device.GetRenderer() == kGfxRendererD3D11);
#endif
#if GFX_SUPPORTS_OPENGL
	// Needs resolve on OpenGL, unless we did the slow RenderBasePassDepth().
	// TODO: get rid of buggyPackedDepthStencil
	needsDepthResolve |= (device.GetRenderer() == kGfxRendererOpenGL) && !gGraphicsCaps.gl.buggyPackedDepthStencil;
#endif
#if GFX_SUPPORTS_OPENGLES30
	// Always needs resolve on GLES30.
	needsDepthResolve |= (device.GetRenderer() == kGfxRendererOpenGLES30);
#endif

	if (needsDepthResolve)
	{
		DebugAssert (depthFormat != kDepthFormatNone);
		RenderTexture* depthCopy = GetRenderBufferManager().GetTempBuffer (RenderBufferManager::kFullSize, RenderBufferManager::kFullSize, depthFormat, kRTFormatDepth, RenderBufferManager::kRBSampleOnlyDepth, kRTReadWriteLinear);
		depthCopy->SetFilterMode (kTexFilterNearest);
		if (!depthCopy->IsCreated())
			depthCopy->Create();
		AddRenderLoopTempBuffer (&renderLoop, depthCopy);

		device.ResolveDepthIntoTexture (depthCopy->GetColorSurfaceHandle (), depthCopy->GetDepthSurfaceHandle ());

		outDepthRT = depthCopy;
		*outDepthTextureID = depthCopy->GetTextureID ();
		*outDepthWasCopied = true;
	}

#endif
}

#if ENABLE_PRE_PASS_LOOP_HASH_SORTING
template<typename T>
static UInt8* InstertToHashBufferPreLoop(const T* p, UInt8* buffer)
{
	Assert((sizeof(T) % 4) == 0);	// unaligned write
	*reinterpret_cast<T*>(buffer) = *p;
	return buffer + sizeof(T);
	}
#endif

void DoPrePassRenderLoop (
	RenderLoopContext& ctx,
	RenderObjectDataContainer& objects,
	RenderObjectDataContainer& outRemainingObjects,
	RenderTexture*& outDepthRT,
	RenderTexture*& outDepthNormalsRT,
	RenderTexture*& outMainShadowMap,
	ActiveLights& activeLights,
	bool linearLighting,
	bool* outDepthWasCopied)
{
	outDepthRT = NULL;
	outDepthNormalsRT = NULL;
	*outDepthWasCopied = false;

	// Allocated on the stack each time, uses temp allocators
	PrePassRenderLoop loop;
	loop.m_Context = &ctx;
	loop.m_Objects = &objects;

	loop.m_PlainRenderPasses.resize_uninitialized(0);
	
	RenderObjectDataContainer::iterator itEnd = objects.end();
	size_t roIndex = 0;
	for (RenderObjectDataContainer::iterator it = objects.begin(); it != itEnd; ++it, ++roIndex)
	{
		RenderObjectData& odata = *it;
		const VisibleNode *node = odata.visibleNode;
		BaseRenderer* renderer = node->renderer;

		PrePassRenderData rpData;
		rpData.roIndex = roIndex;

#if ENABLE_PRE_PASS_LOOP_HASH_SORTING

		//hash state information for render object sorter
		const int kHashBufferSize = 64;
		UInt8 hashBuffer[kHashBufferSize];
		UInt8* hashPtr = hashBuffer;

		// Always write 32b granularity into the hash buffer to avoid unaligned writes
		UInt32 transformType = static_cast<UInt32>(renderer->GetTransformInfo().transformType);	
		hashPtr = InstertToHashBufferPreLoop(&transformType, hashPtr);
		hashPtr = InstertToHashBufferPreLoop(&node->invScale, hashPtr);
		hashPtr = InstertToHashBufferPreLoop(&node->lodFade, hashPtr);
		int materialID = odata.material->GetInstanceID();
		hashPtr = InstertToHashBufferPreLoop(&materialID, hashPtr);
		int shaderID = odata.shader->GetInstanceID();
		hashPtr = InstertToHashBufferPreLoop(&shaderID, hashPtr);
		int ss = odata.shader->GetShaderLabShader()->GetDefaultSubshaderIndex(kRenderPathExtPrePass);
		hashPtr = InstertToHashBufferPreLoop(&ss, hashPtr);
		#if GFX_ENABLE_DRAW_CALL_BATCHING
		hashPtr = InstertToHashBufferPreLoop(&odata.staticBatchIndex, hashPtr);
		#endif
		Assert(hashPtr-hashBuffer <= kHashBufferSize);

		rpData.hash = MurmurHash2A(hashBuffer, hashPtr-hashBuffer, 0x9747b28c);
#endif
		loop.m_PlainRenderPasses.push_back( rpData );
	}
		
	// Sort objects
	{
			PROFILER_AUTO(gPrepassSort, ctx.m_Camera);
#if ENABLE_PRE_PASS_LOOP_HASH_SORTING 			
			loop.SortPreRenderPassData(loop.m_PlainRenderPasses);
#else		
			std::sort (objects.begin(), objects.end(), RenderPrePassObjectSorter());
#endif
	}

	// Setup shadow distance, fade and ambient parameters
	BuiltinShaderParamValues& params = GetGfxDevice().GetBuiltinParamValues();
	Vector4f lightFade;
	Vector4f fadeCenterAndType;
	CalculateLightShadowFade (*ctx.m_Camera, 1.0f, lightFade, fadeCenterAndType);
	params.SetVectorParam(kShaderVecLightmapFade, lightFade);
	params.SetVectorParam(kShaderVecShadowFadeCenterAndType, fadeCenterAndType);
	params.SetVectorParam(kShaderVecUnityAmbient, Vector4f(GetRenderSettings().GetAmbientLightInActiveColorSpace().GetPtr()));

	GfxDevice& device = GetGfxDevice();

	// Prepare for rendering
	RenderTexture* rtMain = ctx.m_Camera->GetCurrentTargetTexture ();
	Assert (rtMain);
	if (!rtMain->IsCreated())
		rtMain->Create();

	LightingLayers lightingLayers (CalculateLightingLayers ());
	#if UNITY_EDITOR
	CheckLightLayerUsage (lightingLayers);
	#endif
	
	// Don't allow shaders to set their own stencil state from base pass until
	// the end of light pass, since it would screw them up.
	ShaderLab::g_GlobalAllowShaderStencil = false;
	
	// Render Geometry base pass
	MinMaxAABB receiverBounds;
	RenderTexture* rtNormalsSpec = loop.RenderBasePass (rtMain, lightingLayers, outRemainingObjects, receiverBounds);
	outDepthRT = rtNormalsSpec;
	
	RenderSurfaceHandle colorSurfaceHandle = rtNormalsSpec->GetColorSurfaceHandle();
	RenderSurfaceHandle depthTextureHandle = rtMain->GetDepthSurfaceHandle();
	TextureID depthTextureID = rtMain->GetSecondaryTextureID();
	DepthBufferFormat depthFormat = rtMain->GetDepthFormat();
	
	#if GFX_SUPPORTS_OPENGL
	if (device.GetRenderer() == kGfxRendererOpenGL && gGraphicsCaps.gl.buggyPackedDepthStencil)
	{
		// Separate pass to render depth into a separate target. And then use that texture to read depth
		// in the lighting pass.
		RenderTexture* rtDepth = RenderBasePassDepth (ctx, objects, loop.m_PlainRenderPasses);
		depthTextureID = rtDepth->GetTextureID();
		outDepthRT = rtDepth;
		colorSurfaceHandle = rtDepth->GetColorSurfaceHandle();
		depthTextureHandle = rtDepth->GetDepthSurfaceHandle();
		*outDepthWasCopied = true;
	}
	#endif

	if (gGraphicsCaps.hasStencilInDepthTexture)
	{
		const ActiveLight* mainActiveLight = GetMainActiveLight(activeLights);
		Light* mainLight = mainActiveLight ? mainActiveLight->light : NULL;
		const bool mainLightHasShadows = mainLight && mainLight->GetType() == kLightDirectional && mainLight->GetShadows() != kShadowNone;
		const bool cameraNeedsDepthTexture = (ctx.m_Camera->GetDepthTextureMode() & Camera::kDepthTexDepthBit);
		if (mainLightHasShadows || cameraNeedsDepthTexture)
		{
			RenderForwardObjectsIntoDepth (
				ctx,
				rtMain,
				&outRemainingObjects,
				colorSurfaceHandle,
				depthTextureHandle,
				rtMain->GetWidth(),
				rtMain->GetHeight(),
				cameraNeedsDepthTexture
			);
		}
	}

	ResolveDepthIntoTextureIfNeeded (device, *(ctx.m_RenderLoop), depthFormat, outDepthRT, &depthTextureID, outDepthWasCopied);
	
	// Render Lighting pass
	RenderTexture* rtLight = NULL;
#if SEPERATE_PREPASS_SPECULAR
	RenderTexture* rtLightSpec = NULL;
#endif
	loop.RenderLighting (activeLights,
						 rtMain, 
						 depthTextureID, 
						 rtNormalsSpec,
						 rtLight,
#if SEPERATE_PREPASS_SPECULAR
						 rtLightSpec,
#endif		
						 lightFade, 
						 lightingLayers, 
						 receiverBounds, 
						 &outMainShadowMap);

	// It's again ok for shaders to set their stencil state now.
	ShaderLab::g_GlobalAllowShaderStencil = true;

	if (ctx.m_Camera->GetClearStencilAfterLightingPass())
	{
		float black[] = {0,0,0,0};
		device.Clear (kGfxClearStencil, black, 1.0f, 0);
	}
	
	// Render final Geometry pass
	loop.RenderFinalPass (rtMain, 
						  rtLight,
#if SEPERATE_PREPASS_SPECULAR
						  rtLightSpec,
#endif
						  ctx.m_Camera->GetUsingHDR(), 
						  linearLighting);
	
	if (rtLight)
	{
		// Do not release the light buffer yet; so that image effects or whatever can access it later
		// if needed (via _LightBuffer)
		device.SetSurfaceFlags(rtLight->GetColorSurfaceHandle(), GfxDevice::kSurfaceDefault, ~GfxDevice::kSurfaceRestoreMask);
		device.SetSurfaceFlags(rtLight->GetDepthSurfaceHandle(), GfxDevice::kSurfaceDefault, ~GfxDevice::kSurfaceRestoreMask);
		AddRenderLoopTempBuffer (ctx.m_RenderLoop, rtLight);
	}
	
#if SEPERATE_PREPASS_SPECULAR
	if (rtLightSpec)
	{
		device.SetSurfaceFlags(rtLightSpec->GetColorSurfaceHandle(), GfxDevice::kSurfaceDefault, ~GfxDevice::kSurfaceRestoreMask);
		device.SetSurfaceFlags(rtLightSpec->GetDepthSurfaceHandle(), GfxDevice::kSurfaceDefault, ~GfxDevice::kSurfaceRestoreMask);
		AddRenderLoopTempBuffer (ctx.m_RenderLoop, rtLightSpec);
	}
#endif
	
	// Combine depth+normals if needed
	if (ctx.m_Camera->GetDepthTextureMode() & Camera::kDepthTexDepthNormalsBit)
	{
		outDepthNormalsRT = CombineDepthNormalsTexture (ctx, outRemainingObjects);
		RenderTexture::SetActive (rtMain);
	}
	
	device.SetViewMatrix( ctx.m_CurCameraMatrix.GetPtr() );
	device.SetNormalizationBackface( kNormalizationDisabled, false );	
}

#endif // GFX_SUPPORTS_RENDERLOOP_PREPASS
