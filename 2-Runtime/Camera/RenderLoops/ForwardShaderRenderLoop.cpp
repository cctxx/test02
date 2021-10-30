#include "UnityPrefix.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"

#include "RenderLoopPrivate.h"
#include "RenderLoop.h"
#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/Renderable.h"
#include "Runtime/Camera/Light.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Camera/Shadows.h"
#include "Runtime/Camera/LODGroupManager.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "Runtime/Graphics/GraphicsHelper.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/Graphics/Transform.h"
#include "External/shaderlab/Library/intshader.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/BatchRendering.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "BuiltinShaderParamUtility.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Camera/LightManager.h"
#include "External/MurmurHash/MurmurHash2.h"


// Enable/disable hash based forward shader render loop sorting functionality.
#define ENABLE_FORWARD_SHADER_LOOP_HASH_SORTING 0

PROFILER_INFORMATION(gFwdOpaquePrepare, "RenderForwardOpaque.Prepare", kProfilerRender)
PROFILER_INFORMATION(gFwdOpaqueSort, "RenderForwardOpaque.Sort", kProfilerRender)
PROFILER_INFORMATION(gFwdOpaqueCollectShadows, "RenderForwardOpaque.CollectShadows", kProfilerRender)
PROFILER_INFORMATION(gFwdOpaqueRender, "RenderForwardOpaque.Render", kProfilerRender)
PROFILER_INFORMATION(gFwdAlphaPrepare, "RenderForwardAlpha.Prepare", kProfilerRender)
PROFILER_INFORMATION(gFwdAlphaSort, "RenderForwardAlpha.Sort", kProfilerRender)
PROFILER_INFORMATION(gFwdAlphaRender, "RenderForwardAlpha.Render", kProfilerRender)

static SHADERPROP (ShadowMapTexture);


static inline bool CompareLights (ForwardLightsBlock const* a, ForwardLightsBlock const* b)
{
	if (!a || !b)
		return false;

	if (a->mainLight != b->mainLight)
		return false;
	if (a->vertexLightCount != b->vertexLightCount)
		return false;
	if (a->addLightCount != b->addLightCount)
		return false;

	int totalLightCount = a->vertexLightCount + a->addLightCount;
	const ActiveLight* const* lightsA = a->GetLights();
	const ActiveLight* const* lightsB = b->GetLights();
	for (int i = 0; i < totalLightCount; ++i)
		if (lightsA[i] != lightsB[i])
			return false;

	if (memcmp(a->sh, b->sh, sizeof(a->sh)) != 0)
		return false;

	if (!CompareApproximately(a->lastAddLightBlend, b->lastAddLightBlend))
		return false;
	if (!CompareApproximately(a->lastVertexLightBlend, b->lastVertexLightBlend))
		return false;

	return true;
}

struct RenderObjectDataCold {
	float		invScale;						// 4
	float		lodFade;						// 4
	size_t		lightsDataOffset;				// 4	into memory block with all light data chunks
	int			subshaderIndex;					// 4
	// 16 bytes
};


namespace ForwardShaderRenderLoop_Enum
{
// Render pass data here is 8 bytes each; an index of the render object and "the rest" packed
// into 4 bytes.
enum {
	kPackPassShift = 0,
	kPackPassMask = 0xFF,
	kPackTypeShift = 8,
	kPackTypeMask = 0xFF,
	kPackFirstPassFlag = (1<<24),
	kPackMultiPassFlag = (1<<25),
};

} // namespace ForwardShaderRenderLoop_Enum

struct RenderPassData {
	int	roIndex;
	// Packed into UInt32: pass number, pass type, first pass flag, multipass flag
	UInt32 data;
#if ENABLE_FORWARD_SHADER_LOOP_HASH_SORTING
	// state hash for optimizing render object sorter
	UInt32 hash;
#endif
};
typedef dynamic_array<RenderPassData> RenderPasses;


struct ForwardShaderRenderState
{
	int rendererType;
	int transformType;

	float invScale;
	float lodFade;

	Material* material;
	Shader* shader;
	int subshaderIndex;
	ShaderPassType passType;
	int passIndex;

	const ForwardLightsBlock* lights;
	int receiveShadows;

	int lightmapIndex;
	Vector4f lightmapST;

	UInt32 customPropsHash;


	void Invalidate()
	{
		rendererType = -1;
		transformType = -1;
		invScale = 0.0f;
		lodFade = 0.0F;
		material = 0; shader = 0; subshaderIndex = -1; passType = kShaderPassTypeCount; passIndex = -1;
		lights = 0;
		lightmapIndex = -1; lightmapST = Vector4f(0,0,0,0);
		receiveShadows = -1;
		customPropsHash = 0;
	}

	bool operator == (const ForwardShaderRenderState& rhs) const
	{
		if (this == &rhs)
			return true;

		return (
				rendererType == rhs.rendererType &&
				transformType == rhs.transformType &&
				material == rhs.material &&
				shader == rhs.shader &&
				CompareLights(lights, rhs.lights) &&
				subshaderIndex == rhs.subshaderIndex &&
				passType == rhs.passType &&
				passIndex == rhs.passIndex &&
				CompareApproximately(invScale,rhs.invScale) &&
				CompareApproximately(lodFade,rhs.lodFade, LOD_FADE_BATCH_EPSILON) &&
			#if ENABLE_SHADOWS
				receiveShadows == rhs.receiveShadows &&
			#endif
				lightmapIndex == rhs.lightmapIndex &&
				lightmapST == rhs.lightmapST &&
				customPropsHash == rhs.customPropsHash
				);
	}

	bool operator != (const ForwardShaderRenderState& rhs) const
	{
		return !(rhs == *this);
	}
};


struct ForwardShadowMap
{
	ForwardShadowMap() : light(NULL), texture(NULL) {}
	const ActiveLight* light;
	RenderTexture* texture;
	Matrix4x4f shadowMatrix;
	MinMaxAABB receiverBounds;
};
typedef dynamic_array<ForwardShadowMap> ForwardShadowMaps;

struct CompactShadowCollectorSortData;

struct ForwardShaderRenderLoop
{
	const RenderLoopContext*	m_Context;
	RenderObjectDataContainer*	m_Objects;

	dynamic_array<RenderObjectDataCold> m_RenderObjectsCold;
	dynamic_array<UInt8>		m_RenderObjectsLightData;

	RenderPasses				m_PlainRenderPasses;
	#if ENABLE_SHADOWS
	ForwardShadowMap			m_MainShadowMap;
	ForwardShadowMaps			m_ShadowMaps;
	// Render object indices of shadow receivers.
	// This includes both shadow receivers and objects that have shadows off, but
	// are within shadow distance. They should still participate in screenspace shadow
	// gathering, otherwise shadows will be visible through them.
	dynamic_array<int>			m_ReceiverObjects;
	#endif

	BatchRenderer				m_BatchRenderer;

	ForwardShaderRenderLoop()
		: m_RenderObjectsCold		(kMemTempAlloc)
		, m_RenderObjectsLightData	(kMemTempAlloc)
		, m_PlainRenderPasses		(kMemTempAlloc)
		#if ENABLE_SHADOWS
		, m_ShadowMaps				(kMemTempAlloc)
		, m_ReceiverObjects			(kMemTempAlloc)
		#endif
	{ }

