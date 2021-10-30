#include "UnityPrefix.h"
#include "RenderLoopPrivate.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Graphics/Transform.h"
#include "External/shaderlab/Library/intshader.h"
#include "Runtime/Camera/Renderable.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "RenderLoop.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/GfxDevice/BatchRendering.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Camera/LightManager.h"
#if UNITY_EDITOR
#include "Editor/Src/LightmapVisualization.h"
#endif
#include "BuiltinShaderParamUtility.h"
#include "External/MurmurHash/MurmurHash2.h"

// Enable/disable hash based forward shader render loop sorting functionality.
#define ENABLE_VERTEX_LOOP_HASH_SORTING 0

static inline bool CompareLights (VertexLightsBlock const* a, VertexLightsBlock const* b)
{
	if (!a || !b)
		return false;

	if (a->lightCount != b->lightCount)
		return false;

	const ActiveLight* const* lightsA = a->GetLights();
	const ActiveLight* const* lightsB = b->GetLights();
	for (int i = 0; i < a->lightCount; ++i)
		if (lightsA[i] != lightsB[i])
			return false;

	return true;
}


struct RODataVLit {
	// help the compiler here a bit...
	RODataVLit() { }
	RODataVLit( const RODataVLit& rhs ) { memcpy(this, &rhs, sizeof(*this)); }

	float		invScale;						// 4
	float		lodFade;						// 4
	size_t		lightsDataOffset;				// 4	into memory block with all light data chunks
	int			subshaderIndex;					// 4

	// 16 bytes
};

namespace ForwardVertexRenderLoop_Enum
{
// Render pass data here is 8 bytes each; an index of the render object and "the rest" packed
// into 4 bytes.
enum {
	kPackPassShift = 0,
	kPackPassMask = 0xFF,
	kPackFirstPassFlag = (1<<16),
	kPackMultiPassFlag = (1<<17),
};
} // namespace ForwardVertexRenderLoop_Enum

struct RPDataVLit {
	int	roIndex;
	// Packed into UInt32: pass number, first pass flag
	UInt32 data;
#if ENABLE_VERTEX_LOOP_HASH_SORTING
	UInt32 hash;
#endif
};
typedef dynamic_array<RPDataVLit> RenderPassesVLit;


struct ForwardVertexRenderState
{
	int rendererType;
	int transformType;
	float invScale;
	float lodFade;
	
	Material* material;
	Shader* shader;
	int subshaderIndex;
	int passIndex;
	
	const VertexLightsBlock* lights;
	
	int lightmapIndex;
	Vector4f lightmapST;

	UInt32 customPropsHash;
	
	void Invalidate()
	{
		rendererType = -1;
		transformType = -1;
		invScale = 0.0f;
		lodFade = 0.0f;
		material = 0; shader = 0; subshaderIndex = -1; passIndex = -1;
		lights = 0;
		lightmapIndex = -1; lightmapST = Vector4f(0,0,0,0);
		customPropsHash = 0;
	}

	bool operator == (const ForwardVertexRenderState& rhs) const
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
				passIndex == rhs.passIndex &&
				CompareApproximately(invScale,rhs.invScale) &&
				CompareApproximately(lodFade,rhs.lodFade, LOD_FADE_BATCH_EPSILON) &&
				lightmapIndex == rhs.lightmapIndex &&
				CompareMemory(lightmapST, rhs.lightmapST) &&
				customPropsHash == rhs.customPropsHash);
	}	
	
	bool operator != (const ForwardVertexRenderState& rhs) const
	{
		return !(rhs == *this);
	}
};
	

struct ForwardVertexRenderLoop
{
	ForwardVertexRenderLoop()
		: m_RenderObjectsCold		(kMemTempAlloc)
		, m_RenderObjectsLightData	(kMemTempAlloc)
		, m_PlainRenderPasses		(kMemTempAlloc)
	{ }

	const RenderLoopContext*	m_Context;
	RenderObjectDataContainer*	m_Objects;
	dynamic_array<RODataVLit>	m_RenderObjectsCold;
	dynamic_array<UInt8>		m_RenderObjectsLightData;
	RenderPassesVLit			m_PlainRenderPasses;
	BatchRenderer				m_BatchRenderer;

	void PerformRendering (bool sSRGBRenderTarget, bool clearFrameBuffer);