	void PerformRendering (const ActiveLight* mainDirShadowLight, RenderTexture* existingShadowMap, const ShadowCullData& shadowCullData, bool disableDynamicBatching, bool sRGBrenderTarget, bool clearFrameBuffer);
	#if ENABLE_SHADOWS
	RenderTexture* CollectShadows (RenderTexture* inputShadowMap, const Light* light, const Matrix4x4f* shadowMatrices, const float* splitDistances, const Vector4f* splitSphereCentersAndSquaredRadii, bool enableSoftShadows, bool useDualInForward, bool clearFrameBuffer);
	void RenderLightShadowMaps (ForwardShadowMap& shadowMap, ShadowCameraData& camData, bool enableSoftShadows, bool useDualInForward, bool clearFrameBuffer);
	int SortShadowCollectorsCompact(CompactShadowCollectorSortData* _resultOrder);
	#endif

	template <bool opaque>
	struct RenderObjectSorter
	{
		bool operator()( const RenderPassData& ra, const RenderPassData& rb ) const;
		const ForwardShaderRenderLoop* queue;
	};

	template <bool opaque>
	void SortRenderPassData( RenderPasses& passes )
	{
		RenderObjectSorter<opaque> sorter;
		sorter.queue = this;
		std::sort( passes.begin(), passes.end(), sorter );
	}
};


template <bool opaque>
bool ForwardShaderRenderLoop::RenderObjectSorter<opaque>::operator() (const RenderPassData& ra, const RenderPassData& rb) const
{
	using namespace ForwardShaderRenderLoop_Enum;

	const RenderObjectData& dataa = (*queue->m_Objects)[ra.roIndex];
	const RenderObjectData& datab = (*queue->m_Objects)[rb.roIndex];

	// Sort by layering depth.
	bool globalLayeringResult;
	if (CompareGlobalLayeringData(dataa.globalLayeringData, datab.globalLayeringData, globalLayeringResult))
		return globalLayeringResult;

#if ENABLE_FORWARD_SHADER_LOOP_HASH_SORTING

	if (!opaque)
	{
		// Sort by render queues first
		if( dataa.queueIndex != datab.queueIndex )
			return dataa.queueIndex < datab.queueIndex;

#if DEBUGMODE
		DebugAssertIf (dataa.queueIndex >= kQueueIndexMin && dataa.queueIndex <= kGeometryQueueIndexMax); // this is alpha loop!
#endif

		// Sort strictly by distance unless they are equal
		if( dataa.distance != datab.distance )
			return dataa.distance < datab.distance;
	}

	UInt64 keya = (0x0000ffff-((dataa.queueIndex)&0x0000ffff))<<16;
	UInt64 keyb = (0x0000ffff-((datab.queueIndex)&0x0000ffff))<<16;

	keya |= (ra.data & kPackFirstPassFlag)>>(24-8);
	keyb |= (rb.data & kPackFirstPassFlag)>>(24-8);
	keya |= (0x000000ff-((dataa.lightmapIndex)&0x000000ff));
	keyb |= (0x000000ff-((datab.lightmapIndex)&0x000000ff));
	keya = keya << 32;
	keyb = keyb << 32;
	keya |= ra.hash;
	keyb |= rb.hash;

	//Sort keys, TODO try to move the key generation outside the sorting loop
	if( keya != keyb )
		return (keya > keyb);

#if DEBUGMODE
	if (opaque)
	{
		DebugAssertIf (dataa.queueIndex < kQueueIndexMin || dataa.queueIndex > kGeometryQueueIndexMax); // this is opaque loop!
	}
#endif

	//fall though distance, TODO insert distance into the key
	return dataa.distance > datab.distance;

#else

	// Sort by render queues first
	if( dataa.queueIndex != datab.queueIndex )
		return dataa.queueIndex < datab.queueIndex;

#if DEBUGMODE
	if (opaque) {
		DebugAssertIf (dataa.queueIndex < kQueueIndexMin || dataa.queueIndex > kGeometryQueueIndexMax); // this is opaque loop!
	} else {
		DebugAssertIf (dataa.queueIndex >= kQueueIndexMin && dataa.queueIndex <= kGeometryQueueIndexMax); // this is alpha loop!
	}
#endif

	if (!opaque)
	{
		// Sort strictly by distance unless they are equal
		if( dataa.distance != datab.distance )
			return dataa.distance < datab.distance;
	}

	UInt32 flagsa = ra.data;
	UInt32 flagsb = rb.data;

	// render all first passes first
	if( (flagsa & kPackFirstPassFlag) != (flagsb & kPackFirstPassFlag) )
		return (flagsa & kPackFirstPassFlag) > (flagsb & kPackFirstPassFlag);

	// sort by lightmap index (fine to do it before source material index
	// since every part of same mesh will have the same lightmap index)
	if( dataa.lightmapIndex != datab.lightmapIndex )
		return dataa.lightmapIndex < datab.lightmapIndex;

#if GFX_ENABLE_DRAW_CALL_BATCHING
	// if part of predefined static batch, then sort by static batch index
	// prefer static batched first as they usually cover quite a lot
	if( dataa.staticBatchIndex != datab.staticBatchIndex )
		return dataa.staticBatchIndex > datab.staticBatchIndex;

	// otherwise sort by material index. Some people are using multiple materials
	// on a single mesh and expect them to be rendered in order.
	if( dataa.staticBatchIndex == 0 && dataa.sourceMaterialIndex != datab.sourceMaterialIndex )
		return dataa.sourceMaterialIndex < datab.sourceMaterialIndex;
#else
	// Sort by material index. Some people are using multiple materials
	// on a single mesh and expect them to be rendered in order.
	if( dataa.sourceMaterialIndex != datab.sourceMaterialIndex )
		return dataa.sourceMaterialIndex < datab.sourceMaterialIndex;
#endif

	// sort by shader
	if( dataa.shader != datab.shader )
		return dataa.shader->GetInstanceID() < datab.shader->GetInstanceID(); // just compare instance IDs

	// then sort by material
	if( dataa.material != datab.material )
		return dataa.material->GetInstanceID() < datab.material->GetInstanceID(); // just compare instance IDs

	// inside same material: by pass
	UInt32 passa = (flagsa >> kPackPassShift) & kPackPassMask;
	UInt32 passb = (flagsb >> kPackPassShift) & kPackPassMask;
	if( passa != passb )
		return passa < passb;

	if (opaque)
	{
		// Sort by distance in reverse order.
		// That way we get consistency in render order, and more pixels not rendered due to z-testing,
		// which benefits performance.
		if( dataa.distance != datab.distance )
			return dataa.distance > datab.distance;
	}

	// fall through: roIndex
	return ra.roIndex < rb.roIndex;

#endif // ENABLE_FORWARD_SHADER_LOOP_HASH_SORTING
}

#if ENABLE_SHADOWS
static void SetLightShadowProps (const Camera& camera, const Light& light, Texture* shadowMap, const Matrix4x4f& shadowMatrix, bool useDualInForward)
{
	const float shadowStrength = light.GetShadowStrength();
	DebugAssert (shadowMap);

	ShaderLab::PropertySheet *props = ShaderLab::g_GlobalProperties;
	BuiltinShaderParamValues& params = GetGfxDevice().GetBuiltinParamValues();

	// shadow matrix
	CopyMatrix (shadowMatrix.GetPtr(), params.GetWritableMatrixParam(kShaderMatWorldToShadow).GetPtr());

	props->SetTexture( kSLPropShadowMapTexture, shadowMap );

	if (light.GetType() == kLightPoint)
	{
		const Vector3f lightPos = light.GetWorldPosition();
		params.SetVectorParam(kShaderVecLightPositionRange, Vector4f(lightPos.x, lightPos.y, lightPos.z, 1.0f/light.GetRange()));
	}

	// ambient & shadow fade out
	Vector4f lightFade;
	Vector4f fadeCenterAndType;
	CalculateLightShadowFade (camera, shadowStrength, lightFade, fadeCenterAndType);
	params.SetVectorParam(kShaderVecLightmapFade, lightFade);
	if (useDualInForward)
		lightFade.z = lightFade.w = 0.0f;
	params.SetVectorParam(kShaderVecLightShadowData, lightFade);
	params.SetVectorParam(kShaderVecShadowFadeCenterAndType, fadeCenterAndType);
	// texel offsets for PCF
	Vector4f offsets;
	float offX = 0.5f / shadowMap->GetGLWidth();
	float offY = 0.5f / shadowMap->GetGLHeight();
	offsets.z = 0.0f; offsets.w = 0.0f;
	offsets.x = -offX; offsets.y = -offY; params.SetVectorParam(kShaderVecShadowOffset0, offsets);
	offsets.x =  offX; offsets.y = -offY; params.SetVectorParam(kShaderVecShadowOffset1, offsets);
	offsets.x = -offX; offsets.y =  offY; params.SetVectorParam(kShaderVecShadowOffset2, offsets);
	offsets.x =  offX; offsets.y =  offY; params.SetVectorParam(kShaderVecShadowOffset3, offsets);
}
static void SetLightShadowCollectProps (const Camera& camera, const Light& light, Texture* shadowMap, const Matrix4x4f* shadowMatrices, const float* splitDistances, const Vector4f* splitSphereCentersAndSquaredRadii, bool useDualInForward)
{
	DebugAssert (shadowMatrices && shadowMap);
	SetLightShadowProps (camera, light, shadowMap, shadowMatrices[0], useDualInForward);
	SetCascadedShadowShaderParams (shadowMatrices, splitDistances, splitSphereCentersAndSquaredRadii);
}
#endif // ENABLE_SHADOWS