	template<bool opaque>
	struct RenderObjectSorter
	{
		bool operator()( const RPDataVLit& ra, const RPDataVLit& rb ) const;
		const ForwardVertexRenderLoop* queue;
	};
	template<bool opaque>
	void SortRenderPassData( RenderPassesVLit& passes )
	{
		RenderObjectSorter<opaque> sorter;
		sorter.queue = this;
		std::sort( passes.begin(), passes.end(), sorter );
	}
};


template<bool opaque>
bool ForwardVertexRenderLoop::RenderObjectSorter<opaque>::operator() (const RPDataVLit& ra, const RPDataVLit& rb) const
{
	using namespace ForwardVertexRenderLoop_Enum;

	const RenderObjectData& dataa = (*queue->m_Objects)[ra.roIndex];
	const RenderObjectData& datab = (*queue->m_Objects)[rb.roIndex];

	// Sort by layering depth.
	bool globalLayeringResult;
	if (CompareGlobalLayeringData(dataa.globalLayeringData, datab.globalLayeringData, globalLayeringResult))
		return globalLayeringResult;

#if ENABLE_VERTEX_LOOP_HASH_SORTING

	// Sort by render queues first
	if( dataa.queueIndex != datab.queueIndex )
		return dataa.queueIndex < datab.queueIndex;

	if (opaque) {
		DebugAssertIf (dataa.queueIndex < kQueueIndexMin || dataa.queueIndex > kGeometryQueueIndexMax); // this is opaque loop!
	} else {
		DebugAssertIf (dataa.queueIndex >= kQueueIndexMin && dataa.queueIndex <= kGeometryQueueIndexMax); // this is alpha loop!
	}

	if (!opaque)
	{
		if( dataa.distance != datab.distance )
			return dataa.distance < datab.distance;
	}

	UInt32 flagsa = ra.data;
	UInt32 flagsb = rb.data;

	// render all first passes first
	if( (flagsa & kPackFirstPassFlag) != (flagsb & kPackFirstPassFlag) )
		return (flagsa & kPackFirstPassFlag) > (flagsb & kPackFirstPassFlag);

	if (ra.hash != rb.hash)
		return ra.hash < rb.hash;

	// then sort by material
	if( dataa.material != datab.material )
		return dataa.material->GetInstanceID() < datab.material->GetInstanceID(); // just compare instance IDs

	// inside same material: by pass
	UInt32 passa = (flagsa >> kPackPassShift) & kPackPassMask;
	UInt32 passb = (flagsb >> kPackPassShift) & kPackPassMask;
	if( passa != passb )
		return passa < passb;

	// Sort by distance in reverse order.
	// That way we get consistency in render order, and more pixels not rendered due to z-testing,
	// which benefits performance.
	if (opaque)
	{
		if( dataa.distance != datab.distance )
			return dataa.distance > datab.distance;
	}

	// fall through: roIndex
	return ra.roIndex < rb.roIndex;

#else

	// Sort by render queues first
	if( dataa.queueIndex != datab.queueIndex )
		return dataa.queueIndex < datab.queueIndex;

	if (opaque) {
		DebugAssertIf (dataa.queueIndex < kQueueIndexMin || dataa.queueIndex > kGeometryQueueIndexMax); // this is opaque loop!
	} else {
		DebugAssertIf (dataa.queueIndex >= kQueueIndexMin && dataa.queueIndex <= kGeometryQueueIndexMax); // this is alpha loop!
	}

	if (!opaque)
	{
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
	if( dataa.staticBatchIndex != datab.staticBatchIndex )
		return dataa.staticBatchIndex < datab.staticBatchIndex;

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

	// then sort by material
	if( dataa.material != datab.material )
		return dataa.material->GetInstanceID() < datab.material->GetInstanceID(); // just compare instance IDs

	// inside same material: by pass
	UInt32 passa = (flagsa >> kPackPassShift) & kPackPassMask;
	UInt32 passb = (flagsb >> kPackPassShift) & kPackPassMask;
	if( passa != passb )
		return passa < passb;

	// Sort by distance in reverse order.
	// That way we get consistency in render order, and more pixels not rendered due to z-testing,
	// which benefits performance.
	if (opaque)
	{
		if( dataa.distance != datab.distance )
			return dataa.distance > datab.distance;
	}

	// fall through: roIndex
	return ra.roIndex < rb.roIndex;

#endif
}

void ForwardVertexRenderLoop::PerformRendering (bool sSRGBRenderTarget, bool clearFrameBuffer)
{
	using namespace ForwardVertexRenderLoop_Enum;

	GfxDevice& device = GetGfxDevice();
	const RenderSettings& renderSettings = GetRenderSettings();

	const RenderManager::Renderables& renderables = GetRenderManager ().GetRenderables ();
	RenderManager::Renderables::const_iterator renderablesBegin = renderables.begin(), renderablesEnd = renderables.end();

	const LightmapSettings& lightmapper = GetLightmapSettings();

	size_t npasses = m_PlainRenderPasses.size();

	int currentQueueIndex = m_Context->m_RenderQueueStart;
	device.SetViewMatrix( m_Context->m_CurCameraMatrix.GetPtr() );
	
	ForwardVertexRenderState prevRenderState;
	prevRenderState.Invalidate();

	// SRGB read/write for vertexRenderLoop
	device.SetSRGBWrite(sSRGBRenderTarget);
	if (clearFrameBuffer)
		m_Context->m_Camera->ClearNoSkybox(false);
	
	const ChannelAssigns* channels = NULL;
	int canBatch = 0;
	StartRenderLoop();
	for( size_t i = 0; i < npasses; ++i )
	{
		const RPDataVLit& rpData = m_PlainRenderPasses[i];
		DebugAssertIf (rpData.roIndex < 0 || rpData.roIndex >= m_Objects->size() || rpData.roIndex >= m_RenderObjectsCold.size());
		const RenderObjectData& roDataH = (*m_Objects)[rpData.roIndex];
		const RODataVLit& roDataC = m_RenderObjectsCold[rpData.roIndex];

		const VertexLightsBlock& roDataL = *reinterpret_cast<VertexLightsBlock*>(&m_RenderObjectsLightData[roDataC.lightsDataOffset]);

		const int roQueueIndex = roDataH.queueIndex;
		DebugAssertIf( roQueueIndex < currentQueueIndex );
		if( roQueueIndex > currentQueueIndex )
		{
			m_BatchRenderer.Flush();
			canBatch = 0;
			EndRenderLoop();
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
			StartRenderLoop();
		}

		const VisibleNode* node = roDataH.visibleNode;
		const UInt16 subsetIndex = roDataH.subsetIndex;
		
		ForwardVertexRenderState rs;
		{
			rs.rendererType = node->renderer->GetRendererType();
			rs.transformType = node->transformType;
			rs.invScale = roDataC.invScale;
			rs.lodFade = roDataC.lodFade;
			
			rs.material = roDataH.material;
			rs.shader = roDataH.shader;
			rs.subshaderIndex = roDataC.subshaderIndex;
			rs.passIndex = (rpData.data >> kPackPassShift) & kPackPassMask;
			
			rs.lights = &roDataL;
			
			rs.lightmapIndex = roDataH.lightmapIndex;
			DebugAssert(rs.lightmapIndex == node->renderer->GetLightmapIndex());
			rs.lightmapST = node->renderer->GetLightmapSTForRendering();
			rs.customPropsHash = node->renderer->GetCustomPropertiesHash();
		}
		
		// multi-pass requires vertex position values to be EXACTLY the same for all passes
		// therefore do NOT batch dynamic multi-pass nodes
		const bool multiPass = (rpData.data & kPackMultiPassFlag) == kPackMultiPassFlag;
		const bool dynamicAndMultiPass = (node->renderer->GetStaticBatchIndex() == 0) && multiPass;
		
		if (dynamicAndMultiPass ||
			prevRenderState != rs)
		{
			m_BatchRenderer.Flush();
			prevRenderState = rs;
			canBatch = 0;
		}
		else
			++canBatch;

		// NOTE: identity matrix has to be set on OpenGLES before lights are set
		// as lighting is specified in World space
		device.SetWorldMatrix( Matrix4x4f::identity.GetPtr() );

		renderSettings.SetupAmbient ();
		SetObjectScale(device, roDataC.lodFade, roDataC.invScale);

		node->renderer->ApplyCustomProperties(*rs.material, rs.shader, rs.subshaderIndex);

		// only setup lights & pass when not batching
		if (canBatch < 1)
		{
			SetupObjectLightmaps (lightmapper, rs.lightmapIndex, rs.lightmapST, true);

			LightManager::SetupVertexLights(rs.lights->lightCount, rs.lights->GetLights());
			channels = rs.material->SetPassWithShader(rs.passIndex, rs.shader, rs.subshaderIndex);
		}
		if (channels)
		{
			m_BatchRenderer.Add(node->renderer, subsetIndex, channels, node->worldMatrix, rs.transformType);
		}
	}

	m_BatchRenderer.Flush();
	EndRenderLoop();
	device.SetSRGBWrite(false);
	device.SetViewMatrix( m_Context->m_CurCameraMatrix.GetPtr() );
	

	// After everything we might still have renderables that should be drawn at the
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
}


ForwardVertexRenderLoop* CreateForwardVertexRenderLoop()
{
	return new ForwardVertexRenderLoop();
}

void DeleteForwardVertexRenderLoop (ForwardVertexRenderLoop* queue)
{
	delete queue;
}


static bool IsPassSuitable (UInt32 currentRenderOptions, UInt32 passRenderOptions, ShaderPassType passType,
							 bool isLightmapped, bool useRGBM)
{
	// All options that a pass requires must be on
	if( (currentRenderOptions & passRenderOptions) != passRenderOptions )
		return false; // some options are off, skip this pass
	
	if (passType != kPassAlways && passType != kPassVertex &&
		passType != kPassVertexLM && passType != kPassVertexLMRGBM)
		return false; // unsuitable pass type
	
	// Use either lightmapped or non-lightmapped pass
	if ((passType == kPassVertex && isLightmapped) ||
		((passType == kPassVertexLM || passType == kPassVertexLMRGBM) && !isLightmapped))
		return false;
	
	// Use pass that can properly decode the lightmap
	if ((passType == kPassVertexLM && useRGBM) ||
		(passType == kPassVertexLMRGBM && !useRGBM))
		return false;
	
	return true;
}

#if ENABLE_VERTEX_LOOP_HASH_SORTING
template<typename T>
static UInt8* InsertIntoHashBufferVtx(const T* p, UInt8* buffer)
{
	Assert((sizeof(T) % 4) == 0);	// unaligned write
	*reinterpret_cast<T*>(buffer) = *p;
	return buffer + sizeof(T);
}
#endif

void DoForwardVertexRenderLoop (RenderLoopContext& ctx, RenderObjectDataContainer& objects, bool opaque, ActiveLights& activeLights, bool linearLighting, bool clearFrameBuffer)
{
	GPU_AUTO_SECTION(opaque ? kGPUSectionOpaquePass : kGPUSectionTransparentPass);

	using namespace ForwardVertexRenderLoop_Enum;

	// Allocated on the stack each time, uses temp allocators
	ForwardVertexRenderLoop queue;
	queue.m_Context = &ctx;
	queue.m_Objects = &objects;
	queue.m_RenderObjectsCold.reserve(objects.size());
	queue.m_PlainRenderPasses.reserve(objects.size());
	const int kEstimatedLightDataPerObject = sizeof(VertexLightsBlock) + kEstimatedLightsPerObject * sizeof(Light*);
	queue.m_RenderObjectsLightData.reserve(objects.size() * kEstimatedLightDataPerObject);

	const CullResults& cullResults = *ctx.m_CullResults;

	// figure out current rendering options
	UInt32 currentRenderOptions = GetCurrentRenderOptions ();

	//RenderSettings& renderSettings = GetRenderSettings();
	const LightmapSettings& lightmapper = GetLightmapSettings();
#if UNITY_EDITOR
	bool useLightmaps = GetLightmapVisualization().GetUseLightmapsForRendering();
#endif

	bool useRGBM = gGraphicsCaps.SupportsRGBM();

	// Figure everything out
	RenderObjectDataContainer::iterator itEnd = objects.end();
	size_t roIndex = 0;
	for (RenderObjectDataContainer::iterator it = objects.begin(); it != itEnd; ++it, ++roIndex)
	{
		RenderObjectData& odata = *it;

		const VisibleNode* node = odata.visibleNode;
		RODataVLit& roDataC = queue.m_RenderObjectsCold.push_back();
		size_t visibleNodeIndex = node - cullResults.nodes.begin();

		LightmapSettings::TextureTriple lmTextures = lightmapper.GetLightmapTexture (node->renderer->GetLightmapIndex());
#if UNITY_EDITOR
		bool isLightmapped = useLightmaps && lmTextures.first.m_ID;
#else
		bool isLightmapped = lmTextures.first.m_ID;
#endif
		ShaderLab::IntShader& slshader = *odata.shader->GetShaderLabShader();
		int vlitSS = odata.subShaderIndex;
		if (vlitSS == -1)
		{
			vlitSS = slshader.GetDefaultSubshaderIndex (isLightmapped ? kRenderPathExtVertexLM : kRenderPathExtVertex);
			if (vlitSS == -1)
				continue;
		}
		roDataC.subshaderIndex = vlitSS;

		size_t objectLightsOffset = queue.m_RenderObjectsLightData.size();
		roDataC.lightsDataOffset = objectLightsOffset;

		GetLightManager().FindVertexLightsForObject (
			queue.m_RenderObjectsLightData,
			GetObjectLightIndices(cullResults, visibleNodeIndex),
			GetObjectLightCount(cullResults, visibleNodeIndex),
			activeLights, *node);

		roDataC.invScale = node->invScale;
		roDataC.lodFade = node->lodFade;

		// Go over all passes in the shader and add suitable ones for rendering
		ShaderLab::SubShader& subshader = slshader.GetSubShader(roDataC.subshaderIndex);
		int shaderPassCount = subshader.GetValidPassCount();
		
		// Determine if we will need more than a single pass
		int suitablePasses = 0;
		for( int pass = 0; pass < shaderPassCount && suitablePasses < 2; ++pass )
		{
			ShaderPassType passType; UInt32 passRenderOptions;
			subshader.GetPass(pass)->GetPassOptions( passType, passRenderOptions );
			
			if (IsPassSuitable (currentRenderOptions, passRenderOptions, passType, isLightmapped, useRGBM))
				++suitablePasses;
		}
		
		// Go over all passes in the shader
		UInt32 firstPassFlag = kPackFirstPassFlag;
		const UInt32 multiPassFlag = (suitablePasses > 1)? kPackMultiPassFlag: 0;
		for (int pass = 0; pass < shaderPassCount; ++pass)
		{
			ShaderPassType passType;
			UInt32 passRenderOptions;
			subshader.GetPass(pass)->GetPassOptions( passType, passRenderOptions );

			if (!IsPassSuitable (currentRenderOptions, passRenderOptions, passType, isLightmapped, useRGBM))
				continue;

			RPDataVLit& rpData = queue.m_PlainRenderPasses.push_back();
			rpData.roIndex = roIndex;
			rpData.data = 
				((pass & kPackPassMask) << kPackPassShift) |
				firstPassFlag |
				multiPassFlag;
			firstPassFlag = 0;

#if ENABLE_VERTEX_LOOP_HASH_SORTING

			//hash state information for render object sorter
			const int kHashBufferSize = 64;
			UInt8 hashBuffer[kHashBufferSize];
			UInt8* hashPtr = hashBuffer;

			// Always write 32b granularity into the hash buffer to avoid unaligned writes
			UInt32 rendererType = static_cast<UInt32>(node->renderer->GetRendererType());
			hashPtr = InsertIntoHashBufferVtx(&rendererType, hashPtr);
			UInt32 lightmapIndex = odata.lightmapIndex;
			hashPtr = InsertIntoHashBufferVtx(&lightmapIndex, hashPtr);
			UInt32 sourceMaterialIndex = 0;
#if GFX_ENABLE_DRAW_CALL_BATCHING
			hashPtr = InsertIntoHashBufferVtx(&odata.staticBatchIndex, hashPtr);
			if (odata.staticBatchIndex == 0)
				sourceMaterialIndex = odata.sourceMaterialIndex;
#else
			sourceMaterialIndex = odata.sourceMaterialIndex;
#endif
			hashPtr = InsertIntoHashBufferVtx(&sourceMaterialIndex, hashPtr);
			
			Assert(hashPtr-hashBuffer <= kHashBufferSize);

			Assert(hashPtr-hashBuffer <= kHashBufferSize);

			rpData.hash = MurmurHash2A(hashBuffer, hashPtr-hashBuffer, 0x9747b28c);
#endif
		}
	}

	// sort everything
	if (opaque)
		queue.SortRenderPassData<true> (queue.m_PlainRenderPasses);
	else
		queue.SortRenderPassData<false> (queue.m_PlainRenderPasses);

	// Render everything. When transitioning to render queues,
	// it will invoke camera renderables (halos, and so on).
	RenderTexture* rtMain = ctx.m_Camera->GetCurrentTargetTexture ();
	queue.PerformRendering (linearLighting && (!rtMain || rtMain->GetSRGBReadWrite()), clearFrameBuffer);
}