void ForwardShaderRenderLoop::PerformRendering (const ActiveLight* mainDirShadowLight, RenderTexture* existingShadowMap, const ShadowCullData& shadowCullData, bool disableDynamicBatching, bool sRGBrenderTarget, bool clearFrameBuffer)
{
	using namespace ForwardShaderRenderLoop_Enum;

	const RenderManager::Renderables& renderables = GetRenderManager ().GetRenderables ();
	RenderManager::Renderables::const_iterator renderablesBegin = renderables.begin(), renderablesEnd = renderables.end();

	SetNoShadowsKeywords();

	GfxDevice& device = GetGfxDevice();
	// save current scissor params
	int oldScissorRect[4];
	device.GetScissorRect(oldScissorRect);
	const bool oldScissor = device.IsScissorEnabled();

	#if ENABLE_SHADOWS
	const bool enableSoftShadows = GetSoftShadowsEnabled();
	ShadowCameraData camData(shadowCullData);
	ForwardShadowMap mainLightShadowMap;
	const bool hasAnyShadows = (mainDirShadowLight != 0 || !m_ShadowMaps.empty());
	const bool useDualInForward = GetLightmapSettings().GetUseDualLightmapsInForward();

	// shadow map of main directional light
	if (mainDirShadowLight != 0)
	{
		// Render shadow map
		if (!existingShadowMap)
		{
			// Prevent receiver bounds to be zero size in any dimension;
			// causes trouble with calculating intersection of frustum and bounds.
			mainLightShadowMap.receiverBounds = m_MainShadowMap.receiverBounds;
			mainLightShadowMap.receiverBounds.Expand (0.01f);
			mainLightShadowMap.light = mainDirShadowLight;

			// One directional light can have shadows in free version, so temporarily
			// enable render textures just for that.
			RenderTexture::SetTemporarilyAllowIndieRenderTexture (true);
			RenderLightShadowMaps (mainLightShadowMap, camData, enableSoftShadows, useDualInForward, clearFrameBuffer);
			RenderTexture::SetTemporarilyAllowIndieRenderTexture (false);

			// There were no shadow casters - no shadowmap is produced
			if (!mainLightShadowMap.texture)
				mainDirShadowLight = 0;
		}
		else
		{
			mainLightShadowMap.texture = existingShadowMap;
		}
	}

	// shadow maps of other lights
	for (ForwardShadowMaps::iterator it = m_ShadowMaps.begin(), itEnd = m_ShadowMaps.end(); it != itEnd; ++it)
	{
		ForwardShadowMap& shadowMap = *it;

		// Prevent receiver bounds to be zero size in any dimension;
		// causes trouble with calculating intersection of frustum and bounds.
		shadowMap.receiverBounds.Expand (0.01f);

		RenderLightShadowMaps (shadowMap, camData, enableSoftShadows, false, clearFrameBuffer);
	}

	if (hasAnyShadows)
	{
		m_Context->m_Camera->SetupRender (Camera::kRenderFlagSetRenderTarget);
		SetNoShadowsKeywords ();
	}
	#endif

	const RenderSettings& renderSettings = GetRenderSettings();
	const LightmapSettings& lightmapper = GetLightmapSettings();
	size_t npasses = m_PlainRenderPasses.size();

	int currentQueueIndex = m_Context->m_RenderQueueStart;

	device.SetViewMatrix( m_Context->m_CurCameraMatrix.GetPtr() );

	ForwardShaderRenderState prevRenderState;
	prevRenderState.Invalidate();

	//If we are in linear lighting enable sRGB writes here...
	device.SetSRGBWrite(sRGBrenderTarget);
	if (clearFrameBuffer)
		m_Context->m_Camera->ClearNoSkybox(false);
	else
		device.IgnoreNextUnresolveOnCurrentRenderTarget();
	
	const ChannelAssigns* channels = NULL;

	for( size_t i = 0; i < npasses; ++i )
	{
		const RenderPassData& rpData = m_PlainRenderPasses[i];
		const RenderObjectData& roDataH = (*m_Objects)[rpData.roIndex];
		const RenderObjectDataCold& roDataC = m_RenderObjectsCold[rpData.roIndex];
		const ForwardLightsBlock& roDataL = *reinterpret_cast<ForwardLightsBlock*>(&m_RenderObjectsLightData[roDataC.lightsDataOffset]);

		// We're going over all things that need to be rendered in increasing
		// render queue order. Whenever we switch to the new queue, we must
		// invoke all "camera renderables" (halos, flares and so on).
		const int roQueueIndex = roDataH.queueIndex;
		DebugAssert (roQueueIndex >= currentQueueIndex);
		if( roQueueIndex > currentQueueIndex )
		{
			m_BatchRenderer.Flush();
			
			// Draw required renderables
			if (!m_Context->m_DontRenderRenderables)
			{
				while( renderablesBegin != renderablesEnd && renderablesBegin->first <= roQueueIndex )
				{
					renderablesBegin->second->RenderRenderable(*m_Context->m_CullResults);
					++renderablesBegin;
				}
			}

			currentQueueIndex = roQueueIndex;
		}

		const VisibleNode *node = roDataH.visibleNode;
		const UInt16 subsetIndex = roDataH.subsetIndex;

		ForwardShaderRenderState rs;
		{
			rs.rendererType = node->renderer->GetRendererType();
			rs.transformType = node->transformType;
			rs.invScale = roDataC.invScale;
			rs.lodFade = roDataC.lodFade;

			rs.material = roDataH.material;
			rs.shader = roDataH.shader;
			rs.subshaderIndex = roDataC.subshaderIndex;
			rs.passType = (ShaderPassType)((rpData.data >> kPackTypeShift) & kPackTypeMask);
			rs.passIndex = (rpData.data >> kPackPassShift) & kPackPassMask;

			rs.lights = &roDataL;
			#if ENABLE_SHADOWS
			rs.receiveShadows = hasAnyShadows && node->renderer->GetReceiveShadows() && IsObjectWithinShadowRange (*m_Context->m_ShadowCullData, node->worldAABB);
			#endif

			rs.lightmapIndex = roDataH.lightmapIndex;
			DebugAssert(rs.lightmapIndex == node->renderer->GetLightmapIndex());
			rs.lightmapST = node->renderer->GetLightmapSTForRendering();
			rs.customPropsHash = node->renderer->GetCustomPropertiesHash();
		}


		// multi-pass requires vertex position values to be EXACTLY the same for all passes
		// therefore do NOT batch dynamic multi-pass nodes
		// same for shadow casters
		const bool multiPass = (rpData.data & kPackMultiPassFlag) == kPackMultiPassFlag;
		const bool dynamicShouldNotBatch = (node->renderer->GetStaticBatchIndex() == 0) && (multiPass || disableDynamicBatching);

		#if ENABLE_SHADOWS
		const bool dynamicAndShadowCaster = (node->renderer->GetStaticBatchIndex() == 0) && (mainDirShadowLight != 0) && node->renderer->GetCastShadows();
		#else
		const bool dynamicAndShadowCaster = false;
		#endif

		bool shouldResetPass;
		if (rs.passType == kPassForwardAdd || // rendering multiple different lights in a row - impossible to batch
			prevRenderState != rs)
		{
			// break the batch
			m_BatchRenderer.Flush();
			prevRenderState = rs;
			shouldResetPass = true;
		}
		// We can not use dynamic batching for shadow casting renderers or multipass renderers,
		// because that will lead to zfighting due to slightly different vertex positions
		else if (dynamicAndShadowCaster || dynamicShouldNotBatch)
		{
			m_BatchRenderer.Flush();
			shouldResetPass = false;
		}
		else
			shouldResetPass = false;

		renderSettings.SetupAmbient();
		SetObjectScale(device, roDataC.lodFade, roDataC.invScale);

		node->renderer->ApplyCustomProperties(*roDataH.material, rs.shader, rs.subshaderIndex);

		// non batchable and generally inefficient multi-pass path
		if (rs.passType == kPassForwardAdd)
		{
			const int lightCount = rs.lights->addLightCount;
			const ActiveLight* const* addLights = rs.lights->GetLights();
			for( int lightNo = 0; lightNo < lightCount; ++lightNo )
			{
				const ActiveLight& activeLight = *addLights[lightNo];
				Light* light = activeLight.light;
				LightManager::SetupForwardAddLight (light, lightNo==lightCount-1 ? rs.lights->lastAddLightBlend : 1.0f);

				if (light->GetType() != kLightDirectional)
					SetLightScissorRect (activeLight.screenRect, m_Context->m_CameraViewport, false, device);


				#if ENABLE_SHADOWS
				if (rs.receiveShadows && light->GetShadows() != kShadowNone)
				{
					// find light among additional shadow lights
					ForwardShadowMaps::iterator sl, slEnd = m_ShadowMaps.end();
					for (sl = m_ShadowMaps.begin(); sl != slEnd; ++sl)
					{
						if (sl->light == &activeLight && sl->texture)
						{
							const Light& light = *activeLight.light;
							SetLightShadowProps (*m_Context->m_Camera, light, sl->texture, sl->shadowMatrix, false);
							SetShadowsKeywords (light.GetType(), light.GetShadows(), light.GetType()==kLightDirectional, enableSoftShadows);
							break;
						}
					}
				}
				#endif

				channels = rs.material->SetPassWithShader(rs.passIndex, rs.shader, rs.subshaderIndex);
				if (channels)
				{
					SetupObjectMatrix (node->worldMatrix, rs.transformType);
					node->renderer->Render( subsetIndex, *channels );
				}

				#if ENABLE_SHADOWS
				if (rs.receiveShadows && light->GetShadows() != kShadowNone)
				{
					SetNoShadowsKeywords ();
				}
				#endif

				if (light->GetType() != kLightDirectional)
					ClearScissorRect (oldScissor, oldScissorRect, device);
			}
		}
		else
		{
			// only setup lights & pass state when they're differ from previous
			if (shouldResetPass)
			{
				// only setup lights & pass state when they're differ from previous
				switch( rs.passType )
				{
					case kPassAlways:
					{
						// Disable all fixed function lights for consistency (so if user
						// has accidentally Lighting On in an Always pass, it will not produce
						// random results)
						device.DisableLights (0);

						// Reset SH lighting
						float blackSH [9][3];
						memset (blackSH, 0, (9 * 3)* sizeof(float));
						SetSHConstants (blackSH, GetGfxDevice().GetBuiltinParamValues());

						SetupObjectLightmaps (lightmapper, rs.lightmapIndex, rs.lightmapST, false);
					}
					break;

					case kPassForwardBase:
					{
						// NOTE: identity matrix has to be set for GLSL & OpenGLES before vertex lights are set
						// as lighting is specified in World space
						device.SetWorldMatrix( Matrix4x4f::identity.GetPtr() );

						LightManager::SetupForwardBaseLights (*rs.lights);
						SetupObjectLightmaps (lightmapper, rs.lightmapIndex, rs.lightmapST, false);

					#if ENABLE_SHADOWS
						if (rs.receiveShadows && mainDirShadowLight && rs.lights->mainLight == mainDirShadowLight)
						{
							const Light& light = *mainDirShadowLight->light;
							SetLightShadowProps (*m_Context->m_Camera, light, mainLightShadowMap.texture, mainLightShadowMap.shadowMatrix, false);
							SetShadowsKeywords (light.GetType(), light.GetShadows(), true, enableSoftShadows);
						}
					#endif
					}
					break;

					case kPassVertex:
					case kPassVertexLM:
					case kPassVertexLMRGBM:
					{
						// NOTE: identity matrix has to be set for GLSL & OpenGLES before vertex lights are set
						// as lighting is specified in World space
						device.SetWorldMatrix( Matrix4x4f::identity.GetPtr() );

						SetupObjectLightmaps (lightmapper, rs.lightmapIndex, rs.lightmapST, true);
						LightManager::SetupVertexLights( rs.lights->vertexLightCount, rs.lights->GetLights() );
					}
					break;

					default:
					{
						AssertString ("This pass type should not happen");
						break;
					}
				}

				channels = roDataH.material->SetPassWithShader(rs.passIndex, rs.shader, rs.subshaderIndex);
			}

			if (channels)
				m_BatchRenderer.Add(node->renderer, subsetIndex, channels, node->worldMatrix, rs.transformType);

			if (ENABLE_SHADOWS && rs.passType == kPassForwardBase)
				SetNoShadowsKeywords ();
		}
	}

	m_BatchRenderer.Flush();

	SetNoShadowsKeywords ();

	// restore scissor
	ClearScissorRect (oldScissor, oldScissorRect, device);

	#if ENABLE_SHADOWS
	if (mainLightShadowMap.texture && mainLightShadowMap.texture != existingShadowMap)
		GetRenderBufferManager().ReleaseTempBuffer( mainLightShadowMap.texture );
	for (ForwardShadowMaps::iterator it = m_ShadowMaps.begin(), itEnd = m_ShadowMaps.end(); it != itEnd; ++it)
	{
		ForwardShadowMap& sl = *it;
		if (sl.texture)
			GetRenderBufferManager().ReleaseTempBuffer (sl.texture);
	}
	#endif

	// After everything we might still have renderables that should be drawn and the
	// very end. Do it.
	if (!m_Context->m_DontRenderRenderables)
	{
		while (renderablesBegin != renderablesEnd && renderablesBegin->first < m_Context->m_RenderQueueStart)
			++renderablesBegin;
		while( renderablesBegin != renderablesEnd && renderablesBegin->first < m_Context->m_RenderQueueEnd )
		{
			renderablesBegin->second->RenderRenderable(*m_Context->m_CullResults);
			++renderablesBegin;
		}
	}
	GetGfxDevice().SetSRGBWrite(false);
	device.SetViewMatrix( m_Context->m_CurCameraMatrix.GetPtr() );
}


// ------------------------------------------------------------------------
//  collect cascaded shadows into screen-space texture; apply blur

#if ENABLE_SHADOWS

struct ShadowCollectorSorter
{
	bool operator() (int raIndex, int rbIndex) const;
	const ForwardShaderRenderLoop* queue;
};

bool ShadowCollectorSorter::operator()(int raIndex, int rbIndex) const
{
	const RenderObjectData& ra = (*queue->m_Objects)[raIndex];
	const RenderObjectData& rb = (*queue->m_Objects)[rbIndex];

	// Sort by layering depth. //@TODO:should this be here?
	bool globalLayeringResult;
	if (CompareGlobalLayeringData(ra.globalLayeringData, rb.globalLayeringData, globalLayeringResult))
		return globalLayeringResult;

	// Sort front to back
	return ra.distance > rb.distance;
}

struct CompactShadowCollectorSortData
{
	UInt64 key;			// 64b key, stores full 32b material instance ID, 16b internal static batch ID, 2b for transform type, and 14b depth
	int collectorIndex;

	CompactShadowCollectorSortData(UInt32 _smallMeshIndex, UInt32 _instanceID, TransformType _transformType, float _depth, int _collectorIndex )
	{
		key=0;
		UInt32 transformType = static_cast<UInt32>(_transformType);
		UInt32 z = (UInt32)(16383.0f*_depth);

		key |= (_instanceID);
		key = key << 32;
		key |= ((_smallMeshIndex&0x0000ffff)<<16)|((transformType&0x00000003)<<14)|(z&0x00003fff);

		collectorIndex = _collectorIndex;
	}
};

struct CompactShadowCollectorKeySorter
{
	inline bool operator()(const CompactShadowCollectorSortData& a, const CompactShadowCollectorSortData& b)
	{
		return a.key < b.key;
	}
};

// Shadow collector sorting
// Sorted shadow collector order is stored into m_ReceiverObjects
// Output:
//		_resultOrder		- Sorted shadow caster sort data
// Returns:
//		Number of active collectors
int ForwardShaderRenderLoop::SortShadowCollectorsCompact(CompactShadowCollectorSortData* _resultOrder)
{
	int activeShadowCollectors = 0;

	// Generate key array for sorting
	for( int i = 0; i < m_ReceiverObjects.size(); ++i )
	{
		int roIndex = m_ReceiverObjects[i];
		const RenderObjectData& roDataH = (*m_Objects)[roIndex];
		Shader* shader = roDataH.shader;

		if( shader->HasShadowCollectorPass() )
		{
			const TransformInfo& xformInfo = roDataH.visibleNode->renderer->GetTransformInfo();

			Matrix4x4f worldToClipMatrix = m_Context->m_Camera->GetWorldToClipMatrix();
			const Vector3f& worldPos = roDataH.visibleNode->worldAABB.GetCenter();
			float z = worldToClipMatrix.Get (2, 0) * worldPos.x + worldToClipMatrix.Get (2, 1) * worldPos.y + worldToClipMatrix.Get (2, 2) * worldPos.z + worldToClipMatrix.Get (2, 3);
			float w = worldToClipMatrix.Get (3, 0) * worldPos.x + worldToClipMatrix.Get (3, 1) * worldPos.y + worldToClipMatrix.Get (3, 2) * worldPos.z + worldToClipMatrix.Get (3, 3);
			float z_proj = z/w;
			z_proj = max(z_proj,0.0f);
			z_proj = min(z_proj,1.0f);

			_resultOrder[activeShadowCollectors++] = CompactShadowCollectorSortData( roDataH.visibleNode->renderer->GetMeshIDSmall(), roDataH.material->GetShadowCollectorHash(),
				xformInfo.transformType, z_proj, roIndex );

		}
	}

	std::sort( _resultOrder, _resultOrder + activeShadowCollectors, CompactShadowCollectorKeySorter() );

	return activeShadowCollectors;
}

RenderTexture* ForwardShaderRenderLoop::CollectShadows (RenderTexture* inputShadowMap, const Light* light, const Matrix4x4f* shadowMatrices, const float* splitDistances, const Vector4f* splitSphereCentersAndSquaredRadii, bool enableSoftShadows, bool useDualInForward, bool clearFrameBuffer)
{
	PROFILER_AUTO_GFX(gFwdOpaqueCollectShadows, m_Context->m_Camera)
		GPU_AUTO_SECTION(kGPUSectionShadowPass);

	DebugAssert (shadowMatrices && inputShadowMap && light && splitDistances);

	//Sort shadow collectors
#if GFX_ENABLE_SHADOW_BATCHING
	CompactShadowCollectorSortData* sortOrder;
	ALLOC_TEMP(sortOrder, CompactShadowCollectorSortData, m_ReceiverObjects.size());
	int shadowColectors = SortShadowCollectorsCompact(sortOrder);
#else
	ShadowCollectorSorter sorter;
	sorter.queue = this;
	std::sort (m_ReceiverObjects.begin(), m_ReceiverObjects.end(), sorter);
#endif

	// If camera is rendering into a texture, we can share its depth buffer while collecting shadows.
	// This doesn't apply if the target texture is antialiased (case 559079).
	bool shareDepthBuffer = false;
	RenderTexture* cameraRT = m_Context->m_Camera->GetCurrentTargetTexture();
	if (cameraRT && cameraRT->GetDepthFormat() != kDepthFormatNone && !cameraRT->IsAntiAliased())
	{
		shareDepthBuffer = true;
		if (!cameraRT->IsCreated())
			cameraRT->Create();
	}

	// create screen-space render texture and collect shadows into it
	RenderTexture* screenShadowMap = GetRenderBufferManager().GetTempBuffer (RenderBufferManager::kFullSize, RenderBufferManager::kFullSize, shareDepthBuffer ? kDepthFormatNone : kDepthFormat24, kRTFormatARGB32, 0, kRTReadWriteLinear);
	if (shareDepthBuffer)
	{
		if (!screenShadowMap->IsCreated())
			screenShadowMap->Create();
		RenderSurfaceHandle rtSurfaceColor = screenShadowMap->GetColorSurfaceHandle();
		RenderSurfaceHandle rtSurfaceDepth = cameraRT->GetDepthSurfaceHandle();
		RenderTexture::SetActive (1, &rtSurfaceColor, rtSurfaceDepth, screenShadowMap);
	}
	else
	{
		RenderTexture::SetActive (screenShadowMap);
	}

	GfxDevice& device = GetGfxDevice();
	// (case 555375)
	// Clear all, expect in cases where depth buffer is shared and forward path is used to render deferred path shadow receiving objects
	// (clearFrameBuffer variable is false in those cases)
	bool clearColorOnly = shareDepthBuffer && !clearFrameBuffer;
	device.Clear (clearColorOnly ? kGfxClearColor : kGfxClearAll, ColorRGBAf(1,1,1,0).GetPtr(), 1.0f, 0);
	if (clearColorOnly)
		device.IgnoreNextUnresolveOnCurrentRenderTarget();
	GPU_TIMESTAMP();
	m_Context->m_Camera->SetupRender ();

	SetLightShadowCollectProps (*m_Context->m_Camera, *light, inputShadowMap, shadowMatrices, splitDistances, splitSphereCentersAndSquaredRadii, useDualInForward);
	light->SetPropsToShaderLab (1.0f);

	device.SetViewMatrix( m_Context->m_CurCameraMatrix.GetPtr() );

#if GFX_ENABLE_SHADOW_BATCHING

	device.SetInverseScale(1.0f);
	m_BatchRenderer.Flush();

	if (shadowColectors > 0)
	{
		UInt64 previousKey = ((sortOrder[0].key)&0xFFFFFFFFFFFFC000ULL); // depth component does not affect state change boundaries
		UInt32 previousHash = 0;
		int roIndex = sortOrder[0].collectorIndex;
		const ChannelAssigns* channels = (*m_Objects)[roIndex].material->SetShadowCollectorPassWithShader ((*m_Objects)[roIndex].shader, m_RenderObjectsCold[roIndex].subshaderIndex);

		for(int i=0; i<shadowColectors;i++)
		{
			UInt64 currentKey = ((sortOrder[i].key)&0xFFFFFFFFFFFFC000ULL);

			roIndex = sortOrder[i].collectorIndex;
			const RenderObjectData& roDataH = (*m_Objects)[roIndex];
			Shader* shader = roDataH.shader;
			const TransformInfo& xformInfo = roDataH.visibleNode->renderer->GetTransformInfo ();
			const RenderObjectDataCold& roDataC = m_RenderObjectsCold[roIndex];

			roDataH.visibleNode->renderer->ApplyCustomProperties(*roDataH.material, shader, roDataC.subshaderIndex);

			UInt32 currentHash = roDataH.visibleNode->renderer->GetCustomPropertiesHash();

			// different property hasah or shared depth buffer cause Flush(), state setup, and one non-batched draw call
			if (currentHash != previousHash || shareDepthBuffer)
			{
				m_BatchRenderer.Flush();	// empty BatchRenderer
				channels = roDataH.material->SetShadowCollectorPassWithShader(shader, roDataC.subshaderIndex);
				SetupObjectMatrix(xformInfo.worldMatrix, xformInfo.transformType);
				roDataH.visibleNode->renderer->Render( roDataH.subsetIndex, *channels );
			}
			else
			{
				if (previousKey != currentKey)	// Flush() and update state when key changes
				{
					m_BatchRenderer.Flush();
					channels = roDataH.material->SetShadowCollectorPassWithShader (shader, roDataC.subshaderIndex);
				}

				// if this pass needs to be rendered
				if (channels)
					m_BatchRenderer.Add(roDataH.visibleNode->renderer, roDataH.subsetIndex, channels, xformInfo.worldMatrix, xformInfo.transformType);
			}
			previousKey = currentKey;
			previousHash = currentHash;
		}
		m_BatchRenderer.Flush();
	}

#else // GFX_ENABLE_SHADOW_BATCHING

	size_t npasses = m_ReceiverObjects.size();
	for( size_t i = 0; i < npasses; ++i )
	{
		int roIndex = m_ReceiverObjects[i];
		const RenderObjectData& roDataH = (*m_Objects)[roIndex];
		const RenderObjectDataCold& roDataC = m_RenderObjectsCold[roIndex];

		Shader* shader = roDataH.shader;
		if( !shader->HasShadowCollectorPass() )
			continue;

		const VisibleNode* node = roDataH.visibleNode;
		BaseRenderer* renderer = node->renderer;
		SetObjectScale(device, roDataC.lodFade, roDataC.invScale);

		renderer->ApplyCustomProperties(*roDataH.material, shader, roDataC.subshaderIndex);

		const ChannelAssigns* channels = roDataH.material->SetShadowCollectorPassWithShader(shader, roDataC.subshaderIndex);
		SetupObjectMatrix (node->worldMatrix, node->transformType);
		renderer->Render( roDataH.subsetIndex, *channels );
	}

#endif	// GFX_ENABLE_SHADOW_BATCHING

	GetRenderBufferManager().ReleaseTempBuffer( inputShadowMap );

	//
	// possibly blur into another screen-space render texture

	if( IsSoftShadow(light->GetShadows()) && enableSoftShadows )
	{
		return BlurScreenShadowMap (screenShadowMap, light->GetShadows(), m_Context->m_Camera->GetFar(), light->GetShadowSoftness(), light->GetShadowSoftnessFade());
	}

	return screenShadowMap;
}
#endif // ENABLE_SHADOWS

// ------------------------------------------------------------------------
//  render shadow maps for a single light


#if ENABLE_SHADOWS

void ForwardShaderRenderLoop::RenderLightShadowMaps (ForwardShadowMap& shadowMap, ShadowCameraData& camData, bool enableSoftShadows, bool useDualInForward, bool clearFrameBuffer)
{
	// Set correct keywords before rendering casters (caster passes use keywords for shader selection)
	const Light* light = shadowMap.light->light;
	SetShadowsKeywords( light->GetType(), light->GetShadows(), false, enableSoftShadows );
	GetGfxDevice().SetViewMatrix( m_Context->m_CurCameraMatrix.GetPtr() );

	Matrix4x4f shadowMatrices[kMaxShadowCascades];

#if UNITY_EDITOR
	bool useLightmaps = GetLightmapVisualization ().GetUseLightmapsForRendering ();
#else
	bool useLightmaps = true;
#endif

	bool excludeLightmapped = !useDualInForward && useLightmaps;
	shadowMap.texture = RenderShadowMaps (camData, *shadowMap.light, shadowMap.receiverBounds, excludeLightmapped, shadowMatrices);
	CopyMatrix (shadowMatrices[0].GetPtr(), shadowMap.shadowMatrix.GetPtr());

	// Shadow map can be null if out of memory; or no shadow casters present
	if (gGraphicsCaps.hasShadowCollectorPass && shadowMap.texture && light->GetType() == kLightDirectional)
	{
		SetShadowsKeywords( light->GetType(), light->GetShadows(), false, enableSoftShadows );
		shadowMap.texture = CollectShadows (shadowMap.texture, light, shadowMatrices, camData.splitDistances, camData.splitSphereCentersAndSquaredRadii, enableSoftShadows, useDualInForward, clearFrameBuffer);
	}
	else
	{
		// If shadow map could not actually be created (out of VRAM, whatever), set the no shadows
		// keywords and proceed. So there will be no shadows, but otherwise it will be ok.
		SetNoShadowsKeywords();
	}
}

#endif // ENABLE_SHADOWS


// ------------------------------------------------------------------------
//  rendering entry points

ForwardShaderRenderLoop* CreateForwardShaderRenderLoop()
{
	return new ForwardShaderRenderLoop();
}

void DeleteForwardShaderRenderLoop (ForwardShaderRenderLoop* queue)
{
	delete queue;
}

static bool IsPassSuitable (UInt32 currentRenderOptions, UInt32 passRenderOptions, ShaderPassType passType,
							 bool isLightmapped, bool useRGBM, bool useVertexLights, bool hasAddLights)
{
	// All options that a pass requires must be on
	if( (currentRenderOptions & passRenderOptions) != passRenderOptions )
		return false; // some options are off, skip this pass

	if (useVertexLights)
	{
		if (passType != kPassAlways && passType != kPassVertex &&
			passType != kPassVertexLM && passType != kPassVertexLMRGBM)
			return false;

		// Use either lightmapped or non-lightmapped pass
		if ((passType == kPassVertex && isLightmapped) ||
			((passType == kPassVertexLM || passType == kPassVertexLMRGBM) && !isLightmapped))
			return false;

		// Use pass that can properly decode the lightmap
		if ((passType == kPassVertexLM && useRGBM) ||
			(passType == kPassVertexLMRGBM && !useRGBM))
			return false;
	}
	else
	{
		if (passType != kPassAlways && passType != kPassForwardBase && passType != kPassForwardAdd)
			return false; // pass does not belong to forward loop

		if (!hasAddLights && passType == kPassForwardAdd)
			return false; // additive pass but have no additive lights
	}
	return true;
}

// A point or spot light might be completely behind shadow distance,
// so there's no point in doing shadows on them.
static bool IsLightBeyondShadowDistance (const Light& light, const Matrix4x4f& cameraMatrix, float shadowDistance)
{
	if (light.GetType() == kLightDirectional)
		return false;
	const Vector3f lightPos = light.GetComponent(Transform).GetPosition();
	float distanceToLight = -cameraMatrix.MultiplyPoint3 (lightPos).z;
	if (distanceToLight - light.GetRange() > shadowDistance)
		return true;
	return false;
}


static void PutAdditionalShadowLights (const AABB& bounds, ForwardLightsBlock& lights, const Matrix4x4f& cameraMatrix, float shadowDistance, ForwardShadowMaps& outShadowMaps)
{
	const int lightCount = lights.addLightCount;
	const ActiveLight* const* addLights = lights.GetLights();
	for (int lightNo = 0; lightNo < lightCount; ++lightNo)
	{
		const ActiveLight* light = addLights[lightNo];
		if (light->light->GetShadows() == kShadowNone)
			continue;

		// Find this light's shadow data
		ForwardShadowMaps::iterator sl, slEnd = outShadowMaps.end();
		ForwardShadowMap* found = NULL;
		for (sl = outShadowMaps.begin(); sl != slEnd; ++sl)
		{
			if (sl->light == light)
			{
				found = &(*sl);
				break;
			}
		}
		if (sl == slEnd)
		{
			// Point/Spot light beyond shadow distance: no need to add
			if (IsLightBeyondShadowDistance (*light->light, cameraMatrix, shadowDistance))
				continue;

			ForwardShadowMap& shadowMap = outShadowMaps.push_back ();
			shadowMap.light = light;
			shadowMap.receiverBounds = bounds;
			shadowMap.texture = NULL;
		}
		else
		{
			found->receiverBounds.Encapsulate (bounds);
		}
	}
}

#if ENABLE_FORWARD_SHADER_LOOP_HASH_SORTING
template<typename T>
static UInt8* InsertIntoHashBuffer(const T* p, UInt8* buffer)
{
	Assert((sizeof(T) % 4) == 0);	// unaligned write
	*reinterpret_cast<T*>(buffer) = *p;
	return buffer + sizeof(T);
}
#endif

void DoForwardShaderRenderLoop (
	RenderLoopContext& ctx,
	RenderObjectDataContainer& objects,
	bool opaque,
	bool disableDynamicBatching,
	RenderTexture* mainShadowMap,
	ActiveLights& activeLights,
	bool linearLighting,
	bool clearFrameBuffer)
{
	GPU_AUTO_SECTION(opaque ? kGPUSectionOpaquePass : kGPUSectionTransparentPass);
	using namespace ForwardShaderRenderLoop_Enum;

	const QualitySettings::QualitySetting& quality = GetQualitySettings().GetCurrent();

	// figure out hardware supports shadows
	#if ENABLE_SHADOWS
	float shadowDistance = QualitySettings::GetShadowDistanceForRendering();
	const bool receiveShadows =
		opaque &&
		GetBuildSettings().hasShadows &&
		CheckPlatformSupportsShadows() &&
		(quality.shadows != QualitySettings::kShadowsDisable) &&
		(shadowDistance > 0.0f);
	const bool localLightShadows = receiveShadows && GetBuildSettings().hasLocalLightShadows;
	#endif

	bool useRGBM = gGraphicsCaps.SupportsRGBM();

	// Allocated on the stack each time, uses temp allocators
	ForwardShaderRenderLoop queue;
	queue.m_Context = &ctx;
	queue.m_Objects = &objects;
	queue.m_RenderObjectsCold.reserve(objects.size());
	const int kEstimatedLightDataPerObject = sizeof(ForwardLightsBlock) + kEstimatedLightsPerObject * sizeof(Light*);
	queue.m_RenderObjectsLightData.reserve(objects.size() * kEstimatedLightDataPerObject);

	const ActiveLight* mainDirShadowLight = NULL;

	// figure out current rendering options
	UInt32 currentRenderOptions = GetCurrentRenderOptions ();

	RenderSettings& renderSettings = GetRenderSettings();
	LightManager& lightManager = GetLightManager();
	const int pixelLightCount = quality.pixelLightCount;
	const bool dualLightmapsMode = (GetLightmapSettings().GetLightmapsMode() == LightmapSettings::kDualLightmapsMode);

#if UNITY_EDITOR
	bool useLightmaps = GetLightmapVisualization ().GetUseLightmapsForRendering ();
#endif

	const CullResults& cullResults = *ctx.m_CullResults;

	// Figure everything out
	{
		PROFILER_AUTO((opaque?gFwdOpaquePrepare:gFwdAlphaPrepare), ctx.m_Camera);

		RenderObjectDataContainer::iterator itEnd = objects.end();
		size_t roIndex = 0;
		for (RenderObjectDataContainer::iterator it = objects.begin(); it != itEnd; ++it, ++roIndex)
		{
			RenderObjectData& odata = *it;
			const VisibleNode *node = odata.visibleNode;
			size_t visibleNodeIndex = node - cullResults.nodes.begin();

			BaseRenderer* renderer = node->renderer;

#if UNITY_EDITOR
			const bool isLightmapped = renderer->IsLightmappedForRendering() && useLightmaps;
#else
			const bool isLightmapped = renderer->IsLightmappedForRendering();
#endif

			ShaderLab::IntShader& slshader = *odata.shader->GetShaderLabShader();
			RenderObjectDataCold& roDataC = queue.m_RenderObjectsCold.push_back();
			bool useVertexLights = false;
			if (odata.subShaderIndex == -1)
			{
				int ss = slshader.GetDefaultSubshaderIndex (kRenderPathExtForward);
				if (ss == -1)
				{
					ss = slshader.GetDefaultSubshaderIndex (isLightmapped ? kRenderPathExtVertexLM : kRenderPathExtVertex);
					useVertexLights = true;
				}
				if (ss == -1)
					continue;
				roDataC.subshaderIndex = ss;
			}
			else
			{
				roDataC.subshaderIndex = odata.subShaderIndex;
			}
			ShaderLab::SubShader& subshader = slshader.GetSubShader(roDataC.subshaderIndex);

			bool disableAddLights = false;
			if (!useVertexLights)
			{
				// If we only have ForwardBase pass and no ForwardAdd,
				// disable additive lights completely. Only support main directional,
				// vertex & SH.
				disableAddLights = !subshader.GetSupportsForwardAddLights();
			}

			size_t objectLightsOffset = queue.m_RenderObjectsLightData.size();
			roDataC.lightsDataOffset = objectLightsOffset;

			lightManager.FindForwardLightsForObject (
				queue.m_RenderObjectsLightData,
				GetObjectLightIndices(cullResults, visibleNodeIndex),
				GetObjectLightCount(cullResults, visibleNodeIndex),
				activeLights,
				*node,
				isLightmapped,
				dualLightmapsMode,
				useVertexLights,
				pixelLightCount,
				disableAddLights,
				renderSettings.GetAmbientLightInActiveColorSpace());

			ForwardLightsBlock& roDataL = *reinterpret_cast<ForwardLightsBlock*>(&queue.m_RenderObjectsLightData[objectLightsOffset]);
			const bool hasAddLights = (roDataL.addLightCount != 0);

			#if ENABLE_SHADOWS
			bool objectReceivesShadows = renderer->GetReceiveShadows();
			bool withinShadowDistance = IsObjectWithinShadowRange (*ctx.m_ShadowCullData, node->worldAABB);
			if (receiveShadows && withinShadowDistance)
			{
				queue.m_ReceiverObjects.push_back (roIndex);

				if (objectReceivesShadows)
				{
					// deal with main directional shadow light
					if (roDataL.mainLight && roDataL.mainLight->light->GetShadows() != kShadowNone)
					{
						if (!mainDirShadowLight)
							mainDirShadowLight = roDataL.mainLight;
						if (mainDirShadowLight == roDataL.mainLight)
							queue.m_MainShadowMap.receiverBounds.Encapsulate (node->worldAABB);
					}

					// deal with additive shadow lights if needed
					if (localLightShadows && subshader.GetSupportsFullForwardShadows())
					{
						PutAdditionalShadowLights (node->worldAABB, roDataL, ctx.m_CurCameraMatrix, shadowDistance, queue.m_ShadowMaps);
					}
				}
			}
			#endif

			roDataC.invScale = node->invScale;
			roDataC.lodFade = node->lodFade;

			int shaderPassCount = subshader.GetValidPassCount();

			// Determine if we will need more than a single pass
			int suitablePasses = 0;
			for( int pass = 0; pass < shaderPassCount && suitablePasses < 2; ++pass )
			{
				ShaderPassType passType; UInt32 passRenderOptions;
				subshader.GetPass(pass)->GetPassOptions( passType, passRenderOptions );

				if (IsPassSuitable (currentRenderOptions, passRenderOptions, passType, isLightmapped, useRGBM, useVertexLights, hasAddLights))
					++suitablePasses;
			}

			// Go over all passes in the shader
			UInt32 firstPassFlag = kPackFirstPassFlag;
			const UInt32 multiPassFlag = (suitablePasses > 1)? kPackMultiPassFlag: 0;
			for( int pass = 0; pass < shaderPassCount; ++pass )
			{
				ShaderPassType passType; UInt32 passRenderOptions;
				subshader.GetPass(pass)->GetPassOptions( passType, passRenderOptions );

				if (!IsPassSuitable (currentRenderOptions, passRenderOptions, passType, isLightmapped, useRGBM, useVertexLights, hasAddLights))
					continue; // skip this pass

				RenderPassData rpData;
				rpData.roIndex = roIndex;
				rpData.data =
					((pass & kPackPassMask) << kPackPassShift) |
					(passType << kPackTypeShift) |
					firstPassFlag |
					multiPassFlag;

#if ENABLE_FORWARD_SHADER_LOOP_HASH_SORTING

				//hash state information for render object sorter
				const int kHashBufferSize = 64;
				UInt8 hashBuffer[kHashBufferSize];
				UInt8* hashPtr = hashBuffer;

				// Always write 32b granularity into the hash buffer to avoid unaligned writes
				if (opaque)
					hashPtr = InsertIntoHashBuffer(&node->invScale, hashPtr);
				int materialID = odata.material->GetInstanceID();
				hashPtr = InsertIntoHashBuffer(&materialID, hashPtr);
				hashPtr = InsertIntoHashBuffer(&roDataC.subshaderIndex, hashPtr);
				UInt32 shaderPassType = (ShaderPassType)((rpData.data >> kPackTypeShift) & kPackTypeMask);
				hashPtr = InsertIntoHashBuffer(&shaderPassType, hashPtr);
				UInt32 passIndex = (rpData.data >> kPackPassShift) & kPackPassMask;
				hashPtr = InsertIntoHashBuffer(&passIndex, hashPtr);
#if GFX_ENABLE_DRAW_CALL_BATCHING
				hashPtr = InsertIntoHashBuffer(&odata.staticBatchIndex, hashPtr);
#endif
				Assert(hashPtr-hashBuffer <= kHashBufferSize);

				rpData.hash = MurmurHash2A(hashBuffer, hashPtr-hashBuffer, 0x9747b28c);
#endif
				queue.m_PlainRenderPasses.push_back( rpData );

				firstPassFlag = 0;
			}
		}
	}

	// sort everything
	{
		PROFILER_AUTO((opaque?gFwdOpaqueSort:gFwdAlphaSort), ctx.m_Camera);
		if (opaque)
			queue.SortRenderPassData<true> (queue.m_PlainRenderPasses);
		else
			queue.SortRenderPassData<false> (queue.m_PlainRenderPasses);
	}

	// Render everything. When transitioning to render queues,
	// it will invoke camera renderables (halos, and so on)
	{
		PROFILER_AUTO_GFX((opaque?gFwdOpaqueRender:gFwdAlphaRender), ctx.m_Camera);
		RenderTexture* rtMain = ctx.m_Camera->GetCurrentTargetTexture ();
		queue.PerformRendering (mainDirShadowLight, mainShadowMap, *ctx.m_ShadowCullData, disableDynamicBatching, linearLighting && (!rtMain || rtMain->GetSRGBReadWrite()), clearFrameBuffer);
	}
}
